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
// Epoch system notes (auto-checked):
// Epoch Editor: keep editor state and source changes within their owning workflows.
// Engine/src/editor/editor.scene.cpp
module;
#include "epoch.build_layout.hpp"
#include "updater.distribution.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <functional>
#include <fstream>
#include <initializer_list>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <cstring>
#include <span>
#include <stop_token>
#include <system_error>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#if defined(_MSC_VER)
#  ifndef _CRT_SECURE_NO_WARNINGS
#    define _CRT_SECURE_NO_WARNINGS
#  endif
#  pragma warning(push)
#  pragma warning(disable: 4996)
#endif

#ifndef EPOCH_HAS_CGLTF
#  define EPOCH_HAS_CGLTF 0
#endif

#if EPOCH_HAS_CGLTF
#  define cgltf_parse epoch_cgltf_parse
#  define cgltf_parse_file epoch_cgltf_parse_file
#  define cgltf_load_buffers epoch_cgltf_load_buffers
#  define cgltf_load_buffer_base64 epoch_cgltf_load_buffer_base64
#  define cgltf_decode_string epoch_cgltf_decode_string
#  define cgltf_decode_uri epoch_cgltf_decode_uri
#  define cgltf_validate epoch_cgltf_validate
#  define cgltf_free epoch_cgltf_free
#  define cgltf_node_transform_local epoch_cgltf_node_transform_local
#  define cgltf_node_transform_world epoch_cgltf_node_transform_world
#  define cgltf_buffer_view_data epoch_cgltf_buffer_view_data
#  define cgltf_find_accessor epoch_cgltf_find_accessor
#  define cgltf_accessor_read_float epoch_cgltf_accessor_read_float
#  define cgltf_accessor_read_uint epoch_cgltf_accessor_read_uint
#  define cgltf_accessor_read_index epoch_cgltf_accessor_read_index
#  define cgltf_num_components epoch_cgltf_num_components
#  define cgltf_component_size epoch_cgltf_component_size
#  define cgltf_calc_size epoch_cgltf_calc_size
#  define cgltf_accessor_unpack_floats epoch_cgltf_accessor_unpack_floats
#  define cgltf_accessor_unpack_indices epoch_cgltf_accessor_unpack_indices
#  define cgltf_copy_extras_json epoch_cgltf_copy_extras_json
#  define cgltf_mesh_index epoch_cgltf_mesh_index
#  define cgltf_material_index epoch_cgltf_material_index
#  define cgltf_accessor_index epoch_cgltf_accessor_index
#  define cgltf_buffer_view_index epoch_cgltf_buffer_view_index
#  define cgltf_buffer_index epoch_cgltf_buffer_index
#  define cgltf_image_index epoch_cgltf_image_index
#  define cgltf_texture_index epoch_cgltf_texture_index
#  define cgltf_sampler_index epoch_cgltf_sampler_index
#  define cgltf_skin_index epoch_cgltf_skin_index
#  define cgltf_camera_index epoch_cgltf_camera_index
#  define cgltf_light_index epoch_cgltf_light_index
#  define cgltf_node_index epoch_cgltf_node_index
#  define cgltf_scene_index epoch_cgltf_scene_index
#  define cgltf_animation_index epoch_cgltf_animation_index
#  define cgltf_animation_sampler_index epoch_cgltf_animation_sampler_index
#  define cgltf_animation_channel_index epoch_cgltf_animation_channel_index
#  define cgltf_parse_json epoch_cgltf_parse_json
#  define CGLTF_IMPLEMENTATION
#  include <cgltf.h>
#  undef CGLTF_IMPLEMENTATION
#endif

#if defined(_MSC_VER)
#  pragma warning(pop)
#endif

module editor.core;

import core.logger;
import core.path;
import ai.project_profile;
import authoring.tilemap;
import authoring.texture;
import authoring.gui_document;
import authoring.gui_compiler;
import capability.profile;
import editor.project_textures;
import editor.tilemap_workspace;
import epoch.gui.tile_workspace;
import package.registry;
import platform.filesystem;
import platform.child_process;
import render.arcade;
import render.device;
import scene.serializer;
import scene.snapshot;
import project.input_profile;
import project.texture_admission;
import project.sprite_animation;
import project.audio_profile;
import project.gui_library;
import project.gui_runtime;
import scripting.compiler;

namespace
{
    inline void ALOG(std::string_view s)
    {
        epochengine::logger::info("Editor.Scene", std::string(s));
    }

    inline void AERR(std::string_view s)
    {
        epochengine::logger::error("Editor.Scene", std::string(s));
    }
}

#include "implementation/editor.scene_commands.inl"

namespace
{
    namespace fs = std::filesystem;

    using epochengine::EditorProjectProfile;
    using epochengine::EditorProjectCreationResult;
    using epochengine::EditorProjectKind;
    using epochengine::EditorSceneSeedEntity;
    using epochengine::EditorScriptBuildResult;
    using epochengine::EditorScriptProfile;

    constexpr std::string_view kProjectManifestFormat{"epoch-project-v1"};
    constexpr std::string_view kStaticRuntimeBuildProfile{"epoch-runtime-static"};

    constexpr EditorProjectProfile kEngineDevelopmentCompatibilityProfile{
        .kind = EditorProjectKind::EngineDevelopment,
        .id = "sandbox",
        .display_name = "EpochEngine",
        .root_path = "Projects/Sandbox",
        .scene_path = "Projects/Sandbox/scene.epoch",
        .world_name = "PersistentLevel",
        .runtime_scene_id = "project:sandbox",
        .manifest_path = "Projects/Sandbox/project.epoch.json",
        .template_family = "engine-development-sandbox",
        .default_script = "engine_development_harness",
        .description = "Legacy compatibility profile. Engine self-iteration is owned by the dedicated AI Engine Dev workspace.",
        .engine_integration_mode = "legacy compatibility only; guarded engine work uses a disposable AI workspace",
        .public_include_root = "Engine/include",
        .demo_model_asset = "",
        .renderer_capability = epochengine::editor_portable_capability_policy()
    };

    constexpr std::array<EditorProjectProfile, 4> kProjectProfiles{{
        {
            .kind = EditorProjectKind::Game,
            .id = "platformer",
            .display_name = "PlatformerDemo",
            .root_path = "Projects/PlatformerDemo",
            .scene_path = "Projects/PlatformerDemo/worlds/platformer.epoch",
            .input_profile_path = epochengine::project_input::canonical_source_path,
            .world_name = "Platformer_Main",
            .runtime_scene_id = "project:platformer",
            .manifest_path = "Projects/PlatformerDemo/project.epoch.json",
            .template_family = "game-project",
            .default_script = "project_demo_bootstrap",
            .description = "Gameplay test profile for movement, camera tuning, and encounter scripting.",
            .engine_integration_mode = "repo-local static Epoch runtime child build",
            .public_include_root = "Engine/include",
            .demo_model_asset = "",
            .renderer_capability = epochengine::editor_portable_capability_policy()
        },
        {
            .kind = EditorProjectKind::Game,
            .id = "twodstudio",
            .display_name = "GUI Editor",
            .root_path = "Projects/TwoDStudio",
            .scene_path = "Projects/TwoDStudio/worlds/twod.epoch",
            .tilemap_path = "Assets/Maps/main.epochmap",
            .input_profile_path = "Assets/Config/input_profile.epochinput",
            .sprite_animation_path = "Assets/Animations/sprite_animations.epochanim",
            .audio_profile_path = "Assets/Audio/project_audio.epochaudio",
            .gui_path = epochengine::project_gui::canonical_source_path,
            .world_name = "TwoD_Main",
            .runtime_scene_id = "project:twodstudio",
            .manifest_path = "Projects/TwoDStudio/project.epoch.json",
            .template_family = "game-2d-project",
            .default_script = "project_demo_bootstrap",
            .description = "GUI-authoring and 2D game workspace for interfaces, side-scrollers, top-down prototypes, and the playable-2D priority track.",
            .engine_integration_mode = "repo-local static Epoch runtime child build",
            .public_include_root = "Engine/include",
            .demo_model_asset = "",
            .renderer_capability = epochengine::editor_portable_capability_policy()
        },
        {
            .kind = EditorProjectKind::Tool,
            .id = "projectlauncher",
            .display_name = "Project Hub",
            .root_path = "Projects/ProjectLauncher",
            .scene_path = "Projects/ProjectLauncher/worlds/launcher.epoch",
            .world_name = "LauncherWorkspace",
            .runtime_scene_id = "project:projectlauncher",
            .manifest_path = "Projects/ProjectLauncher/project.epoch.json",
            .template_family = "tool-project",
            .default_script = "editor_launcher",
            .description = "Editor-facing project hub profile for project selection, context setup, settings, and future engine automation. Compatibility id/path remain projectlauncher/Projects/ProjectLauncher until the generated-project migration is safe.",
            .engine_integration_mode = "repo-local static Epoch runtime child build",
            .public_include_root = "Engine/include",
            .demo_model_asset = "Engine/assets/demo/minisponza/mini_sponza_v2.gltf",
            .renderer_capability = epochengine::editor_portable_capability_policy()
        },
        {
            .kind = EditorProjectKind::Tool,
            .id = "softwarestudio",
            .display_name = "SoftwareStudio",
            .root_path = "Projects/SoftwareStudio",
            .scene_path = "Projects/SoftwareStudio/worlds/tool.epoch",
            .world_name = "ToolWorkspace",
            .runtime_scene_id = "project:softwarestudio",
            .manifest_path = "Projects/SoftwareStudio/project.epoch.json",
            .template_family = "tool-project",
            .default_script = "tool_bootstrap",
            .description = "Software and tool development profile for workflow automation, dashboards, and editor-facing utilities.",
            .engine_integration_mode = "repo-local static Epoch runtime child build",
            .public_include_root = "Engine/include",
            .demo_model_asset = "",
            .renderer_capability = epochengine::editor_portable_capability_policy()
        }
    }};

    constexpr std::array<EditorScriptProfile, 7> kScriptProfiles{{
        {
            "rotate_all_entities",
            "Rotate All Entities",
            "Engine/src/scripts/script.rotate_all_entities.cpp",
            "Validate source path and script host bindings",
            "Rotate current editor scene entities",
            "Checks for a present script source file before using the active engine host to reload it.",
            "Simple validation script for host callbacks against the current editor scene."
        },
        {
            "oscillate_selected_entity",
            "Oscillate Selected Entity",
            "Engine/src/scripts/script.oscillate_selected_entity.cpp",
            "Select a movable scene object before running the oscillator example",
            "Attach a deterministic X-axis oscillator to the selected object",
            "Runs once to attach an engine-owned preview behavior; press Play to advance simulation time.",
            "Renderer benchmark example: select a Cube, run this script, then enable A/B Render Benchmark."
        },
        {
            "project_demo_bootstrap",
            "Project Demo Bootstrap",
            "Engine/src/scripts/script.project_demo_bootstrap.cpp",
            "Validate source path, optional model wiring, and script host bindings",
            "Prime the active project with the default bootstrap flow",
            "Confirms the bootstrap script exists and can drive optional project-declared model loads through the engine-owned script host.",
            "Default bootstrap script for general project shells."
        },
        {
            "engine_arcade_scene",
            "Engine Arcade Scene",
            "Engine/src/scripts/script.engine_arcade_scene.cpp",
            "Validate built-in engine arcade scene scripting hook",
            "Select an engine-owned mini-runtime for the centered Run button",
            "Uses the script host to select built-in engine scene IDs without moving the game implementations out of the kernel engine.",
            "Scene adapter script for render-to-texture arcade cabinets and other in-engine asset flows."
        },
        {
            "editor_launcher",
            "Editor Launcher (Legacy Alias)",
            "Engine/src/scripts/script.editor_launcher.cpp",
            "Validate source path, demo asset wiring, and launcher bindings",
            "Run the launcher-oriented bootstrap flow with the familiar legacy script ID",
            "Launcher-facing script kept as a first-class source file while sharing the model-backed bootstrap flow.",
            "Launcher bootstrap script with the ProjectLauncher demo model wiring."
        },
        {
            "game_bootstrap",
            "Game Bootstrap",
            "Engine/src/scripts/script.project_demo_bootstrap.cpp",
            "Validate game bootstrap source and demo asset wiring",
            "Prime a generated game project scene/runtime shell",
            "Compatibility alias retained for older generated manifests while the default game script moves to project_demo_bootstrap.",
            "Starter script alias for generated game projects."
        },
        {
            "tool_bootstrap",
            "Tool Bootstrap",
            "Projects/Templates/ToolProject/scripts/tool_bootstrap.ascript.cpp",
            "Validate project tool bootstrap source",
            "Prime a generated software/tool project shell",
            "Expected in generated tool projects; create a new project shell if missing.",
            "Starter script surface for generated software and utility projects."
        }
    }};

#include "implementation/editor.project_registry.inl"
#include "implementation/editor.project_shell_generation.inl"
namespace epochengine
{
    std::filesystem::path editor_resolve_candidate_project_path(
        const std::filesystem::path& requested,
        bool allow_root)
    {
        const fs::path data = core::path::candidate_data_root();
        return data.empty() ? fs::path{}
            : candidate_project_path(data, requested, allow_root);
    }

    EditorProjectAdmissionResult editor_admit_project_manifest(
        std::string_view manifest_path)
    {
        EditorProjectAdmissionResult result{};
        if (manifest_path.empty())
        {
            result.summary = "Open Project requires a project.epoch.json file.";
            return result;
        }

        const fs::path data = epochengine::core::path::candidate_data_root();
        const fs::path candidate_manifest = data.empty() ? fs::path{}
            : candidate_project_path(data, fs::path{manifest_path});
        if (!data.empty() && candidate_manifest.empty())
        {
            result.summary = "Candidate previews can open projects only inside their private Projects directory.";
            return result;
        }

        std::error_code error{};
        fs::path selected = data.empty() ? fs::absolute(fs::path{manifest_path}, error)
            : candidate_manifest;
        if (error)
        {
            result.summary = "The selected project path could not be resolved.";
            return result;
        }
        selected = fs::weakly_canonical(selected, error);
        if (error)
        {
            result.summary = "The selected project path could not be canonicalized.";
            return result;
        }
        result.manifest_path = selected.generic_string();
        const auto status = fs::symlink_status(selected, error);
        if (error
            || !fs::is_regular_file(status)
            || fs::is_symlink(status)
            || selected.filename() != "project.epoch.json")
        {
            result.summary =
                "Choose a regular project.epoch.json manifest, not a directory, alias, or generated binary.";
            return result;
        }

        std::string parseDiagnostic{};
        bool requiresMigration{};
        auto parsed = parse_manifest_project_profile(
            selected, &parseDiagnostic, &requiresMigration);
        if (!parsed)
        {
            result.summary = parseDiagnostic.empty()
                ? "The selected project manifest is malformed or uses an unsupported project contract."
                : std::move(parseDiagnostic);
            return result;
        }
        const std::string admissionSummary = requiresMigration
            ? "Legacy project manifest admitted. Save Project, Build, or Run will atomically add the current project/build format fields."
            : "Project manifest admitted.";
        if (is_launcher_application_id(parsed->id))
        {
            result.summary =
                "This manifest names a launcher-owned editor application, not an Epoch project.";
            return result;
        }

        const auto canonical_manifest = [](const EditorProjectProfile& profile)
            -> fs::path
        {
            std::error_code ignored{};
            fs::path path = profile.manifest_path.empty()
                ? resolve_project_root_path(fs::path{profile.root_path})
                    / "project.epoch.json"
                : fs::path{profile.manifest_path};
            path = fs::absolute(path, ignored);
            if (ignored)
                return path.lexically_normal();
            const fs::path canonical = fs::weakly_canonical(path, ignored);
            return ignored ? path.lexically_normal() : canonical;
        };

        for (const auto& profile : live_project_profiles())
        {
            if (profile.id != parsed->id)
                continue;
            if (canonical_manifest(profile) != selected)
            {
                result.summary =
                    "A different project manifest already owns project id '"
                    + parsed->id
                    + "'. Change the manifest id instead of overwriting that project.";
                return result;
            }
            result.succeeded = true;
            result.project_id = parsed->id;
            result.summary = admissionSummary;
            return result;
        }

        auto& admitted = admitted_project_profiles();
        admitted.erase(
            std::remove_if(
                admitted.begin(),
                admitted.end(),
                [&](const OwnedProjectProfile& existing)
                {
                    return existing.manifest_path
                        == parsed->manifest_path;
                }),
            admitted.end());
        result.project_id = parsed->id;
        admitted.push_back(std::move(*parsed));
        invalidate_project_profile_cache();
        result.succeeded = true;
        result.summary = admissionSummary;
        return result;
    }

    std::span<const EditorProjectProfile> editor_project_profiles() noexcept
    {
        try
        {
            const auto& profiles = live_project_profiles();
            return { profiles.data(), profiles.size() };
        }
        catch (...)
        {
            return { kProjectProfiles.data(), kProjectProfiles.size() };
        }
    }

    const EditorProjectProfile& editor_default_project_profile() noexcept
    {
        return kProjectProfiles.front();
    }

    const EditorProjectProfile* editor_find_project_profile(std::string_view project_id) noexcept
    {
        if (project_id == kEngineDevelopmentCompatibilityProfile.id)
            return &kEngineDevelopmentCompatibilityProfile;
        try
        {
            for (const auto& profile : live_project_profiles())
                if (profile.id == project_id)
                    return &profile;
        }
        catch (...)
        {
        }

        for (const auto& profile : kProjectProfiles)
            if (profile.id == project_id)
                return &profile;
        return nullptr;
    }

    std::span<const EditorScriptProfile> editor_script_profiles() noexcept
    {
        return { kScriptProfiles.data(), kScriptProfiles.size() };
    }

    std::vector<EditorSceneSeedEntity> editor_seed_entities_for_project(std::string_view project_id)
    {
        if (const auto* application = editor_application_for_project(project_id))
            return make_editor_application_scene(application->kind).entities;
        if (project_id == "platformer")
            return platformer_seed_entities();
        if (project_id == "softwarestudio")
            return software_seed_entities();
        if (const auto* profile = editor_find_project_profile(project_id))
        {
            if (profile->kind == EditorProjectKind::Editor)
                return make_editor_application_scene(EditorApplicationKind::Standard).entities;
            return profile->kind == EditorProjectKind::Tool ? software_seed_entities() : sandbox_seed_entities();
        }
        return sandbox_seed_entities();
    }

    std::string_view editor_runtime_scene_for_project(std::string_view project_id) noexcept
    {
        if (const auto* profile = editor_find_project_profile(project_id))
            return profile->runtime_scene_id;
        return editor_default_project_profile().runtime_scene_id;
    }

    std::string_view editor_project_kind_name(EditorProjectKind kind) noexcept
    {
        switch (kind)
        {
        case EditorProjectKind::Editor:
            return "Epoch Editor";
        case EditorProjectKind::EngineDevelopment:
            return "Engine Development";
        case EditorProjectKind::Tool:
            return "Software / Tool";
        case EditorProjectKind::Game:
        default:
            return "Game";
        }
    }
    std::string editor_project_demo_model_path(std::string_view project_id)
    {
        const auto* profile = editor_find_project_profile(project_id);
        if (!profile)
            return {};

        return resolve_project_demo_model_path(*profile).generic_string();
    }

    EditorProjectModelSummary editor_project_model_summary(std::string_view project_id)
    {
        EditorProjectModelSummary summary{};
        const auto* profile = editor_find_project_profile(project_id);
        if (!profile)
        {
            summary.summary = "Unknown project profile.";
            return summary;
        }

        summary.declared = !profile->demo_model_asset.empty();
        summary.asset_path = std::string(profile->demo_model_asset);
        if (!summary.declared)
        {
            summary.summary = "No demo model declared for this project.";
            return summary;
        }

        const fs::path resolved = resolve_project_demo_model_path(*profile);
        summary.resolved_path = resolved.generic_string();

        std::error_code ec;
        summary.exists = !resolved.empty() && fs::exists(resolved, ec) && !ec;
        if (!summary.exists)
        {
            summary.summary = "Declared demo model asset is missing.";
            return summary;
        }

        if (resolved.extension() != ".gltf")
        {
            summary.summary = "Demo model asset exists; parser summary is currently glTF-only.";
            return summary;
        }

#if !EPOCH_HAS_CGLTF
        summary.summary = "Demo model asset exists; cgltf headers are unavailable, so glTF parsing was skipped.";
        return summary;
#else
        cgltf_options options{};
        cgltf_data* data = nullptr;
        const std::string resolvedText = resolved.string();
        const cgltf_result parseResult = cgltf_parse_file(&options, resolvedText.c_str(), &data);
        if (parseResult != cgltf_result_success || data == nullptr)
        {
            summary.summary = "Demo model asset exists but glTF parsing failed.";
            return summary;
        }

        summary.parsed = true;
        summary.scene_count = static_cast<std::uint32_t>(data->scenes_count);
        summary.node_count = static_cast<std::uint32_t>(data->nodes_count);
        summary.mesh_count = static_cast<std::uint32_t>(data->meshes_count);
        summary.material_count = static_cast<std::uint32_t>(data->materials_count);
        for (cgltf_size meshIndex = 0; meshIndex < data->meshes_count; ++meshIndex)
            summary.primitive_count += static_cast<std::uint32_t>(data->meshes[meshIndex].primitives_count);

        summary.summary =
            std::to_string(summary.scene_count) + " scene(s) | "
            + std::to_string(summary.node_count) + " node(s) | "
            + std::to_string(summary.mesh_count) + " mesh(es) | "
            + std::to_string(summary.primitive_count) + " primitive(s) | "
            + std::to_string(summary.material_count) + " material(s)";

        cgltf_free(data);
        return summary;
#endif
    }

    bool editor_project_manifest_capability_contract() noexcept
    {
#if defined(_WIN32)
        const fs::path candidateData{"C:/epoch/contracts/candidate-data"};
#else
        const fs::path candidateData{"/epoch/contracts/candidate-data"};
#endif
        // Exercise the production lexical router without reading real projects
        // or changing the process's startup-frozen candidate configuration.
        const fs::path candidateProjects = candidateData / "Projects";
        const bool candidatePathGate =
            candidate_project_path_lexical(candidateData, "Projects/Example")
                == candidateProjects / "Example"
            && candidate_project_path_lexical(candidateData,
                candidateProjects / "Example" / "project.epoch.json")
                == candidateProjects / "Example" / "project.epoch.json"
            && candidate_project_path_lexical(candidateData, "Projects", true)
                == candidateProjects
            && candidate_project_path_lexical(candidateData, "Projects").empty()
            && candidate_project_path_lexical(candidateData, "Engine/src/editor").empty()
            && candidate_project_path_lexical(candidateData, "ProjectsSibling/Example").empty()
            && candidate_project_path_lexical(candidateData, "Projects/../outside").empty()
            && candidate_project_path_lexical(candidateData, "Projects/Example/./scene.epoch").empty()
            && candidate_project_path_lexical(candidateData, "Projects/Example/scene.epoch:stream").empty()
            && candidate_project_path_lexical(candidateData,
                candidateData.parent_path() / "other-candidate" / "Projects" / "Example").empty()
            && !project_path_below(candidateProjects / "Other" / "scene.epoch",
                candidateProjects / "Example")
            && project_path_below(candidateProjects / "Example" / "worlds" / "scene.epoch",
                candidateProjects / "Example");
        for (const auto& profile : kProjectProfiles)
        {
            const fs::path scene{profile.scene_path};
            const auto routed = candidate_project_path_lexical(candidateData, scene);
            if (routed.empty() || routed != (candidateData / scene).lexically_normal()
                || !project_path_below(routed, candidateProjects))
                return false;
        }
        const std::string legacyLinks = "raylib.lib;setupapi.lib;cfgmgr32.lib;version.lib;imm32.lib;winmm.lib;ole32.lib;oleaut32.lib;uuid.lib;advapi32.lib;userenv.lib;bcrypt.lib;user32.lib;gdi32.lib;shell32.lib;EpochEngine.lib;EpochGui.lib;%(AdditionalDependencies)";
        std::string linkFixture = "<ItemDefinitionGroup Condition=\"Debug|x64\"><Link><AdditionalDependencies>"
            + legacyLinks + "</AdditionalDependencies></Link></ItemDefinitionGroup>"
            "<ItemDefinitionGroup Condition=\"Release|x64\"><Link><AdditionalDependencies>"
            + legacyLinks + "</AdditionalDependencies></Link></ItemDefinitionGroup>"
            "<ItemDefinitionGroup Condition=\"Debug|x64\"><Link><AdditionalDependencies>custom.lib;%(AdditionalDependencies)</AdditionalDependencies></Link></ItemDefinitionGroup>";
        repair_generated_project_link_dependencies(linkFixture);
        const auto onceRepaired = linkFixture;
        repair_generated_project_link_dependencies(linkFixture);
        const bool generatedLinkGate = linkFixture == onceRepaired
            && linkFixture.find(generated_child_project_link_dependencies(true)) != std::string::npos
            && linkFixture.find(generated_child_project_link_dependencies(false)) != std::string::npos
            && linkFixture.find("custom.lib;%(AdditionalDependencies)") != std::string::npos;
        const JsonStringFieldResult missing =
            inspect_json_string_field(R"({"id":"legacy"})", "capability_profile");
        const JsonStringFieldResult valid =
            inspect_json_string_field(
                R"({"capability_profile":"portable"})",
                "capability_profile");
        const JsonStringFieldResult unknown =
            inspect_json_string_field(
                R"({"capability_profile":"future-tier"})",
                "capability_profile");
        const JsonStringFieldResult duplicate =
            inspect_json_string_field(
                R"({"capability_profile":"portable","capability_profile":"explicit"})",
                "capability_profile");
        const JsonStringFieldResult wrongType =
            inspect_json_string_field(
                R"({"capability_profile":42})",
                "capability_profile");
        const JsonStringFieldResult unterminated =
            inspect_json_string_field(
                R"({"capability_profile":"portable})",
                "capability_profile");

        const ProjectShellSpec projectSpec{
            .kind = EditorProjectKind::Game,
            .project_name = "GUI Editor",
            .project_id = "twodstudio",
            .world_name = "TwoD_Main",
            .tilemap_path = "Assets/Maps/main.epochmap",
            .input_profile_path = "Assets/Config/input_profile.epochinput",
            .sprite_animation_path =
                "Assets/Animations/sprite_animations.epochanim",
            .template_family = "game-2d-project",
            .script_id = "project_demo_bootstrap"
        };
        const std::string defaultWorld =
            make_project_world_scene_text(projectSpec, "game", false);
        const std::string arcadeWorld =
            make_project_world_scene_text(projectSpec, "game", true);
        const bool explicitPackageGate =
            defaultWorld.find("engine_arcade") == std::string::npos
            && defaultWorld.find("EngineArcade") == std::string::npos
            && arcadeWorld.find("package \"engine_arcade\"") != std::string::npos
            && arcadeWorld.find("EngineArcadeCabinetBody") != std::string::npos;

        const ProjectShellSpec editorSpec{
            .kind = EditorProjectKind::Editor,
            .project_name = "Editor Project Contract",
            .project_id = "editor-project-contract",
            .world_name = default_world_name(EditorProjectKind::Editor),
            .template_family = "editor-project",
            .script_id = "tool_bootstrap"};
        const auto editorWorld = epochengine::scene::parse_snapshot_text(
            make_project_world_scene_text(editorSpec, "editor", false));
        const bool editorProjectGate = editorWorld
            && editorWorld.snapshot.project_id == editorSpec.project_id
            && editorWorld.snapshot.document_kind == "editor"
            && editorWorld.snapshot.world_name == "EditorWorkspace"
            && editorWorld.snapshot.objects.empty()
            && parse_project_manifest_kind(project_manifest_kind(EditorProjectKind::Editor))
                == EditorProjectKind::Editor
            && !editor_project_input_profile_enabled(EditorProjectKind::Editor);

        constexpr std::string_view legacyManifest =
            R"({"id":"twodstudio","display_name":"GUI Editor"})";
        const auto migratedInput = migrated_project_manifest_path_text(
            legacyManifest,
            "input_profile",
            epochengine::project_input::canonical_source_path);
        const auto emptyInput = migrated_project_manifest_path_text(
            "{}",
            "input_profile",
            epochengine::project_input::canonical_source_path);
        const auto invalidInputPath = migrated_project_manifest_path_text(
            legacyManifest,
            "input_profile",
            "Assets/input.epochinput");
        const auto danglingInput = migrated_project_manifest_path_text(
            R"({"id":"twodstudio",})",
            "input_profile",
            epochengine::project_input::canonical_source_path);
        const auto nonObjectInput = migrated_project_manifest_path_text(
            R"(["twodstudio"])",
            "input_profile",
            epochengine::project_input::canonical_source_path);
        const auto migratedGui = migrated_project_manifest_path_text(
            legacyManifest,
            "gui",
            epochengine::project_gui::canonical_source_path);
        const auto invalidGuiPath = migrated_project_manifest_path_text(
            legacyManifest,
            "gui",
            "Assets/Gui/alternate.epochgui");
        const JsonStringFieldResult missingProjectFormat =
            inspect_json_string_field(legacyManifest, "project_format");
        const JsonStringFieldResult validProjectFormat =
            inspect_json_string_field(
                R"({"project_format":"epoch-project-v1"})",
                "project_format");
        const JsonStringFieldResult futureProjectFormat =
            inspect_json_string_field(
                R"({"project_format":"epoch-project-v2"})",
                "project_format");
        const JsonStringFieldResult duplicateProjectFormat =
            inspect_json_string_field(
                R"({"project_format":"epoch-project-v1","project_format":"epoch-project-v1"})",
                "project_format");
        const JsonStringFieldResult typedProjectFormat =
            inspect_json_string_field(
                R"({"project_format":1})",
                "project_format");
        const auto migratedProjectFormat = migrated_project_manifest_path_text(
            legacyManifest,
            "project_format",
            kProjectManifestFormat);
        const auto invalidProjectFormat = migrated_project_manifest_path_text(
            legacyManifest,
            "project_format",
            "epoch-project-v2");
        const auto migratedBuildProfile = migrated_project_manifest_path_text(
            legacyManifest,
            "build_profile",
            kStaticRuntimeBuildProfile);
        const auto invalidBuildProfile = migrated_project_manifest_path_text(
            legacyManifest,
            "build_profile",
            "editor-full");
        const bool projectFormatMigrationGate =
            missingProjectFormat.state == JsonStringFieldState::missing
            && validProjectFormat.state == JsonStringFieldState::present
            && validProjectFormat.value == kProjectManifestFormat
            && futureProjectFormat.state == JsonStringFieldState::present
            && futureProjectFormat.value != kProjectManifestFormat
            && duplicateProjectFormat.state == JsonStringFieldState::malformed
            && typedProjectFormat.state == JsonStringFieldState::malformed
            && migratedProjectFormat
            && inspect_json_string_field(
                *migratedProjectFormat, "project_format").value
                == kProjectManifestFormat
            && !invalidProjectFormat;
        const bool buildProfileMigrationGate = migratedBuildProfile
            && inspect_json_string_field(
                *migratedBuildProfile, "build_profile").value
                == kStaticRuntimeBuildProfile
            && !invalidBuildProfile;
        const bool guiMigrationGate = migratedGui
            && inspect_json_string_field(*migratedGui, "gui").value
                == epochengine::project_gui::canonical_source_path
            && !invalidGuiPath;
        const bool inputMigrationGate = migratedInput
            && emptyInput
            && migratedInput->find(
                legacyManifest.substr(1u, legacyManifest.size() - 2u))
                != std::string::npos
            && inspect_json_string_field(
                *migratedInput, "input_profile").value
                == epochengine::project_input::canonical_source_path
            && inspect_json_string_field(
                *emptyInput, "input_profile").value
                == epochengine::project_input::canonical_source_path
            && !invalidInputPath
            && !danglingInput
            && !nonObjectInput;
        const auto generatedInputSource =
            epochengine::project_input::make_legacy_default_profile();
        const auto serializedGeneratedInput =
            epochengine::project_input::serialize_profile_source(
                generatedInputSource);
        const auto decodedGeneratedInput = serializedGeneratedInput
            ? epochengine::project_input::deserialize_profile_source(
                serializedGeneratedInput.bytes)
            : epochengine::project_input::DeserializedProfile{};
        const auto compiledGeneratedInput = decodedGeneratedInput
            ? epochengine::project_input::compile_profile(
                "GameProjectContract", decodedGeneratedInput.source)
            : epochengine::project_input::CompiledProfileResult{};
        const bool hasKeyboardBinding = std::ranges::any_of(
            generatedInputSource.bindings,
            [](const epochengine::project_input::BindingDefinition& binding)
            {
                return binding.device
                    == epochengine::project_input::BindingDevice::keyboard;
            });
        const bool hasControllerBinding = std::ranges::any_of(
            generatedInputSource.bindings,
            [](const epochengine::project_input::BindingDefinition& binding)
            {
                return binding.device
                    == epochengine::project_input::BindingDevice::controller_button
                    || binding.device
                        == epochengine::project_input::BindingDevice::controller_axis;
            });
        const bool platformerInputGate = std::ranges::any_of(
            kProjectProfiles,
            [](const EditorProjectProfile& profile)
            {
                return profile.id == "platformer"
                    && profile.kind == EditorProjectKind::Game
                    && profile.input_profile_path
                        == epochengine::project_input::canonical_source_path;
            });
        const bool generatedInputProvisionGate =
            editor_project_input_profile_enabled(EditorProjectKind::Game)
            && editor_project_input_profile_enabled(
                EditorProjectKind::Game,
                EditorProjectInputProvision::explicitly_enabled)
            && !editor_project_input_profile_enabled(EditorProjectKind::Tool)
            && !editor_project_input_profile_enabled(
                EditorProjectKind::EngineDevelopment)
            && editor_project_input_profile_enabled(
                EditorProjectKind::Tool,
                EditorProjectInputProvision::explicitly_enabled)
            && editor_project_input_profile_enabled(
                EditorProjectKind::EngineDevelopment,
                EditorProjectInputProvision::explicitly_enabled)
            && serializedGeneratedInput
            && decodedGeneratedInput
            && decodedGeneratedInput.source == generatedInputSource
            && compiledGeneratedInput
            && compiledGeneratedInput.artifact.actions
                == generatedInputSource.actions
            && compiledGeneratedInput.artifact.bindings
                == generatedInputSource.bindings
            && hasKeyboardBinding
            && hasControllerBinding
            && platformerInputGate;
        return generatedLinkGate && candidatePathGate && missing.state == JsonStringFieldState::missing
            && valid.state == JsonStringFieldState::present
            && valid.value == "portable"
            && editor_project_capability_policy(valid.value).has_value()
            && unknown.state == JsonStringFieldState::present
            && !editor_project_capability_policy(unknown.value).has_value()
            && duplicate.state == JsonStringFieldState::malformed
            && wrongType.state == JsonStringFieldState::malformed
            && unterminated.state == JsonStringFieldState::malformed
            && explicitPackageGate
            && editorProjectGate
            && inputMigrationGate
            && generatedInputProvisionGate
            && guiMigrationGate
            && projectFormatMigrationGate
            && buildProfileMigrationGate;
    }

    EditorProjectCreationResult editor_create_project_shell(EditorProjectKind kind)
    {
        return editor_create_project_shell(
            kind, EditorProjectInputProvision::project_default);
    }

    EditorProjectCreationResult editor_create_project_shell(
        EditorProjectKind kind,
        EditorProjectInputProvision input_provision)
    {
        const std::string projectName = next_generated_project_name(kind);
        const std::string defaultScript = kind == EditorProjectKind::Tool || kind == EditorProjectKind::Editor
            ? "tool_bootstrap"
            : "project_demo_bootstrap";

        return write_project_shell(ProjectShellSpec{
            .kind = kind,
            .project_name = projectName,
            .project_id = projectName,
            .root = resolve_projects_root() / projectName,
            .world_file = kind == EditorProjectKind::Tool
                ? fs::path{ "Projects" } / projectName / "worlds" / "tool.epoch"
                : fs::path{ "Projects" } / projectName / "worlds" / "main.epoch",
            .world_name = default_world_name(kind),
            .input_profile_path = editor_project_input_profile_enabled(
                kind, input_provision)
                ? std::string{epochengine::project_input::canonical_source_path}
                : std::string{},
            .template_family = kind == EditorProjectKind::Editor ? "editor-project"
                : (kind == EditorProjectKind::Tool ? "tool-project" : "game-project"),
            .script_id = defaultScript,
            .description = kind == EditorProjectKind::Editor
                ? "Epoch Editor application linked to this edition's admitted engine source."
                : (kind == EditorProjectKind::Tool
                    ? "Generated software/tool shell."
                    : "Generated game shell."),
            .demo_model_asset = std::string{},
            .include_engine_arcade_package = false,
            .overwrite_existing = false
        });
    }

    EditorProjectCreationResult editor_ensure_project_shell(std::string_view project_id)
    {
        const auto* profile = editor_find_project_profile(project_id);
        if (!profile)
        {
            return {
                false,
                std::string(project_id),
                {},
                {},
                {},
                {},
                {},
                "Unknown project profile.",
                {},
                {}
            };
        }

        const fs::path root = resolve_project_root_path(fs::path{ profile->root_path });
        if (root.empty())
            return {.project_id = std::string{project_id},
                .summary = "Project shell refused an unavailable or non-private candidate project root."};
        const fs::path manifest = root / "project.epoch.json";
        const fs::path buildScript = generated_project_windows_build_script_path(root);
        const fs::path entrySource = generated_project_entry_source_path(root);
        const fs::path windowsProject = generated_project_windows_vcxproj_path(root);
        std::error_code ec;
        if (fs::exists(manifest, ec) && !ec
            && fs::exists(buildScript, ec) && !ec
            && fs::exists(entrySource, ec) && !ec
            && fs::exists(windowsProject, ec) && !ec)
        {
            const std::string manifestText = read_text_file(manifest);
            const auto manifestId = extract_json_string_field(manifestText, "id");
            const auto manifestKind = extract_json_string_field(manifestText, "kind");
            const auto manifestScript = extract_json_string_field(manifestText, "default_script");
            const auto manifestTemplate = extract_json_string_field(manifestText, "template_family");
            const auto manifestTileMap = extract_json_string_field(manifestText, "tilemap");
            const JsonStringFieldResult inputProfileField =
                inspect_json_string_field(manifestText, "input_profile");
            const auto manifestInputProfile = inputProfileField.state
                    == JsonStringFieldState::present
                ? std::optional<std::string>{inputProfileField.value}
                : std::nullopt;
            const JsonStringFieldResult audioProfileField =
                inspect_json_string_field(manifestText, "audio_profile");
            const auto manifestAudioProfile = audioProfileField.state
                    == JsonStringFieldState::present
                ? std::optional<std::string>{audioProfileField.value}
                : std::nullopt;
            const JsonStringFieldResult guiField =
                inspect_json_string_field(manifestText, "gui");
            const auto manifestGui = guiField.state
                    == JsonStringFieldState::present
                ? std::optional<std::string>{guiField.value}
                : std::nullopt;
            const auto manifestDisplayName = extract_json_string_field(manifestText, "display_name");
            const auto manifestWindowsProject = extract_json_string_field(manifestText, "windows_project");
            const JsonStringFieldResult projectFormatField =
                inspect_json_string_field(manifestText, "project_format");
            const JsonStringFieldResult buildProfileField =
                inspect_json_string_field(manifestText, "build_profile");
            const JsonStringFieldResult capabilityField =
                inspect_json_string_field(manifestText, "capability_profile");
            if (inputProfileField.state == JsonStringFieldState::malformed)
            {
                return EditorProjectCreationResult{
                    .succeeded = false,
                    .project_id = std::string(profile->id),
                    .root_path = root.generic_string(),
                    .manifest_path = manifest.generic_string(),
                    .summary = "Project manifest input_profile must be one unique JSON string.",
                    .engine_integration_mode = std::string(profile->engine_integration_mode),
                    .public_include_root = std::string(profile->public_include_root)
                };
            }
            if (inputProfileField.state == JsonStringFieldState::present
                && inputProfileField.value
                    != epochengine::project_input::canonical_source_path)
            {
                return EditorProjectCreationResult{
                    .succeeded = false,
                    .project_id = std::string(profile->id),
                    .root_path = root.generic_string(),
                    .manifest_path = manifest.generic_string(),
                    .summary = "Project manifest input_profile must use the canonical Assets/Config location.",
                    .engine_integration_mode = std::string(profile->engine_integration_mode),
                    .public_include_root = std::string(profile->public_include_root)
                };
            }
            if (audioProfileField.state == JsonStringFieldState::malformed)
            {
                return EditorProjectCreationResult{
                    .succeeded = false,
                    .project_id = std::string(profile->id),
                    .root_path = root.generic_string(),
                    .manifest_path = manifest.generic_string(),
                    .summary = "Project manifest audio_profile must be one unique JSON string.",
                    .engine_integration_mode = std::string(profile->engine_integration_mode),
                    .public_include_root = std::string(profile->public_include_root)
                };
            }
            if (audioProfileField.state == JsonStringFieldState::present
                && audioProfileField.value
                    != epochengine::project_audio::canonical_source_path)
            {
                return EditorProjectCreationResult{
                    .succeeded = false,
                    .project_id = std::string(profile->id),
                    .root_path = root.generic_string(),
                    .manifest_path = manifest.generic_string(),
                    .summary = "Project manifest audio_profile must use the canonical Assets/Audio location.",
                    .engine_integration_mode = std::string(profile->engine_integration_mode),
                    .public_include_root = std::string(profile->public_include_root)
                };
            }
            if (guiField.state == JsonStringFieldState::malformed)
            {
                return EditorProjectCreationResult{
                    .succeeded = false,
                    .project_id = std::string(profile->id),
                    .root_path = root.generic_string(),
                    .manifest_path = manifest.generic_string(),
                    .summary = "Project manifest gui must be one unique JSON string.",
                    .engine_integration_mode =
                        std::string(profile->engine_integration_mode),
                    .public_include_root =
                        std::string(profile->public_include_root)
                };
            }
            if (guiField.state == JsonStringFieldState::present
                && guiField.value
                    != epochengine::project_gui::canonical_source_path)
            {
                return EditorProjectCreationResult{
                    .succeeded = false,
                    .project_id = std::string(profile->id),
                    .root_path = root.generic_string(),
                    .manifest_path = manifest.generic_string(),
                    .summary = "Project manifest gui must use the canonical Assets/Gui location.",
                    .engine_integration_mode =
                        std::string(profile->engine_integration_mode),
                    .public_include_root =
                        std::string(profile->public_include_root)
                };
            }
            if (projectFormatField.state == JsonStringFieldState::malformed
                || (projectFormatField.state == JsonStringFieldState::present
                    && projectFormatField.value != kProjectManifestFormat))
            {
                return EditorProjectCreationResult{
                    .succeeded = false,
                    .project_id = std::string(profile->id),
                    .root_path = root.generic_string(),
                    .manifest_path = manifest.generic_string(),
                    .summary = "Project manifest project_format must be the unique epoch-project-v1 format.",
                    .engine_integration_mode =
                        std::string(profile->engine_integration_mode),
                    .public_include_root =
                        std::string(profile->public_include_root)
                };
            }
            if (buildProfileField.state == JsonStringFieldState::malformed
                || (buildProfileField.state == JsonStringFieldState::present
                    && buildProfileField.value != kStaticRuntimeBuildProfile))
            {
                return EditorProjectCreationResult{
                    .succeeded = false,
                    .project_id = std::string(profile->id),
                    .root_path = root.generic_string(),
                    .manifest_path = manifest.generic_string(),
                    .summary = "Project manifest build_profile must be the unique epoch-runtime-static profile.",
                    .engine_integration_mode =
                        std::string(profile->engine_integration_mode),
                    .public_include_root =
                        std::string(profile->public_include_root)
                };
            }
            if (capabilityField.state == JsonStringFieldState::malformed)
            {
                return EditorProjectCreationResult{
                    .succeeded = false,
                    .project_id = std::string(profile->id),
                    .root_path = root.generic_string(),
                    .manifest_path = manifest.generic_string(),
                    .summary = "Project manifest capability_profile must be one unique JSON string.",
                    .engine_integration_mode = std::string(profile->engine_integration_mode),
                    .public_include_root = std::string(profile->public_include_root)
                };
            }
            if (capabilityField.state == JsonStringFieldState::present)
            {
                const auto parsedPolicy =
                    epochengine::editor_project_capability_policy(capabilityField.value);
                if (!parsedPolicy || parsedPolicy->id != profile->renderer_capability.id)
                {
                    return EditorProjectCreationResult{
                        .succeeded = false,
                        .project_id = std::string(profile->id),
                        .root_path = root.generic_string(),
                        .manifest_path = manifest.generic_string(),
                        .summary = "Project manifest capability_profile is unknown or does not match the selected project profile.",
                        .engine_integration_mode = std::string(profile->engine_integration_mode),
                        .public_include_root = std::string(profile->public_include_root)
                    };
                }
            }
            const std::string expectedKind = profile->id == "sandbox"
                ? "engine-development-sandbox"
                : std::string(project_manifest_kind(profile->kind));
            const bool missingLegacyInputProfile =
                !profile->input_profile_path.empty()
                && inputProfileField.state == JsonStringFieldState::missing;
            const bool missingLegacyGui =
                !profile->gui_path.empty()
                && guiField.state == JsonStringFieldState::missing;
            const bool missingLegacyProjectFormat =
                projectFormatField.state == JsonStringFieldState::missing;
            const bool missingLegacyBuildProfile =
                buildProfileField.state == JsonStringFieldState::missing;
            const bool manifestMatchesProfile =
                manifestId && *manifestId == profile->id
                && manifestKind && *manifestKind == expectedKind
                && manifestScript && *manifestScript == profile->default_script
                && manifestTemplate && *manifestTemplate == profile->template_family
                && (profile->tilemap_path.empty()
                    ? !manifestTileMap
                    : manifestTileMap && *manifestTileMap == profile->tilemap_path)
                && (profile->input_profile_path.empty()
                    ? !manifestInputProfile
                    : missingLegacyInputProfile
                        || (manifestInputProfile
                            && *manifestInputProfile
                                == profile->input_profile_path))
                && (profile->audio_profile_path.empty()
                    ? !manifestAudioProfile
                    : manifestAudioProfile
                        && *manifestAudioProfile
                            == profile->audio_profile_path)
                && (profile->gui_path.empty()
                    ? !manifestGui
                    : missingLegacyGui
                        || (manifestGui
                            && *manifestGui == profile->gui_path))
                && manifestDisplayName && *manifestDisplayName == profile->display_name
                && manifestWindowsProject && *manifestWindowsProject == windowsProject.filename().generic_string();

            if (!manifestMatchesProfile)
            {
                return EditorProjectCreationResult{
                    .succeeded = false,
                    .project_id = std::string(profile->id),
                    .root_path = root.generic_string(),
                    .manifest_path = manifest.generic_string(),
                    .entry_source_path = entrySource.generic_string(),
                    .build_script_path = buildScript.generic_string(),
                    .default_script_path =
                        (root / "scripts"
                            / (std::string(profile->default_script)
                                + ".ascript.cpp")).generic_string(),
                    .summary =
                        "Project source differs from its registered profile. Epoch will not overwrite an existing project; open its manifest directly or create a new project.",
                    .engine_integration_mode =
                        std::string(profile->engine_integration_mode),
                    .public_include_root =
                        std::string(profile->public_include_root)
                };
            }

            if (missingLegacyProjectFormat
                && !migrate_project_manifest_path(
                    manifest,
                    "project_format",
                    kProjectManifestFormat,
                    ".project-format.tmp"))
            {
                return EditorProjectCreationResult{
                    .succeeded = false,
                    .project_id = std::string(profile->id),
                    .root_path = root.generic_string(),
                    .manifest_path = manifest.generic_string(),
                    .summary =
                        "Project shell exists, but its project-format migration failed.",
                    .engine_integration_mode =
                        std::string(profile->engine_integration_mode),
                    .public_include_root =
                        std::string(profile->public_include_root)
                };
            }
            if (missingLegacyInputProfile
                && !migrate_project_manifest_path(
                    manifest,
                    "input_profile",
                    profile->input_profile_path,
                    ".input-profile.tmp"))
            {
                return EditorProjectCreationResult{
                    .succeeded = false,
                    .project_id = std::string(profile->id),
                    .root_path = root.generic_string(),
                    .manifest_path = manifest.generic_string(),
                    .summary = "Project shell exists, but its input-profile manifest migration failed.",
                    .engine_integration_mode = std::string(profile->engine_integration_mode),
                    .public_include_root = std::string(profile->public_include_root)
                };
            }
            if (missingLegacyGui
                && !migrate_project_manifest_path(
                    manifest,
                    "gui",
                    profile->gui_path,
                    ".gui.tmp"))
            {
                return EditorProjectCreationResult{
                    .succeeded = false,
                    .project_id = std::string(profile->id),
                    .root_path = root.generic_string(),
                    .manifest_path = manifest.generic_string(),
                    .summary =
                        "Project shell exists, but its GUI manifest migration failed.",
                    .engine_integration_mode =
                        std::string(profile->engine_integration_mode),
                    .public_include_root =
                        std::string(profile->public_include_root)
                };
            }
            if (missingLegacyBuildProfile
                && !migrate_project_manifest_path(
                    manifest,
                    "build_profile",
                    kStaticRuntimeBuildProfile,
                    ".build-profile.tmp"))
            {
                return EditorProjectCreationResult{
                    .succeeded = false,
                    .project_id = std::string(profile->id),
                    .root_path = root.generic_string(),
                    .manifest_path = manifest.generic_string(),
                    .summary =
                        "Project shell exists, but its static-runtime build-profile migration failed.",
                    .engine_integration_mode =
                        std::string(profile->engine_integration_mode),
                    .public_include_root =
                        std::string(profile->public_include_root)
                };
            }
            if (!profile->input_profile_path.empty()
                && !ensure_project_input_source(profile->id, root))
            {
                return EditorProjectCreationResult{
                    .succeeded = false,
                    .project_id = std::string(profile->id),
                    .root_path = root.generic_string(),
                    .manifest_path = manifest.generic_string(),
                    .summary = "Project shell exists, but its canonical input source is missing or invalid.",
                    .engine_integration_mode = std::string(profile->engine_integration_mode),
                    .public_include_root = std::string(profile->public_include_root)
                };
            }

            if (!profile->audio_profile_path.empty()
                && !ensure_project_audio_source(profile->id, root))
            {
                return EditorProjectCreationResult{
                    .succeeded = false,
                    .project_id = std::string(profile->id),
                    .root_path = root.generic_string(),
                    .manifest_path = manifest.generic_string(),
                    .summary = "Project shell exists, but its canonical audio source or clips are missing or invalid.",
                    .engine_integration_mode = std::string(profile->engine_integration_mode),
                    .public_include_root = std::string(profile->public_include_root)
                };
            }
            std::string materializationDiagnostic{};
            if (!ensure_project_2d_assets(
                    profile->id,
                    root,
                    profile->tilemap_path,
                    profile->sprite_animation_path,
                    materializationDiagnostic))
            {
                return EditorProjectCreationResult{
                    .succeeded = false,
                    .project_id = std::string(profile->id),
                    .root_path = root.generic_string(),
                    .manifest_path = manifest.generic_string(),
                    .summary = materializationDiagnostic,
                    .engine_integration_mode =
                        std::string(profile->engine_integration_mode),
                    .public_include_root =
                        std::string(profile->public_include_root)
                };
            }
            if (!ensure_project_gui_assets(
                    profile->id,
                    root,
                    profile->gui_path,
                    materializationDiagnostic))
            {
                return EditorProjectCreationResult{
                    .succeeded = false,
                    .project_id = std::string(profile->id),
                    .root_path = root.generic_string(),
                    .manifest_path = manifest.generic_string(),
                    .summary = materializationDiagnostic,
                    .engine_integration_mode =
                        std::string(profile->engine_integration_mode),
                    .public_include_root =
                        std::string(profile->public_include_root)
                };
            }
        }

        return write_project_shell(ProjectShellSpec{
            .kind = profile->kind,
            .project_name = std::string(profile->display_name),
            .project_id = std::string(profile->id),
            .root = root,
            .world_file = fs::path{ profile->scene_path },
            .world_name = std::string(profile->world_name),
            .tilemap_path = std::string(profile->tilemap_path),
            .input_profile_path = std::string(profile->input_profile_path),
            .sprite_animation_path = std::string(profile->sprite_animation_path),
            .audio_profile_path = std::string(profile->audio_profile_path),
            .gui_path = std::string(profile->gui_path),
            .template_family = std::string(profile->template_family),
            .script_id = std::string(profile->default_script),
            .description = std::string(profile->description),
            .demo_model_asset = std::string(profile->demo_model_asset),
            .capability_profile = std::string(profile->renderer_capability.id),
            .include_engine_arcade_package = false,
            .overwrite_existing = false
        });
    }

    EditorProjectInputProfileSummary editor_project_input_profile_summary(
        std::string_view project_id)
    {
        EditorProjectInputProfileSummary result{};
        const auto* profile = editor_find_project_profile(project_id);
        if (!profile || profile->input_profile_path.empty())
        {
            result.diagnostic = "Project does not declare a runtime input profile.";
            return result;
        }

        const fs::path root = resolve_project_root_path(
            fs::path{profile->root_path});
        epochengine::project_input::ProjectInputProfileStore store{
            std::string{project_id}, root};
        result.source_path = std::string{profile->input_profile_path};
        if (!store.valid())
        {
            result.diagnostic = "Project input store is invalid.";
            return result;
        }

        epochengine::project_input::ProfileSource source{};
        const auto loaded = store.load_source();
        if (loaded)
        {
            source = loaded.source;
        }
        else if (loaded.code == epochengine::project_input::StoreCode::not_found)
        {
            const auto artifact = store.load_artifact();
            if (artifact)
            {
                source = {
                    .id = artifact.artifact.profile_id,
                    .display_name = artifact.artifact.display_name,
                    .revision = artifact.artifact.source_revision,
                    .actions = artifact.artifact.actions,
                    .bindings = artifact.artifact.bindings
                };
                result.diagnostic =
                    "Compiled profile is active; source can be materialized.";
            }
            else if (artifact.code == epochengine::project_input::StoreCode::not_found)
            {
                source = epochengine::project_input::make_legacy_default_profile();
                result.diagnostic = "Default profile is ready to materialize.";
            }
            else
            {
                result.diagnostic = std::string{"Input artifact "}
                    + std::string{epochengine::project_input::store_code_name(artifact.code)};
                return result;
            }
        }
        else
        {
            result.diagnostic = std::string{"Input source "}
                + std::string{epochengine::project_input::store_code_name(loaded.code)};
            return result;
        }

        result.ready = epochengine::project_input::validate_profile_source(source)
            == epochengine::project_input::ValidationCode::ready;
        result.display_name = source.display_name;
        result.action_count = static_cast<std::uint32_t>(source.actions.size());
        result.binding_count = static_cast<std::uint32_t>(source.bindings.size());
        result.bindings.reserve(source.bindings.size());
        bool foundControllerAxis = false;
        for (const auto& binding : source.bindings)
        {
            const auto action = std::find_if(
                source.actions.begin(),
                source.actions.end(),
                [&](const auto& candidate)
                {
                    return candidate.id == binding.action;
                });
            std::string actionLabel = action != source.actions.end()
                ? std::string{
                    epochengine::project_input::action_semantic_name(
                        action->semantic)}
                : std::string{"unknown"};
            std::replace(
                actionLabel.begin(), actionLabel.end(), '_', ' ');
            if (!actionLabel.empty())
                actionLabel.front() = static_cast<char>(
                    std::toupper(
                        static_cast<unsigned char>(actionLabel.front())));
            if (action != source.actions.end()
                && action->value_kind
                    == epochengine::project_input::ActionValueKind::axis)
            {
                actionLabel += binding.scale_q15 < 0 ? " (-)" : " (+)";
            }

            std::string deviceLabel{};
            std::string sourceLabel{};
            EditorProjectInputBindingKind bindingKind{
                EditorProjectInputBindingKind::keyboard};
            switch (binding.device)
            {
            case epochengine::project_input::BindingDevice::keyboard:
                deviceLabel = "Keyboard";
                sourceLabel = epochengine::project_input::key_code_name(
                    static_cast<epochengine::project_input::KeyCode>(
                        binding.code));
                break;
            case epochengine::project_input::BindingDevice::controller_button:
                bindingKind =
                    EditorProjectInputBindingKind::controller_button;
                deviceLabel = "Controller Button";
                sourceLabel = std::string{
                    epochengine::project_input::controller_button_name(
                        static_cast<epochengine::project_input::ControllerButton>(
                            binding.code))}
                    + " / controller "
                    + std::to_string(binding.controller_slot + 1u);
                break;
            case epochengine::project_input::BindingDevice::controller_axis:
                bindingKind =
                    EditorProjectInputBindingKind::controller_axis;
                deviceLabel = "Controller Axis";
                sourceLabel = std::string{
                    epochengine::project_input::controller_axis_name(
                        static_cast<epochengine::project_input::ControllerAxis>(
                            binding.code))}
                    + " / controller "
                    + std::to_string(binding.controller_slot + 1u);
                break;
            }
            result.bindings.push_back({
                .stable_id = binding.id.value,
                .action = std::move(actionLabel),
                .device = std::move(deviceLabel),
                .source = std::move(sourceLabel),
                .code = binding.code,
                .dead_zone_q15 = binding.dead_zone_q15,
                .controller_slot = binding.controller_slot,
                .kind = bindingKind});

            if (binding.device
                != epochengine::project_input::BindingDevice::controller_axis)
            {
                continue;
            }
            if (!foundControllerAxis)
            {
                foundControllerAxis = true;
                result.controller_dead_zone_q15 = binding.dead_zone_q15;
            }
            else if (result.controller_dead_zone_q15
                != binding.dead_zone_q15)
            {
                result.uniform_controller_dead_zone = false;
            }
        }
        if (result.diagnostic.empty())
            result.diagnostic = result.ready ? "Project input source ready."
                                             : "Project input source invalid.";
        return result;
    }
    EditorProjectInputUpdateResult editor_reset_project_input_profile(
        std::string_view project_id)
    {
        const auto* profile = editor_find_project_profile(project_id);
        if (!profile || profile->input_profile_path.empty())
            return {false, "Project does not declare a runtime input profile."};

        const fs::path root = resolve_project_root_path(
            fs::path{profile->root_path});
        epochengine::project_input::ProjectInputProfileStore store{
            std::string{project_id}, root};
        if (!store.valid())
            return {false, "Project input store is invalid."};

        std::uint64_t revision = 1u;
        const auto loaded = store.load_source();
        if (loaded)
        {
            if (loaded.source.revision.sequence
                == (std::numeric_limits<std::uint64_t>::max)())
            {
                return {false, "Project input revision is exhausted."};
            }
            revision = loaded.source.revision.sequence + 1u;
        }
        else
        {
            const auto artifact = store.load_artifact();
            if (artifact)
            {
                if (artifact.artifact.source_revision.sequence
                    == (std::numeric_limits<std::uint64_t>::max)())
                {
                    return {false, "Project input revision is exhausted."};
                }
                revision = artifact.artifact.source_revision.sequence + 1u;
            }
        }

        const auto source =
            epochengine::project_input::make_legacy_default_profile(revision);
        const auto compiled = epochengine::project_input::compile_profile(
            project_id, source);
        if (!compiled)
        {
            return {
                false,
                std::string{"Default project input compile "}
                    + std::string{
                        epochengine::project_input::validation_code_name(
                            compiled.code)}};
        }
        const auto published = store.publish_source_and_artifact(
            source, compiled.artifact);
        if (!published)
        {
            if (published.source)
            {
                return {
                    true,
                    std::string{
                        "Default input source restored; artifact publish "}
                        + std::string{
                            epochengine::project_input::store_code_name(
                                published.code)}
                        + ". Build or Run will regenerate it."};
            }
            return {
                false,
                std::string{"Project input reset "}
                    + std::string{
                        epochengine::project_input::store_code_name(
                            published.code)}};
        }
        return {true, "Default project input source and artifact restored."};
    }

    template <typename EditOperation>
    [[nodiscard]] static EditorProjectInputUpdateResult
        edit_project_input_profile(
            std::string_view projectId,
            EditOperation&& operation,
            std::string unchangedSummary,
            std::string changedSummary)
    {
        const auto* profile = editor_find_project_profile(projectId);
        if (!profile || profile->input_profile_path.empty())
            return {false, "Project does not declare a runtime input profile."};

        const fs::path root = resolve_project_root_path(
            fs::path{profile->root_path});
        epochengine::project_input::ProjectInputProfileStore store{
            std::string{projectId}, root};
        if (!store.valid())
            return {false, "Project input store is invalid."};

        epochengine::project_input::ProfileSource source{};
        const auto loaded = store.load_source();
        if (loaded)
        {
            source = loaded.source;
        }
        else if (loaded.code == epochengine::project_input::StoreCode::not_found)
        {
            const auto artifact = store.load_artifact();
            if (artifact)
            {
                source = {
                    .id = artifact.artifact.profile_id,
                    .display_name = artifact.artifact.display_name,
                    .revision = artifact.artifact.source_revision,
                    .actions = artifact.artifact.actions,
                    .bindings = artifact.artifact.bindings};
            }
            else if (artifact.code
                == epochengine::project_input::StoreCode::not_found)
            {
                source =
                    epochengine::project_input::make_legacy_default_profile();
            }
            else
            {
                return {
                    false,
                    std::string{"Input artifact "}
                        + std::string{
                            epochengine::project_input::store_code_name(
                                artifact.code)}};
            }
        }
        else
        {
            return {
                false,
                std::string{"Input source "}
                    + std::string{
                        epochengine::project_input::store_code_name(
                            loaded.code)}};
        }

        const auto edited = std::forward<EditOperation>(operation)(
            std::move(source));
        if (!edited)
        {
            return {
                false,
                std::string{"Project input edit "}
                    + std::string{
                        epochengine::project_input::profile_edit_code_name(
                            edited.code)}
                    + " / "
                    + std::string{
                        epochengine::project_input::validation_code_name(
                            edited.validation)}};
        }
        const auto compiled = epochengine::project_input::compile_profile(
            projectId, edited.source);
        if (!compiled)
        {
            return {
                false,
                std::string{"Project input compile "}
                    + std::string{
                        epochengine::project_input::validation_code_name(
                            compiled.code)}};
        }
        const auto published = store.publish_source_and_artifact(
            edited.source, compiled.artifact);
        if (!published)
        {
            if (published.source)
            {
                return {
                    true,
                    std::string{
                        "Project input source saved; artifact publish "}
                        + std::string{
                            epochengine::project_input::store_code_name(
                                published.code)}
                        + ". Build or Run will regenerate it."};
            }
            return {
                false,
                std::string{"Project input publication "}
                    + std::string{
                        epochengine::project_input::store_code_name(
                            published.code)}};
        }
        return {
            true,
            edited.code == epochengine::project_input::ProfileEditCode::unchanged
                ? std::move(unchangedSummary)
                : std::move(changedSummary)};
    }

    EditorProjectInputUpdateResult editor_rebind_project_input(
        std::string_view project_id,
        std::uint64_t binding_id,
        std::uint16_t key_code)
    {
        return edit_project_input_profile(
            project_id,
            [binding_id, key_code](
                epochengine::project_input::ProfileSource source)
            {
                return epochengine::project_input::rebind_keyboard(
                    std::move(source),
                    epochengine::project_input::BindingId{binding_id},
                    static_cast<epochengine::project_input::KeyCode>(key_code));
            },
            "Project keyboard binding is already assigned.",
            "Project keyboard binding updated.");
    }

    EditorProjectInputUpdateResult editor_rebind_project_controller_button(
        std::string_view project_id,
        std::uint64_t binding_id,
        std::uint16_t button_code,
        std::uint8_t controller_slot)
    {
        return edit_project_input_profile(
            project_id,
            [binding_id, button_code, controller_slot](
                epochengine::project_input::ProfileSource source)
            {
                return epochengine::project_input::rebind_controller_button(
                    std::move(source),
                    epochengine::project_input::BindingId{binding_id},
                    static_cast<epochengine::project_input::ControllerButton>(
                        button_code),
                    controller_slot);
            },
            "Project controller button binding is already assigned.",
            "Project controller button binding updated.");
    }

    EditorProjectInputUpdateResult editor_rebind_project_controller_axis(
        std::string_view project_id,
        std::uint64_t binding_id,
        std::uint16_t axis_code,
        std::uint8_t controller_slot)
    {
        return edit_project_input_profile(
            project_id,
            [binding_id, axis_code, controller_slot](
                epochengine::project_input::ProfileSource source)
            {
                return epochengine::project_input::rebind_controller_axis(
                    std::move(source),
                    epochengine::project_input::BindingId{binding_id},
                    static_cast<epochengine::project_input::ControllerAxis>(
                        axis_code),
                    controller_slot);
            },
            "Project controller axis binding is already assigned.",
            "Project controller axis binding updated.");
    }

    EditorProjectInputUpdateResult editor_set_project_controller_dead_zone(
        std::string_view project_id,
        std::uint16_t dead_zone_q15)
    {
        return edit_project_input_profile(
            project_id,
            [dead_zone_q15](
                epochengine::project_input::ProfileSource source)
            {
                return epochengine::project_input::set_controller_dead_zone(
                    std::move(source), dead_zone_q15);
            },
            "Project controller dead zone is already assigned.",
            "Project controller dead zone updated.");
    }

    [[nodiscard]] static EditorProjectAudioCueSemantic editor_audio_semantic(
        epochengine::project_audio::CueSemantic semantic) noexcept
    {
        using Source = epochengine::project_audio::CueSemantic;
        switch (semantic)
        {
        case Source::jump: return EditorProjectAudioCueSemantic::jump;
        case Source::land: return EditorProjectAudioCueSemantic::land;
        case Source::music: return EditorProjectAudioCueSemantic::music;
        case Source::ambient: return EditorProjectAudioCueSemantic::ambient;
        case Source::user_interface:
            return EditorProjectAudioCueSemantic::user_interface;
        case Source::custom:
        default:
            return EditorProjectAudioCueSemantic::custom;
        }
    }

    [[nodiscard]] static std::optional<epochengine::project_audio::CueSemantic>
        project_audio_semantic(
            EditorProjectAudioCueSemantic semantic) noexcept
    {
        using Target = epochengine::project_audio::CueSemantic;
        switch (semantic)
        {
        case EditorProjectAudioCueSemantic::custom: return Target::custom;
        case EditorProjectAudioCueSemantic::jump: return Target::jump;
        case EditorProjectAudioCueSemantic::land: return Target::land;
        case EditorProjectAudioCueSemantic::music: return Target::music;
        case EditorProjectAudioCueSemantic::ambient: return Target::ambient;
        case EditorProjectAudioCueSemantic::user_interface:
            return Target::user_interface;
        }
        return std::nullopt;
    }

    [[nodiscard]] static bool portable_audio_name_equal(
        std::string_view left,
        std::string_view right) noexcept
    {
        if (left.size() != right.size())
            return false;
        return std::equal(
            left.begin(), left.end(), right.begin(),
            [](char leftCharacter, char rightCharacter) noexcept
            {
                return std::tolower(
                           static_cast<unsigned char>(leftCharacter))
                    == std::tolower(
                           static_cast<unsigned char>(rightCharacter));
            });
    }

    [[nodiscard]] static const epochengine::project_audio::BusSource*
        find_project_audio_bus(
            const epochengine::project_audio::ProfileSource& source,
            epochengine::project_audio::BusId id) noexcept
    {
        const auto found = std::find_if(
            source.buses.begin(), source.buses.end(),
            [id](const auto& bus) noexcept
            {
                return bus.id == id;
            });
        return found == source.buses.end() ? nullptr : &*found;
    }

    [[nodiscard]] static bool load_project_audio_source(
        std::string_view projectId,
        epochengine::project_audio::ProjectAudioProfileStore& store,
        epochengine::project_audio::ProfileSource& source,
        bool& materialized,
        std::string& diagnostic)
    {
        const auto loaded = store.load();
        if (loaded)
        {
            source = loaded.source;
            materialized = true;
            return true;
        }
        if (loaded.code == epochengine::project_audio::StoreCode::not_found)
        {
            source = epochengine::project_audio::make_default_2d_profile();
            materialized = false;
            diagnostic = "Default project audio is ready to materialize.";
            return static_cast<bool>(source.profile_id);
        }
        diagnostic = std::string{"Project audio source "}
            + std::string{
                epochengine::project_audio::store_code_name(loaded.code)}
            + " for " + std::string{projectId} + ".";
        return false;
    }

    EditorProjectAudioProfileSummary editor_project_audio_profile_summary(
        std::string_view project_id)
    {
        EditorProjectAudioProfileSummary result{};
        const auto* profile = editor_find_project_profile(project_id);
        if (!profile || profile->audio_profile_path.empty())
        {
            result.diagnostic =
                "Project does not declare a runtime audio profile.";
            return result;
        }

        const fs::path root = resolve_project_root_path(
            fs::path{profile->root_path});
        epochengine::project_audio::ProjectAudioProfileStore store{
            std::string{project_id}, root};
        result.source_path = std::string{profile->audio_profile_path};
        result.artifact_path = store.artifact_path().generic_string();
        if (!store.valid())
        {
            result.diagnostic = "Project audio store is invalid.";
            return result;
        }

        epochengine::project_audio::ProfileSource source{};
        if (!load_project_audio_source(
                project_id,
                store,
                source,
                result.source_materialized,
                result.diagnostic))
        {
            return result;
        }
        const auto validation = epochengine::project_audio::validate_profile(
            source, store.limits());
        if (validation != epochengine::project_audio::ValidationCode::ready)
        {
            result.diagnostic = std::string{"Project audio profile "}
                + std::string{
                    epochengine::project_audio::validation_code_name(
                        validation)}
                + ".";
            return result;
        }

        const auto artifact = store.load_artifact(false);
        if (artifact)
        {
            result.artifact_materialized = true;
            result.artifact_current = artifact.artifact.source_digest
                == epochengine::project_audio::profile_digest(
                    source, store.limits());
        }

        result.sequence = source.sequence;
        result.bus_count = static_cast<std::uint32_t>(source.buses.size());
        result.cue_count = static_cast<std::uint32_t>(source.cues.size());
        result.buses.reserve(source.buses.size());
        result.cues.reserve(source.cues.size());
        for (const auto& bus : source.buses)
        {
            const auto* parent = bus.parent
                ? find_project_audio_bus(source, bus.parent)
                : nullptr;
            result.buses.push_back({
                .stable_id = bus.id.value,
                .parent_id = bus.parent.value,
                .name = bus.name,
                .parent_name = parent ? parent->name : std::string{"Master"},
                .gain = bus.gain,
                .muted = bus.muted});
        }
        for (const auto& cue : source.cues)
        {
            const auto* bus = cue.bus
                ? find_project_audio_bus(source, cue.bus)
                : nullptr;
            std::error_code error{};
            const fs::path sourcePath = root
                / fs::path{cue.logical_audio_path};
            const bool exists = fs::is_regular_file(sourcePath, error)
                && !error;
            std::uint64_t sourceBytes{};
            if (exists)
            {
                const std::uintmax_t bytes = fs::file_size(sourcePath, error);
                if (!error
                    && bytes <= static_cast<std::uintmax_t>(
                        (std::numeric_limits<std::uint64_t>::max)()))
                {
                    sourceBytes = static_cast<std::uint64_t>(bytes);
                    result.source_bytes += sourceBytes;
                }
            }
            if (!exists)
                ++result.missing_source_count;
            result.cues.push_back({
                .stable_id = cue.id.value,
                .semantic = editor_audio_semantic(cue.semantic),
                .semantic_name = std::string{
                    epochengine::project_audio::cue_semantic_name(
                        cue.semantic)},
                .name = cue.name,
                .logical_path = cue.logical_audio_path,
                .bus_id = cue.bus.value,
                .bus_name = bus ? bus->name : std::string{"Master"},
                .gain = cue.gain,
                .looping = cue.looping,
                .autoplay = cue.autoplay,
                .source_exists = exists,
                .source_bytes = sourceBytes});
        }
        result.ready = true;
        if (result.missing_source_count != 0u)
        {
            result.diagnostic = std::to_string(
                result.missing_source_count)
                + " project audio cue source(s) are missing; Run and Build will fail closed.";
        }
        else if (artifact
            && result.artifact_current)
        {
            result.diagnostic = result.source_materialized
                ? "Project audio source, cue files, and Library artifact are ready."
                : "The Library artifact is current; default project audio source is ready to materialize.";
        }
        else if (artifact)
        {
            result.diagnostic =
                "Project audio source is ready; its Library artifact is stale and Build or Run will refresh it.";
        }
        else if (artifact.code
            != epochengine::project_audio::StoreCode::not_found)
        {
            result.diagnostic = std::string{
                "Project audio source is ready; its Library artifact was rejected ("}
                + std::string{
                    epochengine::project_audio::store_code_name(
                        artifact.code)}
                + ") and Build or Run will regenerate it.";
        }
        else
        {
            result.diagnostic = result.source_materialized
                ? "Project audio source and cue files are ready; Build or Run will generate the Library artifact."
                : "Default project audio is ready to materialize.";
        }
        return result;
    }

    template <typename EditOperation>
    [[nodiscard]] static EditorProjectAudioUpdateResult
        edit_project_audio_profile(
            std::string_view projectId,
            EditOperation&& operation,
            std::string unchangedSummary,
            std::string changedSummary)
    {
        const auto* profile = editor_find_project_profile(projectId);
        if (!profile || profile->audio_profile_path.empty())
        {
            return {
                .summary =
                    "Project does not declare a runtime audio profile."};
        }
        const fs::path root = resolve_project_root_path(
            fs::path{profile->root_path});
        epochengine::project_audio::ProjectAudioProfileStore store{
            std::string{projectId}, root};
        if (!store.valid())
            return {.summary = "Project audio store is invalid."};

        epochengine::project_audio::ProfileSource source{};
        bool materialized{};
        std::string diagnostic{};
        if (!load_project_audio_source(
                projectId,
                store,
                source,
                materialized,
                diagnostic))
        {
            return {.summary = std::move(diagnostic)};
        }

        const auto edited = std::forward<EditOperation>(operation)(
            std::move(source));
        if (!edited)
        {
            return {
                .summary = std::string{"Project audio edit "}
                    + std::string{
                        epochengine::project_audio::profile_edit_code_name(
                            edited.code)}
                    + " / "
                    + std::string{
                        epochengine::project_audio::validation_code_name(
                            edited.validation)}};
        }

        const auto compiled = store.compile(edited.source, false);
        if (!compiled)
            return {.summary = compiled.diagnostic};

        if (edited.code
            == epochengine::project_audio::ProfileEditCode::unchanged)
        {
            const auto artifact = store.publish_artifact(compiled);
            if (!artifact)
            {
                return {
                    .summary = std::string{"Project audio artifact publish "}
                        + std::string{
                            epochengine::project_audio::store_code_name(
                                artifact.code)}};
            }
            return {
                .succeeded = true,
                .changed = false,
                .decoded_bytes = compiled.decoded_bytes,
                .summary = std::move(unchangedSummary)};
        }

        const auto saved = store.save(edited.source);
        if (!saved)
        {
            return {
                .summary = std::string{"Project audio source save "}
                    + std::string{
                        epochengine::project_audio::store_code_name(
                            saved.code)}};
        }
        const auto artifact = store.publish_artifact(compiled);
        if (!artifact)
        {
            return {
                .summary = std::string{
                    "Project audio source saved, but Library artifact publish "}
                    + std::string{
                        epochengine::project_audio::store_code_name(
                            artifact.code)}
                    + "; Build or Run will regenerate it."};
        }
        return {
            .succeeded = true,
            .changed = true,
            .decoded_bytes = compiled.decoded_bytes,
            .summary = std::move(changedSummary)};
    }
    EditorProjectAudioUpdateResult editor_reset_project_audio_profile(
        std::string_view project_id)
    {
        const auto* profile = editor_find_project_profile(project_id);
        if (!profile || profile->audio_profile_path.empty())
        {
            return {
                .summary =
                    "Project does not declare a runtime audio profile."};
        }
        const fs::path root = resolve_project_root_path(
            fs::path{profile->root_path});
        epochengine::project_audio::ProjectAudioProfileStore store{
            std::string{project_id}, root};
        if (!store.valid())
            return {.summary = "Project audio store is invalid."};

        std::uint64_t nextSequence{1u};
        const auto loaded = store.load();
        if (loaded)
        {
            if (loaded.source.sequence
                == (std::numeric_limits<std::uint64_t>::max)())
            {
                return {
                    .summary = "Project audio revision is exhausted."};
            }
            nextSequence = loaded.source.sequence + 1u;
        }
        auto source = epochengine::project_audio::make_default_2d_profile();
        source.sequence = nextSequence;

        if (!write_text_file_if_allowed(
                root / "Assets/Audio/default_jump.wav",
                make_project_pcm16_wave(660.0f, 95u, 0.24f, true),
                false)
            || !write_text_file_if_allowed(
                root / "Assets/Audio/default_land.wav",
                make_project_pcm16_wave(165.0f, 75u, 0.20f, true),
                false)
            || !write_text_file_if_allowed(
                root / "Assets/Audio/default_ambient.wav",
                make_project_pcm16_wave(110.0f, 1'000u, 0.08f, false),
                false))
        {
            return {
                .summary =
                    "Default project audio clips could not be materialized."};
        }
        const auto compiled = store.compile(source, false);
        if (!compiled)
        {
            return {
                .summary = std::string{
                    "Default project audio decode validation failed: "}
                    + compiled.diagnostic};
        }
        const auto saved = store.save(source);
        if (!saved)
        {
            return {
                .summary = std::string{"Default project audio save "}
                    + std::string{
                        epochengine::project_audio::store_code_name(
                            saved.code)}};
        }
        const auto artifact = store.publish_artifact(compiled);
        if (!artifact)
        {
            return {
                .summary = std::string{
                    "Default audio source saved, but Library artifact publish "}
                    + std::string{
                        epochengine::project_audio::store_code_name(
                            artifact.code)}};
        }
        return {
            .succeeded = true,
            .changed = saved.code
                == epochengine::project_audio::StoreCode::ready,
            .decoded_bytes = compiled.decoded_bytes,
            .summary =
                "Default project audio source, decoded clips, and Library artifact restored."};
    }
    EditorProjectAudioUpdateResult editor_import_project_audio_cue(
        std::string_view project_id,
        std::string_view external_source_path)
    {
        const auto* profile = editor_find_project_profile(project_id);
        if (!profile || profile->audio_profile_path.empty())
        {
            return {
                .summary =
                    "Project does not declare a runtime audio profile."};
        }
        const fs::path root = resolve_project_root_path(
            fs::path{profile->root_path});
        epochengine::project_audio::ProjectAudioProfileStore store{
            std::string{project_id}, root};
        if (!store.valid())
            return {.summary = "Project audio store is invalid."};

        const auto imported = store.import_source(
            fs::path{external_source_path});
        if (!imported)
        {
            return {
                .summary = imported.diagnostic.empty()
                    ? std::string{"Project audio import "}
                        + std::string{
                            epochengine::project_audio::
                                source_import_code_name(imported.code)}
                    : imported.diagnostic};
        }

        epochengine::project_audio::ProfileSource source{};
        bool materialized{};
        std::string diagnostic{};
        if (!load_project_audio_source(
                project_id,
                store,
                source,
                materialized,
                diagnostic))
        {
            if (imported.created)
            {
                std::error_code ignored{};
                fs::remove(imported.destination_path, ignored);
            }
            return {.summary = std::move(diagnostic)};
        }
        const auto existing = std::find_if(
            source.cues.begin(), source.cues.end(),
            [&imported](const auto& cue)
            {
                return cue.logical_audio_path == imported.logical_path;
            });
        if (existing != source.cues.end())
        {
            return {
                .succeeded = true,
                .changed = false,
                .stable_id = existing->id.value,
                .logical_path = imported.logical_path,
                .decoded_bytes = imported.decoded_bytes,
                .frame_count = imported.frame_count,
                .sample_rate = imported.sample_rate,
                .channel_count = imported.channel_count,
                .summary =
                    "The selected decoded audio source is already assigned to a project cue."};
        }

        std::string cueName = fs::path{external_source_path}.stem().string();
        if (cueName.empty())
            cueName = "Imported Cue";
        const std::string baseName = cueName;
        for (std::uint32_t suffix{2u};; ++suffix)
        {
            const bool duplicate = std::any_of(
                source.cues.begin(), source.cues.end(),
                [&cueName](const auto& cue)
                {
                    return portable_audio_name_equal(
                        cue.name, cueName);
                });
            if (!duplicate)
                break;
            cueName = baseName + " " + std::to_string(suffix);
        }
        epochengine::project_audio::BusId effects{};
        const auto effectsBus = std::find_if(
            source.buses.begin(), source.buses.end(),
            [](const auto& bus)
            {
                return bus.name == "Effects";
            });
        if (effectsBus != source.buses.end())
            effects = effectsBus->id;
        else if (!source.buses.empty())
            effects = source.buses.front().id;

        auto edited = epochengine::project_audio::add_cue(
            std::move(source),
            epochengine::project_audio::CueSource{
                .semantic = epochengine::project_audio::CueSemantic::custom,
                .name = std::move(cueName),
                .logical_audio_path = imported.logical_path,
                .bus = effects,
                .gain = 1.0f});
        if (!edited)
        {
            if (imported.created)
            {
                std::error_code ignored{};
                fs::remove(imported.destination_path, ignored);
            }
            return {
                .summary = std::string{"Imported audio cue edit "}
                    + std::string{
                        epochengine::project_audio::profile_edit_code_name(
                            edited.code)}
                    + " / "
                    + std::string{
                        epochengine::project_audio::validation_code_name(
                            edited.validation)}};
        }
        const auto cue = std::find_if(
            edited.source.cues.begin(), edited.source.cues.end(),
            [&imported](const auto& candidate)
            {
                return candidate.logical_audio_path
                    == imported.logical_path;
            });
        const auto compiled = store.compile(edited.source, false);
        if (!compiled)
        {
            if (imported.created)
            {
                std::error_code ignored{};
                fs::remove(imported.destination_path, ignored);
            }
            return {.summary = compiled.diagnostic};
        }
        const auto saved = store.save(edited.source);
        if (!saved)
        {
            if (imported.created)
            {
                std::error_code ignored{};
                fs::remove(imported.destination_path, ignored);
            }
            return {
                .summary = std::string{"Imported audio profile save "}
                    + std::string{
                        epochengine::project_audio::store_code_name(
                            saved.code)}};
        }
        const auto artifact = store.publish_artifact(compiled);
        if (!artifact)
        {
            return {
                .summary = std::string{
                    "Imported audio source and profile were saved, but Library artifact publish "}
                    + std::string{
                        epochengine::project_audio::store_code_name(
                            artifact.code)}
                    + "; Build or Run will regenerate it."};
        }
        return {
            .succeeded = true,
            .changed = true,
            .stable_id = cue == edited.source.cues.end()
                ? 0u : cue->id.value,
            .logical_path = imported.logical_path,
            .decoded_bytes = imported.decoded_bytes,
            .frame_count = imported.frame_count,
            .sample_rate = imported.sample_rate,
            .channel_count = imported.channel_count,
            .summary =
                "Decoded WAV imported, assigned to a cue, and published to the Library artifact."};
    }
    EditorProjectAudioUpdateResult editor_set_project_audio_bus_mix(
        std::string_view project_id,
        std::uint64_t bus_id,
        float gain,
        bool muted)
    {
        return edit_project_audio_profile(
            project_id,
            [bus_id, gain, muted](
                epochengine::project_audio::ProfileSource source)
            {
                return epochengine::project_audio::set_bus_mix(
                    std::move(source),
                    epochengine::project_audio::BusId{bus_id},
                    gain,
                    muted);
            },
            "Project audio bus mix is already assigned.",
            "Project audio bus volume and mute state updated.");
    }

    EditorProjectAudioUpdateResult editor_configure_project_audio_cue(
        std::string_view project_id,
        std::uint64_t cue_id,
        EditorProjectAudioCueSemantic semantic,
        std::uint64_t bus_id,
        float gain,
        bool looping,
        bool autoplay)
    {
        const auto targetSemantic = project_audio_semantic(semantic);
        if (!targetSemantic)
            return {.summary = "Project audio cue semantic is invalid."};
        return edit_project_audio_profile(
            project_id,
            [cue_id, targetSemantic, bus_id, gain, looping, autoplay](
                epochengine::project_audio::ProfileSource source)
            {
                return epochengine::project_audio::configure_cue(
                    std::move(source),
                    epochengine::project_audio::CueId{cue_id},
                    *targetSemantic,
                    epochengine::project_audio::BusId{bus_id},
                    gain,
                    looping,
                    autoplay);
            },
            "Project audio cue settings are already assigned.",
            "Project audio cue routing and playback settings updated.");
    }

    EditorProjectAudioUpdateResult editor_remove_project_audio_cue(
        std::string_view project_id,
        std::uint64_t cue_id)
    {
        return edit_project_audio_profile(
            project_id,
            [cue_id](epochengine::project_audio::ProfileSource source)
            {
                return epochengine::project_audio::remove_cue(
                    std::move(source),
                    epochengine::project_audio::CueId{cue_id});
            },
            "Project audio cue was already removed.",
            "Project audio cue removed; its source asset remains available.");
    }

    EditorProjectAudioUpdateResult editor_validate_project_audio_profile(
        std::string_view project_id)
    {
        const auto* profile = editor_find_project_profile(project_id);
        if (!profile || profile->audio_profile_path.empty())
        {
            return {
                .summary =
                    "Project does not declare a runtime audio profile."};
        }
        const fs::path root = resolve_project_root_path(
            fs::path{profile->root_path});
        epochengine::project_audio::ProjectAudioProfileStore store{
            std::string{project_id}, root};
        if (!store.valid())
            return {.summary = "Project audio store is invalid."};
        const auto loaded = store.load();
        if (!loaded)
        {
            return {
                .summary = std::string{"Project audio source "}
                    + std::string{
                        epochengine::project_audio::store_code_name(
                            loaded.code)}};
        }
        const auto compiled = store.compile(loaded.source, false);
        if (!compiled)
            return {.summary = compiled.diagnostic};
        const auto artifact = store.publish_artifact(compiled);
        if (!artifact)
        {
            return {
                .summary = std::string{"Project audio artifact publish "}
                    + std::string{
                        epochengine::project_audio::store_code_name(
                            artifact.code)}};
        }
        std::uint64_t frames{};
        for (const auto& cue : compiled.request.cues)
            frames += cue.clip.frame_count;
        return {
            .succeeded = true,
            .changed = artifact.code
                == epochengine::project_audio::StoreCode::ready,
            .decoded_bytes = compiled.decoded_bytes,
            .frame_count = frames,
            .summary =
                "All project audio cues decoded and the verified Library artifact is current."};
    }
    EditorProjectBuildResult editor_build_project(std::string_view project_root)
    {
        return editor_build_project(project_root, {});
    }

    EditorProjectBuildResult editor_build_project(
        std::string_view project_root,
        std::stop_token cancellation)
    {
        if (project_root.empty())
            return {false, "No active project root selected."};
        if (cancellation.stop_requested())
            return {false, "Project build cancelled before preflight.", {}, {}, true};

        const fs::path root = resolve_project_root_path(fs::path{project_root});
        if (root.empty())
            return {false, "Project build refused an unavailable or non-private candidate project root."};
        const fs::path scriptPath =
#if defined(_WIN32)
            generated_project_windows_build_script_path(root);
#else
            generated_project_linux_build_script_path(root);
#endif
        const fs::path logPath = generated_project_build_log_path(root);
        const fs::path outputPath = generated_project_output_path(root);

        static std::mutex generatedProjectBuildMutex;
        static std::uint64_t generatedProjectBuildSequence{};
        std::unique_lock buildLock{generatedProjectBuildMutex, std::try_to_lock};
        if (!buildLock.owns_lock())
        {
            return {
                false,
                "Another generated project build is already running. Wait for that build to finish before pressing Run again.",
                outputPath.generic_string(),
                logPath.generic_string()};
        }

        std::error_code ec;
        if (!fs::exists(scriptPath, ec) || ec)
        {
            if (const auto* profile = find_project_profile_by_root(project_root))
            {
                const auto ensured = editor_ensure_project_shell(profile->id);
                if (!ensured.succeeded)
                {
                    return {
                        false,
                        ensured.summary,
                        outputPath.generic_string(),
                        logPath.generic_string()};
                }
            }
            else
            {
                return {
                    false,
                    "Missing generated build script: " + scriptPath.generic_string()
                        + ". Create a generated project shell first.",
                    outputPath.generic_string(),
                    logPath.generic_string()};
            }
        }

        if (!repair_generated_windows_child_project_build_files(root))
        {
            return {
                false,
                "Generated project build files could not be repaired before build.",
                outputPath.generic_string(),
                logPath.generic_string()};
        }
        if (cancellation.stop_requested())
        {
            return {
                false,
                "Project build cancelled after preflight.",
                outputPath.generic_string(),
                logPath.generic_string(),
                true};
        }

        platform::child_process::LaunchRequest request{};
#if defined(_WIN32)
        char* systemRootValue{};
        std::size_t systemRootSize{};
        if (_dupenv_s(
                &systemRootValue,
                &systemRootSize,
                "SystemRoot") != 0
            || systemRootValue == nullptr
            || systemRootSize <= 1u)
        {
            std::free(systemRootValue);
            return {
                false,
                "Project build could not resolve the Windows PowerShell host.",
                outputPath.generic_string(),
                logPath.generic_string()};
        }
        const fs::path systemRoot{systemRootValue};
        std::free(systemRootValue);
        request.executable = systemRoot
            / "System32" / "WindowsPowerShell" / "v1.0" / "powershell.exe";
        request.arguments = {
            "-NoProfile",
            "-NonInteractive",
            "-ExecutionPolicy",
            "Bypass",
            "-File",
            fs::absolute(scriptPath).string(),
            "-Configuration",
            "Debug",
            "-Platform",
            "x64"};
#else
        request.executable = "/bin/bash";
        request.arguments = {fs::absolute(scriptPath).string()};
#endif
        if (generatedProjectBuildSequence
            == (std::numeric_limits<std::uint64_t>::max)())
        {
            return {
                false,
                "Project build sequence is exhausted.",
                outputPath.generic_string(),
                logPath.generic_string()};
        }
        ++generatedProjectBuildSequence;
        request.working_directory = root;
        request.correlation_key = "epoch.project.build:"
            + root.filename().string() + ":"
            + std::to_string(generatedProjectBuildSequence);
        request.exclusive_group = "epoch.project.build";
        request.display_name = "Epoch Project Build";
        request.window_mode = platform::child_process::WindowMode::hidden;

        const platform::child_process::LaunchResult launched =
            platform::child_process::launch_or_focus(request);
        if (launched.code != platform::child_process::LaunchCode::started)
        {
            return {
                false,
                "Project build process could not start: " + launched.message,
                outputPath.generic_string(),
                logPath.generic_string()};
        }

        const platform::child_process::WaitResult waited =
            platform::child_process::wait(launched.handle, cancellation);
        (void)platform::child_process::release(launched.handle);
        if (waited.code == platform::child_process::WaitCode::cancelled)
        {
            return {
                false,
                "Project build cancelled; the owned process tree was stopped.",
                outputPath.generic_string(),
                logPath.generic_string(),
                true};
        }
        if (waited.code != platform::child_process::WaitCode::exited
            || !waited.process || !waited.process->exit_code_valid)
        {
            return {
                false,
                "Project build process failed: " + waited.message,
                outputPath.generic_string(),
                logPath.generic_string()};
        }

        const fs::path resolvedOutputPath =
            generated_project_existing_output_path(root);
        const bool succeeded = waited.process->exit_code == 0
            && fs::exists(resolvedOutputPath, ec)
            && !ec;
        return {
            succeeded,
            succeeded
                ? "Built generated child project to "
                    + resolvedOutputPath.generic_string() + "."
                : "Project build failed with exit code "
                    + std::to_string(waited.process->exit_code)
                    + ". See " + logPath.generic_string() + " for details.",
            (succeeded ? resolvedOutputPath : outputPath).generic_string(),
            logPath.generic_string()};
    }
    EditorScriptBuildResult editor_build_script(std::string_view script_name)
    {
        return editor_build_script(script_name, {}, {});
    }

    std::string editor_resolve_script_source_path(std::string_view script_name, std::string_view project_root)
    {
        std::error_code ec;
        const std::string projectPath = project_script_source_path(script_name, project_root);
        if (!projectPath.empty() && fs::exists(fs::path(projectPath), ec) && !ec)
            return projectPath;

        if (const auto* profile = find_script_profile(script_name))
        {
            const fs::path profilePath{ profile->source_path };
            if (profilePath.is_absolute())
                return profilePath.generic_string();

            return (resolve_epoch_repo_root(profilePath) / profilePath).lexically_normal().generic_string();
        }

        return projectPath.empty()
            ? (resolve_epoch_repo_root({}) / "Engine" / "src" / "scripts" / (std::string(script_name) + ".ascript.cpp")).generic_string()
            : projectPath;
    }

    EditorScriptBuildResult editor_build_script(
        std::string_view script_name,
        std::string_view project_root)
    {
        return editor_build_script(script_name, project_root, {});
    }

    EditorScriptBuildResult editor_build_script(
        std::string_view script_name,
        std::string_view project_root,
        std::stop_token cancellation)
    {
        if (cancellation.stop_requested())
            return {false, "Script build cancelled before preflight.", true};
        const auto* profile = find_script_profile(script_name);
        if (!profile && project_root.empty())
            return { false, "Unknown script profile." };

        const fs::path sourcePath{ editor_resolve_script_source_path(script_name, project_root) };
        std::error_code ec;
        if (!fs::exists(sourcePath, ec) || ec)
        {
            return {
                false,
                "Missing script source: " + sourcePath.generic_string()
                + ". "
                + std::string(profile ? profile->diagnostic_hint : "Project-local script was not found.")
            };
        }

        const auto sourceSize = fs::file_size(sourcePath, ec);
        if (ec)
        {
            return {
                false,
                "Script source exists but its size could not be read: "
                    + sourcePath.generic_string() + "."
            };
        }

        const fs::path outputPath = compiler::default_script_output_path(sourcePath);
        fs::create_directories(outputPath.parent_path(), ec);
        if (ec)
        {
            return {
                false,
                "Could not prepare the script output directory: "
                    + outputPath.parent_path().generic_string() + "."
            };
        }

        const compiler::CompileResult compiled =
            compiler::compile_script_to_dll_result(
                sourcePath, outputPath, cancellation);
        ec.clear();
        const bool outputExists = fs::is_regular_file(outputPath, ec) && !ec;
        const auto outputSize =
            outputExists ? fs::file_size(outputPath, ec) : 0u;
        if (!compiled || !outputExists || ec)
        {
            const bool cancelled =
                cancellation.stop_requested()
                || compiled.status == compiler::CompileStatus::cancelled;
            std::string diagnostic =
                "C++23 script compilation "
                + std::string{compiler::compile_status_name(compiled.status)}
                + " ["
                + std::string{compiler::compile_refusal_name(compiled.refusal)}
                + "] for " + sourcePath.generic_string() + ".";
            if (!compiled.message.empty())
                diagnostic += " " + compiled.message;
            if (!outputExists && compiled)
                diagnostic += " Verified output artifact is missing.";
            return {false, std::move(diagnostic), cancelled};
        }

        return {
            true,
            "Compiled " + sourcePath.generic_string()
                + " (" + std::to_string(sourceSize) + " bytes) to "
                + outputPath.generic_string()
                + " (" + std::to_string(outputSize) + " bytes)."
        };
    }
}
