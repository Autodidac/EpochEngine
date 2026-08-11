module;

#include <include/engine.config.hpp>

#if defined(EPOCH_USING_DIRECTX) && (EPOCH_USING_DIRECTX == 1)
#include "../../platform.framework.hpp"
#include <d3d11.h>
#include <d3dcompiler.h>
#endif

#include <algorithm>
#include <cstdint>
#include <functional>
#include <mutex>
#include <source_location>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

module directx.context;

import core.context;
import core.logger;
import atlas.texture;
import package.registry;
import sprite.handle;

#if defined(EPOCH_USING_DIRECTX) && (EPOCH_USING_DIRECTX == 1)
#include "directx.context_detail.hpp"

namespace epochengine::directxcontext::detail
{
    std::recursive_mutex g_directxMutex;
    std::unordered_map<const core::Context*, DirectXState> g_states;

    bool succeeded(HRESULT hr) noexcept
    {
        return SUCCEEDED(hr);
    }

    void log_failure(const char* action, HRESULT hr)
    {
        logger::get(kLogDirectX).log(
            logger::LogLevel::WARN,
            std::string(action) + " failed, HRESULT=" + std::to_string(static_cast<long>(hr)),
            std::source_location::current());
    }

    bool client_size(HWND hwnd, int& width, int& height) noexcept
    {
        RECT rect{};
        if (!hwnd || ::GetClientRect(hwnd, &rect) == FALSE)
            return false;

        width = (std::max)(1, static_cast<int>(rect.right - rect.left));
        height = (std::max)(1, static_cast<int>(rect.bottom - rect.top));
        return true;
    }

    void release_render_target(DirectXState& state) noexcept
    {
        safe_release(state.renderTarget);
    }

    void release_arcade_screen_target(DirectXState& state) noexcept
    {
        safe_release(state.arcadeScreen.sampleVertexBuffer);
        safe_release(state.arcadeScreen.shaderView);
        safe_release(state.arcadeScreen.renderTarget);
        safe_release(state.arcadeScreen.texture);
        state.arcadeScreen = {};
    }

    bool ensure_arcade_screen_target(DirectXState& state)
    {
        constexpr std::uint32_t kMaximumDimension = 2048u;
        constexpr std::uint64_t kMaximumPixels =
            static_cast<std::uint64_t>(kMaximumDimension) * kMaximumDimension;
        static_assert(package_registry::engine_arcade_render_texture_width() > 0u);
        static_assert(package_registry::engine_arcade_render_texture_height() > 0u);
        static_assert(
            package_registry::engine_arcade_render_texture_width() <= kMaximumDimension);
        static_assert(
            package_registry::engine_arcade_render_texture_height() <= kMaximumDimension);

        if (!state.device)
            return false;

        const std::uint32_t width = package_registry::engine_arcade_render_texture_width();
        const std::uint32_t height = package_registry::engine_arcade_render_texture_height();
        const std::uint64_t pixels = static_cast<std::uint64_t>(width) * height;
        if (width == 0u || height == 0u
            || width > kMaximumDimension || height > kMaximumDimension
            || pixels > kMaximumPixels)
        {
            return false;
        }

        if (state.arcadeScreen.ready()
            && state.arcadeScreen.width == width
            && state.arcadeScreen.height == height)
        {
            return true;
        }

        release_arcade_screen_target(state);

        D3D11_TEXTURE2D_DESC textureDesc{};
        textureDesc.Width = width;
        textureDesc.Height = height;
        textureDesc.MipLevels = 1;
        textureDesc.ArraySize = 1;
        textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        textureDesc.SampleDesc.Count = 1;
        textureDesc.Usage = D3D11_USAGE_DEFAULT;
        textureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

        HRESULT hr = state.device->CreateTexture2D(
            &textureDesc,
            nullptr,
            &state.arcadeScreen.texture);
        if (!succeeded(hr) || !state.arcadeScreen.texture)
        {
            log_failure("ID3D11Device::CreateTexture2D(engine_arcade.screen)", hr);
            release_arcade_screen_target(state);
            return false;
        }

        hr = state.device->CreateRenderTargetView(
            state.arcadeScreen.texture,
            nullptr,
            &state.arcadeScreen.renderTarget);
        if (!succeeded(hr) || !state.arcadeScreen.renderTarget)
        {
            log_failure("ID3D11Device::CreateRenderTargetView(engine_arcade.screen)", hr);
            release_arcade_screen_target(state);
            return false;
        }

        D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc{};
        viewDesc.Format = textureDesc.Format;
        viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        viewDesc.Texture2D.MipLevels = 1;
        hr = state.device->CreateShaderResourceView(
            state.arcadeScreen.texture,
            &viewDesc,
            &state.arcadeScreen.shaderView);
        if (!succeeded(hr) || !state.arcadeScreen.shaderView)
        {
            log_failure("ID3D11Device::CreateShaderResourceView(engine_arcade.screen)", hr);
            release_arcade_screen_target(state);
            return false;
        }

        state.arcadeScreen.width = width;
        state.arcadeScreen.height = height;
        return true;
    }

    bool create_render_target(DirectXState& state)
    {
        if (!state.swapchain || !state.device)
            return false;

        ID3D11Texture2D* backBuffer = nullptr;
        HRESULT hr = state.swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&backBuffer));
        if (!succeeded(hr) || !backBuffer)
        {
            log_failure("IDXGISwapChain::GetBuffer", hr);
            return false;
        }

        hr = state.device->CreateRenderTargetView(backBuffer, nullptr, &state.renderTarget);
        safe_release(backBuffer);
        if (!succeeded(hr))
        {
            log_failure("ID3D11Device::CreateRenderTargetView", hr);
            return false;
        }

        return true;
    }

    void release_state(DirectXState& state) noexcept
    {
        if (state.immediate)
            state.immediate->ClearState();

        for (auto& [_, gpu] : state.guiAtlases)
        {
            safe_release(gpu.view);
            safe_release(gpu.texture);
        }
        state.guiAtlases.clear();
        release_canvas2d_state(state);
        release_arcade_screen_target(state);
        release_render_target(state);
        safe_release(state.spriteVertexBuffer);
        safe_release(state.vertexBuffer);
        safe_release(state.spriteBlend);
        safe_release(state.spriteSampler);
        safe_release(state.rasterizer);
        safe_release(state.spriteInputLayout);
        safe_release(state.inputLayout);
        safe_release(state.spritePixelShader);
        safe_release(state.spriteVertexShader);
        safe_release(state.pixelShader);
        safe_release(state.vertexShader);
        safe_release(state.swapchain);
        safe_release(state.immediate);
        safe_release(state.device);
    }

    bool update_size(core::Context& ctx, DirectXState& state)
    {
        int width = state.width;
        int height = state.height;
        if (!client_size(state.hwnd, width, height))
            return false;

        if (width == state.width && height == state.height && state.renderTarget)
            return true;

        state.width = width;
        state.height = height;
        ctx.width = width;
        ctx.height = height;
        ctx.virtualWidth = width;
        ctx.virtualHeight = height;
        ctx.framebufferWidth = width;
        ctx.framebufferHeight = height;

        if (ctx.windowData)
            ctx.windowData->set_size(width, height);
        if (state.onResize)
            state.onResize(width, height);

        if (!state.swapchain)
            return false;

        if (state.immediate)
            state.immediate->OMSetRenderTargets(0, nullptr, nullptr);
        release_render_target(state);

        const HRESULT hr = state.swapchain->ResizeBuffers(0, static_cast<UINT>(width), static_cast<UINT>(height), DXGI_FORMAT_UNKNOWN, 0);
        if (!succeeded(hr))
        {
            log_failure("IDXGISwapChain::ResizeBuffers", hr);
            return false;
        }

        return create_render_target(state);
    }

    D3D11_VIEWPORT full_window_viewport(const DirectXState& state) noexcept
    {
        D3D11_VIEWPORT viewport{};
        viewport.TopLeftX = 0.0f;
        viewport.TopLeftY = 0.0f;
        viewport.Width = static_cast<float>((std::max)(1, state.width));
        viewport.Height = static_cast<float>((std::max)(1, state.height));
        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 1.0f;
        return viewport;
    }

    D3D11_VIEWPORT scene_viewport_for(
        const core::Context& ctx,
        const DirectXState& state) noexcept
    {
        const auto sceneViewport = ctx.scene_viewport();
        if (!sceneViewport.valid())
            return full_window_viewport(state);

        const int x = (std::clamp)(sceneViewport.x, 0, (std::max)(1, state.width - 1));
        const int y = (std::clamp)(sceneViewport.y, 0, (std::max)(1, state.height - 1));
        const int maxWidth = (std::max)(1, state.width - x);
        const int maxHeight = (std::max)(1, state.height - y);

        D3D11_VIEWPORT viewport{};
        viewport.TopLeftX = static_cast<float>(x);
        viewport.TopLeftY = static_cast<float>(y);
        viewport.Width = static_cast<float>((std::clamp)(sceneViewport.width, 1, maxWidth));
        viewport.Height = static_cast<float>((std::clamp)(sceneViewport.height, 1, maxHeight));
        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 1.0f;
        return viewport;
    }
}
#endif
