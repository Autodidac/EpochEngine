// acontext.sfml.state.ixx
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

#include "aengine.hpp"          // DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT
#include "aengine.config.hpp"   // ALMOND_USING_* macros

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
// Prefer targeted headers over the umbrella to reduce accidental includes.
#   include <SFML/Graphics/RenderWindow.hpp>
#   include <SFML/Window/Keyboard.hpp>
#   include <SFML/Window/Mouse.hpp>
#endif

export module acontext.sfml.state;

import aengine.platform;
import aengine.context.window;
import aengine.core.time;

import <array>;
import <bitset>;
import <cstddef>;
import <functional>;

export namespace almondnamespace::sfmlcontext::state
{
#if defined(ALMOND_USING_SFML)

    // SFML3 note:
    // Key/button enums are not guaranteed to remain simple "0..Count-1" ints forever.
    // We keep internal state arrays sized to conservative constants.
    // If you want exact mapping, implement translation functions at the input layer.
    inline constexpr std::size_t kMaxMouseButtons = 16;
    inline constexpr std::size_t kMaxKeys = 256;

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
        int  screenWidth{ DEFAULT_WINDOW_WIDTH };
        int  screenHeight{ DEFAULT_WINDOW_HEIGHT };
        bool running{ false };

        std::function<void(int, int)> onResize{};

        struct MouseState
        {
            std::array<bool, kMaxMouseButtons> down{};
            std::array<bool, kMaxMouseButtons> pressed{};
            std::array<bool, kMaxMouseButtons> prevDown{};
            int lastX = 0;
            int lastY = 0;

            static constexpr std::size_t idx(sf::Mouse::Button b) noexcept
            {
                // SFML 3 uses enum class; cast through underlying type.
                const auto i = static_cast<std::size_t>(b);
                return (i < kMaxMouseButtons) ? i : (kMaxMouseButtons - 1);
            }

            void begin_frame() noexcept
            {
                pressed.fill(false);
            }

            void update_pressed() noexcept
            {
                for (std::size_t i = 0; i < kMaxMouseButtons; ++i)
                    pressed[i] = down[i] && !prevDown[i];
                prevDown = down;
            }
        } mouse{};

        struct KeyboardState
        {
            std::bitset<kMaxKeys> down{};
            std::bitset<kMaxKeys> pressed{};
            std::bitset<kMaxKeys> prevDown{};

            static constexpr std::size_t idx(sf::Keyboard::Key k) noexcept
            {
                const auto i = static_cast<std::size_t>(k);
                return (i < kMaxKeys) ? i : (kMaxKeys - 1);
            }

            void begin_frame() noexcept
            {
                pressed.reset();
            }

            void update_pressed() noexcept
            {
                pressed = down & (~prevDown);
                prevDown = down;
            }
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
} // namespace almondnamespace::sfmlcontext::state
