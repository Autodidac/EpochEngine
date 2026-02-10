/**************************************************************
 *   █████╗ ██╗     ███╗   ███╗   ███╗   ██╗    ██╗██████╗    *
 *  ██╔══██╗██║     ████╗ ████║ ██╔═══██╗████╗  ██║██╔══██╗   *
 *  ███████║██║     ██╔████╔██║ ██║   ██║██╔██╗ ██║██║  ██║   *
 *  ██╔══██║██║     ██║╚██╔╝██║ ██║   ██║██║╚██╗██║██║  ██║   *
 *  ██║  ██║███████╗██║ ╚═╝ ██║ ╚██████╔╝██║ ╚████║██████╔╝   *
 *  ╚═╝  ╚═╝╚══════╝╚═╝     ╚═╝  ╚═════╝ ╚═╝  ╚═══╝╚═════╝    *
 *                                                            *
 *   This file is part of the Almond Project.                 *
 *   AlmondShell - Modular C++ Framework                      *
 *                                                            *
 *   SPDX-License-Identifier: LicenseRef-MIT-NoSell           *
 *                                                            *
 *   Provided "AS IS", without warranty of any kind.          *
 *   Use permitted for Non-Commercial Purposes ONLY,          *
 *   without prior commercial licensing agreement.            *
 *                                                            *
 *   Redistribution Allowed with This Notice and              *
 *   LICENSE file. No obligation to disclose modifications.   *
 *                                                            *
 *   See LICENSE file for full terms.                         *
 *                                                            *
 **************************************************************/
// aplatform.hpp
#pragma once

// MSVC's standard library implementation can spuriously report redefinition
// errors for default template arguments when header units or modules are
// involved but the corresponding C++20 modules are not actually available.
// Force the project to use the traditional header-based includes instead of
// attempting to import modules guarded by __cpp_modules feature detection.
// This keeps builds consistent across toolchains that do not ship the
// module interfaces referenced in other AlmondShell headers.
#ifndef ALMOND_FORCE_LEGACY_HEADERS
#define ALMOND_FORCE_LEGACY_HEADERS 1
#endif

// Use classic headers instead of importing the C++ standard library module.
// This avoids duplicate definitions when the toolchain lacks full module
// support and ensures consistent behavior across compilers.
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

//#include <string>
//#include <ctime>

// Maintain compatibility with legacy code that referenced the old
// `almondshell` namespace name while the project now uses
// `almondnamespace`. Use a namespace alias instead of a macro so tools
// and modules see the real namespace and avoid accidental token
// replacement that can break builds.
namespace almondnamespace { }
namespace almondshell = almondnamespace;

// API Visibility Macros
#ifdef _WIN32
    // Windows DLL export/import logic
#ifndef ENGINE_STATICLIB
#ifdef ENGINE_DLL_EXPORTS
#define ENGINE_API __declspec(dllexport)
#else
#define ENGINE_API __declspec(dllimport)
#endif
#else
#define ENGINE_API  // Static library, no special attributes
#endif
#else
    // GCC/Clang visibility attributes
#if (__GNUC__ >= 4) && !defined(ENGINE_STATICLIB) && defined(ENGINE_DLL_EXPORTS)
#define ENGINE_API __attribute__((visibility("default")))
#else
#define ENGINE_API  // Static library or unsupported compiler
#endif
#endif

// Calling Convention Macros
#ifndef _STDCALL_SUPPORTED
#define ALECALLCONV __cdecl
#else
#define ALECALLCONV __stdcall
#endif

#ifdef _WIN32
#ifndef ALMOND_MAIN_HEADLESS
#include <shellscalingapi.h>  // For GetDpiForMonitor
#pragma comment(lib, "Shcore.lib")
#include <CommCtrl.h>
#pragma comment(lib, "comctl32.lib")
#endif // !ALMOND_MAIN_HEADLESS

// <ctime> (via <time.h>) on MSVC defines a macro named `time` that collides with
// our `almondnamespace::time` namespace. Undefine it after the Windows headers so
// nested namespace usage stays intact.
#ifdef time
#undef time
#endif // !ALMOND_MAIN_HEADLESS
#endif
