// SPDX-License-Identifier: MPL-2.0
module;

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

export module epoch.gui.command_menu;
export import epoch.gui;

export namespace epochengine::gui_lib
{
    enum class CommandMenuIndicator : std::uint8_t { none, check, radio };

    // Borrowed child ranges cannot instantiate std::span's iterator concepts
    // while their recursive item type is incomplete at a module boundary.
    template<typename Item>
    struct CommandMenuItemsView
    {
        const Item* data_{};
        std::size_t count_{};
        constexpr CommandMenuItemsView() noexcept = default;
        constexpr CommandMenuItemsView(const Item* data,std::size_t count) noexcept : data_(data),count_(count) {}
        template<typename Range> requires requires(Range& range){range.data();range.size();}
        constexpr CommandMenuItemsView(Range& range) noexcept : data_(range.data()),count_(range.size()) {}
        [[nodiscard]] constexpr bool empty() const noexcept {return count_==0u;}
        [[nodiscard]] constexpr std::size_t size() const noexcept {return count_;}
        [[nodiscard]] constexpr const Item* begin() const noexcept {return data_;}
        [[nodiscard]] constexpr const Item* end() const noexcept {return count_?data_+count_:data_;}
        [[nodiscard]] constexpr const Item& operator[](std::size_t index) const noexcept {return data_[index];}
        constexpr operator std::span<const Item>() const noexcept {return {data_,count_};}
    };

    struct CommandMenuItemSpec
    {
        std::uint64_t id{};
        std::string_view label{}, shortcut{};
        // Measured by the consumer's existing font/menu-row policy, including
        // its indicator and shortcut gutters. No font/renderer is owned here.
        float preferred_width{};
        bool enabled{true};
        bool separator_before{}; // Drawn within the row, not another row.
        CommandMenuIndicator indicator{CommandMenuIndicator::none};
        bool checked{};
        CommandMenuItemsView<CommandMenuItemSpec> children{};
    };

    struct CommandMenuOptions
    {
        Rect owner{};
        Vec2 viewport_size{};
        Vec2 padding{6.0f, 6.0f};
        float row_height{24.0f};
        float minimum_width{};
        float maximum_height{}; // Zero uses the available viewport.
        float scrollbar_width{10.0f};
        float scrollbar_gap{2.0f};
        float gap{4.0f};
        float margin{4.0f};
        PopupPlacement placement{PopupPlacement::below};
    };

    struct CommandMenuState
    {
        PopupState popup{};
        std::optional<std::uint64_t> focused_id{}, pressed_id{};
        float scroll_offset{};
        float hover_exit_seconds{};
        bool dragging_scrollbar{};
        float drag_grab_offset{};
        Vec2 last_pointer{};
        bool pointer_initialized{};
        std::optional<std::uint64_t> open_submenu_id{};
        std::vector<CommandMenuState> submenu{}; // At most one active branch.
    };

    struct CommandMenuInput
    {
        PopupInput popup{};
        // Queued input may contain press and release in one update. Hit the
        // press at its admitted origin, not the later hover/release position.
        std::optional<Vec2> press_position{};
        float wheel_delta{}; // Pixels; positive moves toward the first row.
        bool pointer_down{};
        bool up_pressed{}, down_pressed{}, home_pressed{}, end_pressed{};
        bool page_up_pressed{}, page_down_pressed{}, accept_pressed{};
        bool left_pressed{}, right_pressed{}, focus_lost{};
        // A caller-resolved shortcut; the component validates enabled/id state.
        std::optional<std::uint64_t> shortcut_id{};
    };

    struct CommandMenuMeasure
    {
        Vec2 preferred_size{};
        float content_height{};
        bool valid{};
    };

    struct CommandMenuRow
    {
        std::uint64_t id{};
        std::size_t item_index{};
        Rect rect{};
        bool visible{}, hovered{}, focused{};
    };

    struct CommandMenuLayout
    {
        PopupLayout popup{};
        Rect content_clip{};
        std::vector<CommandMenuRow> rows{};
        std::optional<ScrollbarLayout> scrollbar{};
        Vec2 preferred_size{};
        float content_height{}, maximum_scroll_offset{}, scroll_offset{};
        std::optional<std::size_t> hovered_index{}, focused_index{}, activated_index{};
        std::optional<std::uint64_t> activated_id{};
        int owner_navigation{}; // -1/1 requests the previous/next menu owner.
        bool valid{}, pointer_captured{}, keyboard_captured{}, wheel_scrolled{};
        std::vector<CommandMenuLayout> submenus{};
    };

    [[nodiscard]] CommandMenuMeasure measure_command_menu(
        std::span<const CommandMenuItemSpec> items, const CommandMenuOptions& options) noexcept;
    [[nodiscard]] CommandMenuLayout update_command_menu(CommandMenuState& state,
        std::span<const CommandMenuItemSpec> items, const CommandMenuOptions& options,
        const CommandMenuInput& input);
    // True only after continuous pointer absence; returning to any menu pane resets it.
    [[nodiscard]] bool command_menu_hover_exit_due(CommandMenuState& state,
        bool keep_alive, float delta_seconds, float delay_seconds = 0.75f) noexcept;
    void close_command_menu(CommandMenuState& state) noexcept;
}
