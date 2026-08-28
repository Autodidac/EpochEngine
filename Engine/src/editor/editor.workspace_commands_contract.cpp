// SPDX-License-Identifier: LicenseRef-MIT-NoSell

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

import editor.workspace_commands;

#if defined(_MSC_VER)
#define EPOCH_CONTRACT_NOINLINE __declspec(noinline)
#else
#define EPOCH_CONTRACT_NOINLINE __attribute__((noinline))
#endif

namespace
{
    using namespace epochengine::editor_workspace_commands;

    enum class Failure : std::uint8_t
    {
        none,
        aggregate_contract,
        surface_catalog,
        command_catalog,
        deterministic_catalog,
        deterministic_facts,
        ready_commands,
        unsupported_reason,
        invalid_snapshot,
        invalid_request,
        unknown_command,
        invalid_target,
        target_kind,
        stale_workspace_identity,
        stale_workspace_revision,
        stale_surface,
        stale_surface_revision,
        stale_project_identity,
        stale_project_revision,
        stale_document_identity,
        stale_document_revision,
        stale_selection_revision,
        stale_async_generation,
        project_required,
        document_required,
        project_read_only,
        document_read_only,
        project_not_buildable,
        project_not_runnable,
        async_lane_busy,
        ai_capability_boundary,
        ai_catalog_boundary
    };

    EPOCH_CONTRACT_NOINLINE [[nodiscard]] WorkspaceSnapshot snapshot_for(
        Surface surface)
    {
        return WorkspaceSnapshot{
            .id = WorkspaceId{11u},
            .revision = 101u,
            .active_surface = surface,
            .surface = SurfaceSnapshot{
                .id = surface,
                .revision = 201u,
                .available = true},
            .project = ProjectSnapshot{
                .id = ProjectId{31u},
                .revision = 301u,
                .loaded = true,
                .writable = true,
                .buildable = true,
                .runnable = true},
            .document = DocumentSnapshot{
                .id = DocumentId{41u},
                .project = ProjectId{31u},
                .surface = surface,
                .revision = 401u,
                .selection_revision = 402u,
                .loaded = true,
                .writable = true},
            .async = AsyncSnapshot{
                .generation = 501u,
                .running = false}};
    }

    EPOCH_CONTRACT_NOINLINE [[nodiscard]] CommandTarget target_for(
        TargetKind kind)
    {
        switch (kind)
        {
        case TargetKind::none: return NoTarget{};
        case TargetKind::workspace:
            return WorkspaceTarget{WorkspaceId{11u}, 1u};
        case TargetKind::project:
            return ProjectTarget{ProjectId{31u}, 1u};
        case TargetKind::document:
            return DocumentTarget{DocumentId{41u}, 1u};
        case TargetKind::entity:
            return EntityTarget{EntityId{51u}, 1u};
        case TargetKind::element:
            return ElementTarget{ElementId{61u}, 1u};
        case TargetKind::asset:
            return AssetTarget{AssetId{71u}, 1u};
        case TargetKind::script:
            return ScriptTarget{ScriptId{81u}, 1u};
        case TargetKind::timeline_track:
            return TimelineTrackTarget{TimelineTrackId{91u}, 1u};
        case TargetKind::timeline_key:
            return TimelineKeyTarget{TimelineKeyId{101u}, 1u};
        case TargetKind::system_service:
            return SystemServiceTarget{SystemServiceId{111u}, 1u};
        case TargetKind::ai_session:
            return AiSessionTarget{AiSessionId{121u}, 1u};
        }
        return NoTarget{};
    }

    [[nodiscard]] bool rejected_with_reason(
        const CommandPlan& plan,
        PlanCode expected) noexcept
    {
        return plan.code == expected
            && !plan.ready()
            && !plan.reason.empty()
            && plan.reason == plan_code_reason(expected);
    }

    EPOCH_CONTRACT_NOINLINE [[nodiscard]] Failure check_aggregate_contract()
    {
        return epochengine::editor_workspace_commands::run_aggregate_contract()
                == ContractFailure::none
            ? Failure::none
            : Failure::aggregate_contract;
    }

    EPOCH_CONTRACT_NOINLINE [[nodiscard]] Failure check_catalogs()
    {
        constexpr std::size_t surface_count =
            static_cast<std::size_t>(Surface::count);
        constexpr std::size_t command_count =
            static_cast<std::size_t>(CommandId::count);
        std::array<bool, command_count> seen{};
        std::size_t catalogued = 0u;

        for (std::size_t surface_ordinal = 0u;
            surface_ordinal < surface_count;
            ++surface_ordinal)
        {
            const Surface surface = static_cast<Surface>(surface_ordinal);
            const auto first = commands_for_surface(surface);
            const auto second = commands_for_surface(surface);
            if (!valid_surface(surface) || surface_name(surface).empty()
                || first.empty() || first.data() != second.data()
                || first.size() != second.size())
            {
                return Failure::surface_catalog;
            }

            for (std::size_t index = 0u; index < first.size(); ++index)
            {
                const CommandDescriptor& command = first[index];
                const std::size_t command_ordinal =
                    static_cast<std::size_t>(command.id);
                if (command.surface != surface || command.stable_name.empty()
                    || command.label.empty() || command_ordinal >= command_count
                    || seen[command_ordinal]
                    || find_command(command.id) != &command)
                {
                    return Failure::command_catalog;
                }
                seen[command_ordinal] = true;
                ++catalogued;
            }
        }

        if (catalogued != command_count)
            return Failure::command_catalog;
        for (const bool present : seen)
            if (!present)
                return Failure::command_catalog;

        if (valid_surface(Surface::count)
            || !commands_for_surface(Surface::count).empty()
            || surface_name(Surface::count) != "invalid_surface")
        {
            return Failure::surface_catalog;
        }
        return Failure::none;
    }

    EPOCH_CONTRACT_NOINLINE [[nodiscard]] Failure check_ready_surface(
        Surface surface,
        std::uint64_t& request_value)
    {
        const WorkspaceSnapshot snapshot = snapshot_for(surface);
        if (snapshot.active_surface != surface || !snapshot.surface.available)
            return Failure::deterministic_facts;

        for (const CommandDescriptor& descriptor :
            commands_for_surface(surface))
        {
            const RequestId request{request_value++};
            const CommandIntent first = make_intent(
                request, descriptor.id, target_for(descriptor.target),
                snapshot);
            const CommandIntent second = make_intent(
                request, descriptor.id, target_for(descriptor.target),
                snapshot);
            if (first.request != second.request
                || first.command != second.command
                || first.surface != second.surface
                || first.observed != second.observed
                || target_kind(first.target) != target_kind(second.target)
                || !valid_target(first.target) || !valid_target(second.target))
                return Failure::deterministic_catalog;

            const CommandPlan first_plan = plan_command(snapshot, first);
            const CommandPlan second_plan = plan_command(snapshot, second);
            if (!first_plan || !second_plan
                || first_plan.code != second_plan.code
                || first_plan.descriptor != second_plan.descriptor
                || first_plan.reason != second_plan.reason)
            {
                return Failure::ready_commands;
            }
        }
        return Failure::none;
    }

    EPOCH_CONTRACT_NOINLINE [[nodiscard]] Failure check_ready_commands()
    {
        constexpr std::size_t surface_count =
            static_cast<std::size_t>(Surface::count);
        std::uint64_t request_value = 1u;
        for (std::size_t surface_ordinal = 0u;
            surface_ordinal < surface_count;
            ++surface_ordinal)
        {
            const Failure failure = check_ready_surface(
                static_cast<Surface>(surface_ordinal), request_value);
            if (failure != Failure::none)
                return failure;
        }
        return Failure::none;
    }

    EPOCH_CONTRACT_NOINLINE [[nodiscard]] Failure check_basic_refusals()
    {
        constexpr std::size_t command_count =
            static_cast<std::size_t>(CommandId::count);
        const WorkspaceSnapshot world = snapshot_for(Surface::world);
        const CommandIntent intent = make_intent(
            RequestId{701u}, CommandId::world_select_entity,
            EntityTarget{EntityId{51u}, 1u}, world);

        WorkspaceSnapshot invalid = world;
        invalid.surface.revision = 0u;
        if (!rejected_with_reason(
                plan_command(invalid, intent), PlanCode::invalid_snapshot))
            return Failure::invalid_snapshot;

        CommandIntent changed = intent;
        changed.request = {};
        if (!rejected_with_reason(
                plan_command(world, changed), PlanCode::invalid_request))
            return Failure::invalid_request;
        changed = intent;
        changed.command = static_cast<CommandId>(command_count + 10u);
        if (!rejected_with_reason(
                plan_command(world, changed), PlanCode::unknown_command))
            return Failure::unknown_command;

        changed = intent;
        changed.target = EntityTarget{EntityId{}, 1u};
        if (!rejected_with_reason(
                plan_command(world, changed), PlanCode::invalid_target))
            return Failure::invalid_target;
        changed = intent;
        changed.target = WorkspaceTarget{WorkspaceId{11u}, 1u};
        if (!rejected_with_reason(
                plan_command(world, changed), PlanCode::target_kind_mismatch))
            return Failure::target_kind;

        const WorkspaceSnapshot assets = snapshot_for(Surface::assets);
        changed = make_intent(RequestId{702u},
            CommandId::world_select_entity,
            EntityTarget{EntityId{51u}, 1u}, assets);
        const CommandPlan unsupported = plan_command(assets, changed);
        if (!rejected_with_reason(
                unsupported, PlanCode::unsupported_on_surface)
            || unsupported.descriptor == nullptr)
        {
            return Failure::unsupported_reason;
        }
        return Failure::none;
    }

    EPOCH_CONTRACT_NOINLINE [[nodiscard]] Failure
    check_workspace_surface_staleness()
    {
        const WorkspaceSnapshot world = snapshot_for(Surface::world);
        const CommandIntent intent = make_intent(
            RequestId{711u}, CommandId::world_select_entity,
            EntityTarget{EntityId{51u}, 1u}, world);

        WorkspaceSnapshot current = world;
        current.id = WorkspaceId{12u};
        if (!rejected_with_reason(plan_command(current, intent),
                PlanCode::stale_workspace_identity))
            return Failure::stale_workspace_identity;
        current = world;
        ++current.revision;
        if (!rejected_with_reason(plan_command(current, intent),
                PlanCode::stale_workspace_revision))
            return Failure::stale_workspace_revision;
        current = snapshot_for(Surface::assets);
        if (!rejected_with_reason(plan_command(current, intent),
                PlanCode::stale_surface))
            return Failure::stale_surface;
        current = world;
        ++current.surface.revision;
        if (!rejected_with_reason(plan_command(current, intent),
                PlanCode::stale_surface_revision))
            return Failure::stale_surface_revision;
        return Failure::none;
    }

    EPOCH_CONTRACT_NOINLINE [[nodiscard]] Failure
    check_project_document_staleness()
    {
        const WorkspaceSnapshot world = snapshot_for(Surface::world);
        const CommandIntent intent = make_intent(
            RequestId{712u}, CommandId::world_select_entity,
            EntityTarget{EntityId{51u}, 1u}, world);

        WorkspaceSnapshot current = world;
        current.project.id = ProjectId{32u};
        current.document.project = current.project.id;
        if (!rejected_with_reason(plan_command(current, intent),
                PlanCode::stale_project_identity))
            return Failure::stale_project_identity;
        current = world;
        ++current.project.revision;
        if (!rejected_with_reason(plan_command(current, intent),
                PlanCode::stale_project_revision))
            return Failure::stale_project_revision;
        current = world;
        current.document.id = DocumentId{42u};
        if (!rejected_with_reason(plan_command(current, intent),
                PlanCode::stale_document_identity))
            return Failure::stale_document_identity;
        current = world;
        ++current.document.revision;
        if (!rejected_with_reason(plan_command(current, intent),
                PlanCode::stale_document_revision))
            return Failure::stale_document_revision;
        return Failure::none;
    }

    EPOCH_CONTRACT_NOINLINE [[nodiscard]] Failure
    check_selection_async_staleness()
    {
        const WorkspaceSnapshot world = snapshot_for(Surface::world);
        const CommandIntent intent = make_intent(
            RequestId{713u}, CommandId::world_select_entity,
            EntityTarget{EntityId{51u}, 1u}, world);

        WorkspaceSnapshot current = world;
        ++current.document.selection_revision;
        if (!rejected_with_reason(plan_command(current, intent),
                PlanCode::stale_selection_revision))
            return Failure::stale_selection_revision;
        current = world;
        ++current.async.generation;
        if (!rejected_with_reason(plan_command(current, intent),
                PlanCode::stale_async_generation))
            return Failure::stale_async_generation;
        return Failure::none;
    }

    EPOCH_CONTRACT_NOINLINE [[nodiscard]] Failure
    check_document_requirements()
    {
        const WorkspaceSnapshot world = snapshot_for(Surface::world);

        WorkspaceSnapshot missing_project = world;
        missing_project.project = {};
        missing_project.document = {};
        CommandIntent intent = make_intent(RequestId{721u},
            CommandId::world_select_entity,
            EntityTarget{EntityId{51u}, 1u}, missing_project);
        if (!rejected_with_reason(plan_command(missing_project, intent),
                PlanCode::project_required))
            return Failure::project_required;

        WorkspaceSnapshot missing_document = world;
        missing_document.document = {};
        intent = make_intent(RequestId{722u},
            CommandId::world_select_entity,
            EntityTarget{EntityId{51u}, 1u}, missing_document);
        if (!rejected_with_reason(plan_command(missing_document, intent),
                PlanCode::document_required))
            return Failure::document_required;

        WorkspaceSnapshot project_read_only = world;
        project_read_only.project.writable = false;
        intent = make_intent(RequestId{723u},
            CommandId::world_duplicate_entity,
            EntityTarget{EntityId{51u}, 1u}, project_read_only);
        if (!rejected_with_reason(plan_command(project_read_only, intent),
                PlanCode::project_read_only))
            return Failure::project_read_only;

        WorkspaceSnapshot document_read_only = world;
        document_read_only.document.writable = false;
        intent = make_intent(RequestId{724u},
            CommandId::world_duplicate_entity,
            EntityTarget{EntityId{51u}, 1u}, document_read_only);
        if (!rejected_with_reason(plan_command(document_read_only, intent),
                PlanCode::document_read_only))
            return Failure::document_read_only;
        return Failure::none;
    }

    EPOCH_CONTRACT_NOINLINE [[nodiscard]] Failure
    check_project_and_async_requirements()
    {
        WorkspaceSnapshot project = snapshot_for(Surface::project);
        project.project.buildable = false;
        CommandIntent intent = make_intent(RequestId{731u},
            CommandId::project_build,
            ProjectTarget{ProjectId{31u}, 1u}, project);
        if (!rejected_with_reason(plan_command(project, intent),
                PlanCode::project_not_buildable))
            return Failure::project_not_buildable;

        project = snapshot_for(Surface::project);
        project.project.runnable = false;
        intent = make_intent(RequestId{732u}, CommandId::project_run,
            ProjectTarget{ProjectId{31u}, 1u}, project);
        if (!rejected_with_reason(plan_command(project, intent),
                PlanCode::project_not_runnable))
            return Failure::project_not_runnable;

        WorkspaceSnapshot ai = snapshot_for(Surface::ai_development);
        ai.async.running = true;
        intent = make_intent(RequestId{733u}, CommandId::ai_request_plan,
            AiSessionTarget{AiSessionId{121u}, 1u}, ai);
        if (!rejected_with_reason(plan_command(ai, intent),
                PlanCode::async_lane_busy))
            return Failure::async_lane_busy;
        return Failure::none;
    }

    EPOCH_CONTRACT_NOINLINE [[nodiscard]] Failure check_ai_boundary()
    {
        constexpr std::array sensitive{
            SensitiveCapability::approve_candidate,
            SensitiveCapability::promote_to_live_source,
            SensitiveCapability::publish_release,
            SensitiveCapability::git_operation,
            SensitiveCapability::unrestricted_execution};
        for (const SensitiveCapability capability : sensitive)
        {
            if (can_express_sensitive_capability(
                    Surface::ai_development, capability))
                return Failure::ai_capability_boundary;
        }

        constexpr std::array forbidden_words{
            std::string_view{"approve"},
            std::string_view{"promote"},
            std::string_view{"release"},
            std::string_view{"git"},
            std::string_view{"unrestricted"},
            std::string_view{"execute"}};
        for (const CommandDescriptor& command :
            commands_for_surface(Surface::ai_development))
        {
            if (command.surface != Surface::ai_development)
                return Failure::ai_catalog_boundary;
            for (const std::string_view word : forbidden_words)
            {
                if (command.stable_name.find(word) != std::string_view::npos
                    || command.label.find(word) != std::string_view::npos)
                    return Failure::ai_catalog_boundary;
            }
        }
        return Failure::none;
    }

    [[nodiscard]] Failure run_standalone_contract()
    {
        using Check = Failure (*)();
        constexpr std::array checks{
            static_cast<Check>(&check_aggregate_contract),
            static_cast<Check>(&check_catalogs),
            static_cast<Check>(&check_ready_commands),
            static_cast<Check>(&check_basic_refusals),
            static_cast<Check>(&check_workspace_surface_staleness),
            static_cast<Check>(&check_project_document_staleness),
            static_cast<Check>(&check_selection_async_staleness),
            static_cast<Check>(&check_document_requirements),
            static_cast<Check>(&check_project_and_async_requirements),
            static_cast<Check>(&check_ai_boundary)};
        for (const Check check : checks)
        {
            const Failure failure = check();
            if (failure != Failure::none)
                return failure;
        }
        return Failure::none;
    }
}

#undef EPOCH_CONTRACT_NOINLINE

int main()
{
    return static_cast<int>(run_standalone_contract());
}
