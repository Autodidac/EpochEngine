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

#include "core.format_text.hpp"

#include <string>
#include <string_view>

export module ecs.internal_private;

import ecs.storage;
import core.logger;
import core.timer;
import events.input;

namespace epochengine::ecs::_detail
{
	using namespace epochengine::logger;
    using namespace epochengine::timing;
    //using namespace epochengine::ecs;

    inline void notify(Logger* log,
        Timer* clk,
        Entity e,
        std::string_view action,
        std::string_view comp)
    {
        if (!log || !clk) return;
        auto ts = timing::getCurrentTimeString();
        log->log(epochengine::format_text("[ECS] {}{} entity={} at {}",
            action,
            comp.empty() ? "" : epochengine::format_text(":{}", comp),
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
