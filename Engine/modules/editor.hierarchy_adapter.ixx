/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

export module editor.hierarchy_adapter;

export namespace epochengine::editor_hierarchy
{
    using CanonicalId = std::uint64_t;
    using NodeId = std::uint64_t;

    inline constexpr CanonicalId invalid_canonical_id{};
    inline constexpr NodeId invalid_node_id{};

    struct EntityRecord final
    {
        CanonicalId canonical_id{};
        std::string_view label{};
        std::string_view type{};
        std::string_view category{};
        std::uint64_t order{};
        bool visible{true};
        bool locked{};
    };

    enum class Code : std::uint8_t
    {
        ready,
        unchanged,
        invalid_scene,
        invalid_entity,
        duplicate_entity,
        hierarchy_rejected,
        unknown_node,
        hidden_node,
        disabled_node,
        locked_node,
        selection_limit,
        invalid_action,
        action_disabled,
        no_targets,
        invalid_viewport
    };

    [[nodiscard]] constexpr std::string_view code_name(Code code) noexcept
    {
        switch (code)
        {
        case Code::ready: return "ready";
        case Code::unchanged: return "unchanged";
        case Code::invalid_scene: return "invalid_scene";
        case Code::invalid_entity: return "invalid_entity";
        case Code::duplicate_entity: return "duplicate_entity";
        case Code::hierarchy_rejected: return "hierarchy_rejected";
        case Code::unknown_node: return "unknown_node";
        case Code::hidden_node: return "hidden_node";
        case Code::disabled_node: return "disabled_node";
        case Code::locked_node: return "locked_node";
        case Code::selection_limit: return "selection_limit";
        case Code::invalid_action: return "invalid_action";
        case Code::action_disabled: return "action_disabled";
        case Code::no_targets: return "no_targets";
        case Code::invalid_viewport: return "invalid_viewport";
        }
        return "unknown";
    }

    enum class SelectionMode : std::uint8_t
    {
        replace,
        toggle,
        range,
        add_range
    };

    enum class Navigation : std::uint8_t
    {
        first,
        previous,
        next,
        last,
        parent,
        first_child,
        collapse_or_parent,
        expand_or_child,
        toggle_expansion
    };

    enum class Action : std::uint8_t
    {
        select,
        focus,
        duplicate_entity,
        delete_entity,
        deselect
    };

    [[nodiscard]] constexpr std::string_view action_name(Action action) noexcept
    {
        switch (action)
        {
        case Action::select: return "select";
        case Action::focus: return "focus";
        case Action::duplicate_entity: return "duplicate";
        case Action::delete_entity: return "delete";
        case Action::deselect: return "deselect";
        }
        return "unknown";
    }

    struct RowView final
    {
        NodeId node_id{};
        NodeId parent_node_id{};
        CanonicalId canonical_id{};
        std::string label{};
        std::string detail{};
        std::size_t visible_index{};
        std::size_t depth{};
        bool group{};
        bool has_children{};
        bool expanded{};
        bool expansion_forced{};
        bool direct_match{};
        bool selected{};
        bool focused{};
        bool locked{};
    };

    struct Snapshot final
    {
        std::string scene_identity{};
        std::string filter{};
        std::size_t entity_count{};
        std::size_t visible_rows{};
        std::size_t selected_entities{};
        std::size_t expanded_nodes{};
        std::size_t direct_matches{};
        CanonicalId focused_entity{};
        std::uint64_t model_revision{};
        std::uint64_t view_revision{};
    };

    struct Result final
    {
        Code code{Code::unchanged};
        CanonicalId focused_entity{};
        std::vector<CanonicalId> selected_entities{};
        bool accepted{true};
        bool changed{};
        bool expansion_changed{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return accepted
                && (code == Code::ready || code == Code::unchanged);
        }
    };

    struct Viewport final
    {
        float row_height{28.0f};
        float viewport_height{};
        float scroll_y{};
        std::size_t overscan_rows{2u};
    };

    struct LayoutRow final
    {
        std::size_t row_index{};
        float top{};
        float bottom{};
    };

    struct LayoutPlan final
    {
        Code code{Code::invalid_viewport};
        std::size_t first{};
        std::size_t past_last{};
        std::size_t total_rows{};
        float content_height{};
        float scroll_y{};
        float maximum_scroll_y{};
        std::vector<LayoutRow> rows{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == Code::ready;
        }
    };

    struct ScrollPlan final
    {
        Code code{Code::invalid_viewport};
        CanonicalId target{};
        float previous_scroll_y{};
        float scroll_y{};
        bool changed{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == Code::ready || code == Code::unchanged;
        }
    };

    struct Route final
    {
        Code code{Code::invalid_action};
        Action action{Action::select};
        NodeId invoked_on{};
        std::string command{};
        std::vector<CanonicalId> targets{};
        bool accepted{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return accepted && code == Code::ready;
        }
    };

    class Controller final
    {
    public:
        Controller();
        ~Controller();

        Controller(const Controller&) = delete;
        Controller& operator=(const Controller&) = delete;
        Controller(Controller&&) noexcept;
        Controller& operator=(Controller&&) noexcept;

        [[nodiscard]] Result refresh(
            std::string_view scene_identity,
            std::span<const EntityRecord> entities,
            CanonicalId authoritative_selection = invalid_canonical_id);
        [[nodiscard]] Result set_filter(std::string_view query);
        [[nodiscard]] Result clear_filter();
        [[nodiscard]] Result toggle_expanded(NodeId node);
        [[nodiscard]] Result select(
            NodeId node,
            SelectionMode mode = SelectionMode::replace);
        [[nodiscard]] Result synchronize_selection(CanonicalId entity);
        [[nodiscard]] Result navigate(
            Navigation command,
            bool extend_selection = false);
        [[nodiscard]] Route route(Action action, NodeId invoked_on) const;
        [[nodiscard]] LayoutPlan plan_rows(Viewport viewport) const;
        [[nodiscard]] ScrollPlan scroll_to_entity(
            CanonicalId entity,
            Viewport viewport) const;

        [[nodiscard]] NodeId node_for_entity(CanonicalId entity) const noexcept;
        [[nodiscard]] CanonicalId entity_for_node(NodeId node) const noexcept;
        [[nodiscard]] const RowView* find_row(NodeId node) const noexcept;
        [[nodiscard]] std::span<const RowView> rows() const noexcept;
        [[nodiscard]] Snapshot snapshot() const;

    private:
        struct Implementation;
        std::unique_ptr<Implementation> implementation_;
    };

    enum class ContractFailure : std::uint8_t
    {
        none,
        admission,
        stable_identity,
        expansion_preservation,
        filtering,
        selection,
        navigation,
        locked_route,
        action_route,
        virtualization,
        scroll_visibility,
        transactional_rejection
    };

    [[nodiscard]] constexpr std::string_view contract_failure_name(
        ContractFailure failure) noexcept
    {
        switch (failure)
        {
        case ContractFailure::none: return "none";
        case ContractFailure::admission: return "admission";
        case ContractFailure::stable_identity: return "stable_identity";
        case ContractFailure::expansion_preservation:
            return "expansion_preservation";
        case ContractFailure::filtering: return "filtering";
        case ContractFailure::selection: return "selection";
        case ContractFailure::navigation: return "navigation";
        case ContractFailure::locked_route: return "locked_route";
        case ContractFailure::action_route: return "action_route";
        case ContractFailure::virtualization: return "virtualization";
        case ContractFailure::scroll_visibility:
            return "scroll_visibility";
        case ContractFailure::transactional_rejection:
            return "transactional_rejection";
        }
        return "unknown";
    }

    [[nodiscard]] ContractFailure run_contract() noexcept;
}
