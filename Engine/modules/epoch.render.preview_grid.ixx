module;

export module epoch.render.preview_grid;

import <algorithm>;
import <array>;
import <cmath>;
import <cstddef>;
import <cstdint>;
import <span>;
import <vector>;

export namespace epochnamespace::previewgrid
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
        0.06f, 0.08f, 0.11f, 1.0f
    };

    export inline constexpr Camera kCamera{};

    export [[nodiscard]] inline Vec3 subtract(Vec3 lhs, Vec3 rhs) noexcept
    {
        return { lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z };
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
