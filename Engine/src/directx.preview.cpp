module;

#include <include/engine.config.hpp>

#if defined(EPOCH_USING_DIRECTX) && (EPOCH_USING_DIRECTX == 1)
#include "framework.hpp"
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
import render.arcade;
import render.preview_grid;
import atlas.texture;
import spritehandle;

#if defined(EPOCH_USING_DIRECTX) && (EPOCH_USING_DIRECTX == 1)
#include "renderers/directx/directx_context_detail.hpp"

namespace epochengine::directxcontext::detail
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

    void render_engine_arcade_sampled_surface_preview(
        const core::Context& ctx,
        DirectXState& state) noexcept
    {
        constexpr std::size_t kVerticesPerQuad = 6u;
        static_assert(kVerticesPerQuad == 6u);

        if (!state.device || !state.immediate || !state.renderTarget)
            return;

        const auto markers = previewgrid::sampled_render_surface_markers_for(&ctx);
        if (markers.empty() || !ensure_arcade_screen_target(state))
            return;

        auto& target = state.arcadeScreen;
        ID3D11ShaderResourceView* nullView = nullptr;
        state.immediate->PSSetShaderResources(0, 1, &nullView);
        state.immediate->OMSetRenderTargets(1, &target.renderTarget, nullptr);

        D3D11_VIEWPORT targetViewport{};
        targetViewport.Width = static_cast<float>(target.width);
        targetViewport.Height = static_cast<float>(target.height);
        targetViewport.MinDepth = 0.0f;
        targetViewport.MaxDepth = 1.0f;
        state.immediate->RSSetViewports(1, &targetViewport);
        state.immediate->RSSetState(state.rasterizer);
        state.immediate->IASetInputLayout(state.inputLayout);
        state.immediate->VSSetShader(state.vertexShader, nullptr, 0);
        state.immediate->PSSetShader(state.pixelShader, nullptr, 0);

        constexpr float kBackground[4]{
            4.0f / 255.0f,
            6.0f / 255.0f,
            11.0f / 255.0f,
            1.0f
        };
        state.immediate->ClearRenderTargetView(target.renderTarget, kBackground);

        std::vector<DirectXVertex> patternVertices{};
        patternVertices.reserve(192u);
        render_arcade::emit_arcade_attract_pattern(
            static_cast<int>(target.width),
            static_cast<int>(target.height),
            target.frame++,
            [&](const render_arcade::ArcadePreviewRect& rect)
            {
                const float left = (static_cast<float>(rect.x) / static_cast<float>(target.width)) * 2.0f - 1.0f;
                const float right = (static_cast<float>(rect.x + rect.width) / static_cast<float>(target.width)) * 2.0f - 1.0f;
                const float top = 1.0f - (static_cast<float>(rect.y) / static_cast<float>(target.height)) * 2.0f;
                const float bottom = 1.0f - (static_cast<float>(rect.y + rect.height) / static_cast<float>(target.height)) * 2.0f;
                const auto vertex = [&](float x, float y) noexcept
                {
                    return DirectXVertex{
                        x,
                        y,
                        0.0f,
                        1.0f,
                        rect.color[0],
                        rect.color[1],
                        rect.color[2]
                    };
                };
                patternVertices.push_back(vertex(left, top));
                patternVertices.push_back(vertex(right, top));
                patternVertices.push_back(vertex(right, bottom));
                patternVertices.push_back(vertex(right, bottom));
                patternVertices.push_back(vertex(left, bottom));
                patternVertices.push_back(vertex(left, top));
            });
        draw_vertices(state, patternVertices, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        state.immediate->OMSetRenderTargets(1, &state.renderTarget, nullptr);
        const D3D11_VIEWPORT sceneViewport = scene_viewport_for(ctx, state);
        state.immediate->RSSetViewports(1, &sceneViewport);

        const auto camera = previewgrid::camera_for(&ctx);
        const auto sourceViewport = ctx.scene_viewport();
        const float aspect = sourceViewport.height > 0
            ? sourceViewport.width / static_cast<float>(sourceViewport.height)
            : 1.0f;
        const auto projection = previewgrid::projection_for(&ctx, aspect, camera);
        const auto view = previewgrid::look_at(camera.eye, camera.target, camera.up);
        const auto mvp = previewgrid::multiply(projection, view);

        const auto make_sample_vertex = [&](const previewgrid::Vec3& position, float u, float v, DirectXSpriteVertex& out) noexcept
        {
            auto clip = previewgrid::transform_point(mvp, position);
            if (clip.w <= 1.0e-4f
                || !std::isfinite(clip.x) || !std::isfinite(clip.y)
                || !std::isfinite(clip.z) || !std::isfinite(clip.w))
            {
                return false;
            }
            clip.z = (clip.z + clip.w) * 0.5f;
            out = DirectXSpriteVertex{ clip.x, clip.y, clip.z, clip.w, u, v };
            return true;
        };

        std::vector<DirectXSpriteVertex> sampleVertices{};
        sampleVertices.reserve(markers.size() * kVerticesPerQuad);
        for (const auto& marker : markers)
        {
            const float halfX = (std::max)(std::abs(marker.scale.x) * 0.5f, 0.25f);
            const float halfY = (std::max)(std::abs(marker.scale.y) * 0.5f, 0.18f);
            const float z =
                render_arcade::screen_sample_plane_z(
                    marker.position.z, marker.scale.z);
            const previewgrid::Vec3 world[4]{
                { marker.position.x - halfX, marker.position.y - halfY, z },
                { marker.position.x + halfX, marker.position.y - halfY, z },
                { marker.position.x + halfX, marker.position.y + halfY, z },
                { marker.position.x - halfX, marker.position.y + halfY, z }
            };
            DirectXSpriteVertex quad[4]{};
            if (!make_sample_vertex(world[0], 0.0f, 1.0f, quad[0])
                || !make_sample_vertex(world[1], 1.0f, 1.0f, quad[1])
                || !make_sample_vertex(world[2], 1.0f, 0.0f, quad[2])
                || !make_sample_vertex(world[3], 0.0f, 0.0f, quad[3]))
            {
                continue;
            }
            sampleVertices.insert(
                sampleVertices.end(),
                { quad[0], quad[1], quad[2], quad[0], quad[2], quad[3] });
        }

        const auto ensureSampleCapacity = [&](std::size_t count)
        {
            if (count == 0u)
                return false;
            if (target.sampleVertexBuffer && target.sampleVertexCapacity >= count)
                return true;

            safe_release(target.sampleVertexBuffer);
            target.sampleVertexCapacity = (std::max<std::size_t>)(64u, count + 24u);
            D3D11_BUFFER_DESC desc{};
            desc.ByteWidth = static_cast<UINT>(
                sizeof(DirectXSpriteVertex) * target.sampleVertexCapacity);
            desc.Usage = D3D11_USAGE_DYNAMIC;
            desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            const HRESULT hr = state.device->CreateBuffer(
                &desc,
                nullptr,
                &target.sampleVertexBuffer);
            if (!succeeded(hr) || !target.sampleVertexBuffer)
            {
                log_failure("ID3D11Device::CreateBuffer(engine_arcade.screen sample)", hr);
                target.sampleVertexCapacity = 0u;
                return false;
            }
            return true;
        };

        if (!ensureSampleCapacity(sampleVertices.size()))
            return;

        D3D11_MAPPED_SUBRESOURCE mapped{};
        const HRESULT mapResult = state.immediate->Map(
            target.sampleVertexBuffer,
            0,
            D3D11_MAP_WRITE_DISCARD,
            0,
            &mapped);
        if (!succeeded(mapResult) || !mapped.pData)
        {
            log_failure("ID3D11DeviceContext::Map(engine_arcade.screen sample)", mapResult);
            return;
        }
        std::memcpy(
            mapped.pData,
            sampleVertices.data(),
            sizeof(DirectXSpriteVertex) * sampleVertices.size());
        state.immediate->Unmap(target.sampleVertexBuffer, 0);

        constexpr UINT stride = sizeof(DirectXSpriteVertex);
        constexpr UINT offset = 0;
        ID3D11Buffer* sampleBuffer = target.sampleVertexBuffer;
        state.immediate->IASetInputLayout(state.spriteInputLayout);
        state.immediate->IASetVertexBuffers(0, 1, &sampleBuffer, &stride, &offset);
        state.immediate->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        state.immediate->VSSetShader(state.spriteVertexShader, nullptr, 0);
        state.immediate->PSSetShader(state.spritePixelShader, nullptr, 0);
        const float blendFactor[4]{};
        state.immediate->OMSetBlendState(state.spriteBlend, blendFactor, 0xffffffffu);
        ID3D11ShaderResourceView* viewResource = target.shaderView;
        state.immediate->PSSetShaderResources(0, 1, &viewResource);
        state.immediate->PSSetSamplers(0, 1, &state.spriteSampler);
        state.immediate->Draw(static_cast<UINT>(sampleVertices.size()), 0);

        state.immediate->PSSetShaderResources(0, 1, &nullView);
        state.immediate->OMSetBlendState(nullptr, blendFactor, 0xffffffffu);
        state.immediate->IASetInputLayout(state.inputLayout);
        state.immediate->VSSetShader(state.vertexShader, nullptr, 0);
        state.immediate->PSSetShader(state.pixelShader, nullptr, 0);
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
        const auto proj = previewgrid::projection_for(&ctx, aspect, camera);
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
            if (!previewgrid::clockwise_solid_triangle_faces_camera(
                    solidVertices[i].position,
                    solidVertices[i + 1].position,
                    solidVertices[i + 2].position,
                    camera.eye))
            {
                continue;
            }
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
