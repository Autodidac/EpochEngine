// SPDX-License-Identifier: LicenseRef-MIT-NoSell
module;

#include <array>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

module render.portal;

import render.camera;

namespace epochengine::render_portal
{
    namespace
    {
        [[nodiscard]] ViewDescriptor make_view(
            std::uint64_t scene,
            std::uint64_t view,
            std::uint64_t revision,
            float z) noexcept
        {
            ViewDescriptor descriptor{};
            descriptor.handle = {view, 1u};
            descriptor.scene_id = scene;
            descriptor.purpose = render_camera::ViewPurpose::portal;
            descriptor.projection = render_camera::ProjectionKind::perspective;
            descriptor.orientation = render_camera::ViewOrientation::free;
            descriptor.position = {0.0f, 1.5f, z};
            descriptor.target = {0.0f, 1.5f, z - 1.0f};
            descriptor.up = {0.0f, 1.0f, 0.0f};
            descriptor.perspective = {
                .vertical_field_of_view_radians = 1.0f,
                .near_plane = 0.05f,
                .far_plane = 2048.0f
            };
            descriptor.orthographic = {
                .vertical_size = 10.0f,
                .near_plane = 0.05f,
                .far_plane = 2048.0f
            };
            descriptor.source_revision = revision;
            descriptor.editor_navigation_enabled = false;
            return descriptor;
        }

        [[nodiscard]] PortalDescriptor make_portal(
            std::uint64_t stable_id,
            std::uint64_t source_view,
            std::uint64_t destination_view) noexcept
        {
            return {
                .stable_object_id = stable_id,
                .source_view = make_view(10u, source_view, 3u, 4.0f),
                .destination_view = make_view(10u, destination_view, 7u, -4.0f),
                .destination_clip_plane = {
                    .normal = {0.0f, 0.0f, 1.0f},
                    .signed_distance = -0.02f,
                    .guard_band = 0.002f,
                    .enabled = true
                },
                .projection_adjustment =
                    ProjectionAdjustmentIntent::oblique_near_plane,
                .compositing =
                    PortalCompositingIntent::sampled_surface_with_depth,
                .target = {
                    .width = 1024u,
                    .height = 512u,
                    .color = true,
                    .depth = true,
                    .mipmapped = false
                },
                .visible_from_destination = {},
                .enabled = true
            };
        }

        [[nodiscard]] bool same_request(
            const PortalRenderRequest& lhs,
            const PortalRenderRequest& rhs) noexcept
        {
            if (lhs.deterministic_order != rhs.deterministic_order
                || lhs.recursion_depth != rhs.recursion_depth
                || lhs.portal != rhs.portal
                || lhs.output != rhs.output
                || lhs.source_view != rhs.source_view
                || lhs.destination_view != rhs.destination_view
                || lhs.clip_plane != rhs.clip_plane
                || lhs.projection_adjustment != rhs.projection_adjustment
                || lhs.compositing != rhs.compositing
                || lhs.target != rhs.target
                || lhs.dependencies.size() != rhs.dependencies.size())
            {
                return false;
            }
            for (std::size_t index = 0u; index < lhs.dependencies.size(); ++index)
            {
                if (lhs.dependencies[index].output
                        != rhs.dependencies[index].output
                    || lhs.dependencies[index].sampled_portal
                        != rhs.dependencies[index].sampled_portal)
                {
                    return false;
                }
            }
            return true;
        }
    }

    PortalContractReport run_portal_contract_checks() noexcept
    {
        PortalContractReport report{};
        try
        {
            PortalRegistry registry{{
                .maximum_portals = 4u,
                .maximum_visibility_links_per_portal = 3u,
                .maximum_target_dimension = 2048u
            }};

            const PortalCreateResult first = registry.create(
                make_portal(101u, 1001u, 2001u));
            const PortalCreateResult second = registry.create(
                make_portal(102u, 1002u, 2002u));
            const PortalCreateResult third = registry.create(
                make_portal(103u, 1003u, 2003u));
            if (!first || !second || !third)
                return report;

            const auto first_snapshot = registry.snapshot(first.portal);
            const auto second_snapshot = registry.snapshot(second.portal);
            const auto third_snapshot = registry.snapshot(third.portal);
            if (!first_snapshot || !second_snapshot || !third_snapshot)
                return report;
            PortalDescriptor first_descriptor = first_snapshot->descriptor;
            PortalDescriptor second_descriptor = second_snapshot->descriptor;
            PortalDescriptor third_descriptor = third_snapshot->descriptor;
            first_descriptor.visible_from_destination = {second.portal};
            second_descriptor.visible_from_destination = {third.portal};
            third_descriptor.visible_from_destination = {first.portal};
            if (registry.update(first.portal, first_descriptor)
                    != RegistryStatus::success
                || registry.update(second.portal, second_descriptor)
                    != RegistryStatus::success
                || registry.update(third.portal, third_descriptor)
                    != RegistryStatus::success)
            {
                return report;
            }

            const std::array roots{first.portal, first.portal};
            const PortalPlanInput input{
                .plan_revision = 55u,
                .roots = std::span<const PortalHandle>{roots},
                .budget = {
                    .maximum_root_requests = 4u,
                    .maximum_render_requests = 8u,
                    .maximum_dependencies_per_request = 2u,
                    .maximum_recursion_depth = 4u,
                    .resolution_shift_per_depth = 1u,
                    .maximum_total_pixels = 2u * 1024u * 1024u
                }
            };
            const PortalPlan plan = registry.plan(input);
            const PortalPlan repeat = registry.plan(input);

            bool requests_match = plan.requests.size() == repeat.requests.size();
            for (std::size_t index = 0u;
                requests_match && index < plan.requests.size();
                ++index)
            {
                requests_match = same_request(
                    plan.requests[index],
                    repeat.requests[index]);
            }

            report.deterministic_request_order =
                plan.accepted()
                && requests_match
                && plan.requests.size() == 3u
                && plan.requests[0].portal == third.portal
                && plan.requests[1].portal == second.portal
                && plan.requests[2].portal == first.portal
                && plan.requests[0].deterministic_order == 0u
                && plan.requests[2].deterministic_order == 2u;
            report.recursion_and_cycle_bounded =
                plan.metrics.cycle_refusals == 1u
                && plan.metrics.duplicate_root_refusals == 1u
                && plan.metrics.deepest_recursion == 2u
                && plan.status == PortalPlanStatus::complete_with_refusals;
            if (plan.requests.size() != 3u)
                return report;
            report.clip_and_projection_intent_preserved =
                plan.requests.back().clip_plane
                    == first_descriptor.destination_clip_plane
                && plan.requests.back().projection_adjustment
                    == ProjectionAdjustmentIntent::oblique_near_plane
                && plan.requests.back().compositing
                    == PortalCompositingIntent::sampled_surface_with_depth;
            const auto root_pass = render_pass_for(plan.requests.back());
            report.render_pass_view_bound = root_pass
                && root_pass->view_binding.enabled
                && root_pass->view_binding.require_clip_planes
                && root_pass->view_binding.view.purpose
                    == render_camera::ViewPurpose::portal
                && root_pass->view_binding.view.clip_plane_count == 1u
                && render_camera::resolve(root_pass->view_binding.view).valid;
            report.logical_output_stable =
                plan.requests[0].output.valid()
                && plan.requests[0].output == repeat.requests[0].output
                && plan.requests[0].output != plan.requests[1].output;

            const std::array one_root{first.portal};
            const PortalPlan bounded = registry.plan({
                .plan_revision = 56u,
                .roots = std::span<const PortalHandle>{one_root},
                .budget = {
                    .maximum_root_requests = 1u,
                    .maximum_render_requests = 1u,
                    .maximum_dependencies_per_request = 1u,
                    .maximum_recursion_depth = 8u,
                    .resolution_shift_per_depth = 1u,
                    .maximum_total_pixels = 1024u * 512u
                }
            });
            report.request_and_pixel_budgets_enforced =
                bounded.accepted()
                && bounded.requests.size() == 1u
                && bounded.requests.front().portal == first.portal
                && bounded.metrics.request_budget_refusals == 1u
                && bounded.metrics.total_pixels <= 1024u * 512u;

            PortalPhysicalCache cache{2u, 8u * 1024u * 1024u};
            const PortalRenderRequest& root_request = plan.requests.back();
            const std::uint64_t bytes = static_cast<std::uint64_t>(
                root_request.target.width) * root_request.target.height * 8u;
            const PhysicalCacheStatus published = cache.publish({
                .logical_output = root_request.output,
                .backend_epoch = 9u,
                .opaque_graph_target = 100u,
                .opaque_color_resource = 101u,
                .opaque_depth_resource = 102u,
                .resident_bytes = bytes,
                .width = root_request.target.width,
                .height = root_request.target.height
            });
            const bool found_before_retirement =
                cache.find(root_request.output).has_value();
            const std::size_t retired = cache.retire_backend(9u);
            report.physical_state_disposable =
                published == PhysicalCacheStatus::success
                && found_before_retirement
                && retired == 1u
                && !cache.find(root_request.output).has_value()
                && cache.metrics().resident_targets == 0u
                && cache.metrics().resident_bytes == 0u;

            const PortalHandle stale = second.portal;
            const RegistryStatus destroyed = registry.destroy(stale);
            const PortalCreateResult replacement = registry.create(
                make_portal(104u, 1004u, 2004u));
            report.generation_checked_identity =
                destroyed == RegistryStatus::success
                && !registry.contains(stale)
                && replacement
                && replacement.portal.index == stale.index
                && replacement.portal.generation != stale.generation;

            const RegistryMetrics registry_metrics = registry.metrics();
            report.metrics_consistent =
                plan.metrics.requests_emitted == plan.requests.size()
                && plan.metrics.dependencies_emitted == 2u
                && registry_metrics.active_portals == 3u
                && registry_metrics.creates == 4u
                && registry_metrics.updates == 3u
                && registry_metrics.destroys == 1u;
        }
        catch (...)
        {
            return {};
        }
        return report;
    }
}
