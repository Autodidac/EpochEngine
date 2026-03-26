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
 // SoftRenderer - State  (C++23 module)

module;

#include <array>
#include <bitset>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

//#include "aplatform.hpp"

#include <include/aengine.config.hpp> // for EPOCH_USING Macros   // may bring in <windows.h>, etc.
//#include "arobusttime.hpp"     // time::Timer, time::createTimer(...)
#if defined(EPOCH_USING_SOFTWARE_RENDERER) && (EPOCH_USING_SOFTWARE_RENDERER == 1)
#   if defined(_WIN32)
#       ifdef EPOCH_USING_WINMAIN
#         include "aframework.hpp"
#       endif
#       ifndef WIN32_LEAN_AND_MEAN
#           define WIN32_LEAN_AND_MEAN
#       endif
#   endif
#endif

export module acontext.softrenderer.state;


//import aengine.platform;
import aengine.core.context;
import aengine.core.time;

#if defined(EPOCH_USING_SOFTWARE_RENDERER) && (EPOCH_USING_SOFTWARE_RENDERER == 1)

export namespace epochnamespace::anativecontext
{
    struct SoftRendState
    {
#ifdef EPOCH_USING_WINMAIN
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
        std::vector<std::uint32_t> sceneFramebuffer{};
        bool frameValid{ false };
        std::uint64_t lastGuiGeneration{ 0 };
        epochnamespace::core::RenderViewport lastSceneViewport{};
        std::uint8_t lastPreviewMode{
            static_cast<std::uint8_t>(epochnamespace::core::ScenePreviewMode::None)
        };

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

#endif // EPOCH_USING_SOFTWARE_RENDERER
