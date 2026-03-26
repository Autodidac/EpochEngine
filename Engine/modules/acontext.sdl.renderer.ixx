/************************************************
 *  ███████╗██████╗  ██████╗  ██████╗██╗  ██╗   *
 *  ██╔════╝██╔══██╗██╔═══██╗██╔════╝██║  ██║   *
 *  █████╗  ██████╔╝██║   ██║██║     ███████║   *
 *  ██╔══╝  ██╔═══╝ ██║   ██║██║     ██╔══██║   *
 *  ███████╗██║     ╚██████╔╝╚██████╗██║  ██║   *
 *  ╚══════╝╚═╝      ╚═════╝  ╚═════╝╚═╝  ╚═╝   *
 *                                              *
 *   This file is part of the Epoch   Project.  *
 *   epochengine - Modular C++ Framework        *
 *                                              *
 *   SPDX-License-Identifier:                   *
 *   LicenseRef-MIT-NoSell                      *
 *                                              *
 *   Provided "AS IS", without warranty         *
 *   of any kind.                               *
 *                                              *
 *   Use permitted for Non-Commercial           *
 *   Purposes ONLY, without prior               *
 *   commercial licensing agreement.            *
 *                                              *
 *   Redistribution Allowed with This Notice    *
 *   and LICENSE file.                          *
 *                                              *
 *   No obligation to disclose                  *
 *   modifications.                             *
 *                                              *
 *   See LICENSE file for full terms.           *
 *                                              *
 ***********************************************/

module;

#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <SDL3/SDL.h>

#include <include/aengine.config.hpp> // for EPOCH_USING Macros 		// for EPOCH_USING_SDL

export module acontext.sdl.renderer;

//import aengine.config;

#if defined(EPOCH_USING_SDL) && (EPOCH_USING_SDL == 1)
import aengine.core.context;
import acontext.sdl.state;

export namespace epochnamespace::sdlcontext
{
   // using epochnamespace::sdlcontext::state::SDL3State::s_sdlstate;

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
        const auto color = epochnamespace::core::clear_color_for_context(
            epochnamespace::core::ContextType::SDL);
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
        if (!sdl_renderer.renderer || epochnamespace::sdlcontext::state::get_sdl_state().renderFaulted)
            return;
    }

    inline void end_frame()
    {
        if (!sdl_renderer.renderer || epochnamespace::sdlcontext::state::get_sdl_state().renderFaulted)
            return;

        if (!SDL_RenderPresent(sdl_renderer.renderer))
        {
            check_sdl_error("SDL_RenderPresent");
            epochnamespace::sdlcontext::state::get_sdl_state().renderFaulted = true;
        }
    }
}

#endif
