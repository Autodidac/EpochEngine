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

#include "core.format_text.hpp"

#include <cstdint>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <SDL3/SDL.h>

#include <include/engine.config.hpp> // for EPOCH_USING Macros 		// for EPOCH_USING_SDL

extern "C" const char* epoch_reserve_capture_path_utf8(
    const char* backend,
    std::uintptr_t windowId);
extern "C" void epoch_release_capture_path_utf8(
    const char* backend,
    std::uintptr_t windowId);

export module sdl.renderer;

//import engine.config;

#if defined(EPOCH_USING_SDL) && (EPOCH_USING_SDL == 1)
import core.context;
import core.logger;
import context.type;
import sdl.state;

export namespace epochengine::sdlcontext
{
   // using epochengine::sdlcontext::state::SDL3State::s_sdlstate;

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

    struct FramePresentationResult final
    {
        bool capture_reserved{};
        bool capture_written{};
        bool present_succeeded{};
    };

    inline void check_sdl_error(const char* location)
    {
        const char* err = SDL_GetError();
        if (err && *err) {
            logger::error("SDL", epochengine::format_text("{}: {}", location, err));
            SDL_ClearError();
        }
    }

    inline void init_renderer(SDL_Renderer* renderer)
    {
        if (!renderer) {
            throw std::runtime_error("SDL_Renderer is null");
        }
        sdl_renderer.renderer = renderer;
        const auto color = epochengine::core::clear_color_for_context(
            epochengine::core::ContextType::SDL);
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
        if (!sdl_renderer.renderer || epochengine::sdlcontext::state::get_sdl_state().renderFaulted)
            return;
    }

    [[nodiscard]] FramePresentationResult present_frame(
        const int width,
        const int height,
        const std::uintptr_t windowId)
    {
        FramePresentationResult result{};
        if (!sdl_renderer.renderer
            || epochengine::sdlcontext::state::get_sdl_state().renderFaulted)
        {
            return result;
        }

        const char* const capturePath =
            epoch_reserve_capture_path_utf8("sdl", windowId);
        result.capture_reserved =
            capturePath != nullptr && capturePath[0] != '\0';
        if (result.capture_reserved && width > 0 && height > 0)
        {
            SDL_Surface* surface = SDL_RenderReadPixels(
                sdl_renderer.renderer, nullptr);
            if (surface)
            {
                const bool dimensionsMatch =
                    surface->w == width && surface->h == height;
                result.capture_written = dimensionsMatch
                    && SDL_SaveBMP(surface, capturePath);
                SDL_DestroySurface(surface);
            }
            else
            {
                check_sdl_error("SDL_RenderReadPixels");
            }
        }

        result.present_succeeded = SDL_RenderPresent(sdl_renderer.renderer);
        if (!result.present_succeeded)
        {
            check_sdl_error("SDL_RenderPresent");
            epochengine::sdlcontext::state::get_sdl_state().renderFaulted = true;
        }

        if (result.capture_reserved && !result.capture_written)
            epoch_release_capture_path_utf8("sdl", windowId);

        if (result.capture_written)
        {
            logger::info(
                "SDL",
                std::string{"Captured frame to "} + capturePath);
        }
        else if (result.capture_reserved)
        {
            logger::warn(
                "SDL",
                std::string{"Failed to capture frame to "} + capturePath);
        }

        return result;
    }

    inline void end_frame()
    {
        if (!sdl_renderer.renderer || epochengine::sdlcontext::state::get_sdl_state().renderFaulted)
            return;

        if (!SDL_RenderPresent(sdl_renderer.renderer))
        {
            check_sdl_error("SDL_RenderPresent");
            epochengine::sdlcontext::state::get_sdl_state().renderFaulted = true;
        }
    }
}

#endif
