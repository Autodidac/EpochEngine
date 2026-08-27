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

#include <string>
#include <vector>

export module ecs.components;

import core.logger; // LogLevel lives here
import core.math;
import core.timer;   // Timer lives here

export namespace epochengine::ecs
{
    // ─── Position ─────────────────────────────────────────────────────────
    struct PositionDomain final {};
    struct VelocityDomain final {};

    using Position = core::math::Vector2<float, PositionDomain>;

    // ─── Velocity ─────────────────────────────────────────────────────────
    using Velocity = core::math::Vector2<float, VelocityDomain>;

    // ─── History ──────────────────────────────────────────────────────────
    // Each entity that needs rewind support keeps its past states here.
    // (Still trivial; if you later need time-stamped state, store time + state.)
    struct History
    {
        std::vector<Position> states;
    };

    // ─── LoggerComponent ──────────────────────────────────────────────────
    // New logger model:
    //  - Logs are system/channel based: logs/<system>.log + console.
    //  - This component is only metadata: it does NOT create files.
    //
    // IMPORTANT:
    //  - Do not bake std::source_location into components.
    //  - Call sites should pass loc (or rely on default loc=current()).
    struct LoggerComponent
    {
        // System/channel name (example: "ECS", "AI", "Vulkan.Texture", etc.)
        std::string system{ "ECS" };

        // Minimum level to emit when THIS entity logs (call sites check this).
        epochengine::logger::LogLevel min_level{ epochengine::logger::LogLevel::INFO };

        // Optional entity-owned clock (nullptr means "use global timing utilities").
        epochengine::timing::Timer* clock{ nullptr };
    };
} // namespace epochengine::ecs
