module;

#include <include/aengine.config.hpp> // for ALMOND_USING Macros

#if defined(_WIN32) && !defined(ALMOND_MAIN_HEADLESS)
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
#   include <include/aframework.hpp>
#endif

export module aengine.core.context;


// Std
import <algorithm>;
import <array>;
import <atomic>;
import <cstdint>;
import <functional>;
import <map>;
import <memory>;
import <mutex>;
import <queue>;
import <shared_mutex>;
import <span>;
import <string>;
import <utility>;
import <vector>;

// Project
import aengine.context.type;
import aengine.context.commandqueue;
import aengine.context.window;
import aatomicfunction;
import aengine.input;
import aatlas.texture;
import aatlas.manager;   // reacquire atlas vector inside queued draw
import aspritehandle;
import aimage.loader;
import acontext.raylib.api; // for ImageData

export namespace almondnamespace::core
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

    export [[nodiscard]] constexpr ClearColor clear_color_for_context(ContextType type) noexcept
    {
        // Palette: Vulkan = cyan, OpenGL = blue, Raylib = deep red, SDL = amber,
        // SFML = blue, Software = violet.
        switch (type)
        {
        case ContextType::Vulkan:
            return { 0.0f, 1.0f, 1.0f, 1.0f };
        case ContextType::OpenGL:
            return { 0.0f, 0.0f, 1.0f, 1.0f };
        case ContextType::RayLib:
            return { 0.6f, 0.0f, 0.0f, 1.0f };
        case ContextType::SDL:
            return { 1.0f, 0.75f, 0.0f, 1.0f };
        case ContextType::SFML:
            return { 0.0f, 0.0f, 1.0f, 1.0f };
        case ContextType::Software:
            return { 0.5f, 0.0f, 1.0f, 1.0f };
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
        friend class aengine;

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
        void initialize_safe() const noexcept { if (initialize) initialize(); }
        void cleanup_safe()    const noexcept { if (cleanup) cleanup(); }

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
            std::weak_ptr<Context> weak = windowData->context;
            windowData->commandQueue.enqueue([weak]()
                {
                    if (auto self = weak.lock(); self && self->clear)
                        self->clear();
                });
        }

        // Back-compat shim
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

            std::weak_ptr<Context> weak = windowData->context;
            windowData->commandQueue.enqueue([weak]()
                {
                    if (auto self = weak.lock(); self && self->present)
                        self->present();
                });
        }

        int get_width_safe()  const noexcept { return get_width ? get_width() : width; }
        int get_height_safe() const noexcept { return get_height ? get_height() : height; }

        bool is_key_held_safe(input::Key k) const noexcept
        {
            return is_key_held ? is_key_held(k) : false;
        }

        bool is_key_down_safe(input::Key k) const noexcept
        {
            return is_key_down ? is_key_down(k) : false;
        }

        void get_mouse_position_safe(int& x, int& y) const noexcept
        {
            if (get_mouse_position)
            {
                get_mouse_position(x, y);
                return;
            }

            x = input::mouseX.load(std::memory_order_acquire);
            y = input::mouseY.load(std::memory_order_acquire);
        }

        bool is_mouse_button_held_safe(input::MouseButton b) const noexcept
        {
            return is_mouse_button_held ? is_mouse_button_held(b)
                : input::is_mouse_button_held(b);
        }

        bool is_mouse_button_down_safe(input::MouseButton b) const noexcept
        {
            return is_mouse_button_down ? is_mouse_button_down(b)
                : input::is_mouse_button_down(b);
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
                : core::RenderPath::Unknown;

            std::weak_ptr<Context> weak = windowData->context;

            // Do NOT capture `atlases` (span may reference transient storage).
            // Re-acquire atlas vector on render thread.
            windowData->commandQueue.enqueue([weak, sprite, x, y, w, hgt]()
                {
                    auto self = weak.lock();
                    if (!self || !self->draw_sprite) return;

                    // Snapshot by value (safe)
                    auto av = almondnamespace::atlasmanager::get_atlas_vector_snapshot();
                    std::span<const TextureAtlas* const> span(av.data(), av.size());
                    self->draw_sprite(sprite, span, x, y, w, hgt);
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

#if defined(_WIN32) && !defined(ALMOND_MAIN_HEADLESS)
        HWND  get_hwnd()  const noexcept { return hwnd; }
        HDC   get_hdc()   const noexcept { return hdc; }
        HGLRC get_hglrc() const noexcept { return hglrc; }
#endif

        // Legacy public pointer (kept on purpose)
        WindowData* windowData = nullptr;

        void* native_window = nullptr;
        void* native_drawable = nullptr;
        void* native_gl_context = nullptr;

#if defined(_WIN32) && !defined(ALMOND_MAIN_HEADLESS)
        HWND  hwnd = nullptr;
        HDC   hdc = nullptr;
        HGLRC hglrc = nullptr;
#endif

        // logical canvas
        int width = 400;
        int height = 300;

        // physical framebuffer size
        int framebufferWidth = 400;
        int framebufferHeight = 300;

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

        // Input hooks
        std::function<bool(input::Key)>         is_key_held;
        std::function<bool(input::Key)>         is_key_down;
        std::function<void(int&, int&)>         get_mouse_position; // client coords
        std::function<bool(input::MouseButton)> is_mouse_button_held;
        std::function<bool(input::MouseButton)> is_mouse_button_down;

        // High-level callbacks
        core::AlmondAtomicFunction<std::uint32_t(TextureAtlas&, std::string, const ImageData&)> add_texture;
        core::AlmondAtomicFunction<std::uint32_t(const TextureAtlas&)>                         add_atlas;
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

    using BackendMap = std::map<core::ContextType, BackendState>;

    extern BackendMap        g_backends;
    extern std::shared_mutex g_backendsMutex;

    extern void InitializeAllContexts();
    std::shared_ptr<Context> CloneContext(const Context& prototype);
    void AddContextForBackend(core::ContextType type, std::shared_ptr<Context> context);
    bool ProcessAllContexts();
} // namespace almondnamespace::core
