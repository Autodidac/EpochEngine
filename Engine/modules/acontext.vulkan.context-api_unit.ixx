
module;

export module acontext.vulkan.context;

import :api;            // brings in declarations for vulkan_* funcs
import :shared_vk; // brings in per-context Application registry helpers
import :texture;

import aengine.core.context;
import aengine.diagnostics;
import aengine.telemetry;
import aatlas.manager;
import aatlas.texture;
import aspritehandle;
import aengine.context.commandqueue;

import <algorithm>;
import <cstddef>;
import <cstdint>;
import <functional>;
import <stdexcept>;
import <utility>;
import <memory>;
import <iostream>;
import <span>;

export namespace almondnamespace::vulkancontext
{
    void vulkan_draw_sprite(
        SpriteHandle sprite,
        std::span<const TextureAtlas* const> atlases,
        float x,
        float y,
        float w,
        float h)
    {
        auto ctx = core::get_current_render_context();
        if (!ctx)
            return;

        if (auto* app = try_get_vulkan_app(ctx.get()))
            app->enqueue_gui_draw(ctx.get(), sprite, atlases, x, y, w, h);
    }

    // Small ones first so they're visible no matter what.
    int vulkan_get_width()
    {
        auto ctx = core::get_current_render_context();
        if (!ctx)
            return 0;

        if (auto* app = try_get_vulkan_app(ctx.get()))
            return app->get_framebuffer_width();

        return 0;
    }

    int vulkan_get_height()
    {
        auto ctx = core::get_current_render_context();
        if (!ctx)
            return 0;

        if (auto* app = try_get_vulkan_app(ctx.get()))
            return app->get_framebuffer_height();

        return 0;
    }

    bool vulkan_initialize(
        std::shared_ptr<core::Context> ctx,
        void* parentWindowOpaque,
        unsigned int w,
        unsigned int h,
        std::function<void(int, int)> onResize)
    {
        if (!ctx)
            throw std::runtime_error("[Vulkan] vulkan_initialize requires non-null Context");

        void* nativeWindow = parentWindowOpaque;

#if defined(_WIN32) && !defined(ALMOND_MAIN_HEADLESS)
        if (ctx->windowData && ctx->windowData->hwnd)
            nativeWindow = ctx->windowData->hwnd;
        else if (ctx->get_hwnd())
            nativeWindow = ctx->get_hwnd(); // <-- CALL IT
#endif

        auto& app = bind_vulkan_app(ctx);

        app.set_framebuffer_size(static_cast<int>(w), static_cast<int>(h));
        ctx->framebufferWidth = static_cast<int>(w);
        ctx->framebufferHeight = static_cast<int>(h);
        app.set_context(ctx, nativeWindow);

        ctx->get_width  = &vulkan_get_width;
        ctx->get_height = &vulkan_get_height;

        ctx->onResize = [ctxWeak = std::weak_ptr<core::Context>{ ctx }, resize = std::move(onResize)](int nw, int nh) mutable
        {
            if (auto ctxStrong = ctxWeak.lock())
            {
                if (auto* app = try_get_vulkan_app(ctxStrong.get()))
                    app->set_framebuffer_size(nw, nh);

                ctxStrong->framebufferWidth = nw;
                ctxStrong->framebufferHeight = nh;
            }

            if (resize) resize(nw, nh);
        };

        app.initWindow();
        app.initVulkan();

        //vulkan_initialize
		std::cout << "[Vulkan] Initialized successfully.\n";

        ctx->draw_sprite = &vulkan_draw_sprite;

        atlasmanager::register_backend_uploader(core::ContextType::Vulkan,
            [](const TextureAtlas& atlas) { vulkantextures::ensure_uploaded(atlas); });

        return true;
    }

    bool vulkan_process(std::shared_ptr<core::Context> ctx, core::CommandQueue& queue)
    {
        if (!ctx)
            return false;

        const std::uintptr_t windowId = ctx->windowData
            ? reinterpret_cast<std::uintptr_t>(ctx->windowData->hwnd)
            : reinterpret_cast<std::uintptr_t>(ctx->native_window);

        almond::diagnostics::FrameTiming frameTimer{ core::ContextType::Vulkan, windowId, "Vulkan" };

        auto* app = try_get_vulkan_app(ctx.get());
        if (!app)
            return false;

        const int fbW = (std::max)(1, app->get_framebuffer_width());
        const int fbH = (std::max)(1, app->get_framebuffer_height());

        telemetry::emit_gauge(
            "renderer.framebuffer.size",
            static_cast<std::int64_t>(fbW),
            telemetry::RendererTelemetryTags{ core::ContextType::Vulkan, windowId, "width" });
        telemetry::emit_gauge(
            "renderer.framebuffer.size",
            static_cast<std::int64_t>(fbH),
            telemetry::RendererTelemetryTags{ core::ContextType::Vulkan, windowId, "height" });

        const std::size_t depth = queue.depth();
        telemetry::emit_gauge(
            "renderer.command_queue.depth",
            static_cast<std::int64_t>(depth),
            telemetry::RendererTelemetryTags{ core::ContextType::Vulkan, windowId });

        atlasmanager::process_pending_uploads(core::ContextType::Vulkan);

        if (app->should_stop_rendering())
        {
            frameTimer.finish();
            return false;
        }

        const bool result = app->process(ctx, queue);

        frameTimer.finish();

        if (!result)
            return false;

        return !app->should_stop_rendering();
    }

    void vulkan_present()
    {
        // no-op; presentation happens in Application flow
    }

    void vulkan_cleanup(std::shared_ptr<core::Context> ctx)
    {
        if (!ctx)
            return;

        if (auto* app = try_get_vulkan_app(ctx.get()))
        {
            app->cleanup_gui_context(ctx.get());
            app->cleanup();
        }

        (void)release_vulkan_app(ctx.get());

        if (!has_vulkan_apps())
            atlasmanager::unregister_backend_uploader(core::ContextType::Vulkan);
    }
}
