// SPDX-License-Identifier: LicenseRef-MIT-NoSell
module;

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

export module editor.workspace_commands;

export namespace epochengine::editor_workspace_commands
{
    template <typename Tag>
    struct StableId final
    {
        std::uint64_t value{};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return value != 0u;
        }

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return valid();
        }

        [[nodiscard]] friend constexpr auto operator<=> (
            const StableId&,
            const StableId&) noexcept = default;
    };

    struct WorkspaceTag final {};
    struct ProjectTag final {};
    struct DocumentTag final {};
    struct EntityTag final {};
    struct ElementTag final {};
    struct AssetTag final {};
    struct ScriptTag final {};
    struct TimelineTrackTag final {};
    struct TimelineKeyTag final {};
    struct SystemServiceTag final {};
    struct AiSessionTag final {};
    struct RequestTag final {};

    using WorkspaceId = StableId<WorkspaceTag>;
    using ProjectId = StableId<ProjectTag>;
    using DocumentId = StableId<DocumentTag>;
    using EntityId = StableId<EntityTag>;
    using ElementId = StableId<ElementTag>;
    using AssetId = StableId<AssetTag>;
    using ScriptId = StableId<ScriptTag>;
    using TimelineTrackId = StableId<TimelineTrackTag>;
    using TimelineKeyId = StableId<TimelineKeyTag>;
    using SystemServiceId = StableId<SystemServiceTag>;
    using AiSessionId = StableId<AiSessionTag>;
    using RequestId = StableId<RequestTag>;

    enum class Surface : std::uint8_t
    {
        world,
        gui_canvas,
        forest_factory,
        plant_lab,
        timeline,
        project,
        assets,
        ai_development,
        systems,
        count
    };

    [[nodiscard]] constexpr std::string_view surface_name(
        Surface surface) noexcept
    {
        switch (surface)
        {
        case Surface::world: return "world";
        case Surface::gui_canvas: return "gui_canvas";
        case Surface::forest_factory: return "forest_factory";
        case Surface::plant_lab: return "plant_lab";
        case Surface::timeline: return "timeline";
        case Surface::project: return "project";
        case Surface::assets: return "assets";
        case Surface::ai_development: return "ai_development";
        case Surface::systems: return "systems";
        case Surface::count: break;
        }
        return "invalid_surface";
    }

    [[nodiscard]] constexpr bool valid_surface(Surface surface) noexcept
    {
        return surface < Surface::count;
    }

    enum class CommandId : std::uint16_t
    {
        world_undo,
        world_redo,
        world_select_entity,
        world_focus_entity,
        world_create_entity,
        world_duplicate_entity,
        world_delete_entity,
        world_set_transform,
        world_toggle_visibility,
        world_save_document,

        gui_undo,
        gui_redo,
        gui_select_element,
        gui_focus_element,
        gui_create_element,
        gui_duplicate_element,
        gui_delete_element,
        gui_set_properties,
        gui_align_selection,
        gui_preview_document,
        gui_save_document,

        forest_select_element,
        forest_focus_element,
        forest_create_element,
        forest_delete_element,
        forest_edit_rules,
        forest_generate_preview,
        forest_bake_asset,
        forest_save_document,

        plant_undo,
        plant_redo,
        plant_select_element,
        plant_focus_element,
        plant_edit_species,
        plant_edit_growth,
        plant_generate_preview,
        plant_save_document,

        timeline_select_track,
        timeline_add_track,
        timeline_delete_track,
        timeline_add_key,
        timeline_delete_key,
        timeline_set_playhead,
        timeline_toggle_playback,
        timeline_save_document,

        project_open,
        project_save,
        project_edit_defaults,
        project_edit_input_map,
        project_configure_packages,
        project_build,
        project_run,

        assets_open,
        assets_import,
        assets_create,
        assets_rename,
        assets_delete,
        assets_reimport,

        ai_new_goal,
        ai_pause_goal,
        ai_resume_goal,
        ai_request_plan,
        ai_share_curated_context,
        ai_request_bounded_iteration,
        ai_cancel_operation,
        ai_open_evidence,

        systems_refresh,
        systems_select_service,
        systems_pause_task,
        systems_resume_task,
        systems_cancel_task,
        systems_open_evidence,

        count
    };

    enum class TargetKind : std::uint8_t
    {
        none,
        workspace,
        project,
        document,
        entity,
        element,
        asset,
        script,
        timeline_track,
        timeline_key,
        system_service,
        ai_session
    };

    struct NoTarget final
    {
        [[nodiscard]] friend constexpr bool operator==(
            const NoTarget&,
            const NoTarget&) noexcept = default;
    };

    template <typename Id>
    struct VersionedTarget final
    {
        Id id{};
        std::uint64_t generation{};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return id.valid() && generation != 0u;
        }

        [[nodiscard]] friend constexpr auto operator<=> (
            const VersionedTarget&,
            const VersionedTarget&) noexcept = default;
    };

    using WorkspaceTarget = VersionedTarget<WorkspaceId>;
    using ProjectTarget = VersionedTarget<ProjectId>;
    using DocumentTarget = VersionedTarget<DocumentId>;
    using EntityTarget = VersionedTarget<EntityId>;
    using ElementTarget = VersionedTarget<ElementId>;
    using AssetTarget = VersionedTarget<AssetId>;
    using ScriptTarget = VersionedTarget<ScriptId>;
    using TimelineTrackTarget = VersionedTarget<TimelineTrackId>;
    using TimelineKeyTarget = VersionedTarget<TimelineKeyId>;
    using SystemServiceTarget = VersionedTarget<SystemServiceId>;
    using AiSessionTarget = VersionedTarget<AiSessionId>;

    using CommandTarget = std::variant<
        NoTarget,
        WorkspaceTarget,
        ProjectTarget,
        DocumentTarget,
        EntityTarget,
        ElementTarget,
        AssetTarget,
        ScriptTarget,
        TimelineTrackTarget,
        TimelineKeyTarget,
        SystemServiceTarget,
        AiSessionTarget>;

    [[nodiscard]] constexpr TargetKind target_kind(
        const CommandTarget& target) noexcept
    {
        return static_cast<TargetKind>(target.index());
    }

    [[nodiscard]] inline bool valid_target(const CommandTarget& target) noexcept
    {
        return std::visit([](const auto& value) noexcept
        {
            using Value = std::remove_cvref_t<decltype(value)>;
            if constexpr (std::is_same_v<Value, NoTarget>)
                return true;
            else
                return value.valid();
        }, target);
    }

    enum class Authority : std::uint8_t
    {
        observe,
        bounded_mutation,
        bounded_async_request
    };

    enum class SensitiveCapability : std::uint8_t
    {
        approve_candidate,
        promote_to_live_source,
        publish_release,
        git_operation,
        unrestricted_execution
    };

    // Sensitive operations deliberately have no CommandId representation.
    // The surface command plane can request bounded work, but it cannot approve
    // or promote the result of that work.
    [[nodiscard]] constexpr bool can_express_sensitive_capability(
        Surface,
        SensitiveCapability) noexcept
    {
        return false;
    }

    struct CommandRequirements final
    {
        bool project{};
        bool document{};
        bool writable_project{};
        bool writable_document{};
        bool buildable_project{};
        bool runnable_project{};
        bool idle_async_lane{};
    };

    struct CommandDescriptor final
    {
        CommandId id{CommandId::world_select_entity};
        Surface surface{Surface::world};
        TargetKind target{TargetKind::none};
        Authority authority{Authority::observe};
        std::string_view stable_name{};
        std::string_view label{};
        CommandRequirements requirements{};
    };

    namespace detail
    {
        inline constexpr CommandRequirements project_read{
            .project = true};
        inline constexpr CommandRequirements project_write{
            .project = true,
            .writable_project = true};
        inline constexpr CommandRequirements document_read{
            .project = true,
            .document = true};
        inline constexpr CommandRequirements document_write{
            .project = true,
            .document = true,
            .writable_project = true,
            .writable_document = true};
        inline constexpr CommandRequirements document_async{
            .project = true,
            .document = true,
            .writable_project = true,
            .writable_document = true,
            .idle_async_lane = true};
        inline constexpr CommandRequirements build_async{
            .project = true,
            .buildable_project = true,
            .idle_async_lane = true};
        inline constexpr CommandRequirements run_async{
            .project = true,
            .runnable_project = true,
            .idle_async_lane = true};
        inline constexpr CommandRequirements project_write_async{
            .project = true,
            .writable_project = true,
            .idle_async_lane = true};
        inline constexpr CommandRequirements async_only{
            .idle_async_lane = true};

#define EPOCH_WORKSPACE_COMMAND(command_id, command_surface, target_kind_value, authority_value, label_value, requirements_value) \
        CommandDescriptor{CommandId::command_id, Surface::command_surface, TargetKind::target_kind_value, Authority::authority_value, #command_id, label_value, requirements_value}

        inline constexpr std::array world_commands{
            EPOCH_WORKSPACE_COMMAND(world_undo, world, document,
                bounded_mutation, "Undo world edit", document_write),
            EPOCH_WORKSPACE_COMMAND(world_redo, world, document,
                bounded_mutation, "Redo world edit", document_write),
            EPOCH_WORKSPACE_COMMAND(world_select_entity, world, entity, observe,
                "Select entity", document_read),
            EPOCH_WORKSPACE_COMMAND(world_focus_entity, world, entity, observe,
                "Focus entity", document_read),
            EPOCH_WORKSPACE_COMMAND(world_create_entity, world, document,
                bounded_mutation, "Create entity", document_write),
            EPOCH_WORKSPACE_COMMAND(world_duplicate_entity, world, entity,
                bounded_mutation, "Duplicate entity", document_write),
            EPOCH_WORKSPACE_COMMAND(world_delete_entity, world, entity,
                bounded_mutation, "Delete entity", document_write),
            EPOCH_WORKSPACE_COMMAND(world_set_transform, world, entity,
                bounded_mutation, "Set transform", document_write),
            EPOCH_WORKSPACE_COMMAND(world_toggle_visibility, world, entity,
                bounded_mutation, "Toggle visibility", document_write),
            EPOCH_WORKSPACE_COMMAND(world_save_document, world, document,
                bounded_mutation, "Save world", document_write)};

        inline constexpr std::array gui_commands{
            EPOCH_WORKSPACE_COMMAND(gui_undo, gui_canvas, document,
                bounded_mutation, "Undo GUI edit", document_write),
            EPOCH_WORKSPACE_COMMAND(gui_redo, gui_canvas, document,
                bounded_mutation, "Redo GUI edit", document_write),
            EPOCH_WORKSPACE_COMMAND(gui_select_element, gui_canvas, element,
                observe, "Select GUI element", document_read),
            EPOCH_WORKSPACE_COMMAND(gui_focus_element, gui_canvas, element,
                observe, "Focus GUI element", document_read),
            EPOCH_WORKSPACE_COMMAND(gui_create_element, gui_canvas, document,
                bounded_mutation, "Create GUI element", document_write),
            EPOCH_WORKSPACE_COMMAND(gui_duplicate_element, gui_canvas, element,
                bounded_mutation, "Duplicate GUI element", document_write),
            EPOCH_WORKSPACE_COMMAND(gui_delete_element, gui_canvas, element,
                bounded_mutation, "Delete GUI element", document_write),
            EPOCH_WORKSPACE_COMMAND(gui_set_properties, gui_canvas, element,
                bounded_mutation, "Set GUI properties", document_write),
            EPOCH_WORKSPACE_COMMAND(gui_align_selection, gui_canvas, document,
                bounded_mutation, "Align GUI selection", document_write),
            EPOCH_WORKSPACE_COMMAND(gui_preview_document, gui_canvas, document,
                bounded_async_request, "Preview GUI document", document_async),
            EPOCH_WORKSPACE_COMMAND(gui_save_document, gui_canvas, document,
                bounded_mutation, "Save GUI document", document_write)};

        inline constexpr std::array forest_commands{
            EPOCH_WORKSPACE_COMMAND(forest_select_element, forest_factory,
                element, observe, "Select forest element", document_read),
            EPOCH_WORKSPACE_COMMAND(forest_focus_element, forest_factory,
                element, observe, "Focus forest element", document_read),
            EPOCH_WORKSPACE_COMMAND(forest_create_element, forest_factory,
                document, bounded_mutation, "Create forest element",
                document_write),
            EPOCH_WORKSPACE_COMMAND(forest_delete_element, forest_factory,
                element, bounded_mutation, "Delete forest element",
                document_write),
            EPOCH_WORKSPACE_COMMAND(forest_edit_rules, forest_factory, document,
                bounded_mutation, "Edit forest rules", document_write),
            EPOCH_WORKSPACE_COMMAND(forest_generate_preview, forest_factory,
                document, bounded_async_request, "Generate forest preview",
                document_async),
            EPOCH_WORKSPACE_COMMAND(forest_bake_asset, forest_factory, document,
                bounded_async_request, "Bake forest asset", document_async),
            EPOCH_WORKSPACE_COMMAND(forest_save_document, forest_factory,
                document, bounded_mutation, "Save forest document",
                document_write)};

        inline constexpr std::array plant_commands{
            EPOCH_WORKSPACE_COMMAND(plant_undo, plant_lab, document,
                bounded_mutation, "Undo Plant Lab edit", document_write),
            EPOCH_WORKSPACE_COMMAND(plant_redo, plant_lab, document,
                bounded_mutation, "Redo Plant Lab edit", document_write),
            EPOCH_WORKSPACE_COMMAND(plant_select_element, plant_lab, element,
                observe, "Select plant element", document_read),
            EPOCH_WORKSPACE_COMMAND(plant_focus_element, plant_lab, element,
                observe, "Focus plant element", document_read),
            EPOCH_WORKSPACE_COMMAND(plant_edit_species, plant_lab, document,
                bounded_mutation, "Edit plant species", document_write),
            EPOCH_WORKSPACE_COMMAND(plant_edit_growth, plant_lab, document,
                bounded_mutation, "Edit plant growth", document_write),
            EPOCH_WORKSPACE_COMMAND(plant_generate_preview, plant_lab, document,
                bounded_async_request, "Generate plant preview",
                document_async),
            EPOCH_WORKSPACE_COMMAND(plant_save_document, plant_lab, document,
                bounded_mutation, "Save plant document", document_write)};

        inline constexpr std::array timeline_commands{
            EPOCH_WORKSPACE_COMMAND(timeline_select_track, timeline,
                timeline_track, observe, "Select timeline track", document_read),
            EPOCH_WORKSPACE_COMMAND(timeline_add_track, timeline, document,
                bounded_mutation, "Add timeline track", document_write),
            EPOCH_WORKSPACE_COMMAND(timeline_delete_track, timeline,
                timeline_track, bounded_mutation, "Delete timeline track",
                document_write),
            EPOCH_WORKSPACE_COMMAND(timeline_add_key, timeline, timeline_track,
                bounded_mutation, "Add timeline key", document_write),
            EPOCH_WORKSPACE_COMMAND(timeline_delete_key, timeline, timeline_key,
                bounded_mutation, "Delete timeline key", document_write),
            EPOCH_WORKSPACE_COMMAND(timeline_set_playhead, timeline, document,
                bounded_mutation, "Set timeline playhead", document_write),
            EPOCH_WORKSPACE_COMMAND(timeline_toggle_playback, timeline, document,
                bounded_async_request, "Toggle timeline playback",
                document_async),
            EPOCH_WORKSPACE_COMMAND(timeline_save_document, timeline, document,
                bounded_mutation, "Save timeline", document_write)};

        inline constexpr std::array project_commands{
            EPOCH_WORKSPACE_COMMAND(project_open, project, project, observe,
                "Open project", project_read),
            EPOCH_WORKSPACE_COMMAND(project_save, project, project,
                bounded_mutation, "Save project", project_write),
            EPOCH_WORKSPACE_COMMAND(project_edit_defaults, project, project,
                bounded_mutation, "Edit project defaults", project_write),
            EPOCH_WORKSPACE_COMMAND(project_edit_input_map, project, project,
                bounded_mutation, "Edit input map", project_write),
            EPOCH_WORKSPACE_COMMAND(project_configure_packages, project, project,
                bounded_mutation, "Configure packages", project_write),
            EPOCH_WORKSPACE_COMMAND(project_build, project, project,
                bounded_async_request, "Build project", build_async),
            EPOCH_WORKSPACE_COMMAND(project_run, project, project,
                bounded_async_request, "Run project", run_async)};

        inline constexpr std::array asset_commands{
            EPOCH_WORKSPACE_COMMAND(assets_open, assets, asset, observe,
                "Open asset", project_read),
            EPOCH_WORKSPACE_COMMAND(assets_import, assets, project,
                bounded_async_request, "Import asset", project_write_async),
            EPOCH_WORKSPACE_COMMAND(assets_create, assets, project,
                bounded_mutation, "Create asset", project_write),
            EPOCH_WORKSPACE_COMMAND(assets_rename, assets, asset,
                bounded_mutation, "Rename asset", project_write),
            EPOCH_WORKSPACE_COMMAND(assets_delete, assets, asset,
                bounded_mutation, "Delete asset", project_write),
            EPOCH_WORKSPACE_COMMAND(assets_reimport, assets, asset,
                bounded_async_request, "Reimport asset", project_write_async)};

        inline constexpr std::array ai_commands{
            EPOCH_WORKSPACE_COMMAND(ai_new_goal, ai_development, workspace,
                bounded_mutation, "Create bounded AI goal", {}),
            EPOCH_WORKSPACE_COMMAND(ai_pause_goal, ai_development, ai_session,
                bounded_mutation, "Pause AI goal", {}),
            EPOCH_WORKSPACE_COMMAND(ai_resume_goal, ai_development, ai_session,
                bounded_mutation, "Resume AI goal", {}),
            EPOCH_WORKSPACE_COMMAND(ai_request_plan, ai_development, ai_session,
                bounded_async_request, "Request bounded plan", async_only),
            EPOCH_WORKSPACE_COMMAND(ai_share_curated_context, ai_development,
                ai_session, bounded_mutation, "Share curated context", {}),
            EPOCH_WORKSPACE_COMMAND(ai_request_bounded_iteration,
                ai_development, ai_session, bounded_async_request,
                "Request bounded iteration", async_only),
            EPOCH_WORKSPACE_COMMAND(ai_cancel_operation, ai_development,
                ai_session, bounded_mutation, "Cancel AI operation", {}),
            EPOCH_WORKSPACE_COMMAND(ai_open_evidence, ai_development,
                ai_session, observe, "Open AI evidence", {})};

        inline constexpr std::array systems_commands{
            EPOCH_WORKSPACE_COMMAND(systems_refresh, systems, workspace,
                observe, "Refresh systems", {}),
            EPOCH_WORKSPACE_COMMAND(systems_select_service, systems,
                system_service, observe, "Select system service", {}),
            EPOCH_WORKSPACE_COMMAND(systems_pause_task, systems, system_service,
                bounded_mutation, "Pause system task", {}),
            EPOCH_WORKSPACE_COMMAND(systems_resume_task, systems, system_service,
                bounded_mutation, "Resume system task", {}),
            EPOCH_WORKSPACE_COMMAND(systems_cancel_task, systems, system_service,
                bounded_mutation, "Cancel system task", {}),
            EPOCH_WORKSPACE_COMMAND(systems_open_evidence, systems,
                system_service, observe, "Open system evidence", {})};

#undef EPOCH_WORKSPACE_COMMAND
    }

    [[nodiscard]] constexpr std::span<const CommandDescriptor>
        commands_for_surface(Surface surface) noexcept
    {
        switch (surface)
        {
        case Surface::world: return detail::world_commands;
        case Surface::gui_canvas: return detail::gui_commands;
        case Surface::forest_factory: return detail::forest_commands;
        case Surface::plant_lab: return detail::plant_commands;
        case Surface::timeline: return detail::timeline_commands;
        case Surface::project: return detail::project_commands;
        case Surface::assets: return detail::asset_commands;
        case Surface::ai_development: return detail::ai_commands;
        case Surface::systems: return detail::systems_commands;
        case Surface::count: break;
        }
        return {};
    }

    [[nodiscard]] constexpr const CommandDescriptor* find_command(
        CommandId id) noexcept
    {
        for (std::uint8_t ordinal = 0u;
            ordinal < static_cast<std::uint8_t>(Surface::count);
            ++ordinal)
        {
            for (const CommandDescriptor& descriptor : commands_for_surface(
                static_cast<Surface>(ordinal)))
            {
                if (descriptor.id == id)
                    return &descriptor;
            }
        }
        return nullptr;
    }

    struct SurfaceSnapshot final
    {
        Surface id{Surface::world};
        std::uint64_t revision{};
        bool available{};
    };

    struct ProjectSnapshot final
    {
        ProjectId id{};
        std::uint64_t revision{};
        bool loaded{};
        bool writable{};
        bool buildable{};
        bool runnable{};
    };

    struct DocumentSnapshot final
    {
        DocumentId id{};
        ProjectId project{};
        Surface surface{Surface::world};
        std::uint64_t revision{};
        std::uint64_t selection_revision{};
        bool loaded{};
        bool writable{};
    };

    struct AsyncSnapshot final
    {
        std::uint64_t generation{};
        bool running{};
    };

    struct WorkspaceSnapshot final
    {
        WorkspaceId id{};
        std::uint64_t revision{};
        Surface active_surface{Surface::world};
        SurfaceSnapshot surface{};
        ProjectSnapshot project{};
        DocumentSnapshot document{};
        AsyncSnapshot async{};
    };

    [[nodiscard]] constexpr bool valid_snapshot(
        const WorkspaceSnapshot& snapshot) noexcept
    {
        if (!snapshot.id.valid() || snapshot.revision == 0u
            || !valid_surface(snapshot.active_surface)
            || !snapshot.surface.available
            || snapshot.surface.id != snapshot.active_surface
            || snapshot.surface.revision == 0u
            || snapshot.async.generation == 0u)
        {
            return false;
        }
        if (snapshot.project.loaded
            != (snapshot.project.id.valid()
                && snapshot.project.revision != 0u))
        {
            return false;
        }
        if (!snapshot.project.loaded
            && (snapshot.project.writable || snapshot.project.buildable
                || snapshot.project.runnable))
        {
            return false;
        }
        if (snapshot.document.loaded
            != (snapshot.document.id.valid()
                && snapshot.document.project.valid()
                && snapshot.document.revision != 0u
                && snapshot.document.selection_revision != 0u))
        {
            return false;
        }
        if (!snapshot.document.loaded && snapshot.document.writable)
            return false;
        return !snapshot.document.loaded
            || (snapshot.project.loaded
                && snapshot.document.project == snapshot.project.id
                && snapshot.document.surface == snapshot.active_surface);
    }

    struct ObservedFacts final
    {
        WorkspaceId workspace{};
        std::uint64_t workspace_revision{};
        Surface surface{Surface::world};
        std::uint64_t surface_revision{};
        ProjectId project{};
        std::uint64_t project_revision{};
        DocumentId document{};
        std::uint64_t document_revision{};
        std::uint64_t selection_revision{};
        std::uint64_t async_generation{};

        [[nodiscard]] friend constexpr auto operator<=> (
            const ObservedFacts&,
            const ObservedFacts&) noexcept = default;
    };

    [[nodiscard]] constexpr ObservedFacts capture_facts(
        const WorkspaceSnapshot& snapshot) noexcept
    {
        return ObservedFacts{
            .workspace = snapshot.id,
            .workspace_revision = snapshot.revision,
            .surface = snapshot.active_surface,
            .surface_revision = snapshot.surface.revision,
            .project = snapshot.project.id,
            .project_revision = snapshot.project.revision,
            .document = snapshot.document.id,
            .document_revision = snapshot.document.revision,
            .selection_revision = snapshot.document.selection_revision,
            .async_generation = snapshot.async.generation};
    }

    struct CommandIntent final
    {
        RequestId request{};
        CommandId command{CommandId::world_select_entity};
        Surface surface{Surface::world};
        CommandTarget target{};
        ObservedFacts observed{};

        [[nodiscard]] friend bool operator==(
            const CommandIntent&,
            const CommandIntent&) noexcept = default;
    };

    [[nodiscard]] inline CommandIntent make_intent(
        RequestId request,
        CommandId command,
        CommandTarget target,
        const WorkspaceSnapshot& snapshot)
    {
        return CommandIntent{
            .request = request,
            .command = command,
            .surface = snapshot.active_surface,
            .target = std::move(target),
            .observed = capture_facts(snapshot)};
    }

    enum class PlanCode : std::uint8_t
    {
        ready,
        invalid_snapshot,
        invalid_request,
        unknown_command,
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
        unsupported_on_surface,
        invalid_target,
        target_kind_mismatch,
        project_required,
        document_required,
        project_read_only,
        document_read_only,
        project_not_buildable,
        project_not_runnable,
        async_lane_busy
    };

    [[nodiscard]] constexpr std::string_view plan_code_name(
        PlanCode code) noexcept
    {
        switch (code)
        {
        case PlanCode::ready: return "ready";
        case PlanCode::invalid_snapshot: return "invalid_snapshot";
        case PlanCode::invalid_request: return "invalid_request";
        case PlanCode::unknown_command: return "unknown_command";
        case PlanCode::stale_workspace_identity:
            return "stale_workspace_identity";
        case PlanCode::stale_workspace_revision:
            return "stale_workspace_revision";
        case PlanCode::stale_surface: return "stale_surface";
        case PlanCode::stale_surface_revision:
            return "stale_surface_revision";
        case PlanCode::stale_project_identity:
            return "stale_project_identity";
        case PlanCode::stale_project_revision:
            return "stale_project_revision";
        case PlanCode::stale_document_identity:
            return "stale_document_identity";
        case PlanCode::stale_document_revision:
            return "stale_document_revision";
        case PlanCode::stale_selection_revision:
            return "stale_selection_revision";
        case PlanCode::stale_async_generation:
            return "stale_async_generation";
        case PlanCode::unsupported_on_surface:
            return "unsupported_on_surface";
        case PlanCode::invalid_target: return "invalid_target";
        case PlanCode::target_kind_mismatch: return "target_kind_mismatch";
        case PlanCode::project_required: return "project_required";
        case PlanCode::document_required: return "document_required";
        case PlanCode::project_read_only: return "project_read_only";
        case PlanCode::document_read_only: return "document_read_only";
        case PlanCode::project_not_buildable: return "project_not_buildable";
        case PlanCode::project_not_runnable: return "project_not_runnable";
        case PlanCode::async_lane_busy: return "async_lane_busy";
        }
        return "unknown";
    }

    [[nodiscard]] constexpr std::string_view plan_code_reason(
        PlanCode code) noexcept
    {
        switch (code)
        {
        case PlanCode::ready:
            return "The bounded command intent matches the current snapshot.";
        case PlanCode::invalid_snapshot:
            return "The workspace snapshot is internally inconsistent.";
        case PlanCode::invalid_request:
            return "The command request identity is invalid.";
        case PlanCode::unknown_command:
            return "The command identifier is not in the workspace catalog.";
        case PlanCode::stale_workspace_identity:
            return "The observed workspace is no longer active.";
        case PlanCode::stale_workspace_revision:
            return "The workspace changed after the command was observed.";
        case PlanCode::stale_surface:
            return "The active workspace surface changed after observation.";
        case PlanCode::stale_surface_revision:
            return "The active surface changed after observation.";
        case PlanCode::stale_project_identity:
            return "The active project changed after observation.";
        case PlanCode::stale_project_revision:
            return "The active project revision changed after observation.";
        case PlanCode::stale_document_identity:
            return "The active document changed after observation.";
        case PlanCode::stale_document_revision:
            return "The active document revision changed after observation.";
        case PlanCode::stale_selection_revision:
            return "The document selection changed after observation.";
        case PlanCode::stale_async_generation:
            return "The asynchronous operation generation changed.";
        case PlanCode::unsupported_on_surface:
            return "The command is not supported on the active surface.";
        case PlanCode::invalid_target:
            return "The typed command target is invalid.";
        case PlanCode::target_kind_mismatch:
            return "The typed target does not match the command contract.";
        case PlanCode::project_required:
            return "The command requires an open project.";
        case PlanCode::document_required:
            return "The command requires an open document.";
        case PlanCode::project_read_only:
            return "The active project is read-only.";
        case PlanCode::document_read_only:
            return "The active document is read-only.";
        case PlanCode::project_not_buildable:
            return "The active project is not admitted for builds.";
        case PlanCode::project_not_runnable:
            return "The active project is not admitted for execution.";
        case PlanCode::async_lane_busy:
            return "A bounded asynchronous operation is already active.";
        }
        return "The command was rejected.";
    }

    struct CommandPlan final
    {
        PlanCode code{PlanCode::invalid_request};
        const CommandDescriptor* descriptor{};
        std::string_view reason{plan_code_reason(PlanCode::invalid_request)};

        [[nodiscard]] constexpr bool ready() const noexcept
        {
            return code == PlanCode::ready && descriptor != nullptr;
        }

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return ready();
        }
    };

    [[nodiscard]] constexpr CommandPlan rejected(
        PlanCode code,
        const CommandDescriptor* descriptor = nullptr) noexcept
    {
        return CommandPlan{code, descriptor, plan_code_reason(code)};
    }

    [[nodiscard]] CommandPlan plan_command(
        const WorkspaceSnapshot& snapshot,
        const CommandIntent& intent) noexcept
    {
        if (!valid_snapshot(snapshot))
            return rejected(PlanCode::invalid_snapshot);
        if (!intent.request.valid())
            return rejected(PlanCode::invalid_request);

        const CommandDescriptor* descriptor = find_command(intent.command);
        if (!descriptor)
            return rejected(PlanCode::unknown_command);

        const ObservedFacts current = capture_facts(snapshot);
        if (intent.observed.workspace != current.workspace)
            return rejected(PlanCode::stale_workspace_identity, descriptor);
        if (intent.observed.workspace_revision != current.workspace_revision)
            return rejected(PlanCode::stale_workspace_revision, descriptor);
        if (intent.surface != current.surface
            || intent.observed.surface != current.surface)
        {
            return rejected(PlanCode::stale_surface, descriptor);
        }
        if (intent.observed.surface_revision != current.surface_revision)
            return rejected(PlanCode::stale_surface_revision, descriptor);
        if (intent.observed.project != current.project)
            return rejected(PlanCode::stale_project_identity, descriptor);
        if (intent.observed.project_revision != current.project_revision)
            return rejected(PlanCode::stale_project_revision, descriptor);
        if (intent.observed.document != current.document)
            return rejected(PlanCode::stale_document_identity, descriptor);
        if (intent.observed.document_revision != current.document_revision)
            return rejected(PlanCode::stale_document_revision, descriptor);
        if (intent.observed.selection_revision != current.selection_revision)
            return rejected(PlanCode::stale_selection_revision, descriptor);
        if (intent.observed.async_generation != current.async_generation)
            return rejected(PlanCode::stale_async_generation, descriptor);
        if (descriptor->surface != snapshot.active_surface)
            return rejected(PlanCode::unsupported_on_surface, descriptor);
        if (!valid_target(intent.target))
            return rejected(PlanCode::invalid_target, descriptor);
        if (target_kind(intent.target) != descriptor->target)
            return rejected(PlanCode::target_kind_mismatch, descriptor);

        const CommandRequirements& requirements = descriptor->requirements;
        if (requirements.project && !snapshot.project.loaded)
            return rejected(PlanCode::project_required, descriptor);
        if (requirements.document && !snapshot.document.loaded)
            return rejected(PlanCode::document_required, descriptor);
        if (requirements.writable_project && !snapshot.project.writable)
            return rejected(PlanCode::project_read_only, descriptor);
        if (requirements.writable_document && !snapshot.document.writable)
            return rejected(PlanCode::document_read_only, descriptor);
        if (requirements.buildable_project && !snapshot.project.buildable)
            return rejected(PlanCode::project_not_buildable, descriptor);
        if (requirements.runnable_project && !snapshot.project.runnable)
            return rejected(PlanCode::project_not_runnable, descriptor);
        if (requirements.idle_async_lane && snapshot.async.running)
            return rejected(PlanCode::async_lane_busy, descriptor);
        return CommandPlan{PlanCode::ready, descriptor,
            plan_code_reason(PlanCode::ready)};
    }

    enum class ContractFailure : std::uint8_t
    {
        none,
        surface_catalog,
        command_catalog,
        ready_plan,
        stale_plan,
        sensitive_capability
    };

    [[nodiscard]] inline ContractFailure run_aggregate_contract() noexcept
    {
        const CommandDescriptor* world = find_command(CommandId::world_delete_entity);
        const CommandDescriptor* gui = find_command(CommandId::gui_delete_element);
        const CommandDescriptor* ai = find_command(CommandId::ai_request_plan);
        if (!world || world->surface != Surface::world
            || !gui || gui->surface != Surface::gui_canvas
            || !ai || ai->surface != Surface::ai_development)
        {
            return ContractFailure::command_catalog;
        }
        return can_express_sensitive_capability(
                Surface::ai_development,
                SensitiveCapability::approve_candidate)
            || can_express_sensitive_capability(
                Surface::ai_development,
                SensitiveCapability::publish_release)
            || can_express_sensitive_capability(
                Surface::ai_development,
                SensitiveCapability::unrestricted_execution)
            ? ContractFailure::sensitive_capability
            : ContractFailure::none;
    }

    [[nodiscard]] inline ContractFailure run_contract()
    {
        std::array<bool, static_cast<std::size_t>(CommandId::count)> seen{};
        std::size_t command_count = 0u;
        for (std::uint8_t ordinal = 0u;
            ordinal < static_cast<std::uint8_t>(Surface::count);
            ++ordinal)
        {
            const Surface surface = static_cast<Surface>(ordinal);
            const auto commands = commands_for_surface(surface);
            if (!valid_surface(surface) || commands.empty())
                return ContractFailure::surface_catalog;
            for (const CommandDescriptor& descriptor : commands)
            {
                const std::size_t index =
                    static_cast<std::size_t>(descriptor.id);
                if (descriptor.surface != surface || index >= seen.size()
                    || seen[index] || descriptor.stable_name.empty()
                    || descriptor.label.empty())
                {
                    return ContractFailure::command_catalog;
                }
                seen[index] = true;
                ++command_count;
            }
        }
        if (command_count != seen.size())
            return ContractFailure::command_catalog;

        const WorkspaceSnapshot snapshot{
            .id = WorkspaceId{1u},
            .revision = 2u,
            .active_surface = Surface::world,
            .surface = {Surface::world, 3u, true},
            .project = {ProjectId{4u}, 5u, true, true, true, true},
            .document = {
                DocumentId{6u}, ProjectId{4u}, Surface::world,
                7u, 8u, true, true},
            .async = {9u, false}};
        const CommandIntent intent = make_intent(
            RequestId{10u}, CommandId::world_undo,
            DocumentTarget{DocumentId{6u}, 1u}, snapshot);
        if (!plan_command(snapshot, intent))
            return ContractFailure::ready_plan;
        WorkspaceSnapshot stale = snapshot;
        ++stale.surface.revision;
        if (plan_command(stale, intent).code
            != PlanCode::stale_surface_revision)
        {
            return ContractFailure::stale_plan;
        }
        for (const SensitiveCapability capability : {
            SensitiveCapability::approve_candidate,
            SensitiveCapability::promote_to_live_source,
            SensitiveCapability::publish_release,
            SensitiveCapability::git_operation,
            SensitiveCapability::unrestricted_execution})
        {
            if (can_express_sensitive_capability(
                    Surface::ai_development, capability))
            {
                return ContractFailure::sensitive_capability;
            }
        }
        return ContractFailure::none;
    }
}
