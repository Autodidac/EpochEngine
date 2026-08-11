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
/**************************************************************
 *   epochengine â€“ Raylib Context (Win32 glue)
 *   Purpose: Host-managed OpenGL context integration (no reparenting).
 *   This module may include Win32 headers. It does NOT include raylib.h.
 **************************************************************/

module;

#include <include/engine.config.hpp>

#if defined(_WIN32) && defined(EPOCH_USING_RAYLIB) && (EPOCH_USING_RAYLIB == 1)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <../src/platform.framework.hpp>

#endif

export module raylib.context_win;

#if defined(_WIN32) && defined(EPOCH_USING_RAYLIB) && (EPOCH_USING_RAYLIB == 1)
import raylib.state;
#endif

#if defined(_WIN32) && defined(EPOCH_USING_RAYLIB) && (EPOCH_USING_RAYLIB == 1)

export namespace epochengine::raylibcontext::win
{
    [[nodiscard]] inline bool native_context_is_current() noexcept
    {
        const auto& state = epochengine::raylibstate::s_raylibstate;
        return state.hdc != nullptr
            && state.hglrc != nullptr
            && ::wglGetCurrentDC() == state.hdc
            && ::wglGetCurrentContext() == state.hglrc;
    }
}

#endif
