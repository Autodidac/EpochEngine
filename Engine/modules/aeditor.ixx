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
        std::string_view id{};
        std::string_view display_name{};
        std::string_view scene_path{};
        std::string_view world_name{};
        std::string_view runtime_scene_id{};
        std::string_view default_script{};
        std::string_view description{};
    };

    export struct EditorScriptProfile
    {
        std::string_view id{};
        std::string_view display_name{};
        std::string_view source_path{};
        std::string_view description{};
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

    export void cleanup_chat_context(const core::Context* ctx);
    export void shutdown_chat_system();
}

