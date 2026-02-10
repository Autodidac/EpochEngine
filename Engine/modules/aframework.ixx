module; // global module fragment — REQUIRED for Win32 headers

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

export namespace almondnamespace::platform::win32
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

// ------------------------------------------------------------
// Non-Windows hard stop (intentional)
// ------------------------------------------------------------

static_assert(false, "almond.platform.win32 imported on non-Windows platform");

#endif
