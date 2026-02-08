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
export module aecs.internal_private;

import <format>;
import <string>;
import <string_view>;

import aecs.storage;
import aengine.core.logger;
import aengine.core.time;
import aengine.eventsystem;

namespace almondnamespace::ecs::_detail
{
	using namespace almondnamespace::logger;
    using namespace almondnamespace::timing;
    //using namespace almondnamespace::ecs;

    inline void notify(Logger* log,
        Timer* clk,
        Entity e,
        std::string_view action,
        std::string_view comp)
    {
        if (!log || !clk) return;
        auto ts = timing::getCurrentTimeString();
        log->log(std::format("[ECS] {}{} entity={} at {}",
            action,
            comp.empty() ? "" : std::format(":{}", comp),
            e, ts));
        events::push_event(events::Event{
            events::EventType::Custom,
            { {"ecs_action", std::string(action)},
              {"entity",     std::to_string(e)},
              {"component",  std::string(comp)},
              {"timing",       ts} },
            0.f, 0.f });
    }
}
