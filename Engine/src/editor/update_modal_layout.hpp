#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>
#include <string_view>

namespace epochnamespace::editor_update_modal
{
    struct Vec2
    {
        float x = 0.0f;
        float y = 0.0f;
    };

    struct UpdateFlags
    {
        bool sourceOnlyUpdate = false;
        bool sourceWorkerRunning = false;
        bool updateRunning = false;
        bool restartReady = false;
    };

    struct ActionStrip
    {
        std::array<float, 3> widths{};
        std::size_t count = 0u;
        float height = 0.0f;
        bool stacked = false;
    };

    struct ModalLayout
    {
        Vec2 size{};
        float contentWidth = 1.0f;
        ActionStrip actions{};
    };

    constexpr float kUpdateContentInset = 28.0f;
    constexpr float kSourceContentInset = 24.0f;
    constexpr float kButtonHeight = 30.0f;
    constexpr float kButtonGap = 16.0f;
    constexpr float kButtonStackGap = 8.0f;
    constexpr float kButtonBottomPad = 18.0f;
    constexpr float kButtonTopPad = 12.0f;

    [[nodiscard]] inline Vec2 fit_modal_size(const Vec2 viewport, const Vec2 desired, const Vec2 minimum) noexcept
    {
        const float maxWidth = (std::max)(1.0f, viewport.x - 64.0f);
        const float maxHeight = (std::max)(1.0f, viewport.y - 64.0f);
        const float minWidth = (std::min)(minimum.x, maxWidth);
        const float minHeight = (std::min)(minimum.y, maxHeight);
        return {
            std::clamp(desired.x, minWidth, maxWidth),
            std::clamp(desired.y, minHeight, maxHeight)
        };
    }

    [[nodiscard]] inline std::string trim_status(std::string_view text)
    {
        constexpr std::size_t kMaxModalStatus = 176u;
        if (text.size() <= kMaxModalStatus)
            return std::string{ text };
        return std::string{ text.substr(0u, kMaxModalStatus - 3u) } + "...";
    }

    [[nodiscard]] inline std::string_view intro_text(const UpdateFlags flags) noexcept
    {
        return flags.sourceOnlyUpdate
            ? "No packaged runtime was found for this platform, so Epoch is using the source rebuild lane."
            : "A newer packaged Epoch runtime is available. Epoch will download, verify, stage, and hand off the replacement.";
    }

    [[nodiscard]] inline std::string_view cache_text(const UpdateFlags flags) noexcept
    {
        return flags.sourceOnlyUpdate
            ? "Smart Update checked packaged releases first; source rebuild is the available lane for this platform."
            : "Cached packages are checked before use; stale or broken downloads are replaced.";
    }

    [[nodiscard]] inline std::string action_text(const UpdateFlags flags, const int restartSeconds)
    {
        if (flags.restartReady)
        {
            (void)restartSeconds;
            return "The update is staged. Press Restart when you are ready to close Epoch and let the hidden handoff replace the runtime.";
        }

        if (flags.sourceWorkerRunning)
            return "Keep Epoch open while the source worker runs. Cancel stops at the next safe checkpoint.";

        if (flags.sourceOnlyUpdate)
            return "Use Update From Source to build the newer source locally, or Cancel to stay on this build.";

        return "Install Release is recommended. Advanced Source is only for intentionally building latest main locally.";
    }

    [[nodiscard]] inline std::string_view source_line(std::size_t index) noexcept
    {
        constexpr std::array<std::string_view, 4> kLines{
            "Advanced Source skips the packaged runtime and rebuilds the latest main source locally.",
            "This is slower and riskier than Install Release. It is for source testing, not the default update path.",
            "Epoch deletes stale source snapshots before downloading, restores dependencies, rebuilds, and records handoff evidence.",
            "For normal users, press Back and choose Install Release."
        };
        return index < kLines.size() ? kLines[index] : std::string_view{};
    }

    [[nodiscard]] inline float action_strip_height(
        const std::array<float, 3>& widths,
        const std::size_t count,
        const float availableWidth) noexcept
    {
        if (count == 0u)
            return 0.0f;

        float horizontalWidth = 0.0f;
        for (std::size_t i = 0u; i < count; ++i)
        {
            horizontalWidth += widths[i];
            if (i + 1u < count)
                horizontalWidth += kButtonGap;
        }

        if (horizontalWidth <= (std::max)(1.0f, availableWidth))
            return kButtonHeight;

        return static_cast<float>(count) * kButtonHeight
            + static_cast<float>(count - 1u) * kButtonStackGap;
    }

    [[nodiscard]] inline ActionStrip update_action_strip(const UpdateFlags flags, const float contentWidth) noexcept
    {
        ActionStrip strip{};
        const float cancelButtonWidth = flags.sourceWorkerRunning ? 148.0f : 120.0f;
        const float primaryButtonWidth = (std::min)(220.0f, (std::max)(160.0f, contentWidth * 0.34f));
        const float advancedButtonWidth = (std::min)(190.0f, (std::max)(156.0f, contentWidth * 0.28f));
        const bool showCancelButton = flags.sourceWorkerRunning || (!flags.updateRunning && !flags.restartReady);
        const bool showPrimaryButton = !flags.updateRunning;
        const bool showAdvancedSourceButton = !flags.updateRunning && !flags.restartReady;

        if (showCancelButton)
            strip.widths[strip.count++] = cancelButtonWidth;
        if (showPrimaryButton)
            strip.widths[strip.count++] = primaryButtonWidth;
        if (showAdvancedSourceButton)
            strip.widths[strip.count++] = advancedButtonWidth;

        strip.height = strip.count > 0u
            ? action_strip_height(strip.widths, strip.count, contentWidth)
            : kButtonHeight;
        strip.stacked = strip.height > kButtonHeight + 0.5f;
        return strip;
    }

    [[nodiscard]] inline ActionStrip source_action_strip(const float contentWidth) noexcept
    {
        ActionStrip strip{};
        strip.widths = { 120.0f, 120.0f, 176.0f };
        strip.count = strip.widths.size();
        strip.height = action_strip_height(strip.widths, strip.count, contentWidth);
        strip.stacked = strip.height > kButtonHeight + 0.5f;
        return strip;
    }

    template <typename MeasureWrappedText>
    [[nodiscard]] inline ModalLayout measure_update_layout(
        const Vec2 viewport,
        const UpdateFlags flags,
        const std::string_view status,
        const int restartSeconds,
        MeasureWrappedText measure_wrapped_text)
    {
        ModalLayout layout{};
        layout.size.x = fit_modal_size({ viewport.x, viewport.y }, { 760.0f, 1.0f }, { 660.0f, 1.0f }).x;
        layout.contentWidth = (std::max)(1.0f, layout.size.x - 2.0f * kUpdateContentInset);
        layout.actions = update_action_strip(flags, layout.contentWidth);

        const std::string statusLine = trim_status(status);
        const std::string actionLine = action_text(flags, restartSeconds);

        float desiredHeight = 54.0f;
        desiredHeight += measure_wrapped_text(intro_text(flags), layout.contentWidth) + 8.0f;
        desiredHeight += measure_wrapped_text(statusLine, layout.contentWidth) + 10.0f;
        desiredHeight += 36.0f;
        desiredHeight += measure_wrapped_text(cache_text(flags), layout.contentWidth) + 8.0f;
        desiredHeight += measure_wrapped_text(actionLine, layout.contentWidth) + kButtonTopPad;
        desiredHeight += layout.actions.height + kButtonBottomPad;

        const float minHeight = flags.sourceWorkerRunning ? 278.0f : flags.restartReady ? 258.0f : 286.0f;
        layout.size = fit_modal_size({ viewport.x, viewport.y }, { layout.size.x, desiredHeight }, { 660.0f, minHeight });
        return layout;
    }

    template <typename MeasureWrappedText>
    [[nodiscard]] inline ModalLayout measure_source_layout(
        const Vec2 viewport,
        MeasureWrappedText measure_wrapped_text)
    {
        ModalLayout layout{};
        layout.size.x = fit_modal_size({ viewport.x, viewport.y }, { 640.0f, 1.0f }, { 600.0f, 1.0f }).x;
        layout.contentWidth = (std::max)(1.0f, layout.size.x - 2.0f * kSourceContentInset);
        layout.actions = source_action_strip(layout.contentWidth);

        float textHeight = 0.0f;
        for (std::size_t i = 0u; i < 4u; ++i)
            textHeight += measure_wrapped_text(source_line(i), layout.contentWidth) + (i + 1u < 4u ? 12.0f : 0.0f);

        const float desiredHeight = 54.0f + textHeight + 18.0f + layout.actions.height + kButtonBottomPad;
        layout.size = fit_modal_size({ viewport.x, viewport.y }, { layout.size.x, desiredHeight }, { 600.0f, 300.0f });
        return layout;
    }
}
