module;

// Keep platform / ABI macros in the global fragment.
// They must remain macros for ABI + build-system compatibility.

#ifdef _WIN32
#ifndef ENGINE_STATICLIB
#ifdef ENGINE_DLL_EXPORTS
#define ENGINE_API __declspec(dllexport)
#else
#define ENGINE_API __declspec(dllimport)
#endif
#else
#define ENGINE_API
#endif
#else
#if (__GNUC__ >= 4) && !defined(ENGINE_STATICLIB) && defined(ENGINE_DLL_EXPORTS)
#define ENGINE_API __attribute__((visibility("default")))
#else
#define ENGINE_API
#endif
#endif

#ifndef _STDCALL_SUPPORTED
#define ALECALLCONV __cdecl
#else
#define ALECALLCONV __stdcall
#endif

// -----------------------------------------------------------------------------
// Module declaration
// -----------------------------------------------------------------------------
export module aengine.platform;

import <string>;

// -----------------------------------------------------------------------------
// Namespace selection
// -----------------------------------------------------------------------------
// You were previously redefining this via macro.
// For modules, make this explicit and stable.

export namespace almondnamespace
{
    // This namespace intentionally left minimal.
    // Platform-specific helpers live in other modules.
}

export namespace almondnamespace::platform
{
#if defined(__linux__)
    bool pump_events();
#else
    inline bool pump_events() { return true; }
#endif
}
