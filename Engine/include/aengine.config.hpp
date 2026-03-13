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

// Public Epoch-prefixed defines are mapped onto the older compatibility layer
// so downstream code can migrate without breaking the current internals.
#if defined(EPOCH_MAIN_HEADLESS) && !defined(ALMOND_MAIN_HEADLESS)
#define ALMOND_MAIN_HEADLESS EPOCH_MAIN_HEADLESS
#endif
#if defined(EPOCH_MAIN_HANDLED) && !defined(ALMOND_MAIN_HANDLED)
#define ALMOND_MAIN_HANDLED EPOCH_MAIN_HANDLED
#endif
#if defined(EPOCH_FORCE_DISABLE_SDL) && !defined(ALMOND_FORCE_DISABLE_SDL)
#define ALMOND_FORCE_DISABLE_SDL EPOCH_FORCE_DISABLE_SDL
#endif
#if defined(EPOCH_FORCE_ENABLE_SDL) && !defined(ALMOND_FORCE_ENABLE_SDL)
#define ALMOND_FORCE_ENABLE_SDL EPOCH_FORCE_ENABLE_SDL
#endif
#if defined(EPOCH_FORCE_DISABLE_SFML) && !defined(ALMOND_FORCE_DISABLE_SFML)
#define ALMOND_FORCE_DISABLE_SFML EPOCH_FORCE_DISABLE_SFML
#endif
#if defined(EPOCH_FORCE_ENABLE_SFML) && !defined(ALMOND_FORCE_ENABLE_SFML)
#define ALMOND_FORCE_ENABLE_SFML EPOCH_FORCE_ENABLE_SFML
#endif
#if defined(EPOCH_FORCE_DISABLE_RAYLIB) && !defined(ALMOND_FORCE_DISABLE_RAYLIB)
#define ALMOND_FORCE_DISABLE_RAYLIB EPOCH_FORCE_DISABLE_RAYLIB
#endif
#if defined(EPOCH_FORCE_ENABLE_RAYLIB) && !defined(ALMOND_FORCE_ENABLE_RAYLIB)
#define ALMOND_FORCE_ENABLE_RAYLIB EPOCH_FORCE_ENABLE_RAYLIB
#endif
#if defined(EPOCH_FORCE_DISABLE_SOFTWARE_RENDERER) && !defined(ALMOND_FORCE_DISABLE_SOFTWARE_RENDERER)
#define ALMOND_FORCE_DISABLE_SOFTWARE_RENDERER EPOCH_FORCE_DISABLE_SOFTWARE_RENDERER
#endif
#if defined(EPOCH_FORCE_ENABLE_SOFTWARE_RENDERER) && !defined(ALMOND_FORCE_ENABLE_SOFTWARE_RENDERER)
#define ALMOND_FORCE_ENABLE_SOFTWARE_RENDERER EPOCH_FORCE_ENABLE_SOFTWARE_RENDERER
#endif
#if defined(EPOCH_FORCE_DISABLE_OPENGL) && !defined(ALMOND_FORCE_DISABLE_OPENGL)
#define ALMOND_FORCE_DISABLE_OPENGL EPOCH_FORCE_DISABLE_OPENGL
#endif
#if defined(EPOCH_FORCE_ENABLE_OPENGL) && !defined(ALMOND_FORCE_ENABLE_OPENGL)
#define ALMOND_FORCE_ENABLE_OPENGL EPOCH_FORCE_ENABLE_OPENGL
#endif
#if defined(EPOCH_USING_VULKAN) && !defined(ALMOND_USING_VULKAN)
#define ALMOND_USING_VULKAN EPOCH_USING_VULKAN
#endif
#if defined(EPOCH_USING_DIRECTX) && !defined(ALMOND_USING_DIRECTX)
#define ALMOND_USING_DIRECTX EPOCH_USING_DIRECTX
#endif
#if defined(EPOCH_USING_NOOP_HEADLESS) && !defined(ALMOND_USING_NOOP_HEADLESS)
#define ALMOND_USING_NOOP_HEADLESS EPOCH_USING_NOOP_HEADLESS
#endif
#if defined(EPOCH_USE_CLEAR_COLOR) && !defined(ALMOND_USE_CLEAR_COLOR)
#define ALMOND_USE_CLEAR_COLOR EPOCH_USE_CLEAR_COLOR
#endif
#if defined(EPOCH_USE_CLEAR_COLOR_VULKAN) && !defined(ALMOND_USE_CLEAR_COLOR_VULKAN)
#define ALMOND_USE_CLEAR_COLOR_VULKAN EPOCH_USE_CLEAR_COLOR_VULKAN
#endif
#if defined(EPOCH_SINGLE_PARENT) && !defined(ALMOND_SINGLE_PARENT)
#define ALMOND_SINGLE_PARENT EPOCH_SINGLE_PARENT
#endif

// Epoch Entry Point No-Op
// #define EPOCH_MAIN_HEADLESS
#ifndef ALMOND_MAIN_HANDLED
#ifndef ALMOND_MAIN_HEADLESS
#ifdef _WIN32

#define ALMOND_USING_WINMAIN
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

// ------------------------------------------------------------
// Engine Context Config
// ------------------------------------------------------------
#ifndef ALMOND_USE_CLEAR_COLOR
#   define ALMOND_USE_CLEAR_COLOR 0
#endif
#ifndef ALMOND_USE_CLEAR_COLOR_VULKAN
#   define ALMOND_USE_CLEAR_COLOR_VULKAN 1
#endif

#define ALMOND_SINGLE_PARENT 1

#define ALMOND_USING_OPENGL 1
#define ALMOND_USING_SFML 1
#define ALMOND_USING_RAYLIB 1
#define ALMOND_USING_SDL 1
#define ALMOND_USING_SOFTWARE_RENDERER 1
#define ALMOND_USING_VULKAN 1

#if defined(ALMOND_FORCE_DISABLE_SDL)
#undef ALMOND_USING_SDL
#endif
#if defined(ALMOND_FORCE_ENABLE_SDL)
#undef ALMOND_USING_SDL
#define ALMOND_USING_SDL 1
#endif

#if defined(ALMOND_FORCE_DISABLE_SFML)
#undef ALMOND_USING_SFML
#endif
#if defined(ALMOND_FORCE_ENABLE_SFML)
#undef ALMOND_USING_SFML
#define ALMOND_USING_SFML 1
#endif

#if defined(ALMOND_FORCE_DISABLE_RAYLIB)
#undef ALMOND_USING_RAYLIB
#endif
#if defined(ALMOND_FORCE_ENABLE_RAYLIB)
#undef ALMOND_USING_RAYLIB
#define ALMOND_USING_RAYLIB 1
#endif

#if defined(ALMOND_FORCE_DISABLE_SOFTWARE_RENDERER)
#undef ALMOND_USING_SOFTWARE_RENDERER
#endif
#if defined(ALMOND_FORCE_ENABLE_SOFTWARE_RENDERER)
#undef ALMOND_USING_SOFTWARE_RENDERER
#define ALMOND_USING_SOFTWARE_RENDERER 1
#endif

#if defined(ALMOND_FORCE_DISABLE_OPENGL)
#undef ALMOND_USING_OPENGL
#endif
#if defined(ALMOND_FORCE_ENABLE_OPENGL)
#undef ALMOND_USING_OPENGL
#define ALMOND_USING_OPENGL 1
#endif

#if defined(ALMOND_MAIN_HEADLESS) && !defined(EPOCH_MAIN_HEADLESS)
#define EPOCH_MAIN_HEADLESS ALMOND_MAIN_HEADLESS
#endif
#if defined(ALMOND_MAIN_HANDLED) && !defined(EPOCH_MAIN_HANDLED)
#define EPOCH_MAIN_HANDLED ALMOND_MAIN_HANDLED
#endif
#if defined(ALMOND_USING_WINMAIN) && !defined(EPOCH_USING_WINMAIN)
#define EPOCH_USING_WINMAIN ALMOND_USING_WINMAIN
#endif
#if defined(ALMOND_SINGLE_PARENT) && !defined(EPOCH_SINGLE_PARENT)
#define EPOCH_SINGLE_PARENT ALMOND_SINGLE_PARENT
#endif
#if defined(ALMOND_USE_CLEAR_COLOR) && !defined(EPOCH_USE_CLEAR_COLOR)
#define EPOCH_USE_CLEAR_COLOR ALMOND_USE_CLEAR_COLOR
#endif
#if defined(ALMOND_USE_CLEAR_COLOR_VULKAN) && !defined(EPOCH_USE_CLEAR_COLOR_VULKAN)
#define EPOCH_USE_CLEAR_COLOR_VULKAN ALMOND_USE_CLEAR_COLOR_VULKAN
#endif
#if defined(ALMOND_USING_OPENGL) && !defined(EPOCH_USING_OPENGL)
#define EPOCH_USING_OPENGL ALMOND_USING_OPENGL
#endif
#if defined(ALMOND_USING_SFML) && !defined(EPOCH_USING_SFML)
#define EPOCH_USING_SFML ALMOND_USING_SFML
#endif
#if defined(ALMOND_USING_RAYLIB) && !defined(EPOCH_USING_RAYLIB)
#define EPOCH_USING_RAYLIB ALMOND_USING_RAYLIB
#endif
#if defined(ALMOND_USING_SDL) && !defined(EPOCH_USING_SDL)
#define EPOCH_USING_SDL ALMOND_USING_SDL
#endif
#if defined(ALMOND_USING_SOFTWARE_RENDERER) && !defined(EPOCH_USING_SOFTWARE_RENDERER)
#define EPOCH_USING_SOFTWARE_RENDERER ALMOND_USING_SOFTWARE_RENDERER
#endif
#if defined(ALMOND_USING_VULKAN) && !defined(EPOCH_USING_VULKAN)
#define EPOCH_USING_VULKAN ALMOND_USING_VULKAN
#endif
#if defined(ALMOND_USING_DIRECTX) && !defined(EPOCH_USING_DIRECTX)
#define EPOCH_USING_DIRECTX ALMOND_USING_DIRECTX
#endif
#if defined(ALMOND_USING_NOOP_HEADLESS) && !defined(EPOCH_USING_NOOP_HEADLESS)
#define EPOCH_USING_NOOP_HEADLESS ALMOND_USING_NOOP_HEADLESS
#endif
// ============================================================
// Includes (verbatim, order preserved)
// ============================================================

//#ifdef ALMOND_USING_OPENGL
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
//#ifdef ALMOND_USING_SDL
//#include <glad/glad.h>
//#include <SDL3/SDL.h>
//#include <SDL3/SDL_version.h>
//
//#ifndef ALMOND_MAIN_HEADLESS
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
//#ifdef ALMOND_USING_VOLK
//#define VK_NO_PROTOTYPES
//#include <volk.h>
//#define ALMOND_USING_GLM
//#endif
//
//#ifdef ALMOND_USING_VULKAN_WITH_GLFW
//#define GLFW_INCLUDE_VULKAN
//#ifndef ALMOND_USING_VULKAN
//#define ALMOND_USING_VULKAN
//#endif
//#endif
//
//#ifdef ALMOND_USING_GLM
//#include <glm/glm.hpp>
//#endif
//
//#ifdef ALMOND_USING_VULKAN
//#include <vulkan/vulkan.h>
//#ifdef _WIN32
//#include <vulkan/vulkan_win32.h>
//#endif
//#endif
//
