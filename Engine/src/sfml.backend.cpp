module;

#include <algorithm>
#include <atomic>
#include <exception>
#include <memory>
#include <source_location>
#include <string>

#if defined(_WIN32)
#   ifndef WIN32_LEAN_AND_MEAN
#       define WIN32_LEAN_AND_MEAN
#   endif
#   ifndef NOMINMAX
#       define NOMINMAX
#   endif
#   include <windows.h>
#endif

#include <include/engine.config.hpp>

module sfml.backend;

#if defined(EPOCH_USING_SFML) && (EPOCH_USING_SFML == 1)
import engine.input;
import atlas.texture;
import core.context;
import core.logger;
import image.loader;
import sfml.context;
import sfml.textures;

namespace epochnamespace::sfmlbackend
{
    namespace detail
    {
        constexpr std::string_view kLogSfml = "Context.SFML";

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
                std::string windowTitle{};
                if (current->windowData)
                    windowTitle = current->windowData->titleNarrow;

#if defined(_WIN32)
                (void)sfmlcontext::sfml_initialize(
                    current,
                    reinterpret_cast<HWND>(current->get_hwnd()),
                    static_cast<unsigned>((std::max)(1, current->width)),
                    static_cast<unsigned>((std::max)(1, current->height)),
                    current->onResize,
                    windowTitle);
#else
                (void)sfmlcontext::sfml_initialize(
                    current,
                    current->get_hwnd(),
                    static_cast<unsigned>((std::max)(1, current->width)),
                    static_cast<unsigned>((std::max)(1, current->height)),
                    current->onResize,
                    windowTitle);
#endif
            }
            catch (const std::exception& e)
            {
                logger::get(detail::kLogSfml).logf(
                    logger::LogLevel::Error,
                    std::source_location::current(),
                    "init exception: {}",
                    e.what());
            }
            catch (...)
            {
                logger::get(detail::kLogSfml).log(
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

            auto copy = current;
            sfmlcontext::sfml_cleanup(copy);
        };

        ctx->process = [](std::shared_ptr<core::Context> current, core::CommandQueue& queue)
        {
            if (!current)
                return false;
            return sfmlcontext::sfml_process(current, queue);
        };

        ctx->draw_sprite = sfmlcontext::draw_sprite;
        ctx->add_texture = &detail::default_add_texture;
        ctx->add_atlas = +[](const TextureAtlas& atlas) { return detail::default_add_atlas(atlas); };
        detail::bind_default_input(ctx);
    }
}
#endif
