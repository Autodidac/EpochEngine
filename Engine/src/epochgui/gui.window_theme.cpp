// SPDX-License-Identifier: LicenseRef-MIT-NoSell
module;

#include "gui.theme_cycle.hpp"
#include "gui.theme_palette_spec.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#ifdef _WIN32
#include "../platform/platform.framework.hpp"
#endif

module gui.engine;

namespace epochengine::gui
{
    namespace theme_detail
    {
        void publish_native_window_variant(ThemePreference preference, ThemeVariant variant, bool dark) noexcept;
    }

    namespace
    {
        // The OS chrome policy is independent of the current per-widget theme stack.
        // Default to the OS setting before editor preferences are loaded.
        std::atomic<ThemePreference> g_nativeWindowTheme{ThemePreference::FollowSystemDark};
        std::atomic<ThemeVariant> g_publishedWindowVariant{ThemeVariant::DefaultDark};
        std::atomic<bool> g_cycleWindowDark{true};

        [[nodiscard]] static bool system_prefers_dark_palette() noexcept
        {
#if defined(_WIN32)
            // Windows broadcasts theme changes, but the host and renderer have
            // separate threads. A short per-thread cache avoids a registry read
            // on every rendered frame, including high-refresh editors.
            using Clock = std::chrono::steady_clock;
            static thread_local Clock::time_point lastCheck{};
            static thread_local bool lastDark{};
            const auto now = Clock::now();
            if (lastCheck != Clock::time_point{}
                && now - lastCheck < std::chrono::milliseconds{400})
                return lastDark;
            lastCheck = now;

            DWORD appsUseLightTheme = 1;
            DWORD valueSize = sizeof(appsUseLightTheme);
            const LSTATUS status = ::RegGetValueW(
                HKEY_CURRENT_USER,
                L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                L"AppsUseLightTheme",
                RRF_RT_REG_DWORD,
                nullptr,
                &appsUseLightTheme,
                &valueSize);
            // Windows' default app mode is light when the optional key is absent.
            lastDark = status == ERROR_SUCCESS && appsUseLightTheme == 0;
            return lastDark;
#else
            return true;
#endif
        }
    }

    std::span<const ThemePreferenceChoice> theme_preference_choices() noexcept
    {
        // Infer the list size from its entries. A newly registered preference
        // must not cause C2078 or silently be omitted due to a stale count.
        static constexpr ThemePreferenceChoice kChoices[]{
            { "System Light/Dark", ThemePreference::FollowSystemDark },
            { "Light", ThemePreference::Light },
            { "Dark", ThemePreference::Dark },
            { "Classic Theme", ThemePreference::ClassicLauncher },
            { "Midnight Blue", ThemePreference::MidnightBlue },
            { "Ember Forge", ThemePreference::EmberForge },
            { "Forest Terminal", ThemePreference::ForestTerminal },
            { "Aurora Steel", ThemePreference::AuroraSteel },
            { "Codex Black", ThemePreference::CodexBlack },
            { "Graphite Copper", ThemePreference::GraphiteCopper },
            { "Slate Orchard", ThemePreference::SlateOrchard },
            { "Indigo Quartz", ThemePreference::IndigoQuartz },
            { "Signal Plum", ThemePreference::SignalPlum },
            { "Forged Steel", ThemePreference::ForgedSteel },
            { "Theme Cycle", ThemePreference::ThemeCycle }
        };
        return std::span<const ThemePreferenceChoice>{kChoices};
    }

    std::string_view theme_preference_label(ThemePreference preference) noexcept
    {
        for (const auto& choice : theme_preference_choices())
        {
            if (choice.preference == preference)
                return choice.label;
        }
        if (preference == ThemePreference::ProfessionalDark)
            return "Dark";
        if (preference == ThemePreference::StudioNavy)
            return "Midnight Blue";
        return "System Light/Dark";
    }

    namespace
    {
        detail::ThemeCycleClock g_themeCycle{};
    }

    std::span<const ThemeCycleIntervalChoice> theme_cycle_interval_choices() noexcept
    {
        static constexpr std::array<ThemeCycleIntervalChoice, 10> kIntervals{{
            { "30 seconds", 30u },
            { "1 minute", 60u },
            { "3 minutes", 180u },
            { "5 minutes", 300u },
            { "8 minutes", 480u },
            { "10 minutes", 600u },
            { "15 minutes", 900u },
            { "20 minutes", 1200u },
            { "30 minutes", 1800u },
            { "1 hour", 3600u }
        }};
        return {kIntervals.data(), kIntervals.size()};
    }

    std::uint32_t normalize_theme_cycle_interval(std::uint32_t seconds) noexcept
    {
        for (const auto& choice : theme_cycle_interval_choices())
        {
            if (choice.seconds == seconds)
                return seconds;
        }
        return 30u;
    }

    std::string_view theme_cycle_interval_label(std::uint32_t seconds) noexcept
    {
        const auto normalized = normalize_theme_cycle_interval(seconds);
        for (const auto& choice : theme_cycle_interval_choices())
        {
            if (choice.seconds == normalized)
                return choice.label;
        }
        return "30 seconds";
    }

    void set_theme_cycle_interval(std::uint32_t seconds) noexcept
    {
        const auto normalized = normalize_theme_cycle_interval(seconds);
        g_themeCycle.set_interval(normalized);
    }

    ThemeVariant resolve_theme_preference(ThemePreference preference) noexcept
    {
        if (preference == ThemePreference::ThemeCycle)
        {
            using Clock = std::chrono::steady_clock;
            constexpr std::array<ThemeVariant, 13> kCycle{{
                ThemeVariant::DefaultDark,
                ThemeVariant::DefaultLight,
                ThemeVariant::ClassicLauncher,
                ThemeVariant::MidnightBlue,
                ThemeVariant::EmberForge,
                ThemeVariant::ForestTerminal,
                ThemeVariant::AuroraSteel,
                ThemeVariant::CodexBlack,
                ThemeVariant::GraphiteCopper,
                ThemeVariant::SlateOrchard,
                ThemeVariant::IndigoQuartz,
                ThemeVariant::SignalPlum,
                ThemeVariant::ForgedSteel
            }};
            const auto index = static_cast<std::size_t>(
                g_themeCycle.slot(Clock::now()) % kCycle.size());
            return kCycle[index];
        }

        // Leaving cycle mode freezes the preference immediately; re-entering
        // starts again on Dark instead of advancing while the mode is disabled.
        g_themeCycle.deactivate();
        switch (preference)
        {
        case ThemePreference::Light:
            return ThemeVariant::DefaultLight;
        case ThemePreference::Dark:
        case ThemePreference::ProfessionalDark:
            return ThemeVariant::DefaultDark;
        case ThemePreference::ClassicLauncher:
            return ThemeVariant::ClassicLauncher;
        case ThemePreference::MidnightBlue:
            return ThemeVariant::MidnightBlue;
        case ThemePreference::StudioNavy: // Legacy stored choice maps to the replacement palette.
            return ThemeVariant::MidnightBlue;
        case ThemePreference::EmberForge:
            return ThemeVariant::EmberForge;
        case ThemePreference::ForestTerminal:
            return ThemeVariant::ForestTerminal;
        case ThemePreference::AuroraSteel:
            return ThemeVariant::AuroraSteel;
        case ThemePreference::CodexBlack:
            return ThemeVariant::CodexBlack;
        case ThemePreference::GraphiteCopper:
            return ThemeVariant::GraphiteCopper;
        case ThemePreference::SlateOrchard:
            return ThemeVariant::SlateOrchard;
        case ThemePreference::IndigoQuartz:
            return ThemeVariant::IndigoQuartz;
        case ThemePreference::SignalPlum:
            return ThemeVariant::SignalPlum;
        case ThemePreference::ForgedSteel:
            return ThemeVariant::ForgedSteel;
        case ThemePreference::RemovedLegacyTheme: // Stored ordinal 11: backward-compatible Dark migration.
            return ThemeVariant::DefaultDark;
        case ThemePreference::FollowSystemDark:
            return system_prefers_dark_palette()
                ? ThemeVariant::DefaultDark
                : ThemeVariant::DefaultLight;
        default:
            return ThemeVariant::DefaultDark;
        }
    }

    void theme_detail::publish_native_window_variant(
        ThemePreference preference, ThemeVariant variant, bool dark) noexcept
    {
        g_publishedWindowVariant.store(variant, std::memory_order_relaxed);
        if (preference == ThemePreference::ThemeCycle)
            g_cycleWindowDark.store(dark, std::memory_order_relaxed);
        g_nativeWindowTheme.store(preference, std::memory_order_release);
    }

    NativeWindowChromePalette native_window_chrome_palette() noexcept
    {
        // The window-owner thread never reads render-thread state directly.
        // System theme changes and Theme Cycle update the returned palette ID,
        // so the DWM cache correctly invalidates when the visual palette changes.
        const auto preference = g_nativeWindowTheme.load(std::memory_order_acquire);
        const auto variant = preference == ThemePreference::FollowSystemDark
            ? (system_prefers_dark_palette()
                ? ThemeVariant::DefaultDark : ThemeVariant::DefaultLight)
            : g_publishedWindowVariant.load(std::memory_order_acquire);
        std::size_t index = 0u;
        switch (variant)
        {
        case ThemeVariant::DefaultLight: index = 1u; break;
        case ThemeVariant::ClassicLauncher: index = 2u; break;
        case ThemeVariant::MidnightBlue:
        case ThemeVariant::StudioNavy: index = 3u; break;
        case ThemeVariant::EmberForge: index = 4u; break;
        case ThemeVariant::ForestTerminal: index = 5u; break;
        case ThemeVariant::AuroraSteel: index = 6u; break;
        case ThemeVariant::CodexBlack: index = 7u; break;
        case ThemeVariant::GraphiteCopper: index = 8u; break;
        case ThemeVariant::SlateOrchard: index = 9u; break;
        case ThemeVariant::IndigoQuartz: index = 10u; break;
        case ThemeVariant::SignalPlum: index = 11u; break;
        case ThemeVariant::ForgedSteel: index = 12u; break;
        default: break;
        }
        const auto& spec = theme_detail::palette_at(index);
        return NativeWindowChromePalette{
            .paletteId = static_cast<std::uint8_t>(index),
            .isLight = spec.isLight,
            .backgroundRgb = theme_detail::packed_rgb(spec.window),
            .captionRgb = theme_detail::packed_rgb(spec.title),
            .borderRgb = theme_detail::packed_rgb(spec.hover),
            .textRgb = spec.isLight && !spec.captionUsesLightText ? 0x19273Au : 0xF6F7F8u
        };
    }

    bool native_window_prefers_dark() noexcept
    {
        switch (g_nativeWindowTheme.load(std::memory_order_acquire))
        {
        case ThemePreference::FollowSystemDark:
            return system_prefers_dark_palette();
        case ThemePreference::Light:
            return false;
        case ThemePreference::ThemeCycle:
            return g_cycleWindowDark.load(std::memory_order_relaxed);
        default:
            return !native_window_chrome_palette().isLight;
        }
    }

    ThemeVariant published_window_theme_variant() noexcept
    {
        // Detached panes use the canonical editor palette, not an independently
        // advancing theme cycle or an unpersisted local editor-state default.
        return g_publishedWindowVariant.load(std::memory_order_acquire);
    }
}
