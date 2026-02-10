
module;

export module acontext.vulkan.context;

import :api;            // brings in declarations for vulkan_* funcs
import :shared_vk; // brings in Application + registry helpers
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

        if (auto* app = find_vulkan_app(ctx))
            app->enqueue_gui_draw(ctx, sprite, atlases, x, y, w, h);
    }

    // Small ones first so they're visible no matter what.
    int vulkan_get_width()
    {
        auto ctx = core::get_current_render_context();
        if (auto* app = find_vulkan_app(ctx))
            return app->get_framebuffer_width();
        return 0;
    }

    int vulkan_get_height()
    {
        auto ctx = core::get_current_render_context();
        if (auto* app = find_vulkan_app(ctx))
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

        auto& app = vulkan_app_for_context(ctx);

        app.set_framebuffer_size(static_cast<int>(w), static_cast<int>(h));
        ctx->framebufferWidth = static_cast<int>(w);
        ctx->framebufferHeight = static_cast<int>(h);
        app.set_context(ctx, nativeWindow);

        ctx->get_width  = &vulkan_get_width;
        ctx->get_height = &vulkan_get_height;

        ctx->onResize = [ctx, resize = std::move(onResize)](int nw, int nh) mutable
        {
            if (auto* app = find_vulkan_app(ctx))
            {
                app->set_framebuffer_size(nw, nh);
                ctx->framebufferWidth = nw;
                ctx->framebufferHeight = nh;
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

        const int fbW = (std::max)(1, vulkan_get_width());
        const int fbH = (std::max)(1, vulkan_get_height());

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

        auto& app = vulkan_app_for_context(ctx);
        app.set_active_context(ctx.get());
        atlasmanager::process_pending_uploads(core::ContextType::Vulkan);

        const bool result = app.process(ctx, queue);

        frameTimer.finish();

        return result;
    }

    void vulkan_present()
    {
        // no-op; presentation happens in Application flow
    }

    void vulkan_cleanup(std::shared_ptr<core::Context> ctx)
    {
        if (ctx)
        {
            if (auto* app = find_vulkan_app(ctx))
                app->cleanup_gui_context(ctx.get());
        }
        atlasmanager::unregister_backend_uploader(core::ContextType::Vulkan);
        cleanup_vulkan_app_for_context(ctx);
    }
}
