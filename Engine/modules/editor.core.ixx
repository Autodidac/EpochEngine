/************************************************
 *  ███████╗██████╗  ██████╗  ██████╗██╗  ██╗   *
 *  ██╔════╝██╔══██╗██╔═══██╗██╔════╝██║  ██║   *
 *  █████╗  ██████╔╝██║   ██║██║     ███████║   *
 *  ██╔══╝  ██╔═══╝ ██║   ██║██║     ██╔══██║   *
 *  ███████╗██║     ╚██████╔╝╚██████╗██║  ██║   *
 *  ╚══════╝╚═╝      ╚═════╝  ╚═════╝╚═╝  ╚═╝   *
 *                                              *
 *   This file is part of the Epoch   Project.  *
 *   epochengine - Modular C++ Framework        *
 *                                              *
 *   SPDX-License-Identifier:                   *
 *   LicenseRef-MIT-NoSell                      *
 *                                              *
 *   Provided "AS IS", without warranty         *
 *   of any kind.                               *
 *                                              *
 *   Use permitted for Non-Commercial           *
 *   Purposes ONLY, without prior               *
 *   commercial licensing agreement.            *
 *                                              *
 *   Redistribution Allowed with This Notice    *
 *   and LICENSE file.                          *
 *                                              *
 *   No obligation to disclose                  *
 *   modifications.                             *
 *                                              *
 *   See LICENSE file for full terms.           *
 *                                              *
 ***********************************************/
module;

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

export module editor.core;

import capability.profile;
import context.type;
import core.context;
import gui.engine;
import render.preview_grid;
import scene.snapshot;

export import editor.application;

namespace epochengine
{
    export enum class EditorWorkspaceTab : unsigned char
    {
        Output = 0,
        Project,
        Scripts,
        Assets,
        AI,
        Systems
    };

    export enum class EditorProjectKind : unsigned char
    {
        Game = 0,
        Tool,
        EngineDevelopment
    };

    export enum class EditorProjectInputProvision : unsigned char
    {
        project_default = 0,
        explicitly_enabled
    };

    export [[nodiscard]] constexpr bool editor_project_input_profile_enabled(
        EditorProjectKind kind,
        EditorProjectInputProvision provision =
            EditorProjectInputProvision::project_default) noexcept
    {
        return kind == EditorProjectKind::Game
            || provision == EditorProjectInputProvision::explicitly_enabled;
    }

    export enum class EditorCommand : unsigned char
    {
        None = 0,
        OpenLauncher,
        OpenProject,
        Settings,
        RunScript,
        RunGame,
        UpdateApplication,
        UpdateApplicationFromSource,
        SwitchContext,
        OpenContextWindow,
        Exit
    };

    export struct EditorFrameResult
    {
        gui::WidgetBounds scene_viewport{};
        bool scene_input_captured{ false };
        EditorCommand command{ EditorCommand::None };
        std::string command_argument{};
        core::ContextType requested_context_type{ core::ContextType::None };
        bool close_current_context_after_command{ false };
    };

    export [[nodiscard]] constexpr bool editor_context_selection_allowed(
        bool target_selectable,
        bool target_already_active,
        bool replacement_in_progress) noexcept
    {
        return target_selectable
            && !target_already_active
            && !replacement_in_progress;
    }

    export struct EditorProjectCapabilityPolicy
    {
        std::string_view id{ "portable" };
        capability::Requirement renderer_requirement{
            capability::portable_rendering_requirement() };
        bool allow_experimental{ true };
        bool allow_software_fallback{ true };
    };

    export [[nodiscard]] constexpr EditorProjectCapabilityPolicy
        editor_portable_capability_policy() noexcept
    {
        return EditorProjectCapabilityPolicy{};
    }

    export [[nodiscard]] constexpr std::optional<EditorProjectCapabilityPolicy>
        editor_project_capability_policy(std::string_view id) noexcept
    {
        if (id == "headless")
        {
            return EditorProjectCapabilityPolicy{
                .id = "headless",
                .renderer_requirement = capability::headless_rendering_requirement(),
                .allow_experimental = false,
                .allow_software_fallback = true
            };
        }
        if (id == "portable")
            return editor_portable_capability_policy();
        if (id == "portable-strict")
        {
            return EditorProjectCapabilityPolicy{
                .id = "portable-strict",
                .renderer_requirement = capability::portable_rendering_requirement(),
                .allow_experimental = false,
                .allow_software_fallback = false
            };
        }
        if (id == "explicit")
        {
            return EditorProjectCapabilityPolicy{
                .id = "explicit",
                .renderer_requirement = capability::explicit_rendering_requirement(),
                .allow_experimental = true,
                .allow_software_fallback = false
            };
        }
        if (id == "explicit-strict")
        {
            return EditorProjectCapabilityPolicy{
                .id = "explicit-strict",
                .renderer_requirement = capability::explicit_rendering_requirement(),
                .allow_experimental = false,
                .allow_software_fallback = false
            };
        }
        return std::nullopt;
    }

    export struct EditorProjectProfile
    {
        EditorProjectKind kind{ EditorProjectKind::Game };
        std::string_view id{};
        std::string_view display_name{};
        std::string_view root_path{};
        std::string_view scene_path{};
        std::string_view tilemap_path{};
        std::string_view input_profile_path{};
        std::string_view sprite_animation_path{};
        std::string_view audio_profile_path{};
        std::string_view gui_path{};
        std::string_view world_name{};
        std::string_view runtime_scene_id{};
        std::string_view manifest_path{};
        std::string_view template_family{};
        std::string_view default_script{};
        std::string_view description{};
        std::string_view engine_integration_mode{};
        std::string_view public_include_root{};
        std::string_view demo_model_asset{};
        EditorProjectCapabilityPolicy renderer_capability{};
    };

    export enum class EditorProjectInputBindingKind : std::uint8_t
    {
        keyboard,
        controller_button,
        controller_axis
    };

    export struct EditorProjectInputBindingSummary
    {
        std::uint64_t stable_id{};
        std::string action{};
        std::string device{};
        std::string source{};
        std::uint16_t code{};
        std::uint16_t dead_zone_q15{};
        std::uint8_t controller_slot{};
        EditorProjectInputBindingKind kind{
            EditorProjectInputBindingKind::keyboard};
    };

    export struct EditorProjectInputProfileSummary
    {
        bool ready{};
        bool uniform_controller_dead_zone{true};
        std::string display_name{};
        std::string source_path{};
        std::string diagnostic{};
        std::uint32_t action_count{};
        std::uint32_t binding_count{};
        std::uint16_t controller_dead_zone_q15{};
        std::vector<EditorProjectInputBindingSummary> bindings{};
    };

    export struct EditorProjectInputUpdateResult
    {
        bool succeeded{};
        std::string summary{};
    };
    export enum class EditorProjectAudioCueSemantic : std::uint8_t
    {
        custom,
        jump,
        land,
        music,
        ambient,
        user_interface
    };

    export struct EditorProjectAudioBusSummary
    {
        std::uint64_t stable_id{};
        std::uint64_t parent_id{};
        std::string name{};
        std::string parent_name{};
        float gain{1.0f};
        bool muted{};
    };

    export struct EditorProjectAudioCueSummary
    {
        std::uint64_t stable_id{};
        EditorProjectAudioCueSemantic semantic{
            EditorProjectAudioCueSemantic::custom};
        std::string semantic_name{};
        std::string name{};
        std::string logical_path{};
        std::uint64_t bus_id{};
        std::string bus_name{};
        float gain{1.0f};
        bool looping{};
        bool autoplay{};
        bool source_exists{};
        std::uint64_t source_bytes{};
    };

    export struct EditorProjectAudioProfileSummary
    {
        bool ready{};
        bool source_materialized{};
        bool artifact_materialized{};
        bool artifact_current{};
        std::string source_path{};
        std::string artifact_path{};
        std::string diagnostic{};
        std::uint64_t sequence{};
        std::uint32_t bus_count{};
        std::uint32_t cue_count{};
        std::uint32_t missing_source_count{};
        std::uint64_t source_bytes{};
        std::vector<EditorProjectAudioBusSummary> buses{};
        std::vector<EditorProjectAudioCueSummary> cues{};
    };

    export struct EditorProjectAudioUpdateResult
    {
        bool succeeded{};
        bool changed{};
        std::uint64_t stable_id{};
        std::string logical_path{};
        std::uint64_t decoded_bytes{};
        std::uint64_t frame_count{};
        std::uint32_t sample_rate{};
        std::uint16_t channel_count{};
        std::string summary{};
    };
    export struct EditorScriptProfile
    {
        std::string_view id{};
        std::string_view display_name{};
        std::string_view source_path{};
        std::string_view build_action{};
        std::string_view run_action{};
        std::string_view diagnostic_hint{};
        std::string_view description{};
    };

    export struct EditorProjectCreationResult
    {
        bool succeeded{ false };
        std::string project_id{};
        std::string root_path{};
        std::string manifest_path{};
        std::string entry_source_path{};
        std::string build_script_path{};
        std::string default_script_path{};
        std::string summary{};
        std::string engine_integration_mode{};
        std::string public_include_root{};
    };

    export struct EditorProjectAdmissionResult
    {
        bool succeeded{ false };
        std::string project_id{};
        std::string manifest_path{};
        std::string summary{};
    };

    export struct EditorScriptBuildResult
    {
        bool succeeded{ false };
        std::string summary{};
        bool cancelled{ false };
    };

    export struct EditorProjectBuildResult
    {
        bool succeeded{ false };
        std::string summary{};
        std::string output_path{};
        std::string log_path{};
        bool cancelled{ false };
    };

    export struct EditorProjectModelSummary
    {
        bool declared{ false };
        bool exists{ false };
        bool parsed{ false };
        std::uint32_t scene_count{ 0 };
        std::uint32_t node_count{ 0 };
        std::uint32_t mesh_count{ 0 };
        std::uint32_t primitive_count{ 0 };
        std::uint32_t material_count{ 0 };
        std::string asset_path{};
        std::string resolved_path{};
        std::string summary{};
    };

    export struct EditorTimeSnapshot
    {
        std::uint64_t frame_index = 0;
        std::uint64_t simulated_steps = 0;
        std::uint32_t step_budget = 0;
        std::uint32_t max_steps_per_frame = 8;
        double real_dt_seconds = 0.0;
        double scaled_dt_seconds = 0.0;
        double fixed_dt_seconds = 1.0 / 60.0;
        double accumulator_seconds = 0.0;
        double simulated_seconds = 0.0;
        double time_scale = 1.0;
        bool paused = false;
    };

    export struct EditorTimeControl
    {
        bool paused = false;
        bool step_once = false;
        std::uint32_t max_steps_per_frame = 8;
        double fixed_dt_seconds = 1.0 / 60.0;
        double time_scale = 1.0;
    };

    export struct EditorContextSnapshotEntity
    {
        std::uint64_t scene_object_id{ 0u };
        std::string name{};
        std::string type{};
        std::string category{};
        std::array<float, 3> position{ 0.0f, 0.0f, 0.0f };
        std::array<float, 3> rotation{ 0.0f, 0.0f, 0.0f };
        std::array<float, 3> scale{ 1.0f, 1.0f, 1.0f };
        bool visible{ true };
        bool editor_only{ false };
        std::optional<scene::SceneTextureMaterialSnapshot>
            texture_material{};
    };

    export struct EditorCanvas2DProjectSnapshot
    {
        std::uint32_t logical_width{ 1280 };
        std::uint32_t logical_height{ 720 };
        float pixels_per_world_unit{ 100.0f };
        std::uint8_t scale_policy{ 0 };
        std::uint8_t sampling_policy{ 0 };
        std::uint32_t maximum_sprites_per_batch{ 2048 };
        std::uint32_t maximum_batches{ 256 };
        std::uint32_t tile_chunk_extent{ 32 };
        bool pixel_snapping{ true };
    };

    export struct EditorToolPaneSnapshot
    {
        std::string route{};
        bool open{true};
        std::uint8_t dock_region{};
    };

    export struct EditorContextSnapshot
    {
        bool valid{ false };
        std::string project_id{};
        std::string project_name{};
        std::string project_root{};
        std::string project_scene_path{};
        std::string project_manifest{};
        std::string project_template{};
        std::string project_kind{};
        std::string active_script{};
        std::string active_runtime_scene{};
        std::string active_world{};
        std::string project_status{};
        std::string project_build_status{};
        std::string script_build_status{};
        std::string script_editor_path{};
        std::string script_editor_text{};
        std::string script_editor_status{};
        bool script_editor_dirty{ false };
        std::string project_run_backend{ "opengl" };
        double project_run_frame_limit_fps{ 60.0 };
        std::uint8_t project_camera_mode{ 0 };
        EditorCanvas2DProjectSnapshot canvas2d_project{};
        std::vector<std::byte> tile_map_document{};
        bool tile_map_dirty{};
        std::uint32_t tile_map_tile_extent{32u};
        std::uint8_t tile_map_tool{1u};
        std::uint32_t tile_map_selected_palette{};
        std::uint32_t tile_map_selected_layer{};
        float tile_map_zoom{1.0f};
        float tile_map_pan_x{};
        float tile_map_pan_y{};
        std::uint8_t input_profile_preset{ 0 };
        gui::ThemePreference theme_preference{ gui::ThemePreference::FollowSystemDark };
        bool rounded_rectangles{ true };
        double editor_frame_limit_fps{ 120.0 };
        std::string selected_project_file{};
        std::string selected_asset_path{};
        std::vector<EditorContextSnapshotEntity> entities{};
        std::size_t selected_entity{ 0 };
        std::uint64_t selected_entity_id{ 0u };
        scene::SceneSnapshot scene_document{};
        std::vector<std::string> log_lines{};
        bool helpers_visible{ true };
        EditorTimeSnapshot time_snapshot{};
        EditorTimeControl time_control{};
        core::ScenePreviewMode preview_mode{ core::ScenePreviewMode::Editor };
        EditorWorkspaceTab workspace_tab{ EditorWorkspaceTab::Output };
        EditorWorkspaceTab dock_status_tab{ EditorWorkspaceTab::Output };
        std::uint8_t main_surface{ 0 };
        float bottom_grid_split{ 0.68f };
        float outliner_split{ 0.20f };
        float inspector_split{ 0.22f };
        float dock_split{ 0.24f };
        std::array<float, 9> workspace_bottom_grid_splits{
            0.68f, 0.68f, 0.68f, 0.68f, 0.68f,
            0.68f, 0.68f, 0.68f, 0.68f
        };
        std::array<float, 9> workspace_dock_splits{
            0.24f, 0.24f, 0.24f, 0.24f, 0.24f,
            0.24f, 0.24f, 0.24f, 0.24f
        };
        std::array<bool, 9> workspace_output_follow{
            true, true, true, true, true,
            true, true, true, true
        };
        std::array<bool, 9> workspace_ai_chat_follow{
            true, true, true, true, true,
            true, true, true, true
        };
        bool show_outliner{ true };
        bool show_inspector{ true };
        bool show_console_dock{ true };
        bool show_ai_chat{ true };
        std::vector<EditorToolPaneSnapshot> tool_panes{};
        std::string active_left_pane_route{ "pane.world_outliner" };
        std::string active_right_pane_route{ "pane.properties" };
        std::string active_bottom_left_pane_route{ "pane.output" };
        std::string active_bottom_right_pane_route{ "pane.ai_chat" };
        bool project_notes_visible{ false };
        std::uint8_t ai_workspace_domain{ 0 };
        EditorApplicationKind application_kind{ EditorApplicationKind::Standard };
        float systems_render_zoom{ 1.15f };
        float systems_task_zoom{ 1.15f };
        int systems_render_pan{ 0 };
        int systems_task_pan{ 0 };
        previewgrid::CameraRigSnapshot camera{};
    };

    export EditorFrameResult editor_run(const std::shared_ptr<core::Context>& ctx);
    export EditorFrameResult editor_run_context_panel(const std::shared_ptr<core::Context>& ctx, std::string_view route_id);
    export [[nodiscard]] bool editor_open_project(
        const std::shared_ptr<core::Context>& ctx,
        std::string_view project_id);
    export [[nodiscard]] bool editor_close_project(
        const std::shared_ptr<core::Context>& ctx);
    export void editor_load_project(const std::shared_ptr<core::Context>& ctx, std::string_view project_id);
    export void editor_load_application(
        const std::shared_ptr<core::Context>& ctx,
        EditorApplicationKind application);
    export void editor_suppress_startup_update_check(const std::shared_ptr<core::Context>& ctx);
    export void editor_reset_transient_ui(const core::Context* ctx);
    export bool editor_run_script(const core::Context* ctx, std::string_view script_name);
    export [[nodiscard]] EditorProjectAdmissionResult
        editor_admit_project_manifest(std::string_view manifest_path);
    export [[nodiscard]] std::span<const EditorProjectProfile> editor_project_profiles() noexcept;
    export [[nodiscard]] const EditorProjectProfile& editor_default_project_profile() noexcept;
    export [[nodiscard]] const EditorProjectProfile* editor_find_project_profile(std::string_view project_id) noexcept;
    export [[nodiscard]] std::span<const EditorScriptProfile> editor_script_profiles() noexcept;
    export [[nodiscard]] std::vector<EditorSceneSeedEntity> editor_seed_entities_for_project(std::string_view project_id);
    export [[nodiscard]] std::string_view editor_runtime_scene_for_project(std::string_view project_id) noexcept;
    export [[nodiscard]] std::string_view editor_project_kind_name(EditorProjectKind kind) noexcept;
    export [[nodiscard]] EditorProjectCreationResult editor_create_project_shell(EditorProjectKind kind);
    export [[nodiscard]] EditorProjectCreationResult editor_create_project_shell(
        EditorProjectKind kind,
        EditorProjectInputProvision input_provision);
    export [[nodiscard]] bool editor_project_manifest_capability_contract() noexcept;
    export [[nodiscard]] EditorProjectCreationResult editor_ensure_project_shell(std::string_view project_id);
    export [[nodiscard]] EditorScriptBuildResult editor_build_script(std::string_view script_name);
    export [[nodiscard]] EditorProjectBuildResult editor_build_project(std::string_view project_root);
    [[nodiscard]] EditorProjectBuildResult editor_build_project(
        std::string_view project_root,
        std::stop_token cancellation);
    export [[nodiscard]] EditorProjectModelSummary editor_project_model_summary(std::string_view project_id);
    export [[nodiscard]] EditorProjectInputProfileSummary
        editor_project_input_profile_summary(std::string_view project_id);
    export [[nodiscard]] EditorProjectInputUpdateResult
        editor_reset_project_input_profile(std::string_view project_id);
    export [[nodiscard]] EditorProjectInputUpdateResult
        editor_rebind_project_input(
            std::string_view project_id,
            std::uint64_t binding_id,
            std::uint16_t key_code);
    export [[nodiscard]] EditorProjectInputUpdateResult
        editor_rebind_project_controller_button(
            std::string_view project_id,
            std::uint64_t binding_id,
            std::uint16_t button_code,
            std::uint8_t controller_slot);
    export [[nodiscard]] EditorProjectInputUpdateResult
        editor_rebind_project_controller_axis(
            std::string_view project_id,
            std::uint64_t binding_id,
            std::uint16_t axis_code,
            std::uint8_t controller_slot);
    export [[nodiscard]] EditorProjectInputUpdateResult
        editor_set_project_controller_dead_zone(
            std::string_view project_id,
            std::uint16_t dead_zone_q15);
    export [[nodiscard]] EditorProjectAudioProfileSummary
        editor_project_audio_profile_summary(std::string_view project_id);
    export [[nodiscard]] EditorProjectAudioUpdateResult
        editor_reset_project_audio_profile(std::string_view project_id);
    export [[nodiscard]] EditorProjectAudioUpdateResult
        editor_import_project_audio_cue(
            std::string_view project_id,
            std::string_view external_source_path);
    export [[nodiscard]] EditorProjectAudioUpdateResult
        editor_set_project_audio_bus_mix(
            std::string_view project_id,
            std::uint64_t bus_id,
            float gain,
            bool muted);
    export [[nodiscard]] EditorProjectAudioUpdateResult
        editor_configure_project_audio_cue(
            std::string_view project_id,
            std::uint64_t cue_id,
            EditorProjectAudioCueSemantic semantic,
            std::uint64_t bus_id,
            float gain,
            bool looping,
            bool autoplay);
    export [[nodiscard]] EditorProjectAudioUpdateResult
        editor_remove_project_audio_cue(
            std::string_view project_id,
            std::uint64_t cue_id);
    export [[nodiscard]] EditorProjectAudioUpdateResult
        editor_validate_project_audio_profile(std::string_view project_id);
    export [[nodiscard]] std::string editor_project_demo_model_path(
        std::string_view project_id);
    [[nodiscard]] EditorScriptBuildResult editor_build_script(std::string_view script_name, std::string_view project_root);
    [[nodiscard]] EditorScriptBuildResult editor_build_script(
        std::string_view script_name,
        std::string_view project_root,
        std::stop_token cancellation);
    [[nodiscard]] std::string editor_resolve_script_source_path(std::string_view script_name, std::string_view project_root = {});
    export void editor_set_time_snapshot(const core::Context* ctx, const EditorTimeSnapshot& snapshot);
    export void editor_set_context_selection_status(const core::Context* ctx, std::string_view status);
    export void editor_mark_context_panel_detached(std::string_view route_id, bool detached);
    export void editor_notify_context_panel_closed(std::string_view route_id);
    export void editor_redock_context_panel(
        std::string_view route_id,
        std::uint8_t dock_target);
    export [[nodiscard]] EditorTimeControl editor_time_control(const core::Context* ctx);
    export void editor_consume_time_step_request(const core::Context* ctx);
    export [[nodiscard]] EditorContextSnapshot editor_capture_context_snapshot(const core::Context* ctx);
    export bool editor_restore_context_snapshot(core::Context* ctx, const EditorContextSnapshot& snapshot);

    export void cleanup_chat_context(const core::Context* ctx);
    export void shutdown_chat_system();
}
