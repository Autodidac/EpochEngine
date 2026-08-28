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

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

export module gui.engine;

import core.context;
import sprite.handle;

namespace epochengine::gui
{
    // Engine-internal GUI library API. Editor domains compose these primitives;
    // generic widget, layout, theme, and clipping behavior belongs here first.
    export struct Vec2
    {
        float x{};
        float y{};

        constexpr Vec2() = default;
        constexpr Vec2(float _x, float _y) : x(_x), y(_y) {}

        constexpr Vec2& operator+=(Vec2 rhs) noexcept { x += rhs.x; y += rhs.y; return *this; }
        friend constexpr Vec2 operator+(Vec2 a, Vec2 b) noexcept { a += b; return a; }
    };

    export struct Color
    {
        std::uint8_t r{}, g{}, b{}, a{ 255 };
    };

    export enum class ThemeVariant : std::uint8_t
    {
        DefaultDark = 0,
        DefaultLight,
        ClassicLauncher,
        MidnightBlue,
        EmberForge,
        ForestTerminal,
        AuroraSteel
    };

    export enum class ThemePreference : std::uint8_t
    {
        FollowSystemDark = 0,
        ProfessionalDark,
        ClassicLauncher,
        MidnightBlue,
        EmberForge,
        ForestTerminal,
        AuroraSteel,
        Light,
        Dark
    };

    export struct ThemePreferenceChoice
    {
        std::string_view label{};
        ThemePreference preference{ ThemePreference::FollowSystemDark };
    };

    export class ScopedTheme
    {
    public:
        explicit ScopedTheme(ThemeVariant theme) noexcept;
        explicit ScopedTheme(ThemePreference preference) noexcept;
        ~ScopedTheme() noexcept;

        ScopedTheme(const ScopedTheme&) = delete;
        ScopedTheme& operator=(const ScopedTheme&) = delete;

    private:
        bool active_{ true };
    };

    export class ScopedRoundedRectangles
    {
    public:
        explicit ScopedRoundedRectangles(bool enabled) noexcept;
        ~ScopedRoundedRectangles() noexcept;

        ScopedRoundedRectangles(const ScopedRoundedRectangles&) = delete;
        ScopedRoundedRectangles& operator=(const ScopedRoundedRectangles&) = delete;

    private:
        bool active_{ true };
    };

    export enum class EventType : std::uint8_t
    {
        None = 0,
        MouseMove,
        MouseDown,
        MouseUp,
        MouseWheel,
        KeyDown,
        KeyUp,
        TextInput
    };

    export struct InputEvent
    {
        EventType type{ EventType::None };
        Vec2 mouse_pos{};
        int mouse_button{};
        int wheel_delta{};
        int key{};
        bool ctrl_down{};
        bool shift_down{};
        bool alt_down{};
        std::string text{};
    };

    export struct WidgetBounds
    {
        Vec2 position{};
        Vec2 size{};
    };

    export struct DragSurfaceState
    {
        Vec2 press_position{};
        bool pending{};
        bool dragging{};
    };

    export struct DragSurfaceOptions
    {
        Vec2 position{};
        Vec2 size{};
        float hit_padding{6.0f};
        float minimum_hit_extent{32.0f};
        float drag_threshold{4.0f};
        bool enabled{true};
    };

    export struct DragSurfaceResult
    {
        Vec2 delta{};
        bool hovered{};
        bool pressed{};
        bool dragging{};
        bool released{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return dragging || released;
        }
    };

    export struct EditBoxResult
    {
        bool active{};
        bool changed{};
        bool submitted{};
    };

    export struct SourceEditorOptions
    {
        std::string_view id{};
        Vec2 size{};
        std::size_t max_chars{ 256u * 1024u };
        bool show_context_menu{ true };
    };

    export struct SourceEditorResult
    {
        EditBoxResult edit{};
        bool copied{};
        bool cut{};
        bool pasted{};
        bool selected_all{};
    };

    export struct ConsoleWindowActionSpec
    {
        std::string_view label{};
        float width{};
        bool enabled{ true };
        bool activate_on_press{};
    };

    export enum class TextMessageRole : std::uint8_t
    {
        neutral,
        user,
        assistant,
        system,
        error
    };

    export struct ConsoleWindowOptions
    {
        std::string_view title{};
        Vec2 position{};
        Vec2 size{};

        const std::vector<std::string>& lines;
        std::span<const TextMessageRole> line_roles{};
        std::size_t max_visible_lines{ 200 };

        std::string* input{ nullptr };
        std::size_t max_input_chars{ 4096 };
        bool multiline_input{ false };
        bool show_send_button{ false };
        bool send_button_enabled{ true };
        float send_button_width{ 96.0f };
        std::string_view send_button_label{ "Send >" };
        std::string_view task_label{};
        std::string_view task_value{};
        std::string* task_edit_buffer{ nullptr };
        std::size_t task_max_input_chars{ 1024 };
        bool task_editing{};
        std::span<const ConsoleWindowActionSpec> task_actions{};
        float task_action_gap{ 6.0f };
        std::span<const ConsoleWindowActionSpec> header_actions{};
        float header_action_gap{ 6.0f };
        std::span<const ConsoleWindowActionSpec> message_actions{};
        float message_action_gap{ 6.0f };
        std::span<const ConsoleWindowActionSpec> footer_actions{};
        float footer_action_gap{ 6.0f };
    };

    export struct ConsoleWindowResult
    {
        EditBoxResult input{};
        EditBoxResult task_input{};
        std::optional<std::size_t> task_action_index{};
        bool send_clicked{};
        std::optional<std::size_t> header_action_index{};
        std::optional<std::size_t> message_action_index{};
        std::optional<std::size_t> footer_action_index{};
    };

    export struct ScrollTextPanelOptions
    {
        std::string_view id{};
        Vec2 size{};
        const std::vector<std::string>& lines;
        std::span<const TextMessageRole> line_roles{};
        std::size_t max_line_chars{ 768 };
        bool selectable{ true };
        bool stick_to_bottom{ true };
        bool wrap_lines{ true };
    };

    export struct ScrollTextPanelResult
    {
        std::size_t first_visible_line{};
        std::optional<std::size_t> selected_line{};
        bool wheel_scrolled{};
    };

    export struct ScrollAreaOptions
    {
        std::string_view id{};
        Vec2 size{};
        float content_height{ 0.0f };
        bool draw_background{ false };
        bool show_scrollbar{ true };
        bool capture_wheel{ true };
    };

    export struct ScrollAreaResult
    {
        float scroll_y{};
        float content_height{};
        bool wheel_scrolled{};
    };

    export struct ModalWindowOptions
    {
        std::string_view title{};
        Vec2 position{};
        Vec2 size{};
        Vec2 viewport_size{};
        bool dim_background{ true };
    };

    export struct FloatingWindowState
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

    export struct FloatingWindowOptions
    {
        std::string_view id{};
        std::string_view title{};
        Vec2 default_position{};
        Vec2 default_size{};
        Vec2 min_size{ 160.0f, 96.0f };
        Vec2 viewport_size{};
        bool movable{ true };
        bool resizable{ true };
        bool closable{ true };
        bool draw_background{ true };
        bool top_layer{ true };
        bool capture_input{ true };
    };

    export struct FloatingWindowResult
    {
        bool begun{};
        bool focused{};
        bool hovered{};
        bool moved{};
        bool resized{};
        bool close_requested{};
        bool title_hovered{};
        bool close_hovered{};
        bool resize_hovered{};
        WidgetBounds window{};
        WidgetBounds title_bar{};
        WidgetBounds content{};
        WidgetBounds close_button{};
        WidgetBounds resize_handle{};
    };

    export enum class DockSlot : std::uint8_t
    {
        none,
        left,
        right,
        top,
        bottom,
        center
    };

    export enum class DockGuideTarget : std::uint8_t
    {
        none,
        left_tabs,
        right_tabs,
        bottom_left_tabs,
        bottom_right_tabs,
        left_context,
        right_context,
        float_window
    };

    export struct DockGuideOptions
    {
        WidgetBounds guide_bounds{};
        WidgetBounds left_tabs_preview{};
        WidgetBounds right_tabs_preview{};
        WidgetBounds bottom_left_tabs_preview{};
        WidgetBounds bottom_right_tabs_preview{};
        WidgetBounds left_context_preview{};
        WidgetBounds right_context_preview{};
        WidgetBounds floating_preview{};
        Vec2 pointer{};
        float guide_extent{ 94.0f };
        float guide_gap{ 8.0f };
        bool allow_side_tabs{ true };
        bool allow_bottom_tabs{ true };
        bool allow_contexts{};
        bool allow_float{ true };
        bool center_context_guides_in_previews{};
    };

    export struct DockGuide
    {
        DockGuideTarget target{ DockGuideTarget::none };
        WidgetBounds target_bounds{};
        WidgetBounds preview_bounds{};
        bool hovered{};
    };

    export struct DockGuideLayout
    {
        DockGuide guides[7]{};
        std::uint32_t count{};
        DockGuideTarget hovered_target{ DockGuideTarget::none };
        WidgetBounds hovered_preview{};
    };

    export struct DockGuideOverlayOptions
    {
        std::string_view moving_label{};
        std::string_view left_tabs_label{ "Left Tabs" };
        std::string_view right_tabs_label{ "Right Tabs" };
        std::string_view bottom_left_tabs_label{ "Bottom Left" };
        std::string_view bottom_right_tabs_label{ "Bottom Right" };
        std::string_view left_context_label{ "Left Context" };
        std::string_view right_context_label{ "Right Context" };
        std::string_view floating_label{ "Floating Window" };
        WidgetBounds floating_preview{};
        bool show_floating_preview{};
    };

    export enum class DockableWindowMode : std::uint8_t
    {
        docked,
        floating,
        detached
    };

    export enum class DockableWindowAction : std::uint8_t
    {
        none,
        focus,
        dock,
        float_window,
        detach,
        close
    };

    export struct DockableWindowHostState
    {
        std::uint32_t active_window_id{};
        std::uint32_t next_focus_order{ 1 };
        bool changed_this_frame{};
    };

    export struct DockableWindowState
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

    export struct DockableWindowOptions
    {
        std::string_view title{};
        WidgetBounds docked_frame{};
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

    export struct DockableWindowInput
    {
        Vec2 mouse_position{};
        bool mouse_down{};
        bool mouse_pressed{};
        bool mouse_released{};
        DockableWindowAction requested_action{ DockableWindowAction::none };
        DockSlot requested_dock_slot{ DockSlot::none };
    };

    export struct DockableWindowChrome
    {
        WidgetBounds frame{};
        WidgetBounds title_bar{};
        WidgetBounds content{};
        WidgetBounds dock_button{};
        WidgetBounds float_button{};
        WidgetBounds detach_button{};
        WidgetBounds close_button{};
        bool visible{};
        bool hovered{};
        bool title_hovered{};
        bool dock_hovered{};
        bool float_hovered{};
        bool detach_hovered{};
        bool close_hovered{};
        bool active{};
    };

    export struct DockableWindowResult
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

    export void begin_modal_input_capture(Vec2 position, Vec2 size) noexcept;
    export void block_input_until_clear() noexcept;
    export void clear_modal_input_capture() noexcept;

    export struct SegmentedButtonSpec
    {
        std::string_view label{};
        float width{};
        bool active{};
    };

    export enum class ImageFit : std::uint8_t
    {
        Stretch,
        Contain
    };

    export struct ImageBoxOptions
    {
        std::string_view id{};
        SpriteHandle sprite{};
        Vec2 size{};
        Vec2 source_size{};
        std::string_view caption{};
        ImageFit fit{ ImageFit::Contain };
        bool interactive{};
        bool selected{};
        bool enabled{ true };
    };

    export struct ImageBoxResult
    {
        WidgetBounds bounds{};
        WidgetBounds image_bounds{};
        bool hovered{};
        bool clicked{};
        bool valid{};
    };

    export struct ImageButtonOptions
    {
        std::string_view id{};
        SpriteHandle sprite{};
        Vec2 size{};
        Vec2 source_size{};
        ImageFit fit{ ImageFit::Contain };
        bool selected{};
        bool enabled{ true };
    };

    export enum class AssetGridItemKind : std::uint8_t
    {
        Generic,
        Folder,
        Image,
        Texture,
        Material,
        Model,
        Audio,
        Scene,
        Document
    };

    export struct AssetGridItem
    {
        std::uint64_t id{};
        std::string_view label{};
        std::string_view detail{};
        std::string_view search_terms{};
        SpriteHandle image{};
        Vec2 source_size{};
        AssetGridItemKind kind{ AssetGridItemKind::Generic };
        bool enabled{ true };
    };

    export inline constexpr std::size_t asset_grid_maximum_context_actions = 12u;

    export struct ContextMenuActionSpec
    {
        std::string_view id{};
        std::string_view label{};
        bool enabled{ true };
    };

    export struct AssetGridOptions
    {
        std::string_view id{};
        std::span<const AssetGridItem> items{};
        std::span<const ContextMenuActionSpec> context_actions{};
        std::string_view query{};
        Vec2 size{ 640.0f, 360.0f };
        Vec2 tile_size{ 144.0f, 164.0f };
        Vec2 gap{ 8.0f, 8.0f };
        Vec2 padding{ 8.0f, 8.0f };
        float image_height{ 108.0f };
        float scroll_offset{};
        std::optional<std::uint64_t> selected_id{};
        bool clear_selection_on_empty_press{ true };
        bool enabled{ true };
    };

    export struct AssetGridResult
    {
        WidgetBounds bounds{};
        std::optional<std::uint64_t> selected_id{};
        std::optional<std::uint64_t> activated_id{};
        std::optional<std::uint64_t> context_target_id{};
        std::optional<std::string> selected_action_id{};
        std::vector<std::uint64_t> visible_ids{};
        std::size_t matched_count{};
        std::size_t visible_count{};
        float content_height{};
        float scroll_offset{};
        bool selection_changed{};
        bool wheel_scrolled{};
        bool context_menu_open{};
        bool context_actions_truncated{};
        bool truncated{};
        bool valid{};
    };

    export enum class NodeGraphNodeRole : std::uint8_t
    {
        document,
        container,
        control,
        action
    };

    export struct NodeGraphCanvasNode
    {
        std::uint64_t id{};
        std::string_view title{};
        std::string_view subtitle{};
        Vec2 position{};
        Vec2 size{ 132.0f, 48.0f };
        NodeGraphNodeRole role{ NodeGraphNodeRole::control };
        bool selected{};
        bool enabled{ true };
    };

    export struct NodeGraphCanvasEdge
    {
        std::uint64_t id{};
        std::uint64_t source_node{};
        std::uint64_t target_node{};
        Vec2 source{};
        Vec2 target{};
        bool selected{};
        bool enabled{ true };
    };

    export struct NodeGraphCanvasOptions
    {
        std::string_view id{};
        Vec2 size{ 480.0f, 300.0f };
        std::span<const NodeGraphCanvasNode> nodes{};
        std::span<const NodeGraphCanvasEdge> edges{};
        float grid_step{ 24.0f };
        bool interactive{ true };
        bool allow_node_movement{ true };
        bool allow_connections{};
        bool allow_disconnection{};
        bool fit_to_content{};
        bool reset_view{};
        bool enabled{ true };
    };

    export struct NodeGraphCanvasNodeMove
    {
        std::uint64_t id{};
        Vec2 delta{};
    };

    export struct NodeGraphCanvasConnection
    {
        std::uint64_t source_node{};
        std::uint64_t target_node{};
    };

    export struct NodeGraphCanvasResult
    {
        WidgetBounds bounds{};
        std::optional<std::uint64_t> clicked_node{};
        std::optional<std::uint64_t> clicked_edge{};
        std::vector<std::uint64_t> selected_nodes{};
        std::vector<std::uint64_t> disconnected_edges{};
        std::vector<NodeGraphCanvasNodeMove> moved_nodes{};
        std::optional<NodeGraphCanvasConnection> connection{};
        Vec2 pan{};
        float zoom{ 1.0f };
        bool hovered{};
        bool clicked_background{};
        bool selection_changed{};
        bool view_changed{};
        bool layout_changed{};
        bool valid{};
    };

    export struct SliderOptions
    {
        std::string_view id{};
        std::string_view label{};
        float minimum{};
        float maximum{ 1.0f };
        float value{};
        float step{ 0.01f };
        Vec2 size{ 180.0f, 28.0f };
        bool enabled{ true };
    };

    export struct SliderResult
    {
        WidgetBounds bounds{};
        float value{};
        bool hovered{};
        bool changed{};
    };

    export inline constexpr std::size_t runtime_surface_batch_maximum_entries = 1'024u;
    export inline constexpr std::size_t runtime_surface_identifier_maximum_bytes = 240u;
    export inline constexpr std::uint32_t runtime_surface_maximum_extent = 4096u;
    export inline constexpr std::size_t runtime_surface_batch_maximum_rgba_bytes =
        64u * 1024u * 1024u;

    export struct RuntimeSurfaceDescriptor
    {
        std::string_view id{};
        std::span<const std::uint8_t> rgba_pixels{};
        std::uint32_t width{};
        std::uint32_t height{};
    };

    export enum class RuntimeSurfaceBatchStatus : std::uint8_t
    {
        Ready,
        Empty,
        BatchLimitExceeded,
        InvalidDescriptor,
        DuplicateIdentifier,
        ByteBudgetExceeded,
        ResourceUnavailable,
        AtlasMutationFailed,
        UploadFailed
    };

    export struct RuntimeSurfaceAtlasResult
    {
        std::vector<SpriteHandle> handles{};
        std::size_t descriptor_count{};
        std::size_t accepted_count{};
        std::size_t added_count{};
        std::size_t replaced_count{};
        std::size_t unchanged_count{};
        RuntimeSurfaceBatchStatus status{ RuntimeSurfaceBatchStatus::Empty };
        bool uploaded{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return status == RuntimeSurfaceBatchStatus::Ready
                && accepted_count == descriptor_count
                && handles.size() == descriptor_count
                && uploaded;
        }
    };
    export struct TabButtonSpec
    {
        std::string_view id{};
        std::string_view label{};
        float width{};
        bool active{};
        bool closable{};
        bool dirty{};
        bool enabled{ true };
    };

    export enum class TabBarPresentation : std::uint8_t
    {
        Document,
        Workbench
    };

    export struct TabBarResult
    {
        std::optional<std::size_t> pressed_index{};
        std::optional<std::size_t> selected_index{};
        std::optional<std::size_t> closed_index{};
    };

    export struct ResponsiveTabBarOptions
    {
        std::string_view overflow_id{ "tab-overflow" };
        std::string_view overflow_label{ "More" };
        std::span<const TabButtonSpec> tabs{};
        float available_width{};
        float overflow_width{ 120.0f };
        float height{ 30.0f };
        float gap{ 1.0f };
        TabBarPresentation presentation{ TabBarPresentation::Document };
    };

    export struct InlineButtonSpec
    {
        std::string_view label{};
        float width{};
        bool enabled{ true };
    };

    export struct SelectBoxOptions
    {
        std::string_view id{};
        std::string_view placeholder{ "Select..." };
        std::string_view selected{};
        std::span<const std::string_view> options{};
        Vec2 size{};
        float row_height{ 28.0f };
        std::size_t max_visible_options{ 8 };
    };

    export struct SelectBoxResult
    {
        bool opened{};
        bool changed{};
        std::optional<std::size_t> selected_index{};
    };

    export struct ProgressBarOptions
    {
        std::string_view label{};
        std::string_view status{};
        float value{};
        Vec2 size{};
        bool show_percent{ true };
        bool activity{ false };
        float activity_phase{ 0.0f };
    };

    export struct LoadingScreenOptions
    {
        std::string_view title{};
        std::string_view message{};
        std::string_view progress_label{};
        std::string_view progress_status{};
        float progress{};
        Vec2 viewport_position{};
        Vec2 viewport_size{};
        Vec2 panel_size{ 640.0f, 300.0f };
        float title_scale{ 0.0f };
        float message_scale{ 0.0f };
        float status_scale{ 0.0f };
        bool dim_background{ true };
        bool capture_input{ true };
        bool show_percent{ true };
        bool activity{ false };
        float activity_phase{ 0.0f };
        bool reserve_action_row{ true };
    };

    export struct LoadingScreenResult
    {
        WidgetBounds panel{};
        WidgetBounds progress{};
        WidgetBounds action{};
        bool visible{};
    };

    export void push_input(const InputEvent& e) noexcept;
    export void push_input_for_context(const core::Context* ctx, const InputEvent& e) noexcept;
    export int consume_mouse_wheel_delta() noexcept;
    export void cleanup_context(const core::Context* ctx) noexcept;
    export void refresh_context_resources(const core::Context* ctx) noexcept;
    export bool render_deferred_batch(core::Context* ctx) noexcept;
    export bool render_top_layer_batch(core::Context* ctx) noexcept;
    export std::uint64_t deferred_batch_generation(const core::Context* ctx) noexcept;
    export std::uint64_t top_layer_batch_generation(const core::Context* ctx) noexcept;
    export std::uint64_t replayed_top_layer_batch_generation(const core::Context* ctx) noexcept;

    export void begin_frame(const std::shared_ptr<core::Context>& ctx,
        float dt,
        Vec2 mouse_pos,
        bool mouse_down) noexcept;

    export void end_frame() noexcept;
    export Vec2 mouse_position() noexcept;
    export bool is_mouse_down() noexcept;
    export DragSurfaceResult drag_surface(
        DragSurfaceState& state,
        const DragSurfaceOptions& options) noexcept;
    export bool was_mouse_pressed() noexcept;
    export bool was_mouse_released() noexcept;
    export bool is_mouse_right_down() noexcept;
    export bool was_mouse_right_pressed() noexcept;
    export bool was_mouse_right_released() noexcept;
    export bool keyboard_input_captured() noexcept;

    export void begin_window(std::string_view title, Vec2 position, Vec2 size) noexcept;
    export void begin_window(std::string_view title, Vec2 position, Vec2 size, bool draw_background) noexcept;
    export void end_window() noexcept;
    export void begin_top_layer() noexcept;
    export void end_top_layer() noexcept;
    export void begin_modal_window(const ModalWindowOptions& options) noexcept;
    export void end_modal_window() noexcept;
    export FloatingWindowResult begin_floating_window(
        FloatingWindowState& state,
        const FloatingWindowOptions& options) noexcept;
    export void end_floating_window() noexcept;
    export DockGuideLayout make_dock_guide_layout(
        const DockGuideOptions& options) noexcept;
    export void render_dock_guide_overlay(
        const DockGuideLayout& layout,
        const DockGuideOverlayOptions& options = {}) noexcept;
    export DockableWindowResult update_dockable_window(
        DockableWindowHostState& host,
        DockableWindowState& state,
        const DockableWindowOptions& options,
        const DockableWindowInput& input) noexcept;
    export void focus_dockable_window(
        DockableWindowHostState& host,
        DockableWindowState& state) noexcept;
    export WidgetBounds scene_viewport(std::string_view title, Vec2 position, Vec2 size) noexcept;
    export void selection_outline(
        Vec2 position,
        Vec2 size,
        float thickness = 2.0f) noexcept;
    export void panel_rect(Vec2 position, Vec2 size) noexcept;
    export void titlebar_rect(Vec2 position, Vec2 size) noexcept;
    export void splitter_bar(Vec2 position, Vec2 size, bool hovered, bool active) noexcept;
    export std::span<const ThemePreferenceChoice> theme_preference_choices() noexcept;
    export std::string_view theme_preference_label(ThemePreference preference) noexcept;
    export ThemeVariant resolve_theme_preference(ThemePreference preference) noexcept;
    export void push_theme(ThemeVariant theme) noexcept;
    export void pop_theme() noexcept;
    export bool rounded_rectangles_enabled() noexcept;
    export void push_rounded_rectangles(bool enabled) noexcept;
    export void pop_rounded_rectangles() noexcept;

    export void set_cursor(Vec2 position) noexcept;
    export void advance_cursor(Vec2 delta) noexcept;

    export bool button(std::string_view label, Vec2 size) noexcept;
    export bool button_selected(std::string_view label, Vec2 size, bool selected) noexcept;
    export bool toggle_switch(std::string_view label, bool& value, Vec2 size = { 160.0f, 28.0f }) noexcept;
    export bool text_link(std::string_view label, Vec2 size, bool selected = false) noexcept;
    export bool titlebar_close_button(Vec2 window_position, Vec2 window_size) noexcept;
    export ImageBoxResult image_box(const ImageBoxOptions& options) noexcept;
    export bool image_button(const ImageButtonOptions& options) noexcept;
    export bool image_button(const SpriteHandle& sprite, Vec2 size) noexcept;
    export void image(const SpriteHandle& sprite, Vec2 size) noexcept;
    export AssetGridResult asset_grid(const AssetGridOptions& options) noexcept;
    export NodeGraphCanvasResult node_graph_canvas(
        const NodeGraphCanvasOptions& options) noexcept;
    export SliderResult slider(const SliderOptions& options) noexcept;
    export [[nodiscard]] SpriteHandle register_runtime_surface(
        std::string_view id,
        std::span<const std::uint8_t> rgba_pixels,
        std::uint32_t width,
        std::uint32_t height) noexcept;
    export [[nodiscard]] RuntimeSurfaceAtlasResult register_runtime_surface_atlas(
        std::span<const RuntimeSurfaceDescriptor> surfaces) noexcept;
    export [[nodiscard]] bool run_runtime_surface_contract() noexcept;
    export std::optional<std::size_t> segmented_button_row(
        std::span<const SegmentedButtonSpec> items,
        float height = 26.0f,
        float gap = 6.0f) noexcept;
    export std::optional<std::size_t> tab_bar(
        std::span<const SegmentedButtonSpec> tabs,
        float height = 28.0f,
        float gap = 2.0f) noexcept;
    export TabBarResult tab_bar_buttons(
        std::span<const TabButtonSpec> tabs,
        float height = 30.0f,
        float gap = 1.0f,
        TabBarPresentation presentation = TabBarPresentation::Document) noexcept;
    export TabBarResult responsive_tab_bar_buttons(
        const ResponsiveTabBarOptions& options) noexcept;
    export std::optional<std::size_t> inline_button_row(
        std::span<const InlineButtonSpec> items,
        float height = 24.0f,
        float gap = 6.0f) noexcept;
    export SelectBoxResult select_box(const SelectBoxOptions& options) noexcept;
    export void progress_bar(const ProgressBarOptions& options) noexcept;
    export LoadingScreenResult loading_screen(const LoadingScreenOptions& options) noexcept;

    export EditBoxResult edit_box(std::string& text,
        Vec2 size,
        std::size_t max_chars = 0,
        bool multiline = false) noexcept;
    export void select_all_text_in_edit_box(std::string& text) noexcept;
    export SourceEditorResult source_editor(std::string& text, const SourceEditorOptions& options) noexcept;
    export [[nodiscard]] std::string clipboard_text() noexcept;
    export bool set_clipboard_text(std::string_view text) noexcept;

    export void text_box(std::string_view text, Vec2 size) noexcept;
    export ScrollAreaResult begin_scroll_area(const ScrollAreaOptions& options) noexcept;
    export void end_scroll_area() noexcept;
    export ScrollTextPanelResult scroll_text_panel(const ScrollTextPanelOptions& options) noexcept;

    export ConsoleWindowResult console_window(const ConsoleWindowOptions& options) noexcept;

    export void label(std::string_view text) noexcept;
    export void wrapped_label(std::string_view text, float width = 0.0f) noexcept;
    export void property_row(std::string_view label, std::string_view value, float label_width = 152.0f) noexcept;
    export float wrapped_text_height(std::string_view text, float width = 0.0f) noexcept;
    export float measure_wrapped_label_height(std::string_view text, float width = 0.0f) noexcept;

    export float line_height() noexcept;
    export float glyph_width() noexcept;
    export float titled_window_total_height(float content_height) noexcept;
    export Vec2 cursor_position() noexcept;

    export std::optional<WidgetBounds> last_button_bounds() noexcept;
}
