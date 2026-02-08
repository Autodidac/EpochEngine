module;

#include "../include/_epoch.stl_types.hpp"

module epoch.engine;

import core.log;
import core.format;
import core.error;
import epoch.platform.window;
import epoch.platform.context;

namespace epoch
{
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
            return std::unexpected(core::error::failed("create_window_system failed"));
        _windows = std::move(*wsys);

        // Create primary window
        auto wh = _windows->create_window(cfg.window);
        if (!wh)
            return std::unexpected(core::error::failed("create_window failed"));
        _primary = *wh;

        // Create graphics context (can be null_backend)
        auto gctx = platform::create_graphics_context(cfg.gfx);
        if (!gctx)
            return std::unexpected(core::error::failed("create_graphics_context failed"));
        _gfx = std::move(*gctx);

        // Surface hookup
        if (_primary.valid())
        {
            auto r = _gfx->create_surface(_primary);
            if (!r)
                return std::unexpected(r.error());
        }

        _initialized = true;
        core::log::write(core::log::level::info, "engine", "Engine::init ok");
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
        core::log::write(core::log::level::info, "engine", "Engine::shutdown");
    }
} // namespace epoch
