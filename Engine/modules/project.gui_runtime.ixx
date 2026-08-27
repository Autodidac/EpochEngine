/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

export module project.gui_runtime;

import asset.gui_artifact;

export namespace epochengine::project_gui_runtime
{
    struct Float2 final
    {
        float x{};
        float y{};

        friend constexpr bool operator==(Float2, Float2) noexcept = default;
    };

    struct Rect final
    {
        float x{};
        float y{};
        float width{};
        float height{};

        [[nodiscard]] constexpr bool empty() const noexcept
        {
            return width <= 0.0f || height <= 0.0f;
        }

        friend constexpr bool operator==(Rect, Rect) noexcept = default;
    };

    enum class RuntimeCode : std::uint8_t
    {
        ready,
        no_change,
        invalid_artifact,
        invalid_viewport,
        invalid_widget,
        hidden_widget,
        disabled_widget,
        wrong_widget_kind,
        read_only,
        text_limit_exceeded,
        invalid_value,
        action_limit_exceeded,
        allocation_failure
    };

    [[nodiscard]] constexpr std::string_view runtime_code_name(
        RuntimeCode code) noexcept
    {
        switch (code)
        {
        case RuntimeCode::ready: return "ready";
        case RuntimeCode::no_change: return "no_change";
        case RuntimeCode::invalid_artifact: return "invalid_artifact";
        case RuntimeCode::invalid_viewport: return "invalid_viewport";
        case RuntimeCode::invalid_widget: return "invalid_widget";
        case RuntimeCode::hidden_widget: return "hidden_widget";
        case RuntimeCode::disabled_widget: return "disabled_widget";
        case RuntimeCode::wrong_widget_kind: return "wrong_widget_kind";
        case RuntimeCode::read_only: return "read_only";
        case RuntimeCode::text_limit_exceeded: return "text_limit_exceeded";
        case RuntimeCode::invalid_value: return "invalid_value";
        case RuntimeCode::action_limit_exceeded:
            return "action_limit_exceeded";
        case RuntimeCode::allocation_failure: return "allocation_failure";
        }
        return "unknown";
    }

    enum class EventKind : std::uint8_t
    {
        activated,
        tab_changed,
        text_changed,
        slider_changed,
        focus_changed
    };

    struct RuntimeEvent final
    {
        EventKind kind{EventKind::activated};
        asset::gui::WidgetId widget{};
        asset::gui::WidgetId related{};
        std::string action{};
        std::string text{};
        double value{};

        friend bool operator==(
            const RuntimeEvent&,
            const RuntimeEvent&) noexcept = default;
    };

    struct WidgetView final
    {
        std::uint32_t index{asset::gui::invalid_widget_index};
        asset::gui::WidgetId id{};
        asset::gui::WidgetKind kind{asset::gui::WidgetKind::panel};
        Rect bounds{};
        Rect clip{};
        bool visible{};
        bool enabled{};
        bool focused{};
        bool accepts_pointer{};
        std::string_view name{};
        std::string_view primary_text{};
        std::string_view secondary_text{};
        std::string_view asset_path{};
        double value{};
        const asset::gui::Style* style{};
    };

    struct TabHeaderView final
    {
        std::uint32_t tab_set{asset::gui::invalid_widget_index};
        std::uint32_t page{asset::gui::invalid_widget_index};
        Rect bounds{};
        std::string_view label{};
        bool selected{};
        bool enabled{};
        bool closeable{};
    };

    struct RuntimeFrame final
    {
        RuntimeCode code{RuntimeCode::invalid_artifact};
        Float2 viewport{};
        float scale{1.0f};
        Float2 offset{};
        std::uint64_t revision{};
        std::vector<WidgetView> widgets{};
        std::vector<TabHeaderView> tab_headers{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == RuntimeCode::ready;
        }
    };

    struct RuntimeMetrics final
    {
        std::uint64_t artifact_replacements{};
        std::uint64_t frames_built{};
        std::uint64_t pointer_hits{};
        std::uint64_t focus_changes{};
        std::uint64_t tab_changes{};
        std::uint64_t text_changes{};
        std::uint64_t slider_changes{};
        std::uint64_t actions_emitted{};
        std::uint64_t rejected_operations{};
    };

    struct RuntimeLimits final
    {
        std::uint32_t maximum_pending_events{1'024u};
        std::uint32_t maximum_text_input_bytes{65'536u};
        asset::gui::ArtifactLimits artifact{};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return maximum_pending_events != 0u
                && maximum_text_input_bytes != 0u && artifact.valid();
        }
    };

    class RuntimeSession final
    {
    public:
        explicit RuntimeSession(RuntimeLimits limits = {}) noexcept;
        RuntimeSession(
            asset::gui::CompiledGuiArtifact artifact,
            RuntimeLimits limits = {}) noexcept;

        [[nodiscard]] bool valid() const noexcept;
        [[nodiscard]] const asset::gui::CompiledGuiArtifact* artifact() const noexcept;
        [[nodiscard]] RuntimeMetrics metrics() const noexcept;
        [[nodiscard]] asset::gui::WidgetId focused_widget() const noexcept;

        [[nodiscard]] RuntimeCode replace(
            asset::gui::CompiledGuiArtifact artifact) noexcept;
        [[nodiscard]] RuntimeFrame build_frame(Float2 viewport) noexcept;
        [[nodiscard]] bool pointer_captured(Float2 point) const noexcept;
        [[nodiscard]] bool keyboard_captured() const noexcept;
        [[nodiscard]] RuntimeCode pointer_release(Float2 point) noexcept;
        [[nodiscard]] RuntimeCode focus(asset::gui::WidgetId widget) noexcept;
        [[nodiscard]] RuntimeCode focus_next(bool reverse = false) noexcept;
        [[nodiscard]] RuntimeCode select_tab(
            asset::gui::WidgetId tabSet,
            asset::gui::WidgetId page) noexcept;
        [[nodiscard]] RuntimeCode set_text(
            asset::gui::WidgetId widget,
            std::string text) noexcept;
        [[nodiscard]] RuntimeCode append_text(
            asset::gui::WidgetId widget,
            std::string_view text) noexcept;
        [[nodiscard]] RuntimeCode erase_text_backward(
            asset::gui::WidgetId widget) noexcept;
        [[nodiscard]] RuntimeCode set_slider(
            asset::gui::WidgetId widget,
            double value) noexcept;
        [[nodiscard]] std::vector<RuntimeEvent> drain_events() noexcept;

    private:
        struct MutableState final
        {
            std::string text{};
            double slider_value{};
            float horizontal_offset{};
            float vertical_offset{};
            std::uint32_t selected_child{asset::gui::invalid_widget_index};
        };

        [[nodiscard]] std::optional<std::uint32_t> index_of(
            asset::gui::WidgetId widget) const noexcept;
        [[nodiscard]] bool runtime_visible(std::uint32_t index) const noexcept;
        [[nodiscard]] RuntimeCode reject(RuntimeCode code) noexcept;
        [[nodiscard]] RuntimeCode emit(RuntimeEvent event) noexcept;

        RuntimeLimits limits_{};
        asset::gui::CompiledGuiArtifact artifact_{};
        std::vector<MutableState> state_{};
        std::vector<RuntimeEvent> events_{};
        RuntimeFrame last_frame_{};
        std::uint32_t focused_{asset::gui::invalid_widget_index};
        std::uint64_t frame_revision_{};
        RuntimeMetrics metrics_{};
        bool valid_{};
    };

    enum class ContractFailure : std::uint8_t
    {
        none,
        valid_artifact_rejected,
        artifact_round_trip,
        corrupted_artifact_accepted,
        invalid_hierarchy_accepted,
        frame_projection,
        pointer_action,
        focus_order,
        text_editing,
        slider_editing,
        tab_selection,
        event_budget
    };

    [[nodiscard]] constexpr std::string_view contract_failure_name(
        ContractFailure failure) noexcept
    {
        switch (failure)
        {
        case ContractFailure::none: return "none";
        case ContractFailure::valid_artifact_rejected:
            return "valid_artifact_rejected";
        case ContractFailure::artifact_round_trip: return "artifact_round_trip";
        case ContractFailure::corrupted_artifact_accepted:
            return "corrupted_artifact_accepted";
        case ContractFailure::invalid_hierarchy_accepted:
            return "invalid_hierarchy_accepted";
        case ContractFailure::frame_projection: return "frame_projection";
        case ContractFailure::pointer_action: return "pointer_action";
        case ContractFailure::focus_order: return "focus_order";
        case ContractFailure::text_editing: return "text_editing";
        case ContractFailure::slider_editing: return "slider_editing";
        case ContractFailure::tab_selection: return "tab_selection";
        case ContractFailure::event_budget: return "event_budget";
        }
        return "unknown";
    }

    [[nodiscard]] ContractFailure run_contract() noexcept;
}
