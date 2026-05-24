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
module;

// aengine.context.cpp  (module implementation unit for core.context)

#include <include/engine.config.hpp> // macros only - must stay in the global module fragment

#include <algorithm>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <source_location>
#include <string>
#include <utility>
#include <vector>

module core.context;

import context.type;
import context.commandqueue;
import core.logger;
import atlas.texture;
import image.loader;
import utility.atomicfunction;
#if defined(EPOCH_USING_NOOP_HEADLESS)
import noop.context;
#endif

namespace epochnamespace::core::detail
{
#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)
    void register_opengl_backend();
#endif
#if defined(EPOCH_USING_SFML) && (EPOCH_USING_SFML == 1)
    void register_sfml_backend();
#endif
#if defined(EPOCH_USING_RAYLIB) && (EPOCH_USING_RAYLIB == 1)
    extern "C" void epoch_register_raylib_backend();
#endif
#if defined(EPOCH_USING_SDL) && (EPOCH_USING_SDL == 1)
    void register_sdl_backend();
#endif
#if defined(EPOCH_USING_VULKAN) && (EPOCH_USING_VULKAN == 1) && !defined(__linux__)
    void register_vulkan_backend();
#endif
#if defined(EPOCH_USING_DIRECTX) && (EPOCH_USING_DIRECTX == 1)
    void register_directx_backend();
#endif
#if defined(EPOCH_USING_SOFTWARE_RENDERER) && (EPOCH_USING_SOFTWARE_RENDERER == 1)
    void register_software_backend();
#endif
}

namespace
{
    std::uint32_t default_add_texture(
        epochnamespace::TextureAtlas&,
        std::string,
        const epochnamespace::ImageData&) noexcept
    {
        return 0u;
    }

    std::uint32_t add_atlas_default(
        const epochnamespace::TextureAtlas& atlas,
        const epochnamespace::core::ContextType type) noexcept
    {
        try
        {
            epochnamespace::atlasmanager::ensure_uploaded(atlas);
            epochnamespace::atlasmanager::process_pending_uploads(type);
        }
        catch (...)
        {
        }

        const int idx = atlas.get_index();
        return static_cast<std::uint32_t>(idx >= 0 ? idx + 1 : 1);
    }

}

namespace epochnamespace::core
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
        catch (const std::exception& e)
        {
            const auto message = std::string("Exception in process: ") + e.what();
            logger::get("Context").log(
                logger::LogLevel::Error,
                message,
                std::source_location::current());
            return false;
        }
        catch (...)
        {
            logger::get("Context").log(
                logger::LogLevel::Error,
                "Unknown exception in process",
                std::source_location::current());
            return false;
        }
    }

    namespace
    {
        inline void copy_atomic_function(
            EpochAtomicFunction<std::uint32_t(TextureAtlas&, std::string, const ImageData&)>& dst,
            const EpochAtomicFunction<std::uint32_t(TextureAtlas&, std::string, const ImageData&)>& src)
        {
            dst.ptr.store(src.ptr.load(std::memory_order_acquire), std::memory_order_release);
        }

        inline void copy_atomic_function(
            EpochAtomicFunction<std::uint32_t(const TextureAtlas&)>& dst,
            const EpochAtomicFunction<std::uint32_t(const TextureAtlas&)>& src)
        {
            dst.ptr.store(src.ptr.load(std::memory_order_acquire), std::memory_order_release);
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

#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)
        detail::register_opengl_backend();
#endif

#if defined(EPOCH_USING_SFML) && (EPOCH_USING_SFML == 1)
        detail::register_sfml_backend();
#endif

#if defined(EPOCH_USING_RAYLIB) && (EPOCH_USING_RAYLIB == 1)
        detail::epoch_register_raylib_backend();
#endif

#if defined(EPOCH_USING_VULKAN) && (EPOCH_USING_VULKAN == 1) && !defined(__linux__)
        detail::register_vulkan_backend();
#endif

#if defined(EPOCH_USING_DIRECTX) && (EPOCH_USING_DIRECTX == 1)
        detail::register_directx_backend();
#endif

#if defined(EPOCH_USING_SDL) && (EPOCH_USING_SDL == 1)
        detail::register_sdl_backend();
#endif

#if defined(EPOCH_USING_SOFTWARE_RENDERER) && (EPOCH_USING_SOFTWARE_RENDERER == 1)
        detail::register_software_backend();
#endif

#if defined(EPOCH_USING_NOOP_HEADLESS)
        {
            auto ctx = std::make_shared<Context>();
            ctx->type = ContextType::Noop;
            ctx->backendName = "Noop";

            ctx->initialize = epochnamespace::noopcontext::noop_initialize;
            ctx->cleanup = epochnamespace::noopcontext::noop_cleanup;
            ctx->process = epochnamespace::noopcontext::noop_process;
            ctx->clear = epochnamespace::noopcontext::noop_clear;
            ctx->present = epochnamespace::noopcontext::noop_present;
            ctx->get_width = epochnamespace::noopcontext::noop_get_width;
            ctx->get_height = epochnamespace::noopcontext::noop_get_height;

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
            for (auto& [_, backendSlot] : g_backends) {
                if (backendSlot.master) contexts.push_back(backendSlot.master);
                for (auto& dup : backendSlot.duplicates) contexts.push_back(dup);
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
} // namespace epochnamespace::core
