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
 // ascene.ixx
module;

#include <memory>
#include <string>
#include <string_view>

export module scene;

// Centralized platform glue (no OS headers here)
import engine.platform;

// Engine modules (must already be modules)
import ecs;                  // ecs::reg_ex, create_entity, etc.
import ecs.storage;
import ecs.components;     // Position, etc.
import event.movement;        // MovementEvent

import core.timer;    // time::Timer
import core.context;              // core::Context
import context.window; // core::WindowData
import core.logger;    // Logger, LogLevel

namespace epochengine::scene
{
    using epochengine::ecs::Entity;
    using epochengine::ecs::reg_ex;
    using epochengine::logger::Logger;
    using epochengine::logger::LogLevel;
    using epochengine::timing::Timer;

    // ------------------------------------------------------------
    // SCENE
    // ------------------------------------------------------------

    export class Scene
    {
    public:
        // Explicit component list keeps ECS compile-time and honest
        using Registry = reg_ex<
            ecs::Position,
            ecs::History,
            ecs::LoggerComponent
        >;

        Scene(
            Logger* L = nullptr,
            Timer* C = nullptr,
            LogLevel sceneLevel = LogLevel::INFO)
            : reg(ecs::make_registry<
                ecs::Position,
                ecs::History,
                ecs::LoggerComponent>(nullptr, nullptr)) // ECS silent by default
            , loaded(false)
            , logger(L)
            , clock(C)
            , sceneLogLevel(sceneLevel)
        {
        }

        // Non-copyable, movable
        Scene(const Scene&) = delete;
        Scene& operator=(const Scene&) = delete;
        Scene(Scene&&) noexcept = default;
        Scene& operator=(Scene&&) noexcept = default;

        virtual ~Scene() = default;

        // --------------------------------------------------------
        // Lifecycle
        // --------------------------------------------------------

        virtual void load()
        {
            log("[Scene] Loaded", LogLevel::INFO);
            loaded = true;
        }

        virtual void unload()
        {
            log("[Scene] Unloaded", LogLevel::INFO);
            loaded = false;

            // Reset registry, keep silence
            reg = ecs::make_registry<
                ecs::Position,
                ecs::History,
                ecs::LoggerComponent>(nullptr, nullptr);
        }

        // Per-frame hook (override in derived scenes)
        virtual bool frame(
            std::shared_ptr<epochengine::core::Context>,
            epochengine::core::WindowData*)
        {
            return true; // default: no-op
        }

        // --------------------------------------------------------
        // Entity management
        // --------------------------------------------------------

        Entity createEntity()
        {
            Entity e = ecs::create_entity(reg);
            log("[Scene] Created entity " + std::to_string(e), LogLevel::INFO);
            return e;
        }

        void destroyEntity(Entity e)
        {
            ecs::destroy_entity(reg, e);
            log("[Scene] Destroyed entity " + std::to_string(e), LogLevel::INFO);
        }

        // --------------------------------------------------------
        // Events
        // --------------------------------------------------------

        void applyMovementEvent(const MovementEvent& ev)
        {
            const Entity id = ev.getEntityId();

            if (ecs::has_component<ecs::Position>(reg, id))
            {
                auto& pos = ecs::get_component<ecs::Position>(reg, id);
                pos.x += ev.getDeltaX();
                pos.y += ev.getDeltaY();

                log(
                    "[Scene] Moved entity " + std::to_string(id) +
                    " by (" +
                    std::to_string(ev.getDeltaX()) + "," +
                    std::to_string(ev.getDeltaY()) + ")",
                    LogLevel::INFO);
            }
        }

        // --------------------------------------------------------
        // Cloning
        // --------------------------------------------------------

        virtual std::unique_ptr<Scene> clone() const
        {
            auto newScene =
                std::make_unique<Scene>(logger, clock, sceneLogLevel);

            log("[Scene] Cloned scene", LogLevel::INFO);
            // ECS deep copy intentionally omitted
            return newScene;
        }

        // --------------------------------------------------------
        // Accessors
        // --------------------------------------------------------

        [[nodiscard]] bool isLoaded() const noexcept { return loaded; }

        Registry& registry() noexcept { return reg; }
        const Registry& registry() const noexcept { return reg; }

        void     setLogLevel(LogLevel lvl) noexcept { sceneLogLevel = lvl; }
        LogLevel getLogLevel() const noexcept { return sceneLogLevel; }

    protected:
        void log(const std::string& msg, LogLevel lvl) const
        {
            if (logger && lvl >= sceneLogLevel)
                logger->log(msg, lvl);
        }

    private:
        Registry  reg{};
        bool      loaded{ false };
        Logger* logger{ nullptr }; // optional shared logger
        Timer* clock{ nullptr };  // optional time reference
        LogLevel  sceneLogLevel{ LogLevel::INFO };
    };

} // namespace epochengine::scene
