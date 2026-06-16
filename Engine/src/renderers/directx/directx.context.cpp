module;

#include <include/engine.config.hpp>

#if defined(EPOCH_USING_DIRECTX) && (EPOCH_USING_DIRECTX == 1)
#include "framework.hpp"
#include <d3d11.h>
#include <d3dcompiler.h>
#endif

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <source_location>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

module directx.context;

import core.commandline;
import core.context;
import core.logger;
import context.commandqueue;
import engine.gui;
import render.preview_grid;
import atlas.texture;
import spritehandle;

#if defined(EPOCH_USING_DIRECTX) && (EPOCH_USING_DIRECTX == 1)
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

#include "directx_context_detail.hpp"

namespace epochnamespace::directxcontext
{
    int directx_get_width()
    {
        auto ctx = core::get_current_render_context();
        if (!ctx)
            return 1;

        std::lock_guard lock(detail::g_directxMutex);
        const auto it = detail::g_states.find(ctx.get());
        return it != detail::g_states.end() ? (std::max)(1, it->second.width) : (std::max)(1, ctx->width);
    }

    int directx_get_height()
    {
        auto ctx = core::get_current_render_context();
        if (!ctx)
            return 1;

        std::lock_guard lock(detail::g_directxMutex);
        const auto it = detail::g_states.find(ctx.get());
        return it != detail::g_states.end() ? (std::max)(1, it->second.height) : (std::max)(1, ctx->height);
    }

    bool directx_initialize(
        std::shared_ptr<core::Context> ctx,
        void* parentWnd,
        unsigned int w,
        unsigned int h,
        std::function<void(int, int)> onResize)
    {
        if (!ctx || !parentWnd)
            return false;

        detail::DirectXState state{};
        state.hwnd = static_cast<HWND>(parentWnd);
        state.width = static_cast<int>((std::max)(1u, w));
        state.height = static_cast<int>((std::max)(1u, h));
        state.onResize = std::move(onResize);
        (void)detail::client_size(state.hwnd, state.width, state.height);

        if (!detail::create_device(state))
        {
            detail::release_state(state);
            return false;
        }

        ctx->width = state.width;
        ctx->height = state.height;
        ctx->virtualWidth = state.width;
        ctx->virtualHeight = state.height;
        ctx->framebufferWidth = state.width;
        ctx->framebufferHeight = state.height;
        if (ctx->windowData)
            ctx->windowData->set_size(state.width, state.height);

        {
            std::lock_guard lock(detail::g_directxMutex);
            auto& slot = detail::g_states[ctx.get()];
            detail::release_state(slot);
            slot = std::move(state);
        }

        logger::get(detail::kLogDirectX).log(
            logger::LogLevel::INFO,
            "Initialized DirectX preview context.",
            std::source_location::current());
        return true;
    }

    bool directx_process(std::shared_ptr<core::Context> ctx, core::CommandQueue& queue)
    {
        if (!ctx)
            return false;

        if (ctx->windowData && ctx->windowData->get_should_close())
        {
            queue.clear();
            return false;
        }

        std::lock_guard lock(detail::g_directxMutex);
        auto it = detail::g_states.find(ctx.get());
        if (it == detail::g_states.end())
            return false;

        auto& state = it->second;
        if (!detail::update_size(*ctx, state) || !state.renderTarget)
            return false;

        const auto clearColor = previewgrid::kClearColor;
        const float clear[] = {
            clearColor[0],
            clearColor[1],
            clearColor[2],
            clearColor[3]
        };
        state.immediate->OMSetRenderTargets(1, &state.renderTarget, nullptr);
        state.immediate->ClearRenderTargetView(state.renderTarget, clear);

        const D3D11_VIEWPORT viewport = detail::full_window_viewport(state);
        state.immediate->RSSetViewports(1, &viewport);
        state.immediate->RSSetState(state.rasterizer);
        state.immediate->IASetInputLayout(state.inputLayout);
        state.immediate->VSSetShader(state.vertexShader, nullptr, 0);
        state.immediate->PSSetShader(state.pixelShader, nullptr, 0);

        const bool overlayPriority = ctx->gui_overlay_priority();
        if (!overlayPriority)
        {
            queue.drain();
            (void)gui::render_deferred_batch(ctx.get());
        }

        const auto sceneViewport = ctx->scene_viewport();
        if (ctx->scene_preview_mode() == core::ScenePreviewMode::Editor
            && sceneViewport.valid())
        {
            std::vector<detail::DirectXVertex> solid{};
            std::vector<detail::DirectXVertex> lines{};
            solid.reserve(256);
            lines.reserve(512);
            detail::build_preview_geometry(*ctx, state, solid, lines);
            const D3D11_VIEWPORT previewViewport = detail::scene_viewport_for(*ctx, state);
            state.immediate->RSSetViewports(1, &previewViewport);
            state.immediate->IASetInputLayout(state.inputLayout);
            state.immediate->VSSetShader(state.vertexShader, nullptr, 0);
            state.immediate->PSSetShader(state.pixelShader, nullptr, 0);
            detail::draw_vertices(state, solid, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            detail::draw_vertices(state, lines, D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
        }

        queue.drain();
        (void)gui::render_deferred_batch(ctx.get());
        (void)gui::render_top_layer_batch(ctx.get());

        (void)state.swapchain->Present(0, 0);

        if (core::cli::smoke_requested)
        {
            ++state.smokeFrames;
            if (state.smokeFrames >= 3u)
            {
                if (ctx->windowData)
                    ctx->windowData->set_should_close(true);
                queue.clear();
                return false;
            }
        }

        return true;
    }

    void directx_draw_sprite(
        SpriteHandle sprite,
        std::span<const TextureAtlas* const> atlases,
        float x,
        float y,
        float w,
        float h) noexcept
    {
        detail::draw_gui_sprite(sprite, atlases, x, y, w, h);
    }

    void directx_cleanup(std::shared_ptr<core::Context> ctx)
    {
        if (!ctx)
            return;

        std::lock_guard lock(detail::g_directxMutex);
        const auto it = detail::g_states.find(ctx.get());
        if (it == detail::g_states.end())
            return;

        detail::release_state(it->second);
        detail::g_states.erase(it);
    }
}
#else
namespace epochnamespace::directxcontext
{
    int directx_get_width()
    {
        return 1;
    }

    int directx_get_height()
    {
        return 1;
    }

    bool directx_initialize(
        std::shared_ptr<core::Context>,
        void*,
        unsigned int,
        unsigned int,
        std::function<void(int, int)>)
    {
        return false;
    }

    bool directx_process(std::shared_ptr<core::Context>, core::CommandQueue&)
    {
        return false;
    }

    void directx_draw_sprite(
        SpriteHandle,
        std::span<const TextureAtlas* const>,
        float,
        float,
        float,
        float) noexcept
    {
    }

    void directx_cleanup(std::shared_ptr<core::Context>)
    {
    }
}
#endif
