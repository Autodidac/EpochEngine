/**************************************************************
 *   █████╗ ██╗     ███╗   ███╗   ███╗   ██╗    ██╗██████╗    *
 *  ██╔══██╗██║     ████╗ ████║ ██╔═══██╗████╗  ██║██╔══██╗   *
 *  ███████║██║     ██╔████╔██║ ██║   ██║██╔██╗ ██║██║  ██║   *
 *  ██╔══██║██║     ██║╚██╔╝██║ ██║   ██║██║╚██╗██║██║  ██║   *
 *  ██║  ██║███████╗██║ ╚═╝ ██║ ╚██████╔╝██║ ╚████║██████╔╝   *
 *  ╚═╝  ╚═╝╚══════╝╚═╝     ╚═╝  ╚═════╝ ╚═╝  ╚═══╝╚═════╝    *
 *                                                            *
 *   This file is part of the Almond Project.                 *
 *   AlmondShell - Modular C++ Framework                      *
 *                                                            *
 *   SPDX-License-Identifier: LicenseRef-MIT-NoSell           *
 *                                                            *
 *   Provided "AS IS", without warranty of any kind.          *
 *   Use permitted for Non-Commercial Purposes ONLY,          *
 *   without prior commercial licensing agreement.            *
 *                                                            *
 *   Redistribution Allowed with This Notice and              *
 *   LICENSE file. No obligation to disclose modifications.   *
 *                                                            *
 *   See LICENSE file for full terms.                         *
 *                                                            *
 **************************************************************/
 // aecs.ixx — C++23 module conversion of aecs.hpp
module;
export module aecs;

// ─────────────────────────────────────────────────────────────
// STANDARD LIBRARY
// ─────────────────────────────────────────────────────────────
import <typeinfo>;
import <string>;
import <string_view>;
import <format>;
import <utility>;
import <vector>;
import <cassert>;

// ─────────────────────────────────────────────────────────────
// ENGINE / FRAMEWORK MODULES
// These MUST already be real modules.
// No textual includes remain.
// ─────────────────────────────────────────────────────────────
import aecs.entitycomponentmanager;   // ComponentStorage + add/get/has/remove
import aengine.eventsystem;              // events::push_event
import aengine.core.logger;                   // Logger, LogLevel
import aengine.core.time;               // time::Timer, time helpers
import aecs.entityhistory;            // EntityID, history tracking
import aecs.storage;               // ComponentStorage

// ─────────────────────────────────────────────────────────────
export namespace almondnamespace::ecs
{
    // Public alias
    using Entity = EntityID;

    // ─────────────────────────────────────────────────────────
    // REGISTRY
    // Holds storage + ID counter + optional logging/time hooks
    // ─────────────────────────────────────────────────────────

    export template<typename... Cs>
        struct reg_ex
    {
        ComponentStorage storage{};
        EntityID         nextID{ 1 };
        logger::Logger* log{ nullptr };
        timing::Timer* clk{ nullptr };
    };

    // ─────────────────────────────────────────────────────────
    // REGISTRY FACTORY
    // ─────────────────────────────────────────────────────────

    export template<typename... Cs>
        [[nodiscard]] inline reg_ex<Cs...> make_registry(
            logger::Logger* L = nullptr,
            timing::Timer* C = nullptr)
    {
        return reg_ex<Cs...>{ {}, 1, L, C };
    }

    // ─────────────────────────────────────────────────────────
    // INTERNAL NOTIFICATION
    // ─────────────────────────────────────────────────────────
    namespace detail
    {
        template<typename... Cs>
        inline void notify(
            reg_ex<Cs...>& R,
            std::string_view action,
            Entity e,
            std::string_view comp = {})
        {
            if (!R.log || !R.clk)
                return;

            const auto ts = timing::getCurrentTimeString();

            R.log->log(
                std::format(
                    "[ECS] {}{} entity={} at {}",
                    action,
                    comp.empty() ? "" : std::format(":{}", comp),
                    e,
                    ts));

            events::push_event(events::Event{
                events::EventType::Custom,
                {
                    {"ecs_action", std::string(action)},
                    {"entity",     std::to_string(e)},
                    {"component",  std::string(comp)},
                    {"time",       ts}
                },
                0.f,
                0.f
                });
        }
    } // namespace detail

    // ─────────────────────────────────────────────────────────
    // ENTITY LIFECYCLE
    // ─────────────────────────────────────────────────────────

    export template<typename... Cs>
        inline Entity create_entity(reg_ex<Cs...>& R)
    {
        const Entity e = R.nextID++;
        detail::notify(R, "createEntity", e);
        return e;
    }

    export template<typename... Cs>
        inline void destroy_entity(reg_ex<Cs...>& R, Entity e)
    {
        // Remove all registered component types
        (remove_component<Cs>(R, e), ...);
        detail::notify(R, "destroyEntity", e);
    }

    // ─────────────────────────────────────────────────────────
    // COMPONENT API
    // ─────────────────────────────────────────────────────────

    export template<typename C, typename... Cs>
        inline void add_component(reg_ex<Cs...>& R, Entity e, C c)
    {
        ::almondnamespace::ecs::add_component<C>(
            R.storage, e, std::move(c));

        detail::notify(R, "addComponent", e, typeid(C).name());
    }

    export template<typename C, typename... Cs>
        inline void remove_component(reg_ex<Cs...>& R, Entity e)
    {
        ::almondnamespace::ecs::remove_component<C>(
            R.storage, e);

        detail::notify(R, "removeComponent", e, typeid(C).name());
    }

    export template<typename C, typename... Cs>
        [[nodiscard]] inline bool has_component(
            const reg_ex<Cs...>& R,
            Entity e)
    {
        return ::almondnamespace::ecs::has_component<C>(
            R.storage, e);
    }

    export template<typename C, typename... Cs>
        [[nodiscard]] inline C& get_component(
            reg_ex<Cs...>& R,
            Entity e)
    {
        return ::almondnamespace::ecs::get_component<C>(
            R.storage, e);
    }

    // ─────────────────────────────────────────────────────────
    // VIEW / ITERATION
    // ─────────────────────────────────────────────────────────

    export template<typename... Vs, typename... Cs, typename Fn>
        inline void view(reg_ex<Cs...>& R, Fn&& fn)
    {
        for (auto& [ent, compMap] : R.storage)
        {
            if ((has_component<Vs>(R, ent) && ...))
            {
                fn(ent, get_component<Vs>(R, ent)...);
            }
        }
    }

} // namespace almondnamespace::ecs
