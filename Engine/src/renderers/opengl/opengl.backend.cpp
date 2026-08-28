module;

#include <algorithm>
#include <atomic>
#include <exception>
#include <memory>
#include <source_location>
#include <string>
#include <string_view>

#include <include/engine.config.hpp>

module opengl.backend;

#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)
import input.engine;
import atlas.manager;
import atlas.texture;
import context.commandqueue;
import context.type;
import core.logger;
import image.loader;
import opengl.context;
import opengl.platform;
import opengl.textures;

namespace epochengine::openglbackend
{
    namespace detail
    {
        constexpr std::string_view kLogOpenGL = "Context.OpenGL";

        std::uint32_t default_add_texture(TextureAtlas&, std::string, const ImageData&) noexcept
        {
            return 0u;
        }

        std::uint32_t default_add_atlas(const TextureAtlas& atlas) noexcept
        {
            try
            {
                atlasmanager::ensure_uploaded(atlas);
                atlasmanager::process_pending_uploads(core::ContextType::OpenGL);
            }
            catch (...)
            {
            }

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

        void initialize_adapter()
        {
            auto current = core::get_current_render_context();
            if (!current)
                return;

            const auto native = native_window_handle(current);
            const unsigned w = static_cast<unsigned>((std::max)(1, current->width));
            const unsigned h = static_cast<unsigned>((std::max)(1, current->height));

            current->init_failed = false;
            try
            {
                current->init_failed = !openglcontext::opengl_initialize(current, native, w, h, current->onResize);
            }
            catch (const std::exception& e)
            {
                current->init_failed = true;
                logger::get(kLogOpenGL).logf(
                    logger::LogLevel::Error,
                    std::source_location::current(),
                    "init exception: {}",
                    e.what());
            }
            catch (...)
            {
                current->init_failed = true;
                logger::get(kLogOpenGL).log(
                    logger::LogLevel::Error,
                    "init unknown exception",
                    std::source_location::current());
            }
        }

        void cleanup_adapter()
        {
            auto current = core::get_current_render_context();
            if (!current)
                return;

            try
            {
                openglcontext::opengl_cleanup(current);
            }
            catch (const std::exception& e)
            {
                current->init_failed = true;
                logger::get(kLogOpenGL).logf(
                    logger::LogLevel::Error,
                    std::source_location::current(),
                    "cleanup exception: {}",
                    e.what());
            }
            catch (...)
            {
                current->init_failed = true;
                logger::get(kLogOpenGL).log(
                    logger::LogLevel::Error,
                    "cleanup unknown exception",
                    std::source_location::current());
            }
        }

        bool process_adapter(
            std::shared_ptr<core::Context> ctx,
            core::CommandQueue& queue)
        {
            if (!ctx)
                return false;
            if (ctx->windowData && ctx->windowData->get_should_close())
            {
                queue.clear();
                return false;
            }
            return openglcontext::opengl_process(ctx, queue);
        }
    }

    void configure(const std::shared_ptr<core::Context>& ctx)
    {
        if (!ctx)
            return;

        ctx->initialize = detail::initialize_adapter;
        ctx->cleanup = detail::cleanup_adapter;
        ctx->process = detail::process_adapter;
        ctx->clear = openglcontext::opengl_clear;
        ctx->present = nullptr;
        ctx->get_width = openglcontext::opengl_get_width;
        ctx->get_height = openglcontext::opengl_get_height;
        ctx->frame_pacing_capabilities = {true, false};
        ctx->apply_frame_pacing = [](
            const perf::frame_pacing_mode mode,
            double)
        {
            const bool wantsVsync = mode == perf::frame_pacing_mode::vsync;
            const bool requested = openglcontext::PlatformGL::set_swap_interval(
                wantsVsync ? 1 : 0);
            const auto reported = openglcontext::PlatformGL::query_swap_interval();
            const bool active = reported.available
                ? reported.interval > 0
                : requested && wantsVsync;
            const bool configured = requested
                && (!reported.available
                    || (wantsVsync ? active : !active));
            if (!configured && reported.available && active)
            {
                return perf::native_frame_pacing_result{
                    false,
                    true,
                    perf::frame_pacing_mode::vsync,
                    0.0};
            }
            return perf::native_frame_pacing_result{
                configured,
                configured && wantsVsync,
                wantsVsync
                    ? perf::frame_pacing_mode::vsync
                    : perf::frame_pacing_mode::uncapped,
                0.0};
        };
        ctx->draw_sprite = opengltextures::draw_sprite;
        ctx->add_texture = &detail::default_add_texture;
        ctx->add_atlas = +[](const TextureAtlas& atlas) { return detail::default_add_atlas(atlas); };
        detail::bind_default_input(ctx);
    }
}
#endif
