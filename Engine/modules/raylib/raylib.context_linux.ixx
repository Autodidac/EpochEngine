/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <include/engine.config.hpp>

#if !defined(_WIN32) \
    && defined(EPOCH_USING_RAYLIB) \
    && (EPOCH_USING_RAYLIB == 1)
#  ifndef GLFW_INCLUDE_NONE
#    define GLFW_INCLUDE_NONE
#  endif
#  include <GLFW/glfw3.h>
#  include <thread>
#endif

export module raylib.context_linux;

#if !defined(_WIN32) \
    && defined(EPOCH_USING_RAYLIB) \
    && (EPOCH_USING_RAYLIB == 1)
import raylib.api;
import raylib.state;

export namespace epochengine::raylibcontext::linux
{
    [[nodiscard]] inline GLFWwindow* native_window() noexcept
    {
        return static_cast<GLFWwindow*>(
            epochengine::raylibstate::s_raylibstate.hglrc);
    }

    [[nodiscard]] inline bool capture_native_context() noexcept
    {
        auto& state = epochengine::raylibstate::s_raylibstate;
        if (!epochengine::raylib_api::is_window_ready() || !state.owner_ctx
            || state.owner_thread != std::this_thread::get_id())
        {
            return false;
        }

        // InitWindow has just made Raylib's GLFW context current on its owner.
        // Raylib 6 GetWindowHandle returns native X11/Wayland data, not GLFWwindow*.
        GLFWwindow* const current = glfwGetCurrentContext();
        if (!current || (state.hglrc && state.hglrc != current))
            return false;
        state.hglrc = current;
        return true;
    }

    [[nodiscard]] inline bool native_context_is_current() noexcept
    {
        GLFWwindow* const expected = native_window();
        return expected && glfwGetCurrentContext() == expected;
    }

    [[nodiscard]] inline bool make_native_context_current() noexcept
    {
        GLFWwindow* const expected = native_window();
        if (!expected)
            return false;
        if (glfwGetCurrentContext() != expected)
            glfwMakeContextCurrent(expected);
        return glfwGetCurrentContext() == expected;
    }
}
#endif
