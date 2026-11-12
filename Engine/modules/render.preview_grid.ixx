/************************************************
 *  ███████╗██████╗  ██████╗  ██████╗██╗  ██╗   *
 *  ██╔════╝██╔══██╗██╔═══██╗██╔════╝██║  ██║   *
 *  █████╗  ██████╔╝██║   ██║██║     ███████║   *
 *  ██╔══╝  ██╔═══╝ ██║   ██║██║     ██╔══██║   *
 *  ███████╗██║     ╚██████╔╝╚██████╗██║  ██║   *
 *  ╚══════╝╚═╝      ╚═════╝  ╚═════╝╚═╝  ╚═╝   *
 *                                              *
 *   This file is part of the Epoch   Project.  *
 *   epochengine - Modular C++ Framework        *
 *                                              *
 *   SPDX-License-Identifier:                   *
 *   LicenseRef-MIT-NoSell                      *
 *                                              *
 *   Provided "AS IS", without warranty         *
 *   of any kind.                               *
 *                                              *
 *   Use permitted for Non-Commercial           *
 *   Purposes ONLY, without prior               *
 *   commercial licensing agreement.            *
 *                                              *
 *   Redistribution Allowed with This Notice    *
 *   and LICENSE file.                          *
 *                                              *
 *   No obligation to disclose                  *
 *   modifications.                             *
 *                                              *
 *   See LICENSE file for full terms.           *
 *                                              *
 ***********************************************/
// Epoch system notes (auto-checked):
// Epoch Render: preserve backend boundaries and resource lifetime contracts.
module;

#include <algorithm>
#include <array>
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
    };

    // Presentation-only paint colors. Geometry, depth and authored materials stay intact.
    export struct MeshPaintRegion final
    {
        std::array<double,3> center{}, direction{};
        double radius{}, minimum_dot{};
        std::array<float,3> color{0.20f,0.66f,0.27f};
        friend bool operator==(const MeshPaintRegion&,const MeshPaintRegion&) noexcept = default;
    };
    export struct MeshPaintView final
    {
        std::array<double,3> origin{};
        double meters_per_unit{1.0};
        std::array<float,3> color{0.34f,0.39f,0.37f};
        std::vector<MeshPaintRegion> regions;
        friend bool operator==(const MeshPaintView&,const MeshPaintView&) noexcept = default;
    };
    export [[nodiscard]] inline Vec3 mesh_paint_albedo(const MeshPaintView& paint,
        const std::array<float,3>& position) noexcept
    {
        for(const auto& region:paint.regions)
        {
            std::array<double,3> delta{};double square{},projection{};
            for(std::size_t axis=0;axis<3;++axis) {
                delta[axis]=double(position[axis])*paint.meters_per_unit+paint.origin[axis]-region.center[axis];
                square+=delta[axis]*delta[axis];projection+=delta[axis]*region.direction[axis]; }
            // Reject other bodies and back-facing caps before the angular test.
            if(square>region.radius*region.radius*0.25&&square<region.radius*region.radius*2.25
                &&projection>=std::sqrt(square)*region.minimum_dot)
                return {region.color[0],region.color[1],region.color[2]};
        }
        return {paint.color[0],paint.color[1],paint.color[2]};
    }

    export struct SolidDrawMarker final
        {
            ObjectMarker marker{};
            std::shared_ptr<const std::vector<Vertex>> geometry{};
            std::shared_ptr<const std::vector<TexturedVertex>> textured{};
        };
    export struct SolidDrawFrame final
        {
            epochengine::lighting::LightingFrame lighting{};
            Vec3 eye{};
            std::vector<SolidDrawMarker> markers{};
            std::shared_ptr<const std::vector<Vertex>> geometry{};
        };
    export [[nodiscard]] std::shared_ptr<const SolidDrawFrame> object_solid_frame_for(const void* ctxKey);

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
    };

    export [[nodiscard]] constexpr bool object_marker_uses_solid_fill(
        const ObjectMarker& marker) noexcept
    {
        if (marker.wireframe && marker.primitive == ObjectPreviewPrimitive::IndexedMesh)
            return false;
        if (!object_preview_geometry_route(marker.primitive).solid_scene)
            return false;
        if (!marker.editorOnly)
            return true;

        return marker.primitive != ObjectPreviewPrimitive::Camera
            && marker.primitive != ObjectPreviewPrimitive::Light;
    }

    export inline const std::array<float, 4> kClearColor = epochengine::visuals::scene_background();

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
            std::vector<TexturedVertex>* textured = nullptr, bool wire_depth = false);
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
        export inline std::unordered_map<const void*, std::shared_ptr<const GridGeometry>, PtrHash> g_gridGeometries{};
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
                && lhs.wireframe == rhs.wireframe && lhs.forestLighting == rhs.forestLighting
                && lhs.preciseTransform == rhs.preciseTransform
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

        [[nodiscard]] inline bool valid_camera_snapshot(const CameraRigSnapshot& snapshot) noexcept
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

        [[nodiscard]] inline float navigation_far_plane(const CameraRigState& rig) noexcept
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

        [[nodiscard]] inline float navigation_near_plane(const CameraRigState& rig) noexcept
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

        inline void commit_camera_navigation(CameraRigState& destination, CameraRigState candidate) noexcept
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

        [[nodiscard]] inline Camera local_camera_from_rig(const CameraRigState& rig) noexcept
        {
            if (rig.externalFrame) return *rig.externalFrame;
            const Vec3 worldUp{ 0.0f, 1.0f, 0.0f };
            Vec3 forward = forward_from_angles(rig.yawDegrees, rig.pitchDegrees);
            Vec3 cameraUp = worldUp;
            if (rig.mode == CameraMode::Editor && std::abs(rig.pitchDegrees) >= 85.0f)
            {
                // Continue over either pole without crossing forward with a
                // parallel world-up vector or flipping the view upside down.
                // Inside the legacy range retain its exact unbanked up input.
                constexpr float radians = 0.017453292519943295f;
                const float yaw = rig.yawDegrees * radians;
                const float pitch = rig.pitchDegrees * radians;
                cameraUp = {-std::sin(pitch) * std::cos(yaw), std::cos(pitch),
                    -std::sin(pitch) * std::sin(yaw)};
            }
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
        [[nodiscard]] inline Camera camera_from_rig(const CameraRigState& rig) noexcept
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
        [[nodiscard]] inline render_camera::ViewDescriptor descriptor_from_rig(
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

        [[nodiscard]] inline bool same_camera_frame(const Camera& lhs, const Camera& rhs) noexcept
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

        [[nodiscard]] inline std::optional<Camera> validated_camera_frame(Camera camera) noexcept
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

        [[nodiscard]] inline CameraRigSnapshot snapshot_from_rig(const CameraRigState& rig) noexcept
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

        [[nodiscard]] inline CameraRigState rig_from_snapshot(const CameraRigSnapshot& snapshot) noexcept
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

    export [[nodiscard]] inline CameraMode camera_mode_for(const void* ctxKey) noexcept
    {
        const void* const rigKey = detail::normalize_camera_key(ctxKey);
        if (!rigKey)
            return CameraMode::Editor;

        std::shared_lock lock(detail::g_cameraRigMutex);
        const auto it = detail::g_cameraRigs.find(rigKey);
        return it != detail::g_cameraRigs.end() ? it->second.mode : CameraMode::Editor;
    }

    export [[nodiscard]] inline EditorView editor_view_for(const void* ctxKey) noexcept
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

    export inline bool set_editor_view(const void* ctxKey, EditorView view) noexcept
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

    export [[nodiscard]] inline CameraRigSnapshot capture_camera_rig_snapshot(const void* ctxKey) noexcept
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

    export inline bool restore_camera_rig_snapshot(const void* ctxKey, const CameraRigSnapshot& snapshot) noexcept
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

    export inline void set_camera_mode(const void* ctxKey, CameraMode mode) noexcept
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
    export inline void reset_camera(const void* ctxKey, bool clear_scene_extent = false) noexcept
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

    namespace detail
    {
    inline bool focus_camera_impl(
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
    }
    export inline bool focus_camera(const void* ctxKey,Vec3 focus,float framingRadius=1.0f) noexcept
    {return detail::focus_camera_impl(ctxKey,{focus.x,focus.y,focus.z},framingRadius,false);}
    export inline bool focus_camera_precise(const void* ctxKey,const std::array<double,3>& focus,
        float framingRadius=1.0f) noexcept
    {return detail::focus_camera_impl(ctxKey,focus,framingRadius,true);}
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

    export inline bool frame_initial_orbit_mesh(const void* ctxKey,const ObjectMarker& marker,float aspect) noexcept
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

    export [[nodiscard]] inline float camera_distance_for(const void* ctxKey) noexcept
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
        [[nodiscard]] inline float surface_limited_zoom(const Camera& camera, Vec3 direction, float distance,
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

    export inline void zoom_camera(const void* ctxKey, float amount) noexcept
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

    export [[nodiscard]] inline std::uint64_t camera_revision_for(const void* ctxKey) noexcept
    {
        const void* const rigKey = detail::normalize_camera_key(ctxKey);
        if (!rigKey)
            return 0;

        std::shared_lock lock(detail::g_cameraRigMutex);
        const auto it = detail::g_cameraRigs.find(rigKey);
        return it != detail::g_cameraRigs.end() ? it->second.revision : 0;
    }
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

    export inline void pan_camera_drag(
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

    export inline void dolly_camera_drag(const void* ctxKey, float deltaPixels) noexcept
    {
        if (!std::isfinite(deltaPixels) || deltaPixels == 0.0f)
            return;
        zoom_camera(ctxKey, -deltaPixels * 0.045f);
    }

    export inline void adjust_fly_speed(const void* ctxKey, float wheelSteps) noexcept
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

    export [[nodiscard]] inline float fly_speed_for(const void* ctxKey) noexcept
    {
        const void* const rigKey = detail::normalize_camera_key(ctxKey);
        if (!rigKey)
            return 6.5f;
        std::shared_lock lock(detail::g_cameraRigMutex);
        const auto it = detail::g_cameraRigs.find(rigKey);
        return it != detail::g_cameraRigs.end() ? it->second.flySpeed : 6.5f;
    }

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
            detail::g_gridGeometries.erase(rigKey);
        }
    }

    // A caller-owned simulation may supply a transient, arbitrary camera basis.
    // It shares the existing per-context rig and revision; snapshots deliberately
    // retain only the ordinary navigation state, never this external frame.
    export [[nodiscard]] inline std::optional<Camera> validate_camera_frame(Camera camera) noexcept
    {return detail::validated_camera_frame(camera);}

    export [[nodiscard]] inline bool set_external_camera_frame(const void* ctxKey, const Camera& camera) noexcept
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

    export [[nodiscard]] inline bool clear_external_camera_frame(const void* ctxKey) noexcept
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

    export [[nodiscard]] inline Camera camera_for(const void* ctxKey) noexcept
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

    export [[nodiscard]] inline render_camera::ViewDescriptor camera_descriptor_for(
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
                if(!mesh_wire_requested(marker)) continue;
                while(j<previous.size()&&!mesh_wire_requested(previous[j])) ++j;
                if(j==previous.size()) return false;
                const auto& old=previous[j++];
                if(marker.mesh->key()!=old.mesh->key()||marker.preciseTransform!=old.preciseTransform
                    ||marker.selected!=old.selected||marker.editorOnly!=old.editorOnly
                    ||marker.color.x!=old.color.x||marker.color.y!=old.color.y||marker.color.z!=old.color.z
                    ||marker.position.x!=old.position.x||marker.position.y!=old.position.y||marker.position.z!=old.position.z
                    ||marker.scale.x!=old.scale.x||marker.scale.y!=old.scale.y||marker.scale.z!=old.scale.z
                    ||marker.rotationDegrees.x!=old.rotationDegrees.x||marker.rotationDegrees.y!=old.rotationDegrees.y
                    ||marker.rotationDegrees.z!=old.rotationDegrees.z) return false;
            }
            while(j<previous.size()&&!mesh_wire_requested(previous[j])) ++j;
            return j==previous.size();
        }
        [[nodiscard]] inline bool wireframe_working_set_admitted(std::span<const ObjectMarker> markers,
            const PublishedObjectMarkers& previous, std::uint64_t retained_extra = 0u,
            bool legacy_lines = true) noexcept
        {
            if (!std::any_of(markers.begin(), markers.end(), mesh_wire_requested)) return true;
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
                if (!mesh_wire_requested(marker)) continue;
                if(!reuseTriangles&&!admit(marker.mesh->indices().size(),sizeof(Vertex))) return false;
                // Framed consumers draw the triangle stream directly, or visit
                // its front-facing edges. No consumer reads a second all-edge
                // stream from that packet; legacy marker getters still need it.
                if(!legacy_lines) continue;
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
                + marker.mesh->indices().size() * sizeof(std::uint32_t);
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
            if(std::any_of(markers.begin(),markers.end(),detail::mesh_wire_requested))
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
        const auto safe_axis = [](float value, float fallback) noexcept
        {
            const float magnitude = value < 0.0f ? -value : value;
            return (std::max)(magnitude, fallback);
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
            Vec3 half{
                safe_axis(marker.scale.x * 0.5f, radius * 0.45f),
                safe_axis(marker.scale.y * 0.5f, radius * 0.45f),
                safe_axis(marker.scale.z * 0.5f, radius * 0.45f)
            };

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
                    half.y = (std::max)(0.05f, radius * 0.08f);
                    push_box_edges(center, half, color);
                    break;
                case ObjectPreviewPrimitive::Canvas2D:
                    half.z = (std::max)(0.025f, radius * 0.04f);
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
        std::vector<TexturedVertex>* textured, bool wire_depth)
    {
        std::vector<Vertex> out{};
        std::size_t reserve{};
        for (const auto& marker : markers)
        {
            if(!wire_depth&&!marker.baseColorTexture)reserve+=96u;
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
        const auto safe_axis = [](float value, float fallback) noexcept
        {
            const float magnitude = value < 0.0f ? -value : value;
            return (std::max)(magnitude, fallback);
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
            float visibility = 1.0f) noexcept
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
                    .specularColor = forest ? epochengine::lighting::Color3{} : epochengine::lighting::Color3{0.18f, 0.18f, 0.18f},
                    .shininess = 24.0f,
                    .twoSidedDiffuse = foliage,
                    .shadowedLight = sun,
                    .directVisibility = visibility
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
            if (wire_depth ? !detail::mesh_wire_requested(marker) : !object_marker_uses_solid_fill(marker))
                continue;

            const float radius = (std::clamp)(marker.radius, 0.16f, 1.75f);
            const bool forestPrimitive = marker.forestLighting
                || marker.primitive == ObjectPreviewPrimitive::ForestTrunk
                || marker.primitive == ObjectPreviewPrimitive::ForestBranch
                || marker.primitive == ObjectPreviewPrimitive::ForestLeafCluster;
            Vec3 color = marker.selected && !forestPrimitive ? visual_rgb(epochengine::visuals::object_selected()) : marker.color;
            if (marker.editorOnly && !marker.selected)
                color = scale(color, epochengine::visuals::editor_solid_opacity_factor());

            const Vec3 center{ marker.position.x, marker.position.y, marker.position.z };
            Vec3 half{
                safe_axis(marker.scale.x * 0.5f, radius * 0.45f),
                safe_axis(marker.scale.y * 0.5f, radius * 0.45f),
                safe_axis(marker.scale.z * 0.5f, radius * 0.45f)
            };

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
                std::vector<std::uint32_t> mapped(vertices.size(), missing);
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
                    for(std::size_t corner=0;corner<3u;++corner)
                    {
                    const auto index=indices[triangle+(reverse&&corner?3u-corner:corner)];
                    if (mapped[index] != missing)
                    {
                        if (marker.baseColorTexture) textured->push_back({transformed[mapped[index]].position,
                            transformed[mapped[index]].color, vertices[index].uv});
                        else out.push_back(transformed[mapped[index]]);
                        continue;
                    }
                    const auto& source = vertices[index];
                    const auto position = detail::mesh_vertex_position(marker,source.position,rotation);
                    const auto normal = normalize(detail::mesh_vertex_normal(marker,source.normal,rotation));
                    Vec3 albedo = marker.paintView ? mesh_paint_albedo(*marker.paintView,source.position)
                        : marker.baseColorTexture && !marker.baseColorTexture->modulate_vertex_color
                            ? Vec3{1.0f,1.0f,1.0f} : Vec3{source.color[0], source.color[1], source.color[2]};
                    if (marker.selected && !marker.forestLighting) albedo = add(scale(albedo, 0.85f), scale(color, 0.15f));
                    mapped[index] = static_cast<std::uint32_t>(transformed.size());
                    const auto visibility = shadow ? detail::forest_shadow_visibility(*shadow,
                        detail::mesh_vertex_position(localMarker, source.position, rotation)) : 1.0f;
                    transformed.push_back(make_vertex(position, shade_surface(position, normal, albedo, marker.forestLighting,
                        marker.forestLighting && source.uv[0] >= 0.5f, sun, visibility)));
                    if (marker.baseColorTexture) textured->push_back({transformed.back().position,
                        transformed.back().color, source.uv});
                    else out.push_back(transformed.back());
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
                half.y = (std::max)(0.05f, radius * 0.08f);
                push_oriented_box(center, half, marker.rotationDegrees, color);
                break;
            case ObjectPreviewPrimitive::Canvas2D:
                half.z = (std::max)(0.025f, radius * 0.04f);
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
                    if (!clockwise_solid_triangle_faces_camera(a, b, c, camera.eye))
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
                +frame.lighting.lights.capacity()*sizeof(epochengine::lighting::FrameLight);
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
                    ?marker.mesh->indices().size()+96u:384u;
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
            const bool aggregateOnly=framed_batch&&aggregate_solid_batch(frame.markers,lights);
            const bool reuseAggregate=aggregateOnly&&frame.solid_geometry
                &&same_solid_frame(*frame.solid_geometry,frame.markers,lights,eye);
            for(const auto& marker:frame.markers)
            {
                const bool solid=object_marker_uses_solid_fill(marker);
                const std::uint64_t count=solid_marker_vertex_bound(marker);
                if(!marker.baseColorTexture)vertices+=count;
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
        const bool aggregateOnly=framed_batch&&aggregate_solid_batch(snapshot.markers,lights);
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
        if(aggregateOnly)replacement->geometry=std::make_shared<const std::vector<Vertex>>(
            detail::build_marker_solids(snapshot.markers,lights,camera));
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
        if(a.depth_fit_aspect!=b.depth_fit_aspect||!sameClouds(a.clouds,b.clouds)||a.render_origin!=b.render_origin||a.limits.maximum_indices!=b.limits.maximum_indices
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
        std::span<const CloudVolume> clouds={})
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
        std::optional<std::span<const CloudVolume>> clouds=std::nullopt) noexcept
    {
        try
        {
            const auto key=detail::normalize_camera_key(ctxKey);if(!key)return false;
            const auto previous=scene_frame_for(key);
            if(!previous||previous->revision==(std::numeric_limits<std::uint64_t>::max)())return false;
            auto replacement=std::make_shared<PublishedSceneFrame>(*previous);
            if(!detail::reframe_scene_view(*replacement,camera,render_origin,clouds))return false;
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
        const void* const rigKey = detail::normalize_camera_key(ctxKey);
        const detail::GridPlacement placement = detail::grid_placement(camera_for(rigKey));

        if (rigKey)
        {
            std::shared_lock lock(detail::g_gridGeometryMutex);
            const auto found = detail::g_gridGeometries.find(rigKey);
            if (found != detail::g_gridGeometries.end()
                && found->second
                && found->second->signature == placement.signature)
            {
                return found->second;
            }
        }

        auto geometry = std::make_shared<const GridGeometry>(
            detail::build_grid_geometry(placement));
        if (!rigKey)
            return geometry;

        std::unique_lock lock(detail::g_gridGeometryMutex);
        auto& cached = detail::g_gridGeometries[rigKey];
        if (cached && cached->signature == placement.signature)
            return cached;
        cached = std::move(geometry);
        return cached;
    }
}
