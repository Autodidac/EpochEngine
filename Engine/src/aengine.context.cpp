/**************************************************************
 *   █████╗ ██╗     ███╗   ███╗   ███╗   ██╗    ██╗██████╗
 *  ██╔══██╗██║     ████╗ ████║ ██╔═══██╗████╗  ██║██╔══██╗
 *  ███████║██║     ██╔████╔██║ ██║   ██║██╔██╗ ██║██║  ██║
 *  ██╔══██║██║     ██║╚██╔╝██║ ██║   ██║██║╚██╗██║██║  ██║
 *  ██║  ██║███████╗██║ ╚═╝ ██║ ╚██████╔╝██║ ╚████║██████╔╝
 *  ╚═╝  ╚═╝╚══════╝╚═╝     ╚═╝  ╚═════╝ ╚═╝  ╚═══╝╚═════╝
 *
 *   This file is part of the Almond Project.
 *   AlmondShell - Modular C++ Framework
 *
 *   SPDX-License-Identifier: LicenseRef-MIT-NoSell
 **************************************************************/
 //
 // aengine.context.cpp  (TU implementation; NOT a module interface)
 //

#include <include/aengine.config.hpp> // macros only — must NOT include windows

import <algorithm>;
import <cstdint>;
import <format>;
import <map>;
import <memory>;
import <mutex>;
import <queue>;
import <shared_mutex>;
import <span>;
import <source_location>;
import <stdexcept>;
import <string>;
import <string_view>;
import <utility>;
import <vector>;

import aengine.platform;

import aengine.input;
import aengine.context.type;
import aengine.core.context;
import aengine.core.logger;
//import aengine.context.window;
import aengine.context.multiplexer;

import aatlas.manager;
import aatlas.texture;
import aimage.loader;

#ifdef ALMOND_USING_VULKAN
import acontext.vulkan.context;
//import acontext.vulkan.context:renderer;
//import acontext.vulkan.context:texture;
#endif

#ifdef ALMOND_USING_DIRECTX
import "adirectxcontext.hpp";
import "adirectxrenderer.hpp";
import "adirectxtextures.hpp";
#endif
#ifdef ALMOND_USING_SFML
import acontext.sfml.context;
import acontext.sfml.textures;
#endif
#ifdef ALMOND_USING_CUSTOM
import "acustomcontext.hpp";
import "acustomrenderer.hpp";
import "acustomtextures.hpp";
#endif

#if defined(ALMOND_USING_OPENGL)
import acontext.opengl.context;
import acontext.opengl.textures;
#endif
#if defined(ALMOND_USING_SDL)
import acontext.sdl.context;
import acontext.sdl.textures;
#endif
#if defined(ALMOND_USING_RAYLIB)
import acontext.raylib.context;
import acontext.raylib.renderer;
import acontext.raylib.state;
#endif
#if defined(ALMOND_USING_SOFTWARE_RENDERER)
import acontext.softrenderer.context;
#endif
#if defined(ALMOND_USING_NOOP_HEADLESS)
import acontext.noop.context;
#endif

namespace
{
    constexpr std::string_view kLogOpenGL = "Context.OpenGL";
    constexpr std::string_view kLogVulkan = "Context.Vulkan";
    constexpr std::string_view kLogSoftRenderer = "Context.SoftRenderer";
    constexpr std::string_view kLogSfml = "Context.SFML";
    constexpr std::string_view kLogSdl = "Context.SDL";
    constexpr std::string_view kLogRaylib = "Context.Raylib";

    // ------------------------------------------------------------
    // Atlas helpers that do NOT depend on removed backend APIs.
    // ------------------------------------------------------------
    std::uint32_t default_add_atlas(const almondnamespace::TextureAtlas& atlas,
        almondnamespace::core::ContextType type) noexcept
    {
        try {
            almondnamespace::atlasmanager::ensure_uploaded(atlas);
            almondnamespace::atlasmanager::process_pending_uploads(type);
        }
        catch (...) {}

        const int idx = atlas.get_index();
        return static_cast<std::uint32_t>(idx >= 0 ? idx + 1 : 1);
    }

    std::uint32_t default_add_texture(almondnamespace::TextureAtlas&, std::string,
        const almondnamespace::ImageData&) noexcept
    {
        return 0u;
    }

    // ------------------------------------------------------------
    // Helpers: keep core TU platform-neutral.
    // We never name HWND here; we pass opaque handles through.
    // ------------------------------------------------------------
    inline void* ctx_native_window_handle(const std::shared_ptr<almondnamespace::core::Context>& ctx) noexcept
    {
        if (!ctx) return nullptr;
        if (auto h = ctx->get_hwnd()) return h;               // assumed void*/opaque
        if (ctx->windowData && ctx->windowData->hwnd) return ctx->windowData->hwnd; // assumed void*/opaque
        return nullptr;
    }

#if defined(ALMOND_USING_OPENGL)
    void opengl_initialize_adapter()
    {
        auto ctx = almondnamespace::core::MultiContextManager::GetCurrent();
        if (!ctx) return;

        void* native = ctx_native_window_handle(ctx);

        const unsigned w = static_cast<unsigned>((std::max)(1, ctx->width));
        const unsigned h = static_cast<unsigned>((std::max)(1, ctx->height));

        try {
            // backend owns the platform cast
            (void)almondnamespace::openglcontext::opengl_initialize(ctx, native, w, h, ctx->onResize);
        }
        catch (const std::exception& e) {
            almondnamespace::logger::get(kLogOpenGL).logf(
                almondnamespace::logger::LogLevel::ALMOND_ERROR,
                std::source_location::current(),
                "init exception: {}",
                e.what());
        }
        catch (...) {
            almondnamespace::logger::get(kLogOpenGL).log(
                almondnamespace::logger::LogLevel::ALMOND_ERROR,
                "init unknown exception",
                std::source_location::current());
        }
    }

    void opengl_cleanup_adapter()
    {
        auto ctx = almondnamespace::core::MultiContextManager::GetCurrent();
        if (!ctx) return;

        try { almondnamespace::openglcontext::opengl_cleanup(ctx); }
        catch (const std::exception& e) {
            almondnamespace::logger::get(kLogOpenGL).logf(
                almondnamespace::logger::LogLevel::ALMOND_ERROR,
                std::source_location::current(),
                "cleanup exception: {}",
                e.what());
        }
        catch (...) {
            almondnamespace::logger::get(kLogOpenGL).log(
                almondnamespace::logger::LogLevel::ALMOND_ERROR,
                "cleanup unknown exception",
                std::source_location::current());
        }
    }

    bool opengl_process_adapter(std::shared_ptr<almondnamespace::core::Context> ctx,
        almondnamespace::core::CommandQueue& queue)
    {
        if (!ctx) return false;
        return almondnamespace::openglcontext::opengl_process(ctx, queue);
    }
#endif

#if defined(ALMOND_USING_VULKAN)
    void vulkan_initialize_adapter()
    {
        auto ctx = almondnamespace::core::MultiContextManager::GetCurrent();
        if (!ctx) return;

        void* native = ctx_native_window_handle(ctx);

        const unsigned w = static_cast<unsigned>((std::max)(1, ctx->width));
        const unsigned h = static_cast<unsigned>((std::max)(1, ctx->height));

        ctx->init_failed = false;
        try {
            (void)almondnamespace::vulkancontext::vulkan_initialize(ctx, native, w, h, ctx->onResize);
        }
        catch (const std::exception& e) {
            ctx->init_failed = true;
            almondnamespace::logger::get(kLogVulkan).logf(
                almondnamespace::logger::LogLevel::ALMOND_ERROR,
                std::source_location::current(),
                "init exception: {}",
                e.what());
        }
        catch (...) {
            ctx->init_failed = true;
            almondnamespace::logger::get(kLogVulkan).log(
                almondnamespace::logger::LogLevel::ALMOND_ERROR,
                "init unknown exception",
                std::source_location::current());
        }
    }

    void vulkan_cleanup_adapter()
    {
        auto ctx = almondnamespace::core::MultiContextManager::GetCurrent();
        if (!ctx) return;

        try { almondnamespace::vulkancontext::vulkan_cleanup(ctx); }
        catch (const std::exception& e) {
            almondnamespace::logger::get(kLogVulkan).logf(
                almondnamespace::logger::LogLevel::ALMOND_ERROR,
                std::source_location::current(),
                "cleanup exception: {}",
                e.what());
        }
        catch (...) {
            almondnamespace::logger::get(kLogVulkan).log(
                almondnamespace::logger::LogLevel::ALMOND_ERROR,
                "cleanup unknown exception",
                std::source_location::current());
        }
    }

    bool vulkan_process_adapter(std::shared_ptr<almondnamespace::core::Context> ctx,
        almondnamespace::core::CommandQueue& queue)
    {
        if (!ctx) return false;
        return almondnamespace::vulkancontext::vulkan_process(ctx, queue);
    }
#endif

#if defined(ALMOND_USING_SOFTWARE_RENDERER)
    void softrenderer_initialize_adapter()
    {
        auto ctx = almondnamespace::core::MultiContextManager::GetCurrent();
        if (!ctx) return;

        try {
            (void)almondnamespace::anativecontext::softrenderer_initialize(
                ctx,
                ctx->get_hwnd(),
                static_cast<unsigned>((std::max)(1, ctx->width)),
                static_cast<unsigned>((std::max)(1, ctx->height)),
                ctx->onResize
            );
        }
        catch (const std::exception& e) {
            almondnamespace::logger::get(kLogSoftRenderer).logf(
                almondnamespace::logger::LogLevel::ALMOND_ERROR,
                std::source_location::current(),
                "init exception: {}",
                e.what());
        }
        catch (...) {
            almondnamespace::logger::get(kLogSoftRenderer).log(
                almondnamespace::logger::LogLevel::ALMOND_ERROR,
                "init unknown exception",
                std::source_location::current());
        }
    }

    void softrenderer_cleanup_adapter()
    {
        auto ctx = almondnamespace::core::MultiContextManager::GetCurrent();
        if (!ctx) return;

        try {
            auto copy = ctx;
            almondnamespace::anativecontext::softrenderer_cleanup(copy);
        }
        catch (const std::exception& e) {
            almondnamespace::logger::get(kLogSoftRenderer).logf(
                almondnamespace::logger::LogLevel::ALMOND_ERROR,
                std::source_location::current(),
                "cleanup exception: {}",
                e.what());
        }
        catch (...) {
            almondnamespace::logger::get(kLogSoftRenderer).log(
                almondnamespace::logger::LogLevel::ALMOND_ERROR,
                "cleanup unknown exception",
                std::source_location::current());
        }
    }

    bool softrenderer_process_adapter(std::shared_ptr<almondnamespace::core::Context> ctx,
        almondnamespace::core::CommandQueue& queue)
    {
        if (!ctx) return false;
        return almondnamespace::anativecontext::softrenderer_process(*ctx, queue);
    }
#endif

#if defined(ALMOND_USING_SFML)
    void sfml_initialize_adapter()
    {
        auto ctx = almondnamespace::core::MultiContextManager::GetCurrent();
        if (!ctx) return;

        void* native = ctx_native_window_handle(ctx);

        const unsigned w = static_cast<unsigned>((std::max)(1, ctx->width));
        const unsigned h = static_cast<unsigned>((std::max)(1, ctx->height));

        try {
            std::string windowTitle{};
            if (ctx->windowData)
                windowTitle = ctx->windowData->titleNarrow;
            (void)almondnamespace::sfmlcontext::sfml_initialize(
                ctx,
                reinterpret_cast<HWND>(native),
                w,
                h,
                ctx->onResize,
                windowTitle
            );
        }
        catch (const std::exception& e) {
            almondnamespace::logger::get(kLogSfml).logf(
                almondnamespace::logger::LogLevel::ALMOND_ERROR,
                std::source_location::current(),
                "init exception: {}",
                e.what());
        }
        catch (...) {
            almondnamespace::logger::get(kLogSfml).log(
                almondnamespace::logger::LogLevel::ALMOND_ERROR,
                "init unknown exception",
                std::source_location::current());
        }
    }

    void sfml_cleanup_adapter()
    {
        if (auto ctx = almondnamespace::core::MultiContextManager::GetCurrent()) {
            auto copy = ctx;
            almondnamespace::sfmlcontext::sfml_cleanup(copy);
        }
    }

    bool sfml_process_adapter(std::shared_ptr<almondnamespace::core::Context> ctx,
        almondnamespace::core::CommandQueue& queue)
    {
        if (!ctx) return false;
        return almondnamespace::sfmlcontext::sfml_process(ctx, queue);
    }
#endif

#if defined(ALMOND_USING_SDL)
    void sdl_initialize_adapter()
    {
        auto ctx = almondnamespace::core::MultiContextManager::GetCurrent();
        if (!ctx) return;

        HWND parent = ctx->get_hwnd();
        if (!parent && ctx->windowData) parent = ctx->windowData->hwnd;

        try {
            (void)almondnamespace::sdlcontext::sdl_initialize(
                ctx,
                parent,
                static_cast<int>((std::max)(1, ctx->width)),
                static_cast<int>((std::max)(1, ctx->height)),
                ctx->onResize,
                ctx->backendName
            );
        }
        catch (const std::exception& e) {
            almondnamespace::logger::get(kLogSdl).logf(
                almondnamespace::logger::LogLevel::ALMOND_ERROR,
                std::source_location::current(),
                "init exception: {}",
                e.what());
        }
        catch (...) {
            almondnamespace::logger::get(kLogSdl).log(
                almondnamespace::logger::LogLevel::ALMOND_ERROR,
                "init unknown exception",
                std::source_location::current());
        }
    }

    void sdl_cleanup_adapter()
    {
        auto ctx = almondnamespace::core::MultiContextManager::GetCurrent();
        if (!ctx) return;

        try {
            auto copy = ctx;
            almondnamespace::sdlcontext::sdl_cleanup(copy);
        }
        catch (const std::exception& e) {
            almondnamespace::logger::get(kLogSdl).logf(
                almondnamespace::logger::LogLevel::ALMOND_ERROR,
                std::source_location::current(),
                "cleanup exception: {}",
                e.what());
        }
        catch (...) {
            almondnamespace::logger::get(kLogSdl).log(
                almondnamespace::logger::LogLevel::ALMOND_ERROR,
                "cleanup unknown exception",
                std::source_location::current());
        }
    }

    bool sdl_process_adapter(std::shared_ptr<almondnamespace::core::Context> ctx,
        almondnamespace::core::CommandQueue& queue)
    {
        if (!ctx) return false;
        return almondnamespace::sdlcontext::sdl_process(ctx, queue);
    }
#endif


#if defined(ALMOND_USING_RAYLIB)
    bool raylib_process_adapter(std::shared_ptr<almondnamespace::core::Context> ctx,
        almondnamespace::core::CommandQueue& queue)
    {
        if (!ctx) return false;

        almondnamespace::raylibcontext::raylib_process();

        almondnamespace::atlasmanager::process_pending_uploads(almondnamespace::core::ContextType::RayLib);

        const bool ran_commands = queue.drain();
        if (!ran_commands)
        {
            almondnamespace::raylibcontext::raylib_idle_frame();
        }

        return almondnamespace::raylibstate::s_raylibstate.running;
    }
#endif
} // namespace

namespace almondnamespace::core
{
    std::map<ContextType, BackendState> g_backends{};
    std::shared_mutex g_backendsMutex{};

    void AddContextForBackend(ContextType type, std::shared_ptr<Context> context)
    {
        if (!context) return;

        std::unique_lock lock(g_backendsMutex);
        auto& backendState = g_backends[type];

        if (!backendState.master) backendState.master = std::move(context);
        else backendState.duplicates.emplace_back(std::move(context));
    }

    bool core::Context::process_safe(std::shared_ptr<core::Context> ctx, CommandQueue& queue)
    {
        if (!process) return false;
        try { return process(std::move(ctx), queue); }
        catch (const std::exception& e) { std::cerr << "[Context] Exception in process: " << e.what() << "\n"; return false; }
        catch (...) { std::cerr << "[Context] Unknown exception in process\n"; return false; }
    }

    namespace
    {
        inline void copy_atomic_function(
            AlmondAtomicFunction<std::uint32_t(TextureAtlas&, std::string, const ImageData&)>& dst,
            const AlmondAtomicFunction<std::uint32_t(TextureAtlas&, std::string, const ImageData&)>& src)
        {
            dst.ptr.store(src.ptr.load(std::memory_order_acquire), std::memory_order_release);
        }

        inline void copy_atomic_function(
            AlmondAtomicFunction<std::uint32_t(const TextureAtlas&)>& dst,
            const AlmondAtomicFunction<std::uint32_t(const TextureAtlas&)>& src)
        {
            dst.ptr.store(src.ptr.load(std::memory_order_acquire), std::memory_order_release);
        }

        std::uint32_t add_texture_default(TextureAtlas& a, std::string n, const ImageData& i)
        {
            return default_add_texture(a, std::move(n), i);
        }

        std::uint32_t add_atlas_default(const TextureAtlas& a, ContextType t)
        {
            return default_add_atlas(a, t);
        }
    }

    std::shared_ptr<core::Context> CloneContext(const core::Context& prototype)
    {
        auto clone = std::make_shared<core::Context>();

        clone->initialize = prototype.initialize;
        clone->cleanup = prototype.cleanup;
        clone->process = prototype.process;
        clone->clear = prototype.clear;
        clone->present = prototype.present;
        clone->get_width = prototype.get_width;
        clone->get_height = prototype.get_height;
        clone->registry_get = prototype.registry_get;
        clone->draw_sprite = prototype.draw_sprite;
        clone->add_model = prototype.add_model;

        clone->is_key_held = prototype.is_key_held;
        clone->is_key_down = prototype.is_key_down;
        clone->get_mouse_position = prototype.get_mouse_position;
        clone->is_mouse_button_held = prototype.is_mouse_button_held;
        clone->is_mouse_button_down = prototype.is_mouse_button_down;

        copy_atomic_function(clone->add_texture, prototype.add_texture);
        copy_atomic_function(clone->add_atlas, prototype.add_atlas);

        clone->onResize = prototype.onResize;
        clone->width = prototype.width;
        clone->height = prototype.height;
        clone->type = prototype.type;
        clone->backendName = prototype.backendName;

        clone->hwnd = nullptr;
        clone->hdc = nullptr;
        clone->hglrc = nullptr;
        clone->windowData = nullptr;

        return clone;
    }

    void InitializeAllContexts()
    {
        static bool s_initialized = false;
        if (s_initialized) return;
        s_initialized = true;

#if defined(ALMOND_USING_OPENGL)
        {
            auto ctx = std::make_shared<Context>();
            ctx->type = ContextType::OpenGL;
            ctx->backendName = "OpenGL";

            ctx->initialize = opengl_initialize_adapter;
            ctx->cleanup = opengl_cleanup_adapter;
            ctx->process = opengl_process_adapter;
            ctx->clear = almondnamespace::openglcontext::opengl_clear;
            // OpenGL swaps in opengl_process; keep present unset to avoid double-swap.
            ctx->present = nullptr;
            ctx->get_width = almondnamespace::openglcontext::opengl_get_width;
            ctx->get_height = almondnamespace::openglcontext::opengl_get_height;

            ctx->is_key_held = [](input::Key k) { return input::is_key_held(k); };
            ctx->is_key_down = [](input::Key k) { return input::is_key_down(k); };
            ctx->get_mouse_position = [](int& x, int& y) { x = input::mouseX.load(std::memory_order_relaxed); y = input::mouseY.load(std::memory_order_relaxed); };
            ctx->is_mouse_button_held = [](input::MouseButton b) { return input::is_mouse_button_held(b); };
            ctx->is_mouse_button_down = [](input::MouseButton b) { return input::is_mouse_button_down(b); };

            ctx->draw_sprite = almondnamespace::opengltextures::draw_sprite;
            ctx->add_texture = &add_texture_default;
            ctx->add_atlas = +[](const TextureAtlas& a) { return add_atlas_default(a, ContextType::OpenGL); };

            AddContextForBackend(ContextType::OpenGL, std::move(ctx));
        }
#endif
#if defined(ALMOND_USING_SFML)
        {
            auto ctx = std::make_shared<Context>();
            ctx->type = ContextType::SFML;
            ctx->backendName = "SFML";

            ctx->initialize = sfml_initialize_adapter;
            ctx->cleanup = sfml_cleanup_adapter;
            ctx->process = sfml_process_adapter;
            //ctx->clear = almondnamespace::sfmlcontext::sfml_clear;
            //ctx->present = almondnamespace::sfmlcontext::sfml_present;
            //ctx->get_width = almondnamespace::sfmlcontext::sfml_get_width;
            //ctx->get_height = almondnamespace::sfmlcontext::sfml_get_height;

            ctx->is_key_held = [](input::Key k) { return input::is_key_held(k); };
            ctx->is_key_down = [](input::Key k) { return input::is_key_down(k); };
            ctx->get_mouse_position = [](int& x, int& y) { x = input::mouseX.load(std::memory_order_relaxed); y = input::mouseY.load(std::memory_order_relaxed); };
            ctx->is_mouse_button_held = [](input::MouseButton b) { return input::is_mouse_button_held(b); };
            ctx->is_mouse_button_down = [](input::MouseButton b) { return input::is_mouse_button_down(b); };

            ctx->draw_sprite = almondnamespace::sfmlcontext::draw_sprite;
            ctx->add_texture = &add_texture_default;
            ctx->add_atlas = +[](const TextureAtlas& a) { return add_atlas_default(a, ContextType::SFML); };

            AddContextForBackend(ContextType::SFML, std::move(ctx));
        }
#endif




#if defined(ALMOND_USING_RAYLIB)
        {
            auto ctx = std::make_shared<Context>();
            ctx->type = ContextType::RayLib;
            ctx->backendName = "RayLib";

            ctx->initialize = []() {
                auto current = almondnamespace::core::MultiContextManager::GetCurrent();
                if (!current) return;

                void* parent = ctx_native_window_handle(current);

                try {
                    (void)almondnamespace::raylibcontext::raylib_initialize(
                        current,
                        parent,
                        static_cast<unsigned>((std::max)(1, current->width)),
                        static_cast<unsigned>((std::max)(1, current->height)),
                        current->onResize,
                        current->backendName
                    );
                }
                catch (const std::exception& e) {
                    almondnamespace::logger::get(kLogRaylib).logf(
                        almondnamespace::logger::LogLevel::ALMOND_ERROR,
                        std::source_location::current(),
                        "init exception: {}",
                        e.what());
                }
                catch (...) {
                    almondnamespace::logger::get(kLogRaylib).log(
                        almondnamespace::logger::LogLevel::ALMOND_ERROR,
                        "init unknown exception",
                        std::source_location::current());
                }
                };

            ctx->cleanup = []() {
                auto current = almondnamespace::core::MultiContextManager::GetCurrent();
                if (!current) return;

                try { almondnamespace::raylibcontext::raylib_cleanup(current); }
                catch (const std::exception& e) {
                    almondnamespace::logger::get(kLogRaylib).logf(
                        almondnamespace::logger::LogLevel::ALMOND_ERROR,
                        std::source_location::current(),
                        "cleanup exception: {}",
                        e.what());
                }
                catch (...) {
                    almondnamespace::logger::get(kLogRaylib).log(
                        almondnamespace::logger::LogLevel::ALMOND_ERROR,
                        "cleanup unknown exception",
                        std::source_location::current());
                }
                };

            ctx->process = raylib_process_adapter;
            ctx->clear = []() { almondnamespace::raylibcontext::raylib_clear(0.0f, 0.0f, 0.0f, 1.0f); };
            ctx->present = almondnamespace::raylibcontext::raylib_present;
            ctx->get_width = almondnamespace::raylibcontext::raylib_get_width;
            ctx->get_height = almondnamespace::raylibcontext::raylib_get_height;

            ctx->is_key_held = [](input::Key k) { return input::is_key_held(k); };
            ctx->is_key_down = [](input::Key k) { return input::is_key_down(k); };
            ctx->get_mouse_position = [](int& x, int& y) { x = input::mouseX.load(std::memory_order_relaxed); y = input::mouseY.load(std::memory_order_relaxed); };
            ctx->is_mouse_button_held = [](input::MouseButton b) { return input::is_mouse_button_held(b); };
            ctx->is_mouse_button_down = [](input::MouseButton b) { return input::is_mouse_button_down(b); };

            ctx->draw_sprite = almondnamespace::raylibrenderer::draw_sprite;
            ctx->add_texture = &add_texture_default;
            ctx->add_atlas = +[](const TextureAtlas& a) { return add_atlas_default(a, ContextType::RayLib); };

            AddContextForBackend(ContextType::RayLib, std::move(ctx));
        }
#endif

#if defined(ALMOND_USING_VULKAN)
        {
            auto ctx = std::make_shared<Context>();
            ctx->type = ContextType::Vulkan;
            ctx->backendName = "Vulkan";

            ctx->initialize = vulkan_initialize_adapter;
            ctx->cleanup = vulkan_cleanup_adapter;
            ctx->process = vulkan_process_adapter;
            ctx->present = almondnamespace::vulkancontext::vulkan_present;
            ctx->get_width = almondnamespace::vulkancontext::vulkan_get_width;
            ctx->get_height = almondnamespace::vulkancontext::vulkan_get_height;

            ctx->is_key_held = [](input::Key k) { return input::is_key_held(k); };
            ctx->is_key_down = [](input::Key k) { return input::is_key_down(k); };
            ctx->get_mouse_position = [](int& x, int& y) { x = input::mouseX.load(std::memory_order_relaxed); y = input::mouseY.load(std::memory_order_relaxed); };
            ctx->is_mouse_button_held = [](input::MouseButton b) { return input::is_mouse_button_held(b); };
            ctx->is_mouse_button_down = [](input::MouseButton b) { return input::is_mouse_button_down(b); };

            ctx->draw_sprite = almondnamespace::vulkancontext::vulkan_draw_sprite;
            ctx->add_texture = &add_texture_default;
            ctx->add_atlas = +[](const TextureAtlas& a) { return add_atlas_default(a, ContextType::Vulkan); };

            AddContextForBackend(ContextType::Vulkan, std::move(ctx));
        }
#endif

#if defined(ALMOND_USING_SDL)
        {
            auto ctx = std::make_shared<Context>();
            ctx->type = ContextType::SDL;
            ctx->backendName = "SDL";

            ctx->initialize = sdl_initialize_adapter;
            ctx->cleanup = sdl_cleanup_adapter;
            ctx->process = sdl_process_adapter;
            //ctx->clear = almondnamespace::sdlcontext::sdl_clear;
            //ctx->present = almondnamespace::sdlcontext::sdl_present;
            //ctx->get_width = almondnamespace::sdlcontext::sdl_get_width;
            //ctx->get_height = almondnamespace::sdlcontext::sdl_get_height;

            ctx->is_key_held = [](input::Key k) { return input::is_key_held(k); };
            ctx->is_key_down = [](input::Key k) { return input::is_key_down(k); };
            ctx->get_mouse_position = [](int& x, int& y) { x = input::mouseX.load(std::memory_order_relaxed); y = input::mouseY.load(std::memory_order_relaxed); };
            ctx->is_mouse_button_held = [](input::MouseButton b) { return input::is_mouse_button_held(b); };
            ctx->is_mouse_button_down = [](input::MouseButton b) { return input::is_mouse_button_down(b); };

            ctx->draw_sprite = sdltextures::draw_sprite;
            ctx->add_texture = &add_texture_default;
            ctx->add_atlas = +[](const TextureAtlas& a) { return add_atlas_default(a, ContextType::SDL); };

            AddContextForBackend(ContextType::SDL, std::move(ctx));
        }
#endif

#if defined(ALMOND_USING_SOFTWARE_RENDERER)
        {
            auto ctx = std::make_shared<Context>();
            ctx->type = ContextType::Software;
            ctx->backendName = "Software";

            ctx->initialize = softrenderer_initialize_adapter;
            ctx->cleanup = softrenderer_cleanup_adapter;
            ctx->process = softrenderer_process_adapter;

            ctx->is_key_held = [](input::Key k) { return input::is_key_held(k); };
            ctx->is_key_down = [](input::Key k) { return input::is_key_down(k); };
            ctx->get_mouse_position = [](int& x, int& y) { x = input::mouseX.load(std::memory_order_relaxed); y = input::mouseY.load(std::memory_order_relaxed); };
            ctx->is_mouse_button_held = [](input::MouseButton b) { return input::is_mouse_button_held(b); };
            ctx->is_mouse_button_down = [](input::MouseButton b) { return input::is_mouse_button_down(b); };

            ctx->draw_sprite = almondnamespace::anativecontext::draw_sprite;
            ctx->add_texture = &add_texture_default;
            ctx->add_atlas = +[](const TextureAtlas& a) { return add_atlas_default(a, ContextType::Software); };

            AddContextForBackend(ContextType::Software, std::move(ctx));
        }
#endif

#if defined(ALMOND_USING_NOOP_HEADLESS)
        {
            auto ctx = std::make_shared<Context>();
            ctx->type = ContextType::Noop;
            ctx->backendName = "Noop";

            ctx->initialize = almondnamespace::noopcontext::noop_initialize;
            ctx->cleanup = almondnamespace::noopcontext::noop_cleanup;
            ctx->process = almondnamespace::noopcontext::noop_process;
            ctx->clear = almondnamespace::noopcontext::noop_clear;
            ctx->present = almondnamespace::noopcontext::noop_present;
            ctx->get_width = almondnamespace::noopcontext::noop_get_width;
            ctx->get_height = almondnamespace::noopcontext::noop_get_height;

            ctx->draw_sprite = nullptr;
            ctx->add_texture = &add_texture_default;
            ctx->add_atlas = +[](const TextureAtlas& a) { return add_atlas_default(a, ContextType::Noop); };

            AddContextForBackend(ContextType::Noop, std::move(ctx));
        }
#endif
    }

    bool ProcessAllContexts()
    {
        bool anyRunning = false;

        std::vector<std::shared_ptr<Context>> contexts;
        {
            std::shared_lock lock(g_backendsMutex);
            contexts.reserve(g_backends.size());
            for (auto& [_, state] : g_backends) {
                if (state.master) contexts.push_back(state.master);
                for (auto& dup : state.duplicates) contexts.push_back(dup);
            }
        }

        for (auto& ctx : contexts)
        {
            if (!ctx) continue;

            if (auto* window = ctx->windowData)
            {
                if (window->running) anyRunning = true;
                else window->commandQueue.drain();
                continue;
            }

            CommandQueue localQueue;
            if (!ctx->process) { localQueue.drain(); continue; }

            const auto previous = core::get_current_render_context();
            core::set_current_render_context(ctx);
            struct ResetCurrentContext
            {
                std::shared_ptr<Context> previousCtx{};
                ~ResetCurrentContext() { core::set_current_render_context(std::move(previousCtx)); }
            } reset{ previous };

            if (ctx->process_safe(ctx, localQueue)) anyRunning = true;
        }

        return anyRunning;
    }
} // namespace almondnamespace::core
