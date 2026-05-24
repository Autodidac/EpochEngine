/************************************************
 *  Â¦Â¦Â¦Â¦Â¦Â¦Â¦+Â¦Â¦Â¦Â¦Â¦Â¦+  Â¦Â¦Â¦Â¦Â¦Â¦+  Â¦Â¦Â¦Â¦Â¦Â¦+Â¦Â¦+  Â¦Â¦+   *
 *  Â¦Â¦+----+Â¦Â¦+--Â¦Â¦+Â¦Â¦+---Â¦Â¦+Â¦Â¦+----+Â¦Â¦Â¦  Â¦Â¦Â¦   *
 *  Â¦Â¦Â¦Â¦Â¦+  Â¦Â¦Â¦Â¦Â¦Â¦++Â¦Â¦Â¦   Â¦Â¦Â¦Â¦Â¦Â¦     Â¦Â¦Â¦Â¦Â¦Â¦Â¦Â¦   *
 *  Â¦Â¦+--+  Â¦Â¦+---+ Â¦Â¦Â¦   Â¦Â¦Â¦Â¦Â¦Â¦     Â¦Â¦+--Â¦Â¦Â¦   *
 *  Â¦Â¦Â¦Â¦Â¦Â¦Â¦+Â¦Â¦Â¦     +Â¦Â¦Â¦Â¦Â¦Â¦+++Â¦Â¦Â¦Â¦Â¦Â¦+Â¦Â¦Â¦  Â¦Â¦Â¦   *
 *  +------++-+      +-----+  +-----++-+  +-+   *
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

// context.multiplexer.linux.cpp

module;

#if defined(__linux__)

// Feature flags (defines EPOCH_USING_*)
#include <include/engine.config.hpp> // for EPOCH_USING Macros

// If GLAD is enabled on Linux, it must come before the GLX headers.
#if (defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)) || defined(EPOCH_USING_SDL)
#   include <glad/glad.h>
#endif

// X11 / GLX headers must be includes (not module imports)
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/extensions/Xrandr.h>
#include <GL/glx.h>
#include <GL/glxext.h>

// ---- std ----
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <exception>
#include <format>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

module context.multiplexer;

// ---- engine interfaces/types (modules you already own) ----
import core.context;          // Context, InitializeAllContexts(), CloneContext(), g_backends, etc.
import core.logger;
import context.window;        // WindowData
import context.type;          // ContextType
import core.commandline;
import engine.input;
import engine.cli;
import engine.telemetry;

// ---- helpers ----
import utility.string_converter;     // epochnamespace::text::narrow_utf8

// ---- backends (only referenced when enabled) ----
#   if defined(EPOCH_USING_OPENGL)
import opengl.context;       // epochnamespace::openglcontext::opengl_initialize
#   endif
#   if defined(EPOCH_USING_OPENGL) || defined(EPOCH_USING_SDL)
import opengl.platform;      // epochnamespace::openglcontext::PlatformGL::get_proc_address
#   endif
#   if defined(EPOCH_USING_RAYLIB)
import raylib.context;       // epochnamespace::raylibcontext::raylib_initialize
#   endif
#   if defined(EPOCH_USING_SDL)
import sdl.context;          // epochnamespace::sdlcontext::sdl_initialize
#   endif
#   if defined(EPOCH_USING_SFML)
import sfml.context;         // epochnamespace::sfmlcontext::sfml_initialize
#   endif
#   if defined(EPOCH_USING_SOFTWARE_RENDERER)
import software.context; // epochnamespace::anativecontext::softrenderer_initialize
#   endif

namespace epochnamespace::platform
{
    // Keep these as the same globals your other code expects.
    Display* global_display = nullptr;
    ::Window global_window = 0;
}

namespace epochnamespace::core
{
    namespace
    {
        constexpr auto kRenderThreadStartupStepDelay = std::chrono::milliseconds(250);
    }

    using epochnamespace::platform::global_display;
    using epochnamespace::platform::global_window;

    MultiContextManager* GetActiveMultiContextManager() noexcept
    {
        return MultiContextManager::s_activeInstance;
    }

    void RequestActiveParentLayout() noexcept
    {
        if (auto* mgr = GetActiveMultiContextManager())
            mgr->ArrangeDockedWindowsGrid();
    }

    void HandleX11Configure(::Window window, int width, int height)
    {
        if (auto* mgr = GetActiveMultiContextManager())
        {
            // Your cross-platform typedefs treat HWND as a pointer-sized handle.
            HWND hwnd = reinterpret_cast<HWND>(static_cast<uintptr_t>(window));
            mgr->HandleResize(hwnd, width, height);
        }
    }

namespace
{
    [[nodiscard]] inline std::shared_ptr<epochnamespace::core::Context> typed_context(
        const epochnamespace::core::OpaqueContextHandle& opaque) noexcept
    {
        return opaque
            ? std::reinterpret_pointer_cast<epochnamespace::core::Context>(opaque)
            : nullptr;
    }

        constexpr std::string_view kLogSys = "Context.Multiplexer.Linux";
        constexpr int kDefaultWidth = 800;
        constexpr int kDefaultHeight = 600;

        std::once_flag g_xlibInitFlag;
        bool g_xlibInitialized = false;

        inline ::Window to_xwindow(HWND handle) noexcept
        {
            return static_cast<::Window>(reinterpret_cast<uintptr_t>(handle));
        }

        inline HWND from_xwindow(::Window window) noexcept
        {
            return reinterpret_cast<HWND>(static_cast<uintptr_t>(window));
        }

        inline Display* to_display(HDC dc) noexcept
        {
            return reinterpret_cast<Display*>(dc);
        }

        inline HDC from_display(Display* display) noexcept
        {
            return reinterpret_cast<HDC>(display);
        }

        inline GLXContext to_glx(HGLRC ctx) noexcept
        {
            return reinterpret_cast<GLXContext>(ctx);
        }

        inline HGLRC from_glx(GLXContext ctx) noexcept
        {
            return reinterpret_cast<HGLRC>(ctx);
        }

        std::wstring_view BackendDisplayName(ContextType type) noexcept
        {
            using enum ContextType;
            switch (type)
            {
            case OpenGL:   return L"OpenGL";
            case SDL:      return L"SDL";
            case SFML:     return L"SFML";
            case RayLib:   return L"Raylib";
            case Software: return L"Software";
            case Vulkan:   return L"Vulkan";
            case DirectX:  return L"DirectX";
            case Noop:     return L"Noop";
            case Custom:   return L"Custom";
            default:       return L"Context";
            }
        }

        std::wstring BuildWindowTitle(ContextType type, int index, bool parented)
        {
            const std::wstring_view base = BackendDisplayName(type);
            std::wstring title{ base.begin(), base.end() };
            title += parented ? L" Dock " : L" Window ";
            title += std::to_wstring(static_cast<long long>(index) + 1);
            return title;
        }

        struct NativeTitleFpsState
        {
            std::wstring baseTitle{};
            std::uint64_t frameCount = 0;
            double fps = 0.0;
            std::chrono::steady_clock::time_point lastSample{};
        };

        std::mutex g_nativeTitleFpsMutex;
        std::unordered_map<const WindowData*, NativeTitleFpsState> g_nativeTitleFps;

        [[nodiscard]] std::wstring make_native_fps_title(std::wstring_view base, double fps)
        {
            std::wstring title{ base };
            title += L" | host ";
            title += std::to_wstring(static_cast<long long>(fps + 0.5));
            title += L" FPS";
            return title;
        }

        void record_native_title_frame(Display* display, ::Window xwin, WindowData& window)
        {
            if (!display || !xwin)
                return;

            const auto now = std::chrono::steady_clock::now();
            std::wstring title{};

            {
                std::scoped_lock lock(g_nativeTitleFpsMutex);
                auto& state = g_nativeTitleFps[&window];
                if (state.baseTitle.empty())
                {
                    state.baseTitle = window.titleWide.empty()
                        ? L"Epoch Context"
                        : window.titleWide;
                    state.lastSample = now;
                }

                ++state.frameCount;

                const auto elapsed = now - state.lastSample;
                if (elapsed < std::chrono::seconds(1))
                    return;

                const double seconds = std::chrono::duration<double>(elapsed).count();
                state.fps = seconds > 0.0
                    ? static_cast<double>(state.frameCount) / seconds
                    : 0.0;
                state.frameCount = 0;
                state.lastSample = now;
                title = make_native_fps_title(state.baseTitle, state.fps);
            }

            const auto narrow = epochnamespace::text::narrow_utf8(title);
            XStoreName(display, xwin, narrow.c_str());
        }

        void forget_native_title_frame_source(const WindowData* window) noexcept
        {
            if (!window)
                return;

            std::scoped_lock lock(g_nativeTitleFpsMutex);
            g_nativeTitleFps.erase(window);
        }

        void UpdateContextDimensions(Context& ctx, WindowData& win, int width, int height) noexcept
        {
            ctx.width = width;
            ctx.height = height;
            ctx.framebufferWidth = width;
            ctx.framebufferHeight = height;
            ctx.virtualWidth = width;
            ctx.virtualHeight = height;

            win.width = width;
            win.height = height;
        }

        void SetupResizeCallback(WindowData& win)
        {
            auto liveContext = typed_context(win.context);
            if (!liveContext) return;

            if (!liveContext->onResize && win.onResize)
                liveContext->onResize = win.onResize;

            if (!win.onResize && liveContext->onResize)
                win.onResize = liveContext->onResize;
        }

        [[nodiscard]] inline int clamp_positive(int value) noexcept
        {
            return (value < 1) ? 1 : value;
        }

        [[nodiscard]] epochnamespace::input::Key map_keysym(KeySym sym) noexcept
        {
            using epochnamespace::input::Key;
            switch (sym)
            {
            case XK_a: case XK_A: return Key::A;
            case XK_b: case XK_B: return Key::B;
            case XK_c: case XK_C: return Key::C;
            case XK_d: case XK_D: return Key::D;
            case XK_e: case XK_E: return Key::E;
            case XK_f: case XK_F: return Key::F;
            case XK_g: case XK_G: return Key::G;
            case XK_h: case XK_H: return Key::H;
            case XK_i: case XK_I: return Key::I;
            case XK_j: case XK_J: return Key::J;
            case XK_k: case XK_K: return Key::K;
            case XK_l: case XK_L: return Key::L;
            case XK_m: case XK_M: return Key::M;
            case XK_n: case XK_N: return Key::N;
            case XK_o: case XK_O: return Key::O;
            case XK_p: case XK_P: return Key::P;
            case XK_q: case XK_Q: return Key::Q;
            case XK_r: case XK_R: return Key::R;
            case XK_s: case XK_S: return Key::S;
            case XK_t: case XK_T: return Key::T;
            case XK_u: case XK_U: return Key::U;
            case XK_v: case XK_V: return Key::V;
            case XK_w: case XK_W: return Key::W;
            case XK_x: case XK_X: return Key::X;
            case XK_y: case XK_Y: return Key::Y;
            case XK_z: case XK_Z: return Key::Z;
            case XK_0: return Key::Num0;
            case XK_1: return Key::Num1;
            case XK_2: return Key::Num2;
            case XK_3: return Key::Num3;
            case XK_4: return Key::Num4;
            case XK_5: return Key::Num5;
            case XK_6: return Key::Num6;
            case XK_7: return Key::Num7;
            case XK_8: return Key::Num8;
            case XK_9: return Key::Num9;
            case XK_Escape: return Key::Escape;
            case XK_Return: case XK_KP_Enter: return Key::Enter;
            case XK_Tab: return Key::Tab;
            case XK_BackSpace: return Key::Backspace;
            case XK_space: return Key::Space;
            case XK_Left: return Key::Left;
            case XK_Right: return Key::Right;
            case XK_Up: return Key::Up;
            case XK_Down: return Key::Down;
            case XK_Shift_L: return Key::LeftShift;
            case XK_Shift_R: return Key::RightShift;
            case XK_Control_L: return Key::LeftControl;
            case XK_Control_R: return Key::RightControl;
            case XK_Alt_L: case XK_Meta_L: return Key::LeftAlt;
            case XK_Alt_R: case XK_Meta_R: return Key::RightAlt;
            case XK_Super_L: return Key::LeftSuper;
            case XK_Super_R: return Key::RightSuper;
            case XK_F1: return Key::F1;
            case XK_F2: return Key::F2;
            case XK_F3: return Key::F3;
            case XK_F4: return Key::F4;
            case XK_F5: return Key::F5;
            case XK_F6: return Key::F6;
            case XK_F7: return Key::F7;
            case XK_F8: return Key::F8;
            case XK_F9: return Key::F9;
            case XK_F10: return Key::F10;
            case XK_F11: return Key::F11;
            case XK_F12: return Key::F12;
            default: return Key::Unknown;
            }
        }

        [[nodiscard]] std::optional<epochnamespace::input::MouseButton> map_mouse_button(unsigned int button) noexcept
        {
            using epochnamespace::input::MouseButton;
            switch (button)
            {
            case Button1: return MouseButton::MouseLeft;
            case Button2: return MouseButton::MouseMiddle;
            case Button3: return MouseButton::MouseRight;
            case 8: return MouseButton::MouseButton4;
            case 9: return MouseButton::MouseButton5;
            default: return std::nullopt;
            }
        }

        void clear_linux_input_state() noexcept
        {
            using namespace epochnamespace::input;
            std::unique_lock lock(g_inputMutex);
            keyPressed.reset();
            mousePressed.reset();
            mouseWheel.store(0, std::memory_order_relaxed);
        }
    } // namespace

    // ---- MultiContextManager (Linux impl) ----

    void MultiContextManager::ShowConsole()
    {
        // Linux: no-op (already attached).
    }

    bool MultiContextManager::Initialize(
        HINSTANCE /*hInst*/,
        int RayLibWinCount,
        int SDLWinCount,
        int SFMLWinCount,
        int VulkanWinCount,
        int OpenGLWinCount,
        int DirectXWinCount,
        int SoftwareWinCount,
        bool parented)
    {
        (void)DirectXWinCount;
        const int totalRequested =
            RayLibWinCount + SDLWinCount + SFMLWinCount + VulkanWinCount + OpenGLWinCount + SoftwareWinCount;
        const bool effectiveParented = false;
        const bool singleWindow = (totalRequested == 1);
        const int initialWidth = singleWindow ? cli::window_width : kDefaultWidth;
        const int initialHeight = singleWindow ? cli::window_height : kDefaultHeight;

        if (totalRequested <= 0)
            return false;

        if (parented)
        {
            epochnamespace::logger::get(kLogSys).log(
                epochnamespace::logger::LogLevel::WARN,
                "Linux multiplexer currently forces standalone windows; parented mode is ignored.",
                std::source_location::current());
        }

        std::call_once(g_xlibInitFlag, []()
            {
                g_xlibInitialized = (XInitThreads() != 0);
            });

        if (!g_xlibInitialized)
        {
            epochnamespace::logger::get(kLogSys).log(
                epochnamespace::logger::LogLevel::Error,
                "XInitThreads failed; aborting X11 initialization",
                std::source_location::current());
            return false;
        }

        s_activeInstance = this;
        running.store(true, std::memory_order_release);

        display = XOpenDisplay(nullptr);
        if (!display)
        {
            epochnamespace::logger::get(kLogSys).log(
                epochnamespace::logger::LogLevel::Error,
                "Failed to open X display",
                std::source_location::current());
            return false;
        }

        screen = DefaultScreen(display);
        global_display = display;

        int fbCount = 0;
        static int visualAttribs[] = {
            GLX_X_RENDERABLE, True,
            GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
            GLX_RENDER_TYPE, GLX_RGBA_BIT,
            GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR,
            GLX_DOUBLEBUFFER, True,
            GLX_RED_SIZE, 8,
            GLX_GREEN_SIZE, 8,
            GLX_BLUE_SIZE, 8,
            GLX_ALPHA_SIZE, 8,
            GLX_DEPTH_SIZE, 24,
            0
        };

        GLXFBConfig* configs = glXChooseFBConfig(display, screen, visualAttribs, &fbCount);
        if (!configs || fbCount == 0)
        {
            epochnamespace::logger::get(kLogSys).log(
                epochnamespace::logger::LogLevel::Error,
                "glXChooseFBConfig failed",
                std::source_location::current());
            if (configs) XFree(configs);
            return false;
        }

        fbConfig = configs[0];
        XFree(configs);

        if (auto* vi = glXGetVisualFromFBConfig(display, fbConfig))
        {
            visualInfo = *vi;
            XFree(vi);
        }
        else
        {
            epochnamespace::logger::get(kLogSys).log(
                epochnamespace::logger::LogLevel::Error,
                "glXGetVisualFromFBConfig failed",
                std::source_location::current());
            return false;
        }

        colormap = XCreateColormap(display, RootWindow(display, screen), visualInfo.visual, AllocNone);
        if (!colormap)
        {
            epochnamespace::logger::get(kLogSys).log(
                epochnamespace::logger::LogLevel::Error,
                "Failed to create X colormap",
                std::source_location::current());
            return false;
        }

        wmDeleteMessage = XInternAtom(display, "WM_DELETE_WINDOW", False);

        InitializeAllContexts();

        auto create_backend_windows = [&](ContextType type, int count)
            {
                if (count <= 0) return;

                std::vector<::Window> created;
                created.reserve(static_cast<size_t>(count));

                std::vector<std::string> narrowTitles;
                narrowTitles.reserve(static_cast<size_t>(count));

                for (int i = 0; i < count; ++i)
                {
                    const std::wstring titleWide = BuildWindowTitle(type, i, effectiveParented);
                    const std::string titleNarrow = epochnamespace::text::narrow_utf8(titleWide);

                    XSetWindowAttributes swa{};
                    swa.colormap = colormap;
                    swa.event_mask =
                        ExposureMask | StructureNotifyMask |
                        KeyPressMask | KeyReleaseMask |
                        ButtonPressMask | ButtonReleaseMask |
                        PointerMotionMask | FocusChangeMask;

                    ::Window win = XCreateWindow(
                        display,
                        RootWindow(display, screen),
                        0, 0,
                        initialWidth, initialHeight,
                        0,
                        visualInfo.depth,
                        InputOutput,
                        visualInfo.visual,
                        CWColormap | CWEventMask,
                        &swa);

                    if (!win)
                    {
                        epochnamespace::logger::get(kLogSys).log(
                            epochnamespace::logger::LogLevel::WARN,
                            "Failed to create X11 window",
                            std::source_location::current());
                        continue;
                    }

                    XStoreName(display, win, titleNarrow.c_str());
                    XSetWMProtocols(display, win, &wmDeleteMessage, 1);
                    XMapWindow(display, win);

                    if (global_window == 0)
                        global_window = win;

                    GLXContext glxCtx = nullptr;
                    if (type == ContextType::OpenGL)
                    {
                        glxCtx = CreateGLXContext();
                        if (!glxCtx)
                        {
                            XDestroyWindow(display, win);
                            continue;
                        }
                    }

                    auto winPtr = std::make_unique<WindowData>(
                        from_xwindow(win),
                        from_display(display),
                        from_glx(glxCtx),
                        true,
                        type);

                    winPtr->titleWide = titleWide;
                    winPtr->titleNarrow = titleNarrow;
                    winPtr->width = initialWidth;
                    winPtr->height = initialHeight;
                    winPtr->running = true;

                    WindowData* raw = winPtr.get();
                    {
                        std::scoped_lock lock(windowsMutex);
                        windows.emplace_back(std::move(winPtr));
                    }

                    (void)raw;
                    created.push_back(win);
                    narrowTitles.push_back(titleNarrow);
                }

                // Acquire master + duplicates for this backend
                std::vector<std::shared_ptr<Context>> contexts;
                {
                    std::unique_lock lock(g_backendsMutex);
                    auto it = g_backends.find(type);
                    if (it == g_backends.end() || !it->second.master)
                    {
                        epochnamespace::logger::get(kLogSys).logf(
                            epochnamespace::logger::LogLevel::Error,
                            std::source_location::current(),
                            "Missing prototype context for backend type {}",
                            static_cast<int>(type));
                        return;
                    }

                    BackendState& backendSlot = it->second;
                    contexts.reserve(static_cast<size_t>(count));
                    contexts.push_back(backendSlot.master);

                    auto ensure_duplicate = [&](size_t index) -> std::shared_ptr<Context>
                        {
                            if (index < backendSlot.duplicates.size())
                            {
                                auto& candidate = backendSlot.duplicates[index];
                                if (candidate && candidate->initialize) return candidate;
                                candidate = CloneContext(*backendSlot.master);
                                return candidate;
                            }
                            auto dup = CloneContext(*backendSlot.master);
                            backendSlot.duplicates.push_back(dup);
                            return dup;
                        };

                    while (contexts.size() < static_cast<size_t>(count))
                        contexts.push_back(ensure_duplicate(contexts.size() - 1));
                }

                const size_t limit = (std::min)(created.size(), contexts.size());
                for (size_t i = 0; i < limit; ++i)
                {
                    ::Window xwin = created[i];
                    HWND hwnd = from_xwindow(xwin);

                    auto* window = findWindowByHWND(hwnd);
                    auto& ctx = contexts[i];
                    if (!window || !ctx) continue;

                    ctx->type = type;
                    ctx->hwnd = hwnd;
                    ctx->hdc = window->hdc;
                    ctx->hglrc = window->glContext;
                    ctx->windowData = window;
                    window->context = ctx;

                    XWindowAttributes attrs{};
                    if (XGetWindowAttributes(display, xwin, &attrs))
                    {
                        UpdateContextDimensions(*ctx, *window,
                            (std::max)(1, attrs.width),
                            (std::max)(1, attrs.height));
                    }
                    else
                    {
                        UpdateContextDimensions(*ctx, *window, initialWidth, initialHeight);
                    }

                    SetupResizeCallback(*window);

#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)
                    if (type == ContextType::OpenGL)
                    {
                        const unsigned width = static_cast<unsigned>((std::max)(1, window->width));
                        const unsigned height = static_cast<unsigned>((std::max)(1, window->height));
                        auto resizeCopy = window->onResize;

                        window->threadInitialize =
                            [ctxWeak = std::weak_ptr<Context>(ctx),
                            width, height,
                            resize = std::move(resizeCopy)](const OpaqueContextHandle& liveCtx) mutable -> bool
                            {
                                auto target = liveCtx ? typed_context(liveCtx) : ctxWeak.lock();
                                if (!target)
                                {
                                    epochnamespace::logger::get(kLogSys).log(
                                        epochnamespace::logger::LogLevel::Error,
                                        "OpenGL context unavailable during thread initialization",
                                        std::source_location::current());
                                    return false;
                                }

                                try
                                {
                                    if (!epochnamespace::openglcontext::opengl_initialize(
                                        target, target->hwnd, width, height, std::move(resize)))
                                    {
                                        epochnamespace::logger::get(kLogSys).logf(
                                            epochnamespace::logger::LogLevel::Error,
                                            std::source_location::current(),
                                            "Failed to initialize OpenGL context for hwnd={}",
                                            target->hwnd);
                                        return false;
                                    }
                                }
                                catch (const std::exception& e)
                                {
                                    epochnamespace::logger::get(kLogSys).logf(
                                        epochnamespace::logger::LogLevel::Error,
                                        std::source_location::current(),
                                        "Exception during OpenGL initialization for hwnd={}: {}",
                                        target->hwnd,
                                        e.what());
                                    return false;
                                }
                                catch (...)
                                {
                                    epochnamespace::logger::get(kLogSys).logf(
                                        epochnamespace::logger::LogLevel::Error,
                                        std::source_location::current(),
                                        "Unknown exception during OpenGL initialization for hwnd={}",
                                        target->hwnd);
                                    return false;
                                }

                                return true;
                            };
                    }
#endif

#if defined(EPOCH_USING_SOFTWARE_RENDERER)
                    if (type == ContextType::Software)
                    {
                        const unsigned width = static_cast<unsigned>((std::max)(1, window->width));
                        const unsigned height = static_cast<unsigned>((std::max)(1, window->height));

                        if (!epochnamespace::anativecontext::softrenderer_initialize(
                            ctx, nullptr, width, height, window->onResize))
                        {
                            epochnamespace::logger::get(kLogSys).logf(
                                epochnamespace::logger::LogLevel::Error,
                                std::source_location::current(),
                                "Failed to initialize Software renderer for hwnd={}",
                                ctx->hwnd);
                            window->running = false;
                        }
                    }
#endif

#if defined(EPOCH_USING_SDL) && (EPOCH_USING_SDL == 1)
                    if (type == ContextType::SDL)
                    {
                        const int width = (std::max)(1, window->width);
                        const int height = (std::max)(1, window->height);
                        const std::string title = (i < narrowTitles.size())
                            ? narrowTitles[i] : std::string("SDL Dock");
                        auto resizeCopy = window->onResize;

                        window->threadInitialize =
                            [ctxWeak = std::weak_ptr<Context>(ctx),
                            width, height, title,
                            resize = std::move(resizeCopy)](const OpaqueContextHandle& liveCtx) mutable -> bool
                            {
                                auto target = liveCtx ? typed_context(liveCtx) : ctxWeak.lock();
                                if (!target)
                                {
                                    epochnamespace::logger::get(kLogSys).log(
                                        epochnamespace::logger::LogLevel::Error,
                                        "SDL context unavailable during thread initialization",
                                        std::source_location::current());
                                    return false;
                                }

                                if (!epochnamespace::sdlcontext::sdl_initialize(
                                    target, nullptr, width, height, std::move(resize), title))
                                {
                                    epochnamespace::logger::get(kLogSys).logf(
                                        epochnamespace::logger::LogLevel::Error,
                                        std::source_location::current(),
                                        "Failed to initialize SDL context for hwnd={}",
                                        target->hwnd);
                                    return false;
                                }

                                return true;
                            };
                    }
#endif

#if defined(EPOCH_USING_RAYLIB) && (EPOCH_USING_RAYLIB == 1)
                    if (type == ContextType::RayLib)
                    {
                        const unsigned width = static_cast<unsigned>((std::max)(1, window->width));
                        const unsigned height = static_cast<unsigned>((std::max)(1, window->height));
                        const std::string title = (i < narrowTitles.size())
                            ? narrowTitles[i] : std::string("Raylib Dock");
                        auto resizeCopy = window->onResize;

                        window->threadInitialize =
                            [ctxWeak = std::weak_ptr<Context>(ctx),
                            width, height, title,
                            resize = std::move(resizeCopy)](const OpaqueContextHandle& liveCtx) mutable -> bool
                            {
                                auto target = liveCtx ? typed_context(liveCtx) : ctxWeak.lock();
                                if (!target)
                                {
                                    epochnamespace::logger::get(kLogSys).log(
                                        epochnamespace::logger::LogLevel::Error,
                                        "RayLib context unavailable during thread initialization",
                                        std::source_location::current());
                                    return false;
                                }

                                if (!epochnamespace::raylibcontext::raylib_initialize(
                                    target, nullptr, width, height, std::move(resize), title))
                                {
                                    epochnamespace::logger::get(kLogSys).logf(
                                        epochnamespace::logger::LogLevel::Error,
                                        std::source_location::current(),
                                        "Failed to initialize RayLib context for hwnd={}",
                                        target->hwnd);
                                    return false;
                                }

                                return true;
                            };
                    }
#endif

#if defined(EPOCH_USING_SFML)
                    if (type == ContextType::SFML)
                    {
                        const unsigned width = static_cast<unsigned>((std::max)(1, window->width));
                        const unsigned height = static_cast<unsigned>((std::max)(1, window->height));
                        const std::string title = (i < narrowTitles.size())
                            ? narrowTitles[i] : std::string("SFML Dock");
                        auto resizeCopy = window->onResize;

                        window->threadInitialize =
                            [ctxWeak = std::weak_ptr<Context>(ctx),
                            width, height, title,
                            resize = std::move(resizeCopy)](const OpaqueContextHandle& liveCtx) mutable -> bool
                            {
                                auto target = liveCtx ? typed_context(liveCtx) : ctxWeak.lock();
                                if (!target)
                                {
                                    epochnamespace::logger::get(kLogSys).log(
                                        epochnamespace::logger::LogLevel::Error,
                                        "SFML context unavailable during thread initialization",
                                        std::source_location::current());
                                    return false;
                                }

                                if (!epochnamespace::sfmlcontext::sfml_initialize(
                                    target, nullptr, width, height, std::move(resize), title))
                                {
                                    epochnamespace::logger::get(kLogSys).logf(
                                        epochnamespace::logger::LogLevel::Error,
                                        std::source_location::current(),
                                        "Failed to initialize SFML context for hwnd={}",
                                        target->hwnd);
                                    return false;
                                }

                                return true;
                            };
                    }
#endif
                }
            };

        create_backend_windows(ContextType::RayLib, RayLibWinCount);
        create_backend_windows(ContextType::SDL, SDLWinCount);
        create_backend_windows(ContextType::SFML, SFMLWinCount);
        create_backend_windows(ContextType::Vulkan, VulkanWinCount);
        create_backend_windows(ContextType::OpenGL, OpenGLWinCount);
        create_backend_windows(ContextType::Software, SoftwareWinCount);

        return true;
    }

    void MultiContextManager::StopAll()
    {
        running.store(false, std::memory_order_release);

        {
            std::scoped_lock lock(windowsMutex);
            for (auto& win : windows)
                if (win) win->running = false;
        }

        for (auto& [win, thread] : threads)
            if (thread.joinable()) thread.join();
        threads.clear();

        {
            std::scoped_lock lock(windowsMutex);
            for (auto& win : windows)
                if (win) DestroyWindowData(*win);
            windows.clear();
        }

        if (colormap)
        {
            XFreeColormap(display, colormap);
            colormap = 0;
        }

        if (display)
        {
            if (global_display == display)
            {
                global_display = nullptr;
                global_window = 0;
            }
            XCloseDisplay(display);
            display = nullptr;
        }

        if (s_activeInstance == this)
            s_activeInstance = nullptr;
    }

    bool MultiContextManager::IsRunning() const noexcept
    {
        return running.load(std::memory_order_acquire);
    }

    void MultiContextManager::StopRunning() noexcept
    {
        running.store(false, std::memory_order_release);
    }

    GLXContext MultiContextManager::CreateGLXContext()
    {
        if (!display) return nullptr;

        auto* createContextAttribs =
            reinterpret_cast<PFNGLXCREATECONTEXTATTRIBSARBPROC>(
                glXGetProcAddressARB(reinterpret_cast<const GLubyte*>("glXCreateContextAttribsARB")));

        int contextAttribs[] = {
            GLX_CONTEXT_MAJOR_VERSION_ARB, 3,
            GLX_CONTEXT_MINOR_VERSION_ARB, 3,
            GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
            0
        };

        GLXContext shared = sharedContext;
        GLXContext ctx = nullptr;

        if (createContextAttribs)
            ctx = createContextAttribs(display, fbConfig, shared, True, contextAttribs);

        if (!ctx)
            ctx = glXCreateNewContext(display, fbConfig, GLX_RGBA_TYPE, shared, True);

        if (!sharedContext && ctx)
            sharedContext = ctx;

        return ctx;
    }

    void MultiContextManager::DestroyWindowData(WindowData& win)
    {
        forget_native_title_frame_source(&win);

        ::Window xwin = to_xwindow(win.hwnd);
        GLXContext glx = to_glx(win.glContext);

        if (display && glx)
        {
            glXDestroyContext(display, glx);
            if (sharedContext == glx)
                sharedContext = nullptr;
        }

        if (display && xwin)
        {
            XDestroyWindow(display, xwin);
            if (global_window == xwin)
                global_window = 0;
        }

        win.glContext = nullptr;
        win.hwnd = nullptr;
        win.hdc = nullptr;
        win.context.reset();
    }

    void MultiContextManager::AddWindow(
        HWND hwnd,
        HWND /*parent*/,
        HDC hdc,
        HGLRC glContext,
        bool usesSharedContext,
        ResizeCallback onResize,
        ContextType type)
    {
        if (!hwnd) return;

        if (!display)
            display = to_display(hdc);

        if (!display) return;

        ::Window xwin = to_xwindow(hwnd);
        Display* localDisplay = display;

        if (type == ContextType::OpenGL)
        {
            if (!glContext)
                glContext = from_glx(CreateGLXContext());
        }

        bool needInit = false;
        {
            std::shared_lock lock(g_backendsMutex);
            needInit = g_backends.empty();
        }
        if (needInit)
            InitializeAllContexts();

        std::shared_ptr<Context> ctx;
        {
            std::unique_lock lock(g_backendsMutex);
            auto& backendSlot = g_backends[type];

            if (!backendSlot.master)
            {
                ctx = std::make_shared<Context>();
                ctx->type = type;
                backendSlot.master = ctx;
            }
            else if (!backendSlot.master->windowData)
            {
                ctx = backendSlot.master;
            }
            else
            {
                auto it = std::find_if(
                    backendSlot.duplicates.begin(),
                    backendSlot.duplicates.end(),
                    [](const std::shared_ptr<Context>& dup) { return dup && !dup->windowData; });

                if (it != backendSlot.duplicates.end())
                    ctx = *it;
                else
                {
                    auto dup = CloneContext(*backendSlot.master);
                    backendSlot.duplicates.push_back(dup);
                    ctx = dup;
                }
            }
        }

        if (!ctx) return;

        ctx->type = type;
        ctx->hwnd = hwnd;
        ctx->hdc = hdc ? hdc : from_display(localDisplay);
        ctx->hglrc = glContext;

        auto winPtr = std::make_unique<WindowData>(
            hwnd,
            hdc ? hdc : from_display(localDisplay),
            glContext,
            usesSharedContext,
            type);

        winPtr->running = true;
        winPtr->onResize = std::move(onResize);
        winPtr->context = ctx;
        ctx->windowData = winPtr.get();

        XWindowAttributes attrs{};
        if (XGetWindowAttributes(localDisplay, xwin, &attrs))
        {
            UpdateContextDimensions(*ctx, *winPtr,
                (std::max)(1, attrs.width),
                (std::max)(1, attrs.height));
        }
        else
        {
            UpdateContextDimensions(
                *ctx,
                *winPtr,
                cli::updater_shell_requested ? cli::window_width : kDefaultWidth,
                cli::updater_shell_requested ? cli::window_height : kDefaultHeight);
        }

        SetupResizeCallback(*winPtr);

#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)
        if (type == ContextType::OpenGL)
        {
            const unsigned width = static_cast<unsigned>((std::max)(1, winPtr->width));
            const unsigned height = static_cast<unsigned>((std::max)(1, winPtr->height));
            auto resizeCopy = winPtr->onResize;

            winPtr->threadInitialize =
                [ctxWeak = std::weak_ptr<Context>(ctx),
                width, height,
                resize = std::move(resizeCopy)](const OpaqueContextHandle& liveCtx) mutable -> bool
                {
                    auto target = liveCtx ? typed_context(liveCtx) : ctxWeak.lock();
                    if (!target)
                    {
                        epochnamespace::logger::get(kLogSys).log(
                            epochnamespace::logger::LogLevel::Error,
                            "OpenGL context unavailable during thread initialization",
                            std::source_location::current());
                        return false;
                    }

                    try
                    {
                        if (!epochnamespace::openglcontext::opengl_initialize(
                            target, target->hwnd, width, height, std::move(resize)))
                        {
                            epochnamespace::logger::get(kLogSys).logf(
                                epochnamespace::logger::LogLevel::Error,
                                std::source_location::current(),
                                "Failed to initialize OpenGL context for hwnd={}",
                                target->hwnd);
                            return false;
                        }
                    }
                    catch (const std::exception& e)
                    {
                        epochnamespace::logger::get(kLogSys).logf(
                            epochnamespace::logger::LogLevel::Error,
                            std::source_location::current(),
                            "Exception during OpenGL initialization for hwnd={}: {}",
                            target->hwnd,
                            e.what());
                        return false;
                    }
                    catch (...)
                    {
                        epochnamespace::logger::get(kLogSys).logf(
                            epochnamespace::logger::LogLevel::Error,
                            std::source_location::current(),
                            "Unknown exception during OpenGL initialization for hwnd={}",
                            target->hwnd);
                        return false;
                    }

                    return true;
                };
        }
#endif

#if defined(EPOCH_USING_SDL) && (EPOCH_USING_SDL == 1)
        if (type == ContextType::SDL)
        {
            const int width = (std::max)(1, winPtr->width);
            const int height = (std::max)(1, winPtr->height);
            const std::string title = !winPtr->titleNarrow.empty()
                ? winPtr->titleNarrow
                : std::string("SDL Dock");
            auto resizeCopy = winPtr->onResize;

            winPtr->threadInitialize =
                [ctxWeak = std::weak_ptr<Context>(ctx),
                width, height, title,
                resize = std::move(resizeCopy)](const OpaqueContextHandle& liveCtx) mutable -> bool
                {
                    auto target = liveCtx ? typed_context(liveCtx) : ctxWeak.lock();
                    if (!target)
                    {
                        epochnamespace::logger::get(kLogSys).log(
                            epochnamespace::logger::LogLevel::Error,
                            "SDL context unavailable during thread initialization",
                            std::source_location::current());
                        return false;
                    }

                    if (!epochnamespace::sdlcontext::sdl_initialize(
                        target, nullptr, width, height, std::move(resize), title))
                    {
                        epochnamespace::logger::get(kLogSys).logf(
                            epochnamespace::logger::LogLevel::Error,
                            std::source_location::current(),
                            "Failed to initialize SDL context for hwnd={}",
                            target->hwnd);
                        return false;
                    }

                    return true;
                };
        }
#endif

#if defined(EPOCH_USING_RAYLIB) && (EPOCH_USING_RAYLIB == 1)
        if (type == ContextType::RayLib)
        {
            const unsigned width = static_cast<unsigned>((std::max)(1, winPtr->width));
            const unsigned height = static_cast<unsigned>((std::max)(1, winPtr->height));
            const std::string title = !winPtr->titleNarrow.empty()
                ? winPtr->titleNarrow
                : std::string("Raylib Dock");
            auto resizeCopy = winPtr->onResize;

            winPtr->threadInitialize =
                [ctxWeak = std::weak_ptr<Context>(ctx),
                width, height, title,
                resize = std::move(resizeCopy)](const OpaqueContextHandle& liveCtx) mutable -> bool
                {
                    auto target = liveCtx ? typed_context(liveCtx) : ctxWeak.lock();
                    if (!target)
                    {
                        epochnamespace::logger::get(kLogSys).log(
                            epochnamespace::logger::LogLevel::Error,
                            "RayLib context unavailable during thread initialization",
                            std::source_location::current());
                        return false;
                    }

                    if (!epochnamespace::raylibcontext::raylib_initialize(
                        target, nullptr, width, height, std::move(resize), title))
                    {
                        epochnamespace::logger::get(kLogSys).logf(
                            epochnamespace::logger::LogLevel::Error,
                            std::source_location::current(),
                            "Failed to initialize RayLib context for hwnd={}",
                            target->hwnd);
                        return false;
                    }

                    return true;
                };
        }
#endif

#if defined(EPOCH_USING_SFML)
        if (type == ContextType::SFML)
        {
            const unsigned width = static_cast<unsigned>((std::max)(1, winPtr->width));
            const unsigned height = static_cast<unsigned>((std::max)(1, winPtr->height));
            const std::string title = !winPtr->titleNarrow.empty()
                ? winPtr->titleNarrow
                : std::string("SFML Dock");
            auto resizeCopy = winPtr->onResize;

            winPtr->threadInitialize =
                [ctxWeak = std::weak_ptr<Context>(ctx),
                width, height, title,
                resize = std::move(resizeCopy)](const OpaqueContextHandle& liveCtx) mutable -> bool
                {
                    auto target = liveCtx ? typed_context(liveCtx) : ctxWeak.lock();
                    if (!target)
                    {
                        epochnamespace::logger::get(kLogSys).log(
                            epochnamespace::logger::LogLevel::Error,
                            "SFML context unavailable during thread initialization",
                            std::source_location::current());
                        return false;
                    }

                    if (!epochnamespace::sfmlcontext::sfml_initialize(
                        target, nullptr, width, height, std::move(resize), title))
                    {
                        epochnamespace::logger::get(kLogSys).logf(
                            epochnamespace::logger::LogLevel::Error,
                            std::source_location::current(),
                            "Failed to initialize SFML context for hwnd={}",
                            target->hwnd);
                        return false;
                    }

                    return true;
                };
        }
#endif

        WindowData* raw = winPtr.get();
        {
            std::scoped_lock lock(windowsMutex);
            windows.emplace_back(std::move(winPtr));
        }

        threads[xwin] = std::thread([this, raw]()
            {
                RenderLoop(*raw);
            });
    }

    void MultiContextManager::RemoveWindow(HWND hwnd)
    {
        if (!hwnd) return;

        std::unique_ptr<WindowData> removed;

        {
            std::scoped_lock lock(windowsMutex);
            auto it = std::find_if(windows.begin(), windows.end(),
                [hwnd](const std::unique_ptr<WindowData>& w) { return w && w->hwnd == hwnd; });

            if (it == windows.end()) return;

            // IMPORTANT: do NOT write (*it)->running=false here from this thread.
            // It's a plain bool and would race the render thread -> possible infinite join.
            removed = std::move(*it);
            windows.erase(it);
        }

        // Ask the render thread to stop itself (no cross-thread data race).
        if (removed)
        {
            WindowData* raw = removed.get();
            raw->EnqueueCommand([raw]()
                {
                    raw->running = false;
                });
        }

        ::Window xwin = to_xwindow(hwnd);
        auto threadIt = threads.find(xwin);
        if (threadIt != threads.end())
        {
            if (threadIt->second.joinable()) threadIt->second.join();
            threads.erase(threadIt);
        }

        if (removed)
        {
            if (auto liveContext = typed_context(removed->context);
                liveContext && liveContext->windowData == removed.get())
            {
                liveContext->windowData = nullptr;
            }

            DestroyWindowData(*removed);

            if (global_window == 0)
            {
                std::scoped_lock lock(windowsMutex);
                for (const auto& candidate : windows)
                {
                    if (candidate && candidate->hwnd)
                    {
                        global_window = to_xwindow(candidate->hwnd);
                        break;
                    }
                }
            }
        }
    }


    void MultiContextManager::ArrangeDockedWindowsGrid()
    {
        if (!display) return;

        struct WindowPlacement
        {
            HWND hwnd{};
            int x{};
            int y{};
            int width{};
            int height{};
        };

        std::vector<WindowPlacement> placements;
        {
            std::scoped_lock lock(windowsMutex);
            if (windows.empty()) return;
            if (windows.size() <= 1u) return;

            const int total = static_cast<int>(windows.size());
            int cols = 1;
            int rows = 1;
            while (cols * rows < total)
                (cols <= rows ? ++cols : ++rows);

            const int screenIndex = screen >= 0 ? screen : DefaultScreen(display);
            const int screenWidth = DisplayWidth(display, screenIndex);
            const int screenHeight = DisplayHeight(display, screenIndex);

            const int cellWidth = clamp_positive(screenWidth / cols);
            const int cellHeight = clamp_positive(screenHeight / rows);

            placements.reserve(windows.size());
            for (size_t i = 0; i < windows.size(); ++i)
            {
                const int column = static_cast<int>(i) % cols;
                const int row = static_cast<int>(i) / cols;

                WindowData& win = *windows[i];
                if (!win.hwnd)
                    continue;

                placements.push_back(WindowPlacement{
                    .hwnd = win.hwnd,
                    .x = column * cellWidth,
                    .y = row * cellHeight,
                    .width = cellWidth,
                    .height = cellHeight,
                });
            }
        }

        for (const auto& placement : placements)
        {
            ::Window xwin = to_xwindow(placement.hwnd);
            if (!xwin)
                continue;

            XMoveResizeWindow(
                display,
                xwin,
                placement.x,
                placement.y,
                static_cast<unsigned>(placement.width),
                static_cast<unsigned>(placement.height));

            HandleResize(placement.hwnd, placement.width, placement.height);
        }

        XFlush(display);
    }

    void MultiContextManager::HandleResize(HWND hwnd, int width, int height)
    {
        if (!hwnd) return;

        const int clampedWidth = (std::max)(1, width);
        const int clampedHeight = (std::max)(1, height);

        std::function<void(int, int)> resizeCallback;
        core::ContextType contextType = core::ContextType::Custom;
        std::uintptr_t windowId = 0;
        WindowData* window = nullptr;

        {
            std::scoped_lock lock(windowsMutex);

            auto it = std::find_if(windows.begin(), windows.end(),
                [hwnd](const std::unique_ptr<WindowData>& w) { return w && w->hwnd == hwnd; });

            if (it == windows.end()) return;

            window = it->get();
            window->width = clampedWidth;
            window->height = clampedHeight;

            if (window->context)
            {
                if (auto liveContext = typed_context(window->context))
                {
                    liveContext->width = clampedWidth;
                    liveContext->height = clampedHeight;
                    contextType = liveContext->type;

                    if (liveContext->onResize)
                        resizeCallback = liveContext->onResize;
                }
            }
            else
            {
                contextType = window->type;
            }

            if (!resizeCallback && window->onResize)
                resizeCallback = window->onResize;

            if (window->hwnd)
                windowId = reinterpret_cast<std::uintptr_t>(window->hwnd);
        }

        if (window)
        {
            window->commandQueue.enqueue([cb = std::move(resizeCallback),
                contextType,
                windowId,
                clampedWidth, clampedHeight]() mutable
                {
                    telemetry::emit_counter(
                        "renderer.resize.count",
                        1,
                        telemetry::RendererTelemetryTags{ contextType, windowId });
                    telemetry::emit_gauge(
                        "renderer.resize.latest_dimensions",
                        clampedWidth,
                        telemetry::RendererTelemetryTags{ contextType, windowId, "width" });
                    telemetry::emit_gauge(
                        "renderer.resize.latest_dimensions",
                        clampedHeight,
                        telemetry::RendererTelemetryTags{ contextType, windowId, "height" });

                    if (cb) cb(clampedWidth, clampedHeight);
                });
        }
    }

    void MultiContextManager::StartRenderThreads()
    {
        std::scoped_lock lock(windowsMutex);
        std::size_t launchIndex = 0;
        for (const auto& win : windows)
        {
            if (!win) continue;

            ::Window xwin = to_xwindow(win->hwnd);
            if (!threads.contains(xwin))
            {
                WindowData* raw = win.get();
                const auto startupDelay =
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        kRenderThreadStartupStepDelay * static_cast<int>(launchIndex++));
                threads[xwin] = std::thread([this, raw, startupDelay]()
                    {
                        if (startupDelay.count() > 0)
                        {
                            epochnamespace::logger::get(kLogSys).logf(
                                epochnamespace::logger::LogLevel::INFO,
                                std::source_location::current(),
                                "Startup stagger: delaying render thread {} by {} ms.",
                                static_cast<void*>(raw ? raw->hwnd : nullptr),
                                startupDelay.count());
                            std::this_thread::sleep_for(startupDelay);
                        }
                        RenderLoop(*raw);
                    });
            }
        }
    }

    void MultiContextManager::EnqueueRenderCommand(HWND hwnd, RenderCommand cmd)
    {
        std::scoped_lock lock(windowsMutex);
        auto it = std::find_if(windows.begin(), windows.end(),
            [hwnd](const std::unique_ptr<WindowData>& w) { return w && w->hwnd == hwnd; });

        if (it != windows.end() && *it)
            (*it)->EnqueueCommand(std::move(cmd));
    }

    WindowData* MultiContextManager::findWindowByHWND(HWND hwnd)
    {
        std::scoped_lock lock(windowsMutex);
        auto it = std::find_if(windows.begin(), windows.end(),
            [hwnd](const std::unique_ptr<WindowData>& w) { return w && w->hwnd == hwnd; });

        return (it != windows.end()) ? it->get() : nullptr;
    }

    const WindowData* MultiContextManager::findWindowByHWND(HWND hwnd) const
    {
        std::scoped_lock lock(windowsMutex);
        auto it = std::find_if(windows.begin(), windows.end(),
            [hwnd](const std::unique_ptr<WindowData>& w) { return w && w->hwnd == hwnd; });

        return (it != windows.end()) ? it->get() : nullptr;
    }

    WindowData* MultiContextManager::findWindowByContext(const std::shared_ptr<core::Context>& ctx)
    {
        if (!ctx) return nullptr;

        std::scoped_lock lock(windowsMutex);
        auto it = std::find_if(windows.begin(), windows.end(),
            [&](const std::unique_ptr<WindowData>& w)
            {
                return w && w->context && w->context.get() == static_cast<void*>(ctx.get());
            });

        return (it != windows.end()) ? it->get() : nullptr;
    }

    const WindowData* MultiContextManager::findWindowByContext(const std::shared_ptr<core::Context>& ctx) const
    {
        if (!ctx) return nullptr;

        std::scoped_lock lock(windowsMutex);
        auto it = std::find_if(windows.begin(), windows.end(),
            [&](const std::unique_ptr<WindowData>& w)
            {
                return w && w->context && w->context.get() == static_cast<void*>(ctx.get());
            });

        return (it != windows.end()) ? it->get() : nullptr;
    }

    void MultiContextManager::RenderLoop(WindowData& win)
    {
        auto ctx = typed_context(win.context);
        if (!ctx)
        {
            win.running = false;
            return;
        }

        Display* localDisplay = to_display(win.hdc);
        ::Window xwin = to_xwindow(win.hwnd);
        GLXContext glxCtx = to_glx(win.glContext);

        if (glxCtx && localDisplay)
        {
            glXMakeCurrent(localDisplay, xwin, glxCtx);

#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1) || defined(EPOCH_USING_SDL)
            static std::atomic<bool> gladInitialized{ false };
            if (!gladInitialized.load(std::memory_order_acquire))
            {
                epochnamespace::openglcontext::PlatformGL::PlatformGLContext finalCtx{};
                finalCtx.display = localDisplay;
                finalCtx.drawable = xwin;
                finalCtx.context = glxCtx;

                epochnamespace::openglcontext::PlatformGL::ScopedContext contextGuard{ finalCtx };
                if (!contextGuard.ok())
                {
                    epochnamespace::logger::get(kLogSys).log(
                        epochnamespace::logger::LogLevel::Error,
                        "PlatformGL::make_current(final) failed on Linux",
                        std::source_location::current());
                }
                else if (gladLoadGLLoader(reinterpret_cast<GLADloadproc>(
                    epochnamespace::openglcontext::PlatformGL::get_proc_address)))
                {
                    gladInitialized.store(true, std::memory_order_release);
                }
                else
                {
                    epochnamespace::logger::get(kLogSys).log(
                        epochnamespace::logger::LogLevel::Error,
                        "Failed to load OpenGL functions via GLAD on Linux",
                        std::source_location::current());
                }
            }
#endif
        }

        ctx->windowData = &win;
        MultiContextManager::SetCurrent(ctx);

        struct ResetGuard
        {
            Display* display{};
            GLXContext ctx{};
            ~ResetGuard()
            {
                if (display && ctx)
                    glXMakeCurrent(display, 0, nullptr);

                MultiContextManager::SetCurrent(nullptr);
            }
        } reset{ localDisplay, glxCtx };

        bool initializedOnThread = false;
        if (win.threadInitialize)
        {
            auto init = std::move(win.threadInitialize);
            win.threadInitialize = nullptr;
            initializedOnThread = true;

            if (!init || !init(ctx))
            {
                win.running = false;
                return;
            }
        }

        const bool skipGenericInit =
            initializedOnThread ||
#if defined(EPOCH_USING_SFML)
            (ctx->type == ContextType::SFML) ||
#endif
#if defined(EPOCH_USING_RAYLIB) && (EPOCH_USING_RAYLIB == 1)
            (ctx->type == ContextType::RayLib) ||
#endif
#if defined(EPOCH_USING_SDL) && (EPOCH_USING_SDL == 1)
            (ctx->type == ContextType::SDL) ||
#endif
            false;

        if (!skipGenericInit)
        {
            if (ctx->initialize)
                ctx->initialize_safe();
            else
            {
                win.running = false;
                return;
            }
        }

        if (ctx->init_failed)
        {
            epochnamespace::logger::get(kLogSys).logf(
                epochnamespace::logger::LogLevel::Error,
                std::source_location::current(),
                "Backend init failed for {}. Keeping window alive with no-op process.",
                ctx->backendName);
            ctx->process = nullptr;
        }

        while (running.load(std::memory_order_acquire) && win.running)
        {
            bool keepRunning = true;

            {
                std::size_t depth = 0;
                {
                    depth = win.commandQueue.depth();
                }
                telemetry::emit_gauge(
                    "renderer.command_queue.depth",
                    static_cast<std::int64_t>(depth),
                    telemetry::RendererTelemetryTags{
                        ctx->type,
                        reinterpret_cast<std::uintptr_t>(win.hwnd)
                    });
            }

            if (ctx->process)
                keepRunning = ctx->process_safe(ctx, win.commandQueue);
            else
                win.commandQueue.drain();

            if (!keepRunning)
            {
                win.running = false;
                break;
            }

            record_native_title_frame(localDisplay, xwin, win);

            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }

        win.commandQueue.drain();

        if (ctx->cleanup)
            ctx->cleanup_safe();
    }

} // namespace epochnamespace::core

namespace epochnamespace::platform
{
    bool pump_events()
    {
        Display* display = global_display;
        if (!display)
        {
            return true;
        }

        epochnamespace::core::clear_linux_input_state();

        bool keepRunning = true;
        while (XPending(display) > 0)
        {
            XEvent event{};
            XNextEvent(display, &event);

            switch (event.type)
            {
            case ConfigureNotify:
                epochnamespace::core::HandleX11Configure(
                    event.xconfigure.window,
                    event.xconfigure.width,
                    event.xconfigure.height);
                break;
            case MotionNotify:
            {
                epochnamespace::input::mouseX.store(event.xmotion.x, std::memory_order_relaxed);
                epochnamespace::input::mouseY.store(event.xmotion.y, std::memory_order_relaxed);
                epochnamespace::input::set_mouse_coords_are_global(false);
                break;
            }
            case ButtonPress:
            {
                epochnamespace::input::mouseX.store(event.xbutton.x, std::memory_order_relaxed);
                epochnamespace::input::mouseY.store(event.xbutton.y, std::memory_order_relaxed);
                epochnamespace::input::set_mouse_coords_are_global(false);

                if (event.xbutton.button == Button4)
                {
                    epochnamespace::input::mouseWheel.fetch_add(120, std::memory_order_relaxed);
                    break;
                }
                if (event.xbutton.button == Button5)
                {
                    epochnamespace::input::mouseWheel.fetch_add(-120, std::memory_order_relaxed);
                    break;
                }

                if (const auto button = epochnamespace::core::map_mouse_button(event.xbutton.button))
                {
                    std::unique_lock lock(epochnamespace::input::g_inputMutex);
                    const auto index = static_cast<size_t>(*button);
                    if (!epochnamespace::input::mouseDown.test(index))
                        epochnamespace::input::mousePressed.set(index);
                    epochnamespace::input::mouseDown.set(index);
                }
                break;
            }
            case ButtonRelease:
            {
                epochnamespace::input::mouseX.store(event.xbutton.x, std::memory_order_relaxed);
                epochnamespace::input::mouseY.store(event.xbutton.y, std::memory_order_relaxed);
                epochnamespace::input::set_mouse_coords_are_global(false);

                if (const auto button = epochnamespace::core::map_mouse_button(event.xbutton.button))
                {
                    std::unique_lock lock(epochnamespace::input::g_inputMutex);
                    epochnamespace::input::mouseDown.reset(static_cast<size_t>(*button));
                }
                break;
            }
            case KeyPress:
            {
                const auto key = epochnamespace::core::map_keysym(XLookupKeysym(&event.xkey, 0));
                if (key != epochnamespace::input::Key::Unknown)
                {
                    std::unique_lock lock(epochnamespace::input::g_inputMutex);
                    const auto index = static_cast<size_t>(key);
                    if (!epochnamespace::input::keyDown.test(index))
                        epochnamespace::input::keyPressed.set(index);
                    epochnamespace::input::keyDown.set(index);
                }
                break;
            }
            case KeyRelease:
            {
                const auto key = epochnamespace::core::map_keysym(XLookupKeysym(&event.xkey, 0));
                if (key != epochnamespace::input::Key::Unknown)
                {
                    std::unique_lock lock(epochnamespace::input::g_inputMutex);
                    epochnamespace::input::keyDown.reset(static_cast<size_t>(key));
                }
                break;
            }
            case FocusOut:
            {
                std::unique_lock lock(epochnamespace::input::g_inputMutex);
                epochnamespace::input::keyDown.reset();
                epochnamespace::input::mouseDown.reset();
                break;
            }
            case ClientMessage:
            {
                const Atom wmDelete = XInternAtom(display, "WM_DELETE_WINDOW", False);
                if (static_cast<Atom>(event.xclient.data.l[0]) == wmDelete)
                {
                    auto* mgr = epochnamespace::core::GetActiveMultiContextManager();
                    if (mgr)
                    {
                        HWND hwnd = reinterpret_cast<HWND>(static_cast<std::uintptr_t>(event.xclient.window));
                        mgr->RemoveWindow(hwnd);
                        if (mgr->GetWindows().empty())
                            keepRunning = false;
                    }
                    else
                    {
                        keepRunning = false;
                    }
                }
                break;
            }
            case DestroyNotify:
            {
                auto* mgr = epochnamespace::core::GetActiveMultiContextManager();
                if (mgr)
                {
                    HWND hwnd = reinterpret_cast<HWND>(static_cast<std::uintptr_t>(event.xdestroywindow.window));
                    mgr->RemoveWindow(hwnd);
                    if (mgr->GetWindows().empty())
                        keepRunning = false;
                }
                break;
            }
            default:
                break;
            }
        }

        return keepRunning;
    }
} // namespace epochnamespace::platform

#endif // __linux__
