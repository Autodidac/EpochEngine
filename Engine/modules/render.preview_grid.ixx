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
#include <vector>

export module render.preview_grid;

namespace epochnamespace::previewgrid
{
    export struct Vec3
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
    };

    export struct Vertex
    {
        Vec3 position{};
        Vec3 color{};
    };

    export using Mat4 = std::array<float, 16>;

    export struct Camera
    {
        Vec3 eye{ 9.0f, 7.0f, 9.0f };
        Vec3 target{ 0.0f, 0.0f, 0.0f };
        Vec3 up{ 0.0f, 1.0f, 0.0f };
        float fovRadians = 0.90f;
        float nearPlane = 0.1f;
        float farPlane = 64.0f;
    };

    export inline constexpr std::array<float, 4> kClearColor{
        0.31f, 0.36f, 0.42f, 1.0f
    };

    export inline constexpr Camera kCamera{};

    export enum class CameraMode : std::uint8_t
    {
        Editor = 0,
        FPS = 1
    };

    export [[nodiscard]] inline std::string_view camera_mode_name(CameraMode mode) noexcept
    {
        switch (mode)
        {
        case CameraMode::FPS: return "FPS";
        case CameraMode::Editor:
        default: return "Editor";
        }
    }

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
            Vec3 focus{ 0.0f, 0.75f, 0.0f };
            Vec3 position{ 0.0f, 1.8f, 6.0f };
            float yawDegrees = -135.0f;
            float pitchDegrees = -28.0f;
            float distance = 13.5f;
            std::uint64_t revision = 1;
        };

        export inline std::unordered_map<const void*, CameraRigState, PtrHash> g_cameraRigs{};
        inline std::shared_mutex g_cameraRigMutex{};

        [[nodiscard]] inline CameraRigState make_editor_rig() noexcept
        {
            return CameraRigState{
                .mode = CameraMode::Editor,
                .focus{ 0.0f, 0.75f, 0.0f },
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

        [[nodiscard]] inline CameraRigState make_default_rig(CameraMode mode) noexcept
        {
            return mode == CameraMode::FPS ? make_fps_rig() : make_editor_rig();
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
            auto [it, inserted] = g_cameraRigs.try_emplace(ctxKey, make_default_rig(CameraMode::Editor));
            return it->second;
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

                auto push_box = [&](Vec3 center, Vec3 halfExtent, Vec3 color)
                {
                    const std::array<Vec3, 8> corners{{
                        { center.x - halfExtent.x, center.y - halfExtent.y, center.z - halfExtent.z },
                        { center.x + halfExtent.x, center.y - halfExtent.y, center.z - halfExtent.z },
                        { center.x + halfExtent.x, center.y - halfExtent.y, center.z + halfExtent.z },
                        { center.x - halfExtent.x, center.y - halfExtent.y, center.z + halfExtent.z },
                        { center.x - halfExtent.x, center.y + halfExtent.y, center.z - halfExtent.z },
                        { center.x + halfExtent.x, center.y + halfExtent.y, center.z - halfExtent.z },
                        { center.x + halfExtent.x, center.y + halfExtent.y, center.z + halfExtent.z },
                        { center.x - halfExtent.x, center.y + halfExtent.y, center.z + halfExtent.z }
                    }};

                    constexpr std::array<std::array<int, 2>, 12> edges{{
                        { 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 },
                        { 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 },
                        { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 }
                    }};

                    for (const auto& edge : edges)
                    {
                        const auto& a = corners[static_cast<std::size_t>(edge[0])];
                        const auto& b = corners[static_cast<std::size_t>(edge[1])];
                        push_line(
                            a.x, a.y, a.z,
                            b.x, b.y, b.z,
                            color.x, color.y, color.z);
                    }
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

                push_box({ 0.0f, 0.55f, 0.0f }, { 0.55f, 0.55f, 0.55f }, { 0.88f, 0.90f, 0.95f });
                push_box({ 3.0f, 0.45f, -1.6f }, { 1.25f, 0.45f, 1.0f }, { 0.76f, 0.82f, 0.95f });
                push_box({ -2.6f, 1.05f, 2.1f }, { 0.55f, 1.05f, 0.55f }, { 0.95f, 0.80f, 0.58f });
                return out;
            }();

            return geometry;
        }
    }

    export [[nodiscard]] inline CameraMode camera_mode_for(const void* ctxKey) noexcept
    {
        if (!ctxKey)
            return CameraMode::Editor;

        std::shared_lock lock(detail::g_cameraRigMutex);
        const auto it = detail::g_cameraRigs.find(ctxKey);
        return it != detail::g_cameraRigs.end() ? it->second.mode : CameraMode::Editor;
    }

    export inline void set_camera_mode(const void* ctxKey, CameraMode mode) noexcept
    {
        if (!ctxKey)
            return;

        std::unique_lock lock(detail::g_cameraRigMutex);
        auto& rig = detail::ensure_rig(ctxKey);
        const std::uint64_t nextRevision =
            rig.revision == (std::numeric_limits<std::uint64_t>::max)() ? 1 : (rig.revision + 1);
        rig = detail::make_default_rig(mode);
        rig.revision = nextRevision;
    }

    export inline void reset_camera(const void* ctxKey) noexcept
    {
        if (!ctxKey)
            return;

        std::unique_lock lock(detail::g_cameraRigMutex);
        auto& rig = detail::ensure_rig(ctxKey);
        rig = detail::make_default_rig(rig.mode);
        detail::touch_rig(rig);
    }

    export [[nodiscard]] inline float camera_distance_for(const void* ctxKey) noexcept
    {
        if (!ctxKey)
            return detail::make_default_rig(CameraMode::Editor).distance;

        std::shared_lock lock(detail::g_cameraRigMutex);
        const auto it = detail::g_cameraRigs.find(ctxKey);
        if (it == detail::g_cameraRigs.end())
            return detail::make_default_rig(CameraMode::Editor).distance;
        return it->second.mode == CameraMode::FPS ? 0.0f : it->second.distance;
    }

    export inline void zoom_camera(const void* ctxKey, float amount) noexcept
    {
        if (!ctxKey || amount == 0.0f)
            return;

        std::unique_lock lock(detail::g_cameraRigMutex);
        auto& rig = detail::ensure_rig(ctxKey);
        if (rig.mode == CameraMode::FPS)
            return;

        const float oldDistance = rig.distance;
        rig.distance = (std::clamp)(rig.distance - amount, 2.5f, 48.0f);
        if (rig.distance != oldDistance)
            detail::touch_rig(rig);
    }

    export [[nodiscard]] inline std::uint64_t camera_revision_for(const void* ctxKey) noexcept
    {
        if (!ctxKey)
            return 0;

        std::shared_lock lock(detail::g_cameraRigMutex);
        const auto it = detail::g_cameraRigs.find(ctxKey);
        return it != detail::g_cameraRigs.end() ? it->second.revision : 0;
    }

    export inline void pan_camera_drag(
        const void* ctxKey,
        float deltaRightPixels,
        float deltaForwardPixels) noexcept
    {
        if (!ctxKey)
            return;

        if (deltaRightPixels == 0.0f && deltaForwardPixels == 0.0f)
            return;

        std::unique_lock lock(detail::g_cameraRigMutex);
        auto& rig = detail::ensure_rig(ctxKey);
        if (rig.mode == CameraMode::FPS)
            return;

        const Vec3 right = detail::right_from_angles(rig.yawDegrees, rig.pitchDegrees);
        const Vec3 forward = detail::flat_forward_from_angles(rig.yawDegrees, rig.pitchDegrees);
        const float dragScale = (std::max)(0.010f, rig.distance * 0.0125f);
        rig.focus = add(rig.focus, scale(right, deltaRightPixels * dragScale));
        rig.focus = add(rig.focus, scale(forward, deltaForwardPixels * dragScale));
        detail::touch_rig(rig);
    }

    export inline void cleanup_context(const void* ctxKey) noexcept
    {
        if (!ctxKey)
            return;

        std::unique_lock lock(detail::g_cameraRigMutex);
        detail::g_cameraRigs.erase(ctxKey);
    }

    export [[nodiscard]] inline Camera camera_for(const void* ctxKey) noexcept
    {
        if (!ctxKey)
            return kCamera;

        std::shared_lock lock(detail::g_cameraRigMutex);
        const auto it = detail::g_cameraRigs.find(ctxKey);
        if (it == detail::g_cameraRigs.end())
            return detail::camera_from_rig(detail::make_default_rig(CameraMode::Editor));
        return detail::camera_from_rig(it->second);
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
        if (!ctxKey)
            return;

        const float dt = (std::clamp)(deltaTime, 0.0f, 0.05f);
        if (dt <= 0.0f)
            return;

        std::unique_lock lock(detail::g_cameraRigMutex);
        auto& rig = detail::ensure_rig(ctxKey);

        if (moveForward == 0.0f
            && moveRight == 0.0f
            && moveUp == 0.0f
            && yawInput == 0.0f
            && pitchInput == 0.0f)
        {
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
        if (!ctxKey)
            return;

        if (yawDeltaDegrees == 0.0f && pitchDeltaDegrees == 0.0f)
            return;

        std::unique_lock lock(detail::g_cameraRigMutex);
        auto& rig = detail::ensure_rig(ctxKey);
        rig.yawDegrees += yawDeltaDegrees;
        rig.pitchDegrees = (std::clamp)(rig.pitchDegrees + pitchDeltaDegrees, -80.0f, 80.0f);
        detail::touch_rig(rig);
    }

    export [[nodiscard]] inline std::array<Vertex, 8> look_marker_vertices_for(const void* ctxKey) noexcept
    {
        std::array<Vertex, 8> out{};
        if (!ctxKey)
            return out;

        const auto camera = camera_for(ctxKey);
        const Vec3 ray = normalize(subtract(camera.target, camera.eye));
        if (!std::isfinite(ray.x) || !std::isfinite(ray.y) || !std::isfinite(ray.z))
            return out;

        if (std::abs(ray.y) <= 1.0e-4f)
            return out;

        const float hitDistance = (0.0f - camera.eye.y) / ray.y;
        if (!(hitDistance > 0.0f) || !std::isfinite(hitDistance))
            return out;

        const Vec3 hit = add(camera.eye, scale(ray, hitDistance));
        const float markerSize = (std::max)(0.18f, camera_distance_for(ctxKey) * 0.035f);
        const float markerHeight = 0.035f;

        const auto make_vertex = [](Vec3 position, Vec3 color) noexcept
        {
            return Vertex{ .position = position, .color = color };
        };

        const Vec3 rayColor{ 0.88f, 0.78f, 0.32f };
        const Vec3 markerColor{ 0.99f, 0.89f, 0.34f };

        out[0] = make_vertex(camera.eye, rayColor);
        out[1] = make_vertex({ hit.x, markerHeight, hit.z }, rayColor);
        out[2] = make_vertex({ hit.x - markerSize, markerHeight, hit.z }, markerColor);
        out[3] = make_vertex({ hit.x + markerSize, markerHeight, hit.z }, markerColor);
        out[4] = make_vertex({ hit.x, markerHeight, hit.z - markerSize }, markerColor);
        out[5] = make_vertex({ hit.x, markerHeight, hit.z + markerSize }, markerColor);
        out[6] = make_vertex({ hit.x, markerHeight, hit.z }, markerColor);
        out[7] = make_vertex({ hit.x, markerHeight + markerSize * 0.75f, hit.z }, markerColor);
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
