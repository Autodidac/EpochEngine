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
 // ============================================================================
// modules/perf.select.ixx
// Capabilities -> perf tier selection, with env override.
// Depends on: platform.capabilities, perf.tier
// ============================================================================
module;

#include "../include/_epoch.stl_types.hpp"

export module perf.select;

import platform.capabilities;
import perf.tier;
import core.env;

export namespace epoch::perf
{
    // If EPOCH_TIER is set, it wins. Otherwise choose by capabilities.
    [[nodiscard]] inline tier select_tier(const epoch::Capabilities& caps) noexcept
    {
        // Env override wins (mobile/deck/desktop/editor/uncapped or 30/40/60/120/0)
        if (auto v = epoch::core::env::get(epoch::to_view(std::string_view{ "EPOCH_TIER" })))

            return tier_from_env();

        // Pure policy: map your GPU tier to perf tier.
        // You can tweak these defaults later.
        const auto key = epoch::make_tier_key(caps);

        switch (key.tier)
        {
        case epoch::GpuTier::tier_c_mobile:
            return tier::mobile_30;

        case epoch::GpuTier::tier_b_mid:
            // Mid-tier PC: 60 is usually fine.
            return tier::deck_40;

        case epoch::GpuTier::tier_a_desktop:
            // Editor-capable desktop: the shared core frame limiter defaults to 120.
            return tier::editor_120;
        }

        return tier::editor_120;
    }

    [[nodiscard]] inline double target_fps_for_caps(const epoch::Capabilities& caps) noexcept
    {
        return target_fps_for(select_tier(caps));
    }
}
