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
#include <mutex>
#include <shared_mutex>
#include <span>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

export module render.preview_grid;

import engine.visuals;
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
        ForestLeafCluster
    };

    export struct Camera
    {
        Vec3 eye{ 9.0f, 7.0f, 9.0f };
        Vec3 target{ 0.0f, 0.0f, 0.0f };
        Vec3 up{ 0.0f, 1.0f, 0.0f };
        float fovRadians = 0.90f;
        float nearPlane = 0.1f;
        float farPlane = 64.0f;
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

    export inline const std::array<float, 4> kClearColor = epochengine::visuals::scene_background();

    export inline constexpr Camera kCamera{};

    export enum class CameraMode : std::uint8_t
    {
        Editor = 0,
        FPS = 1,
        Canvas2D = 2
    };

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

    export struct CameraRigSnapshot
    {
        bool valid{ false };
        CameraMode mode{ CameraMode::Editor };
        Vec3 focus{ 0.0f, 0.0f, 0.0f };
        Vec3 position{ 0.0f, 1.8f, 6.0f };
        float yaw_degrees = -135.0f;
        float pitch_degrees = -28.0f;
        float distance = 13.5f;
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
            float distance = 13.5f;
            std::uint64_t revision = 1;
        };

        export inline std::unordered_map<const void*, CameraRigState, PtrHash> g_cameraRigs{};
        export inline std::unordered_map<const void*, Vec3, PtrHash> g_lastMarkerHits{};
        export inline std::unordered_map<const void*, std::vector<ObjectMarker>, PtrHash> g_objectMarkers{};
        export inline std::unordered_map<const void*, epochengine::lighting::LightingFrame, PtrHash> g_lightingFrames{};
        export inline std::unordered_map<const void*, std::uint64_t, PtrHash> g_geometryRevisions{};
        inline std::shared_mutex g_cameraRigMutex{};
        inline std::shared_mutex g_objectMarkerMutex{};
        inline std::shared_mutex g_lightingFrameMutex{};
        inline std::shared_mutex g_geometryRevisionMutex{};

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
                .distance = 13.5f
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
                .distance = 0.0f
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
                .distance = 8.0f
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
            const Vec3 forward = forward_from_angles(rig.yawDegrees, rig.pitchDegrees);

            if (rig.mode == CameraMode::FPS)
            {
                const Vec3 target = add(rig.position, forward);
                return Camera{
                    .eye = rig.position,
                    .target = target,
                    .up = worldUp,
                    .fovRadians = 1.05f,
                    .nearPlane = 0.1f,
                    .farPlane = 96.0f
                };
            }

            if (rig.mode == CameraMode::Canvas2D)
            {
                return Camera{
                    .eye = { rig.focus.x, rig.focus.y, rig.focus.z + rig.distance },
                    .target = rig.focus,
                    .up = { 0.0f, 1.0f, 0.0f },
                    .fovRadians = 0.48f,
                    .nearPlane = 0.1f,
                    .farPlane = 128.0f
                };
            }

            const Vec3 eye = subtract(rig.focus, scale(forward, rig.distance));
            return Camera{
                .eye = eye,
                .target = rig.focus,
                .up = worldUp,
                .fovRadians = 0.90f,
                .nearPlane = 0.1f,
                .farPlane = 96.0f
            };
        }

        [[nodiscard]] inline CameraRigState& ensure_rig(const void* ctxKey)
        {
            const void* const rigKey = normalize_camera_key(ctxKey);
            auto [it, inserted] = g_cameraRigs.try_emplace(rigKey, make_default_rig(CameraMode::Editor));
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
                .distance = rig.distance,
                .revision = rig.revision
            };
        }

        [[nodiscard]] inline CameraRigState rig_from_snapshot(const CameraRigSnapshot& snapshot) noexcept
        {
            CameraRigState rig = make_default_rig(sanitize_camera_mode(snapshot.mode));
            rig.focus = finite_vec_or(snapshot.focus, rig.focus);
            rig.position = finite_vec_or(snapshot.position, rig.position);
            rig.yawDegrees = finite_or(snapshot.yaw_degrees, rig.yawDegrees);
            rig.pitchDegrees = (std::clamp)(finite_or(snapshot.pitch_degrees, rig.pitchDegrees), -80.0f, 80.0f);
            const float minDistance = rig.mode == CameraMode::Canvas2D ? 6.0f : 0.0f;
            const float maxDistance = rig.mode == CameraMode::Canvas2D ? 64.0f : 96.0f;
            rig.distance = (std::clamp)(finite_or(snapshot.distance, rig.distance), minDistance, maxDistance);
            rig.revision = snapshot.revision == 0 ? 1 : snapshot.revision;
            return rig;
        }

        struct Geometry
        {
            std::vector<Vertex> vertices{};
            std::vector<std::uint32_t> indices{};
        };

        [[nodiscard]] inline const Geometry& shared_geometry() noexcept
        {
            static const Geometry geometry = []()
            {
                Geometry out{};
                out.vertices.reserve(128);
                out.indices.reserve(128);

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

                constexpr int kHalfExtent = 12;
                for (int line = -kHalfExtent; line <= kHalfExtent; ++line)
                {
                    const bool center = line == 0;
                    const bool major = center || (line % 4 == 0);
                    const float tone = center ? 0.44f : (major ? 0.26f : 0.15f);

                    push_line(
                        static_cast<float>(line), 0.0f, static_cast<float>(-kHalfExtent),
                        static_cast<float>(line), 0.0f, static_cast<float>(kHalfExtent),
                        tone, tone, tone + 0.03f);
                    push_line(
                        static_cast<float>(-kHalfExtent), 0.0f, static_cast<float>(line),
                        static_cast<float>(kHalfExtent), 0.0f, static_cast<float>(line),
                        tone, tone, tone + 0.03f);
                }

                push_line(0.0f, 0.02f, 0.0f, 3.5f, 0.02f, 0.0f, 0.95f, 0.30f, 0.28f);
                push_line(0.0f, 0.02f, 0.0f, 0.0f, 3.5f, 0.0f, 0.28f, 0.92f, 0.40f);
                push_line(0.0f, 0.02f, 0.0f, 0.0f, 0.02f, 3.5f, 0.33f, 0.58f, 0.98f);
                return out;
            }();

            return geometry;
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
        detail::g_cameraRigs[rigKey] = detail::rig_from_snapshot(snapshot);
        return true;
    }

    export inline void set_camera_mode(const void* ctxKey, CameraMode mode) noexcept
    {
        const void* const rigKey = detail::normalize_camera_key(ctxKey);
        if (!rigKey)
            return;

        std::unique_lock lock(detail::g_cameraRigMutex);
        auto& rig = detail::ensure_rig(rigKey);
        const std::uint64_t nextRevision =
            rig.revision == (std::numeric_limits<std::uint64_t>::max)() ? 1 : (rig.revision + 1);
        rig = detail::make_default_rig(mode);
        rig.revision = nextRevision;
    }

    export inline void reset_camera(const void* ctxKey) noexcept
    {
        const void* const rigKey = detail::normalize_camera_key(ctxKey);
        if (!rigKey)
            return;

        std::unique_lock lock(detail::g_cameraRigMutex);
        auto& rig = detail::ensure_rig(rigKey);
        rig = detail::make_default_rig(rig.mode);
        detail::touch_rig(rig);
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
            const float minimumDistance = rig.mode == CameraMode::Canvas2D ? 6.0f : 2.5f;
            const float maximumDistance = rig.mode == CameraMode::Canvas2D ? 64.0f : 48.0f;
            const float framingDistance = (std::max)(minimumDistance, framingRadius * 3.25f);
            rig.distance = (std::clamp)(framingDistance, minimumDistance, maximumDistance);
        }
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
        return it->second.mode == CameraMode::FPS ? 0.0f : it->second.distance;
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
        const float minDistance = rig.mode == CameraMode::Canvas2D ? 6.0f : 2.5f;
        const float maxDistance = rig.mode == CameraMode::Canvas2D ? 64.0f : 48.0f;
        rig.distance = (std::clamp)(rig.distance - amount, minDistance, maxDistance);
        if (rig.distance != oldDistance)
            detail::touch_rig(rig);
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

        const Vec3 right = rig.mode == CameraMode::Canvas2D
            ? Vec3{ 1.0f, 0.0f, 0.0f }
            : detail::right_from_angles(rig.yawDegrees, rig.pitchDegrees);
        const Vec3 forward = rig.mode == CameraMode::Canvas2D
            ? Vec3{ 0.0f, 1.0f, 0.0f }
            : detail::flat_forward_from_angles(rig.yawDegrees, rig.pitchDegrees);
        const float dragScale = (std::max)(0.010f, rig.distance * 0.0125f);
        rig.focus = add(rig.focus, scale(right, deltaRightPixels * dragScale));
        rig.focus = add(rig.focus, scale(forward, deltaForwardPixels * dragScale));
        detail::touch_rig(rig);
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

    export [[nodiscard]] inline Mat4 projection_for(
        const void* ctxKey,
        float aspect,
        const Camera& camera) noexcept
    {
        const float safeAspect = (std::max)(0.001f, aspect);
        if (camera_mode_for(ctxKey) == CameraMode::Canvas2D)
        {
            const auto delta = subtract(camera.eye, camera.target);
            const float distance = std::sqrt(dot(delta, delta));
            const float halfHeight = (std::max)(2.0f, distance * 0.42f);
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
        float pitchInput) noexcept
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
            constexpr float kPanSpeed = 5.0f;
            constexpr float kDollySpeed = 8.5f;
            rig.focus.x += moveRight * kPanSpeed * dt;
            rig.focus.y += moveUp * kPanSpeed * dt;
            rig.distance = (std::clamp)(rig.distance - moveForward * kDollySpeed * dt, 6.0f, 64.0f);
            detail::touch_rig(rig);
            return;
        }

        const float lookSpeed = rig.mode == CameraMode::FPS ? 105.0f : 92.0f;
        rig.yawDegrees += yawInput * lookSpeed * dt;
        rig.pitchDegrees = (std::clamp)(rig.pitchDegrees + pitchInput * lookSpeed * dt, -80.0f, 80.0f);

        const Vec3 worldUp{ 0.0f, 1.0f, 0.0f };
        const Vec3 forward = detail::forward_from_angles(rig.yawDegrees, rig.pitchDegrees);
        const Vec3 flatForward = detail::flat_forward_from_angles(rig.yawDegrees, rig.pitchDegrees);
        const Vec3 right = normalize(cross(flatForward, worldUp));

        if (rig.mode == CameraMode::FPS)
        {
            constexpr float kMoveSpeed = 6.5f;
            constexpr float kVerticalSpeed = 4.5f;
            rig.position = add(rig.position, scale(flatForward, moveForward * kMoveSpeed * dt));
            rig.position = add(rig.position, scale(right, moveRight * kMoveSpeed * dt));
            rig.position = add(rig.position, scale(worldUp, moveUp * kVerticalSpeed * dt));
            detail::touch_rig(rig);
            return;
        }

        constexpr float kPanSpeed = 5.0f;
        constexpr float kDollySpeed = 8.5f;
        rig.focus = add(rig.focus, scale(right, moveRight * kPanSpeed * dt));
        rig.focus = add(rig.focus, scale(worldUp, moveUp * kPanSpeed * dt));
        rig.distance = (std::clamp)(rig.distance - moveForward * kDollySpeed * dt, 2.5f, 48.0f);
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
        if (rig.mode == CameraMode::Canvas2D)
            return;
        rig.yawDegrees += yawDeltaDegrees;
        rig.pitchDegrees = (std::clamp)(rig.pitchDegrees + pitchDeltaDegrees, -80.0f, 80.0f);
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
        out.reserve(markers.size() * 32u);
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

            const Vec3 center{ marker.position.x, (std::max)(0.035f, marker.position.y), marker.position.z };
            Vec3 half{
                safe_axis(marker.scale.x * 0.5f, radius * 0.45f),
                safe_axis(marker.scale.y * 0.5f, radius * 0.45f),
                safe_axis(marker.scale.z * 0.5f, radius * 0.45f)
            };

            switch (marker.primitive)
            {
            case ObjectPreviewPrimitive::Light:
                half = { radius * 0.32f, radius * 0.32f, radius * 0.32f };
                push_box_edges(center, half, color);
                push_line({ center.x - radius, center.y, center.z }, { center.x + radius, center.y, center.z }, lit(color, 1.2f));
                push_line({ center.x, center.y - radius, center.z }, { center.x, center.y + radius, center.z }, lit(color, 1.2f));
                push_line({ center.x, center.y, center.z - radius }, { center.x, center.y, center.z + radius }, lit(color, 1.2f));
                break;
            case ObjectPreviewPrimitive::Spawn:
                half.y = (std::max)(0.08f, radius * 0.12f);
                push_box_edges(center, half, color);
                push_line(center, { center.x, center.y + radius * 1.4f, center.z }, lit(color, 1.05f));
                break;
            case ObjectPreviewPrimitive::Camera:
                half = { radius * 0.56f, radius * 0.34f, radius * 0.42f };
                push_box_edges(center, half, color);
                push_line(
                    { center.x - half.x, center.y, center.z - half.z },
                    { center.x - half.x - radius * 0.52f, center.y, center.z - half.z - radius * 0.52f },
                    lit(color, 1.0f));
                push_line(
                    { center.x + half.x, center.y, center.z - half.z },
                    { center.x + half.x + radius * 0.52f, center.y, center.z - half.z - radius * 0.52f },
                    lit(color, 1.0f));
                break;
            case ObjectPreviewPrimitive::Level:
                half.y = (std::max)(0.05f, radius * 0.08f);
                push_box_edges(center, half, color);
                break;
            case ObjectPreviewPrimitive::Canvas2D:
            case ObjectPreviewPrimitive::EngineArcadeScreen:
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

            if (marker.selected)
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
        out.reserve(markers.size() * 36u);
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
            const float radius = (std::clamp)(marker.radius, 0.16f, 1.75f);
            Vec3 color = marker.selected ? visual_rgb(epochengine::visuals::object_selected()) : marker.color;
            if (marker.editorOnly && !marker.selected)
                color = scale(color, epochengine::visuals::editor_solid_opacity_factor());

            const Vec3 center{ marker.position.x, (std::max)(0.035f, marker.position.y), marker.position.z };
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
            case ObjectPreviewPrimitive::EngineArcadeScreen:
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

        std::erase_if(
            markers,
            [](const ObjectMarker& marker) noexcept
            {
                return !marker.sampledRenderSurface;
            });
        return markers;
    }

    export [[nodiscard]] inline std::span<const Vertex> grid_vertices() noexcept
    {
        const auto& geometry = detail::shared_geometry();
        return { geometry.vertices.data(), geometry.vertices.size() };
    }

    export [[nodiscard]] inline std::span<const std::uint32_t> grid_indices() noexcept
    {
        const auto& geometry = detail::shared_geometry();
        return { geometry.indices.data(), geometry.indices.size() };
    }
}
