/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

module editor.hierarchy_adapter;

namespace epochengine::editor_hierarchy
{
    namespace
    {
        [[nodiscard]] std::vector<EntityRecord> fixture()
        {
            return {
                EntityRecord{
                    .canonical_id = 101u,
                    .label = "PrimaryCamera",
                    .type = "Camera",
                    .category = "Cameras",
                    .order = 10u},
                EntityRecord{
                    .canonical_id = 201u,
                    .label = "Ground",
                    .type = "Mesh",
                    .category = "World",
                    .order = 20u},
                EntityRecord{
                    .canonical_id = 202u,
                    .label = "Sun",
                    .type = "Light",
                    .category = "World",
                    .order = 30u,
                    .locked = true},
                EntityRecord{
                    .canonical_id = 301u,
                    .label = "PlayerSpawn",
                    .type = "Spawn",
                    .category = "Gameplay",
                    .order = 40u}}
            ;
        }

        [[nodiscard]] const RowView* row_for_entity(
            const Controller& controller,
            CanonicalId entity)
        {
            return controller.find_row(controller.node_for_entity(entity));
        }
    }

    ContractFailure run_contract() noexcept
    {
        try
        {
            Controller controller{};
            std::vector<EntityRecord> entities = fixture();
            Result result = controller.refresh("Scene_Launcher", entities, 201u);
            if (!result || controller.snapshot().entity_count != entities.size()
                || controller.snapshot().visible_rows != 8u
                || result.selected_entities != std::vector<CanonicalId>{201u})
            {
                return ContractFailure::admission;
            }

            const NodeId camera_node = controller.node_for_entity(101u);
            const NodeId ground_node = controller.node_for_entity(201u);
            const RowView* ground = row_for_entity(controller, 201u);
            if (camera_node == invalid_node_id || ground_node == invalid_node_id
                || ground == nullptr || ground->parent_node_id == invalid_node_id)
            {
                return ContractFailure::admission;
            }
            const NodeId world_group = ground->parent_node_id;

            std::reverse(entities.begin(), entities.end());
            result = controller.refresh("Scene_Launcher", entities, 201u);
            if (!result || controller.node_for_entity(101u) != camera_node
                || controller.node_for_entity(201u) != ground_node)
            {
                return ContractFailure::stable_identity;
            }

            result = controller.toggle_expanded(world_group);
            if (!result || !result.expansion_changed
                || row_for_entity(controller, 201u) != nullptr)
            {
                return ContractFailure::expansion_preservation;
            }
            result = controller.refresh("Scene_Launcher", entities, 201u);
            if (!result || row_for_entity(controller, 201u) != nullptr)
                return ContractFailure::expansion_preservation;

            result = controller.set_filter("ground");
            const RowView* filtered_ground = row_for_entity(controller, 201u);
            if (!result || filtered_ground == nullptr
                || !filtered_ground->direct_match
                || controller.snapshot().visible_rows != 3u)
            {
                return ContractFailure::filtering;
            }
            result = controller.clear_filter();
            if (!result)
                return ContractFailure::filtering;
            (void)controller.toggle_expanded(world_group);

            result = controller.select(camera_node, SelectionMode::replace);
            if (!result || result.focused_entity != 101u
                || result.selected_entities != std::vector<CanonicalId>{101u})
            {
                return ContractFailure::selection;
            }
            result = controller.select(ground_node, SelectionMode::toggle);
            if (!result || result.selected_entities.size() != 2u)
                return ContractFailure::selection;

            result = controller.navigate(Navigation::last, false);
            if (!result || result.focused_entity == invalid_canonical_id)
                return ContractFailure::navigation;

            const NodeId locked_node = controller.node_for_entity(202u);
            Route routed = controller.route(Action::delete_entity, locked_node);
            if (routed || routed.code != Code::locked_node)
                return ContractFailure::locked_route;
            routed = controller.route(Action::focus, locked_node);
            if (!routed || routed.targets.empty())
                return ContractFailure::locked_route;

            (void)controller.select(ground_node, SelectionMode::replace);
            routed = controller.route(Action::duplicate_entity, ground_node);
            if (!routed || routed.command != "editor.world.duplicate"
                || routed.targets != std::vector<CanonicalId>{201u})
            {
                return ContractFailure::action_route;
            }

            const LayoutPlan layout = controller.plan_rows(Viewport{
                .row_height = 24.0f,
                .viewport_height = 48.0f,
                .scroll_y = 24.0f,
                .overscan_rows = 1u});
            if (!layout || layout.total_rows != controller.rows().size()
                || layout.rows.empty() || layout.past_last <= layout.first)
            {
                return ContractFailure::virtualization;
            }

            const ScrollPlan scroll = controller.scroll_to_entity(
                301u,
                Viewport{
                    .row_height = 24.0f,
                    .viewport_height = 24.0f,
                    .scroll_y = 0.0f});
            if (!scroll || !scroll.changed || scroll.scroll_y <= 0.0f)
                return ContractFailure::scroll_visibility;

            const Snapshot before = controller.snapshot();
            entities.push_back(entities.front());
            result = controller.refresh("Scene_Launcher", entities, 201u);
            const Snapshot after = controller.snapshot();
            if (result || result.code != Code::duplicate_entity
                || before.model_revision != after.model_revision
                || before.entity_count != after.entity_count)
            {
                return ContractFailure::transactional_rejection;
            }

            return ContractFailure::none;
        }
        catch (...)
        {
            return ContractFailure::admission;
        }
    }
}

#if defined(EPOCH_EDITOR_HIERARCHY_ADAPTER_CONTRACT_MAIN)
int main()
{
    return epochengine::editor_hierarchy::run_contract()
        == epochengine::editor_hierarchy::ContractFailure::none
        ? 0
        : 1;
}
#endif
