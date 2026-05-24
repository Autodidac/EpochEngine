/************************************************
 *  â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—  â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—  â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—â–ˆâ–ˆâ•—  â–ˆâ–ˆâ•—   *
 *  â–ˆâ–ˆâ•”â•â•â•â•â•â–ˆâ–ˆâ•”â•â•â–ˆâ–ˆâ•—â–ˆâ–ˆâ•”â•â•â•â–ˆâ–ˆâ•—â–ˆâ–ˆâ•”â•â•â•â•â•â–ˆâ–ˆâ•‘  â–ˆâ–ˆâ•‘   *
 *  â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—  â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•”â•â–ˆâ–ˆâ•‘   â–ˆâ–ˆâ•‘â–ˆâ–ˆâ•‘     â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•‘   *
 *  â–ˆâ–ˆâ•”â•â•â•  â–ˆâ–ˆâ•”â•â•â•â• â–ˆâ–ˆâ•‘   â–ˆâ–ˆâ•‘â–ˆâ–ˆâ•‘     â–ˆâ–ˆâ•”â•â•â–ˆâ–ˆâ•‘   *
 *  â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—â–ˆâ–ˆâ•‘     â•šâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•”â•â•šâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—â–ˆâ–ˆâ•‘  â–ˆâ–ˆâ•‘   *
 *  â•šâ•â•â•â•â•â•â•â•šâ•â•      â•šâ•â•â•â•â•â•  â•šâ•â•â•â•â•â•â•šâ•â•  â•šâ•â•   *
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

export module engine.gui;

import core.context;
import spritehandle;

namespace epochnamespace::gui
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
        ClassicLauncher
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

    export struct EditBoxResult
    {
        bool active{};
        bool changed{};
        bool submitted{};
    };

    export struct ConsoleWindowOptions
    {
        std::string_view title{};
        Vec2 position{};
        Vec2 size{};

        const std::vector<std::string>& lines;
        std::size_t max_visible_lines{ 200 };

        std::string* input{ nullptr };
        std::size_t max_input_chars{ 4096 };
        bool multiline_input{ false };
        bool show_send_button{ false };
        bool send_button_enabled{ true };
        float send_button_width{ 96.0f };
        std::string_view send_button_label{ "Send >" };
    };

    export struct ConsoleWindowResult
    {
        EditBoxResult input{};
        bool send_clicked{};
    };

    export struct ScrollTextPanelOptions
    {
        std::string_view id{};
        Vec2 size{};
        const std::vector<std::string>& lines;
        std::size_t max_line_chars{ 768 };
        bool selectable{ true };
        bool stick_to_bottom{ true };
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

    export struct SegmentedButtonSpec
    {
        std::string_view label{};
        float width{};
        bool active{};
    };

    export struct InlineButtonSpec
    {
        std::string_view label{};
        float width{};
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
    };

    export void push_input(const InputEvent& e) noexcept;
    export void push_input_for_context(const core::Context* ctx, const InputEvent& e) noexcept;
    export int consume_mouse_wheel_delta() noexcept;
    export void cleanup_context(const core::Context* ctx) noexcept;
    export bool render_deferred_batch(core::Context* ctx) noexcept;
    export bool render_top_layer_batch(core::Context* ctx) noexcept;
    export std::uint64_t deferred_batch_generation(const core::Context* ctx) noexcept;

    export void begin_frame(const std::shared_ptr<core::Context>& ctx,
        float dt,
        Vec2 mouse_pos,
        bool mouse_down) noexcept;

    export void end_frame() noexcept;
    export Vec2 mouse_position() noexcept;
    export bool is_mouse_down() noexcept;
    export bool was_mouse_pressed() noexcept;
    export bool was_mouse_released() noexcept;

    export void begin_window(std::string_view title, Vec2 position, Vec2 size) noexcept;
    export void begin_window(std::string_view title, Vec2 position, Vec2 size, bool draw_background) noexcept;
    export void end_window() noexcept;
    export void begin_top_layer() noexcept;
    export void end_top_layer() noexcept;
    export void begin_modal_window(const ModalWindowOptions& options) noexcept;
    export void end_modal_window() noexcept;
    export WidgetBounds scene_viewport(std::string_view title, Vec2 position, Vec2 size) noexcept;
    export void splitter_bar(Vec2 position, Vec2 size, bool hovered, bool active) noexcept;
    export void push_theme(ThemeVariant theme) noexcept;
    export void pop_theme() noexcept;

    export void set_cursor(Vec2 position) noexcept;
    export void advance_cursor(Vec2 delta) noexcept;

    export bool button(std::string_view label, Vec2 size) noexcept;
    export bool image_button(const SpriteHandle& sprite, Vec2 size) noexcept;
    export void image(const SpriteHandle& sprite, Vec2 size) noexcept;
    export [[nodiscard]] SpriteHandle register_runtime_surface(
        std::string_view id,
        std::span<const std::uint8_t> rgba_pixels,
        std::uint32_t width,
        std::uint32_t height) noexcept;
    export std::optional<std::size_t> segmented_button_row(
        std::span<const SegmentedButtonSpec> items,
        float height = 26.0f,
        float gap = 6.0f) noexcept;
    export std::optional<std::size_t> tab_bar(
        std::span<const SegmentedButtonSpec> tabs,
        float height = 28.0f,
        float gap = 2.0f) noexcept;
    export std::optional<std::size_t> inline_button_row(
        std::span<const InlineButtonSpec> items,
        float height = 24.0f,
        float gap = 6.0f) noexcept;
    export SelectBoxResult select_box(const SelectBoxOptions& options) noexcept;
    export void progress_bar(const ProgressBarOptions& options) noexcept;

    export EditBoxResult edit_box(std::string& text,
        Vec2 size,
        std::size_t max_chars = 0,
        bool multiline = false) noexcept;

    export void text_box(std::string_view text, Vec2 size) noexcept;
    export ScrollAreaResult begin_scroll_area(const ScrollAreaOptions& options) noexcept;
    export void end_scroll_area() noexcept;
    export ScrollTextPanelResult scroll_text_panel(const ScrollTextPanelOptions& options) noexcept;

    export ConsoleWindowResult console_window(const ConsoleWindowOptions& options) noexcept;

    export void label(std::string_view text) noexcept;
    export void wrapped_label(std::string_view text, float width = 0.0f) noexcept;
    export void property_row(std::string_view label, std::string_view value, float label_width = 152.0f) noexcept;
    export float wrapped_text_height(std::string_view text, float width = 0.0f) noexcept;

    export float line_height() noexcept;
    export float glyph_width() noexcept;
    export float titled_window_total_height(float content_height) noexcept;
    export Vec2 cursor_position() noexcept;

    export std::optional<WidgetBounds> last_button_bounds() noexcept;
}
