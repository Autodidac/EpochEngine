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


#if defined(ALMOND_USING_SFML)
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

export namespace almondnamespace::sfmlcontext::state
{
#if defined(ALMOND_USING_SFML)
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

        almondnamespace::contextwindow::WindowData window{};

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

        almondnamespace::timing::Timer pollTimer = almondnamespace::timing::createTimer(1.0);
        almondnamespace::timing::Timer fpsTimer = almondnamespace::timing::createTimer(1.0);
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
