#include <gui/dock_layout.hpp>

#include <cmath>
#include <cstdint>

namespace
{
    [[nodiscard]] bool same(float lhs, float rhs) noexcept
    {
        return std::abs(lhs - rhs) < 0.001f;
    }

    [[nodiscard]] int check_three_by_two_grid()
    {
        using namespace epochengine::gui_lib;

        const Rect previews[6]{
            { { 10.0f, 20.0f }, { 100.0f, 80.0f } },
            { { 120.0f, 20.0f }, { 100.0f, 80.0f } },
            { { 230.0f, 20.0f }, { 100.0f, 80.0f } },
            { { 10.0f, 140.0f }, { 100.0f, 80.0f } },
            { { 120.0f, 140.0f }, { 100.0f, 80.0f } },
            { { 230.0f, 140.0f }, { 100.0f, 80.0f } }
        };

        DockContextGridOptions options{};
        options.item_previews = previews;
        options.item_count = 6U;
        options.pointer = { 11.0f, 160.0f };
        options.highlight_extent = 12.0f;
        options.column_count = 3U;

        const auto beforeLayout = make_dock_context_grid_layout(options);
        if (beforeLayout.count != 12U
            || beforeLayout.column_count != 3U
            || beforeLayout.row_count != 2U)
        {
            return 1;
        }

        for (std::uint32_t index = 0U; index < 6U; ++index)
        {
            const auto& before = beforeLayout.highlights[index * 2U];
            const auto& after = beforeLayout.highlights[index * 2U + 1U];
            const std::uint32_t expectedRow = index / 3U;
            const std::uint32_t expectedColumn = index % 3U;

            if (before.position != DockInsertionPosition::before
                || before.context_item_index != index
                || before.insertion_index != index
                || before.row_index != expectedRow
                || before.column_index != expectedColumn
                || !same(before.target_bounds.position.y, previews[index].position.y)
                || !same(before.target_bounds.size.y, previews[index].size.y))
            {
                return 2;
            }

            if (after.position != DockInsertionPosition::after
                || after.context_item_index != index
                || after.insertion_index != index + 1U
                || after.row_index != expectedRow
                || after.column_index != expectedColumn
                || !same(after.target_bounds.position.y, previews[index].position.y)
                || !same(after.target_bounds.size.y, previews[index].size.y))
            {
                return 3;
            }
        }

        if (!beforeLayout.has_hovered_insertion
            || beforeLayout.hovered_position != DockInsertionPosition::before
            || beforeLayout.hovered_context_item_index != 3U
            || beforeLayout.hovered_insertion_index != 3U
            || beforeLayout.hovered_row_index != 1U
            || beforeLayout.hovered_column_index != 0U)
        {
            return 4;
        }

        options.pointer = { 329.0f, 160.0f };
        const auto afterLayout = make_dock_context_grid_layout(options);
        if (!afterLayout.has_hovered_insertion
            || afterLayout.hovered_position != DockInsertionPosition::after
            || afterLayout.hovered_context_item_index != 5U
            || afterLayout.hovered_insertion_index != 6U
            || afterLayout.hovered_row_index != 1U
            || afterLayout.hovered_column_index != 2U)
        {
            return 5;
        }

        return 0;
    }

    [[nodiscard]] int check_general_item_count()
    {
        using namespace epochengine::gui_lib;

        const Rect previews[5]{
            { { 0.0f, 0.0f }, { 80.0f, 60.0f } },
            { { 90.0f, 0.0f }, { 80.0f, 60.0f } },
            { { 0.0f, 70.0f }, { 80.0f, 60.0f } },
            { { 90.0f, 70.0f }, { 80.0f, 60.0f } },
            { { 0.0f, 140.0f }, { 80.0f, 60.0f } }
        };

        DockContextGridOptions options{};
        options.item_previews = previews;
        options.item_count = 5U;
        options.pointer = { -100.0f, -100.0f };
        options.column_count = 2U;

        const auto layout = make_dock_context_grid_layout(options);
        if (layout.count != 10U
            || layout.column_count != 2U
            || layout.row_count != 3U
            || layout.has_hovered_insertion)
        {
            return 6;
        }

        const auto& finalBefore = layout.highlights[8U];
        const auto& finalAfter = layout.highlights[9U];
        if (finalBefore.context_item_index != 4U
            || finalBefore.row_index != 2U
            || finalBefore.column_index != 0U
            || finalBefore.insertion_index != 4U
            || finalAfter.insertion_index != 5U)
        {
            return 7;
        }

        return 0;
    }

    [[nodiscard]] int check_overlapping_targets_choose_nearest_edge()
    {
        using namespace epochengine::gui_lib;

        const Rect previews[2]{
            { { 0.0f, 0.0f }, { 100.0f, 40.0f } },
            { { 96.0f, 0.0f }, { 100.0f, 40.0f } }
        };

        DockContextGridOptions options{};
        options.item_previews = previews;
        options.item_count = 2U;
        options.pointer = { 97.0f, 20.0f };
        options.highlight_extent = 10.0f;
        options.column_count = 2U;

        const auto layout = make_dock_context_grid_layout(options);
        if (!layout.has_hovered_insertion
            || layout.hovered_position != DockInsertionPosition::before
            || layout.hovered_context_item_index != 1U
            || layout.hovered_insertion_index != 1U
            || layout.hovered_row_index != 0U
            || layout.hovered_column_index != 1U)
        {
            return 8;
        }

        return 0;
    }
}

int main()
{
    if (const int result = check_three_by_two_grid(); result != 0)
        return result;
    if (const int result = check_general_item_count(); result != 0)
        return result;
    return check_overlapping_targets_choose_nearest_edge();
}