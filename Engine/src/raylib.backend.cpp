module;

#include <algorithm>
#include <atomic>
#include <exception>
#include <memory>
#include <source_location>
#include <string>

#include <include/engine.config.hpp>

module raylib.backend;

#if defined(EPOCH_USING_RAYLIB) && (EPOCH_USING_RAYLIB == 1)
import engine.gui;
import engine.input;
import atlas.texture;
import core.context;
import core.logger;
import image.loader;
import raylib.context;
import raylib.renderer;

namespace epochnamespace::raylibbackend
{
    namespace detail
    {
        constexpr std::string_view kLogRaylib = "Context.Raylib";

        void* native_window_handle(const std::shared_ptr<core::Context>& ctx) noexcept
        {
            if (!ctx)
                return nullptr;
            if (auto handle = ctx->get_hwnd())
                return handle;
            if (ctx->windowData && ctx->windowData->hwnd)
                return ctx->windowData->hwnd;
            return nullptr;
        }

        std::uint32_t default_add_texture(TextureAtlas&, std::string, const ImageData&) noexcept
        {
            return 0u;
        }

        std::uint32_t default_add_atlas(const TextureAtlas& atlas) noexcept
        {
            const int idx = atlas.get_index();
            return static_cast<std::uint32_t>(idx >= 0 ? idx + 1 : 1);
        }

        void bind_default_input(const std::shared_ptr<core::Context>& ctx)
        {
            ctx->is_key_held = [](input::Key key) { return input::is_key_held(key); };
            ctx->is_key_down = [](input::Key key) { return input::is_key_down(key); };
            ctx->get_mouse_position = [](int& x, int& y)
            {
                x = input::mouseX.load(std::memory_order_relaxed);
                y = input::mouseY.load(std::memory_order_relaxed);
            };
            ctx->is_mouse_button_held = [](input::MouseButton button) { return input::is_mouse_button_held(button); };
            ctx->is_mouse_button_down = [](input::MouseButton button) { return input::is_mouse_button_down(button); };
        }
    }

    void configure(const std::shared_ptr<core::Context>& ctx)
    {
        if (!ctx)
            return;

        ctx->initialize = []()
        {
            auto current = core::get_current_render_context();
            if (!current)
                return;

            try
            {
                (void)raylibcontext::raylib_initialize(
                    current,
                    detail::native_window_handle(current),
                    static_cast<unsigned>((std::max)(1, current->width)),
                    static_cast<unsigned>((std::max)(1, current->height)),
                    current->onResize,
                    current->backendName);
            }
            catch (const std::exception& e)
            {
                logger::get(detail::kLogRaylib).logf(
                    logger::LogLevel::Error,
                    std::source_location::current(),
                    "init exception: {}",
                    e.what());
            }
            catch (...)
            {
                logger::get(detail::kLogRaylib).log(
                    logger::LogLevel::Error,
                    "init unknown exception",
                    std::source_location::current());
            }
        };

        ctx->cleanup = []()
        {
            auto current = core::get_current_render_context();
            if (!current)
                return;

            try
            {
                raylibcontext::raylib_cleanup(current);
            }
            catch (const std::exception& e)
            {
                logger::get(detail::kLogRaylib).logf(
                    logger::LogLevel::Error,
                    std::source_location::current(),
                    "cleanup exception: {}",
                    e.what());
            }
            catch (...)
            {
                logger::get(detail::kLogRaylib).log(
                    logger::LogLevel::Error,
                    "cleanup unknown exception",
                    std::source_location::current());
            }
        };

        ctx->process = [](std::shared_ptr<core::Context> current, core::CommandQueue& queue)
        {
            if (!current)
                return false;

            raylibcontext::raylib_process();
            if (!raylibcontext::raylib_is_running())
                return false;

            raylibcontext::raylib_render_scene_preview(current);
            (void)queue.drain();
            (void)gui::render_deferred_batch(current.get());
            raylibcontext::raylib_present();
            return raylibcontext::raylib_is_running();
        };

        ctx->clear = nullptr;
        ctx->present = nullptr;
        ctx->get_width = raylibcontext::raylib_get_width;
        ctx->get_height = raylibcontext::raylib_get_height;
        ctx->draw_sprite = raylibrenderer::draw_sprite;
        ctx->add_texture = &detail::default_add_texture;
        ctx->add_atlas = +[](const TextureAtlas& atlas) { return detail::default_add_atlas(atlas); };
        detail::bind_default_input(ctx);
    }
}
#endif
