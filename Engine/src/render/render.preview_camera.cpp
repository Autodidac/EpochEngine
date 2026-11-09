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
#include <optional>
#include <shared_mutex>
#include <span>
#include <utility>

module render.preview_grid;
import asset.mesh_artifact;
import render.camera;
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
        if (!rigKey)
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
}
