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

#include "../include/core.stl_types.hpp"

export module epoch.engine;

import core.error;
import perf.tier;
import platform.capabilities;
import platform.budgets;
import platform.window;
import platform.context;
import platform.runtime;
import systems.registry;
import events.bus;
import ecs.world;

export namespace epochengine
{
    struct EngineConfig
    {
        platform::WindowDesc window{};
        platform::ContextDesc gfx{};
        Budgets budgets{};
    };

    class Engine
    {
    public:
        [[nodiscard]] static Engine& instance() noexcept;

        [[nodiscard]] core::error::result<void> init(const EngineConfig& cfg) noexcept;
        void pump_platform() noexcept;
        void update(double dt_seconds) noexcept;
        void shutdown() noexcept;

        [[nodiscard]] platform::IWindowSystem* windows() noexcept { return _windows.get(); }
        [[nodiscard]] platform::IGraphicsContext* graphics() noexcept { return _gfx.get(); }
        [[nodiscard]] platform::WindowHandle primary_window() const noexcept { return _primary; }
        [[nodiscard]] const platform::RuntimeFrameProfile& runtime_profile() const noexcept { return _runtime_profile; }

        [[nodiscard]] Capabilities& capabilities() noexcept { return _caps; }
        [[nodiscard]] const Capabilities& capabilities() const noexcept { return _caps; }

        [[nodiscard]] Budgets& budgets() noexcept { return _budgets; }
        [[nodiscard]] const Budgets& budgets() const noexcept { return _budgets; }
        [[nodiscard]] const FramePolicy& frame_policy() const noexcept { return _runtime_profile.frame_policy; }
        [[nodiscard]] perf::tier perf_tier() const noexcept { return _runtime_profile.perf_tier; }
        [[nodiscard]] double target_fps() const noexcept { return _runtime_profile.target_fps; }

        [[nodiscard]] events::bus& events() noexcept { return _events; }
        [[nodiscard]] ecs::world& world() noexcept { return _world; }

        [[nodiscard]] systems::Registry& systems() noexcept { return systems::Registry::instance(); }

    private:
        Engine() = default;

        EngineConfig _cfg{};
        Budgets _budgets{};
        Capabilities _caps{};
        platform::RuntimeFrameProfile _runtime_profile{};

        std::unique_ptr<platform::IWindowSystem> _windows{};
        std::unique_ptr<platform::IGraphicsContext> _gfx{};
        platform::WindowHandle _primary{};

        events::bus _events{};
        ecs::world _world{ ecs::world_desc{ .max_entities = 64 * 1024, .allow_growth = false } };

        bool _initialized = false;
    };
} // namespace epoch
