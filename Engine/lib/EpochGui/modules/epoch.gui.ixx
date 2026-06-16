module;

#include <cstdint>
#include <string_view>

export module epoch.gui;

export namespace epochnamespace::gui_lib
{
    inline constexpr std::string_view library_name = "EpochGui";
    inline constexpr int version_major = 0;
    inline constexpr int version_minor = 87;
    inline constexpr int version_revision = 43;
    inline constexpr std::string_view version_string = "0.87.43";

    struct Vec2
    {
        float x{};
        float y{};
    };

    struct Rect
    {
        Vec2 position{};
        Vec2 size{};
    };

    [[nodiscard]] inline bool contains(Rect rect, Vec2 point) noexcept
    {
        return point.x >= rect.position.x
            && point.x <= rect.position.x + rect.size.x
            && point.y >= rect.position.y
            && point.y <= rect.position.y + rect.size.y;
    }

    class LayoutController
    {
    public:
        virtual ~LayoutController() = default;
        [[nodiscard]] virtual std::string_view name() const noexcept = 0;
    };

    struct FloatingWindowState
    {
        Vec2 position{};
        Vec2 size{};
        Vec2 drag_offset{};
        Vec2 resize_origin_mouse{};
        Vec2 resize_origin_size{};
        bool open{ true };
        bool initialized{};
        bool dragging{};
        bool resizing{};
        bool close_pressed{};
        std::uint32_t focus_order{};
    };

    struct FloatingWindowOptions
    {
        Vec2 default_position{};
        Vec2 default_size{};
        Vec2 min_size{ 160.0f, 96.0f };
        Vec2 viewport_size{};
        float title_bar_height{ 32.0f };
        float content_padding{ 6.0f };
        bool movable{ true };
        bool resizable{ true };
        bool closable{ true };
    };

    struct FloatingWindowInput
    {
        Vec2 mouse_position{};
        bool mouse_down{};
        bool mouse_pressed{};
        bool mouse_released{};
    };

    struct FloatingWindowLayout
    {
        Rect window{};
        Rect title_bar{};
        Rect content{};
        Rect close_button{};
        Rect resize_handle{};
        bool visible{};
        bool hovered{};
        bool title_hovered{};
        bool close_hovered{};
        bool resize_hovered{};
        bool focused{};
        bool moved{};
        bool resized{};
        bool close_requested{};
    };

    class FloatingWindowController final : public LayoutController
    {
    public:
        [[nodiscard]] std::string_view name() const noexcept override;
        void normalize(FloatingWindowState& state, const FloatingWindowOptions& options) const noexcept;
        [[nodiscard]] FloatingWindowLayout update(
            FloatingWindowState& state,
            const FloatingWindowOptions& options,
            const FloatingWindowInput& input) const noexcept;
    };

    [[nodiscard]] const FloatingWindowController& floating_window_controller() noexcept;
    void normalize_floating_window(FloatingWindowState& state, const FloatingWindowOptions& options) noexcept;
    [[nodiscard]] FloatingWindowLayout update_floating_window(
        FloatingWindowState& state,
        const FloatingWindowOptions& options,
        const FloatingWindowInput& input) noexcept;

    enum class SplitterAxis : std::uint8_t
    {
        vertical,
        horizontal
    };

    struct SplitterLayoutOptions
    {
        Rect area{};
        SplitterAxis axis{ SplitterAxis::vertical };
        float split_fraction{ 0.5f };
        float thickness{ 7.0f };
        float min_before{ 64.0f };
        float min_after{ 64.0f };
    };

    struct SplitterLayout
    {
        Rect before{};
        Rect handle{};
        Rect after{};
        float split_fraction{ 0.5f };
        float split_offset{};
        bool fits_minimums{};
    };

    enum class ProgressBarDirection : std::uint8_t
    {
        left_to_right,
        right_to_left,
        top_to_bottom,
        bottom_to_top
    };

    struct ProgressBarLayoutOptions
    {
        Rect track{};
        float value{};
        float minimum{};
        float maximum{ 1.0f };
        float padding{};
        ProgressBarDirection direction{ ProgressBarDirection::left_to_right };
    };

    struct ProgressBarLayout
    {
        Rect track{};
        Rect inner{};
        Rect fill{};
        float fraction{};
        bool has_range{};
    };

    struct LoadingScreenLayoutOptions
    {
        Rect viewport{};
        Vec2 preferred_panel_size{ 640.0f, 300.0f };
        Vec2 minimum_panel_size{ 360.0f, 220.0f };
        float margin{ 32.0f };
        float padding{ 28.0f };
        float gap{ 14.0f };
        float title_height{ 30.0f };
        float message_height{ 72.0f };
        float progress_height{ 24.0f };
        float status_height{ 22.0f };
        float action_height{ 64.0f };
        float progress_padding{ 2.0f };
        float progress_value{};
    };

    struct LoadingScreenLayout
    {
        Rect viewport{};
        Rect panel{};
        Rect title{};
        Rect message{};
        ProgressBarLayout progress{};
        Rect status{};
        Rect action{};
        float progress_fraction{};
        bool visible{};
    };

    inline constexpr std::uint32_t invalid_selectable_row_index = 0xffffffffU;

    struct SelectableListLayoutOptions
    {
        Rect viewport{};
        std::uint32_t row_count{};
        float row_height{ 22.0f };
        float row_gap{};
        float scroll_offset{};
        float content_padding_x{};
        float content_padding_y{};
    };

    struct SelectableListVisibleRange
    {
        std::uint32_t first_index{};
        std::uint32_t past_last_index{};
        float content_height{};
        bool has_visible_rows{};
    };

    struct SelectableRowLayout
    {
        std::uint32_t index{ invalid_selectable_row_index };
        Rect row{};
        Rect content{};
        bool visible{};
        bool hovered{};
        bool selected{};
    };

    class LayoutPrimitiveController final : public LayoutController
    {
    public:
        [[nodiscard]] std::string_view name() const noexcept override;
        [[nodiscard]] SplitterLayout make_splitter(const SplitterLayoutOptions& options) const noexcept;
        [[nodiscard]] float splitter_fraction_from(Vec2 point, const SplitterLayoutOptions& options) const noexcept;
        [[nodiscard]] bool splitter_hit_test(const SplitterLayout& layout, Vec2 point, float hit_padding = 0.0f) const noexcept;
        [[nodiscard]] ProgressBarLayout make_progress_bar(const ProgressBarLayoutOptions& options) const noexcept;
        [[nodiscard]] LoadingScreenLayout make_loading_screen(const LoadingScreenLayoutOptions& options) const noexcept;
        [[nodiscard]] SelectableListVisibleRange visible_range(const SelectableListLayoutOptions& options) const noexcept;
        [[nodiscard]] SelectableRowLayout make_selectable_row(
            const SelectableListLayoutOptions& options,
            std::uint32_t index,
            Vec2 mouse_position,
            bool selected = false) const noexcept;
        [[nodiscard]] std::uint32_t selectable_row_at(const SelectableListLayoutOptions& options, Vec2 point) const noexcept;
    };

    [[nodiscard]] const LayoutPrimitiveController& layout_primitive_controller() noexcept;
    [[nodiscard]] SplitterLayout make_splitter_layout(const SplitterLayoutOptions& options) noexcept;
    [[nodiscard]] float splitter_fraction_from_point(const SplitterLayoutOptions& options, Vec2 point) noexcept;
    [[nodiscard]] bool splitter_hit_test(const SplitterLayout& layout, Vec2 point, float hit_padding = 0.0f) noexcept;
    [[nodiscard]] ProgressBarLayout make_progress_bar_layout(const ProgressBarLayoutOptions& options) noexcept;
    [[nodiscard]] LoadingScreenLayout make_loading_screen_layout(const LoadingScreenLayoutOptions& options) noexcept;
    [[nodiscard]] SelectableListVisibleRange selectable_list_visible_range(const SelectableListLayoutOptions& options) noexcept;
    [[nodiscard]] SelectableRowLayout make_selectable_row_layout(
        const SelectableListLayoutOptions& options,
        std::uint32_t index,
        Vec2 mouse_position,
        bool selected = false) noexcept;
    [[nodiscard]] std::uint32_t selectable_row_index_at(const SelectableListLayoutOptions& options, Vec2 point) noexcept;

    enum class PopupPlacement : std::uint8_t
    {
        below,
        above,
        right,
        left,
        centered,
        cursor
    };

    struct PopupState
    {
        Rect rect{};
        PopupPlacement placement{ PopupPlacement::below };
        bool open{};
        bool initialized{};
        bool pressed_inside{};
        bool pressed_owner{};
        std::uint32_t focus_order{};
    };

    struct PopupOptions
    {
        Rect owner{};
        Vec2 preferred_size{ 240.0f, 160.0f };
        Vec2 viewport_size{};
        Vec2 cursor_offset{ 12.0f, 12.0f };
        PopupPlacement placement{ PopupPlacement::below };
        float gap{ 4.0f };
        float margin{ 4.0f };
        bool flip_to_fit{ true };
        bool clamp_to_viewport{ true };
        bool close_on_outside_press{ true };
    };

    struct PopupInput
    {
        Vec2 mouse_position{};
        bool mouse_pressed{};
        bool mouse_released{};
        bool open_requested{};
        bool toggle_requested{};
        bool close_requested{};
        bool owner_pressed{};
        bool escape_pressed{};
    };

    struct PopupLayout
    {
        Rect popup{};
        Rect owner{};
        PopupPlacement placement{ PopupPlacement::below };
        bool visible{};
        bool hovered{};
        bool owner_hovered{};
        bool opened{};
        bool closed{};
        bool flipped{};
        bool clamped{};
    };

    class PopupLayoutController final : public LayoutController
    {
    public:
        [[nodiscard]] std::string_view name() const noexcept override;
        [[nodiscard]] Rect place(
            const PopupOptions& options,
            const PopupInput& input,
            PopupPlacement* used_placement = nullptr,
            bool* flipped = nullptr,
            bool* clamped = nullptr) const noexcept;
        void normalize(PopupState& state, const PopupOptions& options, const PopupInput& input) const noexcept;
        [[nodiscard]] PopupLayout update(PopupState& state, const PopupOptions& options, const PopupInput& input) const noexcept;
    };

    [[nodiscard]] const PopupLayoutController& popup_layout_controller() noexcept;
    [[nodiscard]] Rect place_popup(
        const PopupOptions& options,
        const PopupInput& input,
        PopupPlacement* used_placement = nullptr,
        bool* flipped = nullptr,
        bool* clamped = nullptr) noexcept;
    void normalize_popup(PopupState& state, const PopupOptions& options, const PopupInput& input) noexcept;
    [[nodiscard]] PopupLayout update_popup(PopupState& state, const PopupOptions& options, const PopupInput& input) noexcept;

    enum class DockSlot : std::uint8_t
    {
        none,
        left,
        right,
        top,
        bottom,
        center
    };

    struct DockPaneState
    {
        std::uint32_t id{};
        DockSlot slot{ DockSlot::center };
        Rect docked_rect{};
        Rect popout_rect{};
        Vec2 min_size{ 120.0f, 80.0f };
        float weight{ 1.0f };
        bool visible{ true };
        bool initialized{};
        bool popped_out{};
        bool popout_context_open{};
        bool active{};
        std::uint32_t focus_order{};
    };

    struct DockLayoutState
    {
        std::uint32_t active_pane_id{};
        std::uint32_t next_focus_order{ 1 };
        bool initialized{};
    };

    struct DockLayoutOptions
    {
        Rect workspace{};
        Vec2 min_pane_size{ 96.0f, 64.0f };
        Vec2 default_popout_size{ 360.0f, 260.0f };
        float edge_fraction{ 0.24f };
        float title_bar_height{ 28.0f };
        float content_padding{ 6.0f };
        float popout_spacing{ 24.0f };
        float visible_margin{ 48.0f };
    };

    struct DockLayoutInput
    {
        Vec2 mouse_position{};
        bool mouse_pressed{};
        bool mouse_released{};
        std::uint32_t activate_pane_id{};
        std::uint32_t popout_pane_id{};
        std::uint32_t redock_pane_id{};
        std::uint32_t toggle_popout_pane_id{};
        DockSlot redock_slot{ DockSlot::none };
    };

    struct DockPaneLayout
    {
        std::uint32_t id{};
        DockSlot slot{ DockSlot::center };
        Rect frame{};
        Rect title_bar{};
        Rect content{};
        bool visible{};
        bool hovered{};
        bool docked{};
        bool popped_out{};
        bool context_window_requested{};
        bool active{};
    };

    struct DockLayoutResult
    {
        bool changed{};
        bool focus_changed{};
        bool context_windows_changed{};
        std::uint32_t active_pane_id{};
        std::uint32_t context_window_count{};
    };

    class DockLayoutController final : public LayoutController
    {
    public:
        [[nodiscard]] std::string_view name() const noexcept override;
        [[nodiscard]] bool is_valid_slot(DockSlot slot) const noexcept;
        [[nodiscard]] bool pane_requests_context_window(const DockPaneState& pane) const noexcept;
        [[nodiscard]] DockPaneLayout make_pane_layout(
            const DockPaneState& pane,
            const DockLayoutOptions& options,
            const DockLayoutInput& input) const noexcept;
        void activate_pane(
            DockLayoutState& state,
            DockPaneState* panes,
            std::uint32_t pane_count,
            std::uint32_t pane_id) const noexcept;
        void normalize(
            DockLayoutState& state,
            DockPaneState* panes,
            std::uint32_t pane_count,
            const DockLayoutOptions& options) const noexcept;
        [[nodiscard]] DockLayoutResult update(
            DockLayoutState& state,
            DockPaneState* panes,
            std::uint32_t pane_count,
            const DockLayoutOptions& options,
            const DockLayoutInput& input) const noexcept;
    };

    [[nodiscard]] const DockLayoutController& dock_layout_controller() noexcept;
    [[nodiscard]] bool is_valid_dock_slot(DockSlot slot) noexcept;
    [[nodiscard]] bool dock_pane_requests_context_window(const DockPaneState& pane) noexcept;
    [[nodiscard]] DockPaneLayout make_dock_pane_layout(
        const DockPaneState& pane,
        const DockLayoutOptions& options,
        const DockLayoutInput& input) noexcept;
    void activate_dock_pane(
        DockLayoutState& state,
        DockPaneState* panes,
        std::uint32_t pane_count,
        std::uint32_t pane_id) noexcept;
    void normalize_dock_layout(
        DockLayoutState& state,
        DockPaneState* panes,
        std::uint32_t pane_count,
        const DockLayoutOptions& options) noexcept;
    [[nodiscard]] DockLayoutResult update_dock_layout(
        DockLayoutState& state,
        DockPaneState* panes,
        std::uint32_t pane_count,
        const DockLayoutOptions& options,
        const DockLayoutInput& input) noexcept;

    enum class DockableWindowMode : std::uint8_t
    {
        docked,
        floating,
        detached
    };

    enum class DockableWindowAction : std::uint8_t
    {
        none,
        focus,
        dock,
        float_window,
        detach,
        close
    };

    struct DockableWindowHostState
    {
        std::uint32_t active_window_id{};
        std::uint32_t next_focus_order{ 1 };
        bool changed_this_frame{};
    };

    struct DockableWindowState
    {
        std::uint32_t id{};
        DockableWindowMode mode{ DockableWindowMode::floating };
        DockSlot dock_slot{ DockSlot::right };
        FloatingWindowState floating{};
        bool visible{ true };
        bool initialized{};
        bool active{};
        bool detach_requested{};
        bool close_requested{};
        std::uint32_t focus_order{};
    };

    struct DockableWindowOptions
    {
        std::string_view title{};
        Rect docked_frame{};
        FloatingWindowOptions floating{};
        Vec2 viewport_size{};
        float title_bar_height{ 30.0f };
        float content_padding{ 6.0f };
        float action_button_width{ 72.0f };
        float action_button_gap{ 4.0f };
        bool allow_dock{ true };
        bool allow_float{ true };
        bool allow_detach{ true };
        bool allow_close{ true };
        DockSlot fallback_dock_slot{ DockSlot::right };
    };

    struct DockableWindowInput
    {
        Vec2 mouse_position{};
        bool mouse_down{};
        bool mouse_pressed{};
        bool mouse_released{};
        DockableWindowAction requested_action{ DockableWindowAction::none };
        DockSlot requested_dock_slot{ DockSlot::none };
    };

    struct DockableWindowChrome
    {
        Rect frame{};
        Rect title_bar{};
        Rect content{};
        Rect dock_button{};
        Rect float_button{};
        Rect detach_button{};
        Rect close_button{};
        bool visible{};
        bool hovered{};
        bool title_hovered{};
        bool dock_hovered{};
        bool float_hovered{};
        bool detach_hovered{};
        bool close_hovered{};
        bool active{};
    };

    struct DockableWindowResult
    {
        DockableWindowChrome chrome{};
        DockableWindowMode mode{ DockableWindowMode::floating };
        DockableWindowAction action{ DockableWindowAction::none };
        DockSlot dock_slot{ DockSlot::none };
        bool changed{};
        bool focused{};
        bool dock_requested{};
        bool float_requested{};
        bool detach_requested{};
        bool close_requested{};
    };

    class DockableWindowController final : public LayoutController
    {
    public:
        [[nodiscard]] std::string_view name() const noexcept override;
        void focus(
            DockableWindowHostState& host,
            DockableWindowState& state) const noexcept;
        void normalize(
            DockableWindowHostState& host,
            DockableWindowState& state,
            const DockableWindowOptions& options) const noexcept;
        [[nodiscard]] DockableWindowChrome make_chrome(
            const DockableWindowState& state,
            const DockableWindowOptions& options,
            const DockableWindowInput& input) const noexcept;
        [[nodiscard]] DockableWindowResult update(
            DockableWindowHostState& host,
            DockableWindowState& state,
            const DockableWindowOptions& options,
            const DockableWindowInput& input) const noexcept;
    };

    [[nodiscard]] const DockableWindowController& dockable_window_controller() noexcept;
    void focus_dockable_window(
        DockableWindowHostState& host,
        DockableWindowState& state) noexcept;
    void normalize_dockable_window(
        DockableWindowHostState& host,
        DockableWindowState& state,
        const DockableWindowOptions& options) noexcept;
    [[nodiscard]] DockableWindowChrome make_dockable_window_chrome(
        const DockableWindowState& state,
        const DockableWindowOptions& options,
        const DockableWindowInput& input) noexcept;
    [[nodiscard]] DockableWindowResult update_dockable_window(
        DockableWindowHostState& host,
        DockableWindowState& state,
        const DockableWindowOptions& options,
        const DockableWindowInput& input) noexcept;

    enum class PanelHostMode : std::uint8_t
    {
        docked,
        floating,
        popup,
        external_host
    };

    enum class PanelHostAction : std::uint8_t
    {
        none,
        focus,
        dock,
        float_panel,
        show_popup,
        request_external_host,
        redock_from_external_host,
        close
    };

    struct PanelHostState
    {
        std::uint32_t id{};
        PanelHostMode mode{ PanelHostMode::docked };
        DockSlot dock_slot{ DockSlot::right };
        Rect docked_frame{};
        FloatingWindowState floating{};
        PopupState popup{};
        Rect external_frame{};
        bool visible{ true };
        bool initialized{};
        bool active{};
        bool external_host_requested{};
        bool external_host_active{};
        std::uint64_t external_host_token{};
        std::uint32_t focus_order{};
    };

    struct PanelHostOptions
    {
        std::string_view title{};
        Rect docked_frame{};
        FloatingWindowOptions floating{};
        PopupOptions popup{};
        Rect default_external_frame{};
        DockSlot fallback_dock_slot{ DockSlot::right };
        bool allow_dock{ true };
        bool allow_float{ true };
        bool allow_popup{ true };
        bool allow_external_host{ true };
        bool allow_close{ true };
    };

    struct PanelHostInput
    {
        DockableWindowHostState* focus_host{};
        Vec2 mouse_position{};
        bool mouse_down{};
        bool mouse_pressed{};
        bool mouse_released{};
        PanelHostAction requested_action{ PanelHostAction::none };
        DockSlot requested_dock_slot{ DockSlot::none };
        std::uint64_t external_host_token{};
        bool external_host_confirmed{};
        bool external_host_closed{};
        bool escape_pressed{};
    };

    struct PanelHostMetadata
    {
        std::uint32_t id{};
        PanelHostMode mode{ PanelHostMode::docked };
        DockSlot dock_slot{ DockSlot::none };
        Rect frame{};
        bool visible{};
        bool active{};
        bool wants_external_host{};
        bool external_host_active{};
        std::uint64_t external_host_token{};
    };

    struct PanelHostResult
    {
        PanelHostMetadata metadata{};
        PanelHostAction action{ PanelHostAction::none };
        bool changed{};
        bool focus_changed{};
        bool placement_changed{};
        bool external_host_changed{};
        bool close_requested{};
    };

    class PanelHostController final : public LayoutController
    {
    public:
        [[nodiscard]] std::string_view name() const noexcept override;
        void focus(PanelHostState& state, DockableWindowHostState* host = nullptr) const noexcept;
        void normalize(PanelHostState& state, const PanelHostOptions& options) const noexcept;
        [[nodiscard]] PanelHostMetadata metadata(const PanelHostState& state) const noexcept;
        [[nodiscard]] PanelHostResult update(
            PanelHostState& state,
            const PanelHostOptions& options,
            const PanelHostInput& input) const noexcept;
    };

    [[nodiscard]] const PanelHostController& panel_host_controller() noexcept;
    void focus_panel_host(
        PanelHostState& state,
        DockableWindowHostState* host = nullptr) noexcept;
    void normalize_panel_host(
        PanelHostState& state,
        const PanelHostOptions& options) noexcept;
    [[nodiscard]] PanelHostMetadata panel_host_metadata(
        const PanelHostState& state) noexcept;
    [[nodiscard]] PanelHostResult update_panel_host(
        PanelHostState& state,
        const PanelHostOptions& options,
        const PanelHostInput& input) noexcept;
}
