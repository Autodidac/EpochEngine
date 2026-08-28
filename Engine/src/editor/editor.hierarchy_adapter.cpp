/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <gui/hierarchy_tree.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

module editor.hierarchy_adapter;

namespace epochengine::editor_hierarchy
{
    namespace tree = epochengine::gui_lib::hierarchy_tree;

    namespace
    {
        constexpr tree::ActionId select_action{1u};
        constexpr tree::ActionId focus_action{2u};
        constexpr tree::ActionId duplicate_action{3u};
        constexpr tree::ActionId delete_action{4u};
        constexpr tree::ActionId deselect_action{5u};
        constexpr std::size_t maximum_identity_bytes{2'048u};

        [[nodiscard]] Code translate(tree::ErrorCode code) noexcept
        {
            switch (code)
            {
            case tree::ErrorCode::none: return Code::ready;
            case tree::ErrorCode::unknown_node: return Code::unknown_node;
            case tree::ErrorCode::node_not_visible: return Code::hidden_node;
            case tree::ErrorCode::disabled_node: return Code::disabled_node;
            case tree::ErrorCode::locked_node: return Code::locked_node;
            case tree::ErrorCode::selection_limit_exceeded:
                return Code::selection_limit;
            case tree::ErrorCode::invalid_viewport:
                return Code::invalid_viewport;
            case tree::ErrorCode::invalid_action:
            case tree::ErrorCode::duplicate_action:
            case tree::ErrorCode::action_limit_exceeded:
                return Code::invalid_action;
            case tree::ErrorCode::action_disabled:
                return Code::action_disabled;
            case tree::ErrorCode::no_action_targets:
                return Code::no_targets;
            default: return Code::hierarchy_rejected;
            }
        }

        [[nodiscard]] tree::SelectionMode translate(SelectionMode mode) noexcept
        {
            switch (mode)
            {
            case SelectionMode::replace:
                return tree::SelectionMode::replace;
            case SelectionMode::toggle:
                return tree::SelectionMode::toggle;
            case SelectionMode::range:
                return tree::SelectionMode::range;
            case SelectionMode::add_range:
                return tree::SelectionMode::add_range;
            }
            return tree::SelectionMode::replace;
        }

        [[nodiscard]] tree::NavigationCommand translate(
            Navigation command) noexcept
        {
            switch (command)
            {
            case Navigation::first: return tree::NavigationCommand::first;
            case Navigation::previous:
                return tree::NavigationCommand::previous;
            case Navigation::next: return tree::NavigationCommand::next;
            case Navigation::last: return tree::NavigationCommand::last;
            case Navigation::parent: return tree::NavigationCommand::parent;
            case Navigation::first_child:
                return tree::NavigationCommand::first_child;
            case Navigation::collapse_or_parent:
                return tree::NavigationCommand::collapse_or_parent;
            case Navigation::expand_or_child:
                return tree::NavigationCommand::expand_or_child;
            case Navigation::toggle_expansion:
                return tree::NavigationCommand::toggle_expansion;
            }
            return tree::NavigationCommand::next;
        }

        [[nodiscard]] tree::ActionId action_id(Action action) noexcept
        {
            switch (action)
            {
            case Action::select: return select_action;
            case Action::focus: return focus_action;
            case Action::duplicate_entity: return duplicate_action;
            case Action::delete_entity: return delete_action;
            case Action::deselect: return deselect_action;
            }
            return {};
        }
    }

    struct Controller::Implementation final
    {
        tree::Controller hierarchy{};
        std::string scene_identity{};
        std::unordered_map<CanonicalId, NodeId> entity_nodes{};
        std::unordered_map<std::string, NodeId> group_nodes{};
        std::unordered_map<NodeId, CanonicalId> node_entities{};
        std::unordered_set<NodeId> group_node_ids{};
        std::vector<RowView> visible_rows{};
        NodeId root_node{};
        NodeId next_node{1u};
        std::size_t entity_count{};

        Implementation()
        {
            const std::vector<tree::ContextAction> actions{
                tree::ContextAction{
                    .id = select_action,
                    .label = "Select",
                    .command = "editor.world.select",
                    .order = 10u,
                    .enabled = true,
                    .allow_locked_targets = true,
                    .use_selected_set = false},
                tree::ContextAction{
                    .id = focus_action,
                    .label = "Focus",
                    .command = "editor.world.focus",
                    .order = 20u,
                    .enabled = true,
                    .allow_locked_targets = true,
                    .use_selected_set = true},
                tree::ContextAction{
                    .id = duplicate_action,
                    .label = "Duplicate",
                    .command = "editor.world.duplicate",
                    .order = 30u,
                    .enabled = true,
                    .allow_locked_targets = false,
                    .use_selected_set = true},
                tree::ContextAction{
                    .id = delete_action,
                    .label = "Delete",
                    .command = "editor.world.delete",
                    .order = 40u,
                    .enabled = true,
                    .allow_locked_targets = false,
                    .use_selected_set = true},
                tree::ContextAction{
                    .id = deselect_action,
                    .label = "Deselect",
                    .command = "editor.world.deselect",
                    .order = 50u,
                    .enabled = true,
                    .allow_locked_targets = true,
                    .use_selected_set = false}}
            ;
            (void)hierarchy.replace_context_actions(actions);
        }

        [[nodiscard]] CanonicalId focused_entity() const noexcept
        {
            const auto found = node_entities.find(hierarchy.focused_id().value);
            return found == node_entities.end()
                ? invalid_canonical_id
                : found->second;
        }

        [[nodiscard]] std::vector<CanonicalId> selected_entities() const
        {
            std::vector<CanonicalId> result{};
            result.reserve(hierarchy.selected_ids().size());
            for (const tree::NodeId selected : hierarchy.selected_ids())
            {
                const auto found = node_entities.find(selected.value);
                if (found != node_entities.end())
                    result.push_back(found->second);
            }
            return result;
        }

        void rebuild_visible_rows()
        {
            visible_rows.clear();
            visible_rows.reserve(hierarchy.visible_rows().size());
            for (const tree::VisibleRow& source : hierarchy.visible_rows())
            {
                if (source.node == nullptr)
                    continue;

                const NodeId node = source.node->id.value;
                const auto entity = node_entities.find(node);
                visible_rows.push_back(RowView{
                    .node_id = node,
                    .parent_node_id = source.node->parent.value,
                    .canonical_id = entity == node_entities.end()
                        ? invalid_canonical_id
                        : entity->second,
                    .label = source.node->label,
                    .detail = source.node->detail,
                    .visible_index = source.visible_index,
                    .depth = source.depth,
                    .group = node == root_node
                        || group_node_ids.contains(node),
                    .has_children = source.has_children,
                    .expanded = source.expanded,
                    .expansion_forced = source.expansion_forced,
                    .direct_match = source.direct_match,
                    .selected = source.selected,
                    .focused = source.focused,
                    .locked = source.node->locked});
            }
        }

        [[nodiscard]] Result result(
            Code code,
            bool accepted,
            bool changed,
            bool expansion_changed = false) const
        {
            return Result{
                .code = code,
                .focused_entity = focused_entity(),
                .selected_entities = selected_entities(),
                .accepted = accepted,
                .changed = changed,
                .expansion_changed = expansion_changed};
        }
    };

    Controller::Controller()
        : implementation_(std::make_unique<Implementation>())
    {
    }

    Controller::~Controller() = default;
    Controller::Controller(Controller&&) noexcept = default;
    Controller& Controller::operator=(Controller&&) noexcept = default;

    Result Controller::refresh(
        std::string_view scene_identity,
        std::span<const EntityRecord> entities,
        CanonicalId authoritative_selection)
    {
        Implementation& state = *implementation_;
        if (scene_identity.empty()
            || scene_identity.size() > maximum_identity_bytes)
        {
            return state.result(Code::invalid_scene, false, false);
        }

        std::unordered_set<CanonicalId> supplied_entities{};
        std::vector<std::string> categories{};
        categories.reserve(entities.size());
        supplied_entities.reserve(entities.size());
        for (const EntityRecord& entity : entities)
        {
            if (entity.canonical_id == invalid_canonical_id
                || entity.label.empty()
                || entity.label.size() > maximum_identity_bytes
                || entity.type.size() > maximum_identity_bytes
                || entity.category.size() > maximum_identity_bytes)
            {
                return state.result(Code::invalid_entity, false, false);
            }
            if (!supplied_entities.emplace(entity.canonical_id).second)
                return state.result(Code::duplicate_entity, false, false);
            categories.emplace_back(
                entity.category.empty() ? "Uncategorized" : entity.category);
        }

        std::sort(categories.begin(), categories.end());
        categories.erase(
            std::unique(categories.begin(), categories.end()), categories.end());

        const bool preserve_state = state.scene_identity == scene_identity;
        auto entity_nodes = preserve_state
            ? state.entity_nodes
            : std::unordered_map<CanonicalId, NodeId>{};
        auto group_nodes = preserve_state
            ? state.group_nodes
            : std::unordered_map<std::string, NodeId>{};
        NodeId next_node = preserve_state ? state.next_node : 1u;

        const auto allocate_node = [&next_node]() -> NodeId
        {
            if (next_node == invalid_node_id
                || next_node == std::numeric_limits<NodeId>::max())
            {
                return invalid_node_id;
            }
            return next_node++;
        };

        NodeId root = preserve_state ? state.root_node : allocate_node();
        if (root == invalid_node_id)
            return state.result(Code::hierarchy_rejected, false, false);

        std::vector<NodeId> newly_created_groups{};
        for (const std::string& category : categories)
        {
            if (group_nodes.contains(category))
                continue;
            const NodeId node = allocate_node();
            if (node == invalid_node_id)
                return state.result(Code::hierarchy_rejected, false, false);
            group_nodes.emplace(category, node);
            newly_created_groups.push_back(node);
        }
        for (const EntityRecord& entity : entities)
        {
            if (entity_nodes.contains(entity.canonical_id))
                continue;
            const NodeId node = allocate_node();
            if (node == invalid_node_id)
                return state.result(Code::hierarchy_rejected, false, false);
            entity_nodes.emplace(entity.canonical_id, node);
        }

        std::vector<tree::Node> nodes{};
        nodes.reserve(1u + categories.size() + entities.size());
        nodes.push_back(tree::Node{
            .id = tree::NodeId{root},
            .label = std::string{scene_identity},
            .detail = "Scene",
            .search_terms = "scene world",
            .context_tag = "world-root",
            .order = 0u,
            .kind = tree::NodeKind::document,
            .expandable = true});

        std::uint64_t group_order{1u};
        for (const std::string& category : categories)
        {
            nodes.push_back(tree::Node{
                .id = tree::NodeId{group_nodes.at(category)},
                .parent = tree::NodeId{root},
                .label = category,
                .detail = "Category",
                .search_terms = category,
                .context_tag = "world-category",
                .order = group_order++,
                .kind = tree::NodeKind::group,
                .expandable = true});
        }

        for (const EntityRecord& entity : entities)
        {
            const std::string category = entity.category.empty()
                ? "Uncategorized"
                : std::string{entity.category};
            std::string detail{entity.type};
            if (!entity.visible)
            {
                if (!detail.empty())
                    detail += " | ";
                detail += "hidden";
            }
            std::string search_terms{entity.type};
            if (!search_terms.empty())
                search_terms += ' ';
            search_terms += category;
            nodes.push_back(tree::Node{
                .id = tree::NodeId{entity_nodes.at(entity.canonical_id)},
                .parent = tree::NodeId{group_nodes.at(category)},
                .label = std::string{entity.label},
                .detail = std::move(detail),
                .search_terms = std::move(search_terms),
                .context_tag = "world-entity",
                .order = entity.order,
                .kind = tree::NodeKind::item,
                .expandable = false,
                .disabled = false,
                .locked = entity.locked});
        }

        const tree::ReplaceResult admitted = state.hierarchy.replace_nodes(
            nodes, preserve_state);
        if (!admitted.committed)
        {
            return state.result(
                translate(admitted.error), false, false);
        }

        for (auto iterator = entity_nodes.begin(); iterator != entity_nodes.end();)
        {
            if (!supplied_entities.contains(iterator->first))
                iterator = entity_nodes.erase(iterator);
            else
                ++iterator;
        }
        for (auto iterator = group_nodes.begin(); iterator != group_nodes.end();)
        {
            if (!std::binary_search(
                    categories.begin(), categories.end(), iterator->first))
            {
                iterator = group_nodes.erase(iterator);
            }
            else
            {
                ++iterator;
            }
        }

        state.scene_identity.assign(scene_identity);
        state.entity_nodes = std::move(entity_nodes);
        state.group_nodes = std::move(group_nodes);
        state.root_node = root;
        state.next_node = next_node;
        state.entity_count = entities.size();
        state.node_entities.clear();
        state.node_entities.reserve(state.entity_nodes.size());
        for (const auto& [entity, node] : state.entity_nodes)
            state.node_entities.emplace(node, entity);
        state.group_node_ids.clear();
        state.group_node_ids.reserve(state.group_nodes.size());
        for (const auto& [category, node] : state.group_nodes)
        {
            (void)category;
            state.group_node_ids.emplace(node);
        }

        if (!preserve_state)
            (void)state.hierarchy.set_expanded(tree::NodeId{root}, true);
        for (const NodeId group : newly_created_groups)
            (void)state.hierarchy.set_expanded(tree::NodeId{group}, true);

        if (authoritative_selection == invalid_canonical_id)
        {
            (void)state.hierarchy.clear_selection();
        }
        else if (const auto selected = state.entity_nodes.find(
                     authoritative_selection);
                 selected != state.entity_nodes.end())
        {
            (void)state.hierarchy.select(
                tree::NodeId{selected->second}, tree::SelectionMode::replace);
        }
        else
        {
            (void)state.hierarchy.clear_selection();
        }

        state.rebuild_visible_rows();
        return state.result(Code::ready, true, true);
    }

    Result Controller::set_filter(std::string_view query)
    {
        Implementation& state = *implementation_;
        const tree::MutationResult result = state.hierarchy.set_filter({
            .query = query,
            .include_descendants_of_matches = true});
        if (result.accepted)
            state.rebuild_visible_rows();
        return state.result(
            translate(result.error), result.accepted, result.changed);
    }

    Result Controller::clear_filter()
    {
        Implementation& state = *implementation_;
        const tree::MutationResult result = state.hierarchy.clear_filter();
        if (result.accepted)
            state.rebuild_visible_rows();
        return state.result(
            translate(result.error), result.accepted, result.changed);
    }

    Result Controller::toggle_expanded(NodeId node)
    {
        Implementation& state = *implementation_;
        const tree::MutationResult result = state.hierarchy.toggle_expanded(
            tree::NodeId{node});
        if (result.accepted)
            state.rebuild_visible_rows();
        return state.result(
            translate(result.error), result.accepted, result.changed,
            result.changed);
    }

    Result Controller::select(NodeId node, SelectionMode mode)
    {
        Implementation& state = *implementation_;
        const tree::SelectionResult result = state.hierarchy.select(
            tree::NodeId{node}, translate(mode));
        if (result.accepted)
            state.rebuild_visible_rows();
        return state.result(
            translate(result.error), result.accepted, result.changed);
    }

    Result Controller::synchronize_selection(CanonicalId entity)
    {
        Implementation& state = *implementation_;
        if (entity == invalid_canonical_id)
        {
            const bool changed = state.hierarchy.clear_selection();
            state.rebuild_visible_rows();
            return state.result(
                changed ? Code::ready : Code::unchanged, true, changed);
        }
        const auto found = state.entity_nodes.find(entity);
        if (found == state.entity_nodes.end())
            return state.result(Code::unknown_node, false, false);
        return select(found->second, SelectionMode::replace);
    }

    Result Controller::navigate(Navigation command, bool extend_selection)
    {
        Implementation& state = *implementation_;
        const tree::NavigationResult result = state.hierarchy.navigate(
            translate(command), extend_selection);
        if (result.accepted)
            state.rebuild_visible_rows();
        return state.result(
            translate(result.error), result.accepted,
            result.focus_changed || result.expansion_changed,
            result.expansion_changed);
    }

    Route Controller::route(Action action, NodeId invoked_on) const
    {
        const Implementation& state = *implementation_;
        const tree::ActionId id = action_id(action);
        if (!id)
            return Route{.code = Code::invalid_action, .action = action};

        const tree::ContextRoute routed = state.hierarchy.route_context_action(
            id, tree::NodeId{invoked_on});
        Route result{
            .code = translate(routed.error),
            .action = action,
            .invoked_on = invoked_on,
            .command = routed.command,
            .accepted = routed.accepted};
        result.targets.reserve(routed.targets.size());
        for (const tree::NodeId target : routed.targets)
        {
            const auto entity = state.node_entities.find(target.value);
            if (entity != state.node_entities.end())
                result.targets.push_back(entity->second);
        }
        if (result.accepted && result.targets.empty())
        {
            result.accepted = false;
            result.code = Code::no_targets;
        }
        return result;
    }

    LayoutPlan Controller::plan_rows(Viewport viewport) const
    {
        const tree::LayoutPlan source = implementation_->hierarchy.plan_rows({
            .row_height = viewport.row_height,
            .viewport_height = viewport.viewport_height,
            .scroll_y = viewport.scroll_y,
            .overscan_rows = viewport.overscan_rows});
        LayoutPlan result{
            .code = translate(source.error),
            .first = source.range.first,
            .past_last = source.range.past_last,
            .total_rows = source.range.total_rows,
            .content_height = source.range.content_height,
            .scroll_y = source.range.scroll_y,
            .maximum_scroll_y = source.range.maximum_scroll_y};
        result.rows.reserve(source.rows.size());
        for (const tree::RowLayout& row : source.rows)
        {
            if (row.row == nullptr)
                continue;
            result.rows.push_back(LayoutRow{
                .row_index = row.row->visible_index,
                .top = row.top,
                .bottom = row.bottom});
        }
        return result;
    }

    ScrollPlan Controller::scroll_to_entity(
        CanonicalId entity,
        Viewport viewport) const
    {
        const auto found = implementation_->entity_nodes.find(entity);
        if (found == implementation_->entity_nodes.end())
        {
            return ScrollPlan{
                .code = Code::unknown_node,
                .target = entity,
                .previous_scroll_y = viewport.scroll_y,
                .scroll_y = viewport.scroll_y};
        }
        const tree::ScrollPlan source = implementation_->hierarchy.scroll_to_visible(
            tree::NodeId{found->second},
            tree::Viewport{
                .row_height = viewport.row_height,
                .viewport_height = viewport.viewport_height,
                .scroll_y = viewport.scroll_y,
                .overscan_rows = viewport.overscan_rows});
        return ScrollPlan{
            .code = translate(source.error),
            .target = entity,
            .previous_scroll_y = source.previous_scroll_y,
            .scroll_y = source.scroll_y,
            .changed = source.changed};
    }

    NodeId Controller::node_for_entity(CanonicalId entity) const noexcept
    {
        const auto found = implementation_->entity_nodes.find(entity);
        return found == implementation_->entity_nodes.end()
            ? invalid_node_id
            : found->second;
    }

    CanonicalId Controller::entity_for_node(NodeId node) const noexcept
    {
        const auto found = implementation_->node_entities.find(node);
        return found == implementation_->node_entities.end()
            ? invalid_canonical_id
            : found->second;
    }

    const RowView* Controller::find_row(NodeId node) const noexcept
    {
        const auto found = std::find_if(
            implementation_->visible_rows.begin(),
            implementation_->visible_rows.end(),
            [node](const RowView& row) { return row.node_id == node; });
        return found == implementation_->visible_rows.end()
            ? nullptr
            : std::addressof(*found);
    }

    std::span<const RowView> Controller::rows() const noexcept
    {
        return implementation_->visible_rows;
    }

    Snapshot Controller::snapshot() const
    {
        const Implementation& state = *implementation_;
        const tree::Summary summary = state.hierarchy.summary();
        return Snapshot{
            .scene_identity = state.scene_identity,
            .filter = std::string{state.hierarchy.filter()},
            .entity_count = state.entity_count,
            .visible_rows = summary.visible_rows,
            .selected_entities = state.selected_entities().size(),
            .expanded_nodes = summary.expanded_nodes,
            .direct_matches = summary.direct_matches,
            .focused_entity = state.focused_entity(),
            .model_revision = state.hierarchy.model_revision(),
            .view_revision = state.hierarchy.view_revision()};
    }
}
