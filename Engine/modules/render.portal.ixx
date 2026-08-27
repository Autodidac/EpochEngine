// SPDX-License-Identifier: LicenseRef-MIT-NoSell
module;

#include <algorithm>
#include <array>
#include <cmath>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

export module render.portal;

import render.math;
import render.camera;
import render.device;

export namespace epochengine::render_portal
{
    struct PortalTag final {};

    template<typename Tag>
    struct GenerationHandle final
    {
        static constexpr std::uint32_t invalid_index =
            (std::numeric_limits<std::uint32_t>::max)();

        std::uint32_t index{invalid_index};
        std::uint32_t generation{};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return index != invalid_index && generation != 0u;
        }

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return valid();
        }

        [[nodiscard]] friend constexpr auto operator<=>(
            const GenerationHandle&,
            const GenerationHandle&) noexcept = default;
    };

    using PortalHandle = GenerationHandle<PortalTag>;

    [[nodiscard]] constexpr std::uint32_t next_generation(
        std::uint32_t generation) noexcept
    {
        ++generation;
        return generation == 0u ? 1u : generation;
    }

    using ViewDescriptor = render_camera::ViewDescriptor;

    struct ClipPlaneIntent final
    {
        render_math::Float3 normal{0.0f, 0.0f, 1.0f};
        float signed_distance{};
        float guard_band{0.001f};
        bool enabled{true};

        [[nodiscard]] friend constexpr bool operator==(
            const ClipPlaneIntent&,
            const ClipPlaneIntent&) noexcept = default;
    };

    enum class ProjectionAdjustmentIntent : std::uint8_t
    {
        preserve_projection,
        oblique_near_plane
    };

    enum class PortalCompositingIntent : std::uint8_t
    {
        sampled_surface,
        sampled_surface_with_depth
    };

    struct RenderTargetIntent final
    {
        std::uint32_t width{1280u};
        std::uint32_t height{720u};
        bool color{true};
        bool depth{true};
        bool mipmapped{};

        [[nodiscard]] friend constexpr bool operator==(
            const RenderTargetIntent&,
            const RenderTargetIntent&) noexcept = default;
    };

    struct PortalDescriptor final
    {
        std::uint64_t stable_object_id{};
        ViewDescriptor source_view{};
        ViewDescriptor destination_view{};
        ClipPlaneIntent destination_clip_plane{};
        ProjectionAdjustmentIntent projection_adjustment{
            ProjectionAdjustmentIntent::oblique_near_plane};
        PortalCompositingIntent compositing{
            PortalCompositingIntent::sampled_surface_with_depth};
        RenderTargetIntent target{};
        std::vector<PortalHandle> visible_from_destination{};
        bool enabled{true};
    };

    enum class RegistryStatus : std::uint8_t
    {
        success,
        invalid_descriptor,
        invalid_handle,
        capacity_exceeded,
        visibility_capacity_exceeded,
        duplicate_visibility
    };

    struct PortalCreateResult final
    {
        RegistryStatus status{RegistryStatus::invalid_descriptor};
        PortalHandle portal{};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return status == RegistryStatus::success && portal.valid();
        }
    };

    struct PortalSnapshot final
    {
        PortalHandle portal{};
        PortalDescriptor descriptor{};
        std::uint64_t revision{};
    };

    struct RegistryMetrics final
    {
        std::size_t active_portals{};
        std::size_t peak_active_portals{};
        std::size_t active_visibility_links{};
        std::uint64_t creates{};
        std::uint64_t updates{};
        std::uint64_t destroys{};
        std::uint64_t rejected_operations{};
    };

    struct PortalRegistryConfig final
    {
        std::size_t maximum_portals{64u};
        std::size_t maximum_visibility_links_per_portal{16u};
        std::uint32_t maximum_target_dimension{8192u};
    };

    struct PortalOutputIdentity final
    {
        PortalHandle portal{};
        std::uint64_t plan_revision{};
        std::uint64_t lineage{};
        std::uint64_t source_revision{};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return portal.valid()
                && plan_revision != 0u
                && lineage != 0u
                && source_revision != 0u;
        }

        [[nodiscard]] friend constexpr auto operator<=>(
            const PortalOutputIdentity&,
            const PortalOutputIdentity&) noexcept = default;
    };

    struct PortalRequestDependency final
    {
        PortalOutputIdentity output{};
        PortalHandle sampled_portal{};
    };

    struct PortalRenderRequest final
    {
        std::uint32_t deterministic_order{};
        std::uint32_t recursion_depth{};
        PortalHandle portal{};
        PortalOutputIdentity output{};
        ViewDescriptor source_view{};
        ViewDescriptor destination_view{};
        ClipPlaneIntent clip_plane{};
        ProjectionAdjustmentIntent projection_adjustment{
            ProjectionAdjustmentIntent::preserve_projection};
        PortalCompositingIntent compositing{
            PortalCompositingIntent::sampled_surface};
        RenderTargetIntent target{};
        std::vector<PortalRequestDependency> dependencies{};
    };

    [[nodiscard]] inline std::optional<render_camera::ViewDescriptor>
        render_view_for(const PortalRenderRequest& request) noexcept
    {
        render_camera::ViewDescriptor view = request.destination_view;
        view.purpose = render_camera::ViewPurpose::portal;
        view.editor_navigation_enabled = false;
        if (request.clip_plane.enabled
            && !render_camera::add_clip_plane(
                view,
                {
                    request.clip_plane.normal,
                    request.clip_plane.signed_distance + request.clip_plane.guard_band,
                    true
                }))
        {
            return std::nullopt;
        }
        return render_camera::resolve(view).valid
            ? std::optional<render_camera::ViewDescriptor>{view}
            : std::nullopt;
    }

    [[nodiscard]] inline std::optional<epochengine::RenderPassDesc>
        render_pass_for(const PortalRenderRequest& request) noexcept
    {
        const auto view = render_view_for(request);
        if (!view)
            return std::nullopt;
        epochengine::RenderPassDesc pass{};
        pass.clear_color = true;
        pass.clear_depth = request.target.depth;
        pass.view_binding.view = *view;
        pass.view_binding.enabled = true;
        pass.view_binding.require_clip_planes =
            request.projection_adjustment
                == ProjectionAdjustmentIntent::oblique_near_plane;
        return pass;
    }

    struct PortalPlanBudget final
    {
        std::size_t maximum_root_requests{16u};
        std::size_t maximum_render_requests{64u};
        std::size_t maximum_dependencies_per_request{16u};
        std::uint32_t maximum_recursion_depth{4u};
        std::uint8_t resolution_shift_per_depth{1u};
        std::uint64_t maximum_total_pixels{64ull * 1024ull * 1024ull};
    };

    struct PortalPlanInput final
    {
        std::uint64_t plan_revision{};
        std::span<const PortalHandle> roots{};
        PortalPlanBudget budget{};
    };

    enum class PortalPlanStatus : std::uint8_t
    {
        complete,
        complete_with_refusals,
        invalid_input,
        invalid_budget
    };

    struct PortalPlanMetrics final
    {
        std::size_t roots_submitted{};
        std::size_t roots_considered{};
        std::size_t requests_emitted{};
        std::size_t dependencies_emitted{};
        std::uint64_t total_pixels{};
        std::uint32_t deepest_recursion{};
        std::uint64_t cycle_refusals{};
        std::uint64_t depth_refusals{};
        std::uint64_t request_budget_refusals{};
        std::uint64_t pixel_budget_refusals{};
        std::uint64_t dependency_budget_refusals{};
        std::uint64_t stale_handle_refusals{};
        std::uint64_t disabled_portal_refusals{};
        std::uint64_t duplicate_root_refusals{};

        [[nodiscard]] constexpr std::uint64_t total_refusals() const noexcept
        {
            return cycle_refusals
                + depth_refusals
                + request_budget_refusals
                + pixel_budget_refusals
                + dependency_budget_refusals
                + stale_handle_refusals
                + disabled_portal_refusals
                + duplicate_root_refusals;
        }
    };

    struct PortalPlan final
    {
        PortalPlanStatus status{PortalPlanStatus::invalid_input};
        std::uint64_t registry_revision{};
        std::uint64_t plan_revision{};
        std::vector<PortalRenderRequest> requests{};
        PortalPlanMetrics metrics{};

        [[nodiscard]] constexpr bool accepted() const noexcept
        {
            return status == PortalPlanStatus::complete
                || status == PortalPlanStatus::complete_with_refusals;
        }
    };

    class PortalRegistry final
    {
    public:
        explicit PortalRegistry(PortalRegistryConfig config = {})
            : config_(sanitize_config(config))
        {
            slots_.reserve(config_.maximum_portals);
        }

        [[nodiscard]] PortalCreateResult create(PortalDescriptor descriptor)
        {
            const RegistryStatus validation = validate_descriptor(descriptor, {});
            if (validation != RegistryStatus::success)
            {
                ++metrics_.rejected_operations;
                return {validation, {}};
            }

            std::size_t index = slots_.size();
            for (std::size_t candidate = 0u; candidate < slots_.size(); ++candidate)
            {
                if (!slots_[candidate].occupied)
                {
                    index = candidate;
                    break;
                }
            }

            if (index == slots_.size())
            {
                if (slots_.size() >= config_.maximum_portals)
                {
                    ++metrics_.rejected_operations;
                    return {RegistryStatus::capacity_exceeded, {}};
                }
                slots_.push_back({});
            }

            Slot& slot = slots_[index];
            if (slot.generation == 0u)
                slot.generation = 1u;
            slot.occupied = true;
            slot.descriptor = std::move(descriptor);
            slot.revision = next_revision();

            ++metrics_.active_portals;
            metrics_.peak_active_portals = (std::max)(
                metrics_.peak_active_portals,
                metrics_.active_portals);
            metrics_.active_visibility_links +=
                slot.descriptor.visible_from_destination.size();
            ++metrics_.creates;
            return {
                RegistryStatus::success,
                {static_cast<std::uint32_t>(index), slot.generation}
            };
        }

        [[nodiscard]] RegistryStatus update(
            PortalHandle portal,
            PortalDescriptor descriptor)
        {
            Slot* slot = resolve(portal);
            if (slot == nullptr)
            {
                ++metrics_.rejected_operations;
                return RegistryStatus::invalid_handle;
            }

            const RegistryStatus validation = validate_descriptor(descriptor, portal);
            if (validation != RegistryStatus::success)
            {
                ++metrics_.rejected_operations;
                return validation;
            }

            metrics_.active_visibility_links -=
                slot->descriptor.visible_from_destination.size();
            slot->descriptor = std::move(descriptor);
            slot->revision = next_revision();
            metrics_.active_visibility_links +=
                slot->descriptor.visible_from_destination.size();
            ++metrics_.updates;
            return RegistryStatus::success;
        }

        [[nodiscard]] RegistryStatus destroy(PortalHandle portal) noexcept
        {
            Slot* slot = resolve(portal);
            if (slot == nullptr)
            {
                ++metrics_.rejected_operations;
                return RegistryStatus::invalid_handle;
            }

            metrics_.active_visibility_links -=
                slot->descriptor.visible_from_destination.size();
            slot->descriptor = {};
            slot->revision = 0u;
            slot->occupied = false;
            slot->generation = next_generation(slot->generation);
            --metrics_.active_portals;
            ++metrics_.destroys;
            registry_revision_ = next_revision_value(registry_revision_);
            return RegistryStatus::success;
        }

        [[nodiscard]] bool contains(PortalHandle portal) const noexcept
        {
            return resolve(portal) != nullptr;
        }

        [[nodiscard]] std::optional<PortalSnapshot> snapshot(
            PortalHandle portal) const
        {
            const Slot* slot = resolve(portal);
            if (slot == nullptr)
                return std::nullopt;
            return PortalSnapshot{portal, slot->descriptor, slot->revision};
        }

        [[nodiscard]] RegistryMetrics metrics() const noexcept
        {
            return metrics_;
        }

        [[nodiscard]] std::uint64_t revision() const noexcept
        {
            return registry_revision_;
        }

        [[nodiscard]] PortalPlan plan(const PortalPlanInput& input) const
        {
            PortalPlan result{};
            result.registry_revision = registry_revision_;
            result.plan_revision = input.plan_revision;
            result.metrics.roots_submitted = input.roots.size();

            if (input.plan_revision == 0u)
            {
                result.status = PortalPlanStatus::invalid_input;
                return result;
            }
            if (!valid_budget(input.budget))
            {
                result.status = PortalPlanStatus::invalid_budget;
                return result;
            }

            std::vector<PortalHandle> roots(input.roots.begin(), input.roots.end());
            std::sort(roots.begin(), roots.end());
            if (roots.size() > input.budget.maximum_root_requests)
            {
                result.metrics.request_budget_refusals +=
                    roots.size() - input.budget.maximum_root_requests;
                roots.resize(input.budget.maximum_root_requests);
            }

            const auto duplicate = std::unique(roots.begin(), roots.end());
            result.metrics.duplicate_root_refusals +=
                static_cast<std::uint64_t>(std::distance(duplicate, roots.end()));
            roots.erase(duplicate, roots.end());
            result.metrics.roots_considered = roots.size();

            std::vector<PortalHandle> path{};
            path.reserve(static_cast<std::size_t>(
                input.budget.maximum_recursion_depth) + 1u);
            std::size_t reserved_requests{};
            std::uint64_t reserved_pixels{};

            const auto visit = [&]<typename Self>(
                Self&& self,
                PortalHandle portal,
                std::uint32_t depth,
                std::uint64_t parent_lineage)
                -> std::optional<PortalOutputIdentity>
            {
                if (std::find(path.begin(), path.end(), portal) != path.end())
                {
                    ++result.metrics.cycle_refusals;
                    return std::nullopt;
                }

                const Slot* slot = resolve(portal);
                if (slot == nullptr)
                {
                    ++result.metrics.stale_handle_refusals;
                    return std::nullopt;
                }
                if (!slot->descriptor.enabled)
                {
                    ++result.metrics.disabled_portal_refusals;
                    return std::nullopt;
                }
                if (depth > input.budget.maximum_recursion_depth)
                {
                    ++result.metrics.depth_refusals;
                    return std::nullopt;
                }
                if (reserved_requests >= input.budget.maximum_render_requests)
                {
                    ++result.metrics.request_budget_refusals;
                    return std::nullopt;
                }

                RenderTargetIntent target = scaled_target(
                    slot->descriptor.target,
                    depth,
                    input.budget.resolution_shift_per_depth);
                const std::uint64_t pixels =
                    static_cast<std::uint64_t>(target.width) * target.height;
                if (pixels > input.budget.maximum_total_pixels - reserved_pixels)
                {
                    ++result.metrics.pixel_budget_refusals;
                    return std::nullopt;
                }

                ++reserved_requests;
                reserved_pixels += pixels;
                path.push_back(portal);

                const std::uint64_t lineage = hash_portal(
                    parent_lineage,
                    portal,
                    slot->revision);
                const PortalOutputIdentity output{
                    .portal = portal,
                    .plan_revision = input.plan_revision,
                    .lineage = lineage == 0u ? 1u : lineage,
                    .source_revision = slot->descriptor.destination_view.source_revision
                };

                std::vector<PortalHandle> visible =
                    slot->descriptor.visible_from_destination;
                std::sort(visible.begin(), visible.end());

                std::vector<PortalRequestDependency> dependencies{};
                dependencies.reserve((std::min)(
                    visible.size(),
                    input.budget.maximum_dependencies_per_request));
                for (const PortalHandle child : visible)
                {
                    if (dependencies.size()
                        >= input.budget.maximum_dependencies_per_request)
                    {
                        ++result.metrics.dependency_budget_refusals;
                        continue;
                    }
                    if (depth == input.budget.maximum_recursion_depth)
                    {
                        ++result.metrics.depth_refusals;
                        continue;
                    }
                    const std::optional<PortalOutputIdentity> dependency =
                        self(self, child, depth + 1u, output.lineage);
                    if (dependency)
                    {
                        dependencies.push_back({*dependency, child});
                    }
                }

                path.pop_back();
                result.metrics.deepest_recursion = (std::max)(
                    result.metrics.deepest_recursion,
                    depth);
                result.metrics.dependencies_emitted += dependencies.size();
                result.metrics.total_pixels += pixels;
                result.requests.push_back({
                    .deterministic_order = 0u,
                    .recursion_depth = depth,
                    .portal = portal,
                    .output = output,
                    .source_view = slot->descriptor.source_view,
                    .destination_view = slot->descriptor.destination_view,
                    .clip_plane = slot->descriptor.destination_clip_plane,
                    .projection_adjustment = slot->descriptor.projection_adjustment,
                    .compositing = slot->descriptor.compositing,
                    .target = target,
                    .dependencies = std::move(dependencies)
                });
                return output;
            };

            constexpr std::uint64_t root_lineage = 1469598103934665603ull;
            for (const PortalHandle root : roots)
                (void)visit(visit, root, 0u, root_lineage);

            for (std::size_t index = 0u; index < result.requests.size(); ++index)
            {
                result.requests[index].deterministic_order =
                    static_cast<std::uint32_t>(index);
            }
            result.metrics.requests_emitted = result.requests.size();
            result.status = result.metrics.total_refusals() == 0u
                ? PortalPlanStatus::complete
                : PortalPlanStatus::complete_with_refusals;
            return result;
        }

    private:
        struct Slot final
        {
            PortalDescriptor descriptor{};
            std::uint64_t revision{};
            std::uint32_t generation{1u};
            bool occupied{};
        };

        [[nodiscard]] static PortalRegistryConfig sanitize_config(
            PortalRegistryConfig config) noexcept
        {
            config.maximum_portals = (std::max)(
                config.maximum_portals,
                std::size_t{1u});
            config.maximum_visibility_links_per_portal = (std::max)(
                config.maximum_visibility_links_per_portal,
                std::size_t{1u});
            config.maximum_target_dimension = (std::max)(
                config.maximum_target_dimension, 1u);
            return config;
        }

        [[nodiscard]] static bool finite(float value) noexcept
        {
            return std::isfinite(value);
        }

        [[nodiscard]] static bool valid_view(const ViewDescriptor& view) noexcept
        {
            return view.purpose == render_camera::ViewPurpose::portal
                && render_camera::resolve(view).valid;
        }

        [[nodiscard]] RegistryStatus validate_descriptor(
            const PortalDescriptor& descriptor,
            PortalHandle updating) const noexcept
        {
            if (descriptor.stable_object_id == 0u
                || !valid_view(descriptor.source_view)
                || !valid_view(descriptor.destination_view)
                || descriptor.target.width == 0u
                || descriptor.target.height == 0u
                || descriptor.target.width > config_.maximum_target_dimension
                || descriptor.target.height > config_.maximum_target_dimension
                || !descriptor.target.color)
            {
                return RegistryStatus::invalid_descriptor;
            }
            if (descriptor.destination_clip_plane.enabled
                && (!render_math::finite(descriptor.destination_clip_plane.normal)
                    || !finite(descriptor.destination_clip_plane.signed_distance)
                    || !finite(descriptor.destination_clip_plane.guard_band)
                    || descriptor.destination_clip_plane.guard_band < 0.0f
                    || render_math::length_squared(
                        descriptor.destination_clip_plane.normal) <= 1.0e-8f))
            {
                return RegistryStatus::invalid_descriptor;
            }
            if (descriptor.visible_from_destination.size()
                > config_.maximum_visibility_links_per_portal)
            {
                return RegistryStatus::visibility_capacity_exceeded;
            }

            std::vector<PortalHandle> visible =
                descriptor.visible_from_destination;
            std::sort(visible.begin(), visible.end());
            if (std::adjacent_find(visible.begin(), visible.end()) != visible.end())
                return RegistryStatus::duplicate_visibility;
            for (const PortalHandle child : visible)
            {
                if (child == updating)
                    continue;
                if (!contains(child))
                    return RegistryStatus::invalid_handle;
            }
            return RegistryStatus::success;
        }

        [[nodiscard]] static bool valid_budget(
            const PortalPlanBudget& budget) noexcept
        {
            return budget.maximum_root_requests != 0u
                && budget.maximum_render_requests != 0u
                && budget.maximum_dependencies_per_request != 0u
                && budget.maximum_total_pixels != 0u
                && budget.resolution_shift_per_depth <= 8u;
        }

        [[nodiscard]] static RenderTargetIntent scaled_target(
            RenderTargetIntent target,
            std::uint32_t depth,
            std::uint8_t shift_per_depth) noexcept
        {
            const std::uint32_t shift = (std::min)(
                depth * static_cast<std::uint32_t>(shift_per_depth),
                31u);
            target.width = (std::max)(target.width >> shift, 1u);
            target.height = (std::max)(target.height >> shift, 1u);
            return target;
        }

        [[nodiscard]] static constexpr std::uint64_t hash_word(
            std::uint64_t hash,
            std::uint64_t word) noexcept
        {
            for (std::uint32_t byte = 0u; byte < 8u; ++byte)
            {
                hash ^= (word >> (byte * 8u)) & 0xffu;
                hash *= 1099511628211ull;
            }
            return hash;
        }

        [[nodiscard]] static constexpr std::uint64_t hash_portal(
            std::uint64_t parent,
            PortalHandle portal,
            std::uint64_t revision) noexcept
        {
            std::uint64_t hash = hash_word(parent, portal.index);
            hash = hash_word(hash, portal.generation);
            return hash_word(hash, revision);
        }

        [[nodiscard]] static constexpr std::uint64_t next_revision_value(
            std::uint64_t revision) noexcept
        {
            ++revision;
            return revision == 0u ? 1u : revision;
        }

        [[nodiscard]] std::uint64_t next_revision() noexcept
        {
            registry_revision_ = next_revision_value(registry_revision_);
            return registry_revision_;
        }

        [[nodiscard]] Slot* resolve(PortalHandle portal) noexcept
        {
            if (!portal.valid() || portal.index >= slots_.size())
                return nullptr;
            Slot& slot = slots_[portal.index];
            return slot.occupied && slot.generation == portal.generation
                ? &slot
                : nullptr;
        }

        [[nodiscard]] const Slot* resolve(PortalHandle portal) const noexcept
        {
            if (!portal.valid() || portal.index >= slots_.size())
                return nullptr;
            const Slot& slot = slots_[portal.index];
            return slot.occupied && slot.generation == portal.generation
                ? &slot
                : nullptr;
        }

        PortalRegistryConfig config_{};
        std::vector<Slot> slots_{};
        RegistryMetrics metrics_{};
        std::uint64_t registry_revision_{1u};
    };

    struct PortalPhysicalTarget final
    {
        PortalOutputIdentity logical_output{};
        std::uint64_t backend_epoch{};
        std::uint64_t opaque_graph_target{};
        std::uint64_t opaque_color_resource{};
        std::uint64_t opaque_depth_resource{};
        std::uint64_t resident_bytes{};
        std::uint32_t width{};
        std::uint32_t height{};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return logical_output.valid()
                && backend_epoch != 0u
                && opaque_graph_target != 0u
                && opaque_color_resource != 0u
                && resident_bytes != 0u
                && width != 0u
                && height != 0u;
        }
    };

    enum class PhysicalCacheStatus : std::uint8_t
    {
        success,
        invalid_target,
        capacity_exceeded,
        byte_budget_exceeded,
        not_found
    };

    struct PhysicalCacheMetrics final
    {
        std::size_t resident_targets{};
        std::size_t peak_resident_targets{};
        std::uint64_t resident_bytes{};
        std::uint64_t peak_resident_bytes{};
        std::uint64_t publishes{};
        std::uint64_t replacements{};
        std::uint64_t retires{};
        std::uint64_t rejected_publishes{};
    };

    class PortalPhysicalCache final
    {
    public:
        explicit PortalPhysicalCache(
            std::size_t maximum_targets = 64u,
            std::uint64_t maximum_bytes = 256ull * 1024ull * 1024ull) noexcept
            : maximum_targets_((std::max)(maximum_targets, std::size_t{1u}))
            , maximum_bytes_((std::max)(maximum_bytes, std::uint64_t{1u}))
        {
        }

        [[nodiscard]] PhysicalCacheStatus publish(PortalPhysicalTarget target)
        {
            if (!target.valid())
            {
                ++metrics_.rejected_publishes;
                return PhysicalCacheStatus::invalid_target;
            }

            const auto existing = std::find_if(
                targets_.begin(),
                targets_.end(),
                [&](const PortalPhysicalTarget& candidate)
                {
                    return candidate.logical_output == target.logical_output;
                });
            const std::uint64_t replaced_bytes = existing == targets_.end()
                ? 0u
                : existing->resident_bytes;
            const std::uint64_t candidate_bytes =
                metrics_.resident_bytes - replaced_bytes + target.resident_bytes;
            if (candidate_bytes > maximum_bytes_)
            {
                ++metrics_.rejected_publishes;
                return PhysicalCacheStatus::byte_budget_exceeded;
            }
            if (existing == targets_.end() && targets_.size() >= maximum_targets_)
            {
                ++metrics_.rejected_publishes;
                return PhysicalCacheStatus::capacity_exceeded;
            }

            if (existing == targets_.end())
            {
                targets_.push_back(std::move(target));
                ++metrics_.publishes;
            }
            else
            {
                *existing = std::move(target);
                ++metrics_.replacements;
            }
            metrics_.resident_targets = targets_.size();
            metrics_.resident_bytes = candidate_bytes;
            metrics_.peak_resident_targets = (std::max)(
                metrics_.peak_resident_targets,
                metrics_.resident_targets);
            metrics_.peak_resident_bytes = (std::max)(
                metrics_.peak_resident_bytes,
                metrics_.resident_bytes);
            return PhysicalCacheStatus::success;
        }

        [[nodiscard]] std::optional<PortalPhysicalTarget> find(
            const PortalOutputIdentity& output) const
        {
            const auto found = std::find_if(
                targets_.begin(),
                targets_.end(),
                [&](const PortalPhysicalTarget& candidate)
                {
                    return candidate.logical_output == output;
                });
            return found == targets_.end()
                ? std::nullopt
                : std::optional<PortalPhysicalTarget>{*found};
        }

        [[nodiscard]] std::size_t retire_backend(
            std::uint64_t backend_epoch) noexcept
        {
            std::uint64_t retired_bytes{};
            std::size_t retired_count{};
            const auto first_retired = std::remove_if(
                targets_.begin(),
                targets_.end(),
                [&](const PortalPhysicalTarget& candidate)
                {
                    if (candidate.backend_epoch != backend_epoch)
                        return false;
                    retired_bytes += candidate.resident_bytes;
                    ++retired_count;
                    return true;
                });
            targets_.erase(first_retired, targets_.end());
            metrics_.resident_targets = targets_.size();
            metrics_.resident_bytes -= retired_bytes;
            metrics_.retires += retired_count;
            return retired_count;
        }

        void clear() noexcept
        {
            metrics_.retires += targets_.size();
            targets_.clear();
            metrics_.resident_targets = 0u;
            metrics_.resident_bytes = 0u;
        }

        [[nodiscard]] PhysicalCacheMetrics metrics() const noexcept
        {
            return metrics_;
        }

    private:
        std::size_t maximum_targets_{};
        std::uint64_t maximum_bytes_{};
        std::vector<PortalPhysicalTarget> targets_{};
        PhysicalCacheMetrics metrics_{};
    };

    struct PortalContractReport final
    {
        bool generation_checked_identity{};
        bool deterministic_request_order{};
        bool recursion_and_cycle_bounded{};
        bool clip_and_projection_intent_preserved{};
        bool render_pass_view_bound{};
        bool logical_output_stable{};
        bool request_and_pixel_budgets_enforced{};
        bool physical_state_disposable{};
        bool metrics_consistent{};

        [[nodiscard]] constexpr bool passed() const noexcept
        {
            return generation_checked_identity
                && deterministic_request_order
                && recursion_and_cycle_bounded
                && clip_and_projection_intent_preserved
                && render_pass_view_bound
                && logical_output_stable
                && request_and_pixel_budgets_enforced
                && physical_state_disposable
                && metrics_consistent;
        }
    };

    [[nodiscard]] PortalContractReport run_portal_contract_checks() noexcept;
}
