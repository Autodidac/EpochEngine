/************************************************
 *  â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—  â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—  â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—â–ˆâ–ˆâ•—  â–ˆâ–ˆâ•—   *
 *  â–ˆâ–ˆâ•”â•â•â•â•â•â–ˆâ–ˆâ•”â•â•â–ˆâ–ˆâ•—â–ˆâ–ˆâ•”â•â•â•â–ˆâ–ˆâ•—â–ˆâ–ˆâ•”â•â•â•â•â•â–ˆâ–ˆâ•‘  â–ˆâ–ˆâ•‘   *
 *  â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—  â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•”â•â–ˆâ–ˆâ•‘   â–ˆâ–ˆâ•‘â–ˆâ–ˆâ•‘     â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•‘   *
 *  â–ˆâ–ˆâ•”â•â•â•  â–ˆâ–ˆâ•”â•â•â•â• â–ˆâ–ˆâ•‘   â–ˆâ–ˆâ•‘â–ˆâ–ˆâ•‘     â–ˆâ–ˆâ•”â•â•â–ˆâ–ˆâ•‘   *
 *  â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—â–ˆâ–ˆâ•‘     â•šâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•”â•â•šâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—â–ˆâ–ˆâ•‘  â–ˆâ–ˆâ•‘   *
 *  â•šâ•â•â•â•â•â•â•â•šâ•â•      â•šâ•â•â•â•â•â•  â•šâ•â•â•â•â•â•â•šâ•â•  â•šâ•â•   *
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

#include <array>
#include <bitset>
#include <cstdint>
#include <functional>
#include <thread>

#include <include/aengine.config.hpp>

#if defined(EPOCH_USING_RAYLIB) && (EPOCH_USING_RAYLIB == 1)
#if defined(_WIN32)
#   ifdef EPOCH_USING_WINMAIN
#       include <include/aframework.hpp>
#   endif
#endif
#endif

export module raylib.state;

import core.timer;
import aengine.cli;
import core.context;
import raylib.api;

#if defined(EPOCH_USING_RAYLIB) && (EPOCH_USING_RAYLIB == 1)

namespace epochnamespace::raylibstate
{
    // ------------------------------------------------------------
    // Viewport info (renderer-only, no window management)
    // ------------------------------------------------------------
    struct GuiFitViewport
    {
        int vpX = 0, vpY = 0, vpW = 1, vpH = 1;
        int fbW = 1, fbH = 1;
        int refW = 1920, refH = 1080;
        float scale = 1.0f;
    };

    // ------------------------------------------------------------
    // Raylib backend state (NO resizing, NO parenting logic)
    // ------------------------------------------------------------
    export struct RaylibState
    {
#if defined(_WIN32)
        HWND  hwnd = nullptr;
        HDC   hdc = nullptr;
        HGLRC hglrc = nullptr;
        HWND  parent = nullptr;
#else
        void* hwnd = nullptr;
        void* hdc = nullptr;
        void* hglrc = nullptr;
        void* parent = nullptr;
#endif

        bool ownsDC = false;

        // Owning engine context + thread
        epochnamespace::core::Context* owner_ctx = nullptr;
        std::thread::id owner_thread{};

        // Optional user resize callback (engine-driven only)
        std::function<void(int, int)> onResize{};
        std::function<void(int, int)> userResize{};

        // Logical dimensions (authoritative values set by multiplexer)
        unsigned width = 0;
        unsigned height = 0;
        unsigned offscreenWidth = 0;
        unsigned offscreenHeight = 0;

        // Offscreen render target owned by the host OpenGL context
        epochnamespace::raylib_api::RenderTexture2D offscreen{};

        bool frameActive = false;
        bool frameInTextureMode = false;

        // Lifecycle
        bool running = false;
        bool renderingActive = false;
        bool cleanupIssued = false;
        bool cleanupRequested = false;
        unsigned currentFailureStreak = 0;
        bool currentFailureWarned = false;

        // Timers (unchanged, preserved)
        timing::Timer pollTimer = timing::createTimer(1.0);
        timing::Timer fpsTimer = timing::createTimer(1.0);
        timing::Timer frameTimer = timing::createTimer(1.0);
        int frameCount = 0;

        // Renderer-facing state only
        GuiFitViewport lastViewport{};
    };

    // Single instance (raylib is single-context by design)
    export inline RaylibState s_raylibstate{};

    // Renderer query helper
    export inline GuiFitViewport get_last_viewport_fit() noexcept
    {
        return s_raylibstate.lastViewport;
    }
}

#endif // EPOCH_USING_RAYLIB
