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
module;

export module aecs.components;

import <string>;
import <utility>;
import <vector>;

import aengine.core.logger; // LogLevel lives here
import aengine.core.time;   // Timer lives here

export namespace almondnamespace::ecs
{
    // ─── Position ─────────────────────────────────────────────────────────
    struct Position
    {
        float x{ 0.0f };
        float y{ 0.0f };
    };

    // ─── Velocity ─────────────────────────────────────────────────────────
    struct Velocity
    {
        float dx{ 0.0f };
        float dy{ 0.0f };
    };

    // ─── History ──────────────────────────────────────────────────────────
    // Each entity that needs rewind support keeps its past states here.
    // (Still trivial; if you later need time-stamped state, store time + state.)
    struct History
    {
        std::vector<std::pair<float, float>> states;
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
        almondnamespace::logger::LogLevel min_level{ almondnamespace::logger::LogLevel::INFO };

        // Optional entity-owned clock (nullptr means "use global timing utilities").
        almondnamespace::timing::Timer* clock{ nullptr };
    };
} // namespace almondnamespace::ecs
