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
#endif

export module raylib.context_linux;

#if !defined(_WIN32) \
    && defined(EPOCH_USING_RAYLIB) \
    && (EPOCH_USING_RAYLIB == 1)
import raylib.api;

export namespace epochengine::raylibcontext::linux
{
    [[nodiscard]] inline GLFWwindow* native_window() noexcept
    {
        return static_cast<GLFWwindow*>(
            epochengine::raylib_api::get_window_handle());
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
