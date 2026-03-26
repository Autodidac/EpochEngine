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
module; // global module fragment — REQUIRED for Win32 headers

#if !defined(_WIN32)
#include <cstddef>
#include <cstdint>
#endif

#if defined(_WIN32)

// ------------------------------------------------------------
// Win32 hygiene (must be before <windows.h>)
// ------------------------------------------------------------

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

// Prevent winsock conflicts unless explicitly needed
#ifndef _WINSOCKAPI_
#define _WINSOCKAPI_
#endif

#include <windows.h>
#include <windowsx.h>   // GET_X_LPARAM / GET_Y_LPARAM
#include <shellapi.h>   // ShellExecute, CommandLineToArgvW, etc.

#endif // _WIN32

// ============================================================
// Named module
// ============================================================

export module aframework;

// ============================================================
// Public surface (intentionally minimal)
// ============================================================

#if defined(_WIN32)

export namespace epochnamespace::platform::win32
{
    using hwnd = HWND;
    using hinst = HINSTANCE;
    using msg = MSG;
    using lparam = LPARAM;
    using wparam = WPARAM;

    [[nodiscard]] inline int mouse_x(LPARAM l) noexcept
    {
        return GET_X_LPARAM(l);
    }

    [[nodiscard]] inline int mouse_y(LPARAM l) noexcept
    {
        return GET_Y_LPARAM(l);
    }
}

#else

export namespace epochnamespace::platform::win32
{
    using hwnd = void*;
    using hinst = void*;
    using msg = std::nullptr_t;
    using lparam = std::intptr_t;
    using wparam = std::uintptr_t;

    [[nodiscard]] inline int mouse_x(lparam) noexcept
    {
        return 0;
    }

    [[nodiscard]] inline int mouse_y(lparam) noexcept
    {
        return 0;
    }
}

#endif
