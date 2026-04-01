module;

#include <algorithm>
#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <span>
#include <utility>
#include <vector>

#include <include/aengine.config.hpp>

#if defined(_WIN32)
#   ifdef EPOCH_USING_WINMAIN
#       include "../include/aframework.hpp"
#   endif
#   ifndef WIN32_LEAN_AND_MEAN
#       define WIN32_LEAN_AND_MEAN
#   endif
#endif

module software.context;

import aspritehandle;
import atlas.texture;
import core.context;
import context.commandqueue;
import core.logger;
import software.state;
import software.textures;

namespace epochnamespace::anativecontext
{
#if defined(EPOCH_USING_SOFTWARE_RENDERER) && (EPOCH_USING_SOFTWARE_RENDERER == 1)
    inline TexturePtr cubeTexture{};

#if defined(_WIN32)
    template <class T>
    static HWND try_get_hwnd(const T& ctx)
    {
        if constexpr (requires { ctx.get_hwnd(); })
            return reinterpret_cast<HWND>(ctx.get_hwnd());
        else if constexpr (requires { ctx.hwnd(); })
            return reinterpret_cast<HWND>(ctx.hwnd());
        else if constexpr (requires { ctx.native_hwnd(); })
            return reinterpret_cast<HWND>(ctx.native_hwnd());
        else if constexpr (requires { ctx.native_window_handle(); })
            return reinterpret_cast<HWND>(ctx.native_window_handle());
        else
            return nullptr;
    }
#endif

    void softrenderer_resize(int width, int height)
    {
        auto& sr = s_softrendererstate;
        sr.width = (std::max)(1, width);
        sr.height = (std::max)(1, height);

        sr.framebuffer.assign(
            std::size_t(sr.width) * std::size_t(sr.height),
            0xFF000000u);
        sr.sceneFramebuffer.assign(
            std::size_t(sr.width) * std::size_t(sr.height),
            0xFF000000u);
        sr.frameValid = false;

#if defined(_WIN32)
        sr.bmi.bmiHeader.biWidth = sr.width;
        sr.bmi.bmiHeader.biHeight = -sr.height;
#endif
    }

    bool softrenderer_initialize(
        std::shared_ptr<core::Context> ctx,
        void* parentWnd,
        unsigned int w,
        unsigned int h,
        std::function<void(int, int)> onResize)
    {
        if (!ctx)
        {
            logger::error("Epoch.Software", "Invalid context.");
            return false;
        }

        auto& sr = s_softrendererstate;
        sr.width = static_cast<int>(w);
        sr.height = static_cast<int>(h);
        sr.running = true;
        sr.frameValid = false;
        sr.lastGuiGeneration = 0;
        sr.lastSceneViewport = {};
        sr.lastPreviewMode = static_cast<std::uint8_t>(core::ScenePreviewMode::None);

        ctx->get_width = get_width;
        ctx->get_height = get_height;

        ctx->width = (std::max)(1, sr.width);
        ctx->height = (std::max)(1, sr.height);
        ctx->virtualWidth = ctx->width;
        ctx->virtualHeight = ctx->height;
        ctx->framebufferWidth = ctx->width;
        ctx->framebufferHeight = ctx->height;
        if (ctx->windowData)
            ctx->windowData->set_size(ctx->width, ctx->height);

        std::weak_ptr<core::Context> weakCtx = ctx;
        ctx->onResize = [weakCtx, resize = std::move(onResize)](int newWidth, int newHeight) mutable
            {
                softrenderer_resize(newWidth, newHeight);
                if (auto locked = weakCtx.lock())
                {
                    locked->width = (std::max)(1, s_softrendererstate.width);
                    locked->height = (std::max)(1, s_softrendererstate.height);
                    locked->virtualWidth = locked->width;
                    locked->virtualHeight = locked->height;
                    locked->framebufferWidth = locked->width;
                    locked->framebufferHeight = locked->height;
                    if (locked->windowData)
                        locked->windowData->set_size(locked->width, locked->height);
                }
                if (resize)
                    resize(newWidth, newHeight);
            };
        sr.onResize = ctx->onResize;

        sr.framebuffer.assign(std::size_t(w) * std::size_t(h), 0xFF000000u);
        sr.sceneFramebuffer.assign(std::size_t(w) * std::size_t(h), 0xFF000000u);

#if defined(_WIN32)
        HWND resolvedParent = reinterpret_cast<HWND>(parentWnd);
        if (!resolvedParent)
            resolvedParent = try_get_hwnd(*ctx);

        if (!resolvedParent)
        {
            logger::error("Epoch.Software", "No parent HWND available. Pass parentWnd from multiplexer.");
            return false;
        }

        sr.parent = resolvedParent;
        sr.hwnd = resolvedParent;

        ZeroMemory(&sr.bmi, sizeof(BITMAPINFO));
        sr.bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        sr.bmi.bmiHeader.biWidth = sr.width;
        sr.bmi.bmiHeader.biHeight = -sr.height;
        sr.bmi.bmiHeader.biPlanes = 1;
        sr.bmi.bmiHeader.biBitCount = 32;
        sr.bmi.bmiHeader.biCompression = BI_RGB;

#if EPOCH_ENABLE_BACKEND_CONTEXT_CONFIRMATION_LOGS && EPOCH_ENABLE_SOFTWARE_RENDERER_CONFIRMATION_LOGS
        logger::info(
            "Epoch.Software",
            std::format("Initialized. HWND={} ({}x{})", reinterpret_cast<std::uintptr_t>(sr.hwnd), sr.width, sr.height));
#endif
#else
        (void)parentWnd;
#if EPOCH_ENABLE_BACKEND_CONTEXT_CONFIRMATION_LOGS && EPOCH_ENABLE_SOFTWARE_RENDERER_CONFIRMATION_LOGS
        logger::info(
            "Epoch.Software",
            std::format("Initialized (non-Win32) {}x{}", sr.width, sr.height));
#endif
#endif

        if (!cubeTexture)
        {
            cubeTexture = std::make_shared<Texture>(64, 64);
            for (int y = 0; y < cubeTexture->height; ++y)
            {
                for (int x = 0; x < cubeTexture->width; ++x)
                {
                    cubeTexture->pixels[std::size_t(y) * std::size_t(cubeTexture->width) + std::size_t(x)] =
                        ((x / 8 + y / 8) % 2) ? 0xFFFF0000u : 0xFF00FF00u;
                }
            }
        }

        return true;
    }

    void softrenderer_cleanup(std::shared_ptr<core::Context>&)
    {
        auto& sr = s_softrendererstate;

        sr.framebuffer.clear();
        sr.sceneFramebuffer.clear();
        cubeTexture.reset();
        sr.frameValid = false;
        sr.lastGuiGeneration = 0;
        sr.lastSceneViewport = {};
        sr.lastPreviewMode = static_cast<std::uint8_t>(core::ScenePreviewMode::None);

#if defined(_WIN32)
        sr.hwnd = nullptr;
        sr.parent = nullptr;
#endif
        sr.running = false;
        sr = {};

#if EPOCH_ENABLE_BACKEND_CONTEXT_CONFIRMATION_LOGS && EPOCH_ENABLE_SOFTWARE_RENDERER_CONFIRMATION_LOGS
        logger::info("Epoch.Software", "Cleanup complete.");
#endif
    }

    int get_width() { return s_softrendererstate.width; }
    int get_height() { return s_softrendererstate.height; }

#else
    bool softrenderer_initialize(std::shared_ptr<core::Context>, void*, unsigned, unsigned, std::function<void(int, int)>)
    {
        logger::error("Epoch.Software", "Not built (EPOCH_USING_SOFTWARE_RENDERER not defined).");
        return false;
    }

    void draw_sprite(SpriteHandle, std::span<const TextureAtlas* const>, float, float, float, float) noexcept {}
    bool softrenderer_process(core::Context&, core::CommandQueue&) { return false; }
    void softrenderer_cleanup(std::shared_ptr<epochnamespace::core::Context>&) {}
    int get_width() { return 0; }
    int get_height() { return 0; }
#endif
}
