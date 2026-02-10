// ascene.ixx
module;
export module ascene;

// Centralized platform glue (no OS headers here)
import aengine.platform;

// Engine modules (must already be modules)
import aecs;                  // ecs::reg_ex, create_entity, etc.
import aecs.components;     // Position, etc.
import aengine.event.movement;        // MovementEvent

import aengine.core.time;    // time::Timer
import aengine.core.context;              // core::Context
import aengine.context.window; // core::WindowData
import aengine.core.logger;    // Logger, LogLevel

// STL
import std;


export namespace almondnamespace::scene
{
    using almondnamespace::ecs::Entity;
    using almondnamespace::ecs::reg_ex;
    using almondnamespace::logger::Logger;
    using almondnamespace::logger::LogLevel;
    using almondnamespace::timing::Timer;

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
            std::shared_ptr<almondnamespace::core::Context>,
            almondnamespace::core::WindowData*)
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

} // namespace almondnamespace::scene
