/************************************************
 *  â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—  â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—  â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—â–ˆâ–ˆâ•—  â–ˆâ–ˆâ•—   *
 *  â–ˆâ–ˆâ•”â•â•â•â•â•â–ˆâ–ˆâ•”â•â•â–ˆâ–ˆâ•—â–ˆâ–ˆâ•”â•â•â•â–ˆâ–ˆâ•—â–ˆâ–ˆâ•”â•â•â•â•â•â–ˆâ–ˆâ•‘  â–ˆâ–ˆâ•‘   *
 *  â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—  â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•”â•â–ˆâ–ˆâ•‘   â–ˆâ–ˆâ•‘â–ˆâ–ˆâ•‘     â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•‘   *
 *  â–ˆâ–ˆâ•”â•â•â•  â–ˆâ–ˆâ•”â•â•â•â• â–ˆâ–ˆâ•‘   â–ˆâ–ˆâ•‘â–ˆâ–ˆâ•‘     â–ˆâ–ˆâ•”â•â•â–ˆâ–ˆâ•‘   *
 *  â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—â–ˆâ–ˆâ•‘     â•šâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•”â•â•šâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—â–ˆâ–ˆâ•‘  â–ˆâ–ˆâ•‘   *
 *  â•šâ•â•â•â•â•â•â•â•šâ•â•      â•šâ•â•â•â•â•â•  â•šâ•â•â•â•â•â•â•šâ•â•  â•šâ•â•   *
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
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

export module aeditor;

import core.context;
import aengine.gui;

namespace epochnamespace
{
    export enum class EditorWorkspaceTab : unsigned char
    {
        Output = 0,
        Project,
        Scripts,
        AI,
        Systems
    };

    export enum class EditorProjectKind : unsigned char
    {
        Game = 0,
        Tool
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
        Exit
    };

    export struct EditorFrameResult
    {
        gui::WidgetBounds scene_viewport{};
        EditorCommand command{ EditorCommand::None };
        std::string command_argument{};
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

    export EditorFrameResult editor_run(const std::shared_ptr<core::Context>& ctx);
    export void editor_load_project(const std::shared_ptr<core::Context>& ctx, std::string_view project_id);
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
    export [[nodiscard]] EditorScriptBuildResult editor_build_script(std::string_view script_name);
    export [[nodiscard]] EditorProjectBuildResult editor_build_project(std::string_view project_root);
    [[nodiscard]] EditorScriptBuildResult editor_build_script(std::string_view script_name, std::string_view project_root);
    [[nodiscard]] std::string editor_resolve_script_source_path(std::string_view script_name, std::string_view project_root = {});
    export void editor_set_time_snapshot(const core::Context* ctx, const EditorTimeSnapshot& snapshot);
    export [[nodiscard]] EditorTimeControl editor_time_control(const core::Context* ctx);
    export void editor_consume_time_step_request(const core::Context* ctx);

    export void cleanup_chat_context(const core::Context* ctx);
    export void shutdown_chat_system();
}

