#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>
#include <string_view>

namespace epochengine::editor_update_modal
{
    struct Vec2
    {
        float x = 0.0f;
        float y = 0.0f;
    };

    struct UpdateFlags
    {
        bool checkFailed = false;
        bool sourceOnlyUpdate = false;
        bool sourceWorkerRunning = false;
        bool updateRunning = false;
        bool restartReady = false;
        bool installableUpdate = false;
        bool projectSourceDownload = false;
        bool sourceCancelAvailable = false;
        bool sourceAuthorizationRunning = false;
    };

    struct ActionStrip
    {
        std::array<float, 4> widths{};
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

    struct PackageManagerBodyLayout
    {
        float listHeight = 0.0f;
        float detailHeight = 0.0f;
        float footerTop = 0.0f;
        bool showDetails = false;
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

    [[nodiscard]] constexpr PackageManagerBodyLayout measure_package_manager_body(
        const float bodyTop,
        const float contentBottom,
        const float lineHeight) noexcept
    {
        constexpr float kFooterHeight = 76.0f;
        constexpr float kMinimumPanelHeight = 48.0f;
        constexpr float kMaximumListHeight = 620.0f;
        constexpr float kMaximumDetailHeight = 360.0f;
        constexpr float kSectionSpacing = 12.0f;

        PackageManagerBodyLayout layout{};
        layout.footerTop = (std::max)(bodyTop, contentBottom - kFooterHeight);
        const float panelBudget = (std::max)(
            0.0f,
            layout.footerTop - bodyTop - (std::max)(0.0f, lineHeight)
                - kSectionSpacing);

        if (panelBudget < 2.0f * kMinimumPanelHeight)
        {
            layout.listHeight = panelBudget;
            return layout;
        }

        layout.listHeight = std::clamp(
            panelBudget * 0.52f,
            kMinimumPanelHeight,
            kMaximumListHeight);
        layout.detailHeight = std::clamp(
            panelBudget - layout.listHeight,
            kMinimumPanelHeight,
            kMaximumDetailHeight);

        const float unused = panelBudget - layout.listHeight - layout.detailHeight;
        if (unused > 0.0f)
        {
            const float listRoom = kMaximumListHeight - layout.listHeight;
            const float listGrowth = (std::min)(unused, listRoom);
            layout.listHeight += listGrowth;
            const float detailRoom = kMaximumDetailHeight - layout.detailHeight;
            layout.detailHeight += (std::min)(unused - listGrowth, detailRoom);
        }
        layout.showDetails = true;
        return layout;
    }

    [[nodiscard]] constexpr bool package_manager_layout_contract() noexcept
    {
        const auto normal = measure_package_manager_body(180.0f, 580.0f, 20.0f);
        const auto compact = measure_package_manager_body(180.0f, 380.0f, 20.0f);
        const auto tiny = measure_package_manager_body(180.0f, 310.0f, 20.0f);
        const auto expanded = measure_package_manager_body(180.0f, 1180.0f, 20.0f);
        return normal.showDetails
            && normal.listHeight >= 48.0f
            && normal.detailHeight >= 48.0f
            && normal.listHeight <= 620.0f
            && normal.detailHeight <= 360.0f
            && normal.footerTop == 504.0f
            && !compact.showDetails
            && compact.footerTop == 304.0f
            && compact.listHeight <= 92.0f
            && compact.detailHeight == 0.0f
            && !tiny.showDetails
            && tiny.detailHeight == 0.0f
            && tiny.listHeight >= 0.0f
            && expanded.showDetails
            && expanded.listHeight > normal.listHeight
            && expanded.detailHeight > normal.detailHeight
            && expanded.footerTop == 1104.0f;
    }

    static_assert(package_manager_layout_contract());

    [[nodiscard]] inline std::string format_status_markers(std::string_view text)
    {
        constexpr std::array<std::string_view, 4> kMarkers{
            "[INFO]",
            "[WARN]",
            "[ERROR]",
            "[FATAL]"
        };

        std::string out;
        out.reserve(text.size() + 8u);

        for (std::size_t i = 0u; i < text.size();)
        {
            bool matched = false;
            for (const std::string_view marker : kMarkers)
            {
                if (marker.size() <= text.size() - i
                    && text.compare(i, marker.size(), marker) == 0)
                {
                    if (!out.empty() && out.back() != '\n' && out.back() != '\r')
                        out.push_back('\n');
                    out.append(marker);
                    i += marker.size();
                    matched = true;
                    break;
                }
            }

            if (!matched)
            {
                out.push_back(text[i]);
                ++i;
            }
        }

        return out;
    }

    [[nodiscard]] inline std::string soft_wrap_status_lines(
        const std::string_view text,
        const std::size_t targetLineLength = 112u)
    {
        if (text.empty())
            return {};

        std::string out;
        out.reserve(text.size() + 16u);

        std::size_t pos = 0u;
        bool firstOutputLine = true;
        while (pos < text.size())
        {
            const std::size_t lineEnd = text.find_first_of("\r\n", pos);
            const std::size_t end = lineEnd == std::string_view::npos ? text.size() : lineEnd;
            std::string_view line = text.substr(pos, end - pos);

            while (line.size() > targetLineLength)
            {
                std::size_t wrap = line.rfind(' ', targetLineLength);
                if (wrap == std::string_view::npos || wrap < targetLineLength / 2u)
                    wrap = line.find(' ', targetLineLength);
                if (wrap == std::string_view::npos)
                    break;

                if (!firstOutputLine)
                    out.push_back('\n');
                out.append(line.substr(0u, wrap));
                firstOutputLine = false;

                line.remove_prefix((std::min)(wrap + 1u, line.size()));
                while (!line.empty() && line.front() == ' ')
                    line.remove_prefix(1u);
            }

            if (!firstOutputLine)
                out.push_back('\n');
            out.append(line);
            firstOutputLine = false;

            if (lineEnd == std::string_view::npos)
                break;

            pos = lineEnd + 1u;
            while (pos < text.size() && (text[pos] == '\n' || text[pos] == '\r'))
                ++pos;
        }

        return out;
    }

    [[nodiscard]] inline std::string trim_status(std::string_view text)
    {
        std::string formatted = soft_wrap_status_lines(format_status_markers(text));
        constexpr std::size_t kMaxModalStatus = 520u;
        if (formatted.size() <= kMaxModalStatus)
            return formatted;
        return formatted.substr(0u, kMaxModalStatus - 3u) + "...";
    }

    [[nodiscard]] inline std::string_view intro_text(const UpdateFlags flags) noexcept
    {
        if (flags.projectSourceDownload)
            return "Project source code is downloading into Epoch's project source cache.";

        if (flags.checkFailed)
            return "Epoch could not complete the update operation. No verified runtime replacement handoff was accepted.";

        if (!flags.installableUpdate && !flags.updateRunning && !flags.restartReady && !flags.sourceWorkerRunning)
            return "Epoch is already current for this packaged runtime. Authorized source build and project-source download remain separate options.";

        return flags.sourceOnlyUpdate
            ? "A newer main source build is available. Epoch will build it locally and keep progress, Cancel, and restart evidence visible."
            : "A newer packaged Epoch runtime is available. Epoch will download, verify, stage, and prepare the replacement.";
    }

    [[nodiscard]] inline std::string_view cache_text(const UpdateFlags flags) noexcept
    {
        if (flags.projectSourceDownload)
            return "This cache lane does not update, rebuild, restart, or replace the running Epoch runtime.";

        if (flags.checkFailed)
            return "The running runtime remains unchanged. The exact failure remains visible above in Debug and Release builds.";

        if (!flags.installableUpdate && !flags.updateRunning && !flags.restartReady && !flags.sourceWorkerRunning)
            return "Smart Update checked packaged releases first; no newer compatible packaged runtime is available.";

        return flags.sourceOnlyUpdate
            ? "Smart Update selected the source rebuild lane for this version, so it does not jump straight to packaged restart."
            : "Cached packages are checked before use; stale or broken downloads are replaced.";
    }

    [[nodiscard]] inline std::string action_text(const UpdateFlags flags)
    {
        if (flags.projectSourceDownload)
            return "Keep Epoch open while the source archive downloads and extracts into the project cache.";

        if (flags.checkFailed)
            return "Retry Check repeats signed package and source-version discovery. Source Options remains available for an explicit authorized source action.";

        if (flags.restartReady)
            return "The update is staged. Press Restart when you are ready to close Epoch and finish the hidden runtime replacement.";

        if (flags.sourceWorkerRunning)
        {
            return flags.sourceCancelAvailable
                ? "Keep Epoch open while the source worker runs. Cancel stops at the next safe checkpoint."
                : "Keep Epoch open while the source worker reaches its first safe checkpoint. Cancel appears after startup is armed.";
        }

        if (flags.sourceOnlyUpdate)
            return "Use Update From Source to build the newer source locally, or Cancel to stay on this build.";

        if (!flags.installableUpdate)
            return "Use Source Options only when you intentionally want a local source build or a source snapshot in the project cache.";

        return "Install Release is the normal binary-first path. Authorized source is the fallback and remains available explicitly.";
    }

    [[nodiscard]] inline std::string_view source_line(std::size_t index) noexcept
    {
        constexpr std::array<std::string_view, 4> kLines{
            "The first authorized source action pairs this device in the browser. Later actions use its protected OS signing key without reopening the browser.",
            "Pair / Re-pair forgets only the nonsecret device registration ID; the next source action performs explicit enrollment.",
            "Build / Update From Source decrypts into a restricted temporary cache, builds locally, then removes the temporary source payload.",
            "Download Source Project extracts the same verified encrypted snapshot into the project source cache without changing the running runtime."
        };
        return index < kLines.size() ? kLines[index] : std::string_view{};
    }

    [[nodiscard]] inline float action_strip_height(
        const std::array<float, 4>& widths,
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
        const bool showCancelButton =
            flags.sourceAuthorizationRunning
                ? true
            : flags.sourceWorkerRunning
                ? flags.sourceCancelAvailable
                : (!flags.updateRunning && !flags.restartReady);
        const bool showPrimaryButton =
            !flags.updateRunning
            && (flags.checkFailed || flags.installableUpdate || flags.restartReady || flags.sourceWorkerRunning);
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
        strip.widths = { 104.0f, 168.0f, 208.0f, 220.0f };
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
        MeasureWrappedText measure_wrapped_text)
    {
        ModalLayout layout{};
        const float desiredWidth = (std::min)(1040.0f, (std::max)(760.0f, viewport.x * 0.70f));
        layout.size.x = fit_modal_size({ viewport.x, viewport.y }, { desiredWidth, 1.0f }, { 720.0f, 1.0f }).x;
        layout.contentWidth = (std::max)(1.0f, layout.size.x - 2.0f * kUpdateContentInset);
        layout.actions = update_action_strip(flags, layout.contentWidth);

        const std::string statusLine = trim_status(status);
        const std::string actionLine = action_text(flags);

        float desiredHeight = 54.0f;
        desiredHeight += measure_wrapped_text(intro_text(flags), layout.contentWidth) + 8.0f;
        desiredHeight += measure_wrapped_text(statusLine, layout.contentWidth) + 10.0f;
        desiredHeight += 36.0f;
        desiredHeight += measure_wrapped_text(cache_text(flags), layout.contentWidth) + 8.0f;
        desiredHeight += measure_wrapped_text(actionLine, layout.contentWidth) + kButtonTopPad;
        desiredHeight += layout.actions.height + kButtonBottomPad;

        const float minHeight = flags.sourceWorkerRunning ? 278.0f : flags.restartReady ? 258.0f : 286.0f;
        layout.size = fit_modal_size({ viewport.x, viewport.y }, { layout.size.x, desiredHeight }, { 720.0f, minHeight });
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
