#include <algorithm>
#include <atomic>
#include <memory>
#include <string>

#include <include/aengine.config.hpp>

#if defined(EPOCH_USING_SDL) && (EPOCH_USING_SDL == 1)
#include <SDL3/SDL.h>
#endif

#include "core_context_backends.hpp"

import core.context;

#if defined(EPOCH_USING_SDL) && (EPOCH_USING_SDL == 1)
import aengine.gui;
import aengine.input;
import atlas.texture;
import context.commandqueue;
import image.loader;
import sdl.renderer;
import sdl.textures;

namespace
{
    SDL_Window* s_window = nullptr;
    SDL_Renderer* s_renderer = nullptr;
    bool s_running = false;
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
            SDL_GetWindowSize(s_window, &s_width, &s_height);

        s_width = (std::max)(1, s_width);
        s_height = (std::max)(1, s_height);

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
                ctx->windowData->sdl_window = s_window;
                ctx->windowData->width = s_width;
                ctx->windowData->height = s_height;
            }
        }
    }

    void sdl_initialize_adapter()
    {
        auto ctx = epochnamespace::core::get_current_render_context();
        if (!ctx)
            return;

        s_width = (std::max)(1, ctx->width);
        s_height = (std::max)(1, ctx->height);

        if (!s_running)
        {
            if (static_cast<int>(SDL_Init(SDL_INIT_VIDEO)) < 0)
                return;

            s_window = SDL_CreateWindow(
                ctx->backendName.empty() ? "SDL" : ctx->backendName.c_str(),
                s_width,
                s_height,
                SDL_WINDOW_RESIZABLE);
            if (!s_window)
                return;

            s_renderer = SDL_CreateRenderer(s_window, nullptr);
            if (!s_renderer)
            {
                SDL_DestroyWindow(s_window);
                s_window = nullptr;
                return;
            }

            epochnamespace::sdlcontext::init_renderer(s_renderer);
            s_running = true;
        }
        else if (s_window)
        {
            SDL_SetWindowTitle(s_window, ctx->backendName.c_str());
            SDL_SetWindowSize(s_window, s_width, s_height);
        }

        refresh_dimensions(ctx);
    }

    void sdl_cleanup_adapter()
    {
        s_running = false;

        if (s_renderer)
        {
            SDL_DestroyRenderer(s_renderer);
            s_renderer = nullptr;
        }

        if (s_window)
        {
            SDL_DestroyWindow(s_window);
            s_window = nullptr;
        }

        SDL_Quit();
    }

    bool sdl_process_adapter(
        std::shared_ptr<epochnamespace::core::Context> ctx,
        epochnamespace::core::CommandQueue& queue)
    {
        if (!ctx || !s_running || !s_window || !s_renderer)
            return false;

        SDL_Event event{};
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_EVENT_QUIT)
                return false;

            if (event.type == SDL_EVENT_WINDOW_RESIZED)
            {
                refresh_dimensions(ctx);
                if (ctx->onResize)
                    ctx->onResize(ctx->framebufferWidth, ctx->framebufferHeight);
            }
        }

        refresh_dimensions(ctx);
        epochnamespace::sdlcontext::begin_frame();
        SDL_RenderClear(s_renderer);
        (void)queue.drain();
        (void)epochnamespace::gui::render_deferred_batch(ctx.get());
        epochnamespace::sdlcontext::end_frame();
        return true;
    }
}

namespace epochnamespace::core::detail
{
    void register_sdl_backend()
    {
        auto ctx = std::make_shared<Context>();
        ctx->type = ContextType::SDL;
        ctx->backendName = "SDL";
        ctx->initialize = sdl_initialize_adapter;
        ctx->cleanup = sdl_cleanup_adapter;
        ctx->process = sdl_process_adapter;
        ctx->clear = nullptr;
        ctx->present = nullptr;
        ctx->get_width = []() { return s_width; };
        ctx->get_height = []() { return s_height; };
        ctx->draw_sprite = epochnamespace::sdltextures::draw_sprite;
        ctx->add_texture = &default_add_texture;
        ctx->add_atlas = +[](const epochnamespace::TextureAtlas& atlas)
        {
            return default_add_atlas(atlas);
        };
        bind_default_input(ctx);
        AddContextForBackend(ContextType::SDL, std::move(ctx));
    }
}
#endif
