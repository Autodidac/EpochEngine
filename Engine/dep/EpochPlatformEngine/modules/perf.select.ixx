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
 *   SPDX-License-Identifier: MPL-2.0             *
 *                                              *
 *   Provided "AS IS", without warranty         *
 *   of any kind.                               *
 *                                              *
 *                                              *
 *   Redistribution Allowed with This Notice    *
 *   and LICENSE file.                          *
 *                                              *
 *                                              *
 *   See LICENSE file for full terms.           *
 *                                              *
 ***********************************************/
// Epoch system notes (auto-checked):
// Epoch STL: honor core.stl_types.hpp types/operators; fix callers, not core overloads.
 // ============================================================================
// modules/perf.select.ixx
// Capabilities -> perf tier selection, with env override.
// Depends on: platform.capabilities, perf.tier
// ============================================================================
module;

#include "../include/core.stl_types.hpp"

export module perf.select;

import platform.capabilities;
import perf.tier;
import core.env;

export namespace epochengine::perf
{
    // If EPOCH_TIER is set, it wins. Otherwise choose by capabilities.
    [[nodiscard]] inline tier select_tier(const epochengine::Capabilities& caps) noexcept
    {
        // Env override wins (mobile/deck/desktop/editor/uncapped or 30/40/60/120/0)
        if (auto v = epochengine::core::env::get(epochengine::to_view(std::string_view{ "EPOCH_TIER" })))

            return tier_from_env();

        // Pure policy: map your GPU tier to perf tier.
        // You can tweak these defaults later.
        const auto key = epochengine::make_tier_key(caps);

        switch (key.tier)
        {
        case epochengine::GpuTier::tier_c_mobile:
            return tier::mobile_30;

        case epochengine::GpuTier::tier_b_mid:
            // Mid-tier PC: 60 is usually fine.
            return tier::deck_40;

        case epochengine::GpuTier::tier_a_desktop:
            // Editor-capable desktop: the shared core frame limiter defaults to 120.
            return tier::editor_120;
        }

        return tier::editor_120;
    }

    [[nodiscard]] inline double target_fps_for_caps(const epochengine::Capabilities& caps) noexcept
    {
        return target_fps_for(select_tier(caps));
    }
}
