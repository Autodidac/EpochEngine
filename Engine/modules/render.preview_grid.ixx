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
module;

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <span>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

export module render.preview_grid;

import visuals.engine;
import render.arcade;
export import render.camera;
import render.lighting;

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
        EngineArcadeCabinet
    };

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
                .selection_wire = false,
                .sampled_surface = false
            };
        case ObjectPreviewPrimitive::Spawn:
            return ObjectPreviewGeometryRoute{
                .solid_scene = true,
                .marker_wire = true,
                .selection_wire = true,
                .sampled_surface = false
            };
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
    };

    export struct ObjectMarker
    {
        Vec3 position{};
        Vec3 color{ visual_rgb(epochengine::visuals::object_default()) };
        Vec3 scale{ 1.0f, 1.0f, 1.0f };
        float radius = 0.35f;
        ObjectPreviewPrimitive primitive{ ObjectPreviewPrimitive::Cube };
        bool selected = false;
        bool editorOnly = false;
        bool sampledRenderSurface = false;
    };

    export [[nodiscard]] constexpr bool object_marker_uses_solid_fill(
        const ObjectMarker& marker) noexcept
    {
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
    export [[nodiscard]] inline Vec3 normalize(Vec3 value) noexcept
    {
        const float lengthSq = dot(value, value);
        if (lengthSq <= 1.0e-8f)
            return { 0.0f, 0.0f, 0.0f };

        const float invLength = 1.0f / std::sqrt(lengthSq);
        return { value.x * invLength, value.y * invLength, value.z * invLength };
    }

    export [[nodiscard]] inline Mat4 identity_matrix() noexcept
    {
        return Mat4{
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        };
    }

    export [[nodiscard]] inline Mat4 multiply(const Mat4& lhs, const Mat4& rhs) noexcept
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

    export [[nodiscard]] inline Mat4 perspective(
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

    export [[nodiscard]] inline Mat4 orthographic(
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

    export [[nodiscard]] inline Mat4 look_at(Vec3 eye, Vec3 target, Vec3 up) noexcept
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
        };

        export inline std::unordered_map<const void*, CameraRigState, PtrHash> g_cameraRigs{};
        export inline std::unordered_map<const void*, Vec3, PtrHash> g_lastMarkerHits{};
        export inline std::unordered_map<const void*, std::vector<ObjectMarker>, PtrHash> g_objectMarkers{};
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
                && lhs.radius == rhs.radius && lhs.primitive == rhs.primitive && lhs.selected == rhs.selected
                && lhs.editorOnly == rhs.editorOnly && lhs.sampledRenderSurface == rhs.sampledRenderSurface;
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

        [[nodiscard]] inline Camera camera_from_rig(const CameraRigState& rig) noexcept
        {
            const Vec3 worldUp{ 0.0f, 1.0f, 0.0f };
            Vec3 forward = forward_from_angles(rig.yawDegrees, rig.pitchDegrees);
            Vec3 cameraUp = worldUp;
            if (render_camera::axis_locked(rig.orientation))
            {
                const auto axis = render_camera::orientation_basis(rig.orientation);
                forward = { axis.forward.x, axis.forward.y, axis.forward.z };
                cameraUp = { axis.up.x, axis.up.y, axis.up.z };
            }

            if (rig.mode == CameraMode::FPS)
            {
                const Vec3 target = add(rig.position, forward);
                return Camera{
                    .eye = rig.position,
                    .target = target,
                    .up = worldUp,
                    .fovRadians = 1.05f,
                    .nearPlane = 0.05f,
                    .farPlane = 4096.0f,
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
                .nearPlane = 0.05f,
                .farPlane = 4096.0f,
                .projection = rig.projection,
                .orientation = rig.orientation,
                .orthographicVerticalSize = rig.orthographicVerticalSize
            };
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
                .revision = rig.revision
            };
        }

        [[nodiscard]] inline CameraRigState rig_from_snapshot(const CameraRigSnapshot& snapshot) noexcept
        {
            CameraRigState rig = make_default_rig(sanitize_camera_mode(snapshot.mode));
            rig.focus = finite_vec_or(snapshot.focus, rig.focus);
            rig.position = finite_vec_or(snapshot.position, rig.position);
            rig.yawDegrees = finite_or(snapshot.yaw_degrees, rig.yawDegrees);
            rig.pitchDegrees = (std::clamp)(
                finite_or(snapshot.pitch_degrees, rig.pitchDegrees),
                -85.0f,
                85.0f);
            rig.freeYawDegrees = finite_or(snapshot.free_yaw_degrees, rig.yawDegrees);
            rig.freePitchDegrees = (std::clamp)(
                finite_or(snapshot.free_pitch_degrees, rig.pitchDegrees),
                -85.0f,
                85.0f);
            const float minDistance = rig.mode == CameraMode::FPS
                ? 0.0f
                : (rig.mode == CameraMode::Canvas2D ? 0.75f : 0.50f);
            const float maxDistance = rig.mode == CameraMode::FPS
                ? 0.0f
                : 2048.0f;
            rig.distance = (std::clamp)(
                finite_or(snapshot.distance, rig.distance),
                minDistance,
                maxDistance);
            rig.orthographicVerticalSize = (std::clamp)(
                finite_or(snapshot.orthographic_vertical_size, rig.orthographicVerticalSize),
                0.05f,
                4096.0f);
            rig.flySpeed = (std::clamp)(
                finite_or(snapshot.fly_speed, rig.flySpeed),
                0.05f,
                4096.0f);
            rig.projection = snapshot.projection;
            rig.orientation = snapshot.orientation;
            rig.viewFocuses = snapshot.view_focuses;
            rig.viewDistances = snapshot.view_distances;
            rig.viewOrthographicSizes = snapshot.view_orthographic_sizes;
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
        if (!rigKey || !snapshot.valid)
            return false;

        std::unique_lock lock(detail::g_cameraRigMutex);
        auto restored = detail::rig_from_snapshot(snapshot);
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
        auto& rig = detail::ensure_rig(rigKey);
        mode = detail::sanitize_camera_mode(mode);
        if (rig.mode == mode)
            return;

        const std::uint64_t nextRevision =
            rig.revision == (std::numeric_limits<std::uint64_t>::max)()
                ? 1
                : (rig.revision + 1);
        const std::uint64_t logicalViewId = rig.logicalViewId;
        const std::uint32_t logicalViewGeneration = rig.logicalViewGeneration;
        rig = detail::make_default_rig(mode);
        rig.logicalViewId = logicalViewId;
        rig.logicalViewGeneration = logicalViewGeneration;
        detail::save_active_view(rig);
        rig.revision = nextRevision;
    }

    export inline void reset_camera(const void* ctxKey) noexcept
    {
        const void* const rigKey = detail::normalize_camera_key(ctxKey);
        if (!rigKey)
            return;

        std::unique_lock lock(detail::g_cameraRigMutex);
        auto& rig = detail::ensure_rig(rigKey);
        const CameraMode mode = rig.mode;
        const std::uint64_t logicalViewId = rig.logicalViewId;
        const std::uint32_t logicalViewGeneration = rig.logicalViewGeneration;
        const std::uint64_t nextRevision =
            rig.revision == (std::numeric_limits<std::uint64_t>::max)()
                ? 1
                : (rig.revision + 1);
        rig = detail::make_default_rig(mode);
        rig.logicalViewId = logicalViewId;
        rig.logicalViewGeneration = logicalViewGeneration;
        detail::save_active_view(rig);
        rig.revision = nextRevision;
    }

    export inline bool focus_camera(
        const void* ctxKey,
        Vec3 focus,
        float framingRadius = 1.0f) noexcept
    {
        const void* const rigKey = detail::normalize_camera_key(ctxKey);
        if (!rigKey || !std::isfinite(focus.x) || !std::isfinite(focus.y) || !std::isfinite(focus.z)
            || !std::isfinite(framingRadius) || framingRadius < 0.0f)
        {
            return false;
        }

        std::unique_lock lock(detail::g_cameraRigMutex);
        auto& rig = detail::ensure_rig(rigKey);
        if (rig.mode == CameraMode::FPS)
        {
            rig.position = focus;
        }
        else
        {
            rig.focus = focus;
            const float minimumDistance =
                rig.mode == CameraMode::Canvas2D ? 0.75f : 0.50f;
            constexpr float maximumDistance = 2048.0f;
            const float framingDistance = (std::max)(
                minimumDistance,
                framingRadius * 3.25f);
            rig.distance = (std::clamp)(framingDistance, minimumDistance, maximumDistance);
            if (rig.projection == render_camera::ProjectionKind::orthographic)
            {
                rig.orthographicVerticalSize = (std::clamp)(
                    (std::max)(0.50f, framingRadius * 2.60f),
                    0.05f,
                    4096.0f);
            }
        }
        detail::save_active_view(rig);
        detail::touch_rig(rig);
        return true;
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

    export inline void zoom_camera(const void* ctxKey, float amount) noexcept
    {
        const void* const rigKey = detail::normalize_camera_key(ctxKey);
        if (!rigKey || amount == 0.0f)
            return;

        std::unique_lock lock(detail::g_cameraRigMutex);
        auto& rig = detail::ensure_rig(rigKey);
        if (rig.mode == CameraMode::FPS)
            return;

        const float oldDistance = rig.distance;
        const float oldOrthographicSize = rig.orthographicVerticalSize;
        const float minDistance =
            rig.mode == CameraMode::Canvas2D ? 0.75f : 0.50f;
        constexpr float maxDistance = 2048.0f;
        const float zoomFactor = std::exp(
            -(std::clamp)(amount, -12.0f, 12.0f) * 0.14f);
        if (rig.projection == render_camera::ProjectionKind::orthographic)
        {
            rig.orthographicVerticalSize = (std::clamp)(
                rig.orthographicVerticalSize * zoomFactor,
                0.05f,
                4096.0f);
        }
        else
        {
            rig.distance = (std::clamp)(
                rig.distance * zoomFactor,
                minDistance,
                maxDistance);
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
    export [[nodiscard]] inline std::uint64_t preview_geometry_revision_for(const void* ctxKey) noexcept
    {
        const void* const rigKey = detail::normalize_camera_key(ctxKey);
        if (!rigKey)
            return 0;

        const std::uint64_t cameraRevision = camera_revision_for(rigKey);
        std::shared_lock lock(detail::g_geometryRevisionMutex);
        const auto it = detail::g_geometryRevisions.find(rigKey);
        const std::uint64_t geometryRevision = it != detail::g_geometryRevisions.end() ? it->second : 0u;
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

        if (deltaRightPixels == 0.0f && deltaForwardPixels == 0.0f)
            return;

        std::unique_lock lock(detail::g_cameraRigMutex);
        auto& rig = detail::ensure_rig(rigKey);
        if (rig.mode == CameraMode::FPS)
            return;

        const Camera camera = detail::camera_from_rig(rig);
        const Vec3 forward = normalize(subtract(camera.target, camera.eye));
        const Vec3 right = normalize(cross(forward, camera.up));
        const Vec3 vertical = normalize(cross(right, forward));
        const float navigationScale = rig.projection == render_camera::ProjectionKind::orthographic
            ? rig.orthographicVerticalSize
            : rig.distance;
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
        detail::save_active_view(rig);
        detail::touch_rig(rig);
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

    export [[nodiscard]] inline Mat4 projection_for(
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

    export inline void step_camera(
        const void* ctxKey,
        float deltaTime,
        float moveForward,
        float moveRight,
        float moveUp,
        float yawInput,
        float pitchInput,
        float speedMultiplier = 1.0f) noexcept
    {
        const void* const rigKey = detail::normalize_camera_key(ctxKey);
        if (!rigKey)
            return;

        const float dt = (std::clamp)(deltaTime, 0.0f, 0.05f);
        if (dt <= 0.0f)
            return;

        std::unique_lock lock(detail::g_cameraRigMutex);
        auto& rig = detail::ensure_rig(rigKey);
        if (moveForward == 0.0f
            && moveRight == 0.0f
            && moveUp == 0.0f
            && yawInput == 0.0f
            && pitchInput == 0.0f)
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
            detail::touch_rig(rig);
            return;
        }

        const bool freeOrientation = !render_camera::axis_locked(rig.orientation);
        if (freeOrientation)
        {
            const float lookSpeed = rig.mode == CameraMode::FPS ? 105.0f : 92.0f;
            rig.yawDegrees = std::remainder(
                rig.yawDegrees + yawInput * lookSpeed * dt,
                360.0f);
            rig.pitchDegrees = (std::clamp)(
                rig.pitchDegrees + pitchInput * lookSpeed * dt,
                -85.0f,
                85.0f);
            rig.freeYawDegrees = rig.yawDegrees;
            rig.freePitchDegrees = rig.pitchDegrees;
        }

        const Vec3 worldUp{ 0.0f, 1.0f, 0.0f };
        const Camera camera = detail::camera_from_rig(rig);
        const Vec3 forward = normalize(subtract(camera.target, camera.eye));
        const Vec3 flatForward = detail::flat_forward_from_angles(rig.yawDegrees, rig.pitchDegrees);
        const Vec3 right = normalize(cross(forward, camera.up));
        const float navigationSpeed = rig.flySpeed
            * (std::clamp)(detail::finite_or(speedMultiplier, 1.0f), 0.05f, 16.0f);

        if (rig.mode == CameraMode::FPS)
        {
            rig.position = add(rig.position, scale(flatForward, moveForward * navigationSpeed * dt));
            rig.position = add(rig.position, scale(right, moveRight * navigationSpeed * dt));
            rig.position = add(rig.position, scale(worldUp, moveUp * navigationSpeed * dt));
            detail::touch_rig(rig);
            return;
        }

        rig.focus = add(rig.focus, scale(forward, moveForward * navigationSpeed * dt));
        rig.focus = add(rig.focus, scale(right, moveRight * navigationSpeed * dt));
        rig.focus = add(rig.focus, scale(worldUp, moveUp * navigationSpeed * dt));
        detail::save_active_view(rig);
        detail::touch_rig(rig);
    }

    export inline void look_camera(
        const void* ctxKey,
        float yawDeltaDegrees,
        float pitchDeltaDegrees) noexcept
    {
        const void* const rigKey = detail::normalize_camera_key(ctxKey);
        if (!rigKey)
            return;

        if (yawDeltaDegrees == 0.0f && pitchDeltaDegrees == 0.0f)
            return;

        std::unique_lock lock(detail::g_cameraRigMutex);
        auto& rig = detail::ensure_rig(rigKey);
        if (rig.mode == CameraMode::Canvas2D || render_camera::axis_locked(rig.orientation))
            return;
        rig.yawDegrees = std::remainder(
            rig.yawDegrees + yawDeltaDegrees,
            360.0f);
        rig.pitchDegrees = (std::clamp)(
            rig.pitchDegrees + pitchDeltaDegrees,
            -85.0f,
            85.0f);
        rig.freeYawDegrees = rig.yawDegrees;
        rig.freePitchDegrees = rig.pitchDegrees;
        detail::save_active_view(rig);
        detail::touch_rig(rig);
    }

    export [[nodiscard]] inline std::array<Vertex, 8> look_marker_vertices_for(const void* ctxKey) noexcept
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

    export [[nodiscard]] inline std::size_t look_marker_vertex_count_for(const void* ctxKey) noexcept
    {
        const auto vertices = look_marker_vertices_for(ctxKey);
        for (const auto& vertex : vertices)
        {
            if (vertex.position.x != 0.0f || vertex.position.y != 0.0f || vertex.position.z != 0.0f)
                return vertices.size();
        }
        return 0u;
    }

    export inline void set_lighting_frame(
        const void* ctxKey,
        epochengine::lighting::LightingFrame frame)
    {
        const void* const rigKey = detail::normalize_camera_key(ctxKey);
        if (!rigKey)
            return;

        bool changed = true;
        {
            std::unique_lock lock(detail::g_lightingFrameMutex);
            if (const auto it = detail::g_lightingFrames.find(rigKey); it != detail::g_lightingFrames.end())
            {
                changed = it->second.revision != frame.revision
                    || it->second.lights.size() != frame.lights.size()
                    || it->second.environment.ambient.r != frame.environment.ambient.r
                    || it->second.environment.ambient.g != frame.environment.ambient.g
                    || it->second.environment.ambient.b != frame.environment.ambient.b;
            }
            detail::g_lightingFrames[rigKey] = std::move(frame);
        }
        if (changed)
            detail::touch_geometry(rigKey);
    }

    export inline void clear_lighting_frame(const void* ctxKey) noexcept
    {
        const void* const rigKey = detail::normalize_camera_key(ctxKey);
        if (!rigKey)
            return;

        bool changed = false;
        {
            std::unique_lock lock(detail::g_lightingFrameMutex);
            changed = detail::g_lightingFrames.erase(rigKey) > 0u;
        }
        if (changed)
            detail::touch_geometry(rigKey);
    }

    export [[nodiscard]] inline epochengine::lighting::LightingFrame lighting_frame_for(const void* ctxKey)
    {
        const void* const rigKey = detail::normalize_camera_key(ctxKey);
        if (!rigKey)
            return {};

        std::shared_lock lock(detail::g_lightingFrameMutex);
        if (const auto it = detail::g_lightingFrames.find(rigKey); it != detail::g_lightingFrames.end())
            return it->second;
        return {};
    }
    export inline void set_object_markers(const void* ctxKey, std::span<const ObjectMarker> markers)
    {
        const void* const rigKey = detail::normalize_camera_key(ctxKey);
        if (!rigKey)
            return;

        bool changed = false;
        {
            std::unique_lock lock(detail::g_objectMarkerMutex);
            if (markers.empty())
            {
                changed = detail::g_objectMarkers.erase(rigKey) > 0u;
            }
            else
            {
                const auto it = detail::g_objectMarkers.find(rigKey);
                changed = it == detail::g_objectMarkers.end() || it->second.size() != markers.size();
                if (!changed)
                {
                    for (std::size_t index = 0; index < markers.size(); ++index)
                    {
                        if (!detail::same_marker(it->second[index], markers[index]))
                        {
                            changed = true;
                            break;
                        }
                    }
                }
                if (changed)
                    detail::g_objectMarkers[rigKey] = std::vector<ObjectMarker>{ markers.begin(), markers.end() };
            }
        }
        if (changed)
            detail::touch_geometry(rigKey);
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
        if (changed)
            detail::touch_geometry(rigKey);
    }

    export [[nodiscard]] inline std::vector<Vertex> object_marker_vertices_for(const void* ctxKey)
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
            markers = it->second;
        }

        std::vector<Vertex> out{};
        out.reserve(markers.size() * 96u);
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
            Vec3 half{
                safe_axis(marker.scale.x * 0.5f, radius * 0.45f),
                safe_axis(marker.scale.y * 0.5f, radius * 0.45f),
                safe_axis(marker.scale.z * 0.5f, radius * 0.45f)
            };

            const ObjectPreviewGeometryRoute route =
                object_preview_geometry_route(marker.primitive);
            if (route.marker_wire)
            {
                switch (marker.primitive)
                {
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

            if (marker.selected && route.selection_wire)
            {
                const Vec3 selectedHalf{
                    half.x + 0.055f,
                    half.y + 0.055f,
                    half.z + 0.055f
                };
                push_box_edges(center, selectedHalf, visual_rgb(epochengine::visuals::object_selected_outline()));
            }
        }

        return out;
    }

    export [[nodiscard]] inline std::vector<Vertex> object_solid_vertices_for(const void* ctxKey)
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
            markers = it->second;
        }

        const epochengine::lighting::LightingFrame lightingFrame = lighting_frame_for(rigKey);
        const Camera camera = camera_for(rigKey);

        std::vector<Vertex> out{};
        out.reserve(markers.size() * 96u);
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
        const auto shade_surface = [&](Vec3 position, Vec3 normal, Vec3 albedo) noexcept
        {
            if (lightingFrame.revision == 0u && lightingFrame.lights.empty())
            {
                const float facing = 0.70f + (std::max)(0.0f, normal.y) * 0.46f;
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
                    .specularColor{ 0.18f, 0.18f, 0.18f },
                    .shininess = 24.0f
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
            push_face(c100, c110, c010, c000, { 1.0f, 0.0f, 0.0f }, color);
            push_face(c000, c010, c011, c001, { -1.0f, 0.0f, 0.0f }, color);
            push_face(c101, c111, c110, c100, { 0.0f, 0.0f, -1.0f }, color);
        };
        auto push_leaf_cluster = [&](Vec3 center, Vec3 half, Vec3 color)
        {
            const Vec3 mainHalf{
                (std::max)(0.04f, half.x * 0.72f),
                (std::max)(0.02f, half.y * 0.38f),
                (std::max)(0.04f, half.z * 0.72f)
            };
            push_box(center, mainHalf, color);
            push_box({ center.x + mainHalf.x * 0.68f, center.y + mainHalf.y * 0.58f, center.z - mainHalf.z * 0.18f }, scale(mainHalf, 0.58f), lit(color, 1.04f));
            push_box({ center.x - mainHalf.x * 0.62f, center.y + mainHalf.y * 0.38f, center.z + mainHalf.z * 0.22f }, scale(mainHalf, 0.54f), lit(color, 0.94f));
            push_box({ center.x, center.y + mainHalf.y * 0.74f, center.z + mainHalf.z * 0.62f }, scale(mainHalf, 0.45f), lit(color, 1.10f));
        };

        for (const auto& marker : markers)
        {
            if (!object_marker_uses_solid_fill(marker))
                continue;

            const float radius = (std::clamp)(marker.radius, 0.16f, 1.75f);
            Vec3 color = marker.selected ? visual_rgb(epochengine::visuals::object_selected()) : marker.color;
            if (marker.editorOnly && !marker.selected)
                color = scale(color, epochengine::visuals::editor_solid_opacity_factor());

            const Vec3 center{ marker.position.x, marker.position.y, marker.position.z };
            Vec3 half{
                safe_axis(marker.scale.x * 0.5f, radius * 0.45f),
                safe_axis(marker.scale.y * 0.5f, radius * 0.45f),
                safe_axis(marker.scale.z * 0.5f, radius * 0.45f)
            };

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
                push_box(center, half, color);
                break;
            case ObjectPreviewPrimitive::Canvas2D:
                half.z = (std::max)(0.025f, radius * 0.04f);
                push_box(center, half, color);
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
                push_box({ center.x, center.y - baseHalf.y * 0.38f, center.z }, baseHalf, color);
                push_box({ center.x, center.y + upperHalf.y * 0.52f, center.z }, upperHalf, lit(color, 1.04f));
                break;
            }
            case ObjectPreviewPrimitive::ForestBranch:
            {
                const Vec3 jointHalf{
                    (std::max)(0.025f, radius * 0.18f),
                    (std::max)(0.025f, radius * 0.18f),
                    (std::max)(0.025f, radius * 0.18f)
                };
                push_box(center, jointHalf, color);
                break;
            }
            case ObjectPreviewPrimitive::ForestLeafCluster:
                push_leaf_cluster(center, half, color);
                break;
            case ObjectPreviewPrimitive::Cube:
            default:
                push_box(center, half, color);
                break;
            }
        }

        return out;
    }

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
            markers = it->second;
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
