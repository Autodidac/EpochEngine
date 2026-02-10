/**************************************************************
 *   Epoch Engine - Engine Context (capabilities/budgets/events/ecs) (2026)
 *   License: MIT (adapt as needed)
 **************************************************************/
module;

#include "../include/_epoch.stl_types.hpp"

export module epoch.engine;

import core.error;
import epoch.platform.capabilities;
import epoch.platform.budgets;
import epoch.platform.window;
import epoch.platform.context;
import epoch.systems;
import epoch.events;
import epoch.ecs;

export namespace epoch
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

        [[nodiscard]] Capabilities& capabilities() noexcept { return _caps; }
        [[nodiscard]] const Capabilities& capabilities() const noexcept { return _caps; }

        [[nodiscard]] Budgets& budgets() noexcept { return _budgets; }
        [[nodiscard]] const Budgets& budgets() const noexcept { return _budgets; }

        [[nodiscard]] events::bus& events() noexcept { return _events; }
        [[nodiscard]] ecs::world& world() noexcept { return _world; }

        [[nodiscard]] systems::Registry& systems() noexcept { return systems::Registry::instance(); }

    private:
        Engine() = default;

        EngineConfig _cfg{};
        Budgets _budgets{};
        Capabilities _caps{};

        std::unique_ptr<platform::IWindowSystem> _windows{};
        std::unique_ptr<platform::IGraphicsContext> _gfx{};
        platform::WindowHandle _primary{};

        events::bus _events{};
        ecs::world _world{ ecs::world_desc{ .max_entities = 64 * 1024, .allow_growth = false } };

        bool _initialized = false;
    };
} // namespace epoch
