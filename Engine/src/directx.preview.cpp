module;

#include <include/engine.config.hpp>

#if defined(EPOCH_USING_DIRECTX) && (EPOCH_USING_DIRECTX == 1)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#endif

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <functional>
#include <mutex>
#include <span>
#include <unordered_map>
#include <vector>

module directx.context;

import core.context;
import core.logger;
import render.preview_grid;
import atlas.texture;
import spritehandle;

#if defined(EPOCH_USING_DIRECTX) && (EPOCH_USING_DIRECTX == 1)
#include "directx_context_detail.hpp"

namespace epochnamespace::directxcontext::detail
{
    namespace
    {
        bool ensure_vertex_capacity(DirectXState& state, std::size_t count)
        {
            if (count == 0)
                return false;

            if (state.vertexBuffer && state.vertexCapacity >= count)
                return true;

            safe_release(state.vertexBuffer);
            state.vertexCapacity = (std::max<std::size_t>)(256u, count + 128u);

            D3D11_BUFFER_DESC desc{};
            desc.ByteWidth = static_cast<UINT>(sizeof(DirectXVertex) * state.vertexCapacity);
            desc.Usage = D3D11_USAGE_DYNAMIC;
            desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

            const HRESULT hr = state.device->CreateBuffer(&desc, nullptr, &state.vertexBuffer);
            if (!succeeded(hr))
            {
                log_failure("ID3D11Device::CreateBuffer", hr);
                state.vertexCapacity = 0;
                return false;
            }

            return true;
        }

        bool make_clip_vertex(
            DirectXVertex& out,
            const previewgrid::Mat4& mvp,
            const previewgrid::Vertex& vertex) noexcept
        {
            auto clip = previewgrid::transform_point(mvp, vertex.position);
            if (clip.w <= 1.0e-4f)
                return false;

            clip.z = (clip.z + clip.w) * 0.5f;
            if (!std::isfinite(clip.x) || !std::isfinite(clip.y) || !std::isfinite(clip.z) || !std::isfinite(clip.w))
                return false;

            out = DirectXVertex{
                .x = clip.x,
                .y = clip.y,
                .z = clip.z,
                .w = clip.w,
                .r = vertex.color.x,
                .g = vertex.color.y,
                .b = vertex.color.z
            };
            return true;
        }

        bool append_clip_line(
            std::vector<DirectXVertex>& out,
            const previewgrid::Mat4& mvp,
            const previewgrid::Vertex& first,
            const previewgrid::Vertex& second)
        {
            DirectXVertex a{};
            DirectXVertex b{};
            if (!make_clip_vertex(a, mvp, first) || !make_clip_vertex(b, mvp, second))
                return false;

            out.push_back(a);
            out.push_back(b);
            return true;
        }

        bool append_clip_triangle(
            std::vector<DirectXVertex>& out,
            const previewgrid::Mat4& mvp,
            const previewgrid::Vertex& first,
            const previewgrid::Vertex& second,
            const previewgrid::Vertex& third)
        {
            DirectXVertex a{};
            DirectXVertex b{};
            DirectXVertex c{};
            if (!make_clip_vertex(a, mvp, first)
                || !make_clip_vertex(b, mvp, second)
                || !make_clip_vertex(c, mvp, third))
            {
                return false;
            }

            out.push_back(a);
            out.push_back(b);
            out.push_back(c);
            return true;
        }

        previewgrid::Mat4 preview_projection(
            const core::Context& ctx,
            const float aspect,
            const previewgrid::Camera& camera) noexcept
        {
            if (previewgrid::camera_mode_for(&ctx) == previewgrid::CameraMode::Canvas2D)
            {
                const auto delta = previewgrid::subtract(camera.eye, camera.target);
                const float distance = std::sqrt(previewgrid::dot(delta, delta));
                const float halfHeight = (std::max)(2.0f, distance * 0.42f);
                const float halfWidth = halfHeight * aspect;
                return previewgrid::orthographic(
                    -halfWidth,
                    halfWidth,
                    -halfHeight,
                    halfHeight,
                    camera.nearPlane,
                    camera.farPlane);
            }

            return previewgrid::perspective(
                camera.fovRadians,
                aspect,
                camera.nearPlane,
                camera.farPlane);
        }
    }

    void draw_vertices(
        DirectXState& state,
        const std::vector<DirectXVertex>& vertices,
        D3D11_PRIMITIVE_TOPOLOGY topology)
    {
        if (vertices.empty() || !ensure_vertex_capacity(state, vertices.size()))
            return;

        D3D11_MAPPED_SUBRESOURCE mapped{};
        HRESULT hr = state.immediate->Map(state.vertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        if (!succeeded(hr) || !mapped.pData)
        {
            log_failure("ID3D11DeviceContext::Map", hr);
            return;
        }

        std::memcpy(mapped.pData, vertices.data(), sizeof(DirectXVertex) * vertices.size());
        state.immediate->Unmap(state.vertexBuffer, 0);

        constexpr UINT stride = sizeof(DirectXVertex);
        constexpr UINT offset = 0;
        ID3D11Buffer* buffer = state.vertexBuffer;
        state.immediate->IASetVertexBuffers(0, 1, &buffer, &stride, &offset);
        state.immediate->IASetPrimitiveTopology(topology);
        state.immediate->Draw(static_cast<UINT>(vertices.size()), 0);
    }

    void build_preview_geometry(
        const core::Context& ctx,
        const DirectXState& state,
        std::vector<DirectXVertex>& solid,
        std::vector<DirectXVertex>& lines)
    {
        auto viewport = ctx.scene_viewport();
        if (!viewport.valid())
        {
            viewport = core::RenderViewport{
                0,
                0,
                (std::max)(1, state.width),
                (std::max)(1, state.height)
            };
        }

        const auto camera = previewgrid::camera_for(&ctx);
        const float aspect = viewport.height > 0
            ? (viewport.width / static_cast<float>(viewport.height))
            : 1.0f;
        const auto proj = preview_projection(ctx, aspect, camera);
        const auto view = previewgrid::look_at(
            camera.eye,
            camera.target,
            camera.up);
        const auto mvp = previewgrid::multiply(proj, view);

        const auto gridVertices = previewgrid::grid_vertices();
        const auto gridIndices = previewgrid::grid_indices();
        for (std::size_t i = 0; i + 1 < gridIndices.size(); i += 2)
        {
            const auto first = static_cast<std::size_t>(gridIndices[i]);
            const auto second = static_cast<std::size_t>(gridIndices[i + 1]);
            if (first >= gridVertices.size() || second >= gridVertices.size())
                continue;

            (void)append_clip_line(lines, mvp, gridVertices[first], gridVertices[second]);
        }

        const auto solidVertices = previewgrid::object_solid_vertices_for(&ctx);
        for (std::size_t i = 0; i + 2 < solidVertices.size(); i += 3)
        {
            (void)append_clip_triangle(
                solid,
                mvp,
                solidVertices[i],
                solidVertices[i + 1],
                solidVertices[i + 2]);
        }

        const auto focusVertices = previewgrid::look_marker_vertices_for(&ctx);
        const std::size_t focusCount = previewgrid::look_marker_vertex_count_for(&ctx);
        for (std::size_t i = 0; i + 1 < focusCount && i + 1 < focusVertices.size(); i += 2)
            (void)append_clip_line(lines, mvp, focusVertices[i], focusVertices[i + 1]);

        const auto objectVertices = previewgrid::object_marker_vertices_for(&ctx);
        for (std::size_t i = 0; i + 1 < objectVertices.size(); i += 2)
            (void)append_clip_line(lines, mvp, objectVertices[i], objectVertices[i + 1]);
    }
}
#endif
