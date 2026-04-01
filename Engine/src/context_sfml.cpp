#include <algorithm>
#include <atomic>
#include <memory>
#include <string>

#include <include/aengine.config.hpp>

#if defined(EPOCH_USING_SFML) && (EPOCH_USING_SFML == 1)
#define SFML_STATIC
#include <SFML/Graphics.hpp>
#endif

#include "core_context_backends.hpp"

import core.context;

#if defined(EPOCH_USING_SFML) && (EPOCH_USING_SFML == 1)
import aengine.gui;
import aengine.input;
import atlas.texture;
import context.commandqueue;
import image.loader;
import sfml.state;
import sfml.textures;

namespace
{
    std::unique_ptr<sf::RenderWindow> s_window{};
    int s_width = 0;
    int s_height = 0;

    std::uint32_t default_add_texture(
        epochnamespace::TextureAtlas&,
        std::string,
        const epochnamespace::ImageData&) noexcept
    {
        return 0u;
    }

    std::uint32_t default_add_atlas(const epochnamespace::TextureAtlas& atlas) noexcept
    {
        const int idx = atlas.get_index();
        return static_cast<std::uint32_t>(idx >= 0 ? idx + 1 : 1);
    }

    void bind_default_input(const std::shared_ptr<epochnamespace::core::Context>& ctx)
    {
        ctx->is_key_held = [](epochnamespace::input::Key key) { return epochnamespace::input::is_key_held(key); };
        ctx->is_key_down = [](epochnamespace::input::Key key) { return epochnamespace::input::is_key_down(key); };
        ctx->get_mouse_position = [](int& x, int& y)
        {
            x = epochnamespace::input::mouseX.load(std::memory_order_relaxed);
            y = epochnamespace::input::mouseY.load(std::memory_order_relaxed);
        };
        ctx->is_mouse_button_held = [](epochnamespace::input::MouseButton button) { return epochnamespace::input::is_mouse_button_held(button); };
        ctx->is_mouse_button_down = [](epochnamespace::input::MouseButton button) { return epochnamespace::input::is_mouse_button_down(button); };
    }

    void refresh_dimensions(const std::shared_ptr<epochnamespace::core::Context>& ctx) noexcept
    {
        if (s_window)
        {
            const auto size = s_window->getSize();
            s_width = static_cast<int>((std::max)(1u, size.x));
            s_height = static_cast<int>((std::max)(1u, size.y));
        }

        if (ctx)
        {
            ctx->width = s_width;
            ctx->height = s_height;
            ctx->virtualWidth = s_width;
            ctx->virtualHeight = s_height;
            ctx->framebufferWidth = s_width;
            ctx->framebufferHeight = s_height;
            if (ctx->windowData)
            {
                ctx->windowData->sfml_window = s_window.get();
                ctx->windowData->width = s_width;
                ctx->windowData->height = s_height;
            }
        }

        auto& state = epochnamespace::sfmlcontext::state::s_sfmlstate;
        state.window.sfml_window = s_window.get();
        state.set_dimensions(s_width, s_height);
        state.running = (s_window && s_window->isOpen());
    }

    void sfml_initialize_adapter()
    {
        auto ctx = epochnamespace::core::get_current_render_context();
        if (!ctx)
            return;

        s_width = (std::max)(1, ctx->width);
        s_height = (std::max)(1, ctx->height);

        if (!s_window || !s_window->isOpen())
        {
            s_window = std::make_unique<sf::RenderWindow>(
                sf::VideoMode(static_cast<unsigned>(s_width), static_cast<unsigned>(s_height)),
                ctx->backendName.empty() ? "SFML" : ctx->backendName);
            s_window->setFramerateLimit(60);
        }
        else
        {
            s_window->setTitle(ctx->backendName);
            s_window->setSize({ static_cast<unsigned>(s_width), static_cast<unsigned>(s_height) });
        }

        refresh_dimensions(ctx);
    }

    void sfml_cleanup_adapter()
    {
        if (s_window)
        {
            s_window->close();
            s_window.reset();
        }

        auto& state = epochnamespace::sfmlcontext::state::s_sfmlstate;
        state.window.sfml_window = nullptr;
        state.running = false;
    }

    bool sfml_process_adapter(
        std::shared_ptr<epochnamespace::core::Context> ctx,
        epochnamespace::core::CommandQueue& queue)
    {
        if (!ctx || !s_window || !s_window->isOpen())
            return false;

        sf::Event event{};
        while (s_window->pollEvent(event))
        {
            if (event.type == sf::Event::Closed)
                return false;

            if (event.type == sf::Event::Resized)
            {
                refresh_dimensions(ctx);
                if (ctx->onResize)
                    ctx->onResize(ctx->framebufferWidth, ctx->framebufferHeight);
            }
        }

        refresh_dimensions(ctx);
        s_window->clear(sf::Color(0, 0, 0));
        (void)queue.drain();
        (void)epochnamespace::gui::render_deferred_batch(ctx.get());
        s_window->display();
        return s_window->isOpen();
    }
}

namespace epochnamespace::core::detail
{
    void register_sfml_backend()
    {
        auto ctx = std::make_shared<Context>();
        ctx->type = ContextType::SFML;
        ctx->backendName = "SFML";
        ctx->initialize = sfml_initialize_adapter;
        ctx->cleanup = sfml_cleanup_adapter;
        ctx->process = sfml_process_adapter;
        ctx->clear = nullptr;
        ctx->present = nullptr;
        ctx->get_width = []() { return s_width; };
        ctx->get_height = []() { return s_height; };
        ctx->draw_sprite = epochnamespace::sfmlcontext::draw_sprite;
        ctx->add_texture = &default_add_texture;
        ctx->add_atlas = +[](const epochnamespace::TextureAtlas& atlas)
        {
            return default_add_atlas(atlas);
        };
        bind_default_input(ctx);
        AddContextForBackend(ContextType::SFML, std::move(ctx));
    }
}
#endif
