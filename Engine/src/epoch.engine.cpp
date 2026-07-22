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

#include "../include/_epoch.stl_types.hpp"
#include <source_location>

module epoch.engine;

import core.logger;
import core.format;
import core.error;
import platform.window;
import platform.context;
import platform.runtime;

namespace epochengine
{
    namespace
    {
        inline void engine_info(const std::string_view message)
        {
            epochengine::logger::get("Epoch.Engine").log(
                epochengine::logger::LogLevel::INFO,
                message,
                std::source_location::current());
        }

        inline void engine_error(const std::string_view message)
        {
            epochengine::logger::get("Epoch.Engine").log(
                epochengine::logger::LogLevel::Error,
                message,
                std::source_location::current());
        }
    }

    Engine& Engine::instance() noexcept
    {
        static Engine g{};
        return g;
    }

    core::error::result<void> Engine::init(const EngineConfig& cfg) noexcept
    {
        if (_initialized)
            return {}; // ok

        _cfg = cfg;
        _budgets = cfg.budgets;

        // Create window system
        auto wsys = platform::create_window_system();
        if (!wsys)
        {
            const auto& err = wsys.error();
            engine_error(epochengine::core::format::str("create_window_system failed: {}", err.message).impl);
            return epochengine::unexpected(err);
        }
        _windows = std::move(*wsys);

        // Create primary window
        auto wh = _windows->create_window(cfg.window);
        if (!wh)
        {
            const auto& err = wh.error();
            engine_error(epochengine::core::format::str("create_window failed: {}", err.message).impl);
            return epochengine::unexpected(err);
        }
        _primary = *wh;

        // Create graphics context (can be null_backend)
        auto gctx = platform::create_graphics_context(cfg.gfx);
        if (!gctx)
        {
            const auto& err = gctx.error();
            engine_error(epochengine::core::format::str("create_graphics_context failed: {}", err.message).impl);
            return epochengine::unexpected(err);
        }
        _gfx = std::move(*gctx);

        // Surface hookup
        if (_primary.valid())
        {
            auto r = _gfx->create_surface(_primary);
            if (!r)
            {
                engine_error(epochengine::core::format::str("create_surface failed: {}", r.error().message).impl);
                return epochengine::unexpected(r.error());
            }
        }

        _runtime_profile = platform::build_runtime_frame_profile(_gfx.get());
        _caps = _runtime_profile.capabilities;
        platform::log_runtime_profile("Epoch.Engine", "Epoch.Perf", _runtime_profile);

        _initialized = true;
        engine_info("Engine::init ok");
        return {}; // ok
    }

    void Engine::pump_platform() noexcept
    {
        if (!_initialized || !_windows)
            return;

        _windows->pump_events([this](const platform::WindowEvent& e)
            {
                _events.emit(e);

                if (e.type == platform::WindowEventType::resized && _gfx)
                    _gfx->resize_surface(e.handle, e.width, e.height);

                if (e.type == platform::WindowEventType::close && _windows)
                    _windows->request_close(e.handle);
            });
    }

    void Engine::update(double dt_seconds) noexcept
    {
        systems().update(dt_seconds);
    }

    void Engine::shutdown() noexcept
    {
        if (!_initialized)
            return;

        systems().shutdown();

        if (_gfx)
        {
            _gfx->teardown();
            _gfx.reset();
        }

        if (_windows)
        {
            if (_primary.valid())
                _windows->destroy_window(_primary);
            _windows.reset();
        }

        _initialized = false;
        _runtime_profile = {};
        engine_info("Engine::shutdown");
    }
} // namespace epoch
