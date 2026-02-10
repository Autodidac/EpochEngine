
module; // REQUIRED global module fragment

#include <include/aengine.config.hpp> // for ALMOND_USING Macros 		// for ALMOND_USING_SDL
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
import aengine.context.window;
import aengine.core.context;
import aengine.context.type;

// ------------------------------------------------------------
// Standard library
// ------------------------------------------------------------
import <vector>;
import <string>;
import <optional>;
import <tuple>;
import <algorithm>;
import <array>;
import <cmath>;
import <iomanip>;
import <iostream>;
import <memory>;
import <sstream>;

// ============================================================
// Menu
// ============================================================

namespace gui = almondnamespace::gui;

export namespace almondnamespace::menu
{
    enum class Choice {
        Snake, Tetris, Pacman, Frogger, Sokoban,
        Minesweep, Puzzle, Bejeweled, Fourty,
        Sandsim, Cellular, Settings, Exit
    };

    enum class EditorCommandChoice {
        OpenProject,
        Settings,
        RunGame,
        Exit
    };

    struct ChoiceDescriptor {
        Choice      choice;
        std::string label;
        gui::Vec2   size;
    };

    struct EditorCommandDescriptor {
        EditorCommandChoice choice;
        std::string label;
        gui::Vec2 size;
    };

    struct MenuOverlay
    {
        std::vector<ChoiceDescriptor> descriptors;
        std::size_t selection = 0;

        bool prevUp = false, prevDown = false, prevLeft = false,
            prevRight = false, prevEnter = false;
        bool initialized = false;

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

        void initialize_game_choices()
        {
            if (initialized) return;

            set_max_columns(core::cli::menu_columns);

            selection = 0;
            prevUp = prevDown = prevLeft = prevRight = prevEnter = false;

            constexpr gui::Vec2 DefaultButtonSize{ 256.0f, 96.0f };

            descriptors = {
                { Choice::Snake,      "Snake",      DefaultButtonSize },
                { Choice::Tetris,    "Tetris",     DefaultButtonSize },
                { Choice::Pacman,    "Pacman",     DefaultButtonSize },
                { Choice::Frogger,   "Frogger",    DefaultButtonSize },
                { Choice::Sokoban,   "Sokoban",    DefaultButtonSize },
                { Choice::Minesweep, "Minesweep",  DefaultButtonSize },
                { Choice::Puzzle,    "Puzzle",     DefaultButtonSize },
                { Choice::Bejeweled, "Bejeweled",  DefaultButtonSize },
                { Choice::Fourty,    "2048",       DefaultButtonSize },
                { Choice::Sandsim,   "Sand Sim",   DefaultButtonSize },
                { Choice::Cellular,  "Cellular",   DefaultButtonSize }
            };

            initialized = true;
            std::cout << "[Menu] Initialized " << descriptors.size() << " game entries\n";
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
            std::shared_ptr<almondnamespace::core::Context> ctx,
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

            set_max_columns(core::cli::menu_columns);

            selection = 0;
            prevUp = prevDown = prevLeft = prevRight = prevEnter = false;

            constexpr gui::Vec2 DefaultButtonSize{ 256.0f, 96.0f };

            descriptors = {
                { Choice::Snake,      "Snake",      DefaultButtonSize },
                { Choice::Tetris,    "Tetris",     DefaultButtonSize },
                { Choice::Pacman,    "Pacman",     DefaultButtonSize },
				{ Choice::Frogger,   "Frogger",    DefaultButtonSize },
                { Choice::Sokoban,   "Sokoban",    DefaultButtonSize },
                { Choice::Minesweep, "Minesweep",  DefaultButtonSize },
                { Choice::Puzzle,    "Puzzle",     DefaultButtonSize },
                { Choice::Bejeweled, "Bejeweled",  DefaultButtonSize },
                { Choice::Fourty,    "2048",       DefaultButtonSize },
                { Choice::Sandsim,   "Sand Sim",   DefaultButtonSize },
                { Choice::Cellular,  "Cellular",   DefaultButtonSize },
                { Choice::Settings,  "Settings",   DefaultButtonSize },
                { Choice::Exit,      "Quit",       DefaultButtonSize }
            };

            const int w = ctx ? ctx->get_width_safe() : cachedWidth;
            const int h = ctx ? ctx->get_height_safe() : cachedHeight;
            recompute_layout(ctx, w, h);

            initialized = true;
            std::cout << "[Menu] Initialized " << descriptors.size() << " entries\n";
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
                "Main Menu",
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

            std::ignore = win;
            std::ignore = dt;

            int currentWidth = windowSize.x > 0 ? static_cast<int>(windowSize.x) : (ctx ? ctx->get_width_safe() : cachedWidth);
            int currentHeight = windowSize.y > 0 ? static_cast<int>(windowSize.y) : (ctx ? ctx->get_height_safe() : cachedHeight);
            if (currentWidth <= 0) currentWidth = cachedWidth;
            if (currentHeight <= 0) currentHeight = cachedHeight;
            if (currentWidth <= 0) currentWidth = 1;
            if (currentHeight <= 0) currentHeight = 1;

            if (currentWidth != cachedWidth || currentHeight != cachedHeight)
                recompute_layout(ctx, currentWidth, currentHeight);

            int mx = 0, my = 0;
            ctx->get_mouse_position_safe(mx, my);

            const int totalItems = int(descriptors.size());
            if (totalItems == 0 || cachedPositions.size() != size_t(totalItems))
                return std::nullopt;

            if (selection >= size_t(totalItems))
                selection = size_t(totalItems - 1);

           // const bool flipVertical = ctx && ctx ->type == core::ContextType::OpenGL;

            auto position_for_index = [&](int idx) {
                auto base = cachedPositions[idx];
                return std::pair<int, int>{
                    base.first + static_cast<int>(std::round(windowPosition.x)),
                    base.second + static_cast<int>(std::round(windowPosition.y))
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

            const int cols = (std::max)(1, columns);
            const int rowsLocal = (std::max)(1, rows);

            if (leftPressed && !prevLeft)  selection = (selection == 0) ? totalItems - 1 : selection - 1;
            if (rightPressed && !prevRight) selection = (selection + 1) % totalItems;
            if (upPressed && !prevUp)    selection = (selection < cols) ? selection + (rowsLocal - 1) * cols : selection - cols;
            if (downPressed && !prevDown)  selection = (selection + cols >= totalItems) ? selection % cols : selection + cols;
            if (!upPressed && !downPressed && !leftPressed && !rightPressed && hover >= 0)
                selection = hover;

            prevUp = upPressed; prevDown = downPressed;
            prevLeft = leftPressed; prevRight = rightPressed;

            const float pad = LayoutSpacing * 0.5f;
            const gui::Vec2 chromePosition{
                windowPosition.x + layoutOriginX - pad,
                windowPosition.y + layoutOriginY - pad
            };
            const gui::Vec2 chromeSize{
                layoutWidth + pad * 2,
                layoutHeight + pad * 2
            };

            if (clampToWindow && windowSize.x > 0.f && windowSize.y > 0.f)
            {
                gui::begin_window(
                    title,
                    windowPosition,
                    windowSize
                );
            }
            else
            {
                gui::begin_window(
                    title,
                    chromePosition,
                    chromeSize
                );
            }

            std::optional<Choice> chosen{};
            for (int i = 0; i < totalItems; ++i) {
                const auto pos = position_for_index(i);
                gui::set_cursor({ float(pos.first), float(pos.second) });

                std::string label = descriptors[i].label;
                if (size_t(i) == selection) label = "> " + label + " <";

                if (gui::button(label, descriptors[i].size)) {
                    selection = size_t(i);
                    chosen = descriptors[i].choice;
                }
            }

            gui::end_window();

            if (chosen) return chosen;
            if (enterPressed && !prevEnter) return Choice(selection);

            prevEnter = enterPressed;
            return std::nullopt;
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

                std::string label = d.label;
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
