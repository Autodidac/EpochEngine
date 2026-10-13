/***********************************************
 * Epoch Engine - host runtime logging policy.
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ***********************************************/
#pragma once

#include <chrono>

namespace epochengine::rendercontext::logging
{
    // Report activity/visibility changes immediately, but combine the dozens of
    // intermediate dimensions produced by interactive window resizing.
    template<class WindowState>
    class WindowStateLogGate final
    {
    public:
        using clock = std::chrono::steady_clock;

        [[nodiscard]] bool should_report(
            const WindowState& state,
            const clock::time_point now = clock::now()) noexcept
        {
            if (!m_initialized || significant_transition(m_reported, state))
            {
                m_initialized = true;
                m_pending = false;
                m_reported = state;
                m_observed = state;
                return true;
            }

            if (state != m_observed)
            {
                m_observed = state;
                m_last_change = now;
                m_pending = state != m_reported;
            }

            if (m_pending && now - m_last_change >= std::chrono::milliseconds{ 350 })
            {
                m_reported = m_observed;
                m_pending = false;
                return true;
            }
            return false;
        }

    private:
        [[nodiscard]] static constexpr bool significant_transition(
            const WindowState& from, const WindowState& to) noexcept
        {
            return from.activity != to.activity
                || from.presentable != to.presentable
                || from.visible != to.visible
                || from.focused != to.focused
                || from.minimized != to.minimized
                || from.occluded != to.occluded
                || from.dpi_milli != to.dpi_milli
                || from.dpi_known != to.dpi_known
                || from.visibility_known != to.visibility_known
                || from.focus_known != to.focus_known
                || from.minimized_known != to.minimized_known
                || from.occlusion_known != to.occlusion_known;
        }

        bool m_initialized = false;
        bool m_pending = false;
        WindowState m_reported{};
        WindowState m_observed{};
        clock::time_point m_last_change{};
    };
}
