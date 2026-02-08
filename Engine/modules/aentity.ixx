module;

export module aentity;

// ─────────────────────────────────────────────────────────────
// STANDARD LIBRARY
// ─────────────────────────────────────────────────────────────
import <string>;
import <string_view>;
import <format>;

// ─────────────────────────────────────────────────────────────
// ENGINE / FRAMEWORK MODULES
// All of these must already be real modules.
// No textual includes remain.
// ─────────────────────────────────────────────────────────────
import aengine.platform;          // replaces aplatform.hpp (ordering handled by BMI)
import aengine.core.logger;    // Logger, LogLevel
import aengine.eventsystem;              // events::push_event
import aengine.core.time;        // Timer, time helpers
import aecs;                      // reg_ex, Entity, ECS core API
import aecs.components;         // Position, History, LoggerComponent


// ─────────────────────────────────────────────────────────────
export namespace almondnamespace::ecs
{
    using almondnamespace::timing::Timer;
    using almondnamespace::logger::LogLevel;

    // ─────────────────────────────────────────────────────────
    // SPAWN ENTITY
    // ─────────────────────────────────────────────────────────
    export template<typename... Cs>
        inline Entity spawn_entity(
            reg_ex<Cs...>& R,
            std::string_view logfile,
            LogLevel lvl,
            Timer& clock)
    {
        const Entity e = create_entity(R);

        add_component<Position>(R, e, {});
        add_component<History>(R, e, { { { 0.f, 0.f } } });
        add_component<LoggerComponent>(
            R, e,
            { std::string(logfile), lvl, &clock });

        if (R.log && R.clk)
        {
            R.log->log(std::format(
                "[ECS] Entity {} spawned at {}",
                e,
                almondnamespace::timing::getCurrentTimeString()));
        }

        events::push_event(events::Event{
            events::EventType::Custom,
            {
                {"action", "spawn"},
                {"entity", std::to_string(e)}
            }
            });

        return e;
    }

    // ─────────────────────────────────────────────────────────
    // MOVE ENTITY
    // ─────────────────────────────────────────────────────────
    export template<typename... Cs>
        inline void move_entity(
            reg_ex<Cs...>& R,
            Entity e,
            float dx,
            float dy)
    {
        auto& pos = get_component<Position>(R, e);
        auto& hist = get_component<History>(R, e);

        // snapshot previous state
        hist.states.emplace_back(pos.x, pos.y);

        pos.x += dx;
        pos.y += dy;

        auto& lc = get_component<LoggerComponent>(R, e);
        almondnamespace::logger::Logger logger{ lc.file, *lc.clock, lc.level };

        const std::string ts = lc.clock->getCurrentTimeString();
        logger.log(std::format(
            "[ECS] Entity {} moved to ({:.2f},{:.2f}) at {}",
            e, pos.x, pos.y, ts));

        events::push_event(events::Event{
            events::EventType::Custom,
            {
                {"action", "move"},
                {"entity", std::to_string(e)},
                {"x", std::to_string(pos.x)},
                {"y", std::to_string(pos.y)},
                {"time", ts}
            },
            pos.x,
            pos.y
            });
    }

    // ─────────────────────────────────────────────────────────
    // REWIND ENTITY
    // ─────────────────────────────────────────────────────────
    export template<typename... Cs>
        inline bool rewind_entity(reg_ex<Cs...>& R, Entity e)
    {
        auto& hist = get_component<History>(R, e);
        if (hist.states.size() <= 1)
            return false;

        hist.states.pop_back();
        const auto [px, py] = hist.states.back();

        auto& pos = get_component<Position>(R, e);
        pos.x = px;
        pos.y = py;

        auto& lc = get_component<LoggerComponent>(R, e);
        almondnamespace::logger::Logger logger{ lc.file, *lc.clock, lc.level };

        const std::string ts = lc.clock->getCurrentTimeString();
        logger.log(std::format(
            "[ECS] Entity {} rewound to ({:.2f},{:.2f}) at {}",
            e, pos.x, pos.y, ts));

        events::push_event(events::Event{
            events::EventType::Custom,
            {
                {"action", "rewind"},
                {"entity", std::to_string(e)},
                {"x", std::to_string(pos.x)},
                {"y", std::to_string(pos.y)},
                {"time", ts}
            },
            pos.x,
            pos.y
            });

        return true;
    }

} // namespace almondnamespace::ecs
