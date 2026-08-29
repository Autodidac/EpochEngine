/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <algorithm>
#include <cmath>
#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

export module authoring.gui_document;

import authoring.document;

export namespace epochengine::authoring::gui
{
    inline constexpr std::uint32_t kSchemaVersion = 1u;
    inline constexpr std::uint32_t kAppendPosition =
        (std::numeric_limits<std::uint32_t>::max)();

    struct WidgetTag final {};
    using WidgetHandle = authoring::Handle<WidgetTag, std::uint32_t>;
    using DocumentHandle = authoring::DocumentHandle;
    using DocumentRevision = authoring::DocumentRevision;
    using BranchIdentity = authoring::BranchIdentity;
    using OperationId = authoring::OperationId;

    enum class WidgetKind : std::uint8_t
    {
        canvas,
        panel,
        button,
        text,
        image,
        image_button,
        text_input,
        slider,
        scroll_area,
        tab_set,
        tab_page
    };

    [[nodiscard]] constexpr std::string_view widget_kind_name(
        WidgetKind kind) noexcept
    {
        switch (kind)
        {
        case WidgetKind::canvas: return "canvas";
        case WidgetKind::panel: return "panel";
        case WidgetKind::button: return "button";
        case WidgetKind::text: return "text";
        case WidgetKind::image: return "image";
        case WidgetKind::image_button: return "image_button";
        case WidgetKind::text_input: return "text_input";
        case WidgetKind::slider: return "slider";
        case WidgetKind::scroll_area: return "scroll_area";
        case WidgetKind::tab_set: return "tab_set";
        case WidgetKind::tab_page: return "tab_page";
        default: return "invalid";
        }
    }

    [[nodiscard]] constexpr bool valid(WidgetKind kind) noexcept
    {
        return kind >= WidgetKind::canvas && kind <= WidgetKind::tab_page;
    }

    enum class LayoutFlow : std::uint8_t
    {
        absolute,
        horizontal,
        vertical,
        overlay
    };

    enum class SizeRule : std::uint8_t
    {
        fixed,
        content,
        fill
    };

    struct Insets final
    {
        float left{};
        float top{};
        float right{};
        float bottom{};

        friend constexpr bool operator==(
            const Insets&,
            const Insets&) noexcept = default;
    };

    struct LayoutDescriptor final
    {
        LayoutFlow flow{LayoutFlow::absolute};
        SizeRule width_rule{SizeRule::fixed};
        SizeRule height_rule{SizeRule::fixed};
        float x{};
        float y{};
        float width{100.0f};
        float height{30.0f};
        float minimum_width{};
        float minimum_height{};
        float maximum_width{16'384.0f};
        float maximum_height{16'384.0f};
        float flex_grow{};
        float spacing{};
        Insets padding{};
        bool clip_children{};

        friend constexpr bool operator==(
            const LayoutDescriptor&,
            const LayoutDescriptor&) noexcept = default;
    };

    struct Color final
    {
        float red{};
        float green{};
        float blue{};
        float alpha{1.0f};

        friend constexpr bool operator==(
            const Color&,
            const Color&) noexcept = default;
    };

    struct StyleDescriptor final
    {
        Color background{0.16f, 0.17f, 0.19f, 1.0f};
        Color foreground{0.94f, 0.94f, 0.94f, 1.0f};
        Color border{0.32f, 0.34f, 0.38f, 1.0f};
        Color accent{0.20f, 0.56f, 0.86f, 1.0f};
        float border_width{1.0f};
        float corner_radius{2.0f};
        float opacity{1.0f};
        std::string style_class{};

        friend bool operator==(
            const StyleDescriptor&,
            const StyleDescriptor&) noexcept = default;
    };

    struct CanvasContent final
    {
        float virtual_width{1'280.0f};
        float virtual_height{720.0f};
        bool pixel_snap{true};

        friend constexpr bool operator==(
            const CanvasContent&,
            const CanvasContent&) noexcept = default;
    };

    struct PanelContent final
    {
        std::string title{};

        friend bool operator==(
            const PanelContent&,
            const PanelContent&) noexcept = default;
    };

    struct ButtonContent final
    {
        std::string label{};

        friend bool operator==(
            const ButtonContent&,
            const ButtonContent&) noexcept = default;
    };

    struct TextContent final
    {
        std::string text{};
        bool wrap{true};

        friend bool operator==(
            const TextContent&,
            const TextContent&) noexcept = default;
    };

    struct ImageContent final
    {
        std::string asset_path{};
        std::string alternative_text{};
        bool preserve_aspect{true};

        friend bool operator==(
            const ImageContent&,
            const ImageContent&) noexcept = default;
    };

    struct ImageButtonContent final
    {
        std::string asset_path{};
        std::string label{};
        bool preserve_aspect{true};

        friend bool operator==(
            const ImageButtonContent&,
            const ImageButtonContent&) noexcept = default;
    };

    struct TextInputContent final
    {
        std::string text{};
        std::string placeholder{};
        std::uint32_t maximum_length{4'096u};
        bool multiline{};
        bool read_only{};

        friend bool operator==(
            const TextInputContent&,
            const TextInputContent&) noexcept = default;
    };

    struct SliderContent final
    {
        std::string label{};
        double minimum{};
        double maximum{1.0};
        double value{};
        double step{0.01};

        friend bool operator==(
            const SliderContent&,
            const SliderContent&) noexcept = default;
    };

    struct ScrollAreaContent final
    {
        float horizontal_offset{};
        float vertical_offset{};
        bool allow_horizontal{};
        bool allow_vertical{true};

        friend constexpr bool operator==(
            const ScrollAreaContent&,
            const ScrollAreaContent&) noexcept = default;
    };

    struct TabSetContent final
    {
        WidgetHandle selected_page{};

        friend constexpr bool operator==(
            const TabSetContent&,
            const TabSetContent&) noexcept = default;
    };

    struct TabPageContent final
    {
        std::string label{};
        bool closeable{};

        friend bool operator==(
            const TabPageContent&,
            const TabPageContent&) noexcept = default;
    };

    using WidgetContent = std::variant<
        CanvasContent,
        PanelContent,
        ButtonContent,
        TextContent,
        ImageContent,
        ImageButtonContent,
        TextInputContent,
        SliderContent,
        ScrollAreaContent,
        TabSetContent,
        TabPageContent>;

    struct InteractionDescriptor final
    {
        bool enabled{true};
        bool visible{true};
        bool focusable{};
        bool accepts_pointer{};
        std::int32_t tab_index{-1};
        std::string action{};
        std::string tooltip{};

        friend bool operator==(
            const InteractionDescriptor&,
            const InteractionDescriptor&) noexcept = default;
    };

    struct WidgetDescriptor final
    {
        WidgetKind kind{WidgetKind::canvas};
        std::string name{};
        LayoutDescriptor layout{};
        StyleDescriptor style{};
        WidgetContent content{CanvasContent{}};
        InteractionDescriptor interaction{};

        friend bool operator==(
            const WidgetDescriptor&,
            const WidgetDescriptor&) noexcept = default;
    };

    struct DocumentLimits final
    {
        std::uint32_t maximum_active_widgets{4'096u};
        std::uint32_t maximum_widget_slots{16'384u};
        std::uint32_t maximum_children_per_widget{1'024u};
        std::uint32_t maximum_hierarchy_depth{256u};
        std::uint32_t maximum_name_bytes{256u};
        std::uint32_t maximum_text_bytes{65'536u};
        std::uint32_t maximum_asset_path_bytes{1'024u};
        std::uint32_t maximum_style_class_bytes{128u};
        std::uint32_t maximum_action_bytes{256u};
        std::uint32_t maximum_tooltip_bytes{4'096u};
        std::uint64_t maximum_total_string_bytes{64ull * 1024ull * 1024ull};
        std::uint32_t maximum_history_operations{4'096u};
        std::uint64_t maximum_history_bytes{128ull * 1024ull * 1024ull};
        float maximum_layout_extent{1'000'000.0f};

        friend constexpr bool operator==(
            const DocumentLimits&,
            const DocumentLimits&) noexcept = default;
    };

    enum class ResultCode : std::uint8_t
    {
        success,
        no_change,
        invalid_document,
        invalid_limits,
        invalid_handle,
        missing_widget,
        stale_handle,
        invalid_widget_kind,
        empty_name,
        invalid_text,
        string_limit_exceeded,
        total_string_limit_exceeded,
        invalid_layout,
        invalid_style,
        content_kind_mismatch,
        invalid_content,
        invalid_interaction,
        widget_limit_exceeded,
        widget_slot_limit_exceeded,
        child_limit_exceeded,
        hierarchy_depth_exceeded,
        parent_rejects_child,
        cycle_detected,
        invalid_order,
        unresolved_tab_page,
        history_operation_limit_exceeded,
        history_memory_limit_exceeded,
        nothing_to_undo,
        nothing_to_redo,
        revision_overflow,
        operation_id_overflow,
        malformed_hierarchy
    };

    [[nodiscard]] constexpr std::string_view result_code_name(
        ResultCode code) noexcept
    {
        switch (code)
        {
        case ResultCode::success: return "success";
        case ResultCode::no_change: return "no_change";
        case ResultCode::invalid_document: return "invalid_document";
        case ResultCode::invalid_limits: return "invalid_limits";
        case ResultCode::invalid_handle: return "invalid_handle";
        case ResultCode::missing_widget: return "missing_widget";
        case ResultCode::stale_handle: return "stale_handle";
        case ResultCode::invalid_widget_kind: return "invalid_widget_kind";
        case ResultCode::empty_name: return "empty_name";
        case ResultCode::invalid_text: return "invalid_text";
        case ResultCode::string_limit_exceeded:
            return "string_limit_exceeded";
        case ResultCode::total_string_limit_exceeded:
            return "total_string_limit_exceeded";
        case ResultCode::invalid_layout: return "invalid_layout";
        case ResultCode::invalid_style: return "invalid_style";
        case ResultCode::content_kind_mismatch:
            return "content_kind_mismatch";
        case ResultCode::invalid_content: return "invalid_content";
        case ResultCode::invalid_interaction: return "invalid_interaction";
        case ResultCode::widget_limit_exceeded:
            return "widget_limit_exceeded";
        case ResultCode::widget_slot_limit_exceeded:
            return "widget_slot_limit_exceeded";
        case ResultCode::child_limit_exceeded:
            return "child_limit_exceeded";
        case ResultCode::hierarchy_depth_exceeded:
            return "hierarchy_depth_exceeded";
        case ResultCode::parent_rejects_child:
            return "parent_rejects_child";
        case ResultCode::cycle_detected: return "cycle_detected";
        case ResultCode::invalid_order: return "invalid_order";
        case ResultCode::unresolved_tab_page: return "unresolved_tab_page";
        case ResultCode::history_operation_limit_exceeded:
            return "history_operation_limit_exceeded";
        case ResultCode::history_memory_limit_exceeded:
            return "history_memory_limit_exceeded";
        case ResultCode::nothing_to_undo: return "nothing_to_undo";
        case ResultCode::nothing_to_redo: return "nothing_to_redo";
        case ResultCode::revision_overflow: return "revision_overflow";
        case ResultCode::operation_id_overflow:
            return "operation_id_overflow";
        case ResultCode::malformed_hierarchy: return "malformed_hierarchy";
        default: return "unknown";
        }
    }

    struct ValidationIssue final
    {
        ResultCode code{ResultCode::success};
        WidgetHandle widget{};
        WidgetHandle related{};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return code != ResultCode::success;
        }

        friend constexpr bool operator==(
            const ValidationIssue&,
            const ValidationIssue&) noexcept = default;
    };

    struct MutationResult final
    {
        ResultCode code{ResultCode::invalid_document};
        WidgetHandle widget{};
        OperationId operation{};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return code == ResultCode::success;
        }
    };

    struct WidgetView final
    {
        WidgetHandle handle{};
        WidgetHandle parent{};
        WidgetDescriptor descriptor{};
        std::vector<WidgetHandle> children{};

        friend bool operator==(
            const WidgetView&,
            const WidgetView&) noexcept = default;
    };

    struct WidgetSnapshotState final
    {
        WidgetHandle parent{};
        WidgetDescriptor descriptor{};
        std::vector<WidgetHandle> children{};

        friend bool operator==(
            const WidgetSnapshotState&,
            const WidgetSnapshotState&) noexcept = default;
    };

    struct WidgetSlotSnapshot final
    {
        std::uint32_t generation{1u};
        std::optional<WidgetSnapshotState> widget{};

        friend bool operator==(
            const WidgetSlotSnapshot&,
            const WidgetSlotSnapshot&) noexcept = default;
    };

    struct GuiDocumentSnapshot final
    {
        std::uint32_t schema_version{kSchemaVersion};
        DocumentHandle document{};
        BranchIdentity branch{};
        DocumentRevision revision{};
        std::vector<WidgetSlotSnapshot> slots{};
        std::vector<WidgetHandle> roots{};

        friend bool operator==(
            const GuiDocumentSnapshot&,
            const GuiDocumentSnapshot&) noexcept = default;
    };

    enum class SnapshotCodecCode : std::uint8_t
    {
        ready,
        invalid_snapshot,
        serialized_budget_exceeded,
        malformed_input,
        unsupported_schema,
        bounded_state_exceeded,
        integrity_failure,
        allocation_failure
    };

    [[nodiscard]] constexpr std::string_view snapshot_codec_code_name(
        SnapshotCodecCode code) noexcept
    {
        switch (code)
        {
        case SnapshotCodecCode::ready: return "ready";
        case SnapshotCodecCode::invalid_snapshot: return "invalid_snapshot";
        case SnapshotCodecCode::serialized_budget_exceeded:
            return "serialized_budget_exceeded";
        case SnapshotCodecCode::malformed_input: return "malformed_input";
        case SnapshotCodecCode::unsupported_schema:
            return "unsupported_schema";
        case SnapshotCodecCode::bounded_state_exceeded:
            return "bounded_state_exceeded";
        case SnapshotCodecCode::integrity_failure:
            return "integrity_failure";
        case SnapshotCodecCode::allocation_failure:
            return "allocation_failure";
        }
        return "unknown";
    }

    struct SerializedGuiDocument final
    {
        SnapshotCodecCode code{SnapshotCodecCode::invalid_snapshot};
        std::vector<std::byte> bytes{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == SnapshotCodecCode::ready && !bytes.empty();
        }
    };

    struct DeserializedGuiDocument final
    {
        SnapshotCodecCode code{SnapshotCodecCode::malformed_input};
        std::optional<GuiDocumentSnapshot> snapshot{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == SnapshotCodecCode::ready
                && snapshot.has_value();
        }
    };

    [[nodiscard]] SerializedGuiDocument serialize_gui_document(
        const GuiDocumentSnapshot& snapshot,
        std::uint64_t maximumBytes =
            64ull * 1024ull * 1024ull) noexcept;
    [[nodiscard]] DeserializedGuiDocument deserialize_gui_document(
        std::span<const std::byte> bytes,
        const DocumentLimits& limits = {}) noexcept;

    struct WidgetCreatedOperation final
    {
        WidgetHandle widget{};
        WidgetHandle parent{};
        std::uint32_t position{};
        WidgetDescriptor descriptor{};
    };

    struct WidgetRemovedOperation final
    {
        WidgetHandle root{};
        std::vector<WidgetHandle> removed{};
    };

    struct WidgetReparentedOperation final
    {
        WidgetHandle widget{};
        WidgetHandle before_parent{};
        std::uint32_t before_position{};
        WidgetHandle after_parent{};
        std::uint32_t after_position{};
    };

    struct WidgetReorderedOperation final
    {
        WidgetHandle widget{};
        WidgetHandle parent{};
        std::uint32_t before_position{};
        std::uint32_t after_position{};
    };

    struct LayoutChangedOperation final
    {
        WidgetHandle widget{};
        LayoutDescriptor before{};
        LayoutDescriptor after{};
    };

    struct StyleChangedOperation final
    {
        WidgetHandle widget{};
        StyleDescriptor before{};
        StyleDescriptor after{};
    };

    struct ContentChangedOperation final
    {
        WidgetHandle widget{};
        WidgetContent before{};
        WidgetContent after{};
    };

    struct InteractionChangedOperation final
    {
        WidgetHandle widget{};
        InteractionDescriptor before{};
        InteractionDescriptor after{};
    };

    using OperationPayload = std::variant<
        WidgetCreatedOperation,
        WidgetRemovedOperation,
        WidgetReparentedOperation,
        WidgetReorderedOperation,
        LayoutChangedOperation,
        StyleChangedOperation,
        ContentChangedOperation,
        InteractionChangedOperation>;

    enum class OperationKind : std::uint8_t
    {
        widget_created,
        widget_removed,
        widget_reparented,
        widget_reordered,
        layout_changed,
        style_changed,
        content_changed,
        interaction_changed
    };

    [[nodiscard]] constexpr std::string_view operation_kind_name(
        OperationKind kind) noexcept
    {
        switch (kind)
        {
        case OperationKind::widget_created: return "widget_created";
        case OperationKind::widget_removed: return "widget_removed";
        case OperationKind::widget_reparented: return "widget_reparented";
        case OperationKind::widget_reordered: return "widget_reordered";
        case OperationKind::layout_changed: return "layout_changed";
        case OperationKind::style_changed: return "style_changed";
        case OperationKind::content_changed: return "content_changed";
        case OperationKind::interaction_changed:
            return "interaction_changed";
        default: return "unknown";
        }
    }

    [[nodiscard]] inline OperationKind operation_kind(
        const OperationPayload& payload) noexcept
    {
        return std::visit([](const auto& operation) noexcept
        {
            using Type = std::remove_cvref_t<decltype(operation)>;
            if constexpr (std::same_as<Type, WidgetCreatedOperation>)
                return OperationKind::widget_created;
            else if constexpr (std::same_as<Type, WidgetRemovedOperation>)
                return OperationKind::widget_removed;
            else if constexpr (std::same_as<Type, WidgetReparentedOperation>)
                return OperationKind::widget_reparented;
            else if constexpr (std::same_as<Type, WidgetReorderedOperation>)
                return OperationKind::widget_reordered;
            else if constexpr (std::same_as<Type, LayoutChangedOperation>)
                return OperationKind::layout_changed;
            else if constexpr (std::same_as<Type, StyleChangedOperation>)
                return OperationKind::style_changed;
            else if constexpr (std::same_as<Type, ContentChangedOperation>)
                return OperationKind::content_changed;
            else
                return OperationKind::interaction_changed;
        }, payload);
    }

    struct OperationRecord final
    {
        OperationId id{};
        OperationPayload payload{};
        authoring::ContentHash before_content{};
        authoring::ContentHash after_content{};
        std::uint64_t retained_bytes{};
        bool applied{};

        [[nodiscard]] OperationKind kind() const noexcept
        {
            return operation_kind(payload);
        }
    };

    struct DocumentMetrics final
    {
        std::uint32_t active_widgets{};
        std::uint32_t allocated_slots{};
        std::uint32_t root_widgets{};
        std::uint32_t maximum_depth{};
        std::uint32_t retained_operations{};
        std::uint32_t applied_operations{};
        std::uint64_t retained_history_bytes{};
        std::uint64_t total_string_bytes{};
        std::uint64_t revision_sequence{};

        friend constexpr bool operator==(
            const DocumentMetrics&,
            const DocumentMetrics&) noexcept = default;
    };

    class GuiDocument final
    {
    public:
        explicit GuiDocument(
            DocumentHandle document,
            BranchIdentity branch,
            DocumentLimits limits = {})
            : document_(document)
            , branch_(branch)
            , limits_(limits)
        {
            valid_ = document_.valid()
                && branch_.valid()
                && valid_limits(limits_);
            if (valid_)
                advance_revision();
        }

        explicit GuiDocument(
            GuiDocumentSnapshot snapshot,
            DocumentLimits limits = {});
        [[nodiscard]] bool valid() const noexcept
        {
            return valid_;
        }

        [[nodiscard]] const DocumentHandle& document() const noexcept
        {
            return document_;
        }

        [[nodiscard]] const BranchIdentity& branch() const noexcept
        {
            return branch_;
        }

        [[nodiscard]] const DocumentLimits& limits() const noexcept
        {
            return limits_;
        }

        [[nodiscard]] const DocumentRevision& revision() const noexcept
        {
            return revision_;
        }

        [[nodiscard]] std::span<const WidgetHandle> roots() const noexcept
        {
            return state_.roots;
        }

        [[nodiscard]] std::vector<WidgetView> widgets() const
        {
            std::vector<WidgetView> result{};
            result.reserve(active_widget_count(state_));
            for (std::uint32_t index = 0u; index < state_.slots.size(); ++index)
            {
                const WidgetSlot& slot = state_.slots[index];
                if (!slot.widget)
                    continue;
                result.push_back(WidgetView{
                    WidgetHandle{index, slot.generation},
                    slot.widget->parent,
                    slot.widget->descriptor,
                    slot.widget->children
                });
            }
            return result;
        }

        [[nodiscard]] std::optional<WidgetView> widget(
            WidgetHandle handle) const
        {
            if (widget_status(state_, handle) != ResultCode::success)
                return std::nullopt;
            const WidgetState& value = *state_.slots[handle.index].widget;
            return WidgetView{
                handle,
                value.parent,
                value.descriptor,
                value.children
            };
        }

        [[nodiscard]] MutationResult create_widget(
            WidgetDescriptor descriptor,
            WidgetHandle parent = {},
            std::uint32_t position = kAppendPosition)
        {
            if (!valid_)
                return failure(ResultCode::invalid_document);
            if (const ResultCode code = validate_descriptor(descriptor);
                code != ResultCode::success)
            {
                return failure(code);
            }
            if (active_widget_count(state_)
                >= limits_.maximum_active_widgets)
            {
                return failure(ResultCode::widget_limit_exceeded);
            }
            if (const ResultCode code = validate_parent_for_kind(
                    state_, parent, descriptor.kind);
                code != ResultCode::success)
            {
                return failure(code, parent);
            }

            State candidate = state_;
            std::vector<WidgetHandle>& siblings = sibling_list(candidate, parent);
            const std::uint32_t insertion = resolved_insert_position(
                position,
                siblings.size());
            if (insertion == kAppendPosition)
                return failure(ResultCode::invalid_order, parent);
            if (siblings.size() >= limits_.maximum_children_per_widget)
                return failure(ResultCode::child_limit_exceeded, parent);

            WidgetHandle handle{};
            for (std::uint32_t index = 0u; index < candidate.slots.size(); ++index)
            {
                WidgetSlot& slot = candidate.slots[index];
                if (slot.widget)
                    continue;
                std::uint32_t generation = slot.generation;
                const std::uint32_t issued = index < issued_generations_.size()
                    ? issued_generations_[index]
                    : 0u;
                if (issued >= generation)
                    generation = authoring::next_generation(issued);
                slot.generation = generation;
                slot.widget = WidgetState{
                    parent,
                    std::move(descriptor),
                    {}};
                handle = WidgetHandle{index, generation};
                break;
            }
            if (!handle)
            {
                if (candidate.slots.size() >= limits_.maximum_widget_slots)
                    return failure(ResultCode::widget_slot_limit_exceeded);
                const std::uint32_t index = static_cast<std::uint32_t>(
                    candidate.slots.size());
                const std::uint32_t issued = index < issued_generations_.size()
                    ? issued_generations_[index]
                    : 0u;
                const std::uint32_t generation = authoring::next_generation(
                    issued);
                candidate.slots.push_back(WidgetSlot{
                    generation,
                    WidgetState{parent, std::move(descriptor), {}}});
                handle = WidgetHandle{index, generation};
            }

            std::vector<WidgetHandle>& updatedSiblings =
                sibling_list(candidate, parent);
            updatedSiblings.insert(
                updatedSiblings.begin()
                    + static_cast<std::ptrdiff_t>(insertion),
                handle);

            if (const ValidationIssue issue = validate_state(candidate); issue)
                return failure(issue.code, issue.widget);

            const WidgetDescriptor committedDescriptor =
                candidate.slots[handle.index].widget->descriptor;
            const WidgetCreatedOperation operation{
                handle,
                parent,
                insertion,
                committedDescriptor
            };
            if (const ResultCode code = can_commit(candidate, operation);
                code != ResultCode::success)
            {
                return failure(code, handle);
            }
            ensure_generation_capacity(handle.index + 1u);
            issued_generations_[handle.index] = handle.generation;
            return commit(operation, std::move(candidate), handle);
        }

        [[nodiscard]] MutationResult remove_widget(WidgetHandle handle)
        {
            if (!valid_)
                return failure(ResultCode::invalid_document, handle);
            if (const ResultCode code = widget_status(state_, handle);
                code != ResultCode::success)
            {
                return failure(code, handle);
            }

            State candidate = state_;
            const WidgetHandle parent =
                candidate.slots[handle.index].widget->parent;
            std::vector<WidgetHandle>& siblings = sibling_list(candidate, parent);
            const auto found = std::find(siblings.begin(), siblings.end(), handle);
            if (found == siblings.end())
                return failure(ResultCode::malformed_hierarchy, handle);
            siblings.erase(found);

            const std::vector<WidgetHandle> removed =
                subtree_handles(candidate, handle);
            for (const WidgetHandle removedHandle : removed)
            {
                WidgetSlot& slot = candidate.slots[removedHandle.index];
                slot.widget.reset();
                slot.generation = authoring::next_generation(slot.generation);
            }
            clear_unresolved_tab_selections(candidate);

            if (const ValidationIssue issue = validate_state(candidate); issue)
                return failure(issue.code, issue.widget);

            const WidgetRemovedOperation operation{handle, removed};
            if (const ResultCode code = can_commit(candidate, operation);
                code != ResultCode::success)
            {
                return failure(code, handle);
            }
            return commit(operation, std::move(candidate), handle);
        }

        [[nodiscard]] MutationResult reparent_widget(
            WidgetHandle handle,
            WidgetHandle parent,
            std::uint32_t position = kAppendPosition)
        {
            if (!valid_)
                return failure(ResultCode::invalid_document, handle);
            if (const ResultCode code = widget_status(state_, handle);
                code != ResultCode::success)
            {
                return failure(code, handle);
            }
            if (parent == handle)
                return failure(ResultCode::cycle_detected, handle);
            if (parent && is_descendant(state_, parent, handle))
                return failure(ResultCode::cycle_detected, handle);

            const WidgetState& current = *state_.slots[handle.index].widget;
            if (const ResultCode code = validate_parent_for_kind(
                    state_, parent, current.descriptor.kind);
                code != ResultCode::success)
            {
                return failure(code, parent);
            }

            State candidate = state_;
            const WidgetHandle beforeParent = current.parent;
            std::vector<WidgetHandle>& beforeSiblings =
                sibling_list(candidate, beforeParent);
            const auto found = std::find(
                beforeSiblings.begin(), beforeSiblings.end(), handle);
            if (found == beforeSiblings.end())
                return failure(ResultCode::malformed_hierarchy, handle);
            const std::uint32_t beforePosition =
                static_cast<std::uint32_t>(found - beforeSiblings.begin());
            beforeSiblings.erase(found);

            std::vector<WidgetHandle>& afterSiblings =
                sibling_list(candidate, parent);
            const std::uint32_t afterPosition = resolved_insert_position(
                position,
                afterSiblings.size());
            if (afterPosition == kAppendPosition)
                return failure(ResultCode::invalid_order, handle);
            if (afterSiblings.size() >= limits_.maximum_children_per_widget)
                return failure(ResultCode::child_limit_exceeded, parent);
            afterSiblings.insert(
                afterSiblings.begin()
                    + static_cast<std::ptrdiff_t>(afterPosition),
                handle);
            candidate.slots[handle.index].widget->parent = parent;
            clear_unresolved_tab_selections(candidate);

            if (candidate == state_)
                return failure(ResultCode::no_change, handle);
            if (const ValidationIssue issue = validate_state(candidate); issue)
                return failure(issue.code, issue.widget);

            const WidgetReparentedOperation operation{
                handle,
                beforeParent,
                beforePosition,
                parent,
                afterPosition
            };
            if (const ResultCode code = can_commit(candidate, operation);
                code != ResultCode::success)
            {
                return failure(code, handle);
            }
            return commit(operation, std::move(candidate), handle);
        }

        [[nodiscard]] MutationResult reorder_widget(
            WidgetHandle handle,
            std::uint32_t position)
        {
            if (!valid_)
                return failure(ResultCode::invalid_document, handle);
            if (const ResultCode code = widget_status(state_, handle);
                code != ResultCode::success)
            {
                return failure(code, handle);
            }
            State candidate = state_;
            const WidgetHandle parent =
                candidate.slots[handle.index].widget->parent;
            std::vector<WidgetHandle>& siblings = sibling_list(candidate, parent);
            if (position >= siblings.size())
                return failure(ResultCode::invalid_order, handle);
            const auto found = std::find(siblings.begin(), siblings.end(), handle);
            if (found == siblings.end())
                return failure(ResultCode::malformed_hierarchy, handle);
            const std::uint32_t beforePosition =
                static_cast<std::uint32_t>(found - siblings.begin());
            if (beforePosition == position)
                return failure(ResultCode::no_change, handle);
            siblings.erase(found);
            siblings.insert(
                siblings.begin() + static_cast<std::ptrdiff_t>(position),
                handle);

            if (const ValidationIssue issue = validate_state(candidate); issue)
                return failure(issue.code, issue.widget);

            const WidgetReorderedOperation operation{
                handle,
                parent,
                beforePosition,
                position
            };
            if (const ResultCode code = can_commit(candidate, operation);
                code != ResultCode::success)
            {
                return failure(code, handle);
            }
            return commit(operation, std::move(candidate), handle);
        }

        [[nodiscard]] MutationResult update_layout(
            WidgetHandle handle,
            LayoutDescriptor layout)
        {
            return update_descriptor_part(
                handle,
                std::move(layout),
                [](WidgetDescriptor& descriptor) -> LayoutDescriptor&
                {
                    return descriptor.layout;
                });
        }

        [[nodiscard]] MutationResult update_style(
            WidgetHandle handle,
            StyleDescriptor style)
        {
            return update_descriptor_part(
                handle,
                std::move(style),
                [](WidgetDescriptor& descriptor) -> StyleDescriptor&
                {
                    return descriptor.style;
                });
        }

        [[nodiscard]] MutationResult update_content(
            WidgetHandle handle,
            WidgetContent content)
        {
            return update_descriptor_part(
                handle,
                std::move(content),
                [](WidgetDescriptor& descriptor) -> WidgetContent&
                {
                    return descriptor.content;
                });
        }

        [[nodiscard]] MutationResult update_interaction(
            WidgetHandle handle,
            InteractionDescriptor interaction)
        {
            return update_descriptor_part(
                handle,
                std::move(interaction),
                [](WidgetDescriptor& descriptor) -> InteractionDescriptor&
                {
                    return descriptor.interaction;
                });
        }

        [[nodiscard]] bool can_undo() const noexcept
        {
            return valid_ && history_cursor_ != 0u;
        }

        [[nodiscard]] bool can_redo() const noexcept
        {
            return valid_ && history_cursor_ < history_.size();
        }

        [[nodiscard]] MutationResult undo()
        {
            if (!valid_)
                return failure(ResultCode::invalid_document);
            if (!can_undo())
                return failure(ResultCode::nothing_to_undo);
            if (revision_sequence_
                == (std::numeric_limits<std::uint64_t>::max)())
            {
                return failure(ResultCode::revision_overflow);
            }
            --history_cursor_;
            const HistoryEntry& entry = history_[history_cursor_];
            state_ = entry.before;
            advance_revision();
            return MutationResult{
                ResultCode::success,
                primary_widget(entry.record.payload),
                entry.record.id
            };
        }

        [[nodiscard]] MutationResult redo()
        {
            if (!valid_)
                return failure(ResultCode::invalid_document);
            if (!can_redo())
                return failure(ResultCode::nothing_to_redo);
            if (revision_sequence_
                == (std::numeric_limits<std::uint64_t>::max)())
            {
                return failure(ResultCode::revision_overflow);
            }
            const HistoryEntry& entry = history_[history_cursor_];
            state_ = entry.after;
            ++history_cursor_;
            advance_revision();
            return MutationResult{
                ResultCode::success,
                primary_widget(entry.record.payload),
                entry.record.id
            };
        }

        [[nodiscard]] std::vector<OperationRecord> journal() const
        {
            std::vector<OperationRecord> result{};
            result.reserve(history_.size());
            for (std::size_t index = 0u; index < history_.size(); ++index)
            {
                OperationRecord record = history_[index].record;
                record.applied = index < history_cursor_;
                result.push_back(std::move(record));
            }
            return result;
        }

        [[nodiscard]] ValidationIssue validate() const
        {
            if (!valid_)
                return {ResultCode::invalid_document, {}, {}};
            return validate_state(state_);
        }

        [[nodiscard]] GuiDocumentSnapshot snapshot() const
        {
            GuiDocumentSnapshot result{};
            result.document = document_;
            result.branch = branch_;
            result.revision = revision_;
            result.roots = state_.roots;
            result.slots.reserve(state_.slots.size());
            for (const WidgetSlot& slot : state_.slots)
            {
                WidgetSlotSnapshot output{};
                output.generation = slot.generation;
                if (slot.widget)
                {
                    output.widget = WidgetSnapshotState{
                        slot.widget->parent,
                        slot.widget->descriptor,
                        slot.widget->children
                    };
                }
                result.slots.push_back(std::move(output));
            }
            return result;
        }

        [[nodiscard]] DocumentMetrics metrics() const noexcept
        {
            DocumentMetrics result{};
            result.active_widgets = active_widget_count(state_);
            result.allocated_slots = static_cast<std::uint32_t>(
                state_.slots.size());
            result.root_widgets = static_cast<std::uint32_t>(
                state_.roots.size());
            result.maximum_depth = maximum_depth(state_);
            result.retained_operations = static_cast<std::uint32_t>(
                history_.size());
            result.applied_operations = static_cast<std::uint32_t>(
                history_cursor_);
            result.retained_history_bytes = retained_history_bytes_;
            result.total_string_bytes = total_string_bytes(state_);
            result.revision_sequence = revision_sequence_;
            return result;
        }

        [[nodiscard]] ResultCode validate_descriptor(
            const WidgetDescriptor& descriptor) const noexcept
        {
            if (!epochengine::authoring::gui::valid(descriptor.kind))
                return ResultCode::invalid_widget_kind;
            if (descriptor.name.empty())
                return ResultCode::empty_name;
            if (const ResultCode code = validate_string(
                    descriptor.name,
                    limits_.maximum_name_bytes,
                    false);
                code != ResultCode::success)
            {
                return code;
            }
            if (!valid_layout(descriptor.layout))
                return ResultCode::invalid_layout;
            if (!valid_style(descriptor.style))
                return ResultCode::invalid_style;
            if (const ResultCode code = validate_content(
                    descriptor.kind,
                    descriptor.content);
                code != ResultCode::success)
            {
                return code;
            }
            if (const ResultCode code = validate_interaction(
                    descriptor.interaction);
                code != ResultCode::success)
            {
                return code;
            }
            return ResultCode::success;
        }

    private:
        struct WidgetState final
        {
            WidgetHandle parent{};
            WidgetDescriptor descriptor{};
            std::vector<WidgetHandle> children{};

            friend bool operator==(
                const WidgetState&,
                const WidgetState&) noexcept = default;
        };

        struct WidgetSlot final
        {
            std::uint32_t generation{1u};
            std::optional<WidgetState> widget{};

            friend bool operator==(
                const WidgetSlot&,
                const WidgetSlot&) noexcept = default;
        };

        struct State final
        {
            std::vector<WidgetSlot> slots{};
            std::vector<WidgetHandle> roots{};

            friend bool operator==(
                const State&,
                const State&) noexcept = default;
        };

        struct HistoryEntry final
        {
            OperationRecord record{};
            State before{};
            State after{};
        };

        [[nodiscard]] static bool finite(float value) noexcept
        {
            return std::isfinite(value);
        }

        [[nodiscard]] static bool finite(double value) noexcept
        {
            return std::isfinite(value);
        }

        [[nodiscard]] static bool valid_limits(
            const DocumentLimits& limits) noexcept
        {
            return limits.maximum_active_widgets != 0u
                && limits.maximum_widget_slots
                    >= limits.maximum_active_widgets
                && limits.maximum_children_per_widget != 0u
                && limits.maximum_children_per_widget
                    <= limits.maximum_active_widgets
                && limits.maximum_hierarchy_depth != 0u
                && limits.maximum_hierarchy_depth
                    <= limits.maximum_active_widgets
                && limits.maximum_name_bytes != 0u
                && limits.maximum_text_bytes != 0u
                && limits.maximum_asset_path_bytes != 0u
                && limits.maximum_style_class_bytes != 0u
                && limits.maximum_action_bytes != 0u
                && limits.maximum_tooltip_bytes != 0u
                && limits.maximum_total_string_bytes != 0u
                && limits.maximum_history_operations != 0u
                && limits.maximum_history_bytes != 0u
                && finite(limits.maximum_layout_extent)
                && limits.maximum_layout_extent > 0.0f;
        }

        [[nodiscard]] ResultCode validate_string(
            std::string_view text,
            std::uint64_t limit,
            bool allowLineBreaks) const noexcept
        {
            if (text.size() > limit)
                return ResultCode::string_limit_exceeded;
            for (const unsigned char character : text)
            {
                if (character == 0u)
                    return ResultCode::invalid_text;
                if (character < 0x20u
                    && character != static_cast<unsigned char>('\t')
                    && (!allowLineBreaks
                        || (character != static_cast<unsigned char>('\n')
                            && character
                                != static_cast<unsigned char>('\r'))))
                {
                    return ResultCode::invalid_text;
                }
            }
            return ResultCode::success;
        }

        [[nodiscard]] bool valid_layout(
            const LayoutDescriptor& layout) const noexcept
        {
            if (layout.flow < LayoutFlow::absolute
                || layout.flow > LayoutFlow::overlay
                || layout.width_rule < SizeRule::fixed
                || layout.width_rule > SizeRule::fill
                || layout.height_rule < SizeRule::fixed
                || layout.height_rule > SizeRule::fill)
            {
                return false;
            }
            const float values[]{
                layout.x,
                layout.y,
                layout.width,
                layout.height,
                layout.minimum_width,
                layout.minimum_height,
                layout.maximum_width,
                layout.maximum_height,
                layout.flex_grow,
                layout.spacing,
                layout.padding.left,
                layout.padding.top,
                layout.padding.right,
                layout.padding.bottom
            };
            for (const float value : values)
            {
                if (!finite(value)
                    || std::abs(value) > limits_.maximum_layout_extent)
                {
                    return false;
                }
            }
            return layout.width >= 0.0f
                && layout.height >= 0.0f
                && layout.minimum_width >= 0.0f
                && layout.minimum_height >= 0.0f
                && layout.maximum_width >= layout.minimum_width
                && layout.maximum_height >= layout.minimum_height
                && layout.width >= layout.minimum_width
                && layout.width <= layout.maximum_width
                && layout.height >= layout.minimum_height
                && layout.height <= layout.maximum_height
                && layout.flex_grow >= 0.0f
                && layout.spacing >= 0.0f
                && layout.padding.left >= 0.0f
                && layout.padding.top >= 0.0f
                && layout.padding.right >= 0.0f
                && layout.padding.bottom >= 0.0f;
        }

        [[nodiscard]] static bool valid_color(Color color) noexcept
        {
            return finite(color.red) && color.red >= 0.0f && color.red <= 1.0f
                && finite(color.green) && color.green >= 0.0f
                && color.green <= 1.0f
                && finite(color.blue) && color.blue >= 0.0f
                && color.blue <= 1.0f
                && finite(color.alpha) && color.alpha >= 0.0f
                && color.alpha <= 1.0f;
        }

        [[nodiscard]] bool valid_style(
            const StyleDescriptor& style) const noexcept
        {
            if (!valid_color(style.background)
                || !valid_color(style.foreground)
                || !valid_color(style.border)
                || !valid_color(style.accent)
                || !finite(style.border_width)
                || !finite(style.corner_radius)
                || !finite(style.opacity)
                || style.border_width < 0.0f
                || style.border_width > limits_.maximum_layout_extent
                || style.corner_radius < 0.0f
                || style.corner_radius > limits_.maximum_layout_extent
                || style.opacity < 0.0f
                || style.opacity > 1.0f)
            {
                return false;
            }
            return validate_string(
                style.style_class,
                limits_.maximum_style_class_bytes,
                false) == ResultCode::success;
        }

        [[nodiscard]] ResultCode validate_content(
            WidgetKind kind,
            const WidgetContent& content) const noexcept
        {
            if (static_cast<std::size_t>(kind) != content.index())
                return ResultCode::content_kind_mismatch;

            return std::visit([this](const auto& value) -> ResultCode
            {
                using Type = std::remove_cvref_t<decltype(value)>;
                if constexpr (std::same_as<Type, CanvasContent>)
                {
                    return finite(value.virtual_width)
                        && finite(value.virtual_height)
                        && value.virtual_width > 0.0f
                        && value.virtual_height > 0.0f
                        && value.virtual_width <= limits_.maximum_layout_extent
                        && value.virtual_height <= limits_.maximum_layout_extent
                            ? ResultCode::success
                            : ResultCode::invalid_content;
                }
                else if constexpr (std::same_as<Type, PanelContent>)
                {
                    return validate_string(
                        value.title,
                        limits_.maximum_text_bytes,
                        false);
                }
                else if constexpr (std::same_as<Type, ButtonContent>)
                {
                    if (value.label.empty())
                        return ResultCode::invalid_content;
                    return validate_string(
                        value.label,
                        limits_.maximum_text_bytes,
                        false);
                }
                else if constexpr (std::same_as<Type, TextContent>)
                {
                    return validate_string(
                        value.text,
                        limits_.maximum_text_bytes,
                        true);
                }
                else if constexpr (std::same_as<Type, ImageContent>)
                {
                    if (value.asset_path.empty())
                        return ResultCode::invalid_content;
                    if (const ResultCode code = validate_string(
                            value.asset_path,
                            limits_.maximum_asset_path_bytes,
                            false);
                        code != ResultCode::success)
                    {
                        return code;
                    }
                    return validate_string(
                        value.alternative_text,
                        limits_.maximum_text_bytes,
                        false);
                }
                else if constexpr (std::same_as<Type, ImageButtonContent>)
                {
                    if (value.asset_path.empty())
                        return ResultCode::invalid_content;
                    if (const ResultCode code = validate_string(
                            value.asset_path,
                            limits_.maximum_asset_path_bytes,
                            false);
                        code != ResultCode::success)
                    {
                        return code;
                    }
                    return validate_string(
                        value.label,
                        limits_.maximum_text_bytes,
                        false);
                }
                else if constexpr (std::same_as<Type, TextInputContent>)
                {
                    if (value.maximum_length == 0u
                        || value.maximum_length > limits_.maximum_text_bytes
                        || value.text.size() > value.maximum_length)
                    {
                        return ResultCode::invalid_content;
                    }
                    if (const ResultCode code = validate_string(
                            value.text,
                            limits_.maximum_text_bytes,
                            value.multiline);
                        code != ResultCode::success)
                    {
                        return code;
                    }
                    return validate_string(
                        value.placeholder,
                        limits_.maximum_text_bytes,
                        false);
                }
                else if constexpr (std::same_as<Type, SliderContent>)
                {
                    if (!finite(value.minimum)
                        || !finite(value.maximum)
                        || !finite(value.value)
                        || !finite(value.step)
                        || value.minimum >= value.maximum
                        || value.value < value.minimum
                        || value.value > value.maximum
                        || value.step < 0.0
                        || value.step > value.maximum - value.minimum)
                    {
                        return ResultCode::invalid_content;
                    }
                    return validate_string(
                        value.label,
                        limits_.maximum_text_bytes,
                        false);
                }
                else if constexpr (std::same_as<Type, ScrollAreaContent>)
                {
                    return finite(value.horizontal_offset)
                        && finite(value.vertical_offset)
                        && value.horizontal_offset >= 0.0f
                        && value.vertical_offset >= 0.0f
                        && value.horizontal_offset
                            <= limits_.maximum_layout_extent
                        && value.vertical_offset
                            <= limits_.maximum_layout_extent
                            ? ResultCode::success
                            : ResultCode::invalid_content;
                }
                else if constexpr (std::same_as<Type, TabSetContent>)
                {
                    return value.selected_page
                        || value.selected_page == WidgetHandle{}
                            ? ResultCode::success
                            : ResultCode::invalid_content;
                }
                else
                {
                    if (value.label.empty())
                        return ResultCode::invalid_content;
                    return validate_string(
                        value.label,
                        limits_.maximum_text_bytes,
                        false);
                }
            }, content);
        }

        [[nodiscard]] ResultCode validate_interaction(
            const InteractionDescriptor& interaction) const noexcept
        {
            if (interaction.tab_index < -1)
                return ResultCode::invalid_interaction;
            if (const ResultCode code = validate_string(
                    interaction.action,
                    limits_.maximum_action_bytes,
                    false);
                code != ResultCode::success)
            {
                return code;
            }
            return validate_string(
                interaction.tooltip,
                limits_.maximum_tooltip_bytes,
                true);
        }

        [[nodiscard]] static constexpr bool accepts_children(
            WidgetKind kind) noexcept
        {
            return kind == WidgetKind::canvas
                || kind == WidgetKind::panel
                || kind == WidgetKind::scroll_area
                || kind == WidgetKind::tab_set
                || kind == WidgetKind::tab_page;
        }

        [[nodiscard]] static constexpr bool accepts_child_kind(
            WidgetKind parent,
            WidgetKind child) noexcept
        {
            if (!accepts_children(parent))
                return false;
            if (parent == WidgetKind::tab_set)
                return child == WidgetKind::tab_page;
            return child != WidgetKind::tab_page;
        }

        [[nodiscard]] ResultCode validate_parent_for_kind(
            const State& state,
            WidgetHandle parent,
            WidgetKind childKind) const noexcept
        {
            if (!parent)
            {
                if (parent != WidgetHandle{})
                    return ResultCode::invalid_handle;
                return childKind == WidgetKind::tab_page
                    ? ResultCode::parent_rejects_child
                    : ResultCode::success;
            }
            if (const ResultCode code = widget_status(state, parent);
                code != ResultCode::success)
            {
                return code;
            }
            const WidgetKind parentKind =
                state.slots[parent.index].widget->descriptor.kind;
            return accepts_child_kind(parentKind, childKind)
                ? ResultCode::success
                : ResultCode::parent_rejects_child;
        }

        [[nodiscard]] static std::uint32_t active_widget_count(
            const State& state) noexcept
        {
            return static_cast<std::uint32_t>(std::count_if(
                state.slots.begin(),
                state.slots.end(),
                [](const WidgetSlot& slot)
                {
                    return slot.widget.has_value();
                }));
        }

        [[nodiscard]] static ResultCode widget_status(
            const State& state,
            WidgetHandle handle) noexcept
        {
            if (!handle.valid())
                return ResultCode::invalid_handle;
            if (handle.index >= state.slots.size())
                return ResultCode::missing_widget;
            const WidgetSlot& slot = state.slots[handle.index];
            if (slot.generation != handle.generation)
                return ResultCode::stale_handle;
            if (!slot.widget)
                return ResultCode::missing_widget;
            return ResultCode::success;
        }

        [[nodiscard]] static std::vector<WidgetHandle>& sibling_list(
            State& state,
            WidgetHandle parent)
        {
            return parent
                ? state.slots[parent.index].widget->children
                : state.roots;
        }

        [[nodiscard]] static std::uint32_t resolved_insert_position(
            std::uint32_t requested,
            std::size_t size) noexcept
        {
            if (requested == kAppendPosition)
                return static_cast<std::uint32_t>(size);
            return requested <= size
                ? requested
                : kAppendPosition;
        }

        [[nodiscard]] static bool is_descendant(
            const State& state,
            WidgetHandle candidate,
            WidgetHandle ancestor) noexcept
        {
            WidgetHandle current = candidate;
            std::size_t remaining = state.slots.size();
            while (current && remaining-- != 0u)
            {
                if (current == ancestor)
                    return true;
                if (widget_status(state, current) != ResultCode::success)
                    return false;
                current = state.slots[current.index].widget->parent;
            }
            return false;
        }

        [[nodiscard]] static std::vector<WidgetHandle> subtree_handles(
            const State& state,
            WidgetHandle root)
        {
            std::vector<WidgetHandle> result{};
            std::vector<WidgetHandle> pending{root};
            while (!pending.empty())
            {
                const WidgetHandle current = pending.back();
                pending.pop_back();
                result.push_back(current);
                const std::vector<WidgetHandle>& children =
                    state.slots[current.index].widget->children;
                for (auto iterator = children.rbegin();
                    iterator != children.rend();
                    ++iterator)
                {
                    pending.push_back(*iterator);
                }
            }
            return result;
        }

        static void clear_unresolved_tab_selections(State& state)
        {
            for (std::uint32_t index = 0u; index < state.slots.size(); ++index)
            {
                WidgetSlot& slot = state.slots[index];
                if (!slot.widget
                    || slot.widget->descriptor.kind != WidgetKind::tab_set)
                {
                    continue;
                }
                auto& tabs = std::get<TabSetContent>(
                    slot.widget->descriptor.content);
                if (!tabs.selected_page)
                    continue;
                if (widget_status(state, tabs.selected_page)
                        != ResultCode::success
                    || state.slots[tabs.selected_page.index].widget->parent
                        != WidgetHandle{index, slot.generation})
                {
                    tabs.selected_page = {};
                }
            }
        }

        [[nodiscard]] ValidationIssue validate_state(
            const State& state) const noexcept
        {
            if (state.slots.size() > limits_.maximum_widget_slots)
                return {ResultCode::widget_slot_limit_exceeded, {}, {}};
            if (active_widget_count(state) > limits_.maximum_active_widgets)
                return {ResultCode::widget_limit_exceeded, {}, {}};
            if (state.roots.size() > limits_.maximum_children_per_widget)
                return {ResultCode::child_limit_exceeded, {}, {}};

            std::vector<std::uint32_t> links(state.slots.size(), 0u);
            for (const WidgetHandle root : state.roots)
            {
                if (const ResultCode code = widget_status(state, root);
                    code != ResultCode::success)
                {
                    return {code, root, {}};
                }
                if (state.slots[root.index].widget->parent)
                    return {ResultCode::malformed_hierarchy, root, {}};
                ++links[root.index];
            }

            std::uint64_t strings = 0u;
            for (std::uint32_t index = 0u; index < state.slots.size(); ++index)
            {
                const WidgetSlot& slot = state.slots[index];
                const WidgetHandle handle{index, slot.generation};
                if (slot.generation == 0u)
                    return {ResultCode::malformed_hierarchy, handle, {}};
                if (!slot.widget)
                    continue;

                const WidgetState& widgetValue = *slot.widget;
                if (const ResultCode code = validate_descriptor(
                        widgetValue.descriptor);
                    code != ResultCode::success)
                {
                    return {code, handle, {}};
                }
                const std::uint64_t descriptorBytes =
                    descriptor_string_bytes(widgetValue.descriptor);
                if (descriptorBytes
                    > limits_.maximum_total_string_bytes - strings)
                {
                    return {
                        ResultCode::total_string_limit_exceeded,
                        handle,
                        {}};
                }
                strings += descriptorBytes;

                if (const ResultCode code = validate_parent_for_kind(
                        state,
                        widgetValue.parent,
                        widgetValue.descriptor.kind);
                    code != ResultCode::success)
                {
                    return {code, handle, widgetValue.parent};
                }
                if (widgetValue.children.size()
                    > limits_.maximum_children_per_widget)
                {
                    return {ResultCode::child_limit_exceeded, handle, {}};
                }
                for (const WidgetHandle child : widgetValue.children)
                {
                    if (const ResultCode code = widget_status(state, child);
                        code != ResultCode::success)
                    {
                        return {code, handle, child};
                    }
                    if (state.slots[child.index].widget->parent != handle)
                    {
                        return {
                            ResultCode::malformed_hierarchy,
                            child,
                            handle};
                    }
                    ++links[child.index];
                }

                if (widgetValue.descriptor.kind == WidgetKind::tab_set)
                {
                    const WidgetHandle selected = std::get<TabSetContent>(
                        widgetValue.descriptor.content).selected_page;
                    if (selected)
                    {
                        if (widget_status(state, selected)
                                != ResultCode::success
                            || state.slots[selected.index]
                                    .widget->descriptor.kind
                                != WidgetKind::tab_page
                            || state.slots[selected.index].widget->parent
                                != handle)
                        {
                            return {
                                ResultCode::unresolved_tab_page,
                                handle,
                                selected};
                        }
                    }
                }
            }

            for (std::uint32_t index = 0u; index < state.slots.size(); ++index)
            {
                const WidgetSlot& slot = state.slots[index];
                if (!slot.widget)
                {
                    if (links[index] != 0u)
                    {
                        return {
                            ResultCode::malformed_hierarchy,
                            WidgetHandle{index, slot.generation},
                            {}};
                    }
                    continue;
                }
                const WidgetHandle handle{index, slot.generation};
                if (links[index] != 1u)
                    return {ResultCode::malformed_hierarchy, handle, {}};

                std::uint32_t depth = 1u;
                WidgetHandle current = slot.widget->parent;
                while (current)
                {
                    if (depth >= limits_.maximum_hierarchy_depth)
                    {
                        return {
                            ResultCode::hierarchy_depth_exceeded,
                            handle,
                            current};
                    }
                    if (current == handle)
                        return {ResultCode::cycle_detected, handle, current};
                    if (widget_status(state, current) != ResultCode::success)
                    {
                        return {
                            ResultCode::malformed_hierarchy,
                            handle,
                            current};
                    }
                    current = state.slots[current.index].widget->parent;
                    ++depth;
                }
            }
            return {};
        }

        [[nodiscard]] static std::uint64_t descriptor_string_bytes(
            const WidgetDescriptor& descriptor) noexcept
        {
            std::uint64_t result = descriptor.name.size()
                + descriptor.style.style_class.size()
                + descriptor.interaction.action.size()
                + descriptor.interaction.tooltip.size();
            std::visit([&result](const auto& value)
            {
                using Type = std::remove_cvref_t<decltype(value)>;
                if constexpr (std::same_as<Type, PanelContent>)
                    result += value.title.size();
                else if constexpr (std::same_as<Type, ButtonContent>)
                    result += value.label.size();
                else if constexpr (std::same_as<Type, TextContent>)
                    result += value.text.size();
                else if constexpr (std::same_as<Type, ImageContent>)
                {
                    result += value.asset_path.size();
                    result += value.alternative_text.size();
                }
                else if constexpr (std::same_as<Type, ImageButtonContent>)
                {
                    result += value.asset_path.size();
                    result += value.label.size();
                }
                else if constexpr (std::same_as<Type, TextInputContent>)
                {
                    result += value.text.size();
                    result += value.placeholder.size();
                }
                else if constexpr (std::same_as<Type, SliderContent>)
                    result += value.label.size();
                else if constexpr (std::same_as<Type, TabPageContent>)
                    result += value.label.size();
            }, descriptor.content);
            return result;
        }

        [[nodiscard]] static std::uint64_t total_string_bytes(
            const State& state) noexcept
        {
            std::uint64_t result = 0u;
            for (const WidgetSlot& slot : state.slots)
            {
                if (slot.widget)
                    result += descriptor_string_bytes(slot.widget->descriptor);
            }
            return result;
        }

        [[nodiscard]] static std::uint32_t maximum_depth(
            const State& state) noexcept
        {
            std::uint32_t result = 0u;
            for (const WidgetSlot& slot : state.slots)
            {
                if (!slot.widget)
                    continue;
                std::uint32_t depth = 1u;
                WidgetHandle current = slot.widget->parent;
                std::size_t remaining = state.slots.size();
                while (current && remaining-- != 0u)
                {
                    ++depth;
                    if (widget_status(state, current) != ResultCode::success)
                        break;
                    current = state.slots[current.index].widget->parent;
                }
                result = (std::max)(result, depth);
            }
            return result;
        }

        static void hash_color(
            authoring::DeterministicHashBuilder& hash,
            Color color) noexcept
        {
            hash.append_float(color.red);
            hash.append_float(color.green);
            hash.append_float(color.blue);
            hash.append_float(color.alpha);
        }

        static void hash_descriptor(
            authoring::DeterministicHashBuilder& hash,
            const WidgetDescriptor& descriptor) noexcept
        {
            hash.append_enum(descriptor.kind);
            hash.append_string(descriptor.name);
            const LayoutDescriptor& layout = descriptor.layout;
            hash.append_enum(layout.flow);
            hash.append_enum(layout.width_rule);
            hash.append_enum(layout.height_rule);
            hash.append_float(layout.x);
            hash.append_float(layout.y);
            hash.append_float(layout.width);
            hash.append_float(layout.height);
            hash.append_float(layout.minimum_width);
            hash.append_float(layout.minimum_height);
            hash.append_float(layout.maximum_width);
            hash.append_float(layout.maximum_height);
            hash.append_float(layout.flex_grow);
            hash.append_float(layout.spacing);
            hash.append_float(layout.padding.left);
            hash.append_float(layout.padding.top);
            hash.append_float(layout.padding.right);
            hash.append_float(layout.padding.bottom);
            hash.append_bool(layout.clip_children);

            const StyleDescriptor& style = descriptor.style;
            hash_color(hash, style.background);
            hash_color(hash, style.foreground);
            hash_color(hash, style.border);
            hash_color(hash, style.accent);
            hash.append_float(style.border_width);
            hash.append_float(style.corner_radius);
            hash.append_float(style.opacity);
            hash.append_string(style.style_class);

            hash.append_u64(static_cast<std::uint64_t>(
                descriptor.content.index()));
            std::visit([&hash](const auto& value)
            {
                using Type = std::remove_cvref_t<decltype(value)>;
                if constexpr (std::same_as<Type, CanvasContent>)
                {
                    hash.append_float(value.virtual_width);
                    hash.append_float(value.virtual_height);
                    hash.append_bool(value.pixel_snap);
                }
                else if constexpr (std::same_as<Type, PanelContent>)
                    hash.append_string(value.title);
                else if constexpr (std::same_as<Type, ButtonContent>)
                    hash.append_string(value.label);
                else if constexpr (std::same_as<Type, TextContent>)
                {
                    hash.append_string(value.text);
                    hash.append_bool(value.wrap);
                }
                else if constexpr (std::same_as<Type, ImageContent>)
                {
                    hash.append_string(value.asset_path);
                    hash.append_string(value.alternative_text);
                    hash.append_bool(value.preserve_aspect);
                }
                else if constexpr (std::same_as<Type, ImageButtonContent>)
                {
                    hash.append_string(value.asset_path);
                    hash.append_string(value.label);
                    hash.append_bool(value.preserve_aspect);
                }
                else if constexpr (std::same_as<Type, TextInputContent>)
                {
                    hash.append_string(value.text);
                    hash.append_string(value.placeholder);
                    hash.append_u32(value.maximum_length);
                    hash.append_bool(value.multiline);
                    hash.append_bool(value.read_only);
                }
                else if constexpr (std::same_as<Type, SliderContent>)
                {
                    hash.append_string(value.label);
                    hash.append_double(value.minimum);
                    hash.append_double(value.maximum);
                    hash.append_double(value.value);
                    hash.append_double(value.step);
                }
                else if constexpr (std::same_as<Type, ScrollAreaContent>)
                {
                    hash.append_float(value.horizontal_offset);
                    hash.append_float(value.vertical_offset);
                    hash.append_bool(value.allow_horizontal);
                    hash.append_bool(value.allow_vertical);
                }
                else if constexpr (std::same_as<Type, TabSetContent>)
                    hash.append_handle(value.selected_page);
                else
                {
                    hash.append_string(value.label);
                    hash.append_bool(value.closeable);
                }
            }, descriptor.content);

            const InteractionDescriptor& interaction = descriptor.interaction;
            hash.append_bool(interaction.enabled);
            hash.append_bool(interaction.visible);
            hash.append_bool(interaction.focusable);
            hash.append_bool(interaction.accepts_pointer);
            hash.append_i64(interaction.tab_index);
            hash.append_string(interaction.action);
            hash.append_string(interaction.tooltip);
        }

        [[nodiscard]] static authoring::ContentHash content_hash(
            const State& state) noexcept
        {
            authoring::DeterministicHashBuilder hash{
                "epoch.authoring.gui_document.v1"};
            hash.append_u32(kSchemaVersion);
            hash.append_u64(static_cast<std::uint64_t>(state.slots.size()));
            for (std::uint32_t index = 0u; index < state.slots.size(); ++index)
            {
                const WidgetSlot& slot = state.slots[index];
                hash.append_u32(index);
                hash.append_u32(slot.generation);
                hash.append_bool(slot.widget.has_value());
                if (!slot.widget)
                    continue;
                hash.append_handle(slot.widget->parent);
                hash_descriptor(hash, slot.widget->descriptor);
                hash.append_u64(static_cast<std::uint64_t>(
                    slot.widget->children.size()));
                for (const WidgetHandle child : slot.widget->children)
                    hash.append_handle(child);
            }
            hash.append_u64(static_cast<std::uint64_t>(state.roots.size()));
            for (const WidgetHandle root : state.roots)
                hash.append_handle(root);
            return hash.finish();
        }

        [[nodiscard]] static std::uint64_t retained_state_bytes(
            const State& state) noexcept
        {
            std::uint64_t result = sizeof(State)
                + static_cast<std::uint64_t>(state.slots.size())
                    * sizeof(WidgetSlot)
                + static_cast<std::uint64_t>(state.roots.size())
                    * sizeof(WidgetHandle);
            for (const WidgetSlot& slot : state.slots)
            {
                if (!slot.widget)
                    continue;
                result += descriptor_string_bytes(slot.widget->descriptor);
                result += static_cast<std::uint64_t>(
                    slot.widget->children.size()) * sizeof(WidgetHandle);
            }
            return result;
        }

        [[nodiscard]] static std::uint64_t operation_payload_bytes(
            const OperationPayload& payload) noexcept
        {
            std::uint64_t result = sizeof(OperationPayload);
            std::visit([&result](const auto& operation)
            {
                using Type = std::remove_cvref_t<decltype(operation)>;
                if constexpr (std::same_as<Type, WidgetCreatedOperation>)
                {
                    result += descriptor_string_bytes(operation.descriptor);
                }
                else if constexpr (std::same_as<Type, WidgetRemovedOperation>)
                {
                    result += static_cast<std::uint64_t>(
                        operation.removed.size()) * sizeof(WidgetHandle);
                }
                else if constexpr (std::same_as<Type, StyleChangedOperation>)
                {
                    result += operation.before.style_class.size();
                    result += operation.after.style_class.size();
                }
                else if constexpr (std::same_as<Type, ContentChangedOperation>)
                {
                    WidgetDescriptor before{};
                    WidgetDescriptor after{};
                    before.content = operation.before;
                    after.content = operation.after;
                    result += descriptor_string_bytes(before);
                    result += descriptor_string_bytes(after);
                }
                else if constexpr (
                    std::same_as<Type, InteractionChangedOperation>)
                {
                    result += operation.before.action.size()
                        + operation.before.tooltip.size()
                        + operation.after.action.size()
                        + operation.after.tooltip.size();
                }
            }, payload);
            return result;
        }

        template<typename Payload>
        [[nodiscard]] ResultCode can_commit(
            const State& candidate,
            const Payload& payload) const noexcept
        {
            if (revision_sequence_
                == (std::numeric_limits<std::uint64_t>::max)())
            {
                return ResultCode::revision_overflow;
            }
            if (next_operation_id_ == 0u)
                return ResultCode::operation_id_overflow;
            if (history_cursor_ >= limits_.maximum_history_operations)
                return ResultCode::history_operation_limit_exceeded;

            std::uint64_t prefixBytes = 0u;
            for (std::size_t index = 0u; index < history_cursor_; ++index)
                prefixBytes += history_[index].record.retained_bytes;
            const OperationPayload boxed{payload};
            const std::uint64_t entryBytes = sizeof(HistoryEntry)
                + retained_state_bytes(state_)
                + retained_state_bytes(candidate)
                + operation_payload_bytes(boxed);
            if (entryBytes > limits_.maximum_history_bytes - prefixBytes)
                return ResultCode::history_memory_limit_exceeded;
            return ResultCode::success;
        }

        template<typename Part, typename Accessor>
        [[nodiscard]] MutationResult update_descriptor_part(
            WidgetHandle handle,
            Part value,
            Accessor accessor)
        {
            if (!valid_)
                return failure(ResultCode::invalid_document, handle);
            if (const ResultCode code = widget_status(state_, handle);
                code != ResultCode::success)
            {
                return failure(code, handle);
            }

            State candidate = state_;
            WidgetDescriptor& descriptor =
                candidate.slots[handle.index].widget->descriptor;
            Part before = accessor(descriptor);
            if (before == value)
                return failure(ResultCode::no_change, handle);
            accessor(descriptor) = value;
            if (const ResultCode code = validate_descriptor(descriptor);
                code != ResultCode::success)
            {
                return failure(code, handle);
            }
            if (const ValidationIssue issue = validate_state(candidate); issue)
                return failure(issue.code, issue.widget);

            auto operation = [&]() -> OperationPayload
            {
                if constexpr (std::same_as<Part, LayoutDescriptor>)
                    return LayoutChangedOperation{handle, before, value};
                else if constexpr (std::same_as<Part, StyleDescriptor>)
                    return StyleChangedOperation{handle, before, value};
                else if constexpr (std::same_as<Part, WidgetContent>)
                    return ContentChangedOperation{handle, before, value};
                else
                    return InteractionChangedOperation{handle, before, value};
            }();
            if (const ResultCode code = can_commit(candidate, operation);
                code != ResultCode::success)
            {
                return failure(code, handle);
            }
            return commit(std::move(operation), std::move(candidate), handle);
        }

        [[nodiscard]] static WidgetHandle primary_widget(
            const OperationPayload& payload) noexcept
        {
            return std::visit([](const auto& operation) noexcept
            {
                using Type = std::remove_cvref_t<decltype(operation)>;
                if constexpr (std::same_as<Type, WidgetRemovedOperation>)
                    return operation.root;
                else
                    return operation.widget;
            }, payload);
        }

        void ensure_generation_capacity(std::uint32_t size)
        {
            if (issued_generations_.size() < size)
                issued_generations_.resize(size, 0u);
        }

        void advance_revision() noexcept
        {
            ++revision_sequence_;
            if (revision_sequence_ == 0u)
                revision_sequence_ = 1u;
            revision_ = DocumentRevision{
                content_hash(state_),
                revision_sequence_
            };
        }

        [[nodiscard]] static MutationResult failure(
            ResultCode code,
            WidgetHandle widget = {}) noexcept
        {
            return MutationResult{code, widget, {}};
        }

        template<typename Payload>
        [[nodiscard]] MutationResult commit(
            Payload payload,
            State candidate,
            WidgetHandle widget)
        {
            if (history_cursor_ < history_.size())
            {
                history_.erase(
                    history_.begin()
                        + static_cast<std::ptrdiff_t>(history_cursor_),
                    history_.end());
            }
            retained_history_bytes_ = 0u;
            for (const HistoryEntry& entry : history_)
                retained_history_bytes_ += entry.record.retained_bytes;

            const OperationId operation{next_operation_id_++};
            OperationPayload boxed{std::move(payload)};
            const std::uint64_t retainedBytes = sizeof(HistoryEntry)
                + retained_state_bytes(state_)
                + retained_state_bytes(candidate)
                + operation_payload_bytes(boxed);
            HistoryEntry entry{};
            entry.record = OperationRecord{
                operation,
                std::move(boxed),
                content_hash(state_),
                content_hash(candidate),
                retainedBytes,
                true
            };
            entry.before = state_;
            entry.after = candidate;
            history_.push_back(std::move(entry));
            retained_history_bytes_ += retainedBytes;
            state_ = std::move(candidate);
            history_cursor_ = history_.size();
            advance_revision();
            return MutationResult{ResultCode::success, widget, operation};
        }

        DocumentHandle document_{};
        BranchIdentity branch_{};
        DocumentLimits limits_{};
        bool valid_{};
        State state_{};
        std::vector<std::uint32_t> issued_generations_{};
        std::vector<HistoryEntry> history_{};
        std::size_t history_cursor_{};
        std::uint64_t retained_history_bytes_{};
        std::uint64_t next_operation_id_{1u};
        std::uint64_t revision_sequence_{};
        DocumentRevision revision_{};
    };

    enum class TemplatePreset : std::uint8_t
    {
        blank_canvas = 0,
        desktop_app,
        dashboard,
        mobile_app,
        game_hud
    };

    [[nodiscard]] std::optional<GuiDocument> make_template_document(
        TemplatePreset preset) noexcept;
    [[nodiscard]] bool is_legacy_generated_root_only_document(
        const GuiDocumentSnapshot& snapshot) noexcept;
    [[nodiscard]] std::optional<GuiDocument>
        migrate_legacy_generated_root_only_document(
            const GuiDocumentSnapshot& snapshot,
            TemplatePreset replacement) noexcept;

    enum class ContractFailure : std::uint8_t
    {
        none,
        invalid_document,
        descriptor_validation,
        stale_handles,
        hierarchy_rejection,
        cycle_rejection,
        descriptor_edits,
        tab_image_button_content,
        undo_redo,
        bounded_state,
        deterministic_snapshot,
        snapshot_codec,
        operation_journal,
        template_factory,
        starter_roundtrip,
        starter_migration,
        result_names
    };

    [[nodiscard]] ContractFailure run_contract();
}
