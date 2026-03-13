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

// Must be before anything that might pull <windows.h> (directly or indirectly)
#if defined(_WIN32)
#   ifndef WIN32_LEAN_AND_MEAN
#       define WIN32_LEAN_AND_MEAN
#   endif
#   ifndef NOMINMAX
#       define NOMINMAX
#   endif
#endif

#include "../include/aengine.hpp"          // DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT
#include "../include/aengine.config.hpp"   // ALMOND_USING_* macros

// If some include above already pulled windows.h, nuke the macros anyway.
#if defined(_WIN32)
#   ifdef min
#       undef min
#   endif
#   ifdef max
#       undef max
#   endif
#endif


#if defined(ALMOND_USING_SFML) && (ALMOND_USING_SFML == 1)
#define SFML_STATIC
#include <SFML/Graphics.hpp>
#endif

export module acontext.sfml.state;

import aengine.platform;
import aengine.context.window;
import aengine.core.time;

import <array>;
import <bitset>;
import <functional>;

export namespace epochnamespace::sfmlcontext::state
{
#if defined(ALMOND_USING_SFML) && (ALMOND_USING_SFML == 1)
    struct SFML3State
    {
        SFML3State()
        {
            window.width = DEFAULT_WINDOW_WIDTH;
            window.height = DEFAULT_WINDOW_HEIGHT;
            window.should_close = false;
            screenWidth = window.width;
            screenHeight = window.height;
        }

        epochnamespace::contextwindow::WindowData window{};

        bool shouldClose{ false };
        int screenWidth{ DEFAULT_WINDOW_WIDTH };
        int screenHeight{ DEFAULT_WINDOW_HEIGHT };
        bool running{ false };

        std::function<void(int, int)> onResize{};

        struct MouseState
        {
            std::array<bool, static_cast<std::size_t>(sf::Mouse::ButtonCount)> down{};
            std::array<bool, static_cast<std::size_t>(sf::Mouse::ButtonCount)> pressed{};
            std::array<bool, static_cast<std::size_t>(sf::Mouse::ButtonCount)> prevDown{};
            int lastX = 0;
            int lastY = 0;
        } mouse{};

        struct KeyboardState
        {
            std::bitset<sf::Keyboard::KeyCount> down;
            std::bitset<sf::Keyboard::KeyCount> pressed;
            std::bitset<sf::Keyboard::KeyCount> prevDown;
        } keyboard{};

        epochnamespace::timing::Timer pollTimer = epochnamespace::timing::createTimer(1.0);
        epochnamespace::timing::Timer fpsTimer = epochnamespace::timing::createTimer(1.0);
        int frameCount = 0;

        [[nodiscard]] sf::RenderWindow* get_sfml_window() const noexcept
        {
            return window.sfml_window;
        }

        void mark_should_close(bool value) noexcept
        {
            shouldClose = value;
            window.should_close = value;
        }

        void set_dimensions(int w, int h) noexcept
        {
            screenWidth = w;
            screenHeight = h;
            window.set_size(w, h);
        }
    };

    inline SFML3State s_sfmlstate{};
#endif // ALMOND_USING_SFML
}
