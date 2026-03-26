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

#include <include/aengine.config.hpp> // for EPOCH_USING Macros 		// for EPOCH_USING_SDL
#include <algorithm>
#include <array>
#include <cstdlib>
#include <cmath>
#include <memory>
#include <source_location>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
export module aengine.gui.menu;

// ------------------------------------------------------------
// Engine headers (header units, order-sensitive)
// ------------------------------------------------------------

//import aengine.config;

import aengine.core.context;
import aengine.context.multiplexer;
import aengine.cli;
import aengine.input;
import aengine.gui;
import aengine.version;
import aengine.context.window;
import aengine.core.context;
import aengine.context.type;
import aengine.core.logger;

// ------------------------------------------------------------
// Standard library
// ------------------------------------------------------------

// ============================================================
// Menu
// ============================================================

namespace gui = epochnamespace::gui;

export namespace epochnamespace::menu
{
    inline constexpr std::string_view kLogSys = "Epoch.Menu";

    enum class Choice {
        UpdateLatest,
        OpenEditor,
        ProjectSandbox,
        ProjectPlatformer,
        ProjectPuzzle,
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

    struct EditorCommandDescriptor {
        EditorCommandChoice choice;
        std::string_view label;
        gui::Vec2 size;
    };

    struct MenuOverlay
    {
        enum class LauncherPanel : unsigned char
        {
            Projects = 0,
            Games,
            Tools
        };

        std::vector<ChoiceDescriptor> descriptors;
        std::size_t selection = 0;
        LauncherPanel activePanel = LauncherPanel::Projects;
        std::size_t projectSelection = 0;
        std::size_t gameSelection = 0;
        std::size_t toolSelection = 0;

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

        static constexpr int ExpectedColumns = 4;
        static constexpr float LayoutSpacing = 32.f;

        int maxColumns = ExpectedColumns;

        float layoutOriginX = 0.0f;
        float layoutOriginY = 0.0f;
        float layoutWidth = 0.0f;
        float layoutHeight = 0.0f;

        static constexpr std::array kProjectChoices = {
            ChoiceDescriptor{ Choice::ProjectSandbox, "Sandbox Project", { 256.0f, 96.0f } },
            ChoiceDescriptor{ Choice::ProjectPlatformer, "Platformer Demo", { 256.0f, 96.0f } },
            ChoiceDescriptor{ Choice::ProjectPuzzle, "Puzzle Lab", { 256.0f, 96.0f } },
            ChoiceDescriptor{ Choice::OpenEditor, "Open Editor", { 256.0f, 96.0f } }
        };

        static constexpr std::array kGameChoices = {
            ChoiceDescriptor{ Choice::Snake, "Snake", { 256.0f, 96.0f } },
            ChoiceDescriptor{ Choice::Tetris, "Tetris", { 256.0f, 96.0f } },
            ChoiceDescriptor{ Choice::Pacman, "Pacman", { 256.0f, 96.0f } },
            ChoiceDescriptor{ Choice::Frogger, "Frogger", { 256.0f, 96.0f } },
            ChoiceDescriptor{ Choice::Sokoban, "Sokoban", { 256.0f, 96.0f } },
            ChoiceDescriptor{ Choice::Minesweep, "Minesweeper", { 256.0f, 96.0f } },
            ChoiceDescriptor{ Choice::Puzzle, "Sliding Puzzle", { 256.0f, 96.0f } },
            ChoiceDescriptor{ Choice::Bejeweled, "Bejeweled", { 256.0f, 96.0f } },
            ChoiceDescriptor{ Choice::Fourty, "2048", { 256.0f, 96.0f } },
            ChoiceDescriptor{ Choice::Sandsim, "Sand Sim", { 256.0f, 96.0f } },
            ChoiceDescriptor{ Choice::Cellular, "Cellular", { 256.0f, 96.0f } }
        };

        static constexpr std::array kToolChoices = {
            ChoiceDescriptor{ Choice::OpenEditor, "Open Editor", { 256.0f, 96.0f } },
            ChoiceDescriptor{ Choice::Settings, "Settings", { 256.0f, 96.0f } },
            ChoiceDescriptor{ Choice::About, "About Epoch", { 256.0f, 96.0f } },
            ChoiceDescriptor{ Choice::Exit, "Quit", { 256.0f, 96.0f } }
        };

        static constexpr std::array kUpdaterShellChoices = {
            ChoiceDescriptor{ Choice::UpdateLatest, "Update Epoch Now", { 520.0f, 168.0f } }
        };

        static constexpr std::string_view updater_shell_description() noexcept
        {
            return "Moves through the newest packaged release first, then continues into the current main source build automatically when needed.";
        }

        [[nodiscard]] static bool updater_shell_auto_update_requested() noexcept
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

            return value == "smart-update";
        }

        std::size_t& selection_for_panel(LauncherPanel panel) noexcept
        {
            switch (panel)
            {
            case LauncherPanel::Projects: return projectSelection;
            case LauncherPanel::Games: return gameSelection;
            case LauncherPanel::Tools: return toolSelection;
            default: return projectSelection;
            }
        }

        static constexpr std::string_view panel_title(LauncherPanel panel) noexcept
        {
            switch (panel)
            {
            case LauncherPanel::Projects: return "Projects";
            case LauncherPanel::Games: return "Games";
            case LauncherPanel::Tools: return "Tools";
            default: return "Projects";
            }
        }

        static constexpr std::string_view panel_hint(LauncherPanel panel) noexcept
        {
            switch (panel)
            {
            case LauncherPanel::Projects: return "Choose a project and jump into the editor.";
            case LauncherPanel::Games: return "Launch playable scenes from the shared launcher.";
            case LauncherPanel::Tools: return "Open the editor, inspect info, or end the session.";
            default: return "";
            }
        }

        void activate_panel(LauncherPanel panel)
        {
            activePanel = panel;
            descriptors.clear();

            const auto append = [&](const auto& source)
            {
                descriptors.reserve(source.size());
                for (const auto& item : source)
                    descriptors.push_back(item);
            };

            switch (activePanel)
            {
            case LauncherPanel::Projects: append(kProjectChoices); break;
            case LauncherPanel::Games: append(kGameChoices); break;
            case LauncherPanel::Tools: append(kToolChoices); break;
            }

            selection = selection_for_panel(activePanel);
            if (!descriptors.empty())
                selection = (std::min)(selection, descriptors.size() - 1);
            else
                selection = 0;

            cachedWidth = -1;
            cachedHeight = -1;
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
            std::shared_ptr<epochnamespace::core::Context> ctx,
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

            if (resolvedWidth <= 0 && ctx)  resolvedWidth = ctx->get_width_safe();
            if (resolvedHeight <= 0 && ctx) resolvedHeight = ctx->get_height_safe();

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
            layoutOriginY = (std::max)(0.f, (cachedHeight - totalHeight) * 0.5f);
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

            set_max_columns(core::cli::updater_shell_requested ? 1 : core::cli::menu_columns);
            autoCommandConsumed = false;

            if (core::cli::updater_shell_requested)
            {
                descriptors.clear();
                descriptors.reserve(kUpdaterShellChoices.size());
                for (const auto& item : kUpdaterShellChoices)
                    descriptors.push_back(item);

                activePanel = LauncherPanel::Tools;
                projectSelection = 0;
                gameSelection = 0;
                toolSelection = 0;
                selection = 0;
                prevUp = prevDown = prevLeft = prevRight = prevEnter = false;

                const int w = ctx ? ctx->get_width_safe() : cachedWidth;
                const int h = ctx ? ctx->get_height_safe() : cachedHeight;
                recompute_layout(ctx, w, h);

                initialized = true;
                if (!initializationLogEmitted)
                {
                    initializationLogEmitted = true;
                    logger::get(kLogSys).log(
                        logger::LogLevel::INFO,
                        "Initialized updater-shell launcher.",
                        std::source_location::current());
                }
                return;
            }

            activePanel = LauncherPanel::Projects;
            projectSelection = 0;
            gameSelection = 0;
            toolSelection = 0;
            selection = 0;
            prevUp = prevDown = prevLeft = prevRight = prevEnter = false;
            activate_panel(activePanel);

            const int w = ctx ? ctx->get_width_safe() : cachedWidth;
            const int h = ctx ? ctx->get_height_safe() : cachedHeight;
            recompute_layout(ctx, w, h);

            initialized = true;
            if (!initializationLogEmitted)
            {
                initializationLogEmitted = true;
                logger::get(kLogSys).log(
                    logger::LogLevel::INFO,
                    "Initialized launcher menu with " +
                        std::to_string(descriptors.size()) + " entries",
                    std::source_location::current());
            }
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
            if (core::cli::updater_shell_requested)
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

            constexpr float kHeaderOffsetY = 136.0f;

            std::ignore = win;
            std::ignore = dt;

            int currentWidth = windowSize.x > 0 ? static_cast<int>(windowSize.x) : (ctx ? ctx->get_width_safe() : cachedWidth);
            int currentHeight = windowSize.y > 0 ? static_cast<int>(windowSize.y) : (ctx ? ctx->get_height_safe() : cachedHeight);
            if (currentWidth <= 0) currentWidth = cachedWidth;
            if (currentHeight <= 0) currentHeight = cachedHeight;
            if (currentWidth <= 0) currentWidth = 1;
            if (currentHeight <= 0) currentHeight = 1;

            if (currentWidth != cachedWidth || currentHeight != cachedHeight)
                recompute_layout(ctx, currentWidth, (std::max)(1, currentHeight - static_cast<int>(kHeaderOffsetY)));

            int mx = 0, my = 0;
            ctx->get_mouse_position_safe(mx, my);

            const int totalItems = int(descriptors.size());
            if (totalItems == 0 || cachedPositions.size() != size_t(totalItems))
                return std::nullopt;

            if (upPressed && !prevUp)
            {
                const auto next = static_cast<int>(activePanel) == 0 ? 2 : static_cast<int>(activePanel) - 1;
                activate_panel(static_cast<LauncherPanel>(next));
            }
            if (downPressed && !prevDown)
            {
                const auto next = (static_cast<int>(activePanel) + 1) % 3;
                activate_panel(static_cast<LauncherPanel>(next));
            }

            if (selection >= size_t(totalItems))
                selection = size_t(totalItems - 1);

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

            if (leftPressed && !prevLeft)  selection = (selection == 0) ? totalItems - 1 : selection - 1;
            if (rightPressed && !prevRight) selection = (selection + 1) % totalItems;
            if (!upPressed && !downPressed && !leftPressed && !rightPressed && hover >= 0)
                selection = hover;

            prevUp = upPressed; prevDown = downPressed;
            prevLeft = leftPressed; prevRight = rightPressed;

            gui::begin_window(title, framePosition, frameSize);

            constexpr float panelButtonWidth = 128.0f;
            constexpr float panelButtonHeight = 30.0f;
            constexpr float panelGap = 8.0f;
            float panelX = framePosition.x + 16.0f;
            const float panelY = framePosition.y + 14.0f;

            const auto draw_panel_button = [&](LauncherPanel panel)
            {
                gui::set_cursor({ panelX, panelY });
                std::string label = std::string(panel_title(panel));
                if (activePanel == panel)
                    label = "> " + label;
                if (gui::button(label, { panelButtonWidth, panelButtonHeight }))
                    activate_panel(panel);
                panelX += panelButtonWidth + panelGap;
            };

            draw_panel_button(LauncherPanel::Projects);
            draw_panel_button(LauncherPanel::Games);
            draw_panel_button(LauncherPanel::Tools);

            gui::set_cursor({ framePosition.x + 16.0f, framePosition.y + 52.0f });
            gui::label(std::string("Version: ") + epochnamespace::GetEngineDisplayString());
            gui::set_cursor({ framePosition.x + 16.0f, framePosition.y + 72.0f });
            gui::label(std::string("Launcher: ") + std::string(panel_title(activePanel)));
            gui::set_cursor({ framePosition.x + 16.0f, framePosition.y + 92.0f });
            gui::wrapped_label(panel_hint(activePanel), frameSize.x - 32.0f);

            std::optional<Choice> chosen{};
            for (int i = 0; i < totalItems; ++i) {
                const auto pos = position_for_index(i);
                gui::set_cursor({ float(pos.first), float(pos.second) });

                std::string label{ descriptors[i].label };
                if (size_t(i) == selection) label = "> " + label + " <";

                if (gui::button(label, descriptors[i].size)) {
                    selection = size_t(i);
                    selection_for_panel(activePanel) = selection;
                    chosen = descriptors[i].choice;
                }
            }

            gui::end_window();

            if (chosen) return chosen;
            if (enterPressed && !prevEnter)
            {
                selection_for_panel(activePanel) = selection;
                return descriptors[selection].choice;
            }

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

            int currentWidth = windowSize.x > 0 ? static_cast<int>(windowSize.x) : (ctx ? ctx->get_width_safe() : cachedWidth);
            int currentHeight = windowSize.y > 0 ? static_cast<int>(windowSize.y) : (ctx ? ctx->get_height_safe() : cachedHeight);
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
            const float stackHeight =
                lineHeight +
                18.0f +
                lineHeight +
                26.0f +
                descriptionHeight +
                32.0f +
                buttonHeight;
            const float contentY = framePosition.y + (std::max)(32.0f, (frameSize.y - stackHeight) * 0.5f);

            gui::set_cursor({ contentX, contentY });
            gui::label("Epoch Updater Shell");

            gui::set_cursor({ contentX, contentY + lineHeight + 18.0f });
            gui::label(std::string("Version: ") + epochnamespace::GetEngineDisplayString());

            gui::set_cursor({ contentX + textInset, contentY + lineHeight * 2.0f + 44.0f });
            gui::wrapped_label(updater_shell_description(), textWidth);

            gui::set_cursor({ contentX, contentY + lineHeight * 2.0f + 76.0f + descriptionHeight });
            const bool clicked = gui::button("Update To Current Epoch", { buttonWidth, buttonHeight });

            gui::end_window();

            std::optional<Choice> chosen{};
            if (clicked)
                chosen = Choice::UpdateLatest;
            else if (updater_shell_auto_update_requested() && !autoCommandConsumed)
            {
                autoCommandConsumed = true;
                chosen = Choice::UpdateLatest;
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
            activePanel = LauncherPanel::Projects;
            projectSelection = 0;
            gameSelection = 0;
            toolSelection = 0;
            selection = 0;
            prevUp = prevDown = prevLeft = prevRight = prevEnter = false;
            autoCommandConsumed = false;
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

            const float viewportWidth = ctx ? static_cast<float>(ctx->get_width_safe()) : 0.0f;
            const float viewportHeight = ctx ? static_cast<float>(ctx->get_height_safe()) : 0.0f;
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

                std::string label{ d.label };
                if (static_cast<int>(selection) == i)
                    label = "> " + label + " <";

                if (gui::button(label, d.size))
                {
                    selection = static_cast<std::size_t>(i);
                    chosen = d.choice;
                }

                y += d.size.y + itemSpacing;
            }

            gui::end_window();

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
