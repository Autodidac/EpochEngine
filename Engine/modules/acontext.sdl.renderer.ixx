module;

#include <include/aengine.config.hpp> // for ALMOND_USING Macros 		// for ALMOND_USING_SDL

export module acontext.sdl.renderer;

//import aengine.config;

#if defined(ALMOND_USING_SDL)
import std;
import <iostream>;
import <stdexcept>;
import <SDL3/SDL.h>;

import aengine.core.context;
import acontext.sdl.state;

export namespace almondnamespace::sdlcontext
{
   // using almondnamespace::sdlcontext::state::SDL3State::s_sdlstate;

    struct RendererContext
    {
        enum class RenderMode {
            SingleTexture,
            TextureAtlas
        };
        RenderMode mode = RenderMode::TextureAtlas;

        SDL_Renderer* renderer{ nullptr };
    };

    inline RendererContext sdl_renderer{};

    inline void check_sdl_error(const char* location)
    {
        const char* err = SDL_GetError();
        if (err && *err) {
            std::cerr << "[SDL ERROR] " << location << ": " << err << "\n";
            SDL_ClearError();
        }
    }

    inline void init_renderer(SDL_Renderer* renderer)
    {
        if (!renderer) {
            throw std::runtime_error("SDL_Renderer is null");
        }
        sdl_renderer.renderer = renderer;
        const auto color = almondnamespace::core::clear_color_for_context(
            almondnamespace::core::ContextType::SDL);
        SDL_SetRenderDrawColor(
            sdl_renderer.renderer,
            static_cast<Uint8>(color[0] * 255.0f),
            static_cast<Uint8>(color[1] * 255.0f),
            static_cast<Uint8>(color[2] * 255.0f),
            static_cast<Uint8>(color[3] * 255.0f));
        check_sdl_error("init_renderer");
    }

    inline void begin_frame()
    {
        const auto color = almondnamespace::core::clear_color_for_context(
            almondnamespace::core::ContextType::SDL);
        SDL_SetRenderDrawColor(
            sdl_renderer.renderer,
            static_cast<Uint8>(color[0] * 255.0f),
            static_cast<Uint8>(color[1] * 255.0f),
            static_cast<Uint8>(color[2] * 255.0f),
            static_cast<Uint8>(color[3] * 255.0f));
        SDL_RenderClear(sdl_renderer.renderer);
    }

    inline void end_frame()
    {
        SDL_RenderPresent(sdl_renderer.renderer);
    }
}

#endif
