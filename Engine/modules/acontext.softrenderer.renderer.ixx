/**************************************************************
 *   █████╗ ██╗     ███╗   ███╗   ███╗   ██╗    ██╗██████╗    *
 *  ██╔══██╗██║     ████╗ ████║ ██╔═══██╗████╗  ██║██╔══██╗   *
 *  ███████║██║     ██╔████╔██║ ██║   ██║██╔██╗ ██║██║  ██║   *
 *  ██╔══██║██║     ██║╚██╔╝██║ ██║   ██║██║╚██╗██║██║  ██║   *
 *  ██║  ██║███████╗██║ ╚═╝ ██║ ╚██████╔╝██║ ╚████║██████╔╝   *
 *  ╚═╝  ╚═╝╚══════╝╚═╝     ╚═╝  ╚═════╝ ╚═╝  ╚═══╝╚═════╝    *
 *                                                            *
 *   This file is part of the Almond Project.                 *
 *   AlmondEngine - Modular C++ Game Engine                   *
 *                                                            *
 *   SPDX-License-Identifier: LicenseRef-MIT-NoSell           *
 *                                                            *
 *   Provided "AS IS", without warranty of any kind.          *
 *   Use permitted for non-commercial purposes only           *
 *   without prior commercial licensing agreement.            *
 *                                                            *
 *   Redistribution allowed with this notice.                 *
 *   No obligation to disclose modifications.                 *
 *   See LICENSE file for full terms.                         *
 **************************************************************/
 //
 // acontext.softrenderer.renderer.ixx
 // SoftRenderer - Math + Cube Renderer (module version)
 //

module;

//#include "aplatform.hpp"
#include <include/aengine.config.hpp> // for ALMOND_USING Macros

export module acontext.softrenderer.renderer;

#if defined(ALMOND_USING_SOFTWARE_RENDERER)

import aengine.platform;

import <algorithm>;
import <cstdint>;
import <cmath>;
import <limits>;
import <vector>;

// Provides TexturePtr / Texture (with sample(), width/height).
// If your textures unit is named differently, change this import to match.
import acontext.softrenderer.textures;

export namespace almondnamespace::anativecontext
{
    struct Vec3 { float x = 0.0f, y = 0.0f, z = 0.0f; };
    struct Vec2 { float u = 0.0f, v = 0.0f; };
    struct Mat4 { float m[4][4] = {}; };
    export struct Vertex { Vec3 pos; Vec2 uv; };

    struct Triangle
    {
        Vertex v0{};
        Vertex v1{};
        Vertex v2{};
        TexturePtr tex{};
        std::uint32_t color = 0xFFFFFFFFu;
    };

    export struct Camera
    {
        Vec3 pos{ 0.0f, 0.0f, -3.0f };
        float pitch = 0.0f;
        float yaw = 0.0f;
        float roll = 0.0f;
    };

    struct Framebuffer
    {
        int width = 0;
        int height = 0;
        std::vector<std::uint32_t> pixels;

        Framebuffer() = default;
        Framebuffer(int w, int h) : width(w), height(h), pixels(std::size_t(w)* std::size_t(h), 0xFF101010u) {}

        inline void clear(std::uint32_t color)
        {
            std::fill(pixels.begin(), pixels.end(), color);
        }

        inline void put_pixel(int x, int y, std::uint32_t color)
        {
            if (x < 0 || y < 0 || x >= width || y >= height) return;
            pixels[std::size_t(y) * std::size_t(width) + std::size_t(x)] = color;
        }
    };

    class SoftwareRenderer
    {
    public:
        // =======================
        // Math
        // =======================
        static Mat4 rotationX(float a)
        {
            Mat4 m{};
            m.m[0][0] = 1.0f;
            m.m[3][3] = 1.0f;

            const float c = std::cos(a);
            const float s = std::sin(a);

            m.m[1][1] = c;  m.m[1][2] = -s;
            m.m[2][1] = s;  m.m[2][2] = c;
            return m;
        }

        static Mat4 rotationY(float a)
        {
            Mat4 m{};
            m.m[1][1] = 1.0f;
            m.m[3][3] = 1.0f;

            const float c = std::cos(a);
            const float s = std::sin(a);

            m.m[0][0] = c;  m.m[0][2] = s;
            m.m[2][0] = -s; m.m[2][2] = c;
            return m;
        }

        static Mat4 rotationZ(float a)
        {
            Mat4 m{};
            m.m[2][2] = 1.0f;
            m.m[3][3] = 1.0f;

            const float c = std::cos(a);
            const float s = std::sin(a);

            m.m[0][0] = c;  m.m[0][1] = -s;
            m.m[1][0] = s;  m.m[1][1] = c;
            return m;
        }

        static Mat4 mul(const Mat4& A, const Mat4& B)
        {
            Mat4 R{};
            for (int r = 0; r < 4; ++r)
                for (int c = 0; c < 4; ++c)
                    for (int k = 0; k < 4; ++k)
                        R.m[r][c] += A.m[r][k] * B.m[k][c];
            return R;
        }

        static Vec3 multiply(const Mat4& m, const Vec3& v)
        {
            return {
                m.m[0][0] * v.x + m.m[0][1] * v.y + m.m[0][2] * v.z + m.m[0][3],
                m.m[1][0] * v.x + m.m[1][1] * v.y + m.m[1][2] * v.z + m.m[1][3],
                m.m[2][0] * v.x + m.m[2][1] * v.y + m.m[2][2] * v.z + m.m[2][3]
            };
        }

        static Mat4 transpose(const Mat4& a)
        {
            Mat4 r{};
            for (int i = 0; i < 4; ++i)
                for (int j = 0; j < 4; ++j)
                    r.m[i][j] = a.m[j][i];
            return r;
        }

        // =======================
        // Cube data
        // =======================
        static inline const Vertex cubeVerts[8] = {
            {{-1.0f,-1.0f,-1.0f},{0.0f,0.0f}}, {{ 1.0f,-1.0f,-1.0f},{1.0f,0.0f}},
            {{ 1.0f, 1.0f,-1.0f},{1.0f,1.0f}}, {{-1.0f, 1.0f,-1.0f},{0.0f,1.0f}},
            {{-1.0f,-1.0f, 1.0f},{0.0f,0.0f}}, {{ 1.0f,-1.0f, 1.0f},{1.0f,0.0f}},
            {{ 1.0f, 1.0f, 1.0f},{1.0f,1.0f}}, {{-1.0f, 1.0f, 1.0f},{0.0f,1.0f}}
        };

        static inline const int cubeTris[12][3] = {
            {0,1,2},{0,2,3},{5,4,7},{5,7,6},{4,0,3},{4,3,7},
            {1,5,6},{1,6,2},{3,2,6},{3,6,7},{4,5,1},{4,1,0}
        };

        static inline const std::uint32_t faceColors[6] = {
            0xFF0000FFu, 0xFF00FF00u, 0xFFFF0000u, 0xFFFFFF00u, 0xFFFF00FFu, 0xFF00FFFFu
        };

        // =======================
        // Rasterization
        // =======================
        static void rasterize_triangle(Framebuffer& fb, const Triangle& tri, std::vector<float>& zbuf)
        {
            auto project = [&](const Vec3& v) -> Vec3
                {
                    constexpr float scale = 200.0f;
                    float z = v.z + 3.0f;
                    if (z < 0.001f) z = 0.001f;
                    const float f = scale / z;
                    return { v.x * f + fb.width * 0.5f, -v.y * f + fb.height * 0.5f, z };
                };

            const Vec3 v0 = tri.v0.pos;
            const Vec3 v1 = tri.v1.pos;
            const Vec3 v2 = tri.v2.pos;

            // Backface culling
            const Vec3 ab{ v1.x - v0.x, v1.y - v0.y, v1.z - v0.z };
            const Vec3 ac{ v2.x - v0.x, v2.y - v0.y, v2.z - v0.z };
            const Vec3 normal{
                ab.y * ac.z - ab.z * ac.y,
                ab.z * ac.x - ab.x * ac.z,
                ab.x * ac.y - ab.y * ac.x
            };
            const Vec3 viewDir{ -v0.x, -v0.y, -v0.z }; // camera at origin
            const float dot = normal.x * viewDir.x + normal.y * viewDir.y + normal.z * viewDir.z;
            if (dot >= 0.0f) return;

            const Vec3 p0 = project(v0);
            const Vec3 p1 = project(v1);
            const Vec3 p2 = project(v2);

            const int minX = (std::max)(0, int(std::floor((std::min)({ p0.x, p1.x, p2.x }))));
            const int maxX = (std::min)(fb.width - 1, int(std::ceil((std::max)({ p0.x, p1.x, p2.x }))));
            const int minY = (std::max)(0, int(std::floor((std::min)({ p0.y, p1.y, p2.y }))));
            const int maxY = (std::min)(fb.height - 1, int(std::ceil((std::max)({ p0.y, p1.y, p2.y }))));

            auto edge = [](const Vec3& a, const Vec3& b, float x, float y)
                {
                    return (b.x - a.x) * (y - a.y) - (b.y - a.y) * (x - a.x);
                };

            const float area = edge(p0, p1, p2.x, p2.y);
            if (std::fabs(area) < 1e-6f) return;

            const float iz0 = 1.0f / v0.z;
            const float iz1 = 1.0f / v1.z;
            const float iz2 = 1.0f / v2.z;

            const float u0o = tri.v0.uv.u * iz0, v0o = tri.v0.uv.v * iz0;
            const float u1o = tri.v1.uv.u * iz1, v1o = tri.v1.uv.v * iz1;
            const float u2o = tri.v2.uv.u * iz2, v2o = tri.v2.uv.v * iz2;

            for (int y = minY; y <= maxY; ++y)
            {
                for (int x = minX; x <= maxX; ++x)
                {
                    const float px = float(x) + 0.5f;
                    const float py = float(y) + 0.5f;

                    const float w0 = edge(p1, p2, px, py) / area;
                    const float w1 = edge(p2, p0, px, py) / area;
                    const float w2 = 1.0f - w0 - w1;

                    if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f) continue;

                    const float invZ = w0 * iz0 + w1 * iz1 + w2 * iz2;
                    if (invZ <= 0.0f) continue;

                    const float depth = 1.0f / invZ;
                    const int idx = y * fb.width + x;
                    if (depth >= zbuf[std::size_t(idx)]) continue;

                    zbuf[std::size_t(idx)] = depth;

                    const float u = (w0 * u0o + w1 * u1o + w2 * u2o) / invZ;
                    const float v = (w0 * v0o + w1 * v1o + w2 * v2o) / invZ;

                    if (tri.tex)
                    {
                        fb.put_pixel(
                            x, y,
                            tri.tex->sample(int(u * float(tri.tex->width)), int(v * float(tri.tex->height)))
                        );
                    }
                    else
                    {
                        fb.put_pixel(x, y, tri.color);
                    }
                }
            }
        }

        static void render_cube(Framebuffer& fb, TexturePtr tex, float angle, const Camera& cam = Camera())
        {
            const Mat4 rx = rotationX(angle * 0.5f);
            const Mat4 ry = rotationY(angle);
            const Mat4 model = mul(ry, rx);

            const Mat4 Rc = mul(rotationZ(cam.roll), mul(rotationX(cam.pitch), rotationY(cam.yaw)));
            const Mat4 Rview = transpose(Rc);

            struct VertOut { Vec3 viewPos; Vec2 uv; } verts[8]{};

            for (int i = 0; i < 8; ++i)
            {
                const Vec3 mpos = multiply(model, cubeVerts[i].pos);
                const Vec3 rel{ mpos.x - cam.pos.x, mpos.y - cam.pos.y, mpos.z - cam.pos.z };
                verts[i].viewPos = multiply(Rview, rel);
                verts[i].uv = cubeVerts[i].uv;
            }

            std::vector<float> zbuf(std::size_t(fb.width) * std::size_t(fb.height),
                (std::numeric_limits<float>::infinity)());

            for (int t = 0; t < 12; ++t)
            {
                Triangle tri{};
                tri.v0.pos = verts[cubeTris[t][0]].viewPos;
                tri.v1.pos = verts[cubeTris[t][1]].viewPos;
                tri.v2.pos = verts[cubeTris[t][2]].viewPos;

                tri.v0.uv = verts[cubeTris[t][0]].uv;
                tri.v1.uv = verts[cubeTris[t][1]].uv;
                tri.v2.uv = verts[cubeTris[t][2]].uv;

                tri.tex = tex;
                tri.color = faceColors[t / 2];

                rasterize_triangle(fb, tri, zbuf);
            }
        }
    };
} // namespace almondnamespace::anativecontext

#else
// If the backend is disabled, still provide the module (empty) so imports don't explode.
export namespace almondnamespace::anativecontext {}
#endif // ALMOND_USING_SOFTWARE_RENDERER
