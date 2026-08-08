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


#include <cstdint>

#if defined(_WIN32)
#include <wtypes.h>
#endif

//import platform.engine;
//import engine.config;
#include "engine.config.hpp" // for engine/backend configuration macros

//export import platform.engine;
//export import ecs.legacy_components;
//export import aengine.renderers;
//export import aengine.menu;
//export import input.engine;
//export import aengine.aallocator;
//export import aengine.aatlasmanager;
//export import aengine.aatlastexture;
//export import aengine.aimageloader;
//export import aengine.aimageatlaswriter;
//export import aengine.aimagewriter;
//export import aengine.atexture;
//export import aengine.autilities;

//#include "epoch.engine.hpp"          // DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT

// Window dimensions
//inline constexpr int DEFAULT_WINDOW_WIDTH = 1024;
//inline constexpr int DEFAULT_WINDOW_HEIGHT = 768;
//inline constexpr int DEFAULT_WINDOW_WIDTH = 1920;
//inline constexpr int DEFAULT_WINDOW_HEIGHT = 1080;
//inline constexpr int DEFAULT_WINDOW_WIDTH = 2048;
//inline constexpr int DEFAULT_WINDOW_HEIGHT = 1080;
//inline constexpr int DEFAULT_WINDOW_WIDTH = 2732;
//inline constexpr int DEFAULT_WINDOW_HEIGHT = 1536;

inline constexpr int DEFAULT_WINDOW_WIDTH = 3840;
inline constexpr int DEFAULT_WINDOW_HEIGHT = 2160;
//inline constexpr int DEFAULT_WINDOW_WIDTH = 4096;
//inline constexpr int DEFAULT_WINDOW_HEIGHT = 2160;

namespace epochengine::core
{
    void ParseCommandLine(int argc, char** argv);
    void RunEngine();
}

#if defined(_WIN32) && defined(EPOCH_USING_WINMAIN)

// Max string length for title and class name
#define MAX_LOADSTRING 100

namespace epochengine::core
{
    // Forward declarations of Win32 functions
    ATOM RegisterWindowClass(HINSTANCE hInstance, LPCWSTR window_name, LPCWSTR child_name);
    void PrintLastWin32Error(const wchar_t* lpszFunction);
    void ShowConsole();
}

#endif
#if defined(_WIN32)
HWND InitWindowInstance(HINSTANCE hInstance, int nCmdShow, LPCWSTR szWindowClass, LPCWSTR szTitle, int32_t windowWidth, int32_t windowHeight);
#endif
