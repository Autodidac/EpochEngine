// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Backend-neutral camera matrices, validated mesh framing and depth fitting.
module;
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <mutex>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <span>
#include <utility>

module render.preview_grid;
import asset.mesh_artifact;
import render.camera;
import render.ray;
import visuals.engine;

namespace epochengine::previewgrid
{
    [[nodiscard]] bool valid_precise_mesh_transform(const PreciseMeshTransform& value) noexcept
    {
        for(const auto x:value.position)if(!std::isfinite(x))return false;
        for(const auto x:value.scale)if(!std::isfinite(x)||x==0)return false;
        for(const auto x:value.rotation)if(!std::isfinite(x))return false;
        for(unsigned row=0;row<3;++row)for(unsigned other=0;other<3;++other)
        {
            double dot{};for(unsigned axis=0;axis<3;++axis)dot+=value.rotation[row*3+axis]*value.rotation[other*3+axis];
            if(std::abs(dot-(row==other?1.0:0.0))>1e-8)return false;
        }
        return true;
    }

    [[nodiscard]] std::optional<PreciseMeshTransform> make_precise_mesh_transform(
        std::array<double,3> position,std::array<double,3> scale,Vec3 degrees) noexcept
    {
        constexpr double radians=0.017453292519943295769;
        const double x=double(degrees.x)*radians,y=double(degrees.y)*radians,z=double(degrees.z)*radians;
        const double sx=std::sin(x),cx=std::cos(x),sy=std::sin(y),cy=std::cos(y),sz=std::sin(z),cz=std::cos(z);
        PreciseMeshTransform result{position,scale,
            {cy*cz,-cy*sz,sy,cx*sz+sx*sy*cz,cx*cz-sx*sy*sz,-sx*cy,
             sx*sz-cx*sy*cz,sx*cz+cx*sy*sz,cx*cy}};
        return valid_precise_mesh_transform(result)?std::optional{result}:std::nullopt;
    }

    [[nodiscard]] Mat4 identity_matrix() noexcept
    {
        return Mat4{
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        };
    }

    [[nodiscard]] Mat4 multiply(const Mat4& lhs, const Mat4& rhs) noexcept
    {
        Mat4 out{};
        for (int column = 0; column < 4; ++column)
        {
            for (int row = 0; row < 4; ++row)
            {
                float sum = 0.0f;
                for (int k = 0; k < 4; ++k)
                    sum += lhs[k * 4 + row] * rhs[column * 4 + k];
                out[column * 4 + row] = sum;
            }
        }
        return out;
    }

    [[nodiscard]] Mat4 perspective(
        float fovRadians,
        float aspect,
        float zNear,
        float zFar) noexcept
    {
        Mat4 out{};
        const float tanHalf = std::tan(fovRadians * 0.5f);
        out[0] = 1.0f / ((std::max)(0.001f, aspect) * tanHalf);
        out[5] = 1.0f / tanHalf;
        out[10] = -(zFar + zNear) / (zFar - zNear);
        out[11] = -1.0f;
        out[14] = -(2.0f * zFar * zNear) / (zFar - zNear);
        return out;
    }

    [[nodiscard]] Mat4 orthographic(
        float left,
        float right,
        float bottom,
        float top,
        float zNear,
        float zFar) noexcept
    {
        const float safeWidth = (std::max)(0.001f, right - left);
        const float safeHeight = (std::max)(0.001f, top - bottom);
        const float safeDepth = (std::max)(0.001f, zFar - zNear);
        Mat4 out = identity_matrix();
        out[0] = 2.0f / safeWidth;
        out[5] = 2.0f / safeHeight;
        out[10] = -2.0f / safeDepth;
        out[12] = -(right + left) / safeWidth;
        out[13] = -(top + bottom) / safeHeight;
        out[14] = -(zFar + zNear) / safeDepth;
        return out;
    }

    [[nodiscard]] Mat4 look_at(Vec3 eye, Vec3 target, Vec3 up) noexcept
    {
        const Vec3 forward = normalize(subtract(target, eye));
        const Vec3 right = normalize(cross(forward, up));
        const Vec3 realUp = cross(right, forward);

        Mat4 out = identity_matrix();
        out[0] = right.x;
        out[4] = right.y;
        out[8] = right.z;

        out[1] = realUp.x;
        out[5] = realUp.y;
        out[9] = realUp.z;

        out[2] = -forward.x;
        out[6] = -forward.y;
        out[10] = -forward.z;

        out[12] = -dot(right, eye);
        out[13] = -dot(realUp, eye);
        out[14] = dot(forward, eye);
        return out;
    }

    [[nodiscard]] std::optional<InitialOrbitMeshFraming> initial_orbit_mesh_framing(
        const ObjectMarker& marker,const CameraRigSnapshot& view,float aspect,bool unrestored) noexcept
    {
        if(!unrestored||!detail::valid_camera_snapshot(view)||view.mode!=CameraMode::Editor
            ||view.projection!=render_camera::ProjectionKind::perspective
            ||view.orientation!=render_camera::ViewOrientation::free
            ||view.focus.x!=0.0f||view.focus.y!=0.0f||view.focus.z!=0.0f
            ||view.distance!=13.5f||view.yaw_degrees!=-135.0f||view.pitch_degrees!=-28.0f
            ||!std::isfinite(aspect)||aspect<=0.0f||marker.primitive!=ObjectPreviewPrimitive::IndexedMesh
            ||!marker.mesh||marker.mesh->key().empty()||marker.mesh->vertices().empty())return {};
        const auto finite=[](Vec3 value){return std::isfinite(value.x)&&std::isfinite(value.y)&&std::isfinite(value.z);};
        if(!finite(marker.position)||!finite(marker.rotationDegrees)||!finite(marker.scale)
            ||std::abs(marker.scale.x)<0.00001f||std::abs(marker.scale.y)<0.00001f||std::abs(marker.scale.z)<0.00001f)return {};
        const auto minimum=marker.mesh->minimum(),maximum=marker.mesh->maximum();
        const Vec3 localCenter{(minimum[0]+maximum[0])*0.5f,(minimum[1]+maximum[1])*0.5f,(minimum[2]+maximum[2])*0.5f};
        const Vec3 scaledCenter{localCenter.x*marker.scale.x,localCenter.y*marker.scale.y,localCenter.z*marker.scale.z};
        const Vec3 center=add(marker.position,rotate_euler_degrees(scaledCenter,marker.rotationDegrees));
        if(!detail::finite_navigation_vector(center,CameraMode::Editor))return {};
        double radiusSquared{};
        for(const auto& vertex:marker.mesh->vertices())
        {
            const double x=(double(vertex.position[0])-localCenter.x)*marker.scale.x;
            const double y=(double(vertex.position[1])-localCenter.y)*marker.scale.y;
            const double z=(double(vertex.position[2])-localCenter.z)*marker.scale.z;
            radiusSquared=(std::max)(radiusSquared,x*x+y*y+z*z);
        }
        const float radius=static_cast<float>(std::sqrt(radiusSquared));
        if(!std::isfinite(radius)||radius<=0.0f)return {};
        const Camera camera=detail::camera_from_rig(detail::rig_from_snapshot(view));
        const Vec3 forward=normalize(subtract(camera.target,camera.eye));
        const Vec3 right=normalize(cross(forward,camera.up));
        const Vec3 up=normalize(cross(right,forward));
        const Vec3 relative=subtract(center,camera.eye);
        const float depth=dot(relative,forward);
        const float verticalHalf=camera.fovRadians*0.5f;
        const float horizontalHalf=std::atan(std::tan(verticalHalf)*aspect);
        const bool fits=depth-radius>=camera.nearPlane&&depth+radius<=camera.farPlane
            &&depth*std::sin(verticalHalf)-std::abs(dot(relative,up))*std::cos(verticalHalf)>=radius
            &&depth*std::sin(horizontalHalf)-std::abs(dot(relative,right))*std::cos(horizontalHalf)>=radius;
        if(fits)return {};
        const float fitDistance=radius/std::sin((std::min)(verticalHalf,horizontalHalf))*1.20f;
        const float framingRadius=(std::max)(radius,fitDistance/3.25f);
        const float distance=framingRadius*3.25f;
        if(!std::isfinite(distance)||distance>detail::kMaximum3DNavigationDistance)return {};
        return InitialOrbitMeshFraming{center,radius,framingRadius,distance};
    }

    namespace
    {
    [[nodiscard]] std::optional<std::array<double,2>> marker_depth_bounds_for_view(
        const ObjectMarker& marker,const Camera& camera,double aspect) noexcept
    {
        using Primitive=ObjectPreviewPrimitive;
        std::array<float,3> minimum{},maximum{};
        const bool indexed=marker.primitive==Primitive::IndexedMesh&&marker.mesh;
        if(indexed){minimum=marker.mesh->minimum();maximum=marker.mesh->maximum();}
        else if(marker.primitive==Primitive::Cube||marker.primitive==Primitive::Level
            ||marker.primitive==Primitive::Canvas2D||marker.primitive==Primitive::Spawn)
        {
            const float radius=std::clamp(marker.radius,0.16f,1.75f);
            const std::array scale{marker.scale.x,marker.scale.y,marker.scale.z};
            for(std::size_t axis=0;axis<3;++axis)maximum[axis]=(std::max)(std::abs(scale[axis]*0.5f),radius*0.45f);
            if(marker.primitive==Primitive::Level)maximum[1]=(std::max)(0.05f,radius*0.08f);
            if(marker.primitive==Primitive::Spawn)maximum[1]=(std::max)(0.08f,radius*0.12f);
            if(marker.primitive==Primitive::Canvas2D)maximum[2]=(std::max)(0.025f,radius*0.04f);
            for(std::size_t axis=0;axis<3;++axis)minimum[axis]=-maximum[axis];
        }
        else return {};
        const auto forward=camera_forward(camera);const auto eye=camera_world_eye(camera);
        const auto world_of=[&](Vec3 local) {
            std::array<double,3> point;
            if(indexed&&marker.preciseTransform)point=marker.preciseTransform->apply({local.x,local.y,local.z});
            else
            {
                if(indexed){local.x*=marker.scale.x;local.y*=marker.scale.y;local.z*=marker.scale.z;}
                const auto world=add(marker.position,marker.primitive==Primitive::Spawn
                    ?local:rotate_euler_degrees(local,marker.rotationDegrees));
                point={world.x,world.y,world.z};
            }
            return point;
        };
        const auto depth_of=[&](Vec3 local) {
            const auto point=world_of(local);
            return (point[0]-eye[0])*forward.x+(point[1]-eye[1])*forward.y+(point[2]-eye[2])*forward.z;
        };
        double nearest=(std::numeric_limits<double>::max)(),farthest=-nearest;
        const auto include=[&](double depth){nearest=(std::min)(nearest,depth);farthest=(std::max)(farthest,depth);return std::isfinite(depth);};
        for(unsigned corner=0;corner<8u;++corner)
            if(!include(depth_of({(corner&1u)?maximum[0]:minimum[0],(corner&2u)?maximum[1]:minimum[1],(corner&4u)?maximum[2]:minimum[2]})))return {};
        if(indexed&&nearest<=0.0&&farthest>0.0)
        {
            // A loose box or disconnected triangles may straddle the eye while
            // every visible triangle is distant. Side-plane clipping excludes
            // offscreen crossings without clipping a real crossing at the eye.
            nearest=(std::numeric_limits<double>::max)();farthest=-nearest;
            const auto vertices=marker.mesh->vertices();
            const auto indices=marker.mesh->indices();
            if(indices.empty()||indices.size()%3u!=0u)return {};
            const auto right=normalize(cross(forward,camera.up));
            const auto up=normalize(cross(right,forward));
            const double vertical=std::tan(double(camera.fovRadians)*0.5);
            const bool clip=std::isfinite(aspect)&&aspect>0.0&&std::isfinite(vertical)&&vertical>0.0
                &&camera.fovRadians<3.141592653589793&&dot(right,right)>0.5f&&dot(up,up)>0.5f
                &&std::isfinite(vertical*aspect);
            bool forwardTriangle{};
            for(std::size_t triangle=0;triangle<indices.size();triangle+=3u)
            {
                using Point=std::array<double,3>;
                std::array<Point,8> polygon{};std::size_t count=3u;
                double triangleNear=(std::numeric_limits<double>::max)(),triangleFar=-triangleNear;
                for(std::size_t corner=0;corner<3u;++corner)
                {
                    const auto index=indices[triangle+corner];
                    if(index>=vertices.size())return {};
                    const auto& point=vertices[index].position;
                    auto relative=world_of({point[0],point[1],point[2]});
                    for(unsigned axis=0;axis<3u;++axis)relative[axis]-=eye[axis];
                    const double depth=relative[0]*forward.x+relative[1]*forward.y+relative[2]*forward.z;
                    if(!std::isfinite(depth))return {};
                    triangleNear=(std::min)(triangleNear,depth);
                    triangleFar=(std::max)(triangleFar,depth);
                    if(clip)
                    {
                        polygon[corner]={relative[0]*right.x+relative[1]*right.y+relative[2]*right.z,
                            relative[0]*up.x+relative[1]*up.y+relative[2]*up.z,depth};
                        for(const auto value:polygon[corner])if(!std::isfinite(value))return {};
                    }
                }
                if(triangleFar<=0.0)continue;
                if(clip)
                {
                    // A triangle clipped by four sides has at most seven
                    // vertices. Fixed storage keeps depth fitting allocation-free.
                    for(unsigned plane=0;plane<4u&&count;++plane)
                    {
                        const unsigned axis=plane/2u;
                        const double tangent=axis?vertical:vertical*aspect;
                        const auto distance=[&](const Point& p){return p[2]*tangent+(plane%2u?p[axis]:-p[axis]);};
                        std::array<Point,8> output{};std::size_t next{};
                        for(std::size_t edge=0;edge<count;++edge)
                        {
                            const auto& from=polygon[edge];const auto& to=polygon[(edge+1u)%count];
                            const double a=distance(from),b=distance(to);
                            if(!std::isfinite(a)||!std::isfinite(b))return {};
                            const bool inside=a>=0.0;
                            if(inside){if(next==output.size())return {};output[next++]=from;}
                            if(inside==(b>=0.0)||a==0.0||b==0.0)continue;
                            const double fraction=a/(a-b);Point crossing;
                            for(unsigned component=0;component<3u;++component)
                                crossing[component]=std::lerp(from[component],to[component],fraction);
                            if(next==output.size())return {};output[next++]=crossing;
                        }
                        polygon=output;count=next;
                    }
                    if(!count)continue;
                    triangleNear=(std::numeric_limits<double>::max)();triangleFar=-triangleNear;
                    for(std::size_t i=0;i<count;++i)
                    {triangleNear=(std::min)(triangleNear,polygon[i][2]);triangleFar=(std::max)(triangleFar,polygon[i][2]);}
                    if(triangleFar<=0.0)continue;
                }
                include(triangleNear);include(triangleFar);forwardTriangle=true;
            }
            if(!forwardTriangle)return std::array{0.0,0.0};
        }
        return std::array{nearest,farthest};
    }
    }

    [[nodiscard]] std::optional<std::array<double,2>> marker_depth_bounds(
        const ObjectMarker& marker,const Camera& camera) noexcept
    {return marker_depth_bounds_for_view(marker,camera,0.0);}

    [[nodiscard]] Camera fit_visible_depth_range(Camera camera,
        std::span<const ObjectMarker> markers,double aspect) noexcept
    {
        if(camera.projection!=render_camera::ProjectionKind::perspective)return camera;
        double nearest=(std::numeric_limits<double>::max)(),farthest{};
        bool found{};
        for(const auto& marker:markers)
        {
            if(marker.editorOnly||(!object_marker_uses_solid_fill(marker)
                &&!detail::mesh_wire_requested(marker)))continue;
            const auto bounds=marker_depth_bounds_for_view(marker,camera,aspect);
            if(!bounds){camera.nearPlane=(std::min)(camera.nearPlane,0.05f);return camera;}
            if((*bounds)[1]<=0.0)continue;
            if((*bounds)[0]<=0.0){camera.nearPlane=(std::min)(camera.nearPlane,0.05f);return camera;}
            nearest=(std::min)(nearest,(*bounds)[0]);
            farthest=(std::max)(farthest,(*bounds)[1]);found=true;
        }
        if(!found)return camera;
        const double near=(std::max)(0.05,nearest*0.25);
        const double far=(std::max)(near*2.0,farthest*1.05);
        if(!std::isfinite(near)||!std::isfinite(far)
            ||far>(std::numeric_limits<float>::max)())return camera;
        camera.nearPlane=static_cast<float>(near);
        camera.farPlane=static_cast<float>(far);
        return camera;
    }

    [[nodiscard]] Mat4 projection_for(
        const void* ctxKey,
        float aspect,
        const Camera& camera) noexcept
    {
        const float safeAspect = (std::max)(0.001f, aspect);
        (void)ctxKey;
        if (camera.projection == render_camera::ProjectionKind::orthographic)
        {
            const float halfHeight = (std::max)(0.025f, camera.orthographicVerticalSize * 0.5f);
            const float halfWidth = halfHeight * safeAspect;
            return orthographic(
                -halfWidth,
                halfWidth,
                -halfHeight,
                halfHeight,
                camera.nearPlane,
                camera.farPlane);
        }

        return perspective(
            camera.fovRadians,
            safeAspect,
            camera.nearPlane,
            camera.farPlane);
    }

    void step_camera(
        const void* ctxKey,
        float deltaTime,
        float moveForward,
        float moveRight,
        float moveUp,
        float yawInput,
        float pitchInput,
        float speedMultiplier,
        float rollInput) noexcept
    {
        const void* const rigKey = detail::normalize_camera_key(ctxKey);
        if (!rigKey || !std::isfinite(deltaTime) || !std::isfinite(moveForward)
            || !std::isfinite(moveRight) || !std::isfinite(moveUp)
            || !std::isfinite(yawInput) || !std::isfinite(pitchInput)
            || !std::isfinite(speedMultiplier) || !std::isfinite(rollInput))
            return;

        const float dt = (std::clamp)(deltaTime, 0.0f, 0.05f);
        if (dt <= 0.0f)
            return;

        std::unique_lock lock(detail::g_cameraRigMutex);
        auto& storedRig = detail::ensure_rig(rigKey);
        auto rig = storedRig;
        if (moveForward == 0.0f
            && moveRight == 0.0f
            && moveUp == 0.0f
            && yawInput == 0.0f
            && pitchInput == 0.0f
            && rollInput == 0.0f)
        {
            return;
        }

        if (rig.mode == CameraMode::Canvas2D)
        {
            const float panSpeed =
                (std::max)(1.0f, rig.distance * 0.55f);
            const float dollySpeed =
                (std::max)(1.5f, rig.distance * 1.35f);
            rig.focus.x += moveRight * panSpeed * dt;
            rig.focus.y += moveUp * panSpeed * dt;
            rig.distance = (std::clamp)(
                rig.distance - moveForward * dollySpeed * dt,
                0.75f,
                2048.0f);
            detail::commit_camera_navigation(storedRig, std::move(rig));
            return;
        }

        const bool freeOrientation = !render_camera::axis_locked(rig.orientation);
        if (freeOrientation)
        {
            const float lookSpeed = rig.mode == CameraMode::FPS ? 105.0f : 92.0f;
            rig.yawDegrees = std::remainder(
                rig.yawDegrees + yawInput * lookSpeed * dt,
                360.0f);
            rig.pitchDegrees = detail::navigation_pitch(rig.mode,
                double(rig.pitchDegrees) + double(pitchInput) * lookSpeed * dt);
            rig.freeYawDegrees = rig.yawDegrees;
            rig.freePitchDegrees = rig.pitchDegrees;
            rig.rollDegrees = std::remainder(
                rig.rollDegrees + rollInput * 90.0f * dt, 360.0f);
        }

        const Vec3 worldUp{ 0.0f, 1.0f, 0.0f };
        const Camera camera = detail::camera_from_rig(rig);
        const Vec3 forward = camera_forward(camera);
        const Vec3 flatForward = detail::flat_forward_from_angles(rig.yawDegrees, rig.pitchDegrees);
        const Vec3 right = normalize(cross(forward, camera.up));
        const float navigationSpeed = rig.flySpeed
            * (std::clamp)(detail::finite_or(speedMultiplier, 1.0f), 0.05f, 16.0f);

        if (rig.mode == CameraMode::FPS)
        {
            rig.position = add(rig.position, scale(flatForward, moveForward * navigationSpeed * dt));
            rig.position = add(rig.position, scale(right, moveRight * navigationSpeed * dt));
            rig.position = add(rig.position, scale(worldUp, moveUp * navigationSpeed * dt));
            detail::commit_camera_navigation(storedRig, std::move(rig));
            return;
        }

        rig.focus = add(rig.focus, scale(forward, moveForward * navigationSpeed * dt));
        rig.focus = add(rig.focus, scale(right, moveRight * navigationSpeed * dt));
        rig.focus = add(rig.focus, scale(worldUp, moveUp * navigationSpeed * dt));
        detail::commit_camera_navigation(storedRig, std::move(rig));
    }

    void look_camera(
        const void* ctxKey,
        float yawDeltaDegrees,
        float pitchDeltaDegrees) noexcept
    {
        const void* const rigKey = detail::normalize_camera_key(ctxKey);
        if (!rigKey)
            return;

        if (!std::isfinite(yawDeltaDegrees) || !std::isfinite(pitchDeltaDegrees)
            || (yawDeltaDegrees == 0.0f && pitchDeltaDegrees == 0.0f))
            return;

        std::unique_lock lock(detail::g_cameraRigMutex);
        auto& rig = detail::ensure_rig(rigKey);
        if (rig.mode == CameraMode::Canvas2D || render_camera::axis_locked(rig.orientation))
            return;
        rig.yawDegrees = std::remainder(
            rig.yawDegrees + yawDeltaDegrees,
            360.0f);
        rig.pitchDegrees = detail::navigation_pitch(rig.mode,
            double(rig.pitchDegrees) + pitchDeltaDegrees);
        rig.freeYawDegrees = rig.yawDegrees;
        rig.freePitchDegrees = rig.pitchDegrees;
        detail::save_active_view(rig);
        detail::touch_rig(rig);
    }

    [[nodiscard]] std::array<Vertex, 8> look_marker_vertices_for(const void* ctxKey) noexcept
    {
        std::array<Vertex, 8> out{};
        const void* const rigKey = detail::normalize_camera_key(ctxKey);
        if (!rigKey || !authoring_guides_visible(rigKey))
            return out;

        const auto camera = camera_for(rigKey);
        Vec3 hit{};
        float markerDistance = camera_distance_for(rigKey);
        bool hasHit = false;

        {
            std::shared_lock lock(detail::g_cameraRigMutex);
            const auto it = detail::g_cameraRigs.find(rigKey);
            if (it != detail::g_cameraRigs.end())
                markerDistance = it->second.distance;
        }

        {
            std::shared_lock lock(detail::g_cameraRigMutex);
            const auto it = detail::g_cameraRigs.find(rigKey);
            if (it != detail::g_cameraRigs.end() && it->second.mode == CameraMode::Editor)
            {
                hit = { it->second.focus.x, 0.0f, it->second.focus.z };
                hasHit = std::isfinite(hit.x) && std::isfinite(hit.z);
            }
        }

        if (!hasHit)
        {
            const Vec3 ray = normalize(subtract(camera.target, camera.eye));
            if (std::isfinite(ray.x) && std::isfinite(ray.y) && std::isfinite(ray.z)
                && std::abs(ray.y) > 1.0e-4f)
            {
                const float hitDistance = (0.0f - camera.eye.y) / ray.y;
                if (hitDistance > 0.0f && std::isfinite(hitDistance))
                {
                    hit = add(camera.eye, scale(ray, hitDistance));
                    hasHit = std::isfinite(hit.x) && std::isfinite(hit.z);
                }
            }
        }

        if (!hasHit)
        {
            std::shared_lock lock(detail::g_cameraRigMutex);
            if (const auto lastHit = detail::g_lastMarkerHits.find(rigKey);
                lastHit != detail::g_lastMarkerHits.end())
            {
                hit = lastHit->second;
                hasHit = std::isfinite(hit.x) && std::isfinite(hit.z);
            }
        }

        if (!hasHit)
            return out;

        {
            std::unique_lock lock(detail::g_cameraRigMutex);
            detail::g_lastMarkerHits[rigKey] = hit;
        }

        const float markerSize = (std::max)(0.14f, markerDistance * 0.028f);
        const float markerHeight = 0.001f;

        const auto make_vertex = [](Vec3 position, Vec3 color) noexcept
        {
            return Vertex{ .position = position, .color = color };
        };

        const Vec3 markerColor = visual_rgb(epochengine::visuals::look_marker());

        out[0] = make_vertex({ hit.x - markerSize, markerHeight, hit.z }, markerColor);
        out[1] = make_vertex({ hit.x + markerSize, markerHeight, hit.z }, markerColor);
        out[2] = make_vertex({ hit.x, markerHeight, hit.z - markerSize }, markerColor);
        out[3] = make_vertex({ hit.x, markerHeight, hit.z + markerSize }, markerColor);
        out[4] = make_vertex({ hit.x - markerSize * 0.6f, markerHeight, hit.z - markerSize * 0.6f }, markerColor);
        out[5] = make_vertex({ hit.x + markerSize * 0.6f, markerHeight, hit.z + markerSize * 0.6f }, markerColor);
        out[6] = make_vertex({ hit.x - markerSize * 0.6f, markerHeight, hit.z + markerSize * 0.6f }, markerColor);
        out[7] = make_vertex({ hit.x + markerSize * 0.6f, markerHeight, hit.z - markerSize * 0.6f }, markerColor);
        return out;
    }

    [[nodiscard]] std::size_t look_marker_vertex_count_for(const void* ctxKey) noexcept
    {
        const auto vertices = look_marker_vertices_for(ctxKey);
        for (const auto& vertex : vertices)
        {
            if (vertex.position.x != 0.0f || vertex.position.y != 0.0f || vertex.position.z != 0.0f)
                return vertices.size();
        }
        return 0u;
    }

    namespace detail
    {
        [[nodiscard]] bool valid_camera_snapshot(const CameraRigSnapshot& snapshot) noexcept
        {
            for(const double coordinate:snapshot.navigation_origin)
                if(!std::isfinite(coordinate)||std::abs(coordinate)>kMaximum3DNavigationDistance)return false;
            if (!snapshot.valid || sanitize_camera_mode(snapshot.mode) != snapshot.mode
                || !finite_navigation_vector(snapshot.focus, snapshot.mode)
                || !finite_navigation_vector(snapshot.position, snapshot.mode)
                || !std::isfinite(snapshot.yaw_degrees) || !std::isfinite(snapshot.pitch_degrees)
                || !std::isfinite(snapshot.free_yaw_degrees) || !std::isfinite(snapshot.free_pitch_degrees)
                || !std::isfinite(snapshot.roll_degrees)
                || !std::isfinite(snapshot.distance) || snapshot.distance < 0.0f
                || !std::isfinite(snapshot.orthographic_vertical_size) || snapshot.orthographic_vertical_size < 0.0f
                || !std::isfinite(snapshot.fly_speed) || snapshot.fly_speed < 0.0f
                || !std::isfinite(snapshot.scene_extent) || snapshot.scene_extent < 0.0f
                || snapshot.scene_extent > kMaximum3DNavigationDistance
                || (snapshot.projection != render_camera::ProjectionKind::perspective
                    && snapshot.projection != render_camera::ProjectionKind::orthographic)
                || static_cast<unsigned>(snapshot.orientation)
                    > static_cast<unsigned>(render_camera::ViewOrientation::bottom))
                return false;
            for (std::size_t index = 0u; index < snapshot.view_distances.size(); ++index)
            {
                if (!finite_navigation_vector(snapshot.view_focuses[index], snapshot.mode)
                    || !std::isfinite(snapshot.view_distances[index]) || snapshot.view_distances[index] < 0.0f
                    || !std::isfinite(snapshot.view_orthographic_sizes[index]) || snapshot.view_orthographic_sizes[index] < 0.0f)
                    return false;
            }
            return true;
        }

        [[nodiscard]] float navigation_far_plane(const CameraRigState& rig) noexcept
        {
            if (rig.mode == CameraMode::Canvas2D)
                return 4096.0f;
            double distance = rig.distance;
            if (rig.mode == CameraMode::FPS)
            {
                const double x = double(rig.position.x) - rig.focus.x;
                const double y = double(rig.position.y) - rig.focus.y;
                const double z = double(rig.position.z) - rig.focus.z;
                distance = std::sqrt(x*x + y*y + z*z);
            }
            // Keep the legacy range for ordinary scenes. Retained framing extent
            // prevents zooming closer from clipping a large world's far side.
            return static_cast<float>((std::clamp)(2.0 * (distance + rig.sceneExtent),
                4096.0, double(kMaximum3DNavigationDistance) * 4.0));
        }

        [[nodiscard]] float navigation_near_plane(const CameraRigState& rig) noexcept
        {
            if (rig.mode != CameraMode::Editor
                || rig.projection != render_camera::ProjectionKind::perspective)
                return 0.05f;
            // A fixed centimetre near plane rounds the whole Moon/universe
            // overview onto depth 1 in the float projection. Reserve mantissa
            // precision relative to the orbit's viewing distance, rather than
            // limiting world size or changing its physical geometry. Ordinary
            // scenes retain the exact old plane; zooming close restores it.
            return (std::max)(0.05f, rig.distance
                * (128.0f * (std::numeric_limits<float>::epsilon)()));
        }

        void commit_camera_navigation(CameraRigState& destination, CameraRigState candidate) noexcept
        {
            if (!finite_navigation_vector(candidate.focus, candidate.mode)
                || !finite_navigation_vector(candidate.position, candidate.mode)
                || !std::isfinite(candidate.yawDegrees) || !std::isfinite(candidate.pitchDegrees)
                || !std::isfinite(candidate.freeYawDegrees) || !std::isfinite(candidate.freePitchDegrees)
                || !std::isfinite(candidate.rollDegrees)
                || !std::isfinite(candidate.distance) || !std::isfinite(candidate.orthographicVerticalSize))
                return;
            const auto anchor=candidate.mode==CameraMode::FPS?candidate.position:candidate.focus;
            if(candidate.mode!=CameraMode::Canvas2D&&(std::max)({std::abs(anchor.x),std::abs(anchor.y),std::abs(anchor.z)})>65536.0f)
            {
                candidate.navigationOrigin[0]+=anchor.x;candidate.navigationOrigin[1]+=anchor.y;candidate.navigationOrigin[2]+=anchor.z;
                candidate.focus=subtract(candidate.focus,anchor);candidate.position=subtract(candidate.position,anchor);
                for(auto& focus:candidate.viewFocuses)focus=subtract(focus,anchor);
            }
            for(const double coordinate:candidate.navigationOrigin)
                if(!std::isfinite(coordinate)||std::abs(coordinate)>kMaximum3DNavigationDistance)return;
            save_active_view(candidate);
            touch_rig(candidate);
            destination = std::move(candidate);
        }

        [[nodiscard]] Vec3 navigation_reference_up(const CameraRigState& rig) noexcept
        {
            if (rig.mode == CameraMode::Editor && std::abs(rig.pitchDegrees) >= 85.0f)
            {
                // Continue over either pole without crossing forward with a
                // parallel world-up vector or flipping the view upside down.
                // Inside the legacy range retain its exact unbanked up input.
                constexpr float radians = 0.017453292519943295f;
                const float yaw = rig.yawDegrees * radians;
                const float pitch = rig.pitchDegrees * radians;
                return {-std::sin(pitch) * std::cos(yaw), std::cos(pitch),
                    -std::sin(pitch) * std::sin(yaw)};
            }
            return {0.0f, 1.0f, 0.0f};
        }

        [[nodiscard]] Camera local_camera_from_rig(const CameraRigState& rig) noexcept
        {
            if (rig.externalFrame) return *rig.externalFrame;
            Vec3 forward = forward_from_angles(rig.yawDegrees, rig.pitchDegrees);
            Vec3 cameraUp = navigation_reference_up(rig);
            if (render_camera::axis_locked(rig.orientation))
            {
                const auto axis = render_camera::orientation_basis(rig.orientation);
                forward = { axis.forward.x, axis.forward.y, axis.forward.z };
                cameraUp = { axis.up.x, axis.up.y, axis.up.z };
            }
            // A zero bank keeps the legacy basis byte-for-byte. Free flight
            // rotates the existing camera basis about its viewing direction.
            if (!render_camera::axis_locked(rig.orientation) && rig.rollDegrees != 0.0f
                && rig.mode != CameraMode::Canvas2D)
            {
                const Vec3 right = normalize(cross(forward, cameraUp));
                const Vec3 up = normalize(cross(right, forward));
                const float angle = rig.rollDegrees * 0.017453292519943295f;
                cameraUp = add(scale(up, std::cos(angle)), scale(right, std::sin(angle)));
            }
            if (rig.mode == CameraMode::FPS)
            {
                // One world unit can round back onto the eye at planetary
                // coordinates. This is a direction anchor, not travel speed.
                const float lookDistance = (std::max)(1.0f,
                    (std::max)({std::abs(rig.position.x), std::abs(rig.position.y), std::abs(rig.position.z)})
                        * (8.0f * (std::numeric_limits<float>::epsilon)()));
                const Vec3 target = add(rig.position, scale(forward, lookDistance));
                return Camera{
                    .eye = rig.position,
                    .target = target,
                    .up = cameraUp,
                    .fovRadians = 1.05f,
                    .nearPlane = 0.05f,
                    .farPlane = navigation_far_plane(rig),
                    .projection = render_camera::ProjectionKind::perspective,
                    .orientation = render_camera::ViewOrientation::free,
                    .orthographicVerticalSize = rig.orthographicVerticalSize
                };
            }
            if (rig.mode == CameraMode::Canvas2D)
            {
                return Camera{
                    .eye = { rig.focus.x, rig.focus.y, rig.focus.z + rig.distance },
                    .target = rig.focus,
                    .up = { 0.0f, 1.0f, 0.0f },
                    .fovRadians = 0.48f,
                    .nearPlane = 0.01f,
                    .farPlane = 4096.0f,
                    .projection = render_camera::ProjectionKind::orthographic,
                    .orientation = render_camera::ViewOrientation::free,
                    .orthographicVerticalSize = rig.orthographicVerticalSize
                };
            }
            const Vec3 eye = subtract(rig.focus, scale(forward, rig.distance));
            return Camera{
                .eye = eye,
                .target = rig.focus,
                .up = cameraUp,
                .fovRadians = 0.90f,
                .nearPlane = navigation_near_plane(rig),
                .farPlane = navigation_far_plane(rig),
                .projection = rig.projection,
                .orientation = rig.orientation,
                .orthographicVerticalSize = rig.orthographicVerticalSize
            };
        }

        [[nodiscard]] Camera camera_from_rig(const CameraRigState& rig) noexcept
        {
            auto camera=local_camera_from_rig(rig);
            if(rig.externalFrame||rig.mode==CameraMode::Canvas2D||(rig.navigationOrigin==std::array<double,3>{}
                &&(std::max)({std::abs(rig.focus.x),std::abs(rig.focus.y),std::abs(rig.focus.z),rig.distance})<65536.0f))return camera;
            auto direction=forward_from_angles(rig.yawDegrees,rig.pitchDegrees);
            if(render_camera::axis_locked(rig.orientation))
            {const auto forward=render_camera::orientation_basis(rig.orientation).forward;direction={forward.x,forward.y,forward.z};}
            const auto anchor=rig.mode==CameraMode::FPS?rig.position:rig.focus;
            const double distance=rig.mode==CameraMode::FPS?0.0:rig.distance;
            const std::array<double,3> eye{rig.navigationOrigin[0]+anchor.x-double(direction.x)*distance,
                rig.navigationOrigin[1]+anchor.y-double(direction.y)*distance,
                rig.navigationOrigin[2]+anchor.z-double(direction.z)*distance};
            camera.navigation=CameraNavigationFrame{eye,direction};camera.navigationValid=true;
            camera.eye={float(eye[0]),float(eye[1]),float(eye[2])};
            const float lookDistance=(std::max)(1.0f,(std::max)({std::abs(camera.eye.x),std::abs(camera.eye.y),
                std::abs(camera.eye.z)})*(8.0f*std::numeric_limits<float>::epsilon()));
            camera.target=add(camera.eye,scale(direction,lookDistance));
            return camera;
        }

        [[nodiscard]] render_camera::ViewDescriptor descriptor_from_rig(
            const CameraRigState& rig) noexcept
        {
            const Camera camera = camera_from_rig(rig);
            auto descriptor = render_camera::make_editor_view(
                { rig.logicalViewId == 0u ? 1u : rig.logicalViewId,
                    rig.logicalViewGeneration == 0u ? 1u : rig.logicalViewGeneration },
                { camera.eye.x, camera.eye.y, camera.eye.z },
                { camera.target.x, camera.target.y, camera.target.z },
                { camera.up.x, camera.up.y, camera.up.z },
                camera.projection,
                camera.orientation,
                camera.orthographicVerticalSize,
                rig.revision);
            descriptor.perspective = {
                camera.fovRadians,
                camera.nearPlane,
                camera.farPlane
            };
            descriptor.orthographic = {
                camera.orthographicVerticalSize,
                camera.nearPlane,
                camera.farPlane
            };
            descriptor.purpose = rig.mode == CameraMode::Canvas2D
                ? render_camera::ViewPurpose::canvas_2d
                : (rig.mode == CameraMode::FPS
                    ? render_camera::ViewPurpose::project_runtime
                    : render_camera::ViewPurpose::editor);
            return descriptor;
        }

        [[nodiscard]] bool same_camera_frame(const Camera& lhs, const Camera& rhs) noexcept
        {
            return lhs.eye.x == rhs.eye.x && lhs.eye.y == rhs.eye.y && lhs.eye.z == rhs.eye.z
                && lhs.target.x == rhs.target.x && lhs.target.y == rhs.target.y && lhs.target.z == rhs.target.z
                && lhs.up.x == rhs.up.x && lhs.up.y == rhs.up.y && lhs.up.z == rhs.up.z
                && lhs.fovRadians == rhs.fovRadians && lhs.nearPlane == rhs.nearPlane
                && lhs.farPlane == rhs.farPlane && lhs.projection == rhs.projection
                && lhs.orientation == rhs.orientation && lhs.orthographicVerticalSize == rhs.orthographicVerticalSize
                && lhs.navigationValid==rhs.navigationValid && (!lhs.navigationValid || (lhs.navigation.eye==rhs.navigation.eye
                    && lhs.navigation.forward.x==rhs.navigation.forward.x && lhs.navigation.forward.y==rhs.navigation.forward.y
                    && lhs.navigation.forward.z==rhs.navigation.forward.z));
        }

        [[nodiscard]] std::optional<Camera> validated_camera_frame(Camera camera) noexcept
        {
            const auto finite = [](Vec3 value) {
                return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
            };
            if(camera.navigationValid)
            {
                for(const auto value:camera.navigation.eye)if(!std::isfinite(value))return {};
                if(!finite(camera.navigation.forward)||std::abs(dot(camera.navigation.forward,camera.navigation.forward)-1.0f)>.001f)return {};
            }
            const Vec3 forward = camera_forward(camera);
            const float forwardLength = dot(forward, forward), upLength = dot(camera.up, camera.up);
            if (!finite(camera.eye) || !finite(camera.target) || !finite(camera.up)
                || !std::isfinite(forwardLength) || forwardLength <= 1.0e-8f
                || !std::isfinite(upLength) || upLength <= 1.0e-8f
                || !std::isfinite(camera.fovRadians) || !std::isfinite(camera.orthographicVerticalSize)
                || (camera.projection != render_camera::ProjectionKind::perspective
                    && camera.projection != render_camera::ProjectionKind::orthographic)
                || static_cast<unsigned>(camera.orientation) > static_cast<unsigned>(render_camera::ViewOrientation::bottom))
                return {};
            const Vec3 right = cross(normalize(forward), normalize(camera.up));
            if (!finite(right) || dot(right, right) <= 1.0e-8f) return {};
            auto descriptor = render_camera::make_editor_view({1u, 1u},
                {camera.eye.x, camera.eye.y, camera.eye.z},
                {camera.target.x, camera.target.y, camera.target.z},
                {camera.up.x, camera.up.y, camera.up.z}, camera.projection, camera.orientation,
                camera.orthographicVerticalSize, 1u);
            descriptor.perspective = {camera.fovRadians, camera.nearPlane, camera.farPlane};
            descriptor.orthographic = {camera.orthographicVerticalSize, camera.nearPlane, camera.farPlane};
            const auto resolved = render_camera::resolve(descriptor);
            if (!resolved.valid) return {};
            camera.target = {resolved.descriptor.target.x, resolved.descriptor.target.y, resolved.descriptor.target.z};
            camera.up = {resolved.up.x, resolved.up.y, resolved.up.z};
            return camera;
        }

        [[nodiscard]] CameraRigSnapshot snapshot_from_rig(const CameraRigState& rig) noexcept
        {
            return CameraRigSnapshot{
                .valid = true,
                .mode = sanitize_camera_mode(rig.mode),
                .focus = rig.focus,
                .position = rig.position,
                .yaw_degrees = rig.yawDegrees,
                .pitch_degrees = rig.pitchDegrees,
                .free_yaw_degrees = rig.freeYawDegrees,
                .free_pitch_degrees = rig.freePitchDegrees,
                .distance = rig.distance,
                .orthographic_vertical_size = rig.orthographicVerticalSize,
                .fly_speed = rig.flySpeed,
                .projection = rig.projection,
                .orientation = rig.orientation,
                .view_focuses = rig.viewFocuses,
                .view_distances = rig.viewDistances,
                .view_orthographic_sizes = rig.viewOrthographicSizes,
                .logical_view_id = rig.logicalViewId,
                .logical_view_generation = rig.logicalViewGeneration,
                .revision = rig.revision,
                .scene_extent = rig.sceneExtent,
                .roll_degrees = rig.rollDegrees,
                .navigation_origin = rig.navigationOrigin
            };
        }

        [[nodiscard]] CameraRigState rig_from_snapshot(const CameraRigSnapshot& snapshot) noexcept
        {
            CameraRigState rig = make_default_rig(sanitize_camera_mode(snapshot.mode));
            rig.focus = finite_vec_or(snapshot.focus, rig.focus);
            rig.position = finite_vec_or(snapshot.position, rig.position);
            rig.navigationOrigin=snapshot.navigation_origin;
            rig.yawDegrees = finite_or(snapshot.yaw_degrees, rig.yawDegrees);
            rig.rollDegrees = std::remainder(finite_or(snapshot.roll_degrees, 0.0f), 360.0f);
            rig.pitchDegrees = navigation_pitch(rig.mode,
                finite_or(snapshot.pitch_degrees, rig.pitchDegrees));
            rig.freeYawDegrees = finite_or(snapshot.free_yaw_degrees, rig.yawDegrees);
            rig.freePitchDegrees = navigation_pitch(rig.mode,
                finite_or(snapshot.free_pitch_degrees, rig.pitchDegrees));
            const float minDistance = rig.mode == CameraMode::FPS
                ? 0.0f
                : (rig.mode == CameraMode::Canvas2D ? 0.75f : 0.50f);
            const float maxDistance = rig.mode == CameraMode::FPS
                ? 0.0f
                : maximum_navigation_distance(rig.mode);
            rig.distance = (std::clamp)(
                finite_or(snapshot.distance, rig.distance),
                minDistance,
                maxDistance);
            rig.orthographicVerticalSize = (std::clamp)(
                finite_or(snapshot.orthographic_vertical_size, rig.orthographicVerticalSize),
                0.05f,
                maximum_orthographic_size(rig.mode));
            rig.flySpeed = (std::clamp)(
                finite_or(snapshot.fly_speed, rig.flySpeed),
                0.05f,
                4096.0f);
            rig.projection = snapshot.projection;
            rig.orientation = snapshot.orientation;
            rig.viewFocuses = snapshot.view_focuses;
            rig.viewDistances = snapshot.view_distances;
            rig.viewOrthographicSizes = snapshot.view_orthographic_sizes;
            for (std::size_t index = 0u; index < rig.viewDistances.size(); ++index)
            {
                rig.viewDistances[index] = (std::min)(rig.viewDistances[index], maximum_navigation_distance(rig.mode));
                rig.viewOrthographicSizes[index] = (std::min)(rig.viewOrthographicSizes[index], maximum_orthographic_size(rig.mode));
            }
            rig.sceneExtent = rig.mode == CameraMode::Canvas2D ? 0.0f : snapshot.scene_extent;
            if (rig.mode == CameraMode::FPS)
            {
                rig.projection = render_camera::ProjectionKind::perspective;
                rig.orientation = render_camera::ViewOrientation::free;
            }
            else if (rig.mode == CameraMode::Canvas2D)
            {
                rig.projection = render_camera::ProjectionKind::orthographic;
                rig.orientation = render_camera::ViewOrientation::free;
            }
            rig.logicalViewId = snapshot.logical_view_id;
            rig.logicalViewGeneration = snapshot.logical_view_generation == 0u
                ? 1u
                : snapshot.logical_view_generation;
            const auto anchor=rig.mode==CameraMode::FPS?rig.position:rig.focus;
            if(rig.mode!=CameraMode::Canvas2D&&(std::max)({std::abs(anchor.x),std::abs(anchor.y),std::abs(anchor.z)})>65536.0f)
                commit_camera_navigation(rig,rig);
            rig.revision = snapshot.revision == 0 ? 1 : snapshot.revision;
            return rig;
        }

        bool focus_camera_impl(
            const void* ctxKey,
            const std::array<double,3>& requestedFocus,
            float framingRadius,bool precise) noexcept
        {
            Vec3 focus{float(requestedFocus[0]),float(requestedFocus[1]),float(requestedFocus[2])};
            for(const auto coordinate:requestedFocus)if(!std::isfinite(coordinate))return false;
            const void* const rigKey = detail::normalize_camera_key(ctxKey);
            if (!rigKey || !std::isfinite(focus.x) || !std::isfinite(focus.y) || !std::isfinite(focus.z)
                || !std::isfinite(framingRadius) || framingRadius < 0.0f)
            {
                return false;
            }

            std::unique_lock lock(detail::g_cameraRigMutex);
            const auto existing = detail::g_cameraRigs.find(rigKey);
            const CameraMode mode = existing == detail::g_cameraRigs.end() ? CameraMode::Editor : existing->second.mode;
            if (!detail::finite_navigation_vector(focus, mode)
                || (mode != CameraMode::Canvas2D && framingRadius > detail::kMaximum3DNavigationDistance))
                return false;
            auto& rig = detail::ensure_rig(rigKey);
            if (rig.mode != CameraMode::Canvas2D)
                rig.sceneExtent = (std::max)(rig.sceneExtent, framingRadius);
            if(rig.mode!=CameraMode::Canvas2D)
            {
                // A focus request is absolute; navigation afterwards stays local.
                const auto oldOrigin=rig.navigationOrigin;
                rig.navigationOrigin={};
                if(precise){rig.navigationOrigin=requestedFocus;focus={};}
                else if((std::max)({std::abs(focus.x),std::abs(focus.y),std::abs(focus.z)})>65536.0f)
                {rig.navigationOrigin={focus.x,focus.y,focus.z};focus={};}
                const Vec3 shift{float(oldOrigin[0]-rig.navigationOrigin[0]),float(oldOrigin[1]-rig.navigationOrigin[1]),float(oldOrigin[2]-rig.navigationOrigin[2])};
                for(auto& saved:rig.viewFocuses)saved=add(saved,shift);
                rig.position=add(rig.position,shift);rig.focus=add(rig.focus,shift);
            }
            if (rig.mode == CameraMode::FPS)
            {
                rig.position = focus;
            }
            else
            {
                rig.focus = focus;
                const float minimumDistance =
                    rig.mode == CameraMode::Canvas2D ? 0.75f : 0.50f;
                const float maximumDistance = detail::maximum_navigation_distance(rig.mode);
                const float framingDistance = (std::max)(
                    minimumDistance,
                    framingRadius * 3.25f);
                rig.distance = (std::clamp)(framingDistance, minimumDistance, maximumDistance);
                if (rig.projection == render_camera::ProjectionKind::orthographic
                    || framingRadius * 2.60f > detail::maximum_orthographic_size(CameraMode::Canvas2D))
                {
                    rig.orthographicVerticalSize = (std::clamp)(
                        (std::max)(0.50f, framingRadius * 2.60f),
                        0.05f,
                        detail::maximum_orthographic_size(rig.mode));
                }
            }
            detail::save_active_view(rig);
            detail::touch_rig(rig);
            return true;
        }

        [[nodiscard]] float surface_limited_zoom(const Camera& camera, Vec3 direction, float distance,
            float requested, const SolidDrawFrame* solids, const MeshWireDrawFrame* wire) noexcept
        {
            const float precision=8.0f*std::numeric_limits<float>::epsilon()
                *(std::max)({1.0f,std::abs(camera.eye.x),std::abs(camera.eye.y),std::abs(camera.eye.z)});
            const float clearance=(std::max)(0.1f,precision); // relative surface query, not planet-centre clip distance
            const epochengine::ray::RayDesc query{{camera.eye.x,camera.eye.y,camera.eye.z},
                {direction.x,direction.y,direction.z},0.0f,distance-requested+clearance};
            float allowed=distance-requested;
            const auto scan=[&](const auto& vertices) {
                for(std::size_t i=0;i+2u<vertices.size();i+=3u)
                {
                    const auto a=vertices[i].position,b=vertices[i+1u].position,c=vertices[i+2u].position;
                    const auto hit=epochengine::ray::intersect(query,epochengine::ray::Triangle{
                        {a.x,a.y,a.z},{b.x,b.y,b.z},{c.x,c.y,c.z},true});
                    if(hit)allowed=(std::min)(allowed,(std::max)(0.0f,hit->distance-clearance));
                }
            };
            if(solids)
            {
                if(solids->geometry)scan(*solids->geometry);
                for(const auto& marker:solids->markers)if(marker.textured)scan(*marker.textured);
            }
            if(wire&&wire->depth_triangles)scan(*wire->depth_triangles);
            return (std::max)(requested,distance-allowed);
        }
    }

    [[nodiscard]] CameraMode camera_mode_for(const void* ctxKey) noexcept
    {
        const void* const rigKey = detail::normalize_camera_key(ctxKey);
        if (!rigKey)
            return CameraMode::Editor;

        std::shared_lock lock(detail::g_cameraRigMutex);
        const auto it = detail::g_cameraRigs.find(rigKey);
        return it != detail::g_cameraRigs.end() ? it->second.mode : CameraMode::Editor;
    }

    [[nodiscard]] EditorView editor_view_for(const void* ctxKey) noexcept
    {
        const void* const rigKey = detail::normalize_camera_key(ctxKey);
        if (!rigKey)
            return EditorView::Perspective;

        std::shared_lock lock(detail::g_cameraRigMutex);
        const auto it = detail::g_cameraRigs.find(rigKey);
        return it != detail::g_cameraRigs.end()
            ? detail::editor_view_from_rig(it->second)
            : EditorView::Perspective;
    }

    bool set_editor_view(const void* ctxKey, EditorView view) noexcept
    {
        const void* const rigKey = detail::normalize_camera_key(ctxKey);
        if (!rigKey || static_cast<std::size_t>(view) >= 8u)
            return false;

        std::unique_lock lock(detail::g_cameraRigMutex);
        auto& rig = detail::ensure_rig(rigKey);
        if (rig.mode != CameraMode::Editor)
            return false;
        if (detail::editor_view_from_rig(rig) == view)
            return true;

        detail::save_active_view(rig);
        if (rig.orientation == render_camera::ViewOrientation::free)
        {
            rig.freeYawDegrees = rig.yawDegrees;
            rig.freePitchDegrees = rig.pitchDegrees;
        }

        rig.projection = view == EditorView::Perspective
            ? render_camera::ProjectionKind::perspective
            : render_camera::ProjectionKind::orthographic;
        rig.orientation = detail::orientation_for_editor_view(view);
        if (rig.orientation == render_camera::ViewOrientation::free)
        {
            rig.yawDegrees = rig.freeYawDegrees;
            rig.pitchDegrees = rig.freePitchDegrees;
        }
        detail::restore_or_seed_view(rig, view);
        detail::touch_rig(rig);
        return true;
    }

    [[nodiscard]] CameraRigSnapshot capture_camera_rig_snapshot(const void* ctxKey) noexcept
    {
        const void* const rigKey = detail::normalize_camera_key(ctxKey);
        if (!rigKey)
            return {};

        std::shared_lock lock(detail::g_cameraRigMutex);
        const auto it = detail::g_cameraRigs.find(rigKey);
        if (it == detail::g_cameraRigs.end())
            return detail::snapshot_from_rig(detail::make_default_rig(CameraMode::Editor));
        return detail::snapshot_from_rig(it->second);
    }

    bool restore_camera_rig_snapshot(const void* ctxKey, const CameraRigSnapshot& snapshot) noexcept
    {
        const void* const rigKey = detail::normalize_camera_key(ctxKey);
        if (!rigKey || !detail::valid_camera_snapshot(snapshot))
            return false;

        std::unique_lock lock(detail::g_cameraRigMutex);
        auto restored = detail::rig_from_snapshot(snapshot);
        restored.restoredFromSnapshot = true;
        if (restored.logicalViewId == 0u)
        {
            if (detail::g_nextLogicalViewId == (std::numeric_limits<std::uint64_t>::max)())
                detail::g_nextLogicalViewId = 1u;
            restored.logicalViewId = detail::g_nextLogicalViewId++;
            restored.logicalViewGeneration = 1u;
        }
        detail::g_cameraRigs[rigKey] = restored;
        return true;
    }

    [[nodiscard]] bool set_editor_camera_pose(const void* ctxKey,
        const std::array<double, 3>& eye, Vec3 forward, Vec3 up) noexcept
    {
        const void* const rigKey = detail::normalize_camera_key(ctxKey);
        const Vec3 direction = normalize(forward);
        const Vec3 viewRight = normalize(cross(direction, up));
        const float rightLength = dot(viewRight, viewRight);
        if (!rigKey || !std::isfinite(rightLength) || rightLength <= 0.0f) return false;
        for (const double component : eye)
            if (!std::isfinite(component) || std::abs(component) > detail::kMaximum3DNavigationDistance) return false;
        const Vec3 viewUp = cross(viewRight, direction);

        std::unique_lock lock(detail::g_cameraRigMutex);
        auto& rig = detail::ensure_rig(rigKey);
        if (rig.externalFrame || rig.mode == CameraMode::Canvas2D) return false;
        auto next = rig;
        constexpr float degrees = 57.29577951308232f;
        next.mode = CameraMode::Editor;
        next.projection = render_camera::ProjectionKind::perspective;
        next.orientation = render_camera::ViewOrientation::free;
        next.yawDegrees = next.freeYawDegrees = std::atan2(direction.z, direction.x) * degrees;
        next.pitchDegrees = next.freePitchDegrees = std::atan2(direction.y, std::hypot(direction.x, direction.z)) * degrees;
        const auto navigationForward = detail::forward_from_angles(next.yawDegrees, next.pitchDegrees);
        const auto right = normalize(cross(navigationForward, detail::navigation_reference_up(next)));
        const auto baseUp = cross(right, navigationForward);
        next.rollDegrees = std::atan2(dot(viewUp, right), dot(viewUp, baseUp)) * degrees;

        // A saved camera supplies its eye and sight line. Keep its navigation
        // pivot local so an earlier planetary orbit cannot raise its near plane.
        next.distance = 1.0f;
        next.navigationOrigin = {eye[0] + double(navigationForward.x) * next.distance,
            eye[1] + double(navigationForward.y) * next.distance, eye[2] + double(navigationForward.z) * next.distance};
        const Vec3 shift{float(rig.navigationOrigin[0] - next.navigationOrigin[0]),
            float(rig.navigationOrigin[1] - next.navigationOrigin[1]), float(rig.navigationOrigin[2] - next.navigationOrigin[2])};
        for (auto& focus : next.viewFocuses) focus = add(focus, shift);
        next.focus = {};
        next.position = scale(navigationForward, -next.distance);
        detail::save_active_view(next);
        if (!detail::valid_camera_snapshot(detail::snapshot_from_rig(next))) return false;
        detail::touch_rig(next);
        rig = std::move(next);
        return true;
    }

    void set_camera_mode(const void* ctxKey, CameraMode mode) noexcept
    {
        const void* const rigKey = detail::normalize_camera_key(ctxKey);
        if (!rigKey)
            return;
        std::unique_lock lock(detail::g_cameraRigMutex);
        const bool existingRig = detail::g_cameraRigs.contains(rigKey);
        auto& rig = detail::ensure_rig(rigKey);
        mode = detail::sanitize_camera_mode(mode);
        if (rig.mode == mode)
        {
            if (rig.externalFrame)
            {
                rig.externalFrame.reset();
                detail::touch_rig(rig);
            }
            return;
        }
        const std::uint64_t nextRevision =
            rig.revision == (std::numeric_limits<std::uint64_t>::max)()
                ? 1
                : (rig.revision + 1);
        const std::uint64_t logicalViewId = rig.logicalViewId;
        const std::uint32_t logicalViewGeneration = rig.logicalViewGeneration;
        const bool restoredFromSnapshot = rig.restoredFromSnapshot;
        const float sceneExtent = rig.mode != CameraMode::Canvas2D && mode != CameraMode::Canvas2D
            ? rig.sceneExtent : 0.0f;
        const bool preserve3DView = existingRig && rig.mode != CameraMode::Canvas2D
            && mode != CameraMode::Canvas2D && !rig.externalFrame;
        const auto previousCamera = detail::camera_from_rig(rig);
        const auto previousLocalCamera=detail::local_camera_from_rig(rig);
        const auto previousOrigin=rig.navigationOrigin;
        const float previousDistance = (std::max)(0.5f, rig.distance);
        const float previousFlySpeed = rig.flySpeed;
        rig = detail::make_default_rig(mode);
        rig.logicalViewId = logicalViewId;
        rig.logicalViewGeneration = logicalViewGeneration;
        rig.restoredFromSnapshot = restoredFromSnapshot;
        rig.sceneExtent = sceneExtent;
        if (preserve3DView)
        {
            const Vec3 direction = camera_forward(previousCamera);
            constexpr float degrees = 57.29577951308232f;
            rig.yawDegrees = std::atan2(direction.z, direction.x) * degrees;
            rig.pitchDegrees = detail::navigation_pitch(mode,
                std::asin((std::clamp)(direction.y, -1.0f, 1.0f)) * degrees);
            rig.freeYawDegrees = rig.yawDegrees;
            rig.freePitchDegrees = rig.pitchDegrees;
            const Vec3 forward = detail::forward_from_angles(rig.yawDegrees, rig.pitchDegrees);
            const Vec3 right = normalize(cross(forward, {0.0f, 1.0f, 0.0f}));
            const Vec3 up = normalize(cross(right, forward));
            rig.rollDegrees = std::atan2(dot(previousCamera.up, right), dot(previousCamera.up, up)) * degrees;
            rig.navigationOrigin=previousOrigin;
            rig.position = previousLocalCamera.eye;
            rig.distance = previousDistance;
            rig.focus = add(previousLocalCamera.eye, scale(forward, previousDistance));
            if(previousCamera.navigationValid)
            {rig.navigationOrigin=camera_world_eye(previousCamera);rig.position={};rig.focus={};
                if(mode==CameraMode::Editor){rig.navigationOrigin[0]+=double(forward.x)*previousDistance;
                    rig.navigationOrigin[1]+=double(forward.y)*previousDistance;rig.navigationOrigin[2]+=double(forward.z)*previousDistance;}}
            rig.flySpeed = previousFlySpeed;
        }
        detail::save_active_view(rig);
        rig.revision = nextRevision;
    }

    void reset_camera(const void* ctxKey, bool clear_scene_extent) noexcept
    {
        const void* const rigKey = detail::normalize_camera_key(ctxKey);
        if (!rigKey)
            return;

        std::unique_lock lock(detail::g_cameraRigMutex);
        auto& rig = detail::ensure_rig(rigKey);
        const CameraMode mode = rig.mode;
        const std::uint64_t logicalViewId = rig.logicalViewId;
        const std::uint32_t logicalViewGeneration = rig.logicalViewGeneration;
        const bool restoredFromSnapshot = rig.restoredFromSnapshot;
        const float sceneExtent = clear_scene_extent || rig.mode == CameraMode::Canvas2D
            ? 0.0f : rig.sceneExtent;
        const std::uint64_t nextRevision =
            rig.revision == (std::numeric_limits<std::uint64_t>::max)()
                ? 1
                : (rig.revision + 1);
        rig = detail::make_default_rig(mode);
        rig.logicalViewId = logicalViewId;
        rig.logicalViewGeneration = logicalViewGeneration;
        rig.restoredFromSnapshot = restoredFromSnapshot;
        rig.sceneExtent = sceneExtent;
        detail::save_active_view(rig);
        rig.revision = nextRevision;
    }

    bool focus_camera(const void* ctxKey,Vec3 focus,float framingRadius) noexcept
    {return detail::focus_camera_impl(ctxKey,{focus.x,focus.y,focus.z},framingRadius,false);}

    bool focus_camera_precise(const void* ctxKey,const std::array<double,3>& focus,
        float framingRadius) noexcept
    {return detail::focus_camera_impl(ctxKey,focus,framingRadius,true);}

    bool frame_initial_orbit_mesh(const void* ctxKey,const ObjectMarker& marker,float aspect) noexcept
    {
        if(!ctxKey)return false;
        CameraRigSnapshot view;
        bool unrestored=true;
        {
            std::shared_lock lock(detail::g_cameraRigMutex);
            const auto found=detail::g_cameraRigs.find(ctxKey);
            if(found==detail::g_cameraRigs.end())view=detail::snapshot_from_rig(detail::make_default_rig(CameraMode::Editor));
            else {view=detail::snapshot_from_rig(found->second);unrestored=!found->second.restoredFromSnapshot;}
        }
        const auto framing=initial_orbit_mesh_framing(marker,view,aspect,unrestored);
        return framing&&focus_camera(ctxKey,framing->target,framing->framingRadius);
    }

    [[nodiscard]] float camera_distance_for(const void* ctxKey) noexcept
    {
        const void* const rigKey = detail::normalize_camera_key(ctxKey);
        if (!rigKey)
            return detail::make_default_rig(CameraMode::Editor).distance;

        std::shared_lock lock(detail::g_cameraRigMutex);
        const auto it = detail::g_cameraRigs.find(rigKey);
        if (it == detail::g_cameraRigs.end())
            return detail::make_default_rig(CameraMode::Editor).distance;
        if (it->second.mode == CameraMode::FPS)
            return 0.0f;
        return it->second.projection == render_camera::ProjectionKind::orthographic
            ? it->second.orthographicVerticalSize
            : it->second.distance;
    }

    void zoom_camera(const void* ctxKey, float amount) noexcept
    {
        const void* const rigKey = detail::normalize_camera_key(ctxKey);
        if (!rigKey || !std::isfinite(amount) || amount == 0.0f)
            return;

        // Obtain immutable geometry before taking the camera write lock. The
        // existing solid owner may itself read the camera while resolving a miss.
        std::shared_ptr<const SolidDrawFrame> collisionSolids;
        std::shared_ptr<const PublishedSceneFrame> collisionScene;
        std::shared_ptr<const MeshWireDrawFrame> collisionWire;
        bool surfaceQuery=amount>0.0f;
        if(surfaceQuery)
        {
            std::shared_lock cameraLock(detail::g_cameraRigMutex);
            const auto existing=detail::g_cameraRigs.find(rigKey);
            if(existing!=detail::g_cameraRigs.end())surfaceQuery=existing->second.mode==CameraMode::Editor
                &&existing->second.projection==render_camera::ProjectionKind::perspective&&!existing->second.externalFrame;
        }
        if(surfaceQuery)
        {
            try {collisionSolids=object_solid_frame_for(rigKey);} catch(...) {return;}
            std::shared_lock markerLock(detail::g_objectMarkerMutex);
            const auto found=detail::g_objectMarkers.find(rigKey);
            if(found!=detail::g_objectMarkers.end())
            {
                collisionScene=found->second.scene_frame;
                collisionWire=found->second.mesh_wire_geometry;
                if(collisionScene){collisionSolids=collisionScene->solids;collisionWire=collisionScene->wire;}
            }
        }
        std::unique_lock lock(detail::g_cameraRigMutex);
        auto& rig = detail::ensure_rig(rigKey);
        if (rig.mode == CameraMode::FPS)
            return;

        const float oldDistance = rig.distance;
        const float oldOrthographicSize = rig.orthographicVerticalSize;
        const float minDistance =
            rig.mode == CameraMode::Canvas2D ? 0.75f : 0.50f;
        const float maxDistance = detail::maximum_navigation_distance(rig.mode);
        const float zoomFactor = std::exp(
            -(std::clamp)(amount, -12.0f, 12.0f) * 0.14f);
        if (rig.projection == render_camera::ProjectionKind::orthographic)
        {
            rig.orthographicVerticalSize = (std::clamp)(
                rig.orthographicVerticalSize * zoomFactor,
                0.05f,
                detail::maximum_orthographic_size(rig.mode));
        }
        else
        {
            rig.distance = (std::clamp)(
                rig.distance * zoomFactor,
                minDistance,
                maxDistance);
            if(rig.mode==CameraMode::Editor&&!rig.externalFrame&&rig.distance<oldDistance)
            {
                auto previous=rig;previous.distance=oldDistance;
                auto camera=detail::camera_from_rig(previous);
                const auto direction=camera_forward(camera);
                if(collisionScene)
                {
                    const auto eye=camera_world_eye(camera);
                    camera.eye=add(collisionScene->camera.eye,{float(eye[0]-collisionScene->preciseNavigationEye[0]),
                        float(eye[1]-collisionScene->preciseNavigationEye[1]),float(eye[2]-collisionScene->preciseNavigationEye[2])});
                }
                rig.distance=detail::surface_limited_zoom(camera,direction,oldDistance,rig.distance,collisionSolids.get(),collisionWire.get());
            }
        }
        if (rig.distance != oldDistance || rig.orthographicVerticalSize != oldOrthographicSize)
        {
            detail::save_active_view(rig);
            detail::touch_rig(rig);
        }
    }

    [[nodiscard]] std::uint64_t camera_revision_for(const void* ctxKey) noexcept
    {
        const void* const rigKey = detail::normalize_camera_key(ctxKey);
        if (!rigKey)
            return 0;

        std::shared_lock lock(detail::g_cameraRigMutex);
        const auto it = detail::g_cameraRigs.find(rigKey);
        return it != detail::g_cameraRigs.end() ? it->second.revision : 0;
    }

    void pan_camera_drag(
        const void* ctxKey,
        float deltaRightPixels,
        float deltaForwardPixels) noexcept
    {
        const void* const rigKey = detail::normalize_camera_key(ctxKey);
        if (!rigKey)
            return;

        if (!std::isfinite(deltaRightPixels) || !std::isfinite(deltaForwardPixels)
            || (deltaRightPixels == 0.0f && deltaForwardPixels == 0.0f))
            return;
        const auto scene=scene_frame_for(rigKey);
        std::unique_lock lock(detail::g_cameraRigMutex);
        auto& storedRig = detail::ensure_rig(rigKey);
        auto rig = storedRig;
        if (rig.mode == CameraMode::FPS)
            return;

        const Camera camera = detail::camera_from_rig(rig);
        const Vec3 forward = camera_forward(camera);
        const Vec3 right = normalize(cross(forward, camera.up));
        const Vec3 vertical = normalize(cross(right, forward));
        float navigationScale = rig.projection == render_camera::ProjectionKind::orthographic
            ? rig.orthographicVerticalSize
            : rig.distance;
        if(scene&&rig.mode==CameraMode::Editor&&rig.projection==render_camera::ProjectionKind::perspective&&!rig.externalFrame)
        {
            auto relative=camera;const auto eye=camera_world_eye(camera);
            relative.eye=add(scene->camera.eye,{float(eye[0]-scene->preciseNavigationEye[0]),
                float(eye[1]-scene->preciseNavigationEye[1]),float(eye[2]-scene->preciseNavigationEye[2])});
            navigationScale=(std::max)(0.05f,rig.distance-detail::surface_limited_zoom(relative,forward,
                rig.distance,0.0f,scene->solids.get(),scene->wire.get()));
        }
        const float dragScale = (std::max)(
            0.00075f,
            navigationScale
                * (rig.mode == CameraMode::Canvas2D
                    ? 0.00105f
                    : 0.00120f));
        rig.focus = add(
            rig.focus,
            scale(right, deltaRightPixels * dragScale));
        rig.focus = add(
            rig.focus,
            scale(vertical, deltaForwardPixels * dragScale));
        detail::commit_camera_navigation(storedRig, std::move(rig));
    }

    void dolly_camera_drag(const void* ctxKey, float deltaPixels) noexcept
    {
        if (!std::isfinite(deltaPixels) || deltaPixels == 0.0f)
            return;
        zoom_camera(ctxKey, -deltaPixels * 0.045f);
    }

    void adjust_fly_speed(const void* ctxKey, float wheelSteps) noexcept
    {
        const void* const rigKey = detail::normalize_camera_key(ctxKey);
        if (!rigKey || !std::isfinite(wheelSteps) || wheelSteps == 0.0f)
            return;
        std::unique_lock lock(detail::g_cameraRigMutex);
        auto& rig = detail::ensure_rig(rigKey);
        const float oldSpeed = rig.flySpeed;
        rig.flySpeed = (std::clamp)(
            rig.flySpeed * std::exp((std::clamp)(wheelSteps, -12.0f, 12.0f) * 0.12f),
            0.05f,
            4096.0f);
        if (rig.flySpeed != oldSpeed)
            detail::touch_rig(rig);
    }

    [[nodiscard]] float fly_speed_for(const void* ctxKey) noexcept
    {
        const void* const rigKey = detail::normalize_camera_key(ctxKey);
        if (!rigKey)
            return 6.5f;
        std::shared_lock lock(detail::g_cameraRigMutex);
        const auto it = detail::g_cameraRigs.find(rigKey);
        return it != detail::g_cameraRigs.end() ? it->second.flySpeed : 6.5f;
    }

    [[nodiscard]] std::optional<Camera> validate_camera_frame(Camera camera) noexcept
    {return detail::validated_camera_frame(camera);}

    [[nodiscard]] bool set_external_camera_frame(const void* ctxKey, const Camera& camera) noexcept
    {
        const void* const rigKey = detail::normalize_camera_key(ctxKey);
        if (!rigKey) return false;
        const auto validated = detail::validated_camera_frame(camera);
        if (!validated) return false;
        std::unique_lock lock(detail::g_cameraRigMutex);
        auto& rig = detail::ensure_rig(rigKey);
        if (rig.externalFrame && (detail::same_camera_frame(*rig.externalFrame, camera)
            || detail::same_camera_frame(*rig.externalFrame, *validated))) return true;
        rig.externalFrame = *validated;
        detail::touch_rig(rig);
        return true;
    }

    [[nodiscard]] bool clear_external_camera_frame(const void* ctxKey) noexcept
    {
        const void* const rigKey = detail::normalize_camera_key(ctxKey);
        if (!rigKey) return false;
        std::unique_lock lock(detail::g_cameraRigMutex);
        const auto found = detail::g_cameraRigs.find(rigKey);
        if (found == detail::g_cameraRigs.end() || !found->second.externalFrame) return false;
        found->second.externalFrame.reset();
        detail::touch_rig(found->second);
        return true;
    }

    [[nodiscard]] Camera camera_for(const void* ctxKey) noexcept
    {
        const void* const rigKey = detail::normalize_camera_key(ctxKey);
        if (!rigKey)
            return kCamera;

        std::shared_lock lock(detail::g_cameraRigMutex);
        const auto it = detail::g_cameraRigs.find(rigKey);
        if (it == detail::g_cameraRigs.end())
            return detail::camera_from_rig(detail::make_default_rig(CameraMode::Editor));
        return detail::camera_from_rig(it->second);
    }

    [[nodiscard]] render_camera::ViewDescriptor camera_descriptor_for(
        const void* ctxKey) noexcept
    {
        const void* const rigKey = detail::normalize_camera_key(ctxKey);
        if (!rigKey)
            return detail::descriptor_from_rig(detail::make_default_rig(CameraMode::Editor));

        std::shared_lock lock(detail::g_cameraRigMutex);
        const auto it = detail::g_cameraRigs.find(rigKey);
        return it != detail::g_cameraRigs.end()
            ? detail::descriptor_from_rig(it->second)
            : detail::descriptor_from_rig(detail::make_default_rig(CameraMode::Editor));
    }
}
