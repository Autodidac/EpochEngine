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
// Epoch Entry Point / Platform Configuration
// ============================================================

// Epoch Entry Point No-Op
// #define EPOCH_MAIN_HEADLESS
#ifndef EPOCH_MAIN_HANDLED
#ifndef EPOCH_MAIN_HEADLESS
#ifdef _WIN32
#define EPOCH_USING_WINMAIN
#endif
#endif
#endif

// ------------------------------------------------------------
// Debug / inspection toggles (unchanged)
// ------------------------------------------------------------
// #define RUN_CODE_INSPECTOR
// #define DEBUG_INPUT
// #define DEBUG_INPUT_OS
// #define DEBUG_MOUSE_MOVEMENT
// #define DEBUG_MOUSE_COORDS
// #define DEBUG_TEXTURE_RENDERING_VERBOSE
// #define DEBUG_TEXTURE_RENDERING_VERY_VERBOSE
// #define DEBUG_WINDOW_VERBOSE

#ifndef EPOCH_ENABLE_RENDERER_SLOW_FRAME_LOGS
#   define EPOCH_ENABLE_RENDERER_SLOW_FRAME_LOGS 1
#endif
#ifndef EPOCH_SLOW_FRAME_LOG_STARTUP_GRACE_MS
#   define EPOCH_SLOW_FRAME_LOG_STARTUP_GRACE_MS 5000
#endif
#ifndef EPOCH_SLOW_FRAME_LOG_THROTTLE_MS
#   define EPOCH_SLOW_FRAME_LOG_THROTTLE_MS 5000
#endif
#ifndef EPOCH_VULKAN_RUNTIME_DIAGNOSTICS
#   define EPOCH_VULKAN_RUNTIME_DIAGNOSTICS 0
#endif

// ------------------------------------------------------------
// Engine Context Config
// ------------------------------------------------------------
#ifndef EPOCH_USE_CLEAR_COLOR
#   define EPOCH_USE_CLEAR_COLOR 0
#endif
#ifndef EPOCH_USE_CLEAR_COLOR_VULKAN
#   define EPOCH_USE_CLEAR_COLOR_VULKAN 1
#endif

#define EPOCH_SINGLE_PARENT 1

#define EPOCH_USING_OPENGL 1
#define EPOCH_USING_SFML 1
#define EPOCH_USING_RAYLIB 1
#define EPOCH_USING_SDL 1
#define EPOCH_USING_SOFTWARE_RENDERER 1
#define EPOCH_USING_VULKAN 1

#if defined(EPOCH_FORCE_DISABLE_SDL)
#undef EPOCH_USING_SDL
#endif
#if defined(EPOCH_FORCE_ENABLE_SDL)
#undef EPOCH_USING_SDL
#define EPOCH_USING_SDL 1
#endif

#if defined(EPOCH_FORCE_DISABLE_SFML)
#undef EPOCH_USING_SFML
#endif
#if defined(EPOCH_FORCE_ENABLE_SFML)
#undef EPOCH_USING_SFML
#define EPOCH_USING_SFML 1
#endif

#if defined(EPOCH_FORCE_DISABLE_RAYLIB)
#undef EPOCH_USING_RAYLIB
#endif
#if defined(EPOCH_FORCE_ENABLE_RAYLIB)
#undef EPOCH_USING_RAYLIB
#define EPOCH_USING_RAYLIB 1
#endif

#if defined(EPOCH_FORCE_DISABLE_SOFTWARE_RENDERER)
#undef EPOCH_USING_SOFTWARE_RENDERER
#endif
#if defined(EPOCH_FORCE_ENABLE_SOFTWARE_RENDERER)
#undef EPOCH_USING_SOFTWARE_RENDERER
#define EPOCH_USING_SOFTWARE_RENDERER 1
#endif

#if defined(EPOCH_FORCE_DISABLE_OPENGL)
#undef EPOCH_USING_OPENGL
#endif
#if defined(EPOCH_FORCE_ENABLE_OPENGL)
#undef EPOCH_USING_OPENGL
#define EPOCH_USING_OPENGL 1
#endif
// ============================================================
// Includes (verbatim, order preserved)
// ============================================================

//#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)
//#include <glad/glad.h>
//
//#if defined(_WIN32)
//#if defined(__has_include) && __has_include(<glad/glad_wgl.h>)
//#include <glad/glad_wgl.h>
//#elif defined(__has_include) && __has_include(<GL/wglext.h>)
//#include <GL/wglext.h>
//#else
//#ifndef WGL_CONTEXT_MAJOR_VERSION_ARB
//#define WGL_CONTEXT_MAJOR_VERSION_ARB 0x2091
//#endif
//#ifndef WGL_CONTEXT_MINOR_VERSION_ARB
//#define WGL_CONTEXT_MINOR_VERSION_ARB 0x2092
//#endif
//#ifndef WGL_CONTEXT_PROFILE_MASK_ARB
//#define WGL_CONTEXT_PROFILE_MASK_ARB 0x9126
//#endif
//#ifndef WGL_CONTEXT_CORE_PROFILE_BIT_ARB
//#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB 0x00000001
//#endif
//#ifndef PFNWGLCREATECONTEXTATTRIBSARBPROC
//typedef HGLRC(WINAPI* PFNWGLCREATECONTEXTATTRIBSARBPROC)(HDC, HGLRC, const int*);
//#endif
//#endif
//#endif
//#endif
//
//#ifdef EPOCH_USING_SDL
//#include <glad/glad.h>
//#include <SDL3/SDL.h>
//#include <SDL3/SDL_version.h>
//
//#ifndef EPOCH_MAIN_HEADLESS
//#define SDL_MAIN_HANDLED
//#endif
//
//#include <SDL3/SDL_main.h>
//#include <SDL3/SDL_scancode.h>
//#include <SDL3_image/SDL_image.h>
//#include <SDL3/SDL_events.h>
//#include <SDL3/SDL_video.h>
//#include <SDL3/SDL_render.h>
//#include <SDL3/SDL_surface.h>
//#include <SDL3/SDL_opengl.h>
//#endif
//
//#ifdef EPOCH_USING_VOLK
//#define VK_NO_PROTOTYPES
//#include <volk.h>
//#define EPOCH_USING_GLM
//#endif
//
//#ifdef EPOCH_USING_VULKAN_WITH_GLFW
//#define GLFW_INCLUDE_VULKAN
//#ifndef EPOCH_USING_VULKAN
//#define EPOCH_USING_VULKAN
//#endif
//#endif
//
//#ifdef EPOCH_USING_GLM
//#include <glm/glm.hpp>
//#endif
//
//#ifdef EPOCH_USING_VULKAN
//#include <vulkan/vulkan.h>
//#ifdef _WIN32
//#include <vulkan/vulkan_win32.h>
//#endif
//#endif
//
