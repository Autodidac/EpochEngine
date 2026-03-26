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

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <format>
#include <iostream>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <include/aengine.config.hpp>

export module aengine.diagnostics;

import aengine.context.type;
import aengine.core.logger;
import aengine.telemetry;

// epochengine diagnostics helpers
// --------------------------------
// Lightweight utilities for reporting the current engine build
// configuration.  The helpers are header-only so they can be used
// by the updater target without touching the CMake target graph.

export namespace epochnamespace::diagnostics {
    using epochnamespace::core::ContextType;

    namespace detail
    {
        [[nodiscard]] inline std::uint64_t slow_frame_key(ContextType type, std::uintptr_t windowId) noexcept
        {
            return (static_cast<std::uint64_t>(type) << 56u) ^ static_cast<std::uint64_t>(windowId);
        }

        [[nodiscard]] inline bool should_emit_slow_frame_warning(
            ContextType type,
            std::uintptr_t windowId,
            std::chrono::steady_clock::time_point now) noexcept
        {
#if !EPOCH_ENABLE_RENDERER_SLOW_FRAME_LOGS
            (void)type;
            (void)windowId;
            (void)now;
            return false;
#else
            static const auto processStart = std::chrono::steady_clock::now();
            if (now - processStart < std::chrono::milliseconds(EPOCH_SLOW_FRAME_LOG_STARTUP_GRACE_MS))
                return false;

            static std::mutex s_mutex;
            static std::unordered_map<std::uint64_t, std::chrono::steady_clock::time_point> s_lastWarnAt;

            const auto key = slow_frame_key(type, windowId);
            const auto minInterval = (type == ContextType::Software)
                ? std::chrono::milliseconds(30000)
                : std::chrono::milliseconds(EPOCH_SLOW_FRAME_LOG_THROTTLE_MS);

            std::scoped_lock lock(s_mutex);
            auto& lastWarnAt = s_lastWarnAt[key];
            if (lastWarnAt.time_since_epoch().count() != 0
                && (now - lastWarnAt) < minInterval)
            {
                return false;
            }

            lastWarnAt = now;
            return true;
#endif
        }
    }

    struct FrameTiming
    {
        using Clock = std::chrono::steady_clock;

        ContextType backendType{ ContextType::None };
        std::uintptr_t windowId{};
        std::string_view backendName{};
        double slowFrameMs{ 33.0 };

        Clock::time_point start{ Clock::now() };
        bool finished{ false };
        double lastMs{ 0.0 };

        FrameTiming(ContextType type,
            std::uintptr_t window,
            std::string_view name,
            double slowMs = 33.0)
            : backendType(type)
            , windowId(window)
            , backendName(name)
            , slowFrameMs(slowMs)
        {
        }

        double finish()
        {
            if (finished)
                return lastMs;

            const auto end = Clock::now();
            lastMs = std::chrono::duration<double, std::milli>(end - start).count();
            finished = true;

            epochnamespace::telemetry::emit_histogram_ms(
                "renderer.frame.time_ms",
                lastMs,
                epochnamespace::telemetry::RendererTelemetryTags{ backendType, windowId });

            const double effectiveSlowFrameMs = (backendType == ContextType::Software)
                ? (std::max)(slowFrameMs, 100.0)
                : slowFrameMs;

            if (lastMs > effectiveSlowFrameMs
                && detail::should_emit_slow_frame_warning(backendType, windowId, end))
            {
                const std::string_view backend = backendName.empty() ? "Unknown" : backendName;
                epochnamespace::logger::warn(
                    "Renderer",
                    std::format("[{}] Slow frame {:.2f} ms (> {:.2f} ms)", backend, lastMs, effectiveSlowFrameMs));
            }

            return lastMs;
        }

        ~FrameTiming()
        {
            if (!finished)
                finish();
        }
    };

    struct EngineConfigurationSnapshot
    {
        bool single_parent_topology;
        bool using_sdl;
        bool using_sfml;
        bool using_raylib;
        bool using_software_renderer;
        bool using_opengl;
        bool using_vulkan;
        bool using_directx;
    };

    [[nodiscard]] inline EngineConfigurationSnapshot capture_engine_configuration()
    {
        EngineConfigurationSnapshot snapshot{};

#if defined(EPOCH_SINGLE_PARENT) && (EPOCH_SINGLE_PARENT != 0)
        snapshot.single_parent_topology = true;
#else
        snapshot.single_parent_topology = false;
#endif

#if defined(EPOCH_USING_SDL) && (EPOCH_USING_SDL == 1)
        snapshot.using_sdl = true;
#else
        snapshot.using_sdl = false;
#endif

#if defined(EPOCH_USING_SFML)
        snapshot.using_sfml = true;
#else
        snapshot.using_sfml = false;
#endif

#if defined(EPOCH_USING_RAYLIB) && (EPOCH_USING_RAYLIB == 1)
        snapshot.using_raylib = true;
#else
        snapshot.using_raylib = false;
#endif

#if defined(EPOCH_USING_SOFTWARE_RENDERER)
        snapshot.using_software_renderer = true;
#else
        snapshot.using_software_renderer = false;
#endif

#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)
        snapshot.using_opengl = true;
#else
        snapshot.using_opengl = false;
#endif

#if defined(EPOCH_USING_VULKAN)
        snapshot.using_vulkan = true;
#else
        snapshot.using_vulkan = false;
#endif

#if defined(EPOCH_USING_DIRECTX)
        snapshot.using_directx = true;
#else
        snapshot.using_directx = false;
#endif

        return snapshot;
    }

    struct ContextDimensionSnapshot
    {
        const void* contextId{};
        ContextType type{ ContextType::None };
        std::string_view backendName{};
        int logicalWidth{};
        int logicalHeight{};
        int framebufferWidth{};
        int framebufferHeight{};
        int virtualWidth{};
        int virtualHeight{};
    };

    [[nodiscard]] inline std::string_view to_string(ContextType type) noexcept
    {
        switch (type)
        {
        case ContextType::OpenGL:   return "OpenGL";
        case ContextType::SDL:      return "SDL";
        case ContextType::SFML:     return "SFML";
        case ContextType::RayLib:   return "Raylib";
        case ContextType::Vulkan:   return "Vulkan";
        case ContextType::DirectX:  return "DirectX";
        case ContextType::Software: return "Software";
        case ContextType::Custom:   return "Custom";
        case ContextType::Noop:     return "Noop";
        case ContextType::None:
        default:
            return "None";
        }
    }

    inline void log_context_dimensions_if_changed(const ContextDimensionSnapshot& snapshot,
        std::ostream& output = std::cout)
    {
        struct CachedDimensions
        {
            bool initialised{ false };
            ContextType type{ ContextType::None };
            std::string backendName{};
            int logicalWidth{};
            int logicalHeight{};
            int framebufferWidth{};
            int framebufferHeight{};
            int virtualWidth{};
            int virtualHeight{};
        };

        static std::mutex s_mutex;
        static std::unordered_map<const void*, CachedDimensions> s_cache;

        if (!snapshot.contextId)
        {
            return;
        }

        std::scoped_lock lock(s_mutex);
        auto& record = s_cache[snapshot.contextId];

        const bool changed = !record.initialised
            || record.type != snapshot.type
            || record.backendName != snapshot.backendName
            || record.logicalWidth != snapshot.logicalWidth
            || record.logicalHeight != snapshot.logicalHeight
            || record.framebufferWidth != snapshot.framebufferWidth
            || record.framebufferHeight != snapshot.framebufferHeight
            || record.virtualWidth != snapshot.virtualWidth
            || record.virtualHeight != snapshot.virtualHeight;

        if (!changed)
        {
            return;
        }

        record.initialised = true;
        record.type = snapshot.type;
        record.backendName = std::string(snapshot.backendName);
        record.logicalWidth = snapshot.logicalWidth;
        record.logicalHeight = snapshot.logicalHeight;
        record.framebufferWidth = snapshot.framebufferWidth;
        record.framebufferHeight = snapshot.framebufferHeight;
        record.virtualWidth = snapshot.virtualWidth;
        record.virtualHeight = snapshot.virtualHeight;

        output << "[Context][Dims] backend=" << (record.backendName.empty() ? "(unnamed)" : record.backendName)
            << " type=" << to_string(record.type)
            << " logical=" << record.logicalWidth << 'x' << record.logicalHeight
            << " framebuffer=" << record.framebufferWidth << 'x' << record.framebufferHeight
            << " virtual=" << record.virtualWidth << 'x' << record.virtualHeight
            << '\n';
    }

    inline void print_engine_configuration_summary(std::ostream& output = std::cout)
    {
        const EngineConfigurationSnapshot snapshot = capture_engine_configuration();

        output << "[Engine] Active context topology: "
            << (snapshot.single_parent_topology ? "Single parent window" : "Multiple top-level windows")
            << '\n';

        const std::array<std::pair<std::string_view, bool>, 7> renderers{ {
            {"SDL context", snapshot.using_sdl},
            {"SFML context", snapshot.using_sfml},
            {"Raylib context", snapshot.using_raylib},
            {"Software renderer", snapshot.using_software_renderer},
            {"OpenGL renderer", snapshot.using_opengl},
            {"Vulkan renderer", snapshot.using_vulkan},
            {"DirectX renderer", snapshot.using_directx},
        } };

        output << "[Engine] Enabled integrations:";
        bool first = true;
        for (const auto& [label, enabled] : renderers)
        {
            if (!enabled)
            {
                continue;
            }

            output << (first ? ' ' : ', ') << label;
            first = false;
        }

        if (first)
        {
            output << " none";
        }

        output << '\n';

        if (!snapshot.using_opengl && !snapshot.using_software_renderer && !snapshot.using_raylib
            && !snapshot.using_sdl)
        {
            output << "[Engine][Warning] No primary renderer or context is enabled."
                << " Update aengineconfig.hpp before launching.\n";
        }
    }
} // namespace epochnamespace::diagnostics
