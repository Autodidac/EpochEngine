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
module; // REQUIRED global module fragment

#include <include/engine.config.hpp> // for EPOCH_USING Macros 		// for EPOCH_USING_SDL
#include <algorithm>
#include <array>
#include <cstdlib>
#include <cmath>
#include <memory>
#include <optional>
#include <source_location>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
export module gui.menu;

// ------------------------------------------------------------
// Engine headers (header units, order-sensitive)
// ------------------------------------------------------------

//import engine.config;

import core.context;
import context.multiplexer;
import engine.cli;
import engine.input;
import engine.gui;
import engine.version;
import context.window;
import core.context;
import context.type;
import core.logger;

// ------------------------------------------------------------
// Standard library
// ------------------------------------------------------------

// ============================================================
// Menu
// ============================================================

namespace gui = epochengine::gui;

export namespace epochengine::menu
{
    inline constexpr std::string_view kLogSys = "Epoch.Menu";

    [[nodiscard]] inline int live_layout_width(const std::shared_ptr<core::Context>& ctx) noexcept
    {
        if (!ctx)
            return 0;
        if (ctx->width > 0)
            return ctx->width;
        return ctx->get_width_safe();
    }

    [[nodiscard]] inline int live_layout_height(const std::shared_ptr<core::Context>& ctx) noexcept
    {
        if (!ctx)
            return 0;
        if (ctx->height > 0)
            return ctx->height;
        return ctx->get_height_safe();
    }

    enum class Choice {
        CheckUpdates,
        UpdateLatest,
        UpdatePanelCancel,
        UpdatePanelDismiss,
        UpdatePanelRestart,
        OpenEditor,
        OpenForestFactory,
        OpenGuiEditor,
        Snake, Tetris, Pacman, Frogger, Sokoban,
        Minesweep, Puzzle, Bejeweled, Fourty,
        Sandsim, Cellular, Settings, About, Exit
    };

    enum class EditorCommandChoice {
        OpenProject,
        Settings,
        RunGame,
        Exit
    };

    struct ChoiceDescriptor {
        Choice      choice;
        std::string_view label;
        gui::Vec2   size;
    };

    struct LauncherUpdatePanelState
    {
        bool active = false;
        bool action_enabled = true;
        bool action_visible = true;
        bool action_accepts_enter = true;
        bool restart_ready = false;
        bool cancel_available = false;
        bool show_percent = true;
        bool progress_active = false;
        float progress = 0.0f;
        float activity_phase = 0.0f;
        std::string title{ "Update Epoch" };
        std::string status{};
        std::string progress_label{ "Update" };
        std::string progress_status{ "waiting" };
        std::string action_label{ "Update Epoch" };
        Choice action_choice = Choice::UpdateLatest;
    };

    struct EditorCommandDescriptor {
        EditorCommandChoice choice;
        std::string_view label;
        gui::Vec2 size;
    };

    struct MenuOverlay
    {
        std::vector<ChoiceDescriptor> descriptors;
        std::size_t selection = 0;

        bool prevUp = false, prevDown = false, prevLeft = false,
            prevRight = false, prevEnter = false;
        bool initialized = false;
        bool initializationLogEmitted = false;
        bool autoCommandConsumed = false;

        std::vector<std::pair<int, int>> cachedPositions;
        std::vector<float> colWidths, rowHeights;

        int cachedWidth = -1;
        int cachedHeight = -1;
        int columns = 1;
        int rows = 0;
        std::size_t inputGuardFrames = 0;

        static constexpr int ExpectedColumns = 4;
        static constexpr float LayoutSpacing = 24.f;

        int maxColumns = ExpectedColumns;

        float layoutOriginX = 0.0f;
        float layoutOriginY = 0.0f;
        float layoutWidth = 0.0f;
        float layoutHeight = 0.0f;
        std::string statusLine{};
        LauncherUpdatePanelState updatePanel{};
        std::vector<core::ContextType> launchContextOptions{};
        core::ContextType selectedLaunchContext{ core::ContextType::None };

        static constexpr std::array kLauncherChoices = {
            ChoiceDescriptor{ Choice::OpenEditor, "Open Editor", { 220.0f, 68.0f } },
            ChoiceDescriptor{ Choice::OpenForestFactory, "Forest Factory", { 220.0f, 68.0f } },
            ChoiceDescriptor{ Choice::OpenGuiEditor, "GUI Editor", { 220.0f, 68.0f } },
            ChoiceDescriptor{ Choice::UpdateLatest, "Update Epoch Engine", { 220.0f, 68.0f } },
            ChoiceDescriptor{ Choice::Exit, "Quit", { 220.0f, 68.0f } }
        };

        static constexpr std::array kUpdaterShellChoices = {
            ChoiceDescriptor{ Choice::UpdateLatest, "Update Epoch Now", { 520.0f, 168.0f } }
        };

        static constexpr std::string_view updater_shell_description() noexcept
        {
            return "Checks GitHub once, then builds current main source locally when that source is newer. Packaged releases are used only when no newer source lane is available.";
        }

        [[nodiscard]] static bool updater_shell_automatic_check_requested() noexcept
        {
            std::string value;

#if defined(_WIN32)
            char* raw = nullptr;
            std::size_t raw_size = 0;
            if (_dupenv_s(&raw, &raw_size, "EPOCH_UPDATER_SHELL_AUTO_COMMAND") != 0 || raw == nullptr)
                return false;

            value.assign(raw, raw_size > 0 ? raw_size - 1 : 0);
            free(raw);
#else
            if (const char* const raw = std::getenv("EPOCH_UPDATER_SHELL_AUTO_COMMAND"))
                value = raw;
            else
                return false;
#endif

            const bool requested = value == "smart-update";
#if defined(_WIN32)
            (void)_putenv_s("EPOCH_UPDATER_SHELL_AUTO_COMMAND", "");
#else
            (void)::unsetenv("EPOCH_UPDATER_SHELL_AUTO_COMMAND");
#endif
            return requested;
        }

        static constexpr std::string_view launcher_title() noexcept
        {
            return "Project Launcher";
        }

        static constexpr std::string_view launcher_hint() noexcept
        {
            return "Choose an editor workspace and live render context. Updates and exit remain separate launcher actions.";
        }

        [[nodiscard]] static constexpr std::string_view context_label(
            core::ContextType type) noexcept
        {
            switch (type)
            {
            case core::ContextType::DirectX: return "DirectX";
            case core::ContextType::OpenGL: return "OpenGL";
            case core::ContextType::SDL: return "SDL";
            case core::ContextType::SFML: return "SFML";
            case core::ContextType::RayLib: return "Raylib";
            case core::ContextType::Vulkan: return "Vulkan";
            case core::ContextType::Software: return "Software";
            default: return "Unavailable";
            }
        }

        void refresh_launcher_descriptors()
        {
            descriptors.clear();
            descriptors.reserve(kLauncherChoices.size());
            for (const auto& item : kLauncherChoices)
                descriptors.push_back(item);

            cachedWidth = -1;
            cachedHeight = -1;
        }

        void set_launch_context_options(
            std::span<const core::ContextType> options,
            core::ContextType activeContext)
        {
            std::vector<core::ContextType> resolved;
            resolved.reserve(options.size());
            for (const auto type : options)
            {
                if (type == core::ContextType::None
                    || std::find(resolved.begin(), resolved.end(), type) != resolved.end())
                {
                    continue;
                }
                resolved.push_back(type);
            }

            if (resolved.empty() && activeContext != core::ContextType::None)
                resolved.push_back(activeContext);

            const bool selectionStillAvailable =
                std::find(resolved.begin(), resolved.end(), selectedLaunchContext) != resolved.end();
            launchContextOptions = std::move(resolved);
            if (selectionStillAvailable)
                return;

            const auto active = std::find(
                launchContextOptions.begin(),
                launchContextOptions.end(),
                activeContext);
            selectedLaunchContext = active != launchContextOptions.end()
                ? *active
                : (launchContextOptions.empty()
                    ? core::ContextType::None
                    : launchContextOptions.front());
        }

        [[nodiscard]] core::ContextType selected_launch_context() const noexcept
        {
            return selectedLaunchContext;
        }

        void set_status(std::string status)
        {
            statusLine = std::move(status);
        }

        [[nodiscard]] const std::string& status() const noexcept
        {
            return statusLine;
        }

        void set_update_panel_state(LauncherUpdatePanelState state)
        {
            const bool layoutModeChanged = updatePanel.active != state.active
                || updatePanel.restart_ready != state.restart_ready
                || updatePanel.cancel_available != state.cancel_available
                || updatePanel.action_visible != state.action_visible
                || updatePanel.action_accepts_enter != state.action_accepts_enter
                || updatePanel.action_label != state.action_label;
            updatePanel = std::move(state);
            if (layoutModeChanged)
                inputGuardFrames = (std::max)(inputGuardFrames, std::size_t{ 2u });
        }

        void guard_next_input_frames(const std::size_t frameCount = 2u) noexcept
        {
            inputGuardFrames = (std::max)(inputGuardFrames, frameCount);
        }

        // ----------------------------------------------------
        void set_max_columns(int desiredMax)
        {
            const int clamped = std::clamp(desiredMax, 1, ExpectedColumns);
            if (maxColumns != clamped) {
                maxColumns = clamped;
                cachedWidth = -1;
                cachedHeight = -1;
            }
        }

        // ----------------------------------------------------
        void recompute_layout(
            std::shared_ptr<epochengine::core::Context> ctx,
            int widthPixels,
            int heightPixels)
        {
            const int totalItems = static_cast<int>(descriptors.size());
            if (totalItems == 0) {
                cachedPositions.clear();
                colWidths.clear();
                rowHeights.clear();
                columns = 1;
                rows = 0;
                layoutOriginX = layoutOriginY = 0.0f;
                layoutWidth = layoutHeight = 0.0f;
                return;
            }

            int resolvedWidth = widthPixels;
            int resolvedHeight = heightPixels;

            if (resolvedWidth <= 0 && ctx)  resolvedWidth = live_layout_width(ctx);
            if (resolvedHeight <= 0 && ctx) resolvedHeight = live_layout_height(ctx);

            cachedWidth = (std::max)(1, resolvedWidth);
            cachedHeight = (std::max)(1, resolvedHeight);

            float maxItemWidth = 0.f;
            float maxItemHeight = 0.f;
            for (const auto& d : descriptors) {
                maxItemWidth = (std::max)(maxItemWidth, d.size.x);
                maxItemHeight = (std::max)(maxItemHeight, d.size.y);
            }

            int computedCols = totalItems;
            if (maxItemWidth > 0.f) {
                const float available = static_cast<float>(cachedWidth);
                const float denom = maxItemWidth + LayoutSpacing;
                if (denom > 0.f) {
                    computedCols = static_cast<int>(
                        std::floor((available + LayoutSpacing) / denom));
                    computedCols = std::clamp(computedCols, 1, totalItems);
                }
            }

            const int maxAllowed = (std::max)(1, (std::min)(totalItems, maxColumns));
            columns = (std::max)(1, (std::min)(computedCols, maxAllowed));
            rows = (totalItems + columns - 1) / columns;

            colWidths.assign(columns, 0.f);
            rowHeights.assign(rows, 0.f);

            for (int i = 0; i < totalItems; ++i) {
                const int r = i / columns;
                const int c = i % columns;
                colWidths[c] = (std::max)(colWidths[c], descriptors[i].size.x);
                rowHeights[r] = (std::max)(rowHeights[r], descriptors[i].size.y);
            }

            float totalWidth = LayoutSpacing * (columns - 1);
            float totalHeight = LayoutSpacing * (rows - 1);
            for (float w : colWidths)  totalWidth += w;
            for (float h : rowHeights) totalHeight += h;

            layoutOriginX = (std::max)(0.f, (cachedWidth - totalWidth) * 0.5f);
            const float verticalBias = core::cli::updater_shell_requested ? 0.5f : 0.38f;
            layoutOriginY = (std::max)(0.f, (cachedHeight - totalHeight) * verticalBias);
            layoutWidth = totalWidth;
            layoutHeight = totalHeight;

            cachedPositions.resize(totalItems);

            float y = layoutOriginY;
            for (int r = 0; r < rows; ++r) {
                float x = layoutOriginX;
                for (int c = 0; c < columns; ++c) {
                    const int idx = r * columns + c;
                    if (idx < totalItems) {
                        cachedPositions[idx] = {
                            static_cast<int>(std::round(x)),
                            static_cast<int>(std::round(y))
                        };
                    }
                    x += colWidths[c] + LayoutSpacing;
                }
                y += rowHeights[r] + LayoutSpacing;
            }
        }

        // ----------------------------------------------------
        void initialize(std::shared_ptr<core::Context> ctx)
        {
            if (initialized) return;

            set_max_columns(
                core::cli::updater_shell_requested
                    ? 1
                    : (std::min)(3, core::cli::menu_columns));
            autoCommandConsumed = false;
            guard_next_input_frames();

            if (core::cli::updater_shell_requested)
            {
                descriptors.clear();
                descriptors.reserve(kUpdaterShellChoices.size());
                for (const auto& item : kUpdaterShellChoices)
                    descriptors.push_back(item);

                selection = 0;
                prevUp = prevDown = prevLeft = prevRight = prevEnter = false;

                const int w = ctx ? live_layout_width(ctx) : cachedWidth;
                const int h = ctx ? live_layout_height(ctx) : cachedHeight;
                recompute_layout(ctx, w, h);

                if (!initializationLogEmitted)
                {
                    logger::get(kLogSys.data()).log(
                        logger::LogLevel::INFO,
                        "Initialized updater shell with "
                            + std::to_string(static_cast<unsigned long long>(descriptors.size()))
                            + " entries at "
                            + std::to_string(w)
                            + "x"
                            + std::to_string(h)
                            + ".",
                        std::source_location::current());
                    initializationLogEmitted = true;
                }

                initialized = true;
                return;
            }

            selection = 0;
            prevUp = prevDown = prevLeft = prevRight = prevEnter = false;
            refresh_launcher_descriptors();

            const int w = ctx ? live_layout_width(ctx) : cachedWidth;
            const int h = ctx ? live_layout_height(ctx) : cachedHeight;
            recompute_layout(ctx, w, h);

            if (!initializationLogEmitted)
            {
                logger::get(kLogSys.data()).log(
                    logger::LogLevel::INFO,
                    "Initialized project launcher with "
                        + std::to_string(static_cast<unsigned long long>(descriptors.size()))
                        + " entries, "
                        + std::to_string(columns)
                        + " column(s), at "
                        + std::to_string(w)
                        + "x"
                        + std::to_string(h)
                        + ".",
                    std::source_location::current());
                initializationLogEmitted = true;
            }

            initialized = true;
        }

        // ----------------------------------------------------
        std::optional<Choice> update_and_draw(
            std::shared_ptr<core::Context> ctx,
            core::WindowData* win,
            float dt,
            bool upPressed,
            bool downPressed,
            bool leftPressed,
            bool rightPressed,
            bool enterPressed)
        {
            return update_and_draw_in_window(
                ctx,
                win,
                dt,
                upPressed,
                downPressed,
                leftPressed,
                rightPressed,
                enterPressed,
                "",
                { 0.f, 0.f },
                { 0.f, 0.f },
                false);
        }

        std::optional<Choice> update_and_draw_in_window(
            std::shared_ptr<core::Context> ctx,
            core::WindowData* win,
            float dt,
            bool upPressed,
            bool downPressed,
            bool leftPressed,
            bool rightPressed,
            bool enterPressed,
            std::string_view title,
            gui::Vec2 windowPosition,
            gui::Vec2 windowSize,
            bool clampToWindow)
        {
            if (!initialized) return std::nullopt;
            if (core::cli::updater_shell_requested && !updatePanel.active)
                return update_and_draw_updater_shell(
                    ctx,
                    win,
                    dt,
                    upPressed,
                    downPressed,
                    leftPressed,
                    rightPressed,
                    enterPressed,
                    title,
                    windowPosition,
                    windowSize,
                    clampToWindow);

            constexpr float kHeaderOffsetY = 246.0f;

            std::ignore = win;
            std::ignore = dt;

            int currentWidth = windowSize.x > 0 ? static_cast<int>(windowSize.x) : (ctx ? live_layout_width(ctx) : cachedWidth);
            int currentHeight = windowSize.y > 0 ? static_cast<int>(windowSize.y) : (ctx ? live_layout_height(ctx) : cachedHeight);
            if (currentWidth <= 0) currentWidth = cachedWidth;
            if (currentHeight <= 0) currentHeight = cachedHeight;
            if (currentWidth <= 0) currentWidth = 1;
            if (currentHeight <= 0) currentHeight = 1;

            const int launcherContentHeight =
                (std::max)(1, currentHeight - static_cast<int>(kHeaderOffsetY));
            if (currentWidth != cachedWidth || launcherContentHeight != cachedHeight)
                recompute_layout(ctx, currentWidth, launcherContentHeight);

            const bool inputGuarded = inputGuardFrames > 0u;
            if (inputGuardFrames > 0u)
                --inputGuardFrames;

            int mx = 0, my = 0;
            ctx->get_mouse_position_safe(mx, my);

            const int totalItems = int(descriptors.size());
            if (totalItems == 0 || cachedPositions.size() != size_t(totalItems))
                return std::nullopt;

            if (selection >= size_t(totalItems))
                selection = size_t(totalItems - 1);

            const auto move_vertical = [&](int delta)
            {
                if (totalItems <= 0 || columns <= 0 || rows <= 0)
                    return;

                const int current = static_cast<int>(selection);
                const int currentColumn = current % columns;
                int nextRow = (current / columns) + delta;
                if (nextRow < 0)
                    nextRow = rows - 1;
                else if (nextRow >= rows)
                    nextRow = 0;

                int candidate = nextRow * columns + currentColumn;
                while (candidate >= totalItems && nextRow > 0)
                {
                    --nextRow;
                    candidate = nextRow * columns + currentColumn;
                }

                selection = static_cast<std::size_t>(
                    std::clamp(candidate, 0, totalItems - 1));
            };

           // const bool flipVertical = ctx && ctx ->type == core::ContextType::OpenGL;

            const float pad = LayoutSpacing * 0.5f;
            const gui::Vec2 chromePosition{
                windowPosition.x + layoutOriginX - pad,
                windowPosition.y + layoutOriginY - pad - kHeaderOffsetY * 0.5f
            };
            const gui::Vec2 chromeSize{
                (std::max)(layoutWidth + pad * 2, 432.0f),
                layoutHeight + pad * 2 + kHeaderOffsetY
            };

            const gui::Vec2 framePosition = windowPosition;
            const gui::Vec2 frameSize = (clampToWindow && windowSize.x > 0.f && windowSize.y > 0.f)
                ? windowSize
                : gui::Vec2{
                    static_cast<float>(currentWidth),
                    static_cast<float>(currentHeight)
                };

            auto position_for_index = [&](int idx) {
                auto base = cachedPositions[idx];
                return std::pair<int, int>{
                    base.first + static_cast<int>(std::round(framePosition.x)),
                    base.second + static_cast<int>(std::round(framePosition.y + kHeaderOffsetY))
                };
            };

            if (updatePanel.active)
            {
                const gui::Vec2 framePosition = windowPosition;
                const gui::Vec2 frameSize = (clampToWindow && windowSize.x > 0.f && windowSize.y > 0.f)
                    ? windowSize
                    : gui::Vec2{
                        static_cast<float>(currentWidth),
                        static_cast<float>(currentHeight)
                    };

                gui::push_theme(gui::ThemeVariant::ClassicLauncher);
                gui::begin_window(title, framePosition, frameSize);

                std::string message = "Version: ";
                message += epochengine::GetEngineDisplayString();
                message += "\n";
                message += updatePanel.status.empty()
                    ? std::string{ "Checking update status." }
                    : updatePanel.status;

                const auto loading = gui::loading_screen(gui::LoadingScreenOptions{
                    .title = updatePanel.title,
                    .message = message,
                    .progress_label = updatePanel.progress_label,
                    .progress_status = updatePanel.progress_status,
                    .progress = std::clamp(updatePanel.progress, 0.0f, 1.0f),
                    .viewport_position = framePosition,
                    .viewport_size = frameSize,
                    .panel_size = { (std::max)(460.0f, (std::min)(frameSize.x - 96.0f, 760.0f)), 340.0f },
                    .dim_background = false,
                    .capture_input = true,
                    .show_percent = updatePanel.show_percent,
                    .activity = updatePanel.progress_active,
                    .activity_phase = updatePanel.activity_phase,
                    .reserve_action_row = updatePanel.action_visible
                });

                bool clicked = false;
                if (updatePanel.action_visible
                    && loading.action.size.x > 0.0f
                    && loading.action.size.y > 0.0f)
                {
                    gui::set_cursor(loading.action.position);
                    clicked = gui::button(updatePanel.action_label, loading.action.size);
                }

                gui::end_window();
                gui::pop_theme();

                std::optional<Choice> chosen{};
                if (!inputGuarded && updatePanel.action_visible && updatePanel.action_enabled && clicked)
                    chosen = updatePanel.action_choice;
                else if (!inputGuarded
                    && updatePanel.action_visible
                    && updatePanel.action_enabled
                    && updatePanel.action_accepts_enter
                    && enterPressed
                    && !prevEnter)
                {
                    chosen = updatePanel.action_choice;
                }

                prevEnter = enterPressed;
                return chosen;
            }

            int hover = -1;
            for (int i = 0; i < totalItems; ++i) {
                const auto& d = descriptors[i];
                const auto pos = position_for_index(i);
                if (mx >= pos.first && mx <= pos.first + int(d.size.x) &&
                    my >= pos.second && my <= pos.second + int(d.size.y)) {
                    hover = i;
                    break;
                }
            }

            if (upPressed && !prevUp) move_vertical(-1);
            if (downPressed && !prevDown) move_vertical(+1);
            if (leftPressed && !prevLeft) selection = (selection == 0) ? totalItems - 1 : selection - 1;
            if (rightPressed && !prevRight) selection = (selection + 1) % totalItems;
            if (!inputGuarded && !upPressed && !downPressed && !leftPressed && !rightPressed && hover >= 0)
                selection = hover;

            prevUp = upPressed; prevDown = downPressed;
            prevLeft = leftPressed; prevRight = rightPressed;

            gui::push_theme(gui::ThemeVariant::ClassicLauncher);
            gui::begin_window(title, framePosition, frameSize);

            gui::set_cursor({ framePosition.x + 16.0f, framePosition.y + 52.0f });
            gui::label(std::string("Version: ") + epochengine::GetEngineDisplayString());
            gui::set_cursor({ framePosition.x + 16.0f, framePosition.y + 72.0f });
            gui::label(std::string("Launcher: ") + std::string(launcher_title()));
            gui::set_cursor({ framePosition.x + 16.0f, framePosition.y + 92.0f });
            gui::wrapped_label(launcher_hint(), frameSize.x - 32.0f);
            if (!statusLine.empty())
            {
                gui::set_cursor({ framePosition.x + 16.0f, framePosition.y + 122.0f });
                gui::wrapped_label(statusLine, frameSize.x - 32.0f);
            }

            gui::set_cursor({ framePosition.x + 16.0f, framePosition.y + 154.0f });
            gui::label("Launch Settings");
            gui::set_cursor({ framePosition.x + 16.0f, framePosition.y + 176.0f });
            gui::property_row("Context", context_label(selectedLaunchContext), 84.0f);

            if (!launchContextOptions.empty())
            {
                std::vector<gui::SegmentedButtonSpec> contextButtons;
                contextButtons.reserve(launchContextOptions.size());
                const float availableWidth = (std::max)(220.0f, frameSize.x - 32.0f);
                const float gap = 4.0f;
                const float buttonWidth = std::clamp(
                    (availableWidth - gap * static_cast<float>(launchContextOptions.size() - 1))
                        / static_cast<float>(launchContextOptions.size()),
                    76.0f,
                    112.0f);
                for (const auto type : launchContextOptions)
                {
                    contextButtons.push_back(gui::SegmentedButtonSpec{
                        context_label(type),
                        buttonWidth,
                        type == selectedLaunchContext
                    });
                }

                gui::set_cursor({ framePosition.x + 16.0f, framePosition.y + 202.0f });
                if (const auto selected = gui::segmented_button_row(contextButtons, 30.0f, gap);
                    selected && *selected < launchContextOptions.size())
                {
                    selectedLaunchContext = launchContextOptions[*selected];
                }
            }

            std::optional<Choice> chosen{};
            for (int i = 0; i < totalItems; ++i) {
                const auto pos = position_for_index(i);
                gui::set_cursor({ float(pos.first), float(pos.second) });

                const std::string label{ descriptors[i].label };

                if (gui::button(label, descriptors[i].size) && !inputGuarded) {
                    selection = size_t(i);
                    chosen = descriptors[i].choice;
                }
            }

            gui::end_window();
            gui::pop_theme();

            if (chosen) return chosen;
            if (!inputGuarded && enterPressed && !prevEnter)
                return descriptors[selection].choice;

            prevEnter = enterPressed;
            return std::nullopt;
        }

        std::optional<Choice> update_and_draw_updater_shell(
            std::shared_ptr<core::Context> ctx,
            core::WindowData* win,
            float dt,
            bool upPressed,
            bool downPressed,
            bool leftPressed,
            bool rightPressed,
            bool enterPressed,
            std::string_view title,
            gui::Vec2 windowPosition,
            gui::Vec2 windowSize,
            bool clampToWindow)
        {
            std::ignore = win;
            std::ignore = dt;
            std::ignore = upPressed;
            std::ignore = downPressed;
            std::ignore = leftPressed;
            std::ignore = rightPressed;
            std::ignore = title;

            int currentWidth = windowSize.x > 0 ? static_cast<int>(windowSize.x) : (ctx ? live_layout_width(ctx) : cachedWidth);
            int currentHeight = windowSize.y > 0 ? static_cast<int>(windowSize.y) : (ctx ? live_layout_height(ctx) : cachedHeight);
            if (currentWidth <= 0) currentWidth = cachedWidth;
            if (currentHeight <= 0) currentHeight = cachedHeight;
            if (currentWidth <= 0) currentWidth = 1;
            if (currentHeight <= 0) currentHeight = 1;

            const gui::Vec2 framePosition = windowPosition;
            const gui::Vec2 frameSize = (clampToWindow && windowSize.x > 0.f && windowSize.y > 0.f)
                ? windowSize
                : gui::Vec2{
                    static_cast<float>(currentWidth),
                    static_cast<float>(currentHeight)
                };

            gui::push_theme(gui::ThemeVariant::ClassicLauncher);
            gui::begin_window("", framePosition, frameSize);

            const float contentWidth = (std::max)(380.0f, (std::min)(frameSize.x - 240.0f, 560.0f));
            const float contentX = framePosition.x + (frameSize.x - contentWidth) * 0.5f;
            const float textInset = 8.0f;
            const float textWidth = (std::max)(240.0f, contentWidth - textInset * 2.0f);
            const float buttonWidth = contentWidth;
            const float buttonHeight = (std::max)(120.0f, (std::min)(160.0f, frameSize.y * 0.24f));
            const float lineHeight = gui::line_height();

            gui::set_cursor({ contentX, framePosition.y + 48.0f });
            const float descriptionHeight = gui::wrapped_text_height(updater_shell_description(), textWidth);
            const float statusHeight = statusLine.empty()
                ? 0.0f
                : gui::wrapped_text_height(statusLine, textWidth);
            const float statusGap = statusLine.empty() ? 0.0f : 18.0f;
            const float stackHeight =
                lineHeight +
                18.0f +
                lineHeight +
                26.0f +
                descriptionHeight +
                statusGap +
                statusHeight +
                32.0f +
                buttonHeight;
            const float contentY = framePosition.y + (std::max)(32.0f, (frameSize.y - stackHeight) * 0.5f);

            gui::set_cursor({ contentX, contentY });
            gui::label("Epoch Updater Shell");

            gui::set_cursor({ contentX, contentY + lineHeight + 18.0f });
            gui::label(std::string("Version: ") + epochengine::GetEngineDisplayString());

            gui::set_cursor({ contentX + textInset, contentY + lineHeight * 2.0f + 44.0f });
            gui::wrapped_label(updater_shell_description(), textWidth);

            float buttonY = contentY + lineHeight * 2.0f + 76.0f + descriptionHeight;
            if (!statusLine.empty())
            {
                gui::set_cursor({ contentX + textInset, buttonY + statusGap });
                gui::wrapped_label(statusLine, textWidth);
                buttonY += statusGap + statusHeight;
            }

            gui::set_cursor({ contentX, buttonY });
            const bool clicked = gui::button("Update To Current Epoch", { buttonWidth, buttonHeight });

            gui::end_window();
            gui::pop_theme();

            std::optional<Choice> chosen{};
            if (clicked)
                chosen = Choice::UpdateLatest;
            else if (updater_shell_automatic_check_requested() && !autoCommandConsumed)
            {
                autoCommandConsumed = true;
                chosen = Choice::CheckUpdates;
            }
            else if (core::cli::smoke_requested && !autoCommandConsumed)
            {
                autoCommandConsumed = true;
                chosen = Choice::UpdateLatest;
            }
            else if (enterPressed && !prevEnter)
            {
                chosen = Choice::UpdateLatest;
            }

            prevEnter = enterPressed;
            return chosen;
        }

        // ----------------------------------------------------
        void cleanup()
        {
            descriptors.clear();
            cachedPositions.clear();
            colWidths.clear();
            rowHeights.clear();
            cachedWidth = cachedHeight = -1;
            columns = 1;
            rows = 0;
            layoutOriginX = layoutOriginY = 0.f;
            layoutWidth = layoutHeight = 0.f;
            selection = 0;
            prevUp = prevDown = prevLeft = prevRight = prevEnter = false;
            autoCommandConsumed = false;
            inputGuardFrames = 0;
            updatePanel = {};
            initialized = false;
        }
    };

    struct EditorCommandOverlay
    {
        std::vector<EditorCommandDescriptor> descriptors;
        std::size_t selection = 0;

        bool prevUp = false;
        bool prevDown = false;
        bool prevEnter = false;
        bool initialized = false;

        gui::Vec2 windowPosition{};
        float windowPadding = 16.0f;
        float itemSpacing = 12.0f;

        void initialize()
        {
            if (initialized) return;

            constexpr gui::Vec2 DefaultButtonSize{ 220.0f, 48.0f };

            descriptors = {
                { EditorCommandChoice::OpenProject, "Open Project", DefaultButtonSize },
                { EditorCommandChoice::Settings,    "Settings",     DefaultButtonSize },
                { EditorCommandChoice::RunGame,     "Run Game",     DefaultButtonSize },
                { EditorCommandChoice::Exit,        "Exit",         DefaultButtonSize }
            };

            selection = 0;
            prevUp = prevDown = prevEnter = false;
            initialized = true;
        }

        std::optional<EditorCommandChoice> update_and_draw(
            std::shared_ptr<core::Context> ctx,
            core::WindowData* win,
            float dt,
            bool upPressed,
            bool downPressed,
            bool enterPressed,
            bool allowMouse = true,
            std::optional<gui::WidgetBounds> avoidBounds = std::nullopt)
        {
            if (!initialized) return std::nullopt;

            std::ignore = win;
            std::ignore = dt;

            const int totalItems = static_cast<int>(descriptors.size());
            if (totalItems == 0) return std::nullopt;

            if (selection >= static_cast<std::size_t>(totalItems))
                selection = static_cast<std::size_t>(totalItems - 1);

            if (upPressed && !prevUp)
                selection = (selection == 0) ? totalItems - 1 : selection - 1;
            if (downPressed && !prevDown)
                selection = (selection + 1) % totalItems;

            prevUp = upPressed;
            prevDown = downPressed;

            float maxWidth = 0.f;
            float totalHeight = 0.f;
            for (const auto& d : descriptors)
            {
                maxWidth = (std::max)(maxWidth, d.size.x);
                totalHeight += d.size.y;
            }
            totalHeight += itemSpacing * (totalItems - 1);

            const gui::Vec2 windowSize{
                maxWidth + windowPadding * 2,
                totalHeight + windowPadding * 2
            };

            const float viewportWidth = ctx ? static_cast<float>(live_layout_width(ctx)) : 0.0f;
            const float viewportHeight = ctx ? static_cast<float>(live_layout_height(ctx)) : 0.0f;
            constexpr float margin = 24.0f;

            if (viewportWidth > 0.0f && viewportHeight > 0.0f)
            {
                windowPosition = {
                    viewportWidth - windowSize.x - margin,
                    margin
                };

                auto clamp_to_viewport = [&](gui::Vec2 pos)
                    {
                        pos.x = std::clamp(pos.x, 0.0f, (std::max)(0.0f, viewportWidth - windowSize.x));
                        pos.y = std::clamp(pos.y, 0.0f, (std::max)(0.0f, viewportHeight - windowSize.y));
                        return pos;
                    };

                auto intersects = [](const gui::WidgetBounds& a, const gui::WidgetBounds& b)
                    {
                        return !(a.position.x + a.size.x <= b.position.x ||
                            b.position.x + b.size.x <= a.position.x ||
                            a.position.y + a.size.y <= b.position.y ||
                            b.position.y + b.size.y <= a.position.y);
                    };

                windowPosition = clamp_to_viewport(windowPosition);

                if (avoidBounds)
                {
                    gui::WidgetBounds current{ windowPosition, windowSize };
                    if (intersects(current, *avoidBounds))
                    {
                        gui::Vec2 candidate{ windowPosition.x, avoidBounds->position.y + avoidBounds->size.y + margin };
                        if (candidate.y + windowSize.y > viewportHeight)
                            candidate.y = avoidBounds->position.y - windowSize.y - margin;

                        if (candidate.y < 0.0f || candidate.y + windowSize.y > viewportHeight)
                        {
                            candidate = {
                                avoidBounds->position.x - windowSize.x - margin,
                                windowPosition.y
                            };
                        }

                        windowPosition = clamp_to_viewport(candidate);
                    }
                }
            }

            gui::begin_top_layer();
            gui::begin_window(
                "Editor Commands",
                windowPosition,
                windowSize
            );

            int mx = 0;
            int my = 0;
            if (ctx)
                ctx->get_mouse_position_safe(mx, my);

            std::optional<EditorCommandChoice> chosen{};
            float y = windowPosition.y + windowPadding;
            for (int i = 0; i < totalItems; ++i)
            {
                const auto& d = descriptors[i];
                const gui::Vec2 pos{ windowPosition.x + windowPadding, y };
                gui::set_cursor(pos);

                if (allowMouse)
                {
                    const bool hovering =
                        mx >= static_cast<int>(pos.x) &&
                        mx <= static_cast<int>(pos.x + d.size.x) &&
                        my >= static_cast<int>(pos.y) &&
                        my <= static_cast<int>(pos.y + d.size.y);
                    if (hovering)
                        selection = static_cast<std::size_t>(i);
                }

                const std::string label{ d.label };

                if (gui::button(label, d.size))
                {
                    selection = static_cast<std::size_t>(i);
                    chosen = d.choice;
                }

                y += d.size.y + itemSpacing;
            }

            gui::end_window();
            gui::end_top_layer();

            if (chosen) return chosen;
            if (enterPressed && !prevEnter)
                return descriptors[selection].choice;

            prevEnter = enterPressed;
            return std::nullopt;
        }

        void reset_selection()
        {
            selection = 0;
            prevUp = prevDown = prevEnter = false;
        }
    };
}
