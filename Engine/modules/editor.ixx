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
#include <span>
#include <string>
#include <string_view>
#include <vector>

export module editor;

import context.type;
import core.context;
import engine.gui;
import render.preview_grid;

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
        EngineSelfIteration
    };

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

    export struct EditorProjectProfile
    {
        EditorProjectKind kind{ EditorProjectKind::Game };
        std::string_view id{};
        std::string_view display_name{};
        std::string_view root_path{};
        std::string_view scene_path{};
        std::string_view world_name{};
        std::string_view runtime_scene_id{};
        std::string_view manifest_path{};
        std::string_view template_family{};
        std::string_view default_script{};
        std::string_view description{};
        std::string_view engine_integration_mode{};
        std::string_view public_include_root{};
        std::string_view demo_model_asset{};
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

    export struct EditorScriptBuildResult
    {
        bool succeeded{ false };
        std::string summary{};
    };

    export struct EditorProjectBuildResult
    {
        bool succeeded{ false };
        std::string summary{};
        std::string output_path{};
        std::string log_path{};
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

    export struct EditorSceneSeedEntity
    {
        std::string_view name{};
        std::string_view type{};
        std::string_view category{};
        std::array<float, 3> position{ 0.0f, 0.0f, 0.0f };
        std::array<float, 3> rotation{ 0.0f, 0.0f, 0.0f };
        std::array<float, 3> scale{ 1.0f, 1.0f, 1.0f };
        bool visible{ true };
        bool editor_only{ false };
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
        std::string name{};
        std::string type{};
        std::string category{};
        std::array<float, 3> position{ 0.0f, 0.0f, 0.0f };
        std::array<float, 3> rotation{ 0.0f, 0.0f, 0.0f };
        std::array<float, 3> scale{ 1.0f, 1.0f, 1.0f };
        bool visible{ true };
        bool editor_only{ false };
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
        std::uint8_t input_profile_preset{ 0 };
        gui::ThemePreference theme_preference{ gui::ThemePreference::FollowSystemDark };
        double editor_frame_limit_fps{ 120.0 };
        std::string selected_project_file{};
        std::string selected_asset_path{};
        std::vector<EditorContextSnapshotEntity> entities{};
        std::size_t selected_entity{ 0 };
        std::vector<std::string> log_lines{};
        bool helpers_visible{ true };
        EditorTimeSnapshot time_snapshot{};
        EditorTimeControl time_control{};
        core::ScenePreviewMode preview_mode{ core::ScenePreviewMode::Editor };
        EditorWorkspaceTab workspace_tab{ EditorWorkspaceTab::Output };
        EditorWorkspaceTab dock_status_tab{ EditorWorkspaceTab::Output };
        std::uint8_t main_surface{ 0 };
        float workspace_split{ 0.68f };
        float outliner_split{ 0.20f };
        float inspector_split{ 0.22f };
        float dock_split{ 0.24f };
        bool show_outliner{ true };
        bool show_inspector{ true };
        bool show_console_dock{ true };
        bool show_ai_chat{ true };
        bool project_notes_visible{ false };
        std::uint8_t ai_workspace_domain{ 0 };
        float systems_render_zoom{ 1.15f };
        float systems_task_zoom{ 1.15f };
        int systems_render_pan{ 0 };
        int systems_task_pan{ 0 };
        previewgrid::CameraRigSnapshot camera{};
    };

    export EditorFrameResult editor_run(const std::shared_ptr<core::Context>& ctx);
    export EditorFrameResult editor_run_context_panel(const std::shared_ptr<core::Context>& ctx, std::string_view route_id);
    export void editor_load_project(const std::shared_ptr<core::Context>& ctx, std::string_view project_id);
    export void editor_suppress_startup_update_check(const std::shared_ptr<core::Context>& ctx);
    export void editor_reset_transient_ui(const core::Context* ctx);
    export bool editor_run_script(const core::Context* ctx, std::string_view script_name);
    export [[nodiscard]] std::span<const EditorProjectProfile> editor_project_profiles() noexcept;
    export [[nodiscard]] const EditorProjectProfile& editor_default_project_profile() noexcept;
    export [[nodiscard]] const EditorProjectProfile* editor_find_project_profile(std::string_view project_id) noexcept;
    export [[nodiscard]] std::span<const EditorScriptProfile> editor_script_profiles() noexcept;
    export [[nodiscard]] std::vector<EditorSceneSeedEntity> editor_seed_entities_for_project(std::string_view project_id);
    export [[nodiscard]] std::string_view editor_runtime_scene_for_project(std::string_view project_id) noexcept;
    export [[nodiscard]] std::string_view editor_project_kind_name(EditorProjectKind kind) noexcept;
    export [[nodiscard]] EditorProjectCreationResult editor_create_project_shell(EditorProjectKind kind);
    export [[nodiscard]] EditorProjectCreationResult editor_ensure_project_shell(std::string_view project_id);
    export [[nodiscard]] EditorScriptBuildResult editor_build_script(std::string_view script_name);
    export [[nodiscard]] EditorProjectBuildResult editor_build_project(std::string_view project_root);
    export [[nodiscard]] EditorProjectModelSummary editor_project_model_summary(std::string_view project_id);
    export [[nodiscard]] std::string editor_project_demo_model_path(std::string_view project_id);
    [[nodiscard]] EditorScriptBuildResult editor_build_script(std::string_view script_name, std::string_view project_root);
    [[nodiscard]] std::string editor_resolve_script_source_path(std::string_view script_name, std::string_view project_root = {});
    export void editor_set_time_snapshot(const core::Context* ctx, const EditorTimeSnapshot& snapshot);
    export void editor_set_context_selection_status(const core::Context* ctx, std::string_view status);
    export void editor_mark_context_panel_detached(std::string_view route_id, bool detached);
    export void editor_notify_context_panel_closed(std::string_view route_id);
    export [[nodiscard]] EditorTimeControl editor_time_control(const core::Context* ctx);
    export void editor_consume_time_step_request(const core::Context* ctx);
    export [[nodiscard]] EditorContextSnapshot editor_capture_context_snapshot(const core::Context* ctx);
    export bool editor_restore_context_snapshot(core::Context* ctx, const EditorContextSnapshot& snapshot);

    export void cleanup_chat_context(const core::Context* ctx);
    export void shutdown_chat_system();
}
