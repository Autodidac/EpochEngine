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

#include "../include/core.stl_types.hpp"
#include <string_view>

export module perf.tier;

import core.env;
import core.time;

export namespace epochengine::perf
{
    enum class tier : std::uint8_t
    {
        uncapped,
        mobile_30,
        deck_40,
        desktop_60,
        editor_120
    };

    enum class frame_limit_preset : std::uint8_t
    {
        fps_60,
        fps_120,
        unlimited
    };

    enum class frame_pacing_mode : std::uint8_t
    {
        target_hz,
        vsync,
        uncapped
    };

    enum class frame_activity : std::uint8_t
    {
        foreground,
        background,
        minimized
    };

    struct frame_pacing_policy final
    {
        frame_pacing_mode mode = frame_pacing_mode::target_hz;
        double target_hz = 120.0;
        double background_hz = 30.0;
        double minimized_hz = 10.0;
    };

    struct frame_pacing_capabilities final
    {
        bool vsync = false;
        bool target_hz = false;

        [[nodiscard]] constexpr bool operator==(
            const frame_pacing_capabilities&) const noexcept = default;
    };

    enum class frame_pacing_backend : std::uint8_t
    {
        opengl,
        sdl,
        vulkan,
        raylib,
        sfml,
        directx,
        software
    };

    [[nodiscard]] constexpr frame_pacing_capabilities
        frame_pacing_capabilities_for(
            const frame_pacing_backend backend) noexcept
    {
        switch (backend)
        {
        case frame_pacing_backend::raylib:
            return {false, true};
        case frame_pacing_backend::sfml:
            return {true, true};
        case frame_pacing_backend::opengl:
        case frame_pacing_backend::sdl:
        case frame_pacing_backend::vulkan:
        case frame_pacing_backend::directx:
            return {true, false};
        case frame_pacing_backend::software:
        default:
            return {false, false};
        }
    }

    struct native_frame_pacing_result final
    {
        bool configured = false;
        bool pacing_active = false;
        frame_pacing_mode effective_mode = frame_pacing_mode::uncapped;
        double effective_hz = 0.0;

        [[nodiscard]] constexpr bool operator==(
            const native_frame_pacing_result&) const noexcept = default;
    };

    struct frame_pacing_plan final
    {
        frame_pacing_mode requested_mode = frame_pacing_mode::target_hz;
        frame_pacing_mode effective_mode = frame_pacing_mode::target_hz;
        frame_activity activity = frame_activity::foreground;
        double configured_hz = 120.0;
        double effective_hz = 120.0;
        bool native_vsync_requested = false;
        bool native_pacing_requested = false;
        bool native_pacing_configured = false;
        bool native_pacing_active = false;
        bool cpu_deadline_wait = true;

        [[nodiscard]] constexpr bool operator==(const frame_pacing_plan&) const noexcept = default;
    };

    enum class native_present_mode : std::uint8_t
    {
        immediate,
        mailbox,
        fifo
    };

    struct native_present_mode_support final
    {
        bool immediate = false;
        bool mailbox = false;
        bool fifo = false;
    };

    struct native_present_mode_selection final
    {
        native_present_mode mode = native_present_mode::fifo;
        bool available = false;
        bool request_honored = false;
        bool pacing_active = false;
    };

    [[nodiscard]] constexpr native_present_mode_selection
        select_native_present_mode(
            frame_pacing_mode requested,
            native_present_mode_support support) noexcept
    {
        if (requested == frame_pacing_mode::vsync && support.fifo)
            return {native_present_mode::fifo, true, true, true};

        if (support.immediate)
        {
            return {
                native_present_mode::immediate,
                true,
                requested != frame_pacing_mode::vsync,
                false};
        }
        if (support.mailbox)
        {
            return {
                native_present_mode::mailbox,
                true,
                requested != frame_pacing_mode::vsync,
                false};
        }
        if (support.fifo)
        {
            return {
                native_present_mode::fifo,
                true,
                requested == frame_pacing_mode::vsync,
                true};
        }
        return {};
    }

    [[nodiscard]] constexpr double sanitize_frame_hz(
        double requested,
        double fallback) noexcept
    {
        if (!(requested > 0.0) || requested > 1000.0)
            return fallback;
        return requested;
    }

    [[nodiscard]] constexpr frame_pacing_policy target_hz_policy(
        double target_hz,
        double background_hz = 30.0,
        double minimized_hz = 10.0) noexcept
    {
        if (!(target_hz > 0.0))
        {
            return {
                frame_pacing_mode::uncapped,
                0.0,
                sanitize_frame_hz(background_hz, 30.0),
                sanitize_frame_hz(minimized_hz, 10.0)};
        }
        return {
            frame_pacing_mode::target_hz,
            sanitize_frame_hz(target_hz, 120.0),
            sanitize_frame_hz(background_hz, 30.0),
            sanitize_frame_hz(minimized_hz, 10.0)};
    }

    [[nodiscard]] constexpr frame_pacing_plan resolve_frame_pacing_with_capabilities(
        frame_pacing_policy policy,
        frame_activity activity,
        frame_pacing_capabilities capabilities) noexcept
    {
        policy.target_hz = policy.mode == frame_pacing_mode::uncapped
            ? 0.0
            : sanitize_frame_hz(policy.target_hz, 120.0);
        policy.background_hz = sanitize_frame_hz(policy.background_hz, 30.0);
        policy.minimized_hz = sanitize_frame_hz(policy.minimized_hz, 10.0);

        frame_pacing_plan plan{
            policy.mode,
            policy.mode,
            activity,
            policy.target_hz,
            policy.target_hz,
            false,
            false,
            false,
            false,
            policy.mode == frame_pacing_mode::target_hz};

        if (activity == frame_activity::background)
        {
            plan.effective_mode = frame_pacing_mode::target_hz;
            plan.effective_hz = policy.background_hz;
            plan.native_pacing_requested = capabilities.target_hz;
            plan.cpu_deadline_wait = !plan.native_pacing_requested;
            return plan;
        }
        if (activity == frame_activity::minimized)
        {
            plan.effective_mode = frame_pacing_mode::target_hz;
            plan.effective_hz = policy.minimized_hz;
            plan.native_pacing_requested = capabilities.target_hz;
            plan.cpu_deadline_wait = !plan.native_pacing_requested;
            return plan;
        }

        if (policy.mode == frame_pacing_mode::uncapped)
        {
            plan.effective_hz = 0.0;
            plan.cpu_deadline_wait = false;
            return plan;
        }
        if (policy.mode == frame_pacing_mode::vsync)
        {
            if (capabilities.vsync)
            {
                plan.effective_hz = 0.0;
                plan.native_vsync_requested = true;
                plan.native_pacing_requested = true;
                plan.cpu_deadline_wait = false;
            }
            else
            {
                plan.effective_mode = frame_pacing_mode::target_hz;
                plan.effective_hz = policy.target_hz;
                plan.native_pacing_requested = capabilities.target_hz;
                plan.cpu_deadline_wait = !plan.native_pacing_requested;
            }
        }
        else if (policy.mode == frame_pacing_mode::target_hz
            && capabilities.target_hz)
        {
            plan.native_pacing_requested = true;
            plan.cpu_deadline_wait = false;
        }
        return plan;
    }

    [[nodiscard]] constexpr frame_pacing_plan resolve_frame_pacing(
        frame_pacing_policy policy,
        frame_activity activity,
        bool native_vsync_available) noexcept
    {
        return resolve_frame_pacing_with_capabilities(
            policy,
            activity,
            {native_vsync_available, false});
    }

    [[nodiscard]] constexpr frame_pacing_plan finalize_native_frame_pacing(
        frame_pacing_plan plan,
        native_frame_pacing_result native) noexcept
    {
        plan.native_pacing_configured = native.configured;
        plan.native_pacing_active = native.pacing_active;
        const bool preserveCpuFallback = plan.cpu_deadline_wait;

        if (native.pacing_active)
        {
            plan.effective_mode = native.effective_mode;
            plan.effective_hz = native.effective_hz;
            plan.cpu_deadline_wait = false;
            plan.native_vsync_requested =
                native.effective_mode == frame_pacing_mode::vsync;
            plan.cpu_deadline_wait = preserveCpuFallback;
            return plan;
        }

        if (plan.native_pacing_requested)
        {
            const double fallback_hz = plan.effective_hz > 0.0
                ? plan.effective_hz
                : (plan.configured_hz > 0.0 ? plan.configured_hz : 60.0);
            plan.effective_mode = frame_pacing_mode::target_hz;
            plan.effective_hz = fallback_hz;
            plan.cpu_deadline_wait = true;
            plan.native_vsync_requested = false;
        }
        return plan;
    }

    [[nodiscard]] constexpr frame_pacing_policy select_frame_pacing_policy(
        frame_pacing_mode explicit_mode,
        double explicit_target_hz,
        bool explicit_selection,
        bool standalone_project) noexcept
    {
        if (!explicit_selection)
            return target_hz_policy(standalone_project ? 60.0 : 120.0);
        if (explicit_mode == frame_pacing_mode::uncapped)
            return target_hz_policy(0.0);
        if (explicit_mode == frame_pacing_mode::vsync)
        {
            return {
                frame_pacing_mode::vsync,
                sanitize_frame_hz(explicit_target_hz, 60.0),
                30.0,
                10.0};
        }
        return target_hz_policy(explicit_target_hz);
    }
    [[nodiscard]] tier tier_from_env() noexcept;
    [[nodiscard]] double target_fps_for(tier t) noexcept;
    [[nodiscard]] double target_fps_for(frame_limit_preset preset) noexcept;
    [[nodiscard]] const char* to_string(tier t) noexcept;
    [[nodiscard]] const char* to_string(frame_limit_preset preset) noexcept;
    [[nodiscard]] const char* label_for_frame_limit(double fps) noexcept;
    [[nodiscard]] const char* to_string(frame_pacing_mode mode) noexcept;
    [[nodiscard]] const char* to_string(frame_activity activity) noexcept;
    [[nodiscard]] const char* to_string(native_present_mode mode) noexcept;

    [[nodiscard]] constexpr double next_frame_deadline(
        double previous_deadline,
        double now,
        double target_dt,
        bool started) noexcept
    {
        if (!(target_dt > 0.0))
            return now;
        if (!started || previous_deadline < now - target_dt)
            previous_deadline = now;
        return previous_deadline + target_dt;
    }

    struct frame_limiter
    {
        double target_dt = 0.0;
        double next_time = 0.0;
        bool started = false;
        frame_pacing_plan active_plan{};

        void set_target_fps(double fps) noexcept
        {
            target_dt = (fps > 0.0) ? (1.0 / fps) : 0.0;
            started = false;
            next_time = 0.0;
        }

        void reset() noexcept
        {
            started = false;
            next_time = 0.0;
        }

        void wait_for_next_frame() noexcept;

        void set_plan(frame_pacing_plan plan) noexcept
        {
            active_plan = plan;
            set_target_fps(plan.cpu_deadline_wait ? plan.effective_hz : 0.0);
        }

        [[nodiscard]] const frame_pacing_plan& plan() const noexcept
        {
            return active_plan;
        }
    };
}

namespace epochengine::perf
{
    namespace
    {
        [[nodiscard]] inline bool sv_eq(std::string_view a, const char* b) noexcept
        {
            // compare() avoids operator== ambiguity caused by your epoch overloads.
            return a.compare(b) == 0;
        }
    }

    tier tier_from_env() noexcept
    {
        if (auto v = epochengine::core::env::get(epochengine::string_view{ "EPOCH_TIER" }))
        {
            const std::string_view s = epochengine::to_std(*v);

            if (sv_eq(s, "mobile") || sv_eq(s, "30")) return tier::mobile_30;
            if (sv_eq(s, "deck") || sv_eq(s, "40")) return tier::deck_40;
            if (sv_eq(s, "desktop") || sv_eq(s, "60")) return tier::desktop_60;
            if (sv_eq(s, "editor") || sv_eq(s, "120")) return tier::editor_120;
            if (sv_eq(s, "uncapped") || sv_eq(s, "0"))  return tier::uncapped;
        }
        return tier::editor_120;
    }

    double target_fps_for(tier t) noexcept
    {
        switch (t)
        {
        case tier::mobile_30:  return 30.0;
        case tier::deck_40:    return 40.0;
        case tier::desktop_60: return 60.0;
        case tier::editor_120: return 120.0;
        case tier::uncapped:   return 0.0;
        }
        return 120.0;
    }

    double target_fps_for(frame_limit_preset preset) noexcept
    {
        switch (preset)
        {
        case frame_limit_preset::fps_60:    return 60.0;
        case frame_limit_preset::fps_120:   return 120.0;
        case frame_limit_preset::unlimited: return 0.0;
        }
        return 120.0;
    }

    const char* to_string(tier t) noexcept
    {
        switch (t)
        {
        case tier::mobile_30:  return "mobile_30";
        case tier::deck_40:    return "deck_40";
        case tier::desktop_60: return "desktop_60";
        case tier::editor_120: return "editor_120";
        case tier::uncapped:   return "uncapped";
        }
        return "editor_120";
    }

    const char* to_string(frame_limit_preset preset) noexcept
    {
        switch (preset)
        {
        case frame_limit_preset::fps_60:    return "60";
        case frame_limit_preset::fps_120:   return "120";
        case frame_limit_preset::unlimited: return "unlimited";
        }
        return "120";
    }

    const char* label_for_frame_limit(double fps) noexcept
    {
        if (fps <= 0.0)
            return "Unlimited";
        if (fps >= 119.5 && fps <= 120.5)
            return "120 FPS";
        if (fps >= 59.5 && fps <= 60.5)
            return "60 FPS";
        return "Custom";
    }


    const char* to_string(frame_pacing_mode mode) noexcept
    {
        switch (mode)
        {
        case frame_pacing_mode::target_hz: return "target_hz";
        case frame_pacing_mode::vsync: return "vsync";
        case frame_pacing_mode::uncapped: return "uncapped";
        }
        return "target_hz";
    }

    const char* to_string(frame_activity activity) noexcept
    {
        switch (activity)
        {
        case frame_activity::foreground: return "foreground";
        case frame_activity::background: return "background";
        case frame_activity::minimized: return "minimized";
        }
        return "foreground";
    }

    const char* to_string(native_present_mode mode) noexcept
    {
        switch (mode)
        {
        case native_present_mode::immediate: return "immediate";
        case native_present_mode::mailbox: return "mailbox";
        case native_present_mode::fifo: return "fifo";
        }
        return "fifo";
    }

    void frame_limiter::wait_for_next_frame() noexcept
    {
        if (target_dt <= 0.0)
            return;

        const double now0 = epochengine::core::time::now_seconds();

        next_time = next_frame_deadline(
            next_time, now0, target_dt, started);
        started = true;

        for (;;)
        {
            const double now = epochengine::core::time::now_seconds();
            const double remaining = next_time - now;
            if (remaining <= 0.0)
                break;

            const double ms = remaining * 1000.0;
            if (ms > 2.0)
                epochengine::core::time::sleep_ms(static_cast<std::uint32_t>(ms - 1.0));
        }
    }
}
