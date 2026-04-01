module;

#include <algorithm>
#include <atomic>
#include <exception>
#include <memory>
#include <source_location>
#include <string>
#include <string_view>

#if defined(_WIN32)
#   ifndef WIN32_LEAN_AND_MEAN
#       define WIN32_LEAN_AND_MEAN
#   endif
#   ifndef NOMINMAX
#       define NOMINMAX
#   endif
#   include <windows.h>
#endif

#include <include/aengine.config.hpp>

#ifdef min
#   undef min
#endif
#ifdef max
#   undef max
#endif

module sdl.backend;

#if defined(EPOCH_USING_SDL) && (EPOCH_USING_SDL == 1)
import aengine.input;
import atlas.manager;
import atlas.texture;
import core.logger;
import image.loader;
import sdl.context;
import sdl.textures;

namespace epochnamespace::sdlbackend
{
    namespace detail
    {
        constexpr std::string_view kLogSdl = "Context.SDL";

        std::uint32_t default_add_texture(TextureAtlas&, std::string, const ImageData&) noexcept
        {
            return 0u;
        }

        std::uint32_t default_add_atlas(const TextureAtlas& atlas) noexcept
        {
            try
            {
                atlasmanager::ensure_uploaded(atlas);
                atlasmanager::process_pending_uploads(core::ContextType::SDL);
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

            try
            {
#if defined(_WIN32)
                (void)sdlcontext::sdl_initialize(
                    current,
                    reinterpret_cast<HWND>(native_window_handle(current)),
                    static_cast<int>((std::max)(1, current->width)),
                    static_cast<int>((std::max)(1, current->height)),
                    current->onResize,
                    current->backendName);
#else
                (void)sdlcontext::sdl_initialize(
                    current,
                    native_window_handle(current),
                    static_cast<int>((std::max)(1, current->width)),
                    static_cast<int>((std::max)(1, current->height)),
                    current->onResize,
                    current->backendName);
#endif
            }
            catch (const std::exception& e)
            {
                logger::get(kLogSdl).logf(
                    logger::LogLevel::Error,
                    std::source_location::current(),
                    "init exception: {}",
                    e.what());
            }
            catch (...)
            {
                logger::get(kLogSdl).log(
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
                auto copy = current;
                sdlcontext::sdl_cleanup(copy);
            }
            catch (const std::exception& e)
            {
                logger::get(kLogSdl).logf(
                    logger::LogLevel::Error,
                    std::source_location::current(),
                    "cleanup exception: {}",
                    e.what());
            }
            catch (...)
            {
                logger::get(kLogSdl).log(
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
            return sdlcontext::sdl_process(ctx, queue);
        }
    }

    void configure(const std::shared_ptr<core::Context>& ctx)
    {
        if (!ctx)
            return;

        ctx->initialize = detail::initialize_adapter;
        ctx->cleanup = detail::cleanup_adapter;
        ctx->process = detail::process_adapter;
        ctx->draw_sprite = sdltextures::draw_sprite;
        ctx->add_texture = &detail::default_add_texture;
        ctx->add_atlas = +[](const TextureAtlas& atlas) { return detail::default_add_atlas(atlas); };
        detail::bind_default_input(ctx);
    }
}
#endif
