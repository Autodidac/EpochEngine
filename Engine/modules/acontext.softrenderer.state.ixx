/**************************************************************
 *   █████╗ ██╗     ███╗   ███╗   ███╗   ██╗    ██╗██████╗    *
 *  ██╔══██╗██║     ████╗ ████║ ██╔═══██╗████╗  ██║██╔══██╗   *
 *  ███████║██║     ██╔████╔██║ ██║   ██║██╔██╗ ██║██║  ██║   *
 *  ██╔══██║██║     ██║╚██╔╝██║ ██║   ██║██║╚██╗██║██║  ██║   *
 *  ██║  ██║███████╗██║ ╚═╝ ██║ ╚██████╔╝██║ ╚████║██████╔╝   *
 *  ╚═╝  ╚═╝╚══════╝╚═╝     ╚═╝  ╚═════╝ ╚═╝  ╚═══╝╚═════╝    *
 *                                                            *
 *   This file is part of the Almond Project.                 *
 *   AlmondEngine - Modular C++ Game Engine                   *
 *                                                            *
 *   SPDX-License-Identifier: LicenseRef-MIT-NoSell           *
 *                                                            *
 *   Provided "AS IS", without warranty of any kind.          *
 *   Use permitted for non-commercial purposes only           *
 *   without prior commercial licensing agreement.            *
 *                                                            *
 *   Redistribution allowed with this notice.                 *
 *   No obligation to disclose modifications.                 *
 *   See LICENSE file for full terms.                         *
 **************************************************************/
 // SoftRenderer - State  (C++23 module)

module;

//#include "aplatform.hpp"

#include <include/aengine.config.hpp> // for ALMOND_USING Macros   // may bring in <windows.h>, etc.
//#include "arobusttime.hpp"     // time::Timer, time::createTimer(...)
#if defined(ALMOND_USING_SOFTWARE_RENDERER)
#   if defined(_WIN32)
#       ifdef ALMOND_USING_WINMAIN
#         include <aframework.hpp>
#       endif
#       ifndef WIN32_LEAN_AND_MEAN
#           define WIN32_LEAN_AND_MEAN
#       endif
#   endif
#endif

export module acontext.softrenderer.state;

import <array>;
import <bitset>;
import <cstdint>;
import <functional>;
import <memory>;
import <vector>;

//import aengine.platform;  
import aengine.core.time;

#if defined(ALMOND_USING_SOFTWARE_RENDERER)

export namespace almondnamespace::anativecontext
{
    struct SoftRendState
    {
#ifdef ALMOND_USING_WINMAIN
        HWND hwnd{};           // Window handle
        HDC hdc{};             // Device context
        HWND parent{};         // Parent window
        WNDPROC oldWndProc{};
        BITMAPINFO bmi{};      // DIB info for StretchDIBits
#endif

        std::function<void(int, int)> onResize{};
        int width{ 400 };
        int height{ 300 };
        bool running{ false };
        std::vector<std::uint32_t> framebuffer{};

        struct MouseState
        {
            std::array<bool, 5> down{};
            std::array<bool, 5> pressed{};
            std::array<bool, 5> prevDown{};
            int lastX = 0, lastY = 0;
        } mouse{};

        struct KeyboardState
        {
            std::bitset<256> down{};
            std::bitset<256> pressed{};
            std::bitset<256> prevDown{};
        } keyboard{};

        // Timing
        timing::Timer pollTimer = timing::createTimer(1.0);
        timing::Timer fpsTimer = timing::createTimer(1.0);
        int frameCount = 0;

        // Cube rotation
        float angle = 0.f;
    };

    // Global state instance (exported)
    inline SoftRendState s_softrendererstate{};
}

#endif // ALMOND_USING_SOFTWARE_RENDERER
