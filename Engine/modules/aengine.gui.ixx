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
#include <string>
#include <string_view>
#include <vector>

export module aengine.gui;

import core.context;
import aspritehandle;

namespace epochnamespace::gui
{
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

    export enum class EventType : std::uint8_t
    {
        None = 0,
        MouseMove,
        MouseDown,
        MouseUp,
        KeyDown,
        KeyUp,
        TextInput
    };

    export struct InputEvent
    {
        EventType type{ EventType::None };
        Vec2 mouse_pos{};
        int key{};
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

    export void push_input(const InputEvent& e) noexcept;
    export void cleanup_context(const core::Context* ctx) noexcept;
    export bool render_deferred_batch(core::Context* ctx) noexcept;
    export std::uint64_t deferred_batch_generation(const core::Context* ctx) noexcept;

    export void begin_frame(const std::shared_ptr<core::Context>& ctx,
        float dt,
        Vec2 mouse_pos,
        bool mouse_down) noexcept;

    export void end_frame() noexcept;

    export void begin_window(std::string_view title, Vec2 position, Vec2 size) noexcept;
    export void end_window() noexcept;
    export WidgetBounds scene_viewport(std::string_view title, Vec2 position, Vec2 size) noexcept;

    export void set_cursor(Vec2 position) noexcept;
    export void advance_cursor(Vec2 delta) noexcept;

    export bool button(std::string_view label, Vec2 size) noexcept;
    export bool image_button(const SpriteHandle& sprite, Vec2 size) noexcept;

    export EditBoxResult edit_box(std::string& text,
        Vec2 size,
        std::size_t max_chars = 0,
        bool multiline = false) noexcept;

    export void text_box(std::string_view text, Vec2 size) noexcept;

    export ConsoleWindowResult console_window(const ConsoleWindowOptions& options) noexcept;

    export void label(std::string_view text) noexcept;
    export void wrapped_label(std::string_view text, float width = 0.0f) noexcept;
    export float wrapped_text_height(std::string_view text, float width = 0.0f) noexcept;

    export float line_height() noexcept;
    export float glyph_width() noexcept;

    export std::optional<WidgetBounds> last_button_bounds() noexcept;
}

