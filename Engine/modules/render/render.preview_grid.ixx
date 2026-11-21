// Epoch Project. Modular C++ Framework.
// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Provided "AS IS", without warranty of any kind.
// Use permitted for Non-Commercial Purposes ONLY, without prior commercial licensing agreement.
// Redistribution Allowed with This Notice and LICENSE file.
// No obligation to disclose modifications.
// See LICENSE file for full terms.
// Epoch system notes (auto-checked):
// Epoch Render: preserve backend boundaries and resource lifetime contracts.
module;

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <span>
#include <stop_token>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

export module render.preview_grid;

import asset.mesh_artifact;
import asset.cloud_volume;
import render.texture_artifact;
import asset.texture_artifact;
import render.canvas2d;
import visuals.engine;
import render.arcade;
export import render.camera;
import render.lighting;
import render.ray;

namespace epochengine::previewgrid
{
    export struct Vec3
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
    };

    export [[nodiscard]] constexpr Vec3 visual_rgb(epochengine::visuals::Rgb color) noexcept
    {
        return Vec3{ color.r, color.g, color.b };
    }

    export struct Vertex
    {
        Vec3 position{};
        Vec3 color{};
    };

    export struct GridGeometry final
    {
        std::vector<Vertex> vertices{};
        std::vector<std::uint32_t> indices{};
        std::uint64_t signature{};
        float spacing{ 1.0f };
        Vec3 center{};
    };

    export using Mat4 = std::array<float, 16>;

    export enum class ObjectPreviewPrimitive : std::uint8_t
    {
        Cube = 0,
        Light,
        Spawn,
        Camera,
        Level,
        Canvas2D,
        EngineArcadeScreen,
        ForestTrunk,
        ForestBranch,
        ForestLeafCluster,
        EngineArcadeCabinet,
        IndexedMesh,
        TranslationArrow
    };

    // A single tapered primitive always fits the renderer's fixed eight-sided
    // topology. Admission needs its source dimensions, not an allocated mesh.
    // Keep the squared-axis epsilon test identical to the indexed mesher,
    // including finite input whose intermediate squared length overflows.
    export [[nodiscard]] inline bool tapered_segment_projection_admitted(
        float length, float radiusStart, float radiusEnd) noexcept
    {
        return std::isfinite(length) && std::sqrt(length * length) > 0.000001f
            && std::isfinite(radiusStart) && radiusStart > 0.0f
            && std::isfinite(radiusEnd) && radiusEnd > 0.0f;
    }

    export struct ObjectPreviewGeometryRoute final
    {
        bool solid_scene{};
        bool marker_wire{};
        bool selection_wire{};
        bool sampled_surface{};
    };

    export [[nodiscard]] constexpr ObjectPreviewGeometryRoute object_preview_geometry_route(
        ObjectPreviewPrimitive primitive) noexcept
    {
        switch (primitive)
        {
        case ObjectPreviewPrimitive::EngineArcadeScreen:
            return ObjectPreviewGeometryRoute{
                .solid_scene = false,
                .marker_wire = render_arcade::kScreenSceneNode.diagnostic_overlay,
                .selection_wire = false,
                .sampled_surface = render_arcade::kScreenSceneNode.sampled_render_surface
            };
        case ObjectPreviewPrimitive::EngineArcadeCabinet:
            return ObjectPreviewGeometryRoute{
                .solid_scene = true,
                .marker_wire = render_arcade::kCabinetBodySceneNode.diagnostic_overlay,
                .selection_wire = false,
                .sampled_surface = render_arcade::kCabinetBodySceneNode.sampled_render_surface
            };
        case ObjectPreviewPrimitive::Camera:
        case ObjectPreviewPrimitive::Light:
            return ObjectPreviewGeometryRoute{
                .solid_scene = false,
                .marker_wire = true,
                .selection_wire = true,
                .sampled_surface = false
            };
        case ObjectPreviewPrimitive::TranslationArrow:
            return {.marker_wire = true};
        case ObjectPreviewPrimitive::Spawn:
            return ObjectPreviewGeometryRoute{
                .solid_scene = true,
                .marker_wire = true,
                .selection_wire = true,
                .sampled_surface = false
            };
        case ObjectPreviewPrimitive::IndexedMesh:
        case ObjectPreviewPrimitive::Cube:
        case ObjectPreviewPrimitive::Level:
        case ObjectPreviewPrimitive::Canvas2D:
        case ObjectPreviewPrimitive::ForestTrunk:
        case ObjectPreviewPrimitive::ForestBranch:
        case ObjectPreviewPrimitive::ForestLeafCluster:
            return ObjectPreviewGeometryRoute{
                .solid_scene = true,
                .marker_wire = false,
                .selection_wire = true,
                .sampled_surface = false
            };
        }
        return {};
    }

    export struct ArcadePreviewRoutingContract final
    {
        bool cabinet_is_scene_geometry{};
        bool cabinet_excludes_marker_overlay{};
        bool cabinet_excludes_selection_overlay{};
        bool screen_is_sampled_surface{};
        bool screen_excludes_marker_overlay{};
        bool screen_excludes_selection_overlay{};
        bool screen_accepts_front_view{};
        bool screen_rejects_rear_view{};

        [[nodiscard]] constexpr bool passed() const noexcept
        {
            return cabinet_is_scene_geometry
                && cabinet_excludes_marker_overlay
                && cabinet_excludes_selection_overlay
                && screen_is_sampled_surface
                && screen_excludes_marker_overlay
                && screen_excludes_selection_overlay
                && screen_accepts_front_view
                && screen_rejects_rear_view;
        }
    };

    export [[nodiscard]] constexpr ArcadePreviewRoutingContract
        run_arcade_preview_routing_contract() noexcept
    {
        const ObjectPreviewGeometryRoute cabinet =
            object_preview_geometry_route(ObjectPreviewPrimitive::EngineArcadeCabinet);
        const ObjectPreviewGeometryRoute screen =
            object_preview_geometry_route(ObjectPreviewPrimitive::EngineArcadeScreen);
        return ArcadePreviewRoutingContract{
            .cabinet_is_scene_geometry = cabinet.solid_scene && !cabinet.sampled_surface,
            .cabinet_excludes_marker_overlay = !cabinet.marker_wire,
            .cabinet_excludes_selection_overlay = !cabinet.selection_wire,
            .screen_is_sampled_surface = !screen.solid_scene && screen.sampled_surface,
            .screen_excludes_marker_overlay = !screen.marker_wire,
            .screen_excludes_selection_overlay = !screen.selection_wire,
            .screen_accepts_front_view = render_arcade::screen_sample_plane_faces_viewer(
                render_arcade::kScreenSceneNode.position[2],
                render_arcade::kScreenSceneNode.scale[2],
                8.0f),
            .screen_rejects_rear_view = !render_arcade::screen_sample_plane_faces_viewer(
                render_arcade::kScreenSceneNode.position[2],
                render_arcade::kScreenSceneNode.scale[2],
                -8.0f)
        };
    }

    static_assert(run_arcade_preview_routing_contract().passed());

    export struct CameraNavigationFrame final
    {
        std::array<double,3> eye{};
        Vec3 forward{};
    };
    export struct Camera
    {
        Vec3 eye{ 9.0f, 7.0f, 9.0f };
        Vec3 target{ 0.0f, 0.0f, 0.0f };
        Vec3 up{ 0.0f, 1.0f, 0.0f };
        float fovRadians = 0.90f;
        float nearPlane = 0.1f;
        float farPlane = 64.0f;
        render_camera::ProjectionKind projection{ render_camera::ProjectionKind::perspective };
        render_camera::ViewOrientation orientation{ render_camera::ViewOrientation::free };
        float orthographicVerticalSize = 10.0f;
        CameraNavigationFrame navigation{}; bool navigationValid{};
    };
    export [[nodiscard]] inline std::array<double,3> camera_world_eye(const Camera& camera) noexcept
    {return camera.navigationValid ? camera.navigation.eye : std::array<double,3>{camera.eye.x,camera.eye.y,camera.eye.z};}
    // Immutable compiled input only. Native texture handles remain context-owned.
    export struct MeshBaseColorBinding final
    {
        epochengine::texture_artifact::ValidatedCompiledTextureArtifact artifact{};
        epochengine::canvas2d::LogicalTextureReference logical{};
        epochengine::canvas2d::SpriteSamplerDeclaration sampler{};
        bool alpha_cutout{};
        float alpha_cutoff{0.5f};
        bool modulate_vertex_color{true};
    };

    export [[nodiscard]] inline bool valid_mesh_base_color_binding(const MeshBaseColorBinding& binding) noexcept
    {
        return std::isfinite(binding.alpha_cutoff) && binding.alpha_cutoff >= 0.0f
            && binding.alpha_cutoff <= 1.0f
            && epochengine::canvas2d::valid(binding.sampler) && !binding.sampler.mipmapped
            && binding.sampler.maximum_anisotropy == 1.0f
            && static_cast<bool>(epochengine::texture_artifact::make_artifact_key(binding.artifact,
                binding.logical, 0, epochengine::texture_artifact::TransferUsage::base_color));
    }

    export [[nodiscard]] inline bool same_mesh_base_color_binding(
        const std::shared_ptr<const MeshBaseColorBinding>& lhs,
        const std::shared_ptr<const MeshBaseColorBinding>& rhs) noexcept
    {
        if(lhs==rhs) return true;
        if(!lhs||!rhs||!lhs->artifact||!rhs->artifact) return false;
        return lhs->logical==rhs->logical && lhs->sampler==rhs->sampler
            && lhs->alpha_cutout==rhs->alpha_cutout && lhs->alpha_cutoff==rhs->alpha_cutoff
            && lhs->modulate_vertex_color==rhs->modulate_vertex_color
            && lhs->artifact.artifact()->identity.key==rhs->artifact.artifact()->identity.key;
    }

    export struct TexturedVertex final
    {
        Vec3 position{};
        Vec3 color{};
        std::array<float, 2> uv{};
    };

    export struct PreciseMeshTransform final
    {
        std::array<double,3> position{},scale{1,1,1};
        std::array<double,9> rotation{1,0,0,0,1,0,0,0,1};
        [[nodiscard]] std::array<double,3> apply(const std::array<double,3>& point) const noexcept
        {
            const std::array local{point[0]*scale[0],point[1]*scale[1],point[2]*scale[2]};
            return {position[0]+rotation[0]*local[0]+rotation[1]*local[1]+rotation[2]*local[2],
                position[1]+rotation[3]*local[0]+rotation[4]*local[1]+rotation[5]*local[2],
                position[2]+rotation[6]*local[0]+rotation[7]*local[1]+rotation[8]*local[2]};
        }
        [[nodiscard]] std::array<double, 3> inverse_apply(const std::array<double, 3>& point) const noexcept
        {
            std::array<double, 3> result{};
            for (std::size_t axis = 0; axis < result.size(); ++axis)
            {
                for (std::size_t row = 0; row < result.size(); ++row)
                    result[axis] += rotation[row * 3 + axis] * (point[row] - position[row]);
                result[axis] /= scale[axis];
            }
            return result;
        }
        friend bool operator==(const PreciseMeshTransform&,const PreciseMeshTransform&) noexcept = default;
    };
    export [[nodiscard]] bool valid_precise_mesh_transform(const PreciseMeshTransform& value) noexcept;
    export [[nodiscard]] std::optional<PreciseMeshTransform> make_precise_mesh_transform(
        std::array<double,3> position,std::array<double,3> scale,Vec3 degrees) noexcept;

    export struct MeshPaintView;
    export struct PlanetAtmosphere final
    {
        std::array<double,3> world_center{};
        double radius{}, thickness{};
        friend bool operator==(const PlanetAtmosphere&,const PlanetAtmosphere&) noexcept = default;
    };
    export struct SphericalWaterView final
    {
        std::array<double,3> world_center{};
        double radius_meters{}, meters_per_unit{}, seconds{};
        friend bool operator==(const SphericalWaterView&,const SphericalWaterView&) noexcept = default;
    };
    export struct ObjectMarker
    {
        Vec3 position{};
        Vec3 color{ visual_rgb(epochengine::visuals::object_default()) };
        Vec3 scale{ 1.0f, 1.0f, 1.0f };
        Vec3 rotationDegrees{};
        float radius = 0.35f;
        ObjectPreviewPrimitive primitive{ ObjectPreviewPrimitive::Cube };
        bool selected = false;
        bool editorOnly = false;
        bool sampledRenderSurface = false;
        std::shared_ptr<const epochengine::asset::mesh::Artifact> mesh{};
        bool wireframe = false;
        std::shared_ptr<const MeshBaseColorBinding> baseColorTexture{};
        std::optional<PreciseMeshTransform> preciseTransform{};
        std::shared_ptr<const MeshPaintView> paintView{};
        bool forestLighting{};
        bool unlit{};
        bool neutralSurface{}; // Solid inspection keeps geometry, lighting and alpha coverage.
        std::uint64_t interactionOwner{}; // Transient editor handles only; never a scene transform.
        std::shared_ptr<const asset::mesh::SphericalWaterAppearance> sphericalWater{};
        std::optional<PlanetAtmosphere> planetAtmosphere{};
        std::optional<SphericalWaterView> waterDetail{};
    };

    // Presentation-only paint colors. Geometry, depth and authored materials stay intact.
    export struct MeshPaintRegion final
    {
        std::array<double,3> center{}, direction{};
        double radius{}, minimum_dot{};
        std::array<float,3> color{0.20f,0.66f,0.27f};
        friend bool operator==(const MeshPaintRegion&,const MeshPaintRegion&) noexcept = default;
    };
    export struct MeshPaintLasso final
    {
        std::array<double,3> center{}, direction{};
        double radius{};
        std::vector<std::array<double,3>> outline{};
        std::array<float,4> fill{}, edge{};
        friend bool operator==(const MeshPaintLasso&,const MeshPaintLasso&) noexcept = default;
    };
    export struct MeshPaintView final
    {
        std::array<double,3> origin{};
        double meters_per_unit{1.0};
        std::vector<MeshPaintRegion> regions;
        std::vector<MeshPaintLasso> lassos;
        friend bool operator==(const MeshPaintView&,const MeshPaintView&) noexcept = default;
    };
    export [[nodiscard]] inline Vec3 mesh_paint_albedo(const MeshPaintView& paint,
        const std::array<float,3>& position, const std::array<float,3>& terrain_albedo) noexcept
    {
        for(const auto& region:paint.regions)
        {
            std::array<double,3> delta{};double square{},projection{};
            for(std::size_t axis=0;axis<3;++axis) {
                delta[axis]=double(position[axis])*paint.meters_per_unit
                    +(paint.origin[axis]-region.center[axis]);
                square+=delta[axis]*delta[axis];projection+=delta[axis]*region.direction[axis]; }
            // Reject other bodies and back-facing caps before the angular test.
            if(square>region.radius*region.radius*0.25&&square<region.radius*region.radius*2.25
                &&projection>=std::sqrt(square)*region.minimum_dot)
                return {std::lerp(terrain_albedo[0],region.color[0],.42f),
                    std::lerp(terrain_albedo[1],region.color[1],.42f),
                    std::lerp(terrain_albedo[2],region.color[2],.42f)};
        }
        return {terrain_albedo[0],terrain_albedo[1],terrain_albedo[2]};
    }

    export struct SolidDrawMarker final
        {
            ObjectMarker marker{};
            std::shared_ptr<const std::vector<Vertex>> geometry{};
            std::shared_ptr<const std::vector<TexturedVertex>> textured{};
        };
    export struct WaterDetailRange final
    {
        std::uint32_t first_vertex{}, vertex_count{};
        friend bool operator==(const WaterDetailRange&,const WaterDetailRange&) noexcept = default;
    };
    export inline constexpr std::size_t maximum_water_detail_ranges=256u;
    export struct SolidDrawFrame final
        {
            epochengine::lighting::LightingFrame lighting{};
            Vec3 eye{};
            std::vector<SolidDrawMarker> markers{};
            std::shared_ptr<const std::vector<Vertex>> geometry{};
        std::vector<WaterDetailRange> water_detail_ranges{};
        };
    export [[nodiscard]] std::shared_ptr<const SolidDrawFrame> object_solid_frame_for(const void* ctxKey);

    // Texture rows carry the footprint in a tangent chart. The world point on
    // the reference sphere is the local origin to avoid subtracting large
    // body-coordinate offsets in fragment arithmetic.
    export inline constexpr std::size_t kMaximumMeshPaintCoverageRows = 65u;
    export inline constexpr std::size_t kMeshPaintCoverageRowTexels = 135u;
    export [[nodiscard]] inline std::vector<std::array<float,4>> pack_mesh_paint_coverage(
        const ObjectMarker& marker)
    {
        if (!marker.paintView || marker.paintView->lassos.empty()
            || marker.paintView->lassos.size() > kMaximumMeshPaintCoverageRows)
            return {};
        const auto& paint = *marker.paintView;
        if (!std::isfinite(paint.meters_per_unit) || paint.meters_per_unit <= 0.0)
            return {};
        const auto transform = marker.preciseTransform ? marker.preciseTransform
            : make_precise_mesh_transform({marker.position.x,marker.position.y,marker.position.z},
                {marker.scale.x,marker.scale.y,marker.scale.z},marker.rotationDegrees);
        if (!transform || !valid_precise_mesh_transform(*transform)) return {};

        constexpr std::size_t header = 7u;
        using Direction = std::array<double,3>;
        const auto dot = [](Direction a, Direction b) {
            return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];
        };
        const auto cross = [](Direction a, Direction b) {
            return Direction{a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],
                a[0]*b[1]-a[1]*b[0]};
        };
        const auto unit = [](Direction& value) {
            const auto length = std::hypot(value[0],value[1],value[2]);
            if (!std::isfinite(length) || length < 1e-12) return false;
            for (auto& coordinate : value) coordinate /= length;
            return true;
        };
        const auto valid_color = [](const auto& color) {
            return std::ranges::all_of(color, [](float v) {
                return std::isfinite(v) && v >= 0.0f && v <= 1.0f;
            });
        };
        std::vector<std::array<float,4>> packed;
        packed.reserve(paint.lassos.size() * kMeshPaintCoverageRowTexels);
        for (const auto& lasso : paint.lassos)
        {
            if (lasso.outline.size() < 3u || lasso.outline.size() > kMeshPaintCoverageRowTexels-header
                || !std::isfinite(lasso.radius) || lasso.radius <= 0.0
                || !valid_color(lasso.fill) || !valid_color(lasso.edge)) return {};
            auto depth = lasso.direction;
            if (!unit(depth)) return {};
            const Direction axis = std::abs(depth[1]) < .9 ? Direction{0,1,0} : Direction{1,0,0};
            auto horizontal = cross(axis, depth);
            if (!unit(horizontal)) return {};
            const auto vertical = cross(depth, horizontal);

            Direction meshAnchor{};
            for (std::size_t i=0; i<3u; ++i)
                meshAnchor[i] = ((lasso.center[i] - paint.origin[i]) + depth[i] * lasso.radius)
                    / paint.meters_per_unit;
            const auto anchor = transform->apply(meshAnchor);
            const auto draw_covector = [&](Direction vector) {
                std::array<float,4> result{};
                for (std::size_t world=0; world<3u; ++world)
                {
                    double coordinate{};
                    for (std::size_t local=0; local<3u; ++local)
                        coordinate += transform->rotation[world*3u+local]
                            * vector[local] / transform->scale[local];
                    result[world] = static_cast<float>(coordinate * (paint.meters_per_unit/lasso.radius));
                }
                return result;
            };
            std::array<std::array<float,4>,kMeshPaintCoverageRowTexels> row{};
            for (std::size_t i=0; i<3u; ++i) row[0][i]=static_cast<float>(anchor[i]);
            row[0][3]=static_cast<float>(lasso.outline.size());
            row[1]=draw_covector(horizontal);
            row[2]=draw_covector(vertical);
            row[3]=draw_covector(depth);
            for (std::size_t i=1u; i<=3u; ++i)
                if (std::ranges::all_of(row[i],[](float v) { return v==0.0f; })) return {};
            constexpr float inf = std::numeric_limits<float>::infinity();
            row[4]={inf,inf,-inf,-inf};
            row[5]=lasso.fill;
            row[6]=lasso.edge;
            for (std::size_t i=0; i<lasso.outline.size(); ++i)
            {
                auto point = lasso.outline[i];
                if (!unit(point)) return {};
                const auto denominator = dot(depth,point);
                if (denominator < .49) return {};
                const float x = static_cast<float>(dot(horizontal,point)/denominator);
                const float y = static_cast<float>(dot(vertical,point)/denominator);
                row[4][0]=(std::min)(row[4][0],x);
                row[4][1]=(std::min)(row[4][1],y);
                row[4][2]=(std::max)(row[4][2],x);
                row[4][3]=(std::max)(row[4][3],y);
                row[header+i]={x,y,0,0};
            }
            for (const auto& texel : row)
                if (!std::ranges::all_of(texel, [](float v) { return std::isfinite(v); })) return {};
            packed.insert(packed.end(),row.begin(),row.end());
        }
        return packed;
    }

    export struct MeshWireDrawFrame final
    {
        std::shared_ptr<const std::vector<Vertex>> depth_triangles{}, helper_lines{};
    };

    // Coordinates share the scene packet's render origin. Density parameters
    // remain metric; the orthonormal body rotation preserves authored orientation.
    export struct CloudVolume final
    {
        std::array<double,3> center{};
        double radius{}, meters_per_world_unit{1.0}, time_seconds{};
        std::array<double,9> rotation{1,0,0,0,1,0,0,0,1};
        asset::cloud_volume::Recipe recipe{};
        friend bool operator==(const CloudVolume&,const CloudVolume&)=default;
    };
    export [[nodiscard]] inline bool valid_cloud_volume(const CloudVolume& value) noexcept
    {
        if(!std::isfinite(value.radius)||value.radius<=0.0
            ||!std::isfinite(value.meters_per_world_unit)||value.meters_per_world_unit<=0.0
            ||!std::isfinite(value.time_seconds)||value.time_seconds<0.0||value.time_seconds>=1e12
            ||!asset::cloud_volume::valid(value.recipe,value.radius*value.meters_per_world_unit))return false;
        for(const auto coordinate:value.center)if(!std::isfinite(coordinate))return false;
        for(const auto element:value.rotation)if(!std::isfinite(element))return false;
        for(unsigned row=0;row<3u;++row)for(unsigned other=0;other<3u;++other)
        {
            double product{};for(unsigned axis=0;axis<3u;++axis)
                product+=value.rotation[row*3u+axis]*value.rotation[other*3u+axis];
            if(std::abs(product-(row==other?1.0:0.0))>1e-6)return false;
        }
        const auto& r=value.rotation;
        return std::abs(r[0]*(r[4]*r[8]-r[5]*r[7])-r[1]*(r[3]*r[8]-r[5]*r[6])
            +r[2]*(r[3]*r[7]-r[4]*r[6])-1.0)<1e-6;
    }
    export [[nodiscard]] inline bool admitted_cloud_volumes(std::span<const CloudVolume> volumes) noexcept
    {
        return volumes.size()<=6u&&std::ranges::all_of(volumes,valid_cloud_volume);
    }

    export struct CloudVolumeView final
    {
        std::array<float,3> center{};
        std::array<float,9> world_to_body{};
        std::array<float,2> wind{},evolution{};
        float radius_meters{},radius_world{},base{},thickness{},noise_scale{},view_occluder_radius{};
        float coverage{},extinction{},lower_extinction{};
        std::uint32_t seed{};
    };

    // The floating-point native raymarchers share this camera-relative,
    // body-normalized view of the immutable scene packet.
    export [[nodiscard]] inline std::optional<CloudVolumeView> cloud_volume_view(
        const CloudVolume& cloud,const Camera& camera) noexcept
    {
        if(!valid_cloud_volume(cloud))return {};
        const auto radius=cloud.radius*cloud.meters_per_world_unit;
        const auto& recipe=cloud.recipe;
        if(recipe.thickness_meters/radius<8.0*std::numeric_limits<float>::epsilon()
            ||cloud.radius>(std::numeric_limits<float>::max)()
            ||cloud.radius<(std::numeric_limits<float>::min)())return {};
        CloudVolumeView view;
        const std::array eye{camera.eye.x,camera.eye.y,camera.eye.z};
        for(std::size_t axis=0;axis<eye.size();++axis)
        {
            const auto coordinate=(cloud.center[axis]-eye[axis])/cloud.radius;
            if(!std::isfinite(coordinate)||std::abs(coordinate)>1e12)return {};
            view.center[axis]=static_cast<float>(coordinate);
        }
        view.radius_world=static_cast<float>(cloud.radius);
        view.radius_meters=static_cast<float>(radius);
        view.base=static_cast<float>(recipe.base_altitude_meters/radius);
        view.thickness=static_cast<float>(recipe.thickness_meters/radius);
        const auto occluder=recipe.view_occluder_radius_meters/radius;
        view.view_occluder_radius=static_cast<float>(occluder);
        if(view.view_occluder_radius>occluder)
            view.view_occluder_radius=std::nextafter(view.view_occluder_radius,0.0f);
        view.noise_scale=static_cast<float>(radius/recipe.horizontal_scale_meters);
        const auto angle=-std::fmod(cloud.time_seconds,recipe.wind_period_seconds)
            /recipe.wind_period_seconds*6.28318530717958647692;
        view.wind={static_cast<float>(std::cos(angle)),static_cast<float>(std::sin(angle))};
        const auto evolution=asset::cloud_volume::formation_phase(recipe,cloud.time_seconds);
        view.evolution={static_cast<float>(evolution[0]),static_cast<float>(evolution[1])};
        std::ranges::transform(cloud.rotation,view.world_to_body.begin(),[](double value){return static_cast<float>(value);});
        view.coverage=recipe.coverage;
        view.extinction=recipe.extinction_per_meter;
        view.lower_extinction=static_cast<float>(asset::cloud_volume::lower_deck_extinction(recipe.extinction_per_meter));
        view.seed=recipe.seed;
        return view;
    }

    export struct WaterFrameSource final
    {
        std::uint64_t terrain_request{};
        std::optional<double> sampled_seconds{};
    };

    export struct PublishedSceneFrame final
    {
        Camera camera{};
        Vec3 worldNavigationEye{};
        std::array<double,3> preciseNavigationEye{};
        std::shared_ptr<const SolidDrawFrame> solids{};
        std::shared_ptr<const MeshWireDrawFrame> wire{};
        std::shared_ptr<const std::vector<Vertex>> lines{};
        std::vector<ObjectMarker> markers{};
        std::uint64_t revision{};
        std::array<double,3> render_origin{};
        std::vector<CloudVolume> clouds{};
        bool space_background{};
        std::optional<double> water_detail_seconds{};
        WaterFrameSource water_source{};
    };

    export inline constexpr std::size_t water_crest_wave_count=12u;
    export inline constexpr std::size_t water_wave_count=water_crest_wave_count+3u;
    export struct WaterDetailParameters final
    {
        // Three bands of crest waves, followed by long waves that vary local direction.
        std::array<std::array<float,4>,water_wave_count> waves{}; // Wave vector and wrapped phase.
        std::array<std::array<float,4>,water_crest_wave_count/4u> slopes{{
            {0.082f,0.087f,0.081f,0.089f}, {0.078f,0.083f,0.076f,0.080f}, {0.064f,0.071f,0.063f,0.068f}}};
        Vec3 center_meters{}, eye{}, sun{};
        float meters_per_unit{}, sunlight{}, eye_altitude_meters{};
    };
    export [[nodiscard]] inline std::optional<WaterDetailParameters> water_detail_for(
        const PublishedSceneFrame& scene) noexcept
    {
        if(!scene.solids||scene.solids->water_detail_ranges.empty())return {};
        const SphericalWaterView* view{};
        for(const auto& marker:scene.markers)
            if(marker.waterDetail)
            {
                if(view)return {}; // One independently anchored water surface per draw frame.
                view=&*marker.waterDetail;
            }
        if(!view)return {};
        const double seconds=scene.water_detail_seconds.value_or(view->seconds);
        if(!std::isfinite(seconds)||std::abs(seconds)>1e12)return {};

        WaterDetailParameters result;
        const double scale=view->meters_per_unit;
        const std::array offset{(scene.render_origin[0]-view->world_center[0])*scale,
            (scene.render_origin[1]-view->world_center[1])*scale,
            (scene.render_origin[2]-view->world_center[2])*scale};
        const auto eye=scene.camera.eye;
        const double height=std::hypot(offset[0]+double(eye.x)*scale,offset[1]+double(eye.y)*scale,
            offset[2]+double(eye.z)*scale)-view->radius_meters;
        if(!std::isfinite(height)||height < -30.0||height > 220.0
            ||scale>(std::numeric_limits<float>::max)()||scale<(std::numeric_limits<float>::min)())return {};
        constexpr double turn=6.2831853071795864769;
        constexpr std::array<std::array<double,3>,water_wave_count> directions{{
            {0.48,0.13,0.87}, {-0.81,0.53,0.23}, {0.15,-0.89,0.43}, {0.74,0.45,-0.50},
            {0.58,-0.08,-0.81}, {-0.37,0.87,-0.31}, {0.91,-0.40,0.12}, {0.14,0.39,0.91},
            {0.08,-0.44,0.89}, {0.40,0.90,-0.18}, {-0.69,-0.61,0.38}, {0.73,-0.36,-0.58},
            {-0.21,0.91,0.33}, {0.36,-0.24,0.90}, {0.71,0.64,-0.32}}};
        constexpr std::array<double,water_wave_count> lengths{
            1.42,1.28,1.19,1.37, 0.91,0.77,0.67,0.82, 0.53,0.38,0.31,0.44, 3.9,5.7,8.1};
        for(std::size_t index=0;index<water_wave_count;++index)
        {
            const auto& direction=directions[index];
            const double k=turn/lengths[index];
            const double weight=k/std::hypot(direction[0],direction[1],direction[2]);
            double phase=2.399963229728653*static_cast<double>(index)-seconds*std::sqrt(9.81*k);
            for(std::size_t axis=0;axis<offset.size();++axis)
            {
                const double component=direction[axis]*weight;
                phase+=offset[axis]*component;
                result.waves[index][axis]=static_cast<float>(component);
            }
            result.waves[index][3]=static_cast<float>(std::remainder(phase,turn));
        }
        result.center_meters={static_cast<float>(-offset[0]),static_cast<float>(-offset[1]),static_cast<float>(-offset[2])};
        result.eye=eye;
        result.eye_altitude_meters=static_cast<float>(height);
        result.meters_per_unit=static_cast<float>(scale);
        for(const auto& entry:scene.solids->lighting.lights)
        {
            const auto& light=entry.desc;
            if(light.enabled&&light.kind==epochengine::lighting::LightKind::Directional
                &&light.intensity>result.sunlight)
            {
                result.sunlight=light.intensity;
                result.sun={-light.direction.x,-light.direction.y,-light.direction.z};
            }
        }
        result.sunlight=std::clamp(result.sunlight,0.0f,1.0f);
        return result;
    }

    export [[nodiscard]] constexpr bool object_marker_requests_wireframe(
        const ObjectMarker& marker) noexcept
    {
        if (!marker.wireframe)
            return false;
        if (marker.primitive == ObjectPreviewPrimitive::IndexedMesh)
            return bool(marker.mesh);
        return !marker.editorOnly && object_preview_geometry_route(marker.primitive).solid_scene;
    }

    export [[nodiscard]] constexpr bool object_marker_uses_solid_fill(
        const ObjectMarker& marker) noexcept
    {
        if (object_marker_requests_wireframe(marker))
            return false;
        if (!object_preview_geometry_route(marker.primitive).solid_scene)
            return false;
        if (!marker.editorOnly)
            return true;

        return marker.primitive != ObjectPreviewPrimitive::Camera
            && marker.primitive != ObjectPreviewPrimitive::Light;
    }

    export inline const std::array<float, 4> kClearColor = epochengine::visuals::scene_background();
    export [[nodiscard]] inline std::array<float, 4> scene_clear_color(const PublishedSceneFrame* frame) noexcept
    {
        return frame && frame->space_background ? std::array{0.0f, 0.0f, 0.0f, 1.0f} : kClearColor;
    }

    export inline constexpr Camera kCamera{};

    export enum class CameraMode : std::uint8_t
    {
        Editor = 0,
        FPS = 1,
        Canvas2D = 2
    };

    export struct CameraNavigationGestures final
    {
        bool orbiting{};
        bool panning{};
        bool dollying{};
        bool flying{};
    };

    export [[nodiscard]] constexpr CameraNavigationGestures
    resolve_camera_navigation_gestures(
        bool altHeld,
        bool leftMouseDown,
        bool middleMouseDown,
        bool rightMouseDown) noexcept
    {
        return CameraNavigationGestures{
            .orbiting = altHeld && leftMouseDown,
            .panning = middleMouseDown,
            .dollying = altHeld && rightMouseDown,
            .flying = !altHeld && rightMouseDown};
    }

    static_assert(resolve_camera_navigation_gestures(
        false, false, true, false).panning);
    static_assert(resolve_camera_navigation_gestures(
        true, true, false, false).orbiting);
    static_assert(resolve_camera_navigation_gestures(
        true, false, false, true).dollying);
    static_assert(resolve_camera_navigation_gestures(
        false, false, false, true).flying);

    export [[nodiscard]] inline std::string_view camera_mode_name(CameraMode mode) noexcept
    {
        switch (mode)
        {
        case CameraMode::Canvas2D: return "2D Canvas";
        case CameraMode::FPS: return "FPS";
        case CameraMode::Editor:
        default: return "Editor";
        }
    }

    export enum class EditorView : std::uint8_t
    {
        Perspective = 0,
        FreeOrthographic,
        Front,
        Back,
        Left,
        Right,
        Top,
        Bottom
    };

    export [[nodiscard]] constexpr std::string_view editor_view_name(EditorView view) noexcept
    {
        switch (view)
        {
        case EditorView::FreeOrthographic: return "Orthographic";
        case EditorView::Front: return "Front";
        case EditorView::Back: return "Back";
        case EditorView::Left: return "Left";
        case EditorView::Right: return "Right";
        case EditorView::Top: return "Top";
        case EditorView::Bottom: return "Bottom";
        case EditorView::Perspective:
        default: return "Perspective";
        }
    }

    export struct CameraRigSnapshot
    {
        bool valid{ false };
        CameraMode mode{ CameraMode::Editor };
        Vec3 focus{ 0.0f, 0.0f, 0.0f };
        Vec3 position{ 0.0f, 1.8f, 6.0f };
        float yaw_degrees = -135.0f;
        float pitch_degrees = -28.0f;
        float free_yaw_degrees = -135.0f;
        float free_pitch_degrees = -28.0f;
        float distance = 13.5f;
        float orthographic_vertical_size = 11.34f;
        float fly_speed = 6.5f;
        render_camera::ProjectionKind projection{ render_camera::ProjectionKind::perspective };
        render_camera::ViewOrientation orientation{ render_camera::ViewOrientation::free };
        std::array<Vec3, 8> view_focuses{};
        std::array<float, 8> view_distances{};
        std::array<float, 8> view_orthographic_sizes{};
        std::uint64_t logical_view_id{};
        std::uint32_t logical_view_generation{ 1u };
        std::uint64_t revision = 1;
        float scene_extent{};
        float roll_degrees{};
        std::array<double,3> navigation_origin{};
    };
    export [[nodiscard]] inline Vec3 subtract(Vec3 lhs, Vec3 rhs) noexcept
    {
        return { lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z };
    }
    export [[nodiscard]] inline Vec3 add(Vec3 lhs, Vec3 rhs) noexcept
    {
        return { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z };
    }
    export [[nodiscard]] inline Vec3 scale(Vec3 value, float factor) noexcept
    {
        return { value.x * factor, value.y * factor, value.z * factor };
    }
    namespace detail
    {
        struct EulerRotation final {float sinX{},cosX{},sinY{},cosY{},sinZ{},cosZ{};};
        [[nodiscard]] inline EulerRotation euler_rotation(Vec3 rotationDegrees) noexcept
        {
        constexpr float degreesToRadians = 0.01745329251994329577f;
        const float x = rotationDegrees.x * degreesToRadians;
        const float y = rotationDegrees.y * degreesToRadians;
        const float z = rotationDegrees.z * degreesToRadians;
        const float sinX = std::sin(x);
        const float cosX = std::cos(x);
        const float sinY = std::sin(y);
        const float cosY = std::cos(y);
        const float sinZ = std::sin(z);
        const float cosZ = std::cos(z);
        return {sinX,cosX,sinY,cosY,sinZ,cosZ};
        }
        [[nodiscard]] inline Vec3 rotate_euler(Vec3 value,const EulerRotation& rotation) noexcept
        {
        const auto [sinX,cosX,sinY,cosY,sinZ,cosZ]=rotation;
        const Vec3 aroundZ{
            value.x * cosZ - value.y * sinZ,
            value.x * sinZ + value.y * cosZ,
            value.z
        };
        const Vec3 aroundY{
            aroundZ.x * cosY + aroundZ.z * sinY,
            aroundZ.y,
            -aroundZ.x * sinY + aroundZ.z * cosY
        };
        return {
            aroundY.x,
            aroundY.y * cosX - aroundY.z * sinX,
            aroundY.y * sinX + aroundY.z * cosX
        };
        }
        [[nodiscard]] inline Vec3 mesh_vertex_position(const ObjectMarker& marker,
            const std::array<float,3>& source, const EulerRotation& rotation) noexcept
        {
            if(marker.preciseTransform)
            {
                const auto point=marker.preciseTransform->apply({source[0],source[1],source[2]});
                return {static_cast<float>(point[0]),static_cast<float>(point[1]),static_cast<float>(point[2])};
            }

            return add(marker.position, rotate_euler({source[0]*marker.scale.x,
                source[1]*marker.scale.y, source[2]*marker.scale.z}, rotation));
        }
        [[nodiscard]] inline Vec3 mesh_vertex_normal(const ObjectMarker& marker,
            const std::array<float,3>& source,const EulerRotation& rotation) noexcept
        {
            if(marker.preciseTransform)
            {
                // Position uses R*S. Its normal uses inverse-transpose R/S,
                // including a composed provider orientation and nonuniform scale.
                const auto& transform=*marker.preciseTransform;
                const auto& r=transform.rotation;
                const std::array local{double(source[0])/transform.scale[0],
                    double(source[1])/transform.scale[1],double(source[2])/transform.scale[2]};
                const double x=r[0]*local[0]+r[1]*local[1]+r[2]*local[2];
                const double y=r[3]*local[0]+r[4]*local[1]+r[5]*local[2];
                const double z=r[6]*local[0]+r[7]*local[1]+r[8]*local[2];
                const double magnitude=std::hypot(x,y,z);
                if(!std::isfinite(magnitude)||magnitude<=0.0)return {};
                return {float(x/magnitude),float(y/magnitude),float(z/magnitude)};
            }
            return rotate_euler({source[0]/marker.scale.x,source[1]/marker.scale.y,
                source[2]/marker.scale.z},rotation);
        }
    }
    namespace detail
    {
        [[nodiscard]] inline bool mesh_source_reverse(const ObjectMarker& marker) noexcept {
            // Choose one coherent source winding before emission. Shared skirt
            // vertices carry surface normals almost tangent to their side faces;
            // their tiny signed dots cannot establish a different winding.
            bool sourceCCW{};
            if(marker.mesh) {
                const auto vertices=marker.mesh->vertices();const auto indices=marker.mesh->indices();
                for(std::size_t i=0;i+2u<indices.size();i+=3u) {
                    const auto& a=vertices[indices[i]];const auto& b=vertices[indices[i+1u]];const auto& c=vertices[indices[i+2u]];
                    const double abx=double(b.position[0])-a.position[0],aby=double(b.position[1])-a.position[1],abz=double(b.position[2])-a.position[2];
                    const double acx=double(c.position[0])-a.position[0],acy=double(c.position[1])-a.position[1],acz=double(c.position[2])-a.position[2];
                    const double nx=double(a.normal[0])+b.normal[0]+c.normal[0],ny=double(a.normal[1])+b.normal[1]+c.normal[1],nz=double(a.normal[2])+b.normal[2]+c.normal[2];
                    const double x=aby*acz-abz*acy,y=abz*acx-abx*acz,z=abx*acy-aby*acx;
                    const double facing=x*nx+y*ny+z*nz, evidence=(x*x+y*y+z*z)*(nx*nx+ny*ny+nz*nz);
                    // Require cos(60 degrees) agreement; ambiguous normals keep the existing CW fallback.
                    if(evidence>0.0&&facing*facing>=evidence*0.25){sourceCCW=facing>0.0;break;}
                }
            }
            bool reflected=(marker.scale.x<0)^(marker.scale.y<0)^(marker.scale.z<0);
            if(marker.preciseTransform) {const auto& transform=*marker.preciseTransform;const auto& r=transform.rotation;
                const double determinant=r[0]*(r[4]*r[8]-r[5]*r[7])-r[1]*(r[3]*r[8]-r[5]*r[6])+r[2]*(r[3]*r[7]-r[4]*r[6]);reflected=(transform.scale[0]<0)^(transform.scale[1]<0)^(transform.scale[2]<0)^(determinant<0);
            }
            return sourceCCW^reflected;
        }
    }
    export [[nodiscard]] inline Vec3 rotate_euler_degrees(Vec3 value,Vec3 rotationDegrees) noexcept
    {
        return detail::rotate_euler(value,detail::euler_rotation(rotationDegrees));
    }

    export [[nodiscard]] inline float dot(Vec3 lhs, Vec3 rhs) noexcept
    {
        return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
    }

    export [[nodiscard]] inline Vec3 cross(Vec3 lhs, Vec3 rhs) noexcept
    {
        return {
            lhs.y * rhs.z - lhs.z * rhs.y,
            lhs.z * rhs.x - lhs.x * rhs.z,
            lhs.x * rhs.y - lhs.y * rhs.x
        };
    }

    export [[nodiscard]] constexpr bool clockwise_solid_triangle_faces_camera(
        Vec3 a,
        Vec3 b,
        Vec3 c,
        Vec3 cameraEye) noexcept
    {
        const Vec3 edgeAB{ b.x - a.x, b.y - a.y, b.z - a.z };
        const Vec3 edgeAC{ c.x - a.x, c.y - a.y, c.z - a.z };
        const Vec3 windingNormal{
            edgeAB.y * edgeAC.z - edgeAB.z * edgeAC.y,
            edgeAB.z * edgeAC.x - edgeAB.x * edgeAC.z,
            edgeAB.x * edgeAC.y - edgeAB.y * edgeAC.x
        };
        const Vec3 center{
            (a.x + b.x + c.x) / 3.0f,
            (a.y + b.y + c.y) / 3.0f,
            (a.z + b.z + c.z) / 3.0f
        };
        const Vec3 toCamera{
            cameraEye.x - center.x,
            cameraEye.y - center.y,
            cameraEye.z - center.z
        };

        return windingNormal.x * toCamera.x
            + windingNormal.y * toCamera.y
            + windingNormal.z * toCamera.z < -1.0e-6f;
    }

    static_assert(clockwise_solid_triangle_faces_camera(
        { -1.0f, 1.0f, -1.0f },
        { 1.0f, 1.0f, -1.0f },
        { 1.0f, 1.0f, 1.0f },
        { 0.0f, 2.0f, 0.0f }));
    static_assert(!clockwise_solid_triangle_faces_camera(
        { -1.0f, -1.0f, -1.0f },
        { -1.0f, -1.0f, 1.0f },
        { 1.0f, -1.0f, 1.0f },
        { 0.0f, 2.0f, 0.0f }));
    // Wire triangles use the same outward-clockwise contract as solid preview.
    // This removes back-face edges; hidden-surface depth remains backend-owned.
    export template<class Emit>
    inline void for_each_visible_marker_line(const MeshWireDrawFrame& wire,
        std::span<const Vertex> fallback, Vec3 eye, Emit emit)
    {
        if(wire.depth_triangles && !wire.depth_triangles->empty())
        {
            const auto& triangles=*wire.depth_triangles;
            for(std::size_t i=0;i+2u<triangles.size();i+=3u)
            {
                const auto& a=triangles[i]; const auto& b=triangles[i+1u];
                const auto& c=triangles[i+2u];
                if(!clockwise_solid_triangle_faces_camera(a.position,b.position,c.position,eye)) continue;
                emit(a,b); emit(b,c); emit(c,a);
            }
            if(wire.helper_lines) fallback=*wire.helper_lines;
            else return;
        }
        for(std::size_t i=0;i+1u<fallback.size();i+=2u) emit(fallback[i],fallback[i+1u]);
    }

    export [[nodiscard]] inline Vec3 normalize(Vec3 value) noexcept
    {
        const float lengthSq = dot(value, value);
        if (lengthSq <= 1.0e-8f)
            return { 0.0f, 0.0f, 0.0f };

        const float invLength = 1.0f / std::sqrt(lengthSq);
        return { value.x * invLength, value.y * invLength, value.z * invLength };
    }
    export [[nodiscard]] inline Vec3 camera_forward(const Camera& camera) noexcept
    {return camera.navigationValid?camera.navigation.forward:normalize(subtract(camera.target,camera.eye));}

    export [[nodiscard]] Mat4 identity_matrix() noexcept;

    export [[nodiscard]] Mat4 multiply(const Mat4& lhs, const Mat4& rhs) noexcept;

    export [[nodiscard]] Mat4 perspective(
        float fovRadians,
        float aspect,
        float zNear,
        float zFar) noexcept;

    export [[nodiscard]] Mat4 orthographic(
        float left,
        float right,
        float bottom,
        float top,
        float zNear,
        float zFar) noexcept;

    export [[nodiscard]] Mat4 look_at(Vec3 eye, Vec3 target, Vec3 up) noexcept;

    export struct ClipVertex
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        float w = 1.0f;
    };

    export struct ClippedTriangle final
    {
        // A convex triangle can gain at most one vertex per frustum plane.
        std::array<ClipVertex, 9> vertices{};
        std::array<unsigned, 9> originalEdges{};
        std::size_t count{};
    };

    export using ClipColor = std::array<float, 3>;

    export struct ClippedColoredTriangle final
    {
        ClippedTriangle geometry{};
        std::array<ClipColor, 9> colors{};
    };

    template<bool KeepOriginalEdges, bool KeepColors>
    [[nodiscard]] inline auto clip_triangle_impl(
        const std::array<ClipVertex, 3>& triangle,
        std::conditional_t<KeepColors, const std::array<ClipColor, 3>&, std::nullptr_t> triangleColors) noexcept
        -> std::conditional_t<KeepColors, ClippedColoredTriangle, ClippedTriangle>
    {
        constexpr std::array<ClipVertex, 6> planes{{
            {0, 0, 1, 1}, {0, 0, -1, 1}, {1, 0, 0, 1},
            {-1, 0, 0, 1}, {0, 1, 0, 1}, {0, -1, 0, 1}
        }};
        for (const auto& vertex : triangle)
        {
            if (!std::isfinite(vertex.x) || !std::isfinite(vertex.y)
                || !std::isfinite(vertex.z) || !std::isfinite(vertex.w))
                return {};
        }

        ClippedTriangle polygon{};
        polygon.count = triangle.size();
        std::copy(triangle.begin(), triangle.end(), polygon.vertices.begin());
        using ColorStorage = std::array<ClipColor, KeepColors ? 9 : 0>;
        ColorStorage colors{};
        if constexpr (KeepColors)
            std::copy(triangleColors.begin(), triangleColors.end(), colors.begin());
        if constexpr (KeepOriginalEdges)
            polygon.originalEdges = {6u, 5u, 3u};

        for (const auto& plane : planes)
        {
            const auto distance = [&](const ClipVertex& point) noexcept
            {
                return static_cast<double>(plane.x) * point.x
                    + static_cast<double>(plane.y) * point.y
                    + static_cast<double>(plane.z) * point.z + point.w;
            };
            const auto end = polygon.vertices.begin() + polygon.count;
            if (std::all_of(polygon.vertices.begin(), end, [&](const auto& p) { return distance(p) >= 0.0; }))
                continue;

            ClippedTriangle next{};
            ColorStorage nextColors{};
            std::size_t previousIndex = polygon.count - 1;
            double previousDistance = distance(polygon.vertices[previousIndex]);
            for (std::size_t index = 0; index < polygon.count; ++index)
            {
                const auto& previous = polygon.vertices[previousIndex];
                const auto& current = polygon.vertices[index];
                const double currentDistance = distance(current);
                if ((previousDistance < 0.0 && currentDistance > 0.0)
                    || (previousDistance > 0.0 && currentDistance < 0.0))
                {
                    if (next.count == next.vertices.size())
                        return {};
                    const auto t = static_cast<float>(previousDistance / (previousDistance - currentDistance));
                    next.vertices[next.count] = {
                        std::lerp(previous.x, current.x, t), std::lerp(previous.y, current.y, t),
                        std::lerp(previous.z, current.z, t), std::lerp(previous.w, current.w, t)
                    };
                    if constexpr (KeepColors)
                        for (std::size_t channel = 0; channel < 3; ++channel)
                            nextColors[next.count][channel] = std::lerp(colors[previousIndex][channel], colors[index][channel], t);
                    if constexpr (KeepOriginalEdges)
                        next.originalEdges[next.count] = polygon.originalEdges[previousIndex] & polygon.originalEdges[index];
                    ++next.count;
                }
                if (currentDistance >= 0.0)
                {
                    if (next.count == next.vertices.size())
                        return {};
                    next.vertices[next.count] = current;
                    if constexpr (KeepColors)
                        nextColors[next.count] = colors[index];
                    if constexpr (KeepOriginalEdges)
                        next.originalEdges[next.count] = polygon.originalEdges[index];
                    ++next.count;
                }
                previousIndex = index;
                previousDistance = currentDistance;
            }
            if (next.count < 3)
                return {};
            polygon = next;
            if constexpr (KeepColors)
                colors = nextColors;
        }
        if constexpr (KeepColors)
            return ClippedColoredTriangle{polygon, colors};
        else
            return polygon;
    }

    export template<bool KeepOriginalEdges = false>
    [[nodiscard]] inline ClippedTriangle clip_triangle(const std::array<ClipVertex, 3>& triangle) noexcept
    {
        return clip_triangle_impl<KeepOriginalEdges, false>(triangle, nullptr);
    }

    export [[nodiscard]] inline ClippedColoredTriangle clip_colored_triangle(
        const std::array<ClipVertex, 3>& triangle, const std::array<ClipColor, 3>& colors) noexcept
    {
        return clip_triangle_impl<false, true>(triangle, colors);
    }

    export [[nodiscard]] inline ClipVertex transform_point(const Mat4& matrix, Vec3 position) noexcept
    {
        ClipVertex out{};
        out.x = matrix[0] * position.x + matrix[4] * position.y + matrix[8] * position.z + matrix[12];
        out.y = matrix[1] * position.x + matrix[5] * position.y + matrix[9] * position.z + matrix[13];
        out.z = matrix[2] * position.x + matrix[6] * position.y + matrix[10] * position.z + matrix[14];
        out.w = matrix[3] * position.x + matrix[7] * position.y + matrix[11] * position.z + matrix[15];
        return out;
    }

    namespace detail
    {
        struct PtrHash
        {
            std::size_t operator()(const void* p) const noexcept
            {
                return std::hash<const void*>{}(p);
            }
        };

        struct CameraRigState
        {
            CameraMode mode{ CameraMode::Editor };
            Vec3 focus{ 0.0f, 0.0f, 0.0f };
            Vec3 position{ 0.0f, 1.8f, 6.0f };
            float yawDegrees = -135.0f;
            float pitchDegrees = -28.0f;
            float freeYawDegrees = -135.0f;
            float freePitchDegrees = -28.0f;
            float distance = 13.5f;
            float orthographicVerticalSize = 11.34f;
            float flySpeed = 6.5f;
            render_camera::ProjectionKind projection{ render_camera::ProjectionKind::perspective };
            render_camera::ViewOrientation orientation{ render_camera::ViewOrientation::free };
            std::array<Vec3, 8> viewFocuses{};
            std::array<float, 8> viewDistances{};
            std::array<float, 8> viewOrthographicSizes{};
            std::uint64_t logicalViewId{};
            std::uint32_t logicalViewGeneration{ 1u };
            std::uint64_t revision = 1;
            bool restoredFromSnapshot{};
            std::optional<Camera> externalFrame{};
            float sceneExtent{};
            float rollDegrees{};
            std::array<double,3> navigationOrigin{};
        };
        export inline std::unordered_map<const void*, CameraRigState, PtrHash> g_cameraRigs{};
        export inline std::unordered_map<const void*, Vec3, PtrHash> g_lastMarkerHits{};
        struct MeshWireEdges final
        {
            std::shared_ptr<const epochengine::asset::mesh::Artifact> mesh{};
            std::shared_ptr<const std::vector<std::uint64_t>> edges{};
        };
        using SolidMarkerGeometry=SolidDrawMarker;
        using SolidGeometryFrame=SolidDrawFrame;
        struct PublishedObjectMarkers final
        {
            std::vector<ObjectMarker> markers{};
            std::vector<MeshWireEdges> wire_edges{};
            std::shared_ptr<const std::vector<Vertex>> line_geometry{};
            std::shared_ptr<const SolidGeometryFrame> solid_geometry{};
            std::shared_ptr<const PublishedSceneFrame> scene_frame{};
            std::shared_ptr<const MeshWireDrawFrame> mesh_wire_geometry{};
        };
        enum class MarkerLinePass : std::uint8_t { combined, helpers };
        [[nodiscard]] inline std::vector<Vertex> build_marker_lines(std::span<const ObjectMarker> markers,
            std::span<const MeshWireEdges> wire_edges, MarkerLinePass pass = MarkerLinePass::combined);
        [[nodiscard]] inline std::vector<Vertex> build_marker_solids(std::span<const ObjectMarker> markers,
            const epochengine::lighting::LightingFrame& lightingFrame,const Camera& camera,
            std::vector<TexturedVertex>* textured = nullptr, bool wire_depth = false,
            std::vector<WaterDetailRange>* water_detail = nullptr);
        [[nodiscard]] inline std::uint64_t mesh_wire_frame_bytes(const MeshWireDrawFrame& frame) noexcept
        {   std::uint64_t bytes=sizeof(frame);
            for(const auto* vertices:{&frame.depth_triangles,&frame.helper_lines})
                if(*vertices) bytes+=sizeof(std::vector<Vertex>)+(*vertices)->capacity()*sizeof(Vertex);
            return bytes;
        }
        [[nodiscard]] inline std::uint64_t solid_frame_bytes(const SolidGeometryFrame& frame, bool include_mesh_sources=true) noexcept;
        [[nodiscard]] inline bool solid_cache_admitted(const PublishedObjectMarkers& frame,
            const epochengine::lighting::LightingFrame& lights, Vec3 eye,std::uint64_t extra_bytes=0u,
            bool framed_batch=false) noexcept;
        export inline std::unordered_map<const void*, PublishedObjectMarkers, PtrHash> g_objectMarkers{};
        export inline std::unordered_map<const void*, epochengine::lighting::LightingFrame, PtrHash> g_lightingFrames{};
        export inline std::unordered_map<const void*, std::uint64_t, PtrHash> g_geometryRevisions{};
        struct AuthoringGuidesState final
        {
            std::shared_ptr<const GridGeometry> grid{};
            bool visible{true};
        };
        export inline std::unordered_map<const void*, AuthoringGuidesState, PtrHash> g_authoringGuides{};
        inline std::shared_mutex g_cameraRigMutex{};
        inline std::shared_mutex g_objectMarkerMutex{};
        inline std::shared_mutex g_lightingFrameMutex{};
        inline std::shared_mutex g_geometryRevisionMutex{};
        inline std::shared_mutex g_gridGeometryMutex{};
        inline std::uint64_t g_nextLogicalViewId{ 1u };

        inline void touch_geometry(const void* ctxKey) noexcept
        {
            std::unique_lock lock(g_geometryRevisionMutex);
            auto& revision = g_geometryRevisions[ctxKey];
            revision = revision == (std::numeric_limits<std::uint64_t>::max)() ? 1u : revision + 1u;
        }

        [[nodiscard]] inline bool same_water_appearance(const asset::mesh::SphericalWaterAppearance& lhs,
            const asset::mesh::SphericalWaterAppearance& rhs) noexcept
        {
            return lhs.artifact_key == rhs.artifact_key && lhs.ranges == rhs.ranges
                && lhs.spray_count == rhs.spray_count && lhs.spray_count <= lhs.spray_sites.size()
                && std::equal(lhs.spray_sites.begin(), lhs.spray_sites.begin() + lhs.spray_count,
                    rhs.spray_sites.begin(), [](const auto& a, const auto& b) {
                        return a.cell == b.cell && a.position == b.position && a.strength == b.strength;
                    });
        }

        [[nodiscard]] inline bool same_marker(const ObjectMarker& lhs, const ObjectMarker& rhs) noexcept
        {
            return lhs.position.x == rhs.position.x && lhs.position.y == rhs.position.y && lhs.position.z == rhs.position.z
                && lhs.color.x == rhs.color.x && lhs.color.y == rhs.color.y && lhs.color.z == rhs.color.z
                && lhs.scale.x == rhs.scale.x && lhs.scale.y == rhs.scale.y && lhs.scale.z == rhs.scale.z
                && lhs.rotationDegrees.x == rhs.rotationDegrees.x
                && lhs.rotationDegrees.y == rhs.rotationDegrees.y
                && lhs.rotationDegrees.z == rhs.rotationDegrees.z
                && lhs.radius == rhs.radius && lhs.primitive == rhs.primitive && lhs.selected == rhs.selected
                && lhs.editorOnly == rhs.editorOnly && lhs.sampledRenderSurface == rhs.sampledRenderSurface
                && lhs.wireframe == rhs.wireframe && lhs.forestLighting == rhs.forestLighting && lhs.unlit == rhs.unlit && lhs.neutralSurface == rhs.neutralSurface
                && lhs.preciseTransform == rhs.preciseTransform
                && lhs.planetAtmosphere == rhs.planetAtmosphere
                && lhs.waterDetail == rhs.waterDetail
                && lhs.interactionOwner == rhs.interactionOwner
                && (lhs.sphericalWater==rhs.sphericalWater
                    || (lhs.sphericalWater&&rhs.sphericalWater
                        && same_water_appearance(*lhs.sphericalWater,*rhs.sphericalWater)))
                && bool(lhs.paintView)==bool(rhs.paintView)
                && (!lhs.paintView || *lhs.paintView==*rhs.paintView)
                && same_mesh_base_color_binding(lhs.baseColorTexture,rhs.baseColorTexture)
                && bool(lhs.mesh) == bool(rhs.mesh)
                && (!lhs.mesh || lhs.mesh->key() == rhs.mesh->key());
        }

        [[nodiscard]] inline const void* normalize_camera_key(const void* ctxKey) noexcept
        {
            return ctxKey;
        }

        [[nodiscard]] inline CameraRigState make_editor_rig() noexcept
        {
            return CameraRigState{
                .mode = CameraMode::Editor,
                .focus{ 0.0f, 0.0f, 0.0f },
                .position{ 0.0f, 1.8f, 6.0f },
                .yawDegrees = -135.0f,
                .pitchDegrees = -28.0f,
                .freeYawDegrees = -135.0f,
                .freePitchDegrees = -28.0f,
                .distance = 13.5f,
                .orthographicVerticalSize = 11.34f,
                .flySpeed = 6.5f,
                .projection = render_camera::ProjectionKind::perspective,
                .orientation = render_camera::ViewOrientation::free
            };
        }

        [[nodiscard]] inline CameraRigState make_fps_rig() noexcept
        {
            return CameraRigState{
                .mode = CameraMode::FPS,
                .focus{ 0.0f, 0.75f, 0.0f },
                .position{ 0.0f, 1.8f, 6.0f },
                .yawDegrees = -90.0f,
                .pitchDegrees = -8.0f,
                .freeYawDegrees = -90.0f,
                .freePitchDegrees = -8.0f,
                .distance = 0.0f,
                .orthographicVerticalSize = 10.0f,
                .flySpeed = 6.5f,
                .projection = render_camera::ProjectionKind::perspective,
                .orientation = render_camera::ViewOrientation::free
            };
        }

        [[nodiscard]] inline CameraRigState make_canvas_2d_rig() noexcept
        {
            return CameraRigState{
                .mode = CameraMode::Canvas2D,
                .focus{ 0.0f, 1.8f, 0.0f },
                .position{ 0.0f, 1.8f, 8.0f },
                .yawDegrees = -90.0f,
                .pitchDegrees = 0.0f,
                .freeYawDegrees = -90.0f,
                .freePitchDegrees = 0.0f,
                .distance = 8.0f,
                .orthographicVerticalSize = 6.72f,
                .flySpeed = 6.5f,
                .projection = render_camera::ProjectionKind::orthographic,
                .orientation = render_camera::ViewOrientation::free
            };
        }

        [[nodiscard]] inline CameraRigState make_default_rig(CameraMode mode) noexcept
        {
            switch (mode)
            {
            case CameraMode::FPS:
                return make_fps_rig();
            case CameraMode::Canvas2D:
                return make_canvas_2d_rig();
            case CameraMode::Editor:
            default:
                return make_editor_rig();
            }
        }

        [[nodiscard]] inline Vec3 forward_from_angles(float yawDegrees, float pitchDegrees) noexcept
        {
            constexpr float kPi = 3.14159265358979323846f;
            const float yawRadians = yawDegrees * (kPi / 180.0f);
            const float pitchRadians = pitchDegrees * (kPi / 180.0f);
            return normalize(Vec3{
                std::cos(pitchRadians) * std::cos(yawRadians),
                std::sin(pitchRadians),
                std::cos(pitchRadians) * std::sin(yawRadians)
            });
        }

        [[nodiscard]] inline float navigation_pitch(CameraMode mode, double degrees) noexcept
        {
            return static_cast<float>(mode == CameraMode::Editor
                ? std::remainder(degrees, 360.0)
                : (std::clamp)(degrees, -85.0, 85.0));
        }

        [[nodiscard]] inline Vec3 flat_forward_from_angles(float yawDegrees, float pitchDegrees) noexcept
        {
            Vec3 flatForward = normalize(forward_from_angles(yawDegrees, pitchDegrees));
            flatForward.y = 0.0f;
            flatForward = normalize(flatForward);
            if (dot(flatForward, flatForward) <= 1.0e-6f)
                flatForward = { 0.0f, 0.0f, -1.0f };
            return flatForward;
        }

        [[nodiscard]] inline Vec3 right_from_angles(float yawDegrees, float pitchDegrees) noexcept
        {
            const Vec3 worldUp{ 0.0f, 1.0f, 0.0f };
            return normalize(cross(flat_forward_from_angles(yawDegrees, pitchDegrees), worldUp));
        }

        inline void touch_rig(CameraRigState& rig) noexcept
        {
            if (rig.revision == (std::numeric_limits<std::uint64_t>::max)())
                rig.revision = 1;
            else
                ++rig.revision;
        }

        [[nodiscard]] inline CameraMode sanitize_camera_mode(CameraMode mode) noexcept
        {
            switch (mode)
            {
            case CameraMode::FPS:
            case CameraMode::Canvas2D:
            case CameraMode::Editor:
                return mode;
            default:
                return CameraMode::Editor;
            }
        }

        [[nodiscard]] constexpr EditorView editor_view_from_rig(const CameraRigState& rig) noexcept
        {
            if (rig.projection == render_camera::ProjectionKind::perspective)
                return EditorView::Perspective;
            switch (rig.orientation)
            {
            case render_camera::ViewOrientation::front: return EditorView::Front;
            case render_camera::ViewOrientation::back: return EditorView::Back;
            case render_camera::ViewOrientation::left: return EditorView::Left;
            case render_camera::ViewOrientation::right: return EditorView::Right;
            case render_camera::ViewOrientation::top: return EditorView::Top;
            case render_camera::ViewOrientation::bottom: return EditorView::Bottom;
            case render_camera::ViewOrientation::free:
            default: return EditorView::FreeOrthographic;
            }
        }

        [[nodiscard]] constexpr render_camera::ViewOrientation orientation_for_editor_view(
            EditorView view) noexcept
        {
            switch (view)
            {
            case EditorView::Front: return render_camera::ViewOrientation::front;
            case EditorView::Back: return render_camera::ViewOrientation::back;
            case EditorView::Left: return render_camera::ViewOrientation::left;
            case EditorView::Right: return render_camera::ViewOrientation::right;
            case EditorView::Top: return render_camera::ViewOrientation::top;
            case EditorView::Bottom: return render_camera::ViewOrientation::bottom;
            case EditorView::FreeOrthographic:
            case EditorView::Perspective:
            default: return render_camera::ViewOrientation::free;
            }
        }

        [[nodiscard]] constexpr std::size_t editor_view_index(EditorView view) noexcept
        {
            const auto index = static_cast<std::size_t>(view);
            return index < 8u ? index : 0u;
        }

        inline void save_active_view(CameraRigState& rig) noexcept
        {
            if (rig.mode != CameraMode::Editor)
                return;
            const std::size_t index = editor_view_index(editor_view_from_rig(rig));
            rig.viewFocuses[index] = rig.focus;
            rig.viewDistances[index] = rig.distance;
            rig.viewOrthographicSizes[index] = rig.orthographicVerticalSize;
        }

        inline void restore_or_seed_view(CameraRigState& rig, EditorView view) noexcept
        {
            const std::size_t index = editor_view_index(view);
            if (rig.viewDistances[index] > 0.0f)
            {
                rig.focus = rig.viewFocuses[index];
                rig.distance = rig.viewDistances[index];
            }
            else
            {
                rig.viewFocuses[index] = rig.focus;
                rig.viewDistances[index] = rig.distance;
            }
            if (rig.viewOrthographicSizes[index] > 0.0f)
                rig.orthographicVerticalSize = rig.viewOrthographicSizes[index];
            else
                rig.viewOrthographicSizes[index] = rig.orthographicVerticalSize;
        }

        [[nodiscard]] inline float finite_or(float value, float fallback) noexcept
        {
            return std::isfinite(value) ? value : fallback;
        }

        [[nodiscard]] inline Vec3 finite_vec_or(Vec3 value, Vec3 fallback) noexcept
        {
            return Vec3{
                finite_or(value.x, fallback.x),
                finite_or(value.y, fallback.y),
                finite_or(value.z, fallback.z)
            };
        }

        inline constexpr float kMaximum3DNavigationDistance = 1.0e12f;

        [[nodiscard]] constexpr float maximum_navigation_distance(CameraMode mode) noexcept
        {
            return mode == CameraMode::Canvas2D ? 2048.0f : kMaximum3DNavigationDistance;
        }

        [[nodiscard]] constexpr float maximum_orthographic_size(CameraMode mode) noexcept
        {
            return mode == CameraMode::Canvas2D ? 4096.0f : kMaximum3DNavigationDistance;
        }

        [[nodiscard]] inline bool finite_navigation_vector(Vec3 value, CameraMode mode) noexcept
        {
            return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z)
                && (mode == CameraMode::Canvas2D
                    || (std::abs(value.x) <= kMaximum3DNavigationDistance
                        && std::abs(value.y) <= kMaximum3DNavigationDistance
                        && std::abs(value.z) <= kMaximum3DNavigationDistance));
        }

        [[nodiscard]] bool valid_camera_snapshot(const CameraRigSnapshot& snapshot) noexcept;

        [[nodiscard]] float navigation_far_plane(const CameraRigState& rig) noexcept;

        [[nodiscard]] float navigation_near_plane(const CameraRigState& rig) noexcept;

        void commit_camera_navigation(CameraRigState& destination, CameraRigState candidate) noexcept;

        [[nodiscard]] Vec3 navigation_reference_up(const CameraRigState& rig) noexcept;

        [[nodiscard]] Camera local_camera_from_rig(const CameraRigState& rig) noexcept;
        [[nodiscard]] Camera camera_from_rig(const CameraRigState& rig) noexcept;
        [[nodiscard]] render_camera::ViewDescriptor descriptor_from_rig(
            const CameraRigState& rig) noexcept;

        [[nodiscard]] bool same_camera_frame(const Camera& lhs, const Camera& rhs) noexcept;

        [[nodiscard]] std::optional<Camera> validated_camera_frame(Camera camera) noexcept;

        [[nodiscard]] inline CameraRigState& ensure_rig(const void* ctxKey)
        {
            const void* const rigKey = normalize_camera_key(ctxKey);
            auto [it, inserted] = g_cameraRigs.try_emplace(rigKey, make_default_rig(CameraMode::Editor));
            if (inserted || it->second.logicalViewId == 0u)
            {
                if (g_nextLogicalViewId == (std::numeric_limits<std::uint64_t>::max)())
                    g_nextLogicalViewId = 1u;
                it->second.logicalViewId = g_nextLogicalViewId++;
                it->second.logicalViewGeneration = 1u;
            }
            if (it->second.viewDistances[editor_view_index(editor_view_from_rig(it->second))] <= 0.0f)
                save_active_view(it->second);
            return it->second;
        }

        [[nodiscard]] CameraRigSnapshot snapshot_from_rig(const CameraRigState& rig) noexcept;

        [[nodiscard]] CameraRigState rig_from_snapshot(const CameraRigSnapshot& snapshot) noexcept;

        struct GridPlacement final
        {
            std::int64_t centerCellX{};
            std::int64_t centerCellZ{};
            int spacingExponent{};
            float spacing{ 1.0f };
            std::uint64_t signature{};
        };

        [[nodiscard]] inline std::uint64_t mix_grid_signature(
            std::uint64_t seed,
            std::uint64_t value) noexcept
        {
            constexpr std::uint64_t kPrime = 1099511628211ull;
            seed ^= value;
            seed *= kPrime;
            return seed;
        }

        [[nodiscard]] inline GridPlacement grid_placement(const Camera& camera) noexcept
        {
            const float dx = camera.eye.x - camera.target.x;
            const float dy = camera.eye.y - camera.target.y;
            const float dz = camera.eye.z - camera.target.z;
            const float cameraDistance = (std::max)(
                0.25f,
                std::sqrt(dx * dx + dy * dy + dz * dz));
            const float viewSpan = camera.projection == render_camera::ProjectionKind::orthographic
                ? (std::max)(camera.orthographicVerticalSize, cameraDistance * 0.5f)
                : cameraDistance;
            const float desiredSpacing = (std::clamp)(viewSpan / 8.0f, 0.25f, 64.0f);
            const int spacingExponent = (std::clamp)(
                static_cast<int>(std::ceil(std::log2(desiredSpacing))),
                -2,
                6);
            const float spacing = std::exp2(static_cast<float>(spacingExponent));
            const auto centerCellX = static_cast<std::int64_t>(
                std::llround(static_cast<double>(camera.target.x / spacing)));
            const auto centerCellZ = static_cast<std::int64_t>(
                std::llround(static_cast<double>(camera.target.z / spacing)));

            std::uint64_t signature = 1469598103934665603ull;
            signature = mix_grid_signature(signature, static_cast<std::uint64_t>(centerCellX));
            signature = mix_grid_signature(signature, static_cast<std::uint64_t>(centerCellZ));
            signature = mix_grid_signature(
                signature,
                static_cast<std::uint64_t>(spacingExponent + 2));
            return GridPlacement{
                .centerCellX = centerCellX,
                .centerCellZ = centerCellZ,
                .spacingExponent = spacingExponent,
                .spacing = spacing,
                .signature = signature
            };
        }

        [[nodiscard]] inline GridGeometry build_grid_geometry(
            const GridPlacement& placement)
        {
            GridGeometry out{};
            constexpr int kHalfLineCount = 8;
            constexpr std::size_t kGridLineCount =
                static_cast<std::size_t>((kHalfLineCount * 2 + 1) * 2 + 3);
            out.vertices.reserve(kGridLineCount * 2u);
            out.indices.reserve(kGridLineCount * 2u);
            out.signature = placement.signature;
            out.spacing = placement.spacing;
            out.center = {
                static_cast<float>(placement.centerCellX) * placement.spacing,
                0.0f,
                static_cast<float>(placement.centerCellZ) * placement.spacing
            };

            const float minimumX = static_cast<float>(
                placement.centerCellX - kHalfLineCount) * placement.spacing;
            const float maximumX = static_cast<float>(
                placement.centerCellX + kHalfLineCount) * placement.spacing;
            const float minimumZ = static_cast<float>(
                placement.centerCellZ - kHalfLineCount) * placement.spacing;
            const float maximumZ = static_cast<float>(
                placement.centerCellZ + kHalfLineCount) * placement.spacing;

            auto push_vertex = [&](float x, float y, float z, float r, float g, float b)
            {
                out.vertices.push_back(Vertex{
                    .position{ x, y, z },
                    .color{ r, g, b }
                });
                return static_cast<std::uint32_t>(out.vertices.size() - 1u);
            };

            auto push_line = [&](float x0, float y0, float z0, float x1, float y1, float z1,
                                 float r, float g, float b)
            {
                const std::uint32_t first = push_vertex(x0, y0, z0, r, g, b);
                const std::uint32_t second = push_vertex(x1, y1, z1, r, g, b);
                out.indices.push_back(first);
                out.indices.push_back(second);
            };

            const auto line_tone = [](std::int64_t worldCell) noexcept
            {
                if (worldCell == 0)
                    return 0.36f;
                if (worldCell % 32 == 0)
                    return 0.25f;
                if (worldCell % 8 == 0)
                    return 0.17f;
                return 0.09f;
            };

            for (int line = -kHalfLineCount; line <= kHalfLineCount; ++line)
            {
                const std::int64_t worldCellX = placement.centerCellX + line;
                const std::int64_t worldCellZ = placement.centerCellZ + line;
                const float x = static_cast<float>(worldCellX) * placement.spacing;
                const float z = static_cast<float>(worldCellZ) * placement.spacing;
                const float xTone = line_tone(worldCellX);
                const float zTone = line_tone(worldCellZ);

                push_line(
                    x, 0.0f, minimumZ,
                    x, 0.0f, maximumZ,
                    xTone, xTone, xTone + 0.03f);
                push_line(
                    minimumX, 0.0f, z,
                    maximumX, 0.0f, z,
                    zTone, zTone, zTone + 0.03f);
            }

            const bool containsOrigin =
                placement.centerCellX >= -kHalfLineCount
                && placement.centerCellX <= kHalfLineCount
                && placement.centerCellZ >= -kHalfLineCount
                && placement.centerCellZ <= kHalfLineCount;
            if (containsOrigin)
            {
                const float axisLength = (std::max)(3.5f, placement.spacing * 3.5f);
                push_line(0.0f, 0.02f, 0.0f, axisLength, 0.02f, 0.0f, 0.95f, 0.30f, 0.28f);
                push_line(0.0f, 0.02f, 0.0f, 0.0f, axisLength, 0.0f, 0.28f, 0.92f, 0.40f);
                push_line(0.0f, 0.02f, 0.0f, 0.0f, 0.02f, axisLength, 0.33f, 0.58f, 0.98f);
            }

            return out;
        }
    }

    export [[nodiscard]] CameraMode camera_mode_for(const void* ctxKey) noexcept;

    export [[nodiscard]] inline bool authoring_guides_visible(const void* ctxKey) noexcept
    {
        const void* const key = detail::normalize_camera_key(ctxKey);
        std::shared_lock lock(detail::g_gridGeometryMutex);
        const auto found = detail::g_authoringGuides.find(key);
        return found == detail::g_authoringGuides.end() || found->second.visible;
    }

    export inline void set_authoring_guides_visible(const void* ctxKey, bool visible)
    {
        const void* const key = detail::normalize_camera_key(ctxKey);
        if (!key)
            return;

        {
            std::unique_lock lock(detail::g_gridGeometryMutex);
            const auto found = detail::g_authoringGuides.find(key);
            if (found == detail::g_authoringGuides.end())
            {
                if (visible)
                    return;
                detail::g_authoringGuides.try_emplace(key, detail::AuthoringGuidesState{.visible = false});
            }
            else
            {
                if (found->second.visible == visible)
                    return;
                found->second.visible = visible;
            }
        }
        // Buffered preview providers cache guide lines alongside scene geometry.
        detail::touch_geometry(key);
    }

    export [[nodiscard]] EditorView editor_view_for(const void* ctxKey) noexcept;

    export bool set_editor_view(const void* ctxKey, EditorView view) noexcept;

    export [[nodiscard]] CameraRigSnapshot capture_camera_rig_snapshot(const void* ctxKey) noexcept;

    export bool restore_camera_rig_snapshot(const void* ctxKey, const CameraRigSnapshot& snapshot) noexcept;

    export [[nodiscard]] bool set_editor_camera_pose(const void* ctxKey,
        const std::array<double, 3>& eye, Vec3 forward, Vec3 up) noexcept;

    export void set_camera_mode(const void* ctxKey, CameraMode mode) noexcept;
    export void reset_camera(const void* ctxKey, bool clear_scene_extent = false) noexcept;

    namespace detail
    {
    bool focus_camera_impl(
        const void* ctxKey,
        const std::array<double,3>& requestedFocus,
        float framingRadius,bool precise) noexcept;
    }
    export bool focus_camera(const void* ctxKey,Vec3 focus,float framingRadius=1.0f) noexcept;
    export bool focus_camera_precise(const void* ctxKey,const std::array<double,3>& focus,
        float framingRadius=1.0f) noexcept;
    export struct InitialOrbitMeshFraming final
    {
        Vec3 target{};
        float geometryRadius{}, framingRadius{}, distance{};
    };

    // Initial framing is a viewport convenience, never a scene/camera edit.
    // The pure result uses the actual transformed validated mesh, not a proxy
    // object radius. A restored/custom view is an explicit consumer choice.
    export [[nodiscard]] std::optional<InitialOrbitMeshFraming> initial_orbit_mesh_framing(
        const ObjectMarker& marker,const CameraRigSnapshot& view,float aspect,bool unrestored=true) noexcept;

    export bool frame_initial_orbit_mesh(const void* ctxKey,const ObjectMarker& marker,float aspect) noexcept;

    export [[nodiscard]] float camera_distance_for(const void* ctxKey) noexcept;

    export [[nodiscard]] inline std::shared_ptr<const PublishedSceneFrame> scene_frame_for(const void* ctxKey)
    {
        const auto key=detail::normalize_camera_key(ctxKey);if(!key)return {};
        std::shared_lock lock(detail::g_objectMarkerMutex);
        const auto found=detail::g_objectMarkers.find(key);
        return found==detail::g_objectMarkers.end()?nullptr:found->second.scene_frame;
    }
    namespace detail
    {
        // Use the exact admitted render triangles, including rebased planetary
        // geometry. A wheel/dolly may approach their surface, never cross it.
        // Keyboard flight remains an explicit way to inspect inside a mesh.
        [[nodiscard]] float surface_limited_zoom(const Camera& camera, Vec3 direction, float distance,
            float requested, const SolidDrawFrame* solids, const MeshWireDrawFrame* wire) noexcept;
    }

    export void zoom_camera(const void* ctxKey, float amount) noexcept;

    export [[nodiscard]] std::uint64_t camera_revision_for(const void* ctxKey) noexcept;
    export [[nodiscard]] inline std::uint64_t preview_content_revision_for(const void* ctxKey) noexcept
    {
        const void* const rigKey = detail::normalize_camera_key(ctxKey);
        if (!rigKey)
            return 0;
        std::shared_lock lock(detail::g_geometryRevisionMutex);
        const auto it = detail::g_geometryRevisions.find(rigKey);
        return it != detail::g_geometryRevisions.end() ? it->second : 0u;
    }

    export [[nodiscard]] inline std::uint64_t preview_geometry_revision_for(const void* ctxKey) noexcept
    {
        const void* const rigKey = detail::normalize_camera_key(ctxKey);
        if (!rigKey)
            return 0;

        const std::uint64_t cameraRevision = camera_revision_for(rigKey);
        const std::uint64_t geometryRevision = preview_content_revision_for(rigKey);
        return (cameraRevision * 1099511628211ull) ^ geometryRevision;
    }

    export void pan_camera_drag(
        const void* ctxKey,
        float deltaRightPixels,
        float deltaForwardPixels) noexcept;

    export void dolly_camera_drag(const void* ctxKey, float deltaPixels) noexcept;

    export void adjust_fly_speed(const void* ctxKey, float wheelSteps) noexcept;

    export [[nodiscard]] float fly_speed_for(const void* ctxKey) noexcept;

    export inline void cleanup_context(const void* ctxKey) noexcept
    {
        const void* const rigKey = detail::normalize_camera_key(ctxKey);
        if (!rigKey)
            return;

        {
            std::unique_lock lock(detail::g_objectMarkerMutex);
            detail::g_objectMarkers.erase(rigKey);
            detail::g_lastMarkerHits.erase(rigKey);
        }
        {
            std::unique_lock lock(detail::g_cameraRigMutex);
            detail::g_cameraRigs.erase(rigKey);
        }
        {
            std::unique_lock lock(detail::g_lightingFrameMutex);
            detail::g_lightingFrames.erase(rigKey);
        }
        {
            std::unique_lock lock(detail::g_geometryRevisionMutex);
            detail::g_geometryRevisions.erase(rigKey);
        }
        {
            std::unique_lock lock(detail::g_gridGeometryMutex);
            detail::g_authoringGuides.erase(rigKey);
        }
    }

    // A caller-owned simulation may supply a transient, arbitrary camera basis.
    // It shares the existing per-context rig and revision; snapshots deliberately
    // retain only the ordinary navigation state, never this external frame.
    export [[nodiscard]] std::optional<Camera> validate_camera_frame(Camera camera) noexcept;

    export [[nodiscard]] bool set_external_camera_frame(const void* ctxKey, const Camera& camera) noexcept;

    export [[nodiscard]] bool clear_external_camera_frame(const void* ctxKey) noexcept;

    export [[nodiscard]] Camera camera_for(const void* ctxKey) noexcept;

    export [[nodiscard]] render_camera::ViewDescriptor camera_descriptor_for(
        const void* ctxKey) noexcept;

    export [[nodiscard]] Mat4 projection_for(
        const void* ctxKey,
        float aspect,
        const Camera& camera) noexcept;

    export void step_camera(
        const void* ctxKey,
        float deltaTime,
        float moveForward,
        float moveRight,
        float moveUp,
        float yawInput,
        float pitchInput,
        float speedMultiplier = 1.0f,
        float rollInput = 0.0f) noexcept;

    export void look_camera(
        const void* ctxKey,
        float yawDeltaDegrees,
        float pitchDeltaDegrees) noexcept;

    export [[nodiscard]] std::array<Vertex, 8> look_marker_vertices_for(const void* ctxKey) noexcept;

    export [[nodiscard]] std::size_t look_marker_vertex_count_for(const void* ctxKey) noexcept;

    export inline void set_lighting_frame(
        const void* ctxKey,
        epochengine::lighting::LightingFrame frame)
    {
        const void* const rigKey = detail::normalize_camera_key(ctxKey);
        if (!rigKey)
            return;

        std::unique_lock lock(detail::g_lightingFrameMutex);
        const auto it = detail::g_lightingFrames.find(rigKey);
        const epochengine::lighting::LightingFrame* previous =
            it != detail::g_lightingFrames.end() ? &it->second : nullptr;
        const bool changed = !epochengine::lighting::diff_lighting_frames(previous, frame).empty();
        detail::g_lightingFrames[rigKey] = std::move(frame);
        if (changed)
            detail::touch_geometry(rigKey);
    }

    export inline void clear_lighting_frame(const void* ctxKey) noexcept
    {
        const void* const rigKey = detail::normalize_camera_key(ctxKey);
        if (!rigKey)
            return;

        try
        {
            std::unique_lock lock(detail::g_lightingFrameMutex);
            const auto it = detail::g_lightingFrames.find(rigKey);
            if (it != detail::g_lightingFrames.end())
            {
                epochengine::lighting::LightingFrame empty{};
                auto damage = epochengine::lighting::diff_lighting_frames(
                    &it->second, empty);
                detail::g_lightingFrames.erase(it);
                if (!damage.empty())
                    detail::touch_geometry(rigKey);
            }
        }
        catch(...) {} // Clear is noexcept; failed comparison keeps prior lighting.
    }

    export [[nodiscard]] inline epochengine::lighting::LightingFrame lighting_frame_for(const void* ctxKey)
    {
        const void* const rigKey = detail::normalize_camera_key(ctxKey);
        if (!rigKey)
            return {};

        std::shared_lock lock(detail::g_lightingFrameMutex);
        if (const auto it = detail::g_lightingFrames.find(rigKey);
            it != detail::g_lightingFrames.end())
        {
            return it->second;
        }
        return {};
    }

    // Callers may admit a larger, explicitly budgeted project snapshot. These
    // ceilings bound decoded geometry; they do not reserve GPU or process RAM.
    export struct MeshFrameLimits final
    {
        std::uint64_t maximum_indices{196'608u};
        std::uint64_t maximum_bytes{16ull * 1024ull * 1024ull};
    };

    // Debug wire data has its own CPU working-set ceiling. Count temporary
    // edge keys and the worst-case emitted vertices before allocating either.
    export inline constexpr std::uint64_t maximum_wireframe_working_bytes = 128ull * 1024ull * 1024ull;
    namespace detail
    {
        inline constexpr std::uint64_t maximum_primitive_solid_vertices = 384u;

        [[nodiscard]] inline bool mesh_wire_requested(const ObjectMarker& marker) noexcept
        {
            // Selecting a solid mesh keeps its existing shaded selection tint.
            // Full triangle edges belong only to the explicit Wireframe view;
            // editor helper overlays otherwise expose every back-side triangle.
            return marker.wireframe && marker.primitive==ObjectPreviewPrimitive::IndexedMesh && marker.mesh;
        }
        [[nodiscard]] inline const MeshWireEdges* find_wire_edges(std::span<const MeshWireEdges> entries,
            const ObjectMarker& marker) noexcept
        {
            for (const auto& entry : entries)
                if (entry.mesh->key() == marker.mesh->key()) return &entry;
            return nullptr;
        }
        [[nodiscard]] inline bool same_wire_triangles(std::span<const ObjectMarker> current,
            std::span<const ObjectMarker> previous) noexcept
        {
            std::size_t j{};
            for(const auto& marker:current)
            {
                if(!object_marker_requests_wireframe(marker)) continue;
                while(j<previous.size()&&!object_marker_requests_wireframe(previous[j])) ++j;
                if(j==previous.size()) return false;
                const auto& old=previous[j++];
                if(marker.primitive!=old.primitive||marker.radius!=old.radius||bool(marker.mesh)!=bool(old.mesh)
                    ||(marker.mesh&&marker.mesh->key()!=old.mesh->key())||marker.preciseTransform!=old.preciseTransform
                    ||marker.selected!=old.selected||marker.editorOnly!=old.editorOnly
                    ||marker.color.x!=old.color.x||marker.color.y!=old.color.y||marker.color.z!=old.color.z
                    ||marker.position.x!=old.position.x||marker.position.y!=old.position.y||marker.position.z!=old.position.z
                    ||marker.scale.x!=old.scale.x||marker.scale.y!=old.scale.y||marker.scale.z!=old.scale.z
                    ||marker.rotationDegrees.x!=old.rotationDegrees.x||marker.rotationDegrees.y!=old.rotationDegrees.y
                    ||marker.rotationDegrees.z!=old.rotationDegrees.z) return false;
            }
            while(j<previous.size()&&!object_marker_requests_wireframe(previous[j])) ++j;
            return j==previous.size();
        }
        [[nodiscard]] inline bool wireframe_working_set_admitted(std::span<const ObjectMarker> markers,
            const PublishedObjectMarkers& previous, std::uint64_t retained_extra = 0u,
            bool legacy_lines = true) noexcept
        {
            if (!std::any_of(markers.begin(), markers.end(), object_marker_requests_wireframe)) return true;
            std::uint64_t bytes{};
            const auto admit = [&](std::uint64_t count, std::uint64_t unit) {
                if (count > (maximum_wireframe_working_bytes - bytes) / unit) return false;
                bytes += count * unit; return true;
            };
            if (!admit(retained_extra, 1u)
                || (previous.line_geometry && !admit(previous.line_geometry->capacity(), sizeof(Vertex)))) return false;
            if(previous.mesh_wire_geometry&&!admit(mesh_wire_frame_bytes(*previous.mesh_wire_geometry),1u)) return false;
            if(previous.solid_geometry&&!admit(solid_frame_bytes(*previous.solid_geometry,false),1u)) return false;
            for (const auto& entry : previous.wire_edges)
                if (!admit(entry.edges->capacity(), sizeof(std::uint64_t))) return false;
            if (!admit(sizeof(MeshWireDrawFrame)+3u*sizeof(std::vector<Vertex>),1u)
                || !admit(markers.size(), 192u * sizeof(Vertex)+sizeof(ObjectMarker))) return false;
            const auto source_bytes=[](const ObjectMarker& marker) {
                return marker.mesh ? sizeof(epochengine::asset::mesh::Artifact)
                    +marker.mesh->vertices().size()*sizeof(epochengine::asset::mesh::Vertex)
                    +marker.mesh->indices().size()*sizeof(std::uint32_t) : 0u;
            };
            for(std::size_t i=0;i<markers.size();++i)
            {
                bool duplicate{};for(std::size_t j=0;j<i;++j) duplicate=duplicate||markers[j].mesh==markers[i].mesh;
                if(!duplicate&&!admit(source_bytes(markers[i]),1u)) return false;
            }
            for(std::size_t i=0;i<previous.markers.size();++i)
            {
                bool duplicate{};for(const auto& marker:markers) duplicate=duplicate||marker.mesh==previous.markers[i].mesh;
                for(std::size_t j=0;j<i;++j) duplicate=duplicate||previous.markers[j].mesh==previous.markers[i].mesh;
                if(!duplicate&&!admit(source_bytes(previous.markers[i]),1u)) return false;
            }
            if(previous.solid_geometry)for(std::size_t i=0;i<previous.solid_geometry->markers.size();++i)
            {
                const auto& marker=previous.solid_geometry->markers[i].marker;bool duplicate{};
                for(const auto& other:markers)duplicate=duplicate||other.mesh==marker.mesh;
                for(const auto& other:previous.markers)duplicate=duplicate||other.mesh==marker.mesh;
                for(std::size_t j=0;j<i;++j)duplicate=duplicate||previous.solid_geometry->markers[j].marker.mesh==marker.mesh;
                if(!duplicate&&!admit(source_bytes(marker),1u))return false;
            }
            const bool reuseTriangles=previous.mesh_wire_geometry&&same_wire_triangles(markers,previous.markers);
            for (const auto& marker : markers) {
                if (!object_marker_requests_wireframe(marker)) continue;
                const bool indexed=mesh_wire_requested(marker);
                const auto count=indexed?marker.mesh->indices().size():maximum_primitive_solid_vertices;
                if(!reuseTriangles&&!admit(count,sizeof(Vertex))) return false;
                // Framed consumers draw the triangle stream directly, or visit
                // its front-facing edges. No consumer reads a second all-edge
                // stream from that packet; legacy marker getters still need it.
                if(!legacy_lines||!indexed) continue;
                if (const auto* cached = find_wire_edges(previous.wire_edges, marker))
                {
                    if (!admit(cached->edges->size(), 2u * sizeof(Vertex))) return false;
                }
                else if (!admit(marker.mesh->indices().size(), sizeof(std::uint64_t) + 2u * sizeof(Vertex))) return false;
            }
            return true;
        }
        [[nodiscard]] inline std::vector<MeshWireEdges> build_wire_edges(std::span<const ObjectMarker> markers,
            std::span<const MeshWireEdges> previous)
        {
            std::vector<MeshWireEdges> result;
            for (const auto& marker : markers)
            {
                if (!mesh_wire_requested(marker)||find_wire_edges(result, marker)) continue;
                if (const auto* cached = find_wire_edges(previous, marker)) {result.push_back(*cached); continue;}
                const auto indices = marker.mesh->indices();
                auto edges = std::make_shared<std::vector<std::uint64_t>>();
                edges->reserve(indices.size());
                const auto append = [&](std::uint32_t a, std::uint32_t b) {
                    if (b < a) std::swap(a,b);
                    edges->push_back((static_cast<std::uint64_t>(a) << 32u) | b);
                };
                for (std::size_t triangle=0; triangle<indices.size(); triangle+=3u)
                {append(indices[triangle],indices[triangle+1u]);append(indices[triangle+1u],indices[triangle+2u]);append(indices[triangle+2u],indices[triangle]);}
                std::sort(edges->begin(),edges->end());
                edges->erase(std::unique(edges->begin(),edges->end()),edges->end());
                result.push_back({marker.mesh,std::move(edges)});
            }
            return result;
        }
    }
    export [[nodiscard]] std::optional<std::array<double,2>> marker_depth_bounds(
        const ObjectMarker& marker,const Camera& camera) noexcept;
    // Fit to actual opaque bounds, not an arbitrary far/near ratio. A surface
    // crossing the eye or an unknown primitive keeps the conservative floor.
    export [[nodiscard]] Camera fit_visible_depth_range(Camera camera,
        std::span<const ObjectMarker> markers,double aspect=0.0) noexcept;
    export [[nodiscard]] inline bool wireframe_frame_admitted(
        std::span<const ObjectMarker> markers, std::uint64_t retained_line_bytes = 0u) noexcept
    {return detail::wireframe_working_set_admitted(markers,{},retained_line_bytes);}

    export [[nodiscard]] inline bool mesh_frame_admitted(
        std::span<const ObjectMarker> markers, MeshFrameLimits limits = {}) noexcept
    {
        if (limits.maximum_indices > 6'291'456u
            || limits.maximum_bytes > 64ull * 1024ull * 1024ull) return false;
        std::uint64_t indices{}, bytes{};
        for (const auto& marker : markers)
        {
            if(const auto& detail=marker.waterDetail)
            {
                if(!marker.sphericalWater||marker.baseColorTexture||!std::isfinite(detail->radius_meters)
                    ||detail->radius_meters<=0.0||!std::isfinite(detail->meters_per_unit)
                    ||detail->meters_per_unit<=0.0||!std::isfinite(detail->seconds)
                    ||std::abs(detail->seconds)>1e12
                    ||!std::ranges::all_of(detail->world_center,[](double value){return std::isfinite(value);}))
                    return false;
            }
            if(const auto& water=marker.sphericalWater)
            {
                if(!marker.mesh||marker.mesh->key()!=water->artifact_key||water->ranges.size()>4096u
                    ||water->ranges.capacity()>4096u||water->spray_count>water->spray_sites.size())return false;
                for(const auto& site:std::span{water->spray_sites}.first(water->spray_count))
                    if(!std::isfinite(site.strength)||site.strength<0.0f||site.strength>1.0f
                        ||!std::ranges::all_of(site.position,[](float value){return std::isfinite(value);}))return false;
                std::uint64_t end{};
                for(const auto& range:water->ranges)
                {
                    if(range.first_vertex<end||range.vertex_count<3u
                        ||!std::isfinite(range.specular_scale)||range.specular_scale<0.0f||range.specular_scale>1.0f)return false;
                    end=std::uint64_t(range.first_vertex)+range.vertex_count;
                    if(end>marker.mesh->vertices().size())return false;
                }
            }
            if(marker.preciseTransform)
            {
                if(!marker.mesh||!valid_precise_mesh_transform(*marker.preciseTransform))return false;
                for(const auto value:marker.preciseTransform->scale)if(value<=0)return false;
                const auto minimum=marker.mesh->minimum(),maximum=marker.mesh->maximum();
                for(unsigned corner=0;corner<8u;++corner)
                {
                    const auto point=marker.preciseTransform->apply({
                        (corner&1u)?maximum[0]:minimum[0],(corner&2u)?maximum[1]:minimum[1],
                        (corner&4u)?maximum[2]:minimum[2]});
                    for(const auto value:point)
                        if(!std::isfinite(value)||std::abs(value)>(std::numeric_limits<float>::max)())return false;
                }
            }
            if (!marker.mesh)
            {
                if (marker.primitive == ObjectPreviewPrimitive::IndexedMesh) return false;
                continue;
            }
            if (marker.primitive != ObjectPreviewPrimitive::IndexedMesh || marker.mesh->key().empty()) return false;
            for (float value : {marker.position.x, marker.position.y, marker.position.z,
                marker.rotationDegrees.x, marker.rotationDegrees.y, marker.rotationDegrees.z})
                if (!std::isfinite(value)) return false;
            for (float value : {marker.scale.x, marker.scale.y, marker.scale.z})
                if (!std::isfinite(value) || value <= 0.0f) return false;
            const std::uint64_t markerIndices = marker.mesh->indices().size();
            const std::uint64_t markerBytes = marker.mesh->vertices().size() * sizeof(epochengine::asset::mesh::Vertex)
                + marker.mesh->indices().size() * sizeof(std::uint32_t)
                + (marker.sphericalWater ? sizeof(*marker.sphericalWater)
                    + marker.sphericalWater->ranges.capacity()*sizeof(asset::mesh::SphericalWaterRange) : 0u);
            if (markerIndices > limits.maximum_indices - indices
                || markerBytes > limits.maximum_bytes - bytes) return false;
            indices += markerIndices;
            bytes += markerBytes;
        }
        return true;
    }
    // Reject an oversized mesh frame atomically; existing admitted scene stays visible.
    namespace detail
    {
        [[nodiscard]] inline std::optional<PublishedObjectMarkers> prepare_object_markers(
            std::span<const ObjectMarker> markers,const PublishedObjectMarkers& snapshot,
            const epochengine::lighting::LightingFrame& lights,const Camera& camera,
            bool legacy_lines = true)
        {
        detail::PublishedObjectMarkers replacement;
        if (!markers.empty())
        {
            if (!detail::wireframe_working_set_admitted(markers,snapshot,0u,legacy_lines)) return {};
            replacement.markers.assign(markers.begin(),markers.end());
            replacement.solid_geometry=snapshot.solid_geometry;
            if(std::any_of(markers.begin(),markers.end(),[](const auto& marker){return bool(marker.baseColorTexture);})
                && !detail::solid_cache_admitted(replacement,lights,camera.eye,0u,!legacy_lines)) return {};
            if(legacy_lines)replacement.wire_edges=detail::build_wire_edges(markers,snapshot.wire_edges);
            replacement.line_geometry=std::make_shared<const std::vector<Vertex>>(
                detail::build_marker_lines(markers,replacement.wire_edges,legacy_lines
                    ?detail::MarkerLinePass::combined:detail::MarkerLinePass::helpers));
            if(std::any_of(markers.begin(),markers.end(),object_marker_requests_wireframe))
            {
                auto wire=std::make_shared<MeshWireDrawFrame>();
                wire->helper_lines=legacy_lines?std::make_shared<const std::vector<Vertex>>(
                    detail::build_marker_lines(markers,replacement.wire_edges,detail::MarkerLinePass::helpers))
                    :replacement.line_geometry;
                if(snapshot.mesh_wire_geometry&&detail::same_wire_triangles(markers,snapshot.markers))
                    wire->depth_triangles=snapshot.mesh_wire_geometry->depth_triangles;
                else wire->depth_triangles=std::make_shared<const std::vector<Vertex>>(
                    detail::build_marker_solids(markers,{},Camera{},nullptr,true));
                replacement.mesh_wire_geometry=std::move(wire);
            }
        }
            return replacement;
        }
    }
    export bool set_object_markers(const void* ctxKey, std::span<const ObjectMarker> markers,
        MeshFrameLimits limits = {})
    {
        const void* const rigKey = detail::normalize_camera_key(ctxKey);
        if (!rigKey || !mesh_frame_admitted(markers, limits))
            return false;

        for (const auto& marker : markers)
            if (marker.baseColorTexture && (marker.primitive != ObjectPreviewPrimitive::IndexedMesh
                || !marker.mesh || !valid_mesh_base_color_binding(*marker.baseColorTexture))) return false;

        bool changed = false;
        detail::PublishedObjectMarkers snapshot;
        bool hadPrevious{};
        {
            std::shared_lock lock(detail::g_objectMarkerMutex);
            const auto it = detail::g_objectMarkers.find(rigKey);
            if(it != detail::g_objectMarkers.end()) {snapshot=it->second;hadPrevious=true;}
        }
        const std::vector<ObjectMarker>* previous = hadPrevious ? &snapshot.markers : nullptr;

        changed = !previous ? !markers.empty() : previous->size() != markers.size();
        if (previous && !changed)
            for (std::size_t index = 0; index < markers.size(); ++index)
                if (!detail::same_marker((*previous)[index], markers[index]))
                {
                    changed = true;
                    break;
                }

        if(snapshot.scene_frame)changed=true;
        if (!changed)
            return true;

        // Edge sorting and line construction run outside the global publication
        // mutex. Local edge lists survive camera/body transforms by content key.
        auto prepared=detail::prepare_object_markers(markers,snapshot,lighting_frame_for(rigKey),camera_for(rigKey));
        if(!prepared)return false;
        auto& replacement=*prepared;
        {
            std::unique_lock lock(detail::g_objectMarkerMutex);
            const auto current=detail::g_objectMarkers.find(rigKey);
            if(hadPrevious ? current==detail::g_objectMarkers.end()
                || current->second.line_geometry!=snapshot.line_geometry||current->second.scene_frame!=snapshot.scene_frame
                : current!=detail::g_objectMarkers.end()) return false;
            if(markers.empty()) detail::g_objectMarkers.erase(rigKey);
            else detail::g_objectMarkers.insert_or_assign(rigKey,std::move(replacement));
        }

        detail::touch_geometry(rigKey);
        return true;
    }

    export inline void clear_object_markers(const void* ctxKey) noexcept
    {
        const void* const rigKey = detail::normalize_camera_key(ctxKey);
        if (!rigKey)
            return;

        bool changed = false;
        {
            std::unique_lock lock(detail::g_objectMarkerMutex);
            changed = detail::g_objectMarkers.erase(rigKey) > 0u;
        }
        if (!changed)
            return;

        detail::touch_geometry(rigKey);
    }

    export [[nodiscard]] inline std::shared_ptr<const std::vector<Vertex>> object_marker_geometry_for(const void* ctxKey)
    {
        static const auto empty = std::make_shared<const std::vector<Vertex>>();
        const void* const rigKey = detail::normalize_camera_key(ctxKey);
        if (!rigKey) return empty;
        std::shared_lock lock(detail::g_objectMarkerMutex);
        const auto it = detail::g_objectMarkers.find(rigKey);
        return it == detail::g_objectMarkers.end() || !it->second.line_geometry ? empty : it->second.line_geometry;
    }

    export struct TranslationHandleFrame final
    {
        Camera camera{};
        std::array<ObjectMarker, 3> arrows{};
        std::size_t count{};
    };

    // Pick from the accepted draw packet, never a pending planetary publication.
    // Its camera and arrows already share a rebased coordinate space.
    export [[nodiscard]] inline TranslationHandleFrame translation_handles_for(const void* ctxKey)
    {
        TranslationHandleFrame result;
        const auto key = detail::normalize_camera_key(ctxKey);
        if (!key) return result;
        result.camera = camera_for(key);
        std::shared_lock lock(detail::g_objectMarkerMutex);
        const auto found = detail::g_objectMarkers.find(key);
        if (found == detail::g_objectMarkers.end()) return result;
        if (found->second.scene_frame) result.camera = found->second.scene_frame->camera;
        for (const auto& marker : found->second.markers)
            if (marker.primitive == ObjectPreviewPrimitive::TranslationArrow && marker.editorOnly)
            {
                if (result.count == result.arrows.size()) { result.count = 0; return result; }
                result.arrows[result.count++] = marker;
            }
        return result;
    }

    // Compatibility owner for consumers requiring a mutable copy. Renderers
    // retain object_marker_geometry_for instead, without per-frame copies.
    export [[nodiscard]] inline std::vector<Vertex> object_marker_vertices_for(const void* ctxKey)
    {
        return *object_marker_geometry_for(ctxKey);
    }
    export [[nodiscard]] inline std::shared_ptr<const MeshWireDrawFrame> object_mesh_wire_frame_for(const void* ctxKey)
    {
        static const auto empty=std::make_shared<const MeshWireDrawFrame>();
        const auto key=detail::normalize_camera_key(ctxKey);
        std::shared_lock lock(detail::g_objectMarkerMutex);
        const auto found=detail::g_objectMarkers.find(key);
        return found==detail::g_objectMarkers.end()||!found->second.mesh_wire_geometry?empty:found->second.mesh_wire_geometry;
    }

    namespace detail
    {
        [[nodiscard]] inline Vec3 preview_box_half_extents(const ObjectMarker& marker, float radius) noexcept
        {
            const auto half_axis = [radius](float axis) noexcept
            {
                return (std::max)(std::abs(axis * 0.5f), radius * 0.45f);
            };
            Vec3 half{half_axis(marker.scale.x), half_axis(marker.scale.y), half_axis(marker.scale.z)};
            if (marker.primitive == ObjectPreviewPrimitive::Level)
                half.y = (std::max)(0.05f, radius * 0.08f);
            if (marker.primitive == ObjectPreviewPrimitive::Canvas2D)
                half.z = (std::max)(0.025f, radius * 0.04f);
            return half;
        }
    }

    inline std::vector<Vertex> detail::build_marker_lines(std::span<const ObjectMarker> markers,
        std::span<const MeshWireEdges> wire_edges, detail::MarkerLinePass pass)
    {
        std::vector<Vertex> out{};
        std::size_t reserve = markers.size() * 96u;
        for (const auto& marker : markers)
            if (pass!=detail::MarkerLinePass::helpers && detail::mesh_wire_requested(marker))
                if (const auto* cached=detail::find_wire_edges(wire_edges,marker)) reserve += cached->edges->size() * 2u;
        out.reserve(reserve);
        const auto make_vertex = [](Vec3 position, Vec3 color) noexcept
        {
            return Vertex{ .position = position, .color = color };
        };
        const auto lit = [](Vec3 color, float factor) noexcept
        {
            return Vec3{
                (std::clamp)(color.x * factor + 0.06f, 0.0f, 1.0f),
                (std::clamp)(color.y * factor + 0.06f, 0.0f, 1.0f),
                (std::clamp)(color.z * factor + 0.06f, 0.0f, 1.0f)
            };
        };
        auto push_line = [&](Vec3 a, Vec3 b, Vec3 color)
        {
            out.push_back(make_vertex(a, color));
            out.push_back(make_vertex(b, color));
        };
        auto push_box_edges = [&](Vec3 center, Vec3 half, Vec3 color)
        {
            const Vec3 c000{ center.x - half.x, center.y - half.y, center.z - half.z };
            const Vec3 c001{ center.x - half.x, center.y - half.y, center.z + half.z };
            const Vec3 c010{ center.x - half.x, center.y + half.y, center.z - half.z };
            const Vec3 c011{ center.x - half.x, center.y + half.y, center.z + half.z };
            const Vec3 c100{ center.x + half.x, center.y - half.y, center.z - half.z };
            const Vec3 c101{ center.x + half.x, center.y - half.y, center.z + half.z };
            const Vec3 c110{ center.x + half.x, center.y + half.y, center.z - half.z };
            const Vec3 c111{ center.x + half.x, center.y + half.y, center.z + half.z };
            const Vec3 topColor = lit(color, 1.18f);
            const Vec3 sideColor = lit(color, 0.90f);
            const Vec3 bottomColor = lit(color, 0.62f);

            push_line(c000, c100, bottomColor);
            push_line(c100, c101, bottomColor);
            push_line(c101, c001, bottomColor);
            push_line(c001, c000, bottomColor);
            push_line(c010, c110, topColor);
            push_line(c110, c111, topColor);
            push_line(c111, c011, topColor);
            push_line(c011, c010, topColor);
            push_line(c000, c010, sideColor);
            push_line(c100, c110, sideColor);
            push_line(c101, c111, sideColor);
            push_line(c001, c011, sideColor);
        };
        auto push_oriented_box_edges = [&](Vec3 center, Vec3 half, Vec3 rotation, Vec3 color)
        {
            const auto point = [&](Vec3 local) noexcept
            {
                return add(center, rotate_euler_degrees(local, rotation));
            };
            const Vec3 c000 = point({ -half.x, -half.y, -half.z });
            const Vec3 c001 = point({ -half.x, -half.y, half.z });
            const Vec3 c010 = point({ -half.x, half.y, -half.z });
            const Vec3 c011 = point({ -half.x, half.y, half.z });
            const Vec3 c100 = point({ half.x, -half.y, -half.z });
            const Vec3 c101 = point({ half.x, -half.y, half.z });
            const Vec3 c110 = point({ half.x, half.y, -half.z });
            const Vec3 c111 = point({ half.x, half.y, half.z });
            const Vec3 topColor = lit(color, 1.18f);
            const Vec3 sideColor = lit(color, 0.90f);
            const Vec3 bottomColor = lit(color, 0.62f);

            push_line(c000, c100, bottomColor);
            push_line(c100, c101, bottomColor);
            push_line(c101, c001, bottomColor);
            push_line(c001, c000, bottomColor);
            push_line(c010, c110, topColor);
            push_line(c110, c111, topColor);
            push_line(c111, c011, topColor);
            push_line(c011, c010, topColor);
            push_line(c000, c010, sideColor);
            push_line(c100, c110, sideColor);
            push_line(c101, c111, sideColor);
            push_line(c001, c011, sideColor);
        };
        auto push_leaf_diamond_edges = [&](Vec3 center, Vec3 half, Vec3 color)
        {
            const Vec3 top{ center.x, center.y + half.y, center.z };
            const Vec3 bottom{ center.x, center.y - half.y * 0.45f, center.z };
            const Vec3 left{ center.x - half.x, center.y, center.z };
            const Vec3 right{ center.x + half.x, center.y, center.z };
            const Vec3 front{ center.x, center.y, center.z + half.z };
            const Vec3 back{ center.x, center.y, center.z - half.z };
            const Vec3 edgeColor = lit(color, 1.10f);

            push_line(top, left, edgeColor);
            push_line(top, right, edgeColor);
            push_line(top, front, edgeColor);
            push_line(top, back, edgeColor);
            push_line(bottom, left, color);
            push_line(bottom, right, color);
            push_line(bottom, front, color);
            push_line(bottom, back, color);
            push_line(left, front, lit(color, 0.90f));
            push_line(front, right, lit(color, 0.90f));
            push_line(right, back, lit(color, 0.90f));
            push_line(back, left, lit(color, 0.90f));
        };

        for (const auto& marker : markers)
        {
            const float radius = (std::clamp)(marker.radius, 0.16f, 1.75f);
            Vec3 color = marker.selected ? visual_rgb(epochengine::visuals::object_selected()) : marker.color;
            if (marker.editorOnly && !marker.selected)
                color = scale(color, epochengine::visuals::editor_wire_opacity_factor());

            const Vec3 center{ marker.position.x, marker.position.y, marker.position.z };
            if (pass!=detail::MarkerLinePass::helpers && detail::mesh_wire_requested(marker))
            {
                // Emit each actual indexed triangle edge once through the
                // existing line pipeline; do not substitute a bounding box.
                const auto* cached=detail::find_wire_edges(wire_edges,marker);
                if(!cached) continue;
                const auto vertices = marker.mesh->vertices();
                const auto rotation = detail::euler_rotation(marker.rotationDegrees);
                const auto position = [&](std::uint32_t index)
                {
                    const auto& source = vertices[index].position;
                    return detail::mesh_vertex_position(marker,source,rotation);
                };
                const Vec3 edgeColor=marker.selected
                    ?visual_rgb(epochengine::visuals::object_selected_outline()):color;
                for (const auto edge : *cached->edges)
                    push_line(position(static_cast<std::uint32_t>(edge >> 32u)),
                        position(static_cast<std::uint32_t>(edge)), edgeColor);
            }
            Vec3 half = detail::preview_box_half_extents(marker, radius);

            const ObjectPreviewGeometryRoute route =
                object_preview_geometry_route(marker.primitive);
            const auto helperStart=out.size();
            if (route.marker_wire)
            {
                switch (marker.primitive)
                {
                case ObjectPreviewPrimitive::TranslationArrow:
                {
                    const float length = marker.scale.x;
                    const auto rotation = detail::euler_rotation(marker.rotationDegrees);
                    const auto at = [&](Vec3 point)
                    {
                        return add(center, detail::rotate_euler(scale(point, length), rotation));
                    };
                    const auto tip = at({1.0f,0.0f,0.0f});
                    push_line(center, tip, color);
                    for (const auto wing : {Vec3{0.86f,0.06f,0.0f}, Vec3{0.86f,-0.06f,0.0f},
                        Vec3{0.86f,0.0f,0.06f}, Vec3{0.86f,0.0f,-0.06f}})
                        push_line(tip, at(wing), color);
                    break;
                }
                case ObjectPreviewPrimitive::Light:
                    push_line({ center.x - radius, center.y, center.z }, { center.x + radius, center.y, center.z }, lit(color, 1.2f));
                    push_line({ center.x, center.y - radius, center.z }, { center.x, center.y + radius, center.z }, lit(color, 1.2f));
                    push_line({ center.x, center.y, center.z - radius }, { center.x, center.y, center.z + radius }, lit(color, 1.2f));
                    push_line(
                        { center.x - radius * 0.55f, center.y - radius * 0.55f, center.z },
                        { center.x + radius * 0.55f, center.y + radius * 0.55f, center.z },
                        color);
                    push_line(
                        { center.x - radius * 0.55f, center.y + radius * 0.55f, center.z },
                        { center.x + radius * 0.55f, center.y - radius * 0.55f, center.z },
                        color);
                    break;
                case ObjectPreviewPrimitive::Spawn:
                    half.y = (std::max)(0.08f, radius * 0.12f);
                    push_box_edges(center, half, color);
                    push_line(center, { center.x, center.y + radius * 1.4f, center.z }, lit(color, 1.05f));
                    break;
                case ObjectPreviewPrimitive::Camera:
                {
                    half = { radius * 0.30f, radius * 0.20f, radius * 0.24f };
                    push_box_edges(center, half, color);
                    const Vec3 lens{ center.x, center.y, center.z - half.z };
                    const float farZ = center.z - radius * 1.45f;
                    const float farX = radius * 0.82f;
                    const float farY = radius * 0.52f;
                    const Vec3 farTopLeft{ center.x - farX, center.y + farY, farZ };
                    const Vec3 farTopRight{ center.x + farX, center.y + farY, farZ };
                    const Vec3 farBottomLeft{ center.x - farX, center.y - farY, farZ };
                    const Vec3 farBottomRight{ center.x + farX, center.y - farY, farZ };
                    const auto frustumColor = lit(color, 1.1f);
                    push_line(lens, farTopLeft, frustumColor);
                    push_line(lens, farTopRight, frustumColor);
                    push_line(lens, farBottomLeft, frustumColor);
                    push_line(lens, farBottomRight, frustumColor);
                    push_line(farTopLeft, farTopRight, frustumColor);
                    push_line(farTopRight, farBottomRight, frustumColor);
                    push_line(farBottomRight, farBottomLeft, frustumColor);
                    push_line(farBottomLeft, farTopLeft, frustumColor);
                    break;
                }
                case ObjectPreviewPrimitive::Level:
                case ObjectPreviewPrimitive::Canvas2D:
                    push_box_edges(center, half, color);
                    break;
            case ObjectPreviewPrimitive::ForestTrunk:
            {
                const Vec3 baseHalf{
                    (std::max)(0.035f, half.x * 0.48f),
                    (std::max)(0.10f, half.y * 0.52f),
                    (std::max)(0.035f, half.z * 0.48f)
                };
                const Vec3 upperHalf{
                    (std::max)(0.025f, baseHalf.x * 0.68f),
                    (std::max)(0.08f, half.y * 0.48f),
                    (std::max)(0.025f, baseHalf.z * 0.68f)
                };
                push_box_edges({ center.x, center.y - baseHalf.y * 0.38f, center.z }, baseHalf, color);
                push_box_edges({ center.x, center.y + upperHalf.y * 0.52f, center.z }, upperHalf, lit(color, 1.06f));
                push_line(
                    { center.x, center.y - half.y, center.z },
                    { center.x, center.y + half.y * 1.08f, center.z },
                    lit(color, 1.22f));
                break;
            }
            case ObjectPreviewPrimitive::ForestBranch:
            {
                const Vec3 jointHalf{
                    (std::max)(0.028f, radius * 0.22f),
                    (std::max)(0.028f, radius * 0.22f),
                    (std::max)(0.028f, radius * 0.22f)
                };
                push_box_edges(center, jointHalf, color);
                push_line(center, { center.x + radius * 0.82f, center.y + radius * 0.36f, center.z }, lit(color, 1.10f));
                push_line(center, { center.x - radius * 0.58f, center.y + radius * 0.28f, center.z + radius * 0.52f }, lit(color, 1.02f));
                push_line(center, { center.x, center.y + radius * 0.66f, center.z - radius * 0.50f }, lit(color, 0.96f));
                break;
            }
            case ObjectPreviewPrimitive::ForestLeafCluster:
            {
                const Vec3 leafHalf{
                    (std::max)(0.055f, half.x * 0.86f),
                    (std::max)(0.035f, half.y * 0.72f),
                    (std::max)(0.055f, half.z * 0.86f)
                };
                push_leaf_diamond_edges(center, leafHalf, color);
                push_leaf_diamond_edges({ center.x + leafHalf.x * 0.58f, center.y + leafHalf.y * 0.28f, center.z - leafHalf.z * 0.18f }, scale(leafHalf, 0.62f), lit(color, 1.04f));
                push_leaf_diamond_edges({ center.x - leafHalf.x * 0.52f, center.y + leafHalf.y * 0.18f, center.z + leafHalf.z * 0.22f }, scale(leafHalf, 0.58f), lit(color, 0.94f));
                break;
            }
            case ObjectPreviewPrimitive::Cube:
            default:
                push_box_edges(center, half, color);
                break;
            }
            }

            if(marker.selected&&route.selection_wire&&(marker.primitive==ObjectPreviewPrimitive::Light
                ||marker.primitive==ObjectPreviewPrimitive::Camera)&&out.size()>helperStart)
            {
                // Bound the visible helper vertices themselves. Camera frustum
                // extent is asymmetric; object scale/radius proxies are not it.
                Vec3 minimum=out[helperStart].position,maximum=minimum;
                for(std::size_t index=helperStart;index<out.size();++index)
                {
                    const auto point=out[index].position;
                    minimum={(std::min)(minimum.x,point.x),(std::min)(minimum.y,point.y),(std::min)(minimum.z,point.z)};
                    maximum={(std::max)(maximum.x,point.x),(std::max)(maximum.y,point.y),(std::max)(maximum.z,point.z)};
                }
                push_box_edges(scale(add(minimum,maximum),0.5f),scale(subtract(maximum,minimum),0.5f),
                    visual_rgb(epochengine::visuals::object_selected_outline()));
            }
            else if (marker.selected && route.selection_wire&&!marker.mesh)
                push_oriented_box_edges(center, half, marker.rotationDegrees, visual_rgb(epochengine::visuals::object_selected_outline()));
        }

        return out;
    }

    namespace detail
    {
        inline constexpr std::size_t kMaxForestShadowVertices = 12000u;
        struct ForestShadow;
        [[nodiscard]] const ForestShadow* prepare_forest_shadow(ObjectMarker& localMarker,
            const EulerRotation& rotation, const epochengine::lighting::LightingFrame& frame,
            const epochengine::lighting::LightDesc*& sun);
        [[nodiscard]] float forest_shadow_visibility(const ForestShadow& shadow, Vec3 local) noexcept;
    }

    inline std::vector<Vertex> detail::build_marker_solids(std::span<const ObjectMarker> markers,
        const epochengine::lighting::LightingFrame& lightingFrame,const Camera& camera,
        std::vector<TexturedVertex>* textured, bool wire_depth,std::vector<WaterDetailRange>* water_detail)
    {
        std::vector<Vertex> out{};
        std::size_t reserve{};
        for (const auto& marker : markers)
        {
            if(!wire_depth&&!marker.baseColorTexture)reserve+=96u;
            if(wire_depth&&!marker.mesh&&object_marker_requests_wireframe(marker))
                reserve+=detail::maximum_primitive_solid_vertices;
            if (marker.mesh && marker.primitive == ObjectPreviewPrimitive::IndexedMesh
                && (wire_depth ? detail::mesh_wire_requested(marker)
                    : object_marker_uses_solid_fill(marker)&&!marker.baseColorTexture))
                reserve += marker.mesh->indices().size();
        }
        out.reserve(reserve);
        const auto make_vertex = [](Vec3 position, Vec3 color) noexcept
        {
            return Vertex{ .position = position, .color = color };
        };
        const auto lit = [](Vec3 color, float factor) noexcept
        {
            return Vec3{
                (std::clamp)(color.x * factor + 0.08f, 0.0f, 1.0f),
                (std::clamp)(color.y * factor + 0.08f, 0.0f, 1.0f),
                (std::clamp)(color.z * factor + 0.08f, 0.0f, 1.0f)
            };
        };
        const auto shade_surface = [&](Vec3 position, Vec3 normal, Vec3 albedo,
            bool forest = false, bool foliage = false, const epochengine::lighting::LightDesc* sun = nullptr,
            float visibility = 1.0f, float specularScale = 1.0f, bool water = false) noexcept
        {
            if (lightingFrame.revision == 0u && lightingFrame.lights.empty())
            {
                const float facing = 0.70f + (foliage ? std::abs(normal.y) : (std::max)(0.0f, normal.y)) * 0.46f;
                return lit(albedo, facing);
            }

            const auto result = epochengine::lighting::evaluate_reference_raster_lighting(
                lightingFrame,
                epochengine::lighting::SurfaceSample{
                    .position{ position.x, position.y, position.z },
                    .normal{ normal.x, normal.y, normal.z },
                    .viewDirection{
                        camera.eye.x - position.x,
                        camera.eye.y - position.y,
                        camera.eye.z - position.z
                    },
                    .albedo{ albedo.x, albedo.y, albedo.z },
                    .specularColor = forest || water ? epochengine::lighting::Color3{}
                        : epochengine::lighting::Color3{0.18f * specularScale, 0.18f * specularScale, 0.18f * specularScale},
                    .shininess = 24.0f,
                    .twoSidedDiffuse = foliage,
                    .shadowedLight = sun,
                    .directVisibility = visibility,
                    .waterGlintScale = water ? std::optional{specularScale} : std::nullopt
                });
            return Vec3{
                (std::clamp)(result.combined.r, 0.0f, 1.0f),
                (std::clamp)(result.combined.g, 0.0f, 1.0f),
                (std::clamp)(result.combined.b, 0.0f, 1.0f)
            };
        };
        auto push_tri = [&](Vec3 a, Vec3 b, Vec3 c, Vec3 color)
        {
            out.push_back(make_vertex(a, color));
            out.push_back(make_vertex(b, color));
            out.push_back(make_vertex(c, color));
        };
        auto push_face = [&](Vec3 a, Vec3 b, Vec3 c, Vec3 d, Vec3 normal, Vec3 color)
        {
            const Vec3 center{
                (a.x + b.x + c.x + d.x) * 0.25f,
                (a.y + b.y + c.y + d.y) * 0.25f,
                (a.z + b.z + c.z + d.z) * 0.25f
            };
            const Vec3 shaded = shade_surface(center, normal, color);
            push_tri(a, b, c, shaded);
            push_tri(a, c, d, shaded);
        };
        auto push_box = [&](Vec3 center, Vec3 half, Vec3 color)
        {
            const Vec3 c000{ center.x - half.x, center.y - half.y, center.z - half.z };
            const Vec3 c001{ center.x - half.x, center.y - half.y, center.z + half.z };
            const Vec3 c010{ center.x - half.x, center.y + half.y, center.z - half.z };
            const Vec3 c011{ center.x - half.x, center.y + half.y, center.z + half.z };
            const Vec3 c100{ center.x + half.x, center.y - half.y, center.z - half.z };
            const Vec3 c101{ center.x + half.x, center.y - half.y, center.z + half.z };
            const Vec3 c110{ center.x + half.x, center.y + half.y, center.z - half.z };
            const Vec3 c111{ center.x + half.x, center.y + half.y, center.z + half.z };

            push_face(c010, c110, c111, c011, { 0.0f, 1.0f, 0.0f }, color);
            push_face(c000, c001, c101, c100, { 0.0f, -1.0f, 0.0f }, color);
            push_face(c001, c011, c111, c101, { 0.0f, 0.0f, 1.0f }, color);
            push_face(c100, c110, c010, c000, { 0.0f, 0.0f, -1.0f }, color);
            push_face(c000, c010, c011, c001, { -1.0f, 0.0f, 0.0f }, color);
            push_face(c101, c111, c110, c100, { 1.0f, 0.0f, 0.0f }, color);
        };
        auto push_oriented_box = [&](Vec3 center, Vec3 half, Vec3 rotation, Vec3 color)
        {
            const auto point = [&](Vec3 local) noexcept
            {
                return add(center, rotate_euler_degrees(local, rotation));
            };
            const Vec3 c000 = point({ -half.x, -half.y, -half.z });
            const Vec3 c001 = point({ -half.x, -half.y, half.z });
            const Vec3 c010 = point({ -half.x, half.y, -half.z });
            const Vec3 c011 = point({ -half.x, half.y, half.z });
            const Vec3 c100 = point({ half.x, -half.y, -half.z });
            const Vec3 c101 = point({ half.x, -half.y, half.z });
            const Vec3 c110 = point({ half.x, half.y, -half.z });
            const Vec3 c111 = point({ half.x, half.y, half.z });

            push_face(c010, c110, c111, c011, rotate_euler_degrees({ 0.0f, 1.0f, 0.0f }, rotation), color);
            push_face(c000, c001, c101, c100, rotate_euler_degrees({ 0.0f, -1.0f, 0.0f }, rotation), color);
            push_face(c001, c011, c111, c101, rotate_euler_degrees({ 0.0f, 0.0f, 1.0f }, rotation), color);
            push_face(c100, c110, c010, c000, rotate_euler_degrees({ 0.0f, 0.0f, -1.0f }, rotation), color);
            push_face(c000, c010, c011, c001, rotate_euler_degrees({ -1.0f, 0.0f, 0.0f }, rotation), color);
            push_face(c101, c111, c110, c100, rotate_euler_degrees({ 1.0f, 0.0f, 0.0f }, rotation), color);
        };
        auto push_tapered_segment = [&](
            Vec3 center,
            float length,
            float radiusStart,
            float radiusEnd,
            Vec3 rotation,
            Vec3 color,
            bool capStart,
            bool capEnd)
        {
            constexpr std::uint32_t radialSides = 8u;
            constexpr float tau = 6.28318530717958647692f;
            const float halfLength = length * 0.5f;
            const float taperSlope = (radiusStart - radiusEnd)
                / (std::max)(length, 0.0001f);
            const auto point = [&](float y, float radius, float angle) noexcept
            {
                return add(center, rotate_euler_degrees({
                    std::cos(angle) * radius,
                    y,
                    std::sin(angle) * radius}, rotation));
            };
            const auto side_normal = [&](float angle) noexcept
            {
                return normalize(rotate_euler_degrees({
                    std::cos(angle), taperSlope, std::sin(angle)}, rotation));
            };
            for (std::uint32_t side = 0u; side < radialSides; ++side)
            {
                const float angle0 = tau * static_cast<float>(side)
                    / static_cast<float>(radialSides);
                const float angle1 = tau * static_cast<float>(side + 1u)
                    / static_cast<float>(radialSides);
                const Vec3 s0 = point(-halfLength, radiusStart, angle0);
                const Vec3 s1 = point(-halfLength, radiusStart, angle1);
                const Vec3 e0 = point(halfLength, radiusEnd, angle0);
                const Vec3 e1 = point(halfLength, radiusEnd, angle1);
                const Vec3 normal = normalize(add(
                    side_normal(angle0), side_normal(angle1)));
                const Vec3 surfaceCenter = scale(
                    add(add(s0, s1), add(e0, e1)), 0.25f);
                const Vec3 shaded = shade_surface(surfaceCenter, normal, color);
                push_tri(s0, e1, e0, shaded);
                push_tri(s0, s1, e1, shaded);
            }
            const Vec3 bottomNormal = rotate_euler_degrees(
                {0.0f, -1.0f, 0.0f}, rotation);
            const Vec3 topNormal = rotate_euler_degrees(
                {0.0f, 1.0f, 0.0f}, rotation);
            const Vec3 bottomCenter = add(center, rotate_euler_degrees(
                {0.0f, -halfLength, 0.0f}, rotation));
            const Vec3 topCenter = add(center, rotate_euler_degrees(
                {0.0f, halfLength, 0.0f}, rotation));
            const Vec3 bottomColor = shade_surface(
                bottomCenter, bottomNormal, color);
            const Vec3 topColor = shade_surface(topCenter, topNormal, color);
            if (capStart)
            {
                for (std::uint32_t side = 0u; side < radialSides; ++side)
                {
                    const float angle0 = tau * static_cast<float>(side)
                        / static_cast<float>(radialSides);
                    const float angle1 = tau * static_cast<float>(side + 1u)
                        / static_cast<float>(radialSides);
                    push_tri(
                        bottomCenter,
                        point(-halfLength, radiusStart, angle1),
                        point(-halfLength, radiusStart, angle0),
                        bottomColor);
                }
            }
            if (capEnd)
            {
                for (std::uint32_t side = 0u; side < radialSides; ++side)
                {
                    const float angle0 = tau * static_cast<float>(side)
                        / static_cast<float>(radialSides);
                    const float angle1 = tau * static_cast<float>(side + 1u)
                        / static_cast<float>(radialSides);
                    push_tri(
                        topCenter,
                        point(halfLength, radiusEnd, angle0),
                        point(halfLength, radiusEnd, angle1),
                        topColor);
                }
            }
        };
        auto push_leaf_cluster = [&](Vec3 center, Vec3 half, Vec3 rotation, Vec3 color)
        {
            const Vec3 mainHalf{
                (std::max)(0.04f, half.x * 0.72f),
                (std::max)(0.02f, half.y * 0.38f),
                (std::max)(0.04f, half.z * 0.72f)
            };
            const auto offset = [&](Vec3 local) noexcept
            {
                return add(center, rotate_euler_degrees(local, rotation));
            };
            push_oriented_box(center, mainHalf, rotation, color);
            push_oriented_box(offset({ mainHalf.x * 0.68f, mainHalf.y * 0.58f, -mainHalf.z * 0.18f }), scale(mainHalf, 0.58f), rotation, lit(color, 1.04f));
            push_oriented_box(offset({ -mainHalf.x * 0.62f, mainHalf.y * 0.38f, mainHalf.z * 0.22f }), scale(mainHalf, 0.54f), rotation, lit(color, 0.94f));
            push_oriented_box(offset({ 0.0f, mainHalf.y * 0.74f, mainHalf.z * 0.62f }), scale(mainHalf, 0.45f), rotation, lit(color, 1.10f));
        };

        for (const auto& marker : markers)
        {
            if (wire_depth ? !object_marker_requests_wireframe(marker) : !object_marker_uses_solid_fill(marker))
                continue;

            const float radius = (std::clamp)(marker.radius, 0.16f, 1.75f);
            const bool forestPrimitive = marker.forestLighting
                || marker.primitive == ObjectPreviewPrimitive::ForestTrunk
                || marker.primitive == ObjectPreviewPrimitive::ForestBranch
                || marker.primitive == ObjectPreviewPrimitive::ForestLeafCluster;
            Vec3 color = marker.selected && !forestPrimitive ? visual_rgb(epochengine::visuals::object_selected()) : marker.color;
            const bool neutralSurface = marker.neutralSurface && !marker.unlit && !marker.editorOnly;
            if (neutralSurface && (!marker.selected || forestPrimitive)) color = {0.65f,0.65f,0.65f};
            if (marker.editorOnly && !marker.selected)
                color = scale(color, epochengine::visuals::editor_solid_opacity_factor());

            const Vec3 center{ marker.position.x, marker.position.y, marker.position.z };
            const auto primitiveStart=out.size();
            Vec3 half = detail::preview_box_half_extents(marker, radius);

            if (marker.primitive == ObjectPreviewPrimitive::IndexedMesh)
            {
                const bool reverse=detail::mesh_source_reverse(marker);
                if(wire_depth)
                {
                    const auto rotation=detail::euler_rotation(marker.rotationDegrees);
                    const auto vertices=marker.mesh->vertices();
                    const auto indices=marker.mesh->indices();
                    const Vec3 wireColor=marker.selected?visual_rgb(epochengine::visuals::object_selected_outline())
                        :marker.editorOnly?scale(marker.color,epochengine::visuals::editor_wire_opacity_factor()):marker.color;
                    for(std::size_t triangle=0;triangle<indices.size();triangle+=3u)
                    {
                        for(std::size_t corner=0;corner<3u;++corner)
                        {
                            const auto index=indices[triangle+(reverse&&corner?3u-corner:corner)];
                            out.push_back({detail::mesh_vertex_position(marker,vertices[index].position,rotation),wireColor});
                        }
                    }
                    continue;
                }
                if (!marker.mesh || (marker.baseColorTexture && !textured)) continue;
                const auto vertices = marker.mesh->vertices();
                const auto rotation=detail::euler_rotation(marker.rotationDegrees);
                constexpr auto missing = (std::numeric_limits<std::uint32_t>::max)();
                // The high bit marks a vertex that has not been transformed.
                // Pending water entries also carry their fade. Final mapped
                // indices are bounded by the existing mesh frame byte cap.
                constexpr std::uint32_t pendingBit=1u<<31u;
                constexpr std::uint32_t waterDetailBit=1u<<30u;
                const bool collectWater=water_detail&&marker.waterDetail&&!marker.unlit&&!neutralSurface;
                std::vector<std::uint32_t> mapped(vertices.size(), missing);
                if(marker.sphericalWater)
                    for(const auto& range:marker.sphericalWater->ranges)
                        for(std::uint32_t index=range.first_vertex;index<range.first_vertex+range.vertex_count;++index)
                        {
                            const float specular=range.vertex_specular
                                ? std::clamp(vertices[index].uv[0],0.0f,range.specular_scale) : range.specular_scale;
                            mapped[index]=std::bit_cast<std::uint32_t>(specular)|pendingBit
                                |(collectWater&&range.specular_scale>0.0f?waterDetailBit:0u);
                        }
                std::vector<Vertex> transformed;
                transformed.reserve((std::min)(vertices.size(), marker.mesh->indices().size()));
                const auto indices=marker.mesh->indices();
                const epochengine::lighting::LightDesc* sun{};
                const detail::ForestShadow* shadow{};
                ObjectMarker localMarker;
                if (marker.forestLighting)
                {
                    localMarker = marker;
                    shadow = detail::prepare_forest_shadow(localMarker, rotation, lightingFrame, sun);
                }
                for(std::size_t triangle=0;triangle<indices.size();triangle+=3u)
                {
                    const auto first=static_cast<std::uint32_t>(out.size());
                    bool detailTriangle=collectWater&&water_detail;
                    for(std::size_t corner=0;corner<3u;++corner)
                    {
                    const auto index=indices[triangle+(reverse&&corner?3u-corner:corner)];
                    const auto entry=mapped[index];
                        const auto waterTag=entry==missing?0u:entry&waterDetailBit;
                        detailTriangle=detailTriangle&&waterTag!=0u;
                    if (!(entry&pendingBit))
                    {
                            const auto& vertex=transformed[entry&~waterDetailBit];
                            if (marker.baseColorTexture) textured->push_back({vertex.position,vertex.color,vertices[index].uv});
                            else out.push_back(vertex);
                        continue;
                    }
                        const float specularScale=entry==missing?1.0f:std::bit_cast<float>(entry&~(pendingBit|waterDetailBit));
                    const auto& source = vertices[index];
                    const auto position = detail::mesh_vertex_position(marker,source.position,rotation);
                    const auto normal = normalize(detail::mesh_vertex_normal(marker,source.normal,rotation));
                    Vec3 albedo = neutralSurface ? Vec3{0.65f,0.65f,0.65f}
                        : marker.paintView ? mesh_paint_albedo(*marker.paintView,source.position,source.color)
                        : marker.baseColorTexture && !marker.baseColorTexture->modulate_vertex_color
                            ? Vec3{1.0f,1.0f,1.0f} : Vec3{source.color[0], source.color[1], source.color[2]};
                    if (marker.selected && !marker.forestLighting && !marker.unlit)
                        albedo = add(scale(albedo, 0.85f), scale(color, 0.15f));
                        mapped[index] = static_cast<std::uint32_t>(transformed.size())|waterTag;
                    const auto visibility = shadow ? detail::forest_shadow_visibility(*shadow,
                        detail::mesh_vertex_position(localMarker, source.position, rotation)) : 1.0f;
                    const Vec3 surface = marker.unlit
                        ? Vec3{albedo.x * marker.color.x, albedo.y * marker.color.y, albedo.z * marker.color.z}
                        : shade_surface(position, normal, albedo, marker.forestLighting,
                            marker.forestLighting && source.uv[0] >= 0.5f, sun, visibility, specularScale,
                            entry != missing && !neutralSurface);
                    transformed.push_back(make_vertex(position, surface));
                    if (marker.baseColorTexture) textured->push_back({transformed.back().position,
                        transformed.back().color, source.uv});
                    else out.push_back(transformed.back());
                    }
                    if(detailTriangle)
                    {
                        if(!water_detail->empty()
                            &&water_detail->back().first_vertex+water_detail->back().vertex_count==first)
                            water_detail->back().vertex_count+=3u;
                        else if(water_detail->size()<maximum_water_detail_ranges)
                            water_detail->push_back({first,3u});
                        else
                        {
                            // An unusually fragmented cut keeps the baseline draw.
                            water_detail->clear();
                            water_detail=nullptr;
                        }
                    }
                }
                continue;
            }
            switch (marker.primitive)
            {
            case ObjectPreviewPrimitive::Light:
                half = { radius * 0.30f, radius * 0.30f, radius * 0.30f };
                push_box(center, half, lit(color, 1.25f));
                break;
            case ObjectPreviewPrimitive::Spawn:
                half.y = (std::max)(0.08f, radius * 0.12f);
                push_box(center, half, color);
                break;
            case ObjectPreviewPrimitive::Camera:
                half = { radius * 0.56f, radius * 0.34f, radius * 0.42f };
                push_box(center, half, color);
                break;
            case ObjectPreviewPrimitive::Level:
            case ObjectPreviewPrimitive::Canvas2D:
                push_oriented_box(center, half, marker.rotationDegrees, color);
                break;
            case ObjectPreviewPrimitive::EngineArcadeCabinet:
            {
                std::array<Vec3, render_arcade::kCabinetBodyVertexCount> vertices{};
                for (std::size_t index = 0; index < vertices.size(); ++index)
                {
                    const auto& local = render_arcade::kCabinetBodyVertices[index].position;
                    vertices[index] = {
                        center.x + local[0] * marker.scale.x,
                        center.y + local[1] * marker.scale.y,
                        center.z + local[2] * marker.scale.z
                    };
                }

                for (std::size_t index = 0; index + 2u < render_arcade::kCabinetBodyIndices.size(); index += 3u)
                {
                    const Vec3 a = vertices[render_arcade::kCabinetBodyIndices[index]];
                    const Vec3 b = vertices[render_arcade::kCabinetBodyIndices[index + 1u]];
                    const Vec3 c = vertices[render_arcade::kCabinetBodyIndices[index + 2u]];
                    if (!wire_depth && !clockwise_solid_triangle_faces_camera(a, b, c, camera.eye))
                        continue;

                    const Vec3 inward = cross(subtract(b, a), subtract(c, a));
                    const Vec3 outward = normalize(scale(inward, -1.0f));
                    const Vec3 surfaceCenter{
                        (a.x + b.x + c.x) / 3.0f,
                        (a.y + b.y + c.y) / 3.0f,
                        (a.z + b.z + c.z) / 3.0f
                    };
                    push_tri(a, b, c, shade_surface(surfaceCenter, outward, color));
                }
                break;
            }
            case ObjectPreviewPrimitive::ForestTrunk:
            {
                push_tapered_segment(
                    center,
                    (std::max)(0.10f, std::abs(marker.scale.y)),
                    (std::max)(0.02f, std::abs(marker.scale.x) * 0.5f),
                    (std::max)(0.012f, std::abs(marker.scale.z) * 0.5f),
                    marker.rotationDegrees,
                    color,
                    true,
                    false);
                break;
            }
            case ObjectPreviewPrimitive::ForestBranch:
            {
                push_tapered_segment(
                    center,
                    (std::max)(0.06f, std::abs(marker.scale.y)),
                    (std::max)(0.012f, std::abs(marker.scale.x) * 0.5f),
                    (std::max)(0.006f, std::abs(marker.scale.z) * 0.5f),
                    marker.rotationDegrees,
                    color,
                    false,
                    true);
                break;
            }
            case ObjectPreviewPrimitive::ForestLeafCluster:
                push_leaf_cluster(center, half, marker.rotationDegrees, color);
                break;
            case ObjectPreviewPrimitive::Cube:
            default:
                push_oriented_box(center, half, marker.rotationDegrees, color);
                break;
            }
            if(wire_depth)
            {
                const auto wireColor=marker.selected?visual_rgb(epochengine::visuals::object_selected_outline()):marker.color;
                for(auto index=primitiveStart;index<out.size();++index)out[index].color=wireColor;
            }
        }

        return out;
    }

    // Additional solid caching has a separate working-set ceiling. Legacy
    // source-frame admission is unchanged; larger frames remain uncached.
    export inline constexpr std::uint64_t maximum_solid_cache_working_bytes=128ull*1024ull*1024ull;
    namespace detail
    {
        [[nodiscard]] bool same_solid_lighting(const epochengine::lighting::LightingFrame& a,
            const epochengine::lighting::LightingFrame& b) noexcept;
        [[nodiscard]] bool same_eye(Vec3 a, Vec3 b) noexcept;
        [[nodiscard]] bool solid_uses_eye(const ObjectMarker& marker,
            const epochengine::lighting::LightingFrame& lights) noexcept;
        [[nodiscard]] inline bool same_solid_frame(const SolidGeometryFrame& cached,
            std::span<const ObjectMarker> markers,const epochengine::lighting::LightingFrame& lights,Vec3 eye) noexcept
        {
            if(!cached.geometry||cached.markers.size()!=markers.size()||!same_solid_lighting(cached.lighting,lights)) return false;
            for(std::size_t i=0;i<markers.size();++i)
                if(!same_marker(cached.markers[i].marker,markers[i])
                    ||(solid_uses_eye(markers[i],lights)&&!same_eye(cached.eye,eye))) return false;
            return true;
        }
        [[nodiscard]] inline std::uint64_t solid_frame_bytes(const SolidGeometryFrame& frame, bool include_mesh_sources) noexcept
        {
            std::uint64_t bytes=sizeof(frame)+frame.markers.capacity()*sizeof(SolidMarkerGeometry)
                +frame.lighting.lights.capacity()*sizeof(epochengine::lighting::FrameLight)
                +frame.water_detail_ranges.capacity()*sizeof(WaterDetailRange);
            if(frame.geometry) bytes+=frame.geometry->capacity()*sizeof(Vertex);
            for(std::size_t i=0;i<frame.markers.size();++i)
            {
                const auto& geometry=frame.markers[i].geometry;
                if(!geometry||geometry==frame.geometry) continue;
                bool duplicate{};
                for(std::size_t j=0;j<i;++j) duplicate=duplicate||frame.markers[j].geometry==geometry;
                if(!duplicate) bytes+=geometry->capacity()*sizeof(Vertex);
            }
            if(include_mesh_sources)for(std::size_t i=0;i<frame.markers.size();++i)
            {
                const auto& mesh=frame.markers[i].marker.mesh;if(!mesh) continue;
                bool duplicate{};
                for(std::size_t j=0;j<i;++j)
                    duplicate=duplicate||frame.markers[j].marker.mesh==mesh;
                if(!duplicate) bytes+=sizeof(epochengine::asset::mesh::Artifact)
                    +mesh->vertices().size()*sizeof(epochengine::asset::mesh::Vertex)+mesh->indices().size()*sizeof(std::uint32_t);
            }
            for(std::size_t i=0;i<frame.markers.size();++i)
            {
                const auto& entry=frame.markers[i];
                if(entry.textured) bytes+=entry.textured->capacity()*sizeof(TexturedVertex);
                if(const auto& water=entry.marker.sphericalWater)
                    bytes+=sizeof(*water)+water->ranges.capacity()*sizeof(asset::mesh::SphericalWaterRange);
                if(!entry.marker.baseColorTexture) continue;
                const auto* artifact=entry.marker.baseColorTexture->artifact.artifact();
                bool duplicate{};
                for(std::size_t j=0;j<i;++j)
                    duplicate=duplicate||(frame.markers[j].marker.baseColorTexture
                        && frame.markers[j].marker.baseColorTexture->artifact.artifact()==artifact);
                if(!duplicate&&artifact)
                {
                    bytes+=sizeof(MeshBaseColorBinding)+sizeof(*artifact)
                        +artifact->mips.capacity()*sizeof(epochengine::asset::texture::CompiledTextureMip);
                    for(const auto& mip:artifact->mips) bytes+=mip.texels.capacity();
                }
            }
            return bytes;
        }
        [[nodiscard]] inline std::uint64_t solid_marker_vertex_bound(const ObjectMarker& marker) noexcept
        {
            return !object_marker_uses_solid_fill(marker)?96u
                :marker.mesh&&marker.primitive==ObjectPreviewPrimitive::IndexedMesh
                    ?marker.mesh->indices().size()+96u:maximum_primitive_solid_vertices;
        }
        [[nodiscard]] inline bool uses_water_detail_batch(std::span<const ObjectMarker> markers) noexcept
        {
            return std::ranges::any_of(markers,[](const ObjectMarker& marker){return marker.waterDetail.has_value();});
        }
        [[nodiscard]] inline bool aggregate_solid_batch(std::span<const ObjectMarker> markers,
            const epochengine::lighting::LightingFrame& lights) noexcept
        {
            if(markers.size()<2u)return false;
            // Select the representation before publishing its first generation.
            // Both the old and replacement per-marker buffers and joined stream
            // coexist during movement. Small scenes retain per-marker reuse;
            // large batches must avoid retaining those duplicate triangle copies.
            std::uint64_t bytes{},scratch{};
            const auto admit=[&](std::uint64_t count,std::uint64_t unit) {
                if(count>(maximum_solid_cache_working_bytes-bytes)/unit)return false;
                bytes+=count*unit;return true;
            };
            if(!admit(2u,sizeof(SolidGeometryFrame))
                ||!admit(markers.size(),2u*(sizeof(SolidMarkerGeometry)+sizeof(ObjectMarker)))
                ||!admit(lights.lights.size(),2u*sizeof(epochengine::lighting::FrameLight)))return true;
            for(std::size_t i=0;i<markers.size();++i)
            {
                const auto& marker=markers[i];
                if(!admit(solid_marker_vertex_bound(marker),4u*sizeof(Vertex)))return true;
                if(const auto& water=marker.sphericalWater)
                    if(!admit(2u,sizeof(*water)+water->ranges.capacity()*sizeof(asset::mesh::SphericalWaterRange)))return true;
                if(!marker.mesh)continue;
                bool duplicate{};for(std::size_t j=0;j<i;++j)duplicate=duplicate||markers[j].mesh==marker.mesh;
                if(!duplicate&&(!admit(2u,sizeof(epochengine::asset::mesh::Artifact))
                    ||!admit(marker.mesh->vertices().size(),2u*sizeof(epochengine::asset::mesh::Vertex))
                    ||!admit(marker.mesh->indices().size(),2u*sizeof(std::uint32_t))))return true;
                if(object_marker_uses_solid_fill(marker))
                    scratch=(std::max)(scratch,marker.mesh->vertices().size()*sizeof(std::uint32_t)
                        +(std::min)(marker.mesh->vertices().size(),marker.mesh->indices().size())*sizeof(Vertex)
                        +(marker.forestLighting ? (std::min)(marker.mesh->vertices().size(), kMaxForestShadowVertices)*sizeof(Vec3) : 0u));
            }
            return !admit(scratch,1u);
        }
        [[nodiscard]] inline bool solid_cache_admitted(const PublishedObjectMarkers& frame,
            const epochengine::lighting::LightingFrame& lights,Vec3 eye,std::uint64_t extra_bytes,
            bool framed_batch) noexcept
        {
            if(extra_bytes>maximum_solid_cache_working_bytes)return false;
            std::uint64_t bytes{extra_bytes},vertices{},scratch{};
            const auto admit=[&](std::uint64_t count,std::uint64_t unit) {
                if(count>(maximum_solid_cache_working_bytes-bytes)/unit) return false;
                bytes+=count*unit;return true;
            };
            if(!admit(sizeof(SolidGeometryFrame),1u)||!admit(frame.markers.size(),sizeof(SolidMarkerGeometry)+sizeof(ObjectMarker))
                ||!admit(lights.lights.size(),sizeof(epochengine::lighting::FrameLight))) return false;
            if(frame.solid_geometry&&!admit(solid_frame_bytes(*frame.solid_geometry),1u)) return false;
            if(frame.line_geometry&&!admit(frame.line_geometry->capacity(),sizeof(Vertex))) return false;
            if(frame.mesh_wire_geometry&&!admit(mesh_wire_frame_bytes(*frame.mesh_wire_geometry),1u)) return false;
            for(const auto& edges:frame.wire_edges)
                if(!admit(edges.edges->capacity(),sizeof(std::uint64_t))) return false;
            // Untextured geometry is consumed as one triangle stream, including
            // mixed scenes whose textured markers retain their separate UV data.
            // Per-object untextured copies double every large planetary frame.
            const bool waterDetail=framed_batch&&uses_water_detail_batch(frame.markers);
            const bool aggregateOnly=framed_batch&&(waterDetail||aggregate_solid_batch(frame.markers,lights));
            const bool reuseAggregate=aggregateOnly&&frame.solid_geometry
                &&same_solid_frame(*frame.solid_geometry,frame.markers,lights,eye);
            if(waterDetail&&!reuseAggregate&&!admit(maximum_water_detail_ranges,sizeof(WaterDetailRange)))return false;
            for(const auto& marker:frame.markers)
            {
                const bool solid=object_marker_uses_solid_fill(marker);
                const std::uint64_t count=solid_marker_vertex_bound(marker);
                if(!marker.baseColorTexture)vertices+=count;
                if(const auto& water=marker.sphericalWater)
                    if(!admit(1u,sizeof(*water)+water->ranges.capacity()*sizeof(asset::mesh::SphericalWaterRange)))return false;
                bool reused{};
                if(frame.solid_geometry&&same_solid_lighting(frame.solid_geometry->lighting,lights)
                    &&(!solid_uses_eye(marker,lights)||same_eye(frame.solid_geometry->eye,eye)))
                    for(const auto& previous:frame.solid_geometry->markers)
                        if(previous.geometry&&same_marker(previous.marker,marker)) {reused=true;break;}
                if((!aggregateOnly||marker.baseColorTexture)&&!reused
                    &&!admit(count,marker.baseColorTexture?sizeof(TexturedVertex):sizeof(Vertex))) return false;
                if(marker.baseColorTexture)
                {
                    const auto* texture=marker.baseColorTexture->artifact.artifact();
                    bool retained{};
                    if(frame.solid_geometry) for(const auto& previous:frame.solid_geometry->markers)
                        retained=retained||(previous.marker.baseColorTexture
                            && previous.marker.baseColorTexture->artifact.artifact()==texture);
                    for(const auto& earlier:frame.markers)
                    {
                        if(&earlier==&marker) break;
                        retained=retained||(earlier.baseColorTexture&&earlier.baseColorTexture->artifact.artifact()==texture);
                    }
                    if(!retained)
                    {
                        if(!texture||!admit(sizeof(MeshBaseColorBinding)+sizeof(*texture),1u)
                            ||!admit(texture->mips.capacity(),sizeof(epochengine::asset::texture::CompiledTextureMip))) return false;
                        for(const auto& mip:texture->mips) if(!admit(mip.texels.capacity(),1u)) return false;
                    }
                }
                // Equal content can reuse visual geometry, while a separately
                // decoded Artifact still owns another source allocation.
                bool sourceRetained{};
                if(frame.solid_geometry)
                    for(const auto& previous:frame.solid_geometry->markers)
                        sourceRetained=sourceRetained||(marker.mesh&&previous.marker.mesh==marker.mesh);
                for(const auto& earlier:frame.markers)
                {
                    if(&earlier==&marker) break;
                    sourceRetained=sourceRetained||(marker.mesh&&earlier.mesh==marker.mesh);
                }
                if(!sourceRetained&&marker.mesh&&!admit(sizeof(epochengine::asset::mesh::Artifact)
                    +marker.mesh->vertices().size()*sizeof(epochengine::asset::mesh::Vertex)
                    +marker.mesh->indices().size()*sizeof(std::uint32_t),1u)) return false;
                if(!reuseAggregate&&!reused&&marker.mesh&&solid)
                    scratch=(std::max)(scratch,marker.mesh->vertices().size()*sizeof(std::uint32_t)
                        +(std::min)(marker.mesh->vertices().size(),marker.mesh->indices().size())*sizeof(Vertex)
                        +(marker.forestLighting ? (std::min)(marker.mesh->vertices().size(), kMaxForestShadowVertices)*sizeof(Vec3) : 0u));
            }
            return admit(frame.markers.size()>1u&&!reuseAggregate?vertices:0u,sizeof(Vertex))&&admit(scratch,1u);
        }
    }
    namespace detail
    {
        [[nodiscard]] inline std::shared_ptr<const SolidGeometryFrame> prepare_solid_geometry(
            const PublishedObjectMarkers& snapshot,const epochengine::lighting::LightingFrame& lights,const Camera& camera,
            bool framed_batch=false)
        {
        if(snapshot.solid_geometry&&same_solid_frame(*snapshot.solid_geometry,snapshot.markers,lights,camera.eye))
            return snapshot.solid_geometry;
        auto replacement=std::make_shared<detail::SolidGeometryFrame>();
        replacement->lighting=lights;replacement->eye=camera.eye;
        replacement->markers.reserve(snapshot.markers.size());
        const bool waterDetail=framed_batch&&uses_water_detail_batch(snapshot.markers);
        const bool aggregateOnly=framed_batch&&(waterDetail||aggregate_solid_batch(snapshot.markers,lights));
        const auto previous=snapshot.solid_geometry;
        const bool sameLighting=previous&&detail::same_solid_lighting(previous->lighting,lights);
        for(const auto& marker:snapshot.markers)
        {
            if(aggregateOnly&&!marker.baseColorTexture)
            {
                replacement->markers.push_back({marker,{},{}});
                continue;
            }
            std::shared_ptr<const std::vector<Vertex>> geometry;
            std::shared_ptr<const std::vector<TexturedVertex>> textured;
            if(sameLighting&&(!detail::solid_uses_eye(marker,lights)||detail::same_eye(previous->eye,camera.eye)))
                for(const auto& cached:previous->markers)
                    if(detail::same_marker(cached.marker,marker)) {geometry=cached.geometry;textured=cached.textured;break;}
            if(!geometry)
            {
                if(marker.baseColorTexture)
                {
                    auto vertices=std::make_shared<std::vector<TexturedVertex>>();
                    if(object_marker_uses_solid_fill(marker))vertices->reserve(marker.mesh->indices().size());
                    geometry=std::make_shared<const std::vector<Vertex>>(
                        detail::build_marker_solids(std::span{&marker,1u},lights,camera,vertices.get()));
                    textured=std::move(vertices);
                }
                else geometry=std::make_shared<const std::vector<Vertex>>(
                    detail::build_marker_solids(std::span{&marker,1u},lights,camera));
            }
            replacement->markers.push_back({marker,std::move(geometry),std::move(textured)});
        }
        if(aggregateOnly)
        {
            if(waterDetail)replacement->water_detail_ranges.reserve(maximum_water_detail_ranges);
            replacement->geometry=std::make_shared<const std::vector<Vertex>>(
                detail::build_marker_solids(snapshot.markers,lights,camera,nullptr,false,
                    waterDetail?&replacement->water_detail_ranges:nullptr));
        }
        else if(replacement->markers.size()==1u) replacement->geometry=replacement->markers.front().geometry;
        else
        {
            auto joined=std::make_shared<std::vector<Vertex>>();std::size_t count{};
            for(const auto& marker:replacement->markers) count+=marker.geometry->size();
            joined->reserve(count);
            for(const auto& marker:replacement->markers)
                joined->insert(joined->end(),marker.geometry->begin(),marker.geometry->end());
            replacement->geometry=std::move(joined);
        }
            return replacement;
        }
    }
    export [[nodiscard]] std::shared_ptr<const SolidDrawFrame> object_solid_frame_for(const void* ctxKey)
    {
        static const auto empty=std::make_shared<const SolidDrawFrame>(SolidDrawFrame{
            .geometry=std::make_shared<const std::vector<Vertex>>()});
        static const epochengine::lighting::LightingFrame noLights{};
        const void* const rigKey=detail::normalize_camera_key(ctxKey);
        if(!rigKey) return empty;
        // Empty MSVC Debug vectors allocate iterator proxies. Materialize these
        // owning snapshots only on a miss, never on the allocation-free hit path.
        std::optional<detail::PublishedObjectMarkers> pendingSnapshot;
        std::optional<epochengine::lighting::LightingFrame> pendingLights;
        Camera camera;
        {
            // Existing publishers take one of these locks at a time. Keep this
            // read-only snapshot short; no geometry construction runs under it.
            std::shared_lock lightingLock(detail::g_lightingFrameMutex);
            std::shared_lock cameraLock(detail::g_cameraRigMutex);
            std::shared_lock markerLock(detail::g_objectMarkerMutex);
            const auto found=detail::g_objectMarkers.find(rigKey);
            if(found==detail::g_objectMarkers.end()) return empty;
            const auto light=detail::g_lightingFrames.find(rigKey);
            const auto& currentLights=light==detail::g_lightingFrames.end()?noLights:light->second;
            const auto rig=detail::g_cameraRigs.find(rigKey);
            camera=detail::camera_from_rig(rig==detail::g_cameraRigs.end()
                ?detail::make_default_rig(CameraMode::Editor):rig->second);
            const auto& current=found->second;
            if(current.scene_frame)return current.scene_frame->solids;
            if(current.solid_geometry&&detail::same_solid_frame(*current.solid_geometry,current.markers,currentLights,camera.eye))
                return current.solid_geometry;
            pendingSnapshot.emplace(current);pendingLights.emplace(currentLights);
        }
        auto& snapshot=*pendingSnapshot;
        const auto& lights=*pendingLights;
        if(!detail::solid_cache_admitted(snapshot,lights,camera.eye))
        {
            for(const auto& marker:snapshot.markers) if(marker.baseColorTexture)
                return snapshot.solid_geometry?snapshot.solid_geometry:empty;
            // Do not retain an extra cache on top of an oversized legacy frame.
            {
                std::unique_lock lock(detail::g_objectMarkerMutex);
                const auto current=detail::g_objectMarkers.find(rigKey);
                if(current!=detail::g_objectMarkers.end()&&current->second.line_geometry==snapshot.line_geometry
                    &&current->second.solid_geometry==snapshot.solid_geometry) current->second.solid_geometry.reset();
            }
            snapshot.solid_geometry.reset();
            auto uncached=std::make_shared<detail::SolidGeometryFrame>();
            uncached->geometry=std::make_shared<const std::vector<Vertex>>(detail::build_marker_solids(snapshot.markers,lights,camera));
            return uncached;
        }
        auto replacement=detail::prepare_solid_geometry(snapshot,lights,camera);
        {
            std::unique_lock lock(detail::g_objectMarkerMutex);
            const auto current=detail::g_objectMarkers.find(rigKey);
            if(current!=detail::g_objectMarkers.end()&&current->second.line_geometry==snapshot.line_geometry
                &&current->second.solid_geometry==snapshot.solid_geometry) current->second.solid_geometry=replacement;
        }
        return replacement;
    }
    // Capture mutable context state on its owner. CPU preparation below only
    // reads this immutable copy and supplied values; no backend/context calls.
    namespace detail
    {
        [[nodiscard]] inline std::optional<Camera> admitted_scene_frame_camera(
            std::span<const ObjectMarker> markers,Camera camera,
            const epochengine::lighting::LightingFrame& lights,MeshFrameLimits limits,
            const std::array<double,3>& render_origin) noexcept
        {
            const auto validated=validate_camera_frame(camera);
            if(!validated||!mesh_frame_admitted(markers,limits))return {};
            for(const auto coordinate:render_origin)if(!std::isfinite(coordinate))return {};
            for(const auto& light:lights.lights)if(!epochengine::lighting::validate(light.desc).valid)return {};
            for(const auto& marker:markers)if(marker.baseColorTexture&&(!marker.mesh
                ||marker.primitive!=ObjectPreviewPrimitive::IndexedMesh||!valid_mesh_base_color_binding(*marker.baseColorTexture)))return {};
            return validated;
        }
    }
    export struct ShorelineSprayBatch final
    {
        static constexpr std::size_t desktop_portable_budget=24u;
        static constexpr std::size_t maximum_particles=96u;
        std::array<Vertex,maximum_particles> points{};
        std::size_t size{};
    };

    // Positions live in scene space. The sprite attributes are pixel size,
    // opacity and the daylight tint, independent from the opaque water draw.
    export [[nodiscard]] inline ShorelineSprayBatch shoreline_spray_for(
        const PublishedSceneFrame& scene,int viewport_height,std::size_t particle_budget) noexcept
    {
        ShorelineSprayBatch batch;
        const auto limit=(std::min)(particle_budget,batch.points.size());
        if(limit==0u||viewport_height<1||scene.camera.projection!=render_camera::ProjectionKind::perspective)return batch;
        const auto water=water_detail_for(scene);
        if(!water)return batch;
        const auto camera=scene.camera;
        const auto forward=normalize(subtract(camera.target,camera.eye));
        const auto focal=double(viewport_height)/(2.0*std::tan(double(camera.fovRadians)*0.5));
        const auto light=water->sun;
        if(!std::isfinite(focal)||focal<=0.0)return batch;
        for(const auto& marker:scene.markers)
        {
            if(!marker.waterDetail||!marker.sphericalWater||marker.wireframe||marker.neutralSurface||marker.unlit)continue;
            const auto& view=*marker.waterDetail;
            const auto& appearance=*marker.sphericalWater;
            if(!marker.mesh||marker.mesh->key()!=appearance.artifact_key||appearance.spray_count>appearance.spray_sites.size())return batch;
            const auto scale=view.meters_per_unit;
            if(!std::isfinite(scale)||scale<=0.0)return batch;
            const std::array center{(scene.render_origin[0]-view.world_center[0])*scale,
                (scene.render_origin[1]-view.world_center[1])*scale,(scene.render_origin[2]-view.world_center[2])*scale};
            const auto camera_height=std::hypot(center[0]+camera.eye.x*scale,center[1]+camera.eye.y*scale,
                center[2]+camera.eye.z*scale)-view.radius_meters;
            if(!std::isfinite(camera_height)||camera_height< -0.5||camera_height>=14.0)return batch;
            const auto seconds=scene.water_detail_seconds.value_or(view.seconds);
            const auto rotation=detail::euler_rotation(marker.rotationDegrees);
            for(const auto& site:std::span{appearance.spray_sites}.first(appearance.spray_count))
            {
                const auto base=detail::mesh_vertex_position(marker,site.position,rotation);
                const auto offset=subtract(base,camera.eye);
                const double range=std::hypot(double(offset.x),double(offset.y),double(offset.z))*scale;
                if(!std::isfinite(range)||range>42.0||range<0.4)continue;
                const double r=std::hypot(center[0]+base.x*scale,center[1]+base.y*scale,center[2]+base.z*scale);
                if(!std::isfinite(r)||r<1.0)continue;
                const Vec3 up{float((center[0]+base.x*scale)/r),float((center[1]+base.y*scale)/r),float((center[2]+base.z*scale)/r)};
                const auto tangent=normalize(cross(up,std::abs(up.y)<0.85f?Vec3{0,1,0}:Vec3{1,0,0}));
                const auto crosswind=cross(up,tangent);
                const double random=double(site.cell&0xffffu)/65535.0;
                const double period=0.75+random*1.3;
                const float daylight=std::clamp((dot(up,light)+0.12f)*2.0f,0.0f,1.0f)*water->sunlight;
                for(unsigned drop=0;drop<3u&&batch.size<limit;++drop)
                {
                    double age=std::fmod(seconds+random*period+0.12*drop,period);
                    if(age<0.0)age+=period;
                    const double rise=(2.9+random*0.75)*age-4.9*age*age;
                    if(age<=0.01||rise<=0.01||age>=0.68)continue;
                    const double drift=(0.5+random)*age;
                    const double scatter=(double(drop)-1.0)*age*0.65;
                    const Vec3 point{base.x+float((up.x*rise+tangent.x*drift+crosswind.x*scatter)/scale),
                        base.y+float((up.y*rise+tangent.y*drift+crosswind.y*scatter)/scale),
                        base.z+float((up.z*rise+tangent.z*drift+crosswind.z*scatter)/scale)};
                    const double depth=dot(subtract(point,camera.eye),forward)*scale;
                    if(!std::isfinite(depth)||depth<0.4)continue;
                    const float pixels=static_cast<float>(std::clamp(focal*(0.085+age*0.25)/depth,1.0,22.0));
                    const float opacity=site.strength*static_cast<float>(std::clamp(1.0-(range-12.0)/30.0,0.0,1.0)
                        *std::clamp(1.0-(camera_height-5.0)/9.0,0.0,1.0)
                        *std::clamp(age/0.08,0.0,1.0)*std::clamp((0.68-age)/0.36,0.0,1.0)*0.48);
                    if(pixels>=1.5f&&opacity>0.015f)batch.points[batch.size++]={point,{pixels,opacity,daylight}};
                }
                if(batch.size==limit)return batch;
            }
        }
        return batch;
    }

    export struct SceneFrameSource
    {
        const void* key{};
        std::shared_ptr<const detail::PublishedObjectMarkers> previous{};
        Camera navigation{};
        bool had_previous{};
    };
    export struct PreparedSceneFrame
    {
        SceneFrameSource source{};
        std::shared_ptr<detail::PublishedObjectMarkers> replacement{};
    };
    export struct SceneFrameRequest
    {
        std::vector<ObjectMarker> markers{};
        Camera camera{};
        epochengine::lighting::LightingFrame lighting{};
        MeshFrameLimits limits{};
        std::array<double,3> render_origin{};
        std::vector<CloudVolume> clouds{};
        double depth_fit_aspect{}; // Zero keeps callers without viewport evidence conservative.
        bool space_background{};
        std::optional<double> water_detail_seconds{}; // Live view data, independent of geometry preparation.
        WaterFrameSource water_source{}; // Identity of the geometry admitted by the worker.
    };
    export [[nodiscard]] bool same_scene_frame_request(const SceneFrameRequest& a,const SceneFrameRequest& b) noexcept
    {
        const auto same=[](Vec3 x,Vec3 y){return x.x==y.x&&x.y==y.y&&x.z==y.z;};
        const auto& x=a.camera;const auto& y=b.camera;
        const auto sameClouds=[](const auto& left,const auto& right) {
            if(left.size()!=right.size())return false;
            for(std::size_t i=0;i<left.size();++i) {
                auto a=left[i],b=right[i];a.time_seconds=0.0;b.time_seconds=0.0;
                if(a!=b)return false;
            }
            return true;
        };
        if(a.space_background!=b.space_background||a.depth_fit_aspect!=b.depth_fit_aspect||!sameClouds(a.clouds,b.clouds)||a.render_origin!=b.render_origin||a.limits.maximum_indices!=b.limits.maximum_indices
            ||a.limits.maximum_bytes!=b.limits.maximum_bytes||a.markers.size()!=b.markers.size()
            ||!detail::same_solid_lighting(a.lighting,b.lighting)||!same(x.eye,y.eye)||!same(x.target,y.target)
            ||!same(x.up,y.up)||x.fovRadians!=y.fovRadians||x.nearPlane!=y.nearPlane||x.farPlane!=y.farPlane
            ||x.projection!=y.projection||x.orientation!=y.orientation||x.orthographicVerticalSize!=y.orthographicVerticalSize)return false;
        for(std::size_t index=0;index<a.markers.size();++index)
            if(!detail::same_marker(a.markers[index],b.markers[index]))return false;
        return true;
    }
    export [[nodiscard]] std::optional<SceneFrameSource> capture_scene_frame_source(const void* ctxKey)
    {
        const auto key=detail::normalize_camera_key(ctxKey);if(!key)return {};
        try
        {
            SceneFrameSource result{.key=key,.navigation=camera_for(key)};
            detail::PublishedObjectMarkers snapshot;
            {
                std::shared_lock lock(detail::g_objectMarkerMutex);
                const auto current=detail::g_objectMarkers.find(key);
                if(current!=detail::g_objectMarkers.end())
                {snapshot=current->second;result.had_previous=true;}
            }
            result.previous=std::make_shared<const detail::PublishedObjectMarkers>(std::move(snapshot));
            return result;
        }
        catch(...){return {};}
    }
    export [[nodiscard]] std::optional<PreparedSceneFrame> prepare_scene_frame(
        SceneFrameSource source,std::span<const ObjectMarker> markers,Camera camera,
        const epochengine::lighting::LightingFrame& lights,MeshFrameLimits limits={},
        const std::array<double,3>& render_origin={},std::stop_token cancellation={},
        std::span<const CloudVolume> clouds={},bool space_background=false,WaterFrameSource water_source={})
    {
        if(!source.key||!source.previous||cancellation.stop_requested()||!admitted_cloud_volumes(clouds))return {};
        const auto validated=detail::admitted_scene_frame_camera(markers,camera,lights,limits,render_origin);
        if(!validated)return {};
        try
        {
            const auto& snapshot=*source.previous;
            if(snapshot.scene_frame&&snapshot.scene_frame->revision==(std::numeric_limits<std::uint64_t>::max)())return {};
            auto prepared=detail::prepare_object_markers(markers,snapshot,lights,*validated,false);if(!prepared)return {};
            if(cancellation.stop_requested())return {};
            auto& replacement=*prepared;
            std::uint64_t extra=sizeof(PublishedSceneFrame)+markers.size()*sizeof(ObjectMarker)+clouds.size()*sizeof(CloudVolume);
            if(snapshot.scene_frame)extra+=sizeof(PublishedSceneFrame)+snapshot.scene_frame->markers.capacity()*sizeof(ObjectMarker)
                +snapshot.scene_frame->clouds.capacity()*sizeof(CloudVolume);
            if(!detail::solid_cache_admitted(replacement,lights,validated->eye,extra,true))return {};
            replacement.solid_geometry=detail::prepare_solid_geometry(replacement,lights,*validated,true);
            if(cancellation.stop_requested())return {};
            auto frame=std::make_shared<PublishedSceneFrame>();
            const auto& navigation=source.navigation;
            frame->camera=*validated;frame->worldNavigationEye=navigation.eye;
            frame->preciseNavigationEye=camera_world_eye(navigation);frame->solids=replacement.solid_geometry;
            frame->lines=replacement.line_geometry?replacement.line_geometry:std::make_shared<const std::vector<Vertex>>();
            if(replacement.mesh_wire_geometry)frame->wire=replacement.mesh_wire_geometry;
            else
            {
                const auto empty=std::make_shared<const std::vector<Vertex>>();
                frame->wire=std::make_shared<const MeshWireDrawFrame>(MeshWireDrawFrame{empty,empty});
            }
            frame->markers=replacement.markers;frame->revision=snapshot.scene_frame?snapshot.scene_frame->revision+1u:1u;
            frame->render_origin=render_origin;
            frame->clouds.assign(clouds.begin(),clouds.end());
            frame->space_background=space_background;
            frame->water_source=water_source;
            replacement.scene_frame=std::move(frame);
            return PreparedSceneFrame{std::move(source),
                std::make_shared<detail::PublishedObjectMarkers>(std::move(replacement))};
        }
        catch(...){return {};}
    }
    namespace detail
    {
        // View-only changes share the admitted geometry and stay in its origin.
        // Both camera advance and a delayed geometry commit use this same math.
        [[nodiscard]] inline bool reframe_scene_view(PublishedSceneFrame& frame,Camera camera,
            const std::array<double,3>& render_origin,
            std::optional<std::span<const CloudVolume>> clouds)
        {
            if(clouds&&!admitted_cloud_volumes(*clouds))return false;
            const auto source=validate_camera_frame(camera);if(!source)return false;
            const auto forward=camera_forward(*source);
            std::array<float,3> eye{};
            const std::array<double,3> local{source->eye.x,source->eye.y,source->eye.z};
            for(std::size_t axis=0;axis<3;++axis)
            {
                const double value=local[axis]+render_origin[axis]-frame.render_origin[axis];
                if(!std::isfinite(value)||std::abs(value)>(std::numeric_limits<float>::max)())return false;
                eye[axis]=static_cast<float>(value);
            }
            // Translating a unit look target separately rounds away its heading
            // at million-meter offsets. Retain a representable look distance.
            const float lookDistance=(std::max)(std::sqrt(dot(subtract(source->target,source->eye),
                subtract(source->target,source->eye))),
                (std::max)({1.0f,std::abs(eye[0]),std::abs(eye[1]),std::abs(eye[2])}));
            camera=*source;camera.eye={eye[0],eye[1],eye[2]};
            camera.target=add(camera.eye,scale(forward,lookDistance));
            const auto validated=validate_camera_frame(camera);if(!validated)return false;
            frame.camera=*validated;
            if(clouds)
            {
                frame.clouds.assign(clouds->begin(),clouds->end());
                for(auto& cloud:frame.clouds)for(unsigned axis=0;axis<3u;++axis)
                    cloud.center[axis]+=render_origin[axis]-frame.render_origin[axis];
                if(!admitted_cloud_volumes(frame.clouds))return false;
            }
            return true;
        }
    }
    export [[nodiscard]] bool commit_scene_frame(const void* ctxKey,PreparedSceneFrame prepared,
        const SceneFrameRequest* latest_view=nullptr)
    {
        const auto key=detail::normalize_camera_key(ctxKey);
        if(!key||key!=prepared.source.key||!prepared.source.previous||!prepared.replacement)return false;
        try
        {
            const auto navigation=latest_view?std::optional<Camera>{camera_for(key)}:std::nullopt;
            std::unique_lock lock(detail::g_objectMarkerMutex);
            const auto current=detail::g_objectMarkers.find(key);
            const auto& snapshot=*prepared.source.previous;
            if(prepared.source.had_previous?(current==detail::g_objectMarkers.end()
                ||current->second.line_geometry!=snapshot.line_geometry
                ||current->second.solid_geometry!=snapshot.solid_geometry):current!=detail::g_objectMarkers.end())return false;
            // Rendering consumes this packet independently of the frame owner.
            // Publish prepared geometry with the latest view in one operation;
            // a later camera correction would expose the queued view meanwhile.
            const auto revision=current==detail::g_objectMarkers.end()||!current->second.scene_frame
                ?0u:current->second.scene_frame->revision;
            if(revision==(std::numeric_limits<std::uint64_t>::max)())return false;
            auto frame=std::make_shared<PublishedSceneFrame>(*prepared.replacement->scene_frame);
            const auto currentFrame=current==detail::g_objectMarkers.end()?nullptr:current->second.scene_frame;
            const bool advanced=currentFrame&&currentFrame!=snapshot.scene_frame;
            if(latest_view||advanced)
            {
                // Only view fields are consumed; this never re-admits markers,
                // copies mesh arrays, or fits triangles on the publication owner.
                const auto origin=latest_view?latest_view->render_origin:currentFrame->render_origin;
                const auto validated=validate_camera_frame(latest_view?latest_view->camera:currentFrame->camera);
                if(!validated)return false;
                auto camera=*validated;
                auto fittedView=camera;fittedView.nearPlane=frame->camera.nearPlane;fittedView.farPlane=frame->camera.farPlane;
                if(origin==frame->render_origin&&detail::same_camera_frame(fittedView,frame->camera))
                {camera.nearPlane=frame->camera.nearPlane;camera.farPlane=frame->camera.farPlane;}
                else
                {
                    // A fit for an older heading/eye cannot tighten the live
                    // view. Keep both clip intervals until its own fit arrives.
                    camera.nearPlane=(std::min)(camera.nearPlane,frame->camera.nearPlane);
                    camera.farPlane=(std::max)(camera.farPlane,frame->camera.farPlane);
                }
                const auto clouds=latest_view?std::span<const CloudVolume>{latest_view->clouds}
                    :std::span<const CloudVolume>{currentFrame->clouds};
                if(!detail::reframe_scene_view(*frame,camera,origin,clouds))return false;
                frame->water_detail_seconds=latest_view?latest_view->water_detail_seconds:currentFrame->water_detail_seconds;
                frame->worldNavigationEye=latest_view?navigation->eye:currentFrame->worldNavigationEye;
                frame->preciseNavigationEye=latest_view?camera_world_eye(*navigation):currentFrame->preciseNavigationEye;
            }
            frame->revision=revision+1u;prepared.replacement->scene_frame=std::move(frame);
            detail::g_objectMarkers.insert_or_assign(key,std::move(*prepared.replacement));
            return true;
        }
        catch(...){return false;}
    }
    // Synchronous callers retain their existing preparation/publication order.
    export [[nodiscard]] bool publish_scene_frame(const void* ctxKey,std::span<const ObjectMarker> markers,
        Camera camera,const epochengine::lighting::LightingFrame& lights,MeshFrameLimits limits={},
        const std::array<double,3>& render_origin={},std::span<const CloudVolume> clouds={})
    {
        // Invalid input must be rejected before capturing allocating snapshots.
        // MSVC Debug vectors allocate iterator proxies in noexcept constructors.
        if(!admitted_cloud_volumes(clouds)
            ||!detail::admitted_scene_frame_camera(markers,camera,lights,limits,render_origin))return false;
        const auto source=capture_scene_frame_source(ctxKey);if(!source)return false;
        auto prepared=prepare_scene_frame(*source,markers,camera,lights,limits,render_origin,{},clouds);
        return prepared&&commit_scene_frame(ctxKey,std::move(*prepared));
    }

    // A rejected geometry replacement must not freeze navigation. Retain its
    // immutable geometry and express the new camera in that packet's origin.
    // Geometry and camera still reach every backend through one atomic packet.
    export [[nodiscard]] inline bool advance_scene_frame_camera(const void* ctxKey,Camera camera,
        const std::array<double,3>& render_origin,
        std::optional<std::span<const CloudVolume>> clouds=std::nullopt,
        std::optional<double> water_detail_seconds=std::nullopt) noexcept
    {
        try
        {
            const auto key=detail::normalize_camera_key(ctxKey);if(!key)return false;
            const auto previous=scene_frame_for(key);
            if(!previous||previous->revision==(std::numeric_limits<std::uint64_t>::max)())return false;
            auto replacement=std::make_shared<PublishedSceneFrame>(*previous);
            if(!detail::reframe_scene_view(*replacement,camera,render_origin,clouds))return false;
            if(water_detail_seconds)replacement->water_detail_seconds=water_detail_seconds;
            ++replacement->revision;
            const auto navigation=camera_for(key);
            replacement->worldNavigationEye=navigation.eye;
            replacement->preciseNavigationEye=camera_world_eye(navigation);
            std::unique_lock lock(detail::g_objectMarkerMutex);
            const auto current=detail::g_objectMarkers.find(key);
            if(current==detail::g_objectMarkers.end()||current->second.scene_frame!=previous)return false;
            current->second.scene_frame=std::move(replacement);
            return true;
        }
        catch(...){return false;}
    }

    export [[nodiscard]] inline std::shared_ptr<const std::vector<Vertex>> object_solid_geometry_for(const void* ctxKey)
    { return object_solid_frame_for(ctxKey)->geometry; }
    // Compatibility owner for callers requiring a mutable copy. Backends retain
    // object_solid_geometry_for, sharing the exact immutable frame instead.
    export [[nodiscard]] inline std::vector<Vertex> object_solid_vertices_for(const void* ctxKey)
    {return *object_solid_geometry_for(ctxKey);}

    export [[nodiscard]] inline std::vector<ObjectMarker> sampled_render_surface_markers_for(const void* ctxKey)
    {
        const void* const rigKey = detail::normalize_camera_key(ctxKey);
        if (!rigKey)
            return {};

        std::vector<ObjectMarker> markers{};
        {
            std::shared_lock lock(detail::g_objectMarkerMutex);
            const auto it = detail::g_objectMarkers.find(rigKey);
            if (it == detail::g_objectMarkers.end())
                return {};
            markers = it->second.markers;
        }

        const Camera camera = camera_for(rigKey);
        std::erase_if(
            markers,
            [camera](const ObjectMarker& marker) noexcept
            {
                const ObjectPreviewGeometryRoute route =
                    object_preview_geometry_route(marker.primitive);
                if (!marker.sampledRenderSurface || !route.sampled_surface)
                    return true;
                return marker.primitive == ObjectPreviewPrimitive::EngineArcadeScreen
                    && !render_arcade::screen_sample_plane_faces_viewer(
                        marker.position.z,
                        marker.scale.z,
                        camera.eye.z);
            });
        return markers;
    }
    export [[nodiscard]] inline std::vector<ObjectMarker> sampled_render_surface_markers_for(const PublishedSceneFrame& frame)
    {
        auto markers=frame.markers;const auto& camera=frame.camera;
        std::erase_if(
            markers,
            [camera](const ObjectMarker& marker) noexcept
            {
                const ObjectPreviewGeometryRoute route =
                    object_preview_geometry_route(marker.primitive);
                if (!marker.sampledRenderSurface || !route.sampled_surface)
                    return true;
                return marker.primitive == ObjectPreviewPrimitive::EngineArcadeScreen
                    && !render_arcade::screen_sample_plane_faces_viewer(
                        marker.position.z,
                        marker.scale.z,
                        camera.eye.z);
            });
        return markers;
    }

    export [[nodiscard]] inline std::shared_ptr<const GridGeometry> grid_geometry_for(
        const void* ctxKey)
    {
        static const auto hidden = std::make_shared<const GridGeometry>();
        const void* const rigKey = detail::normalize_camera_key(ctxKey);
        if (!authoring_guides_visible(rigKey))
            return hidden;

        const detail::GridPlacement placement = detail::grid_placement(camera_for(rigKey));

        if (rigKey)
        {
            std::shared_lock lock(detail::g_gridGeometryMutex);
            const auto found = detail::g_authoringGuides.find(rigKey);
            if (found != detail::g_authoringGuides.end())
            {
                const auto& state = found->second;
                if (!state.visible)
                    return hidden;
                if (state.grid && state.grid->signature == placement.signature)
                    return state.grid;
            }
        }

        auto geometry = std::make_shared<const GridGeometry>(
            detail::build_grid_geometry(placement));
        if (!rigKey)
            return geometry;

        std::unique_lock lock(detail::g_gridGeometryMutex);
        auto& state = detail::g_authoringGuides[rigKey];
        if (!state.visible)
            return hidden;
        auto& cached = state.grid;
        if (cached && cached->signature == placement.signature)
            return cached;
        cached = std::move(geometry);
        return cached;
    }
}
