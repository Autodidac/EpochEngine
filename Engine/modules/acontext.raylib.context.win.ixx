/**************************************************************
 *   AlmondShell – Raylib Context (Win32 glue)
 *   Purpose: Host-managed OpenGL context integration (no reparenting).
 *   This module may include Win32 headers. It does NOT include raylib.h.
 **************************************************************/

module;

#include <include/aengine.config.hpp>

#if defined(_WIN32) && defined(ALMOND_USING_RAYLIB)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <include/aframework.hpp>

#endif

export module acontext.raylib.context.win;

#if defined(_WIN32) && defined(ALMOND_USING_RAYLIB)

export namespace almondnamespace::raylibcontext::win
{
    // Intentionally empty: Raylib no longer owns/reparents a window on Win32.
}

#endif
