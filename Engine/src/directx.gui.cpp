module;

#include <include/engine.config.hpp>

#if defined(EPOCH_USING_DIRECTX) && (EPOCH_USING_DIRECTX == 1)
#include "framework.hpp"
#include <d3d11.h>
#include <d3dcompiler.h>
#endif

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <functional>
#include <mutex>
#include <span>
#include <unordered_map>
#include <vector>

module directx.context;

import core.context;
import core.logger;
import atlas.texture;
import spritehandle;

#if defined(EPOCH_USING_DIRECTX) && (EPOCH_USING_DIRECTX == 1)
#include "renderers/directx/directx_context_detail.hpp"

namespace epochnamespace::directxcontext::detail
{
    namespace
    {
        constexpr float kMinGuiSpriteExtent = 0.5f;
        constexpr float kMaxGuiSpriteExtent = 65536.0f;

        struct ClippedGuiSprite
        {
            float x{};
            float y{};
            float w{};
            float h{};
            float u0{};
            float u1{};
            float v0{};
            float v1{};
        };

        [[nodiscard]] bool clip_gui_sprite_to_framebuffer(
            const AtlasRegion& region,
            const float fbW,
            const float fbH,
            const float x,
            const float y,
            const float w,
            const float h,
            ClippedGuiSprite& out) noexcept
        {
            if (!std::isfinite(fbW) || !std::isfinite(fbH) || fbW <= 0.0f || fbH <= 0.0f)
                return false;
            if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(w) || !std::isfinite(h)
                || w < kMinGuiSpriteExtent || h < kMinGuiSpriteExtent
                || w > kMaxGuiSpriteExtent || h > kMaxGuiSpriteExtent)
            {
                return false;
            }
            if (!std::isfinite(region.u1) || !std::isfinite(region.u2)
                || !std::isfinite(region.v1) || !std::isfinite(region.v2)
                || region.u2 <= region.u1 || region.v2 <= region.v1)
            {
                return false;
            }

            const float left = x;
            const float top = y;
            const float right = x + w;
            const float bottom = y + h;
            if (right <= 0.0f || bottom <= 0.0f || left >= fbW || top >= fbH)
                return false;

            const float clippedLeft = (std::clamp)(left, 0.0f, fbW);
            const float clippedTop = (std::clamp)(top, 0.0f, fbH);
            const float clippedRight = (std::clamp)(right, 0.0f, fbW);
            const float clippedBottom = (std::clamp)(bottom, 0.0f, fbH);
            const float clippedW = clippedRight - clippedLeft;
            const float clippedH = clippedBottom - clippedTop;
            if (clippedW < kMinGuiSpriteExtent || clippedH < kMinGuiSpriteExtent)
                return false;

            const float invW = 1.0f / w;
            const float invH = 1.0f / h;
            const float uSpan = region.u2 - region.u1;
            const float sourceV0 = 1.0f - region.v2;
            const float sourceV1 = 1.0f - region.v1;
            const float vSpan = sourceV1 - sourceV0;

            out.x = clippedLeft;
            out.y = clippedTop;
            out.w = clippedW;
            out.h = clippedH;
            out.u0 = region.u1 + (clippedLeft - left) * invW * uSpan;
            out.u1 = region.u1 + (clippedRight - left) * invW * uSpan;
            out.v0 = sourceV0 + (clippedTop - top) * invH * vSpan;
            out.v1 = sourceV0 + (clippedBottom - top) * invH * vSpan;
            return std::isfinite(out.x) && std::isfinite(out.y)
                && std::isfinite(out.w) && std::isfinite(out.h)
                && std::isfinite(out.u0) && std::isfinite(out.u1)
                && std::isfinite(out.v0) && std::isfinite(out.v1);
        }

        bool ensure_sprite_vertex_capacity(DirectXState& state, const std::size_t count)
        {
            if (count == 0)
                return false;

            if (state.spriteVertexBuffer && state.spriteVertexCapacity >= count)
                return true;

            safe_release(state.spriteVertexBuffer);
            state.spriteVertexCapacity = (std::max<std::size_t>)(256u, count + 128u);

            D3D11_BUFFER_DESC desc{};
            desc.ByteWidth = static_cast<UINT>(sizeof(DirectXSpriteVertex) * state.spriteVertexCapacity);
            desc.Usage = D3D11_USAGE_DYNAMIC;
            desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

            const HRESULT hr = state.device->CreateBuffer(&desc, nullptr, &state.spriteVertexBuffer);
            if (!succeeded(hr))
            {
                log_failure("ID3D11Device::CreateBuffer(sprite)", hr);
                state.spriteVertexCapacity = 0;
                return false;
            }

            return true;
        }

        bool ensure_gui_atlas(DirectXState& state, const TextureAtlas& atlas)
        {
            if (!state.device)
                return false;

            if (atlas.pixel_data.empty())
                const_cast<TextureAtlas&>(atlas).rebuild_pixels();

            if (atlas.pixel_data.empty() || atlas.width == 0 || atlas.height == 0)
                return false;

            auto& gpu = state.guiAtlases[&atlas];
            if (gpu.view && gpu.version == atlas.version)
                return true;

            safe_release(gpu.view);
            safe_release(gpu.texture);

            D3D11_TEXTURE2D_DESC textureDesc{};
            textureDesc.Width = atlas.width;
            textureDesc.Height = atlas.height;
            textureDesc.MipLevels = 1;
            textureDesc.ArraySize = 1;
            textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            textureDesc.SampleDesc.Count = 1;
            textureDesc.Usage = D3D11_USAGE_DEFAULT;
            textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

            D3D11_SUBRESOURCE_DATA data{};
            data.pSysMem = atlas.pixel_data.data();
            data.SysMemPitch = atlas.width * 4u;
            data.SysMemSlicePitch = atlas.width * atlas.height * 4u;

            HRESULT hr = state.device->CreateTexture2D(&textureDesc, &data, &gpu.texture);
            if (!succeeded(hr) || !gpu.texture)
            {
                log_failure("ID3D11Device::CreateTexture2D(gui atlas)", hr);
                return false;
            }

            D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc{};
            viewDesc.Format = textureDesc.Format;
            viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            viewDesc.Texture2D.MipLevels = 1;
            hr = state.device->CreateShaderResourceView(gpu.texture, &viewDesc, &gpu.view);
            if (!succeeded(hr) || !gpu.view)
            {
                log_failure("ID3D11Device::CreateShaderResourceView(gui atlas)", hr);
                safe_release(gpu.texture);
                return false;
            }

            gpu.version = atlas.version;
            gpu.width = atlas.width;
            gpu.height = atlas.height;
            return true;
        }
    }

    void draw_gui_sprite(
        SpriteHandle sprite,
        std::span<const TextureAtlas* const> atlases,
        float x,
        float y,
        float w,
        float h) noexcept
    {
        if (!sprite.is_valid())
            return;
        if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(w) || !std::isfinite(h)
            || w <= 0.0f || h <= 0.0f)
        {
            return;
        }

        auto ctx = core::get_current_render_context();
        if (!ctx)
            return;

        std::lock_guard lock(g_directxMutex);
        auto it = g_states.find(ctx.get());
        if (it == g_states.end())
            return;

        auto& state = it->second;
        if (!state.immediate || !state.renderTarget || !state.spriteVertexShader || !state.spritePixelShader)
            return;

        const auto atlasIndex = static_cast<std::size_t>(sprite.atlasIndex);
        if (atlasIndex >= atlases.size())
            return;

        const TextureAtlas* atlas = atlases[atlasIndex];
        if (!atlas)
            return;

        AtlasRegion region{};
        if (!atlas->try_get_entry_info(static_cast<int>(sprite.localIndex), region))
            return;

        if (!ensure_gui_atlas(state, *atlas))
            return;

        const auto atlasIt = state.guiAtlases.find(atlas);
        if (atlasIt == state.guiAtlases.end() || !atlasIt->second.view)
            return;

        const float fbW = static_cast<float>((std::max)(1, state.width));
        const float fbH = static_cast<float>((std::max)(1, state.height));
        ClippedGuiSprite clipped{};
        if (!clip_gui_sprite_to_framebuffer(region, fbW, fbH, x, y, w, h, clipped))
            return;

        const float x0 = (clipped.x / fbW) * 2.0f - 1.0f;
        const float x1 = ((clipped.x + clipped.w) / fbW) * 2.0f - 1.0f;
        const float y0 = 1.0f - (clipped.y / fbH) * 2.0f;
        const float y1 = 1.0f - ((clipped.y + clipped.h) / fbH) * 2.0f;
        const float u0 = clipped.u0;
        const float u1 = clipped.u1;
        const float v0 = clipped.v0;
        const float v1 = clipped.v1;

        const DirectXSpriteVertex vertices[] = {
            { x0, y0, 0.0f, 1.0f, u0, v0 },
            { x1, y0, 0.0f, 1.0f, u1, v0 },
            { x1, y1, 0.0f, 1.0f, u1, v1 },
            { x1, y1, 0.0f, 1.0f, u1, v1 },
            { x0, y1, 0.0f, 1.0f, u0, v1 },
            { x0, y0, 0.0f, 1.0f, u0, v0 },
        };

        constexpr std::size_t vertexCount = sizeof(vertices) / sizeof(vertices[0]);
        if (!ensure_sprite_vertex_capacity(state, vertexCount))
            return;

        D3D11_MAPPED_SUBRESOURCE mapped{};
        HRESULT hr = state.immediate->Map(state.spriteVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        if (!succeeded(hr) || !mapped.pData)
            return;
        std::memcpy(mapped.pData, vertices, sizeof(vertices));
        state.immediate->Unmap(state.spriteVertexBuffer, 0);

        D3D11_VIEWPORT viewport{};
        viewport.Width = fbW;
        viewport.Height = fbH;
        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 1.0f;
        state.immediate->RSSetViewports(1, &viewport);
        state.immediate->OMSetRenderTargets(1, &state.renderTarget, nullptr);
        const float blendFactor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
        state.immediate->OMSetBlendState(state.spriteBlend, blendFactor, 0xffffffffu);

        constexpr UINT stride = sizeof(DirectXSpriteVertex);
        constexpr UINT offset = 0;
        ID3D11Buffer* buffer = state.spriteVertexBuffer;
        state.immediate->IASetInputLayout(state.spriteInputLayout);
        state.immediate->IASetVertexBuffers(0, 1, &buffer, &stride, &offset);
        state.immediate->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        state.immediate->VSSetShader(state.spriteVertexShader, nullptr, 0);
        state.immediate->PSSetShader(state.spritePixelShader, nullptr, 0);
        ID3D11ShaderResourceView* view = atlasIt->second.view;
        state.immediate->PSSetShaderResources(0, 1, &view);
        state.immediate->PSSetSamplers(0, 1, &state.spriteSampler);
        state.immediate->Draw(static_cast<UINT>(vertexCount), 0);

        ID3D11ShaderResourceView* nullView = nullptr;
        state.immediate->PSSetShaderResources(0, 1, &nullView);
        state.immediate->OMSetBlendState(nullptr, blendFactor, 0xffffffffu);
    }
}
#endif
