#pragma once

#include "floating_window.hpp"

#include <cstdint>

namespace epochengine::gui_lib
{
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
    DockLayoutResult update_dock_layout(
        DockLayoutState& state,
        DockPaneState* panes,
        std::uint32_t pane_count,
        const DockLayoutOptions& options,
        const DockLayoutInput& input) noexcept;
}
