module;

#include <include/aengine.config.hpp>

#if defined(ALMOND_USING_RAYLIB)
#if defined(_WIN32)
#   ifdef ALMOND_USING_WINMAIN
#       include <include/aframework.hpp>
#   endif
#endif
#endif

export module acontext.raylib.state;

import aengine.core.time;
import aengine.cli;
import aengine.core.context;
import acontext.raylib.api;

import <array>;
import <bitset>;
import <cstdint>;
import <functional>;
import <thread>;


#if defined(ALMOND_USING_RAYLIB)

export namespace almondnamespace::raylibstate
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
        almondnamespace::core::Context* owner_ctx = nullptr;
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
        almondnamespace::raylib_api::RenderTexture2D offscreen{};

        bool frameActive = false;
        bool frameInTextureMode = false;

        // Lifecycle
        bool running = false;
        bool cleanupIssued = false;
        bool cleanupRequested = false;

        // Timers (unchanged, preserved)
        timing::Timer pollTimer = timing::createTimer(1.0);
        timing::Timer fpsTimer = timing::createTimer(1.0);
        timing::Timer frameTimer = timing::createTimer(1.0);
        int frameCount = 0;

        // Renderer-facing state only
        GuiFitViewport lastViewport{};
    };

    // Single instance (raylib is single-context by design)
    inline RaylibState s_raylibstate{};

    // Renderer query helper
    export inline GuiFitViewport get_last_viewport_fit() noexcept
    {
        return s_raylibstate.lastViewport;
    }
}

#endif // ALMOND_USING_RAYLIB
