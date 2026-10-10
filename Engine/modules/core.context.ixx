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
module;

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <shared_mutex>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <include/engine.config.hpp> // for EPOCH_USING Macros

#if defined(_WIN32) && !defined(EPOCH_MAIN_HEADLESS)
// Preprocessor hygiene MUST come before windows.h
#   ifndef WIN32_LEAN_AND_MEAN
#       define WIN32_LEAN_AND_MEAN
#   endif
#   ifndef NOMINMAX
#       define NOMINMAX
#   endif
#   ifndef _WINSOCKAPI_
#       define _WINSOCKAPI_
#   endif

    // Optional: if you still need your framework helpers, include it AFTER windows.h
#   include <../src/platform/platform.framework.hpp>
#   ifdef min
#       undef min
#   endif
#   ifdef max
#       undef max
#   endif
#endif

export module core.context;

// Std

// Project
import visuals.engine;
import context.type;
import context.commandqueue;
import context.window;
import utility.atomicfunction;
import input.engine;
import atlas.texture;
import atlas.manager;   // reacquire atlas vector inside queued draw
import sprite.handle;
import image.loader;
import perf.tier;
import render.context_frame;

namespace epochengine::core
{
    class MultiContextManager;

    // Forward declare so we can use Context in exported function signatures.
    export class Context;

    // ---------------------------------------------------------------------
    // Render-thread "current context" storage
    // (MultiContextManager sets this per-render-thread before backend work.)
    // ---------------------------------------------------------------------
    export void set_current_render_context(std::shared_ptr<Context> ctx) noexcept;
    export std::shared_ptr<Context> get_current_render_context() noexcept;

    namespace detail
    {
        inline thread_local std::shared_ptr<Context> t_current_render_context{};
    }

    inline void set_current_render_context(std::shared_ptr<Context> ctx) noexcept
    {
        detail::t_current_render_context = std::move(ctx);
    }

    inline std::shared_ptr<Context> get_current_render_context() noexcept
    {
        return detail::t_current_render_context;
    }

    export using ClearColor = std::array<float, 4>;

    export struct RenderViewport
    {
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return width > 0 && height > 0;
        }
    };

    export enum class ScenePreviewMode : std::uint8_t
    {
        None = 0,
        Editor = 1
    };

    export [[nodiscard]] constexpr ClearColor clear_color_for_context(ContextType type) noexcept
    {
        switch (type)
        {
        case ContextType::Vulkan:
        case ContextType::OpenGL:
        case ContextType::DirectX:
        case ContextType::RayLib:
        case ContextType::SDL:
        case ContextType::SFML:
        case ContextType::Software:
            return epochengine::visuals::frame_background();
        default:
            return { 0.0f, 0.0f, 0.0f, 1.0f };
        }
    }

    // ---------------------------------------------------------------------
    // Core Context
    // ---------------------------------------------------------------------
    export class Context
    {
        friend class MultiContextManager;

    public:
        using InitializeFunc = void(*)();
        using CleanupFunc = void(*)();
        using ProcessFunc = bool(*)(std::shared_ptr<Context>, core::CommandQueue&);
        using ClearFunc = void(*)();
        using PresentFunc = void(*)();
        using GetWidthFunc = int(*)();
        using GetHeightFunc = int(*)();
        using RegistryGetFunc = int(*)(const char*);

        using DrawSpriteFunc = void(*)(SpriteHandle,
            std::span<const TextureAtlas* const>,
            float, float, float, float);

        using AddTextureFunc = std::uint32_t(*)(TextureAtlas&, std::string, const ImageData&);
        using AddAtlasFunc = std::uint32_t(*)(const TextureAtlas&);
        using AddModelFunc = int(*)(const char*, const char*);
        using ApplyFramePacingFunc = std::function<perf::native_frame_pacing_result(
            perf::frame_pacing_mode,
            double)>;

        Context() = default;

        Context(std::string name,
            core::ContextType t,
            InitializeFunc init,
            CleanupFunc cln,
            ProcessFunc proc,
            ClearFunc clr,
            PresentFunc pres,
            GetWidthFunc gw,
            GetHeightFunc gh,
            RegistryGetFunc rg,
            DrawSpriteFunc ds,
            AddTextureFunc at,
            AddAtlasFunc aa,
            AddModelFunc am)
            : type(t),
            backendName(std::move(name)),
            initialize(init),
            cleanup(cln),
            process(proc),
            clear(clr),
            present(pres),
            get_width(gw),
            get_height(gh),
            registry_get(rg),
            draw_sprite(ds),
            add_model(am)
        {
            add_texture.store(at);
            add_atlas.store(aa);
        }

        // -----------------------------------------------------------------
        // Lifecycle
        // -----------------------------------------------------------------
        void initialize_safe() noexcept;
        void cleanup_safe() noexcept;

        // Keep as your existing out-of-line implementation.
        bool process_safe(std::shared_ptr<Context> ctx, core::CommandQueue& queue);

        // -----------------------------------------------------------------
        // Render-thread routed wrappers
        // -----------------------------------------------------------------
        void clear_safe() const noexcept
        {
            if (!clear) return;

            // Fast path: already on THIS context's render thread.
            if (auto cur = core::get_current_render_context(); cur && cur.get() == this)
            {
                clear();
                return;
            }

            if (!windowData) return;

            // Assumption: WindowData owns `std::shared_ptr<Context> context;`
            // (your multiplexer sets it that way). If your field name differs, rename it.
            const auto clear_fn = clear;
            windowData->commandQueue.enqueue([clear_fn]()
                {
                    if (clear_fn)
                        clear_fn();
                });
        }

        void clear_safe(std::shared_ptr<Context>) const noexcept { clear_safe(); }

        void present_safe() const noexcept
        {
            if (!present) return;

            if (auto cur = core::get_current_render_context(); cur && cur.get() == this)
            {
                present();
                return;
            }

            if (!windowData) return;

            const auto present_fn = present;
            windowData->commandQueue.enqueue([present_fn]()
                {
                    if (present_fn)
                        present_fn();
                });
        }

        int get_width_safe() const noexcept
        {
            if (get_width)
            {
                const int resolved = get_width();
                if (resolved > 0)
                    return resolved;
            }
            if (framebufferWidth > 0)
                return framebufferWidth;
            return width;
        }

        int get_height_safe() const noexcept
        {
            if (get_height)
            {
                const int resolved = get_height();
                if (resolved > 0)
                    return resolved;
            }
            if (framebufferHeight > 0)
                return framebufferHeight;
            return height;
        }

        void publish_frame_window_state(
            const rendercontext::WindowState& state)
        {
            std::scoped_lock lock{frameWindowStateMutex};
            frameWindowState = state;
        }

        [[nodiscard]] rendercontext::WindowState frame_window_state() const
        {
            std::scoped_lock lock{frameWindowStateMutex};
            return frameWindowState;
        }

        [[nodiscard]] perf::frame_activity frame_pacing_activity() const
        {
            switch (frame_window_state().activity)
            {
            case rendercontext::WindowActivity::background:
            case rendercontext::WindowActivity::occluded:
                return perf::frame_activity::background;
            case rendercontext::WindowActivity::minimized:
                return perf::frame_activity::minimized;
            case rendercontext::WindowActivity::foreground:
                return perf::frame_activity::foreground;
            }
            return perf::frame_activity::foreground;
        }

        [[nodiscard]] RenderViewport scene_viewport() const noexcept
        {
            for (int attempt = 0; attempt < 4; ++attempt)
            {
                const std::uint32_t beginRevision = sceneViewportRevision.load(std::memory_order_acquire);
                if ((beginRevision & 1u) != 0u)
                    continue;

                RenderViewport viewport{
                    sceneViewportX.load(std::memory_order_relaxed),
                    sceneViewportY.load(std::memory_order_relaxed),
                    sceneViewportWidth.load(std::memory_order_relaxed),
                    sceneViewportHeight.load(std::memory_order_relaxed)
                };

                const std::uint32_t endRevision = sceneViewportRevision.load(std::memory_order_acquire);
                if (beginRevision == endRevision && (endRevision & 1u) == 0u)
                    return viewport;
            }

            return RenderViewport{
                sceneViewportX.load(std::memory_order_acquire),
                sceneViewportY.load(std::memory_order_acquire),
                sceneViewportWidth.load(std::memory_order_acquire),
                sceneViewportHeight.load(std::memory_order_acquire)
            };
        }

        void set_scene_viewport(RenderViewport viewport) noexcept
        {
            sceneViewportRevision.fetch_add(1u, std::memory_order_acq_rel);
            sceneViewportX.store((std::max)(0, viewport.x), std::memory_order_relaxed);
            sceneViewportY.store((std::max)(0, viewport.y), std::memory_order_relaxed);
            sceneViewportWidth.store((std::max)(0, viewport.width), std::memory_order_relaxed);
            sceneViewportHeight.store((std::max)(0, viewport.height), std::memory_order_relaxed);
            sceneViewportRevision.fetch_add(1u, std::memory_order_release);
        }

        void clear_scene_viewport() noexcept
        {
            set_scene_viewport({});
        }

        [[nodiscard]] ScenePreviewMode scene_preview_mode() const noexcept
        {
            return static_cast<ScenePreviewMode>(scenePreviewMode.load(std::memory_order_relaxed));
        }

        void set_scene_preview_mode(ScenePreviewMode mode) noexcept
        {
            scenePreviewMode.store(static_cast<std::uint8_t>(mode), std::memory_order_relaxed);
        }

        [[nodiscard]] bool gui_overlay_priority() const noexcept
        {
            return guiOverlayPriority.load(std::memory_order_relaxed);
        }

        void set_gui_overlay_priority(bool enabled) noexcept
        {
            guiOverlayPriority.store(enabled, std::memory_order_relaxed);
        }

#if defined(_WIN32) && !defined(EPOCH_MAIN_HEADLESS)
        [[nodiscard]] bool owns_input_window_safe(HWND candidate) const noexcept
        {
            if (!candidate || ::IsWindow(candidate) == FALSE)
                return false;

            const auto matches = [candidate](HWND owned) noexcept
            {
                return owned
                    && ::IsWindow(owned) != FALSE
                    && (candidate == owned || ::IsChild(owned, candidate));
            };

            if (matches(hwnd))
                return true;
            if (windowData)
            {
                if (matches(windowData->hwndChild)
                    || matches(windowData->host_hwnd)
                    || matches(windowData->hwnd))
                {
                    return true;
                }
            }
            return false;
        }
#endif

        [[nodiscard]] bool has_input_focus_safe() const noexcept
        {
#if defined(_WIN32) && !defined(EPOCH_MAIN_HEADLESS)
            const HWND foreground = ::GetForegroundWindow();
            DWORD foregroundProcessId{};
            const DWORD foregroundThread = ::GetWindowThreadProcessId(foreground, &foregroundProcessId);
            if (!foreground
                || foregroundThread == 0
                || foregroundProcessId != ::GetCurrentProcessId())
            {
                return false;
            }

            GUITHREADINFO guiInfo{};
            guiInfo.cbSize = sizeof(guiInfo);
            if (!::GetGUIThreadInfo(foregroundThread, &guiInfo))
                return false;
            const HWND focused = guiInfo.hwndFocus ? guiInfo.hwndFocus : guiInfo.hwndActive;

            // Focus can temporarily report only the top-level foreground host
            // while a child backend is transitioning. Accept that host only when
            // it belongs to this context; never accept another Epoch viewport.
            return owns_input_window_safe(focused)
                || (!focused && owns_input_window_safe(foreground));
#else
            return true;
#endif
        }

        // Hover-wheel delivery is allowed to an uncovered client without
        // granting keyboard, button, capture or scene-camera input.
        [[nodiscard]] bool has_hover_scroll_safe() const noexcept
        {
#if defined(_WIN32) && !defined(EPOCH_MAIN_HEADLESS)
            POINT cursor{};
            return ::GetCursorPos(&cursor)
                && owns_input_window_safe(::WindowFromPoint(cursor));
#else
            return true;
#endif
        }

        [[nodiscard]] bool has_pointer_input_safe() const noexcept
        {
#if defined(_WIN32) && !defined(EPOCH_MAIN_HEADLESS)
            if (!has_input_focus_safe())
                return false;
            GUITHREADINFO guiInfo{};
            guiInfo.cbSize = sizeof(guiInfo);
            const DWORD thread = ::GetWindowThreadProcessId(::GetForegroundWindow(), nullptr);
            if (!thread || !::GetGUIThreadInfo(thread, &guiInfo))
                return false;
            // Captured drags may leave the client area, but covered/inactive
            // contexts must never poll another window's mouse buttons.
            if (owns_input_window_safe(guiInfo.hwndCapture))
                return true;
            POINT cursor{};
            return ::GetCursorPos(&cursor)
                && owns_input_window_safe(::WindowFromPoint(cursor));
#else
            return true;
#endif
        }

        bool is_key_held_safe(input::Key k) const noexcept
        {
#if defined(_WIN32) && !defined(EPOCH_MAIN_HEADLESS)
            if ((hwnd || windowData) && !has_input_focus_safe())
                return false;
#endif
            return is_key_held ? is_key_held(k) : false;
        }

        bool is_key_down_safe(input::Key k) const noexcept
        {
#if defined(_WIN32) && !defined(EPOCH_MAIN_HEADLESS)
            if ((hwnd || windowData) && !has_input_focus_safe())
                return false;
#endif
            return is_key_down ? is_key_down(k) : false;
        }

        void get_mouse_position_safe(int& x, int& y) const noexcept
        {
#if defined(_WIN32) && !defined(EPOCH_MAIN_HEADLESS)
            if (HWND hwndLocal = get_hwnd(); hwndLocal != nullptr)
            {
                POINT cursor{};
                if (::GetCursorPos(&cursor) && ::ScreenToClient(hwndLocal, &cursor))
                {
                    x = cursor.x;
                    y = cursor.y;
                    if (normalize_mouse_position)
                        normalize_mouse_position(x, y);
                    return;
                }
            }
#endif

            if (get_mouse_position)
            {
                get_mouse_position(x, y);
            }
            else
            {
                x = input::mouseX.load(std::memory_order_acquire);
                y = input::mouseY.load(std::memory_order_acquire);
            }

#if defined(_WIN32) && !defined(EPOCH_MAIN_HEADLESS)
            if (input::are_mouse_coords_global())
            {
                if (HWND hwndLocal = get_hwnd(); hwndLocal != nullptr)
                {
                    POINT clientPoint{ x, y };
                    if (::ScreenToClient(hwndLocal, &clientPoint))
                    {
                        x = clientPoint.x;
                        y = clientPoint.y;
                    }
                }
            }
#endif

            if (normalize_mouse_position)
                normalize_mouse_position(x, y);
        }

        bool is_mouse_button_held_safe(input::MouseButton b) const noexcept
        {
#if defined(_WIN32) && !defined(EPOCH_MAIN_HEADLESS)
            if ((hwnd || windowData) && !has_pointer_input_safe())
                return false;
#endif
            // Keep held/pressed semantics in the input engine. Calling
            // GetAsyncKeyState here would bypass the per-frame edge tracker and,
            // for the pressed query below, turn a held button into a fresh press
            // every frame.
            return is_mouse_button_held ? is_mouse_button_held(b)
                : input::is_mouse_button_held(b);
        }

        bool is_mouse_button_down_safe(input::MouseButton b) const noexcept
        {
#if defined(_WIN32) && !defined(EPOCH_MAIN_HEADLESS)
            if ((hwnd || windowData) && !has_pointer_input_safe())
                return false;
#endif
            return is_mouse_button_down ? is_mouse_button_down(b)
                : input::is_mouse_button_down(b);
        }

        [[nodiscard]] bool action_modifiers_satisfied_safe(
            const input::ActionBinding& binding) const noexcept
        {
            const bool control = is_key_held_safe(input::Key::LeftControl)
                || is_key_held_safe(input::Key::RightControl);
            const bool shift = is_key_held_safe(input::Key::LeftShift)
                || is_key_held_safe(input::Key::RightShift);
            const bool alt = is_key_held_safe(input::Key::LeftAlt)
                || is_key_held_safe(input::Key::RightAlt);
            return (!binding.control || control)
                && (!binding.shift || shift)
                && (!binding.alt || alt);
        }

        [[nodiscard]] bool action_held_safe(input::Action action) const
        {
            const input::ActionBinding binding = input::binding_for(action);
            if (!action_modifiers_satisfied_safe(binding))
                return false;
            if (binding.mouse != input::MouseButton::MouseCount
                && is_mouse_button_held_safe(binding.mouse))
            {
                return true;
            }
            return (binding.primary != input::Key::Unknown
                    && is_key_held_safe(binding.primary))
                || (binding.secondary != input::Key::Unknown
                    && is_key_held_safe(binding.secondary));
        }

        [[nodiscard]] bool action_pressed_safe(input::Action action) const
        {
            const input::ActionBinding binding = input::binding_for(action);
            if (!action_modifiers_satisfied_safe(binding))
                return false;
            if (binding.mouse != input::MouseButton::MouseCount
                && is_mouse_button_down_safe(binding.mouse))
            {
                return true;
            }
            return (binding.primary != input::Key::Unknown
                    && is_key_down_safe(binding.primary))
                || (binding.secondary != input::Key::Unknown
                    && is_key_down_safe(binding.secondary));
        }

        int registry_get_safe(const char* key) const noexcept
        {
            return registry_get ? registry_get(key) : 0;
        }

        void draw_sprite_safe(
            SpriteHandle sprite,
            std::span<const TextureAtlas* const> atlases,
            float x, float y, float w, float hgt) const noexcept
        {
            if (!draw_sprite) return;

            // Fast path: already on THIS context's render thread.
            if (auto cur = core::get_current_render_context(); cur && cur.get() == this)
            {
                draw_sprite(sprite, atlases, x, y, w, hgt);
                return;
            }

            if (!windowData) return;

            const core::RenderPath renderPath =
                (type == core::ContextType::OpenGL) ? core::RenderPath::OpenGL
                : (type == core::ContextType::SFML) ? core::RenderPath::SFML
                : (type == core::ContextType::DirectX) ? core::RenderPath::DirectX
                : core::RenderPath::Unknown;

            const auto draw_fn = draw_sprite;

            // Do NOT capture `atlases` (span may reference transient storage).
            // Re-acquire atlas vector on render thread.
            windowData->commandQueue.enqueue([draw_fn, sprite, x, y, w, hgt]()
                {
                    if (!draw_fn) return;

                    // Snapshot by value (safe)
                    auto av = epochengine::atlasmanager::get_atlas_vector_snapshot();
                    std::span<const TextureAtlas* const> span(av.data(), av.size());
                    draw_fn(sprite, span, x, y, w, hgt);
                }, renderPath);
        }

        std::uint32_t add_texture_safe(TextureAtlas& atlas,
            std::string name,
            const ImageData& img) const noexcept
        {
            try { return add_texture(atlas, std::move(name), img); }
            catch (...) { return 0u; }
        }

        std::uint32_t add_atlas_safe(const TextureAtlas& atlas) const noexcept
        {
            try { return add_atlas(atlas); }
            catch (...) { return 0u; }
        }

        int add_model_safe(const char* name, const char* path) const noexcept
        {
            return add_model ? add_model(name, path) : -1;
        }

        auto get_hdc() const noexcept { return hdc; }
        auto get_hglrc() const noexcept { return hglrc; }

        // Legacy public pointer (kept on purpose)
        WindowData* windowData = nullptr;

        void* native_window = nullptr;
        void* native_drawable = nullptr;
        void* native_gl_context = nullptr;

#if defined(_WIN32) && !defined(EPOCH_MAIN_HEADLESS)
        HWND get_hwnd() const noexcept { return hwnd; }
        HWND  hwnd = nullptr;
        HDC   hdc = nullptr;
        HGLRC hglrc = nullptr;
#else
        void* get_hwnd() const noexcept { return hwnd; }
        void* hwnd = nullptr;
        void* hdc = nullptr;
        void* hglrc = nullptr;
#endif

        // logical canvas
        int width = 400;
        int height = 300;

        // physical framebuffer size
        int framebufferWidth = 400;
        int framebufferHeight = 300;

        mutable std::mutex frameWindowStateMutex{};
        rendercontext::WindowState frameWindowState{};

        std::atomic<int> sceneViewportX{ 0 };
        std::atomic<int> sceneViewportY{ 0 };
        std::atomic<int> sceneViewportWidth{ 0 };
        std::atomic<int> sceneViewportHeight{ 0 };
        std::atomic<std::uint32_t> sceneViewportRevision{ 0 };
        std::atomic<std::uint8_t> scenePreviewMode{ static_cast<std::uint8_t>(ScenePreviewMode::None) };
        std::atomic_bool guiOverlayPriority{ false };

        // virtual design canvas
        int virtualWidth = 400;
        int virtualHeight = 300;

        bool init_failed = false;

        ContextType type = ContextType::Custom;
        std::string backendName{};

        // Backend function pointers
        InitializeFunc  initialize = nullptr;
        CleanupFunc     cleanup = nullptr;
        ProcessFunc     process = nullptr;
        ClearFunc       clear = nullptr;
        PresentFunc     present = nullptr;
        GetWidthFunc    get_width = nullptr;
        GetHeightFunc   get_height = nullptr;
        RegistryGetFunc registry_get = nullptr;
        DrawSpriteFunc  draw_sprite = nullptr;
        AddModelFunc    add_model = nullptr;
        perf::frame_pacing_capabilities frame_pacing_capabilities{};
        ApplyFramePacingFunc apply_frame_pacing{};

        // Input hooks
        std::function<bool(input::Key)>         is_key_held;
        std::function<bool(input::Key)>         is_key_down;
        std::function<void(int&, int&)>         get_mouse_position; // client coords
        std::function<void(int&, int&)>         normalize_mouse_position;
        std::function<bool(input::MouseButton)> is_mouse_button_held;
        std::function<bool(input::MouseButton)> is_mouse_button_down;

        // High-level callbacks
        core::EpochAtomicFunction<std::uint32_t(TextureAtlas&, std::string, const ImageData&)> add_texture;
        core::EpochAtomicFunction<std::uint32_t(const TextureAtlas&)>                         add_atlas;
        std::function<void(int, int)>                                                          onResize;
    };

    // ---------------------------------------------------------------------
    // Backend registry (existing)
    // ---------------------------------------------------------------------
    export struct BackendState
    {
        std::shared_ptr<Context>              master;
        std::vector<std::shared_ptr<Context>> duplicates;
        std::unique_ptr<void, void(*)(void*)> data{ nullptr, [](void*) {} };
    };

    export using BackendMap = std::map<core::ContextType, BackendState>;

    export extern BackendMap        g_backends;
    export extern std::shared_mutex g_backendsMutex;

    export extern void InitializeAllContexts();
    export std::shared_ptr<Context> CloneContext(const Context& prototype);
    export void AddContextForBackend(core::ContextType type, std::shared_ptr<Context> context);
    bool ProcessAllContexts();
} // namespace epochengine::core
