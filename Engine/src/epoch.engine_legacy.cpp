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
// epoch.engine_legacy.cpp (module implementation unit; module names remain compatibility-stable)
 //
 // FIXES APPLIED:
 //  - No direct access to core::Context private members (ctx->hwnd).
 //    We only query windows via MultiContextManager APIs.
 //  - Removed non-constant switch case labels for ContextType::Unknown/Noop
 //    because your ContextType in your current modules is not an enum with those
 //    exact enumerators (or they are not visible here). Default handles it.
 //
#include "core.format_text.hpp"
//#include "pch.h"

#include "../include/engine.config.hpp"
#include "../src/epoch.api_types.hpp"
#include "../include/epoch.engine.hpp"
#include "../include/epoch.runtime_legacy.hpp"

#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  if defined(_DEBUG)
#    include <crtdbg.h>
#  endif
#else

        void append_launcher_cancel_breadcrumb_noexcept(const std::string_view) noexcept
        {
        }

#endif

// -----------------------------
// Standard library imports
// -----------------------------
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <future>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <shared_mutex>
#include <source_location>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

// -----------------------------
// Engine/module imports
// -----------------------------
import platform.engine;
import platform.child_process;
//import engine.config;

import epoch.facade;

import epoch.cli;
import epoch.version;
import engine.updater;
import launcher.update;
import input.engine;
import input.controller;
import ecs.legacy_components;

import context.multiplexer;
import context.type;
import context.window;
import core.context;
import core.env;
import core.logger;
import core.path;
import core.time;
import core.timer;
import ecs.world;

import audio.manager;
import audio.device;
import audio.mixer;
import audio.playback_runtime;
import asset.audio_import;
#if EPOCH_ENABLE_PHYSICAL_AUDIO
import audio.device_sdl;
#endif
import authoring.document;
import authoring.morphology;
#if EPOCH_ENABLE_AUTHORING_PLATFORM && EPOCH_ENABLE_TEXTURE_EDITOR
import authoring.texture;
#endif
import asset.texture_import;
import capability.profile;
import editor.project_textures;
import editor.workspace_layout;
import editor.workspace_commands;
import editor.code_workspace;
import editor.hierarchy_adapter;
#if EPOCH_ENABLE_AUTHORING_PLATFORM && EPOCH_ENABLE_TILEMAP_EDITOR
import editor.tilemap_workspace;
#endif
import forest.factory;
import project.forest_library;
import package.registry;
import extension.catalog;
#if EPOCH_ENABLE_NATIVE_EXTENSIONS
import extension.plugin;
#endif
import physics.manager;
import render.camera;
import render.lighting;
import render.portal;
import render.ray;
import scene.document;
import scene.interaction;
import scene.persistence;
import scene.runtime;
import scene.tier0;
import terrain.foundation;
import voxel.field;
import voxel.storage;
import water.system;
import saveload.system;
import scene.snapshot;
import scene.serializer;
import temporal.request;
import timeline.system;

import gui.engine;
import gui.menu;
import editor.core;
import ai.engine;
import ai.development_proposal_codec;
import ai.project_profile;
import ai.curated_context_bundle;
import ai.iteration_loop;
import ai.iteration_session;
import ai.iteration_campaign;
import ai.mcp_campaign;
import ai.mcp_child_host;
import ai.self_iteration_orchestrator;
import ai.mcp_orchestrator_bridge;
import ai.source_patch_bundle;
import ai.iteration_patch_adapter;
import ai.iteration_validation_adapter;
import epoch.build_validation;
import editor.ai_development_controller;
import editor.ai_development_panel;
import editor.systems_workspace;
import authoring.gui_document;
import authoring.gui_compiler;
import authoring.task_graph;
import render.device;
import render.device_null;
import render.device_opengl_family;
#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)
import opengl.canvas2d;
import opengl.textures;
#endif
import render.canvas2d;
import render.canvas2d_scene;
import render.canvas2d_cpu;
import render.canvas2d_limits;
import render.canvas2d_presentation;
import render.texture_residency;
import canvas2d.scene_contracts;
import project.contracts;
import project.tilemap_runtime;
import project.gui_library;
import project.gui_runtime;
import project.gui_epochgui;
import asset.tilemap_artifact;
import render.canvas2d_tilemap;
import project.input_profile;
import project.input_controller;
import project.actor2d_runtime;
import project.gameplay2d_runtime;
import perf.tier;
import platform.budgets;
import project.sprite_animation;
import project.audio_profile;
import render.texture_artifact;
#if defined(EPOCH_USING_SDL) && (EPOCH_USING_SDL == 1)
import render.device_sdl;
#endif
#if defined(EPOCH_USING_SFML) && (EPOCH_USING_SFML == 1)
import render.device_sfml;
#endif
#if defined(EPOCH_USING_RAYLIB) && (EPOCH_USING_RAYLIB == 1)
import render.device_raylib;
#endif
import render.arcade;
import render.graph;
import render.preview_grid;
import atlas.texture;
import texture.core;

import scene.core;

import game.snake_like;
import game.tetris_like;
import game.pacman_like;
import game.frogger_like;
import game.sokoban_like;
import game.match3_like;

import game.sliding_puzzle_like;
import game.minesweeper_like;
import game.a2048_like;

import simulation.sand;
import simulation.cellular;

#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)
import opengl.context;
#endif
#if defined(EPOCH_USING_SOFTWARE_RENDERER) && (EPOCH_USING_SOFTWARE_RENDERER == 1)
import software.context;
#endif
#if defined(EPOCH_USING_SDL) && (EPOCH_USING_SDL == 1)
import sdl.context;
import sdl.state;
#endif
#if defined(EPOCH_USING_SFML) && (EPOCH_USING_SFML == 1)
import sfml.context;
#endif
#if defined(EPOCH_USING_RAYLIB) && (EPOCH_USING_RAYLIB == 1)
import raylib.context;
import raylib.state;
import raylib.textures;
#endif

namespace input = epochengine::input;
namespace menu = epochengine::menu;
namespace gui = epochengine::gui;

namespace epochengine::core
{
    void RunEngine();
    void StartEngine();
    void RunEditorInterface();

    void clear_before_ui_frame(const std::shared_ptr<Context>& frameCtx)
    {
        if (!frameCtx)
            return;

        // OpenGL owns its clear in opengl_process before scene preview + GUI queue drain.
        // Enqueuing a second clear here can run after the scene pass and cause flicker.
        if (frameCtx->type == core::ContextType::OpenGL)
            return;

        frameCtx->clear_safe();
    }

    struct LegacyLaunchConfig
    {
        int raylib_count = 1;
        int sdl_count = 1;
        int sfml_count = 1;
        int vulkan_count = 1;
        int opengl_count = 1;
        int directx_count = 0;
        int software_count = 0;
        bool parented = (EPOCH_SINGLE_PARENT == 1);
    };

    [[nodiscard]] inline LegacyLaunchConfig resolve_legacy_launch_config()
    {
        LegacyLaunchConfig cfg{};
        cfg.raylib_count = (std::max)(0, cli::raylib_window_count);
        cfg.sdl_count = (std::max)(0, cli::sdl_window_count);
        cfg.sfml_count = (std::max)(0, cli::sfml_window_count);
        cfg.vulkan_count = (std::max)(0, cli::vulkan_window_count);
        cfg.opengl_count = (std::max)(0, cli::opengl_window_count);
        cfg.directx_count = (std::max)(0, cli::directx_window_count);
        cfg.software_count = (std::max)(0, cli::software_window_count);
#if defined(EPOCH_SINGLE_PARENT) && (EPOCH_SINGLE_PARENT == 1)
        cfg.parented = cli::parented_mode;
#else
        cfg.parented = false;
#endif

#if defined(__linux__)
        cfg.raylib_count = 0;
        cfg.sdl_count = 0;
        cfg.sfml_count = 0;
        cfg.vulkan_count = 0;
        cfg.opengl_count = 0;
        cfg.directx_count = 0;
        cfg.software_count = 0;
        cfg.parented = false;
#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)
        cfg.opengl_count = 1;
#elif defined(EPOCH_USING_SOFTWARE_RENDERER) && (EPOCH_USING_SOFTWARE_RENDERER == 1)
        cfg.software_count = 1;
#endif
#endif

        const bool defaultAutoBackendGrid =
            cfg.raylib_count == 1
            && cfg.sdl_count == 1
            && cfg.sfml_count == 1
            && cfg.vulkan_count == 1
            && cfg.opengl_count == 1
            && cfg.directx_count == 1
            && cfg.software_count == 0;

        if (!cli::backend_selection_explicit && defaultAutoBackendGrid)
        {
            cfg.raylib_count = 0;
            cfg.sdl_count = 0;
            cfg.sfml_count = 0;
            cfg.vulkan_count = 0;
            cfg.opengl_count = 0;
            cfg.directx_count = 0;
            cfg.software_count = 0;
#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)
            cfg.opengl_count = 1;
#elif defined(EPOCH_USING_SOFTWARE_RENDERER) && (EPOCH_USING_SOFTWARE_RENDERER == 1)
            cfg.software_count = 1;
#endif
        }

        const int total_requested =
            cfg.raylib_count +
            cfg.sdl_count +
            cfg.sfml_count +
            cfg.vulkan_count +
            cfg.opengl_count +
            cfg.directx_count +
            cfg.software_count;

        if (total_requested > 0)
            return cfg;

#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)
        cfg.opengl_count = 1;
#elif defined(EPOCH_USING_VULKAN) && (EPOCH_USING_VULKAN == 1)
        cfg.vulkan_count = 1;
#elif defined(EPOCH_USING_SDL) && (EPOCH_USING_SDL == 1)
        cfg.sdl_count = 1;
#elif defined(EPOCH_USING_RAYLIB) && (EPOCH_USING_RAYLIB == 1)
        cfg.raylib_count = 1;
#elif defined(EPOCH_USING_SFML) && (EPOCH_USING_SFML == 1)
        cfg.sfml_count = 1;
#else
        cfg.software_count = 1;
#endif
        return cfg;
    }

    [[nodiscard]] inline std::uint64_t smoke_frame_budget() noexcept
    {
        if (!cli::smoke_requested)
            return (std::numeric_limits<std::uint64_t>::max)();

        return cli::capture_requested ? 900u : 300u;
    }

    [[nodiscard]] inline auto smoke_shutdown_delay() noexcept
    {
        return cli::capture_requested
            ? std::chrono::milliseconds(650)
            : std::chrono::milliseconds(100);
    }

    [[nodiscard]] inline int read_post_update_startup_delay_ms() noexcept
    {
        constexpr int kMaxDelayMs = 15000;

#if defined(_WIN32)
        char* raw = nullptr;
        std::size_t raw_size = 0;
        if (_dupenv_s(&raw, &raw_size, "EPOCH_POST_UPDATE_STARTUP_DELAY_MS") != 0 || raw == nullptr)
            return 0;

        const long parsed = std::strtol(raw, nullptr, 10);
        std::free(raw);
#else
        const char* const raw = std::getenv("EPOCH_POST_UPDATE_STARTUP_DELAY_MS");
        if (raw == nullptr || *raw == '\0')
            return 0;

        const long parsed = std::strtol(raw, nullptr, 10);
#endif

        if (parsed <= 0)
            return 0;

        return (std::min)(static_cast<int>(parsed), kMaxDelayMs);
    }

    [[nodiscard]] inline std::string read_environment_string(const char* name)
    {
        if (!name || *name == '\0')
            return {};

#if defined(_WIN32)
        char* raw = nullptr;
        std::size_t raw_size = 0;
        if (_dupenv_s(&raw, &raw_size, name) != 0 || raw == nullptr)
            return {};

        std::string value{ raw };
        std::free(raw);
        return value;
#else
        const char* const raw = std::getenv(name);
        return raw ? std::string{ raw } : std::string{};
#endif
    }

    inline void append_editor_project_self_test_note(
        std::string_view project_root,
        std::string_view title,
        std::string_view summary,
        std::string_view evidence_path,
        std::string_view model_name,
        std::string_view manifest_path,
        std::string_view scene_path,
        std::string_view script_path,
        std::string_view build_log_path,
        std::string_view output_path)
    {
        if (project_root.empty())
            return;

        const std::filesystem::path notes_path =
            std::filesystem::path{ std::string{ project_root } } / "PROJECT_NOTES.md";

        std::error_code ec;
        std::filesystem::create_directories(notes_path.parent_path(), ec);
        const bool had_notes = std::filesystem::exists(notes_path, ec)
            && std::filesystem::file_size(notes_path, ec) > 0u;

        std::ofstream out(notes_path, std::ios::app | std::ios::binary);
        if (!out)
            return;

        if (!had_notes)
        {
            out << "# Project Notes\n\n";
            out << "These notes are generated by the editor so project actions, script runs, and AI sandbox passes leave visible operator evidence.\n";
        }

        out << "\n## " << title << "\n\n";
        out << "- Summary: " << summary << "\n";
        out << "- Model: " << (model_name.empty() ? std::string_view{ "(none selected)" } : model_name) << "\n";
        out << "- Evidence: " << (evidence_path.empty() ? std::string_view{ "(missing)" } : evidence_path) << "\n";
        out << "- Manifest: " << (manifest_path.empty() ? std::string_view{ "(missing)" } : manifest_path) << "\n";
        out << "- Scene: " << (scene_path.empty() ? std::string_view{ "(missing)" } : scene_path) << "\n";
        out << "- Active script: " << (script_path.empty() ? std::string_view{ "(missing)" } : script_path) << "\n";
        out << "- Build log: " << (build_log_path.empty() ? std::string_view{ "(missing)" } : build_log_path) << "\n";
        out << "- Output: " << (output_path.empty() ? std::string_view{ "(missing)" } : output_path) << "\n";
        out << "- Gate: human review required before source changes or reviewed eval-fixture admission.\n";
    }

    struct GeneratedProjectSelfTestResult
    {
        bool attempted = false;
        bool succeeded = false;
        std::string log_path{};
        std::string summary = "child self-test skipped";
    };

    [[nodiscard]] inline GeneratedProjectSelfTestResult run_generated_project_self_test(
        std::string_view executable_path,
        std::string_view project_root)
    {
        GeneratedProjectSelfTestResult result{};
        if (executable_path.empty() || project_root.empty())
            return result;

        const std::filesystem::path executable{ std::string{ executable_path } };
        const std::filesystem::path log_dir =
            std::filesystem::path{ std::string{ project_root } } / "logs";
        const std::filesystem::path log_path =
            log_dir / "project-self-test.log";
        result.log_path = log_path.generic_string();

        std::error_code ec;
        std::filesystem::create_directories(log_dir, ec);
        if (ec)
        {
            result.summary = "failed to create child self-test log directory";
            return result;
        }

        if (!std::filesystem::exists(executable, ec) || ec)
        {
            result.summary = "child self-test executable missing";
            return result;
        }

        result.attempted = true;
        constexpr std::uint32_t requiredRuns = 2u;
        constexpr std::uint64_t childTimeoutNs = 30'000'000'000ull;
        constexpr std::size_t maximumLogBytes = 2u * 1024u * 1024u;
        std::string evidence{};
        evidence.reserve(64u * 1024u);
        for (std::uint32_t run = 0u; run < requiredRuns; ++run)
        {
            const std::filesystem::path runLogPath = log_dir
                / ("project-self-test.run-" + std::to_string(run + 1u)
                    + ".log");
            platform::child_process::LaunchRequest request{};
            request.executable = executable;
            request.working_directory = executable.parent_path();
            request.merged_output_path = runLogPath;
            request.arguments = {"--project-self-test"};
            request.correlation_key = "generated-project-self-test:"
                + std::to_string(run);
            request.exclusive_group = "generated-project-self-test";
            request.display_name = "Generated Project Self-Test";
            request.window_mode =
                platform::child_process::WindowMode::hidden;
            request.append_output = false;

            const auto launched =
                platform::child_process::launch_or_focus(request);
            if (launched.code
                    != platform::child_process::LaunchCode::started
                || !launched.handle.valid())
            {
                if (launched.handle.valid())
                    (void)platform::child_process::release(launched.handle);
                result.summary =
                    "failed to launch supervised child self-test run "
                    + std::to_string(run + 1u) + ": " + launched.message;
                return result;
            }

            const auto waited = platform::child_process::wait(
                launched.handle, {}, childTimeoutNs);
            bool released =
                platform::child_process::release(launched.handle);
            if (!released)
            {
                (void)platform::child_process::stop(
                    launched.handle,
                    platform::child_process::StopMode::force);
                (void)platform::child_process::wait(
                    launched.handle, {}, 5'000'000'000ull);
                released =
                    platform::child_process::release(launched.handle);
            }
            if (waited.code != platform::child_process::WaitCode::exited
                || !waited.process || !waited.process->exit_code_valid
                || waited.process->exit_code != 0 || !released)
            {
                result.summary = "supervised child self-test run "
                    + std::to_string(run + 1u) + " failed: "
                    + waited.message;
                if (waited.process && waited.process->exit_code_valid)
                {
                    result.summary += " exit="
                        + std::to_string(waited.process->exit_code);
                }
                if (!released)
                    result.summary += " handle-release-failed";
                return result;
            }

            std::ifstream runLog{runLogPath, std::ios::binary};
            if (!runLog)
            {
                result.summary = "supervised child self-test run "
                    + std::to_string(run + 1u)
                    + " log could not be opened";
                return result;
            }
            runLog.seekg(0, std::ios::end);
            const std::streamoff runLength = runLog.tellg();
            if (runLength < 0
                || static_cast<std::uint64_t>(runLength)
                    > maximumLogBytes - evidence.size())
            {
                result.summary = "supervised child self-test run "
                    + std::to_string(run + 1u)
                    + " log exceeded the aggregate bound";
                return result;
            }
            const std::size_t offset = evidence.size();
            evidence.resize(offset + static_cast<std::size_t>(runLength));
            runLog.seekg(0, std::ios::beg);
            if (runLength != 0)
            {
                runLog.read(
                    evidence.data() + offset,
                    static_cast<std::streamsize>(runLength));
                if (!runLog)
                {
                    result.summary = "supervised child self-test run "
                        + std::to_string(run + 1u)
                        + " log could not be read exactly";
                    return result;
                }
            }
        }

        {
            std::ofstream log{
                log_path,
                std::ios::binary | std::ios::trunc};
            if (!log)
            {
                result.summary =
                    "supervised child self-test aggregate could not be opened";
                return result;
            }
            if (!evidence.empty())
            {
                log.write(
                    evidence.data(),
                    static_cast<std::streamsize>(evidence.size()));
            }
            log.flush();
            if (!log)
            {
                result.summary =
                    "supervised child self-test aggregate could not be written exactly";
                return result;
            }
        }
        const auto count_marker = [&evidence](std::string_view marker)
        {
            std::uint32_t count{};
            std::size_t cursor{};
            while ((cursor = evidence.find(marker, cursor))
                != std::string::npos)
            {
                ++count;
                cursor += marker.size();
            }
            return count;
        };
        if (count_marker("artifact_acceptance.stage=complete")
                != requiredRuns
            || count_marker("library_regeneration.stage=complete")
                != requiredRuns
            || count_marker("gameplay_acceptance.stage=complete")
                != requiredRuns)
        {
            result.summary =
                "supervised child self-test log lacks repeated acceptance evidence";
            return result;
        }

        result.succeeded = true;
        result.summary = "2/2 supervised child self-test runs passed";
        return result;
    }

    inline void log_editor_self_test_line(std::string_view message)
    {
        logger::get("Engine.Editor.SelfTest").log(
            logger::LogLevel::INFO,
            message,
            std::source_location::current());
    }

    [[nodiscard]] inline bool engine_arcade_screen_graph_contract_ready(epochengine::IRenderDevice& device)
    {
        epochengine::GraphBuilder builder{};
        const std::string_view screenNameStd = epochengine::package_registry::engine_arcade_render_texture_name();
        const epochengine::string_view screenName{ screenNameStd.data(), screenNameStd.size() };
        const epochengine::render_arcade::ArcadeScreenGraphBuild screen = epochengine::render_arcade::add_screen_graph(builder);

        epochengine::CompiledGraph graph = builder.compile(device);

        const bool resourceShape =
            graph.render_texture_assets.size() == 1u
            && graph.textures.size() == 1u
            && graph.samplers.size() == 1u
            && graph.render_targets.size() == 1u
            && graph.buffers.size() == 2u
            && graph.materials.size() == 1u
            && graph.meshes.size() == 1u
            && graph.models.size() == 1u
            && graph.passes.size() == 1u;
        if (!resourceShape)
        {
            graph.destroy(device);
            return false;
        }

        const epochengine::GraphRenderTextureAsset& compiledScreen = graph.render_texture_assets.front();
        const epochengine::GraphTexture& compiledTexture = graph.textures.front();
        const epochengine::GraphSampler& compiledSampler = graph.samplers.front();
        const epochengine::GraphRenderTarget& compiledTarget = graph.render_targets.front();
        const epochengine::GraphMaterial& compiledMaterial = graph.materials.front();
        const epochengine::GraphModel& compiledModel = graph.models.front();
        const epochengine::PassDecl& pass = graph.passes.front();
        const auto same_text = [](epochengine::string_view left, epochengine::string_view right) noexcept
        {
            if (left.size != right.size)
                return false;
            for (std::size_t i = 0; i < left.size; ++i)
            {
                if (left.data[i] != right.data[i])
                    return false;
            }
            return true;
        };

        const bool renderTextureReady =
            same_text(compiledScreen.name.view(), screenName)
            && compiledScreen.desc.width == epochengine::package_registry::engine_arcade_render_texture_width()
            && compiledScreen.desc.height == epochengine::package_registry::engine_arcade_render_texture_height()
            && compiledScreen.desc.usage == epochengine::RenderTextureUsage::arcade_cabinet
            && compiledScreen.backend.color_texture
            && compiledScreen.backend.sampler
            && compiledScreen.backend.render_target
            && compiledScreen.color_texture == screen.screen.color_texture
            && compiledScreen.sampler == screen.screen.sampler
            && compiledScreen.render_target == screen.screen.render_target
            && compiledTexture.backend == compiledScreen.backend.color_texture
            && compiledTexture.sampled_sampler == compiledScreen.backend.sampler
            && compiledTexture.owned_by_render_texture_asset
            && compiledSampler.backend == compiledScreen.backend.sampler
            && compiledSampler.owned_by_render_texture_asset
            && compiledTarget.backend == compiledScreen.backend.render_target
            && compiledTarget.owned_by_render_texture_asset;

        const bool passReady =
            pass.render_target == compiledScreen.backend.render_target
            && pass.binding_set
            && pass.bindings.read_materials.size() == 1u
            && pass.bindings.read_materials.front() == compiledMaterial.backend
            && pass.bindings.read_models.size() == 1u
            && pass.bindings.read_models.front() == compiledModel.backend
            && pass.bindings.read_material_textures.empty()
            && pass.bindings.read_textures.empty()
            && pass.bindings.read_samplers.empty()
            && pass.bindings.write_render_targets.size() == 1u
            && pass.bindings.write_render_targets.front() == compiledScreen.backend.render_target
            && pass.draw_models.size() == 1u
            && pass.draw_models.front().model == screen.model
            && pass.draw_models.front().backend == compiledModel.backend;

        graph.execute(device);
        graph.destroy(device);
        return renderTextureReady && passReady;
    }

    [[nodiscard]] inline bool engine_arcade_screen_graph_contract_ready()
    {
        epochengine::NullRenderDevice nullDevice{};
        return engine_arcade_screen_graph_contract_ready(nullDevice);
    }

    [[nodiscard]] inline bool engine_arcade_cabinet_graph_contract_ready(epochengine::IRenderDevice& device)
    {
        const epochengine::RendererCapabilities caps = device.capabilities();
        if (!epochengine::renderer_supports_sampled_render_targets(caps)
            || !epochengine::renderer_supports_model_resources(caps))
        {
            return false;
        }

        epochengine::GraphBuilder builder{};
        const epochengine::render_arcade::ArcadeCabinetGraphBuild cabinet = epochengine::render_arcade::add_cabinet_graph(builder);

        epochengine::CompiledGraph graph = builder.compile(device);
        const bool resourceShape =
            graph.render_texture_assets.size() == 1u
            && graph.textures.size() == 1u
            && graph.samplers.size() == 1u
            && graph.render_targets.size() == 1u
            && graph.buffers.size() == 6u
            && graph.materials.size() == 3u
            && graph.meshes.size() == 3u
            && graph.models.size() == 2u
            && graph.passes.size() == 2u;
        if (!resourceShape)
        {
            graph.destroy(device);
            return false;
        }

        const epochengine::GraphRenderTextureAsset& compiledScreen = graph.render_texture_assets.front();
        const epochengine::GraphSampler& compiledSampler = graph.samplers.front();
        const epochengine::GraphMaterial& compiledScreenMaterial = graph.materials.front();
        const epochengine::GraphModel& compiledScreenModel = graph.models.front();
        const epochengine::GraphMaterial& compiledMaterial = graph.materials[1u];
        const epochengine::GraphMaterial& compiledBodyMaterial = graph.materials[2u];
        const epochengine::GraphMesh& compiledScreenMesh = graph.meshes[1u];
        const epochengine::GraphMesh& compiledBodyMesh = graph.meshes[2u];
        const epochengine::GraphModel& compiledModel = graph.models.back();
        const epochengine::PassDecl& populatePass = graph.passes.front();
        const epochengine::PassDecl& cabinetPass = graph.passes[1u];

        const bool populateReady =
            populatePass.render_target == compiledScreen.backend.render_target
            && populatePass.binding_set
            && populatePass.bindings.read_materials.size() == 1u
            && populatePass.bindings.read_materials.front() == compiledScreenMaterial.backend
            && populatePass.bindings.read_models.size() == 1u
            && populatePass.bindings.read_models.front() == compiledScreenModel.backend
            && populatePass.bindings.read_material_textures.empty()
            && populatePass.bindings.read_textures.empty()
            && populatePass.bindings.read_samplers.empty()
            && populatePass.bindings.write_render_targets.size() == 1u
            && populatePass.bindings.write_render_targets.front() == compiledScreen.backend.render_target
            && populatePass.draw_models.size() == 1u
            && populatePass.draw_models.front().model == cabinet.screen_scene_model
            && populatePass.draw_models.front().backend == compiledScreenModel.backend;

        const bool materialReady =
            compiledMaterial.backend
            && compiledMaterial.texture_slots.size() == 1u
            && compiledMaterial.texture_slots.front().slot == epochengine::MaterialTextureSlot::render_surface
            && compiledMaterial.texture_slots.front().texture == cabinet.screen.color_texture
            && compiledMaterial.texture_slots.front().sampler == cabinet.screen.sampler;

        const bool bodyMaterialReady = compiledBodyMaterial.backend && compiledBodyMaterial.texture_slots.empty();

        const bool modelReady =
            compiledScreenMesh.backend
            && compiledBodyMesh.backend
            && compiledModel.backend
            && compiledModel.mesh_slots.size() == 2u
            && compiledModel.mesh_slots[0u].mesh == cabinet.body_mesh
            && compiledModel.mesh_slots[0u].material == cabinet.body_material
            && compiledModel.mesh_slots[1u].mesh == cabinet.mesh
            && compiledModel.mesh_slots[1u].material == cabinet.material;

        const bool bindingReady =
            cabinetPass.binding_set
            && compiledScreen.sampler == cabinet.screen.sampler
            && compiledSampler.backend == compiledScreen.backend.sampler
            && compiledSampler.owned_by_render_texture_asset
            && cabinetPass.bindings.read_materials.size() == 2u
            && cabinetPass.bindings.read_materials.front() == compiledMaterial.backend
            && cabinetPass.bindings.read_materials[1u] == compiledBodyMaterial.backend
            && cabinetPass.bindings.read_models.size() == 1u
            && cabinetPass.bindings.read_models.front() == compiledModel.backend
            && cabinetPass.bindings.read_material_textures.size() == 1u
            && cabinetPass.bindings.read_material_textures.front().slot == epochengine::MaterialTextureSlot::render_surface
            && cabinetPass.bindings.read_material_textures.front().texture == compiledScreen.backend.color_texture
            && cabinetPass.bindings.read_material_textures.front().sampler == compiledScreen.backend.sampler
            && cabinetPass.bindings.read_samplers.size() == 1u
            && cabinetPass.bindings.read_samplers.front() == compiledScreen.backend.sampler;

        const bool drawReady =
            cabinetPass.draw_models.size() == 1u
            && cabinetPass.draw_models.front().model == cabinet.model
            && cabinetPass.draw_models.front().backend == compiledModel.backend;

        graph.execute(device);
        graph.destroy(device);
        return populateReady && materialReady && bodyMaterialReady && modelReady && bindingReady && drawReady;
    }

    [[nodiscard]] inline bool render_surface_requires_render_texture_asset_contract_ready()
    {
        epochengine::NullRenderDevice device{};
        epochengine::GraphBuilder builder{};

        epochengine::TextureDesc plainTextureDesc{};
        plainTextureDesc.width = 64u;
        plainTextureDesc.height = 64u;
        plainTextureDesc.format = epochengine::TextureFormat::rgba8_unorm;
        plainTextureDesc.sampled = true;
        plainTextureDesc.debug_name = "plain.render_surface.reject";
        const epochengine::GraphResource plainTexture =
            builder.create_texture("plain.render_surface.reject", plainTextureDesc);

        epochengine::MaterialDesc materialDesc{};
        materialDesc.name = "plain.render_surface.reject.material";
        materialDesc.unlit = true;
        materialDesc.debug_name = "plain.render_surface.reject.material";
        materialDesc.texture_slots.push_back(epochengine::MaterialTextureSlotDesc{
            .slot = epochengine::MaterialTextureSlot::render_surface,
            .name = "screen",
            .expected_format = epochengine::TextureFormat::rgba8_unorm,
            .required = true
        });

        const epochengine::GraphMaterialTextureSlot materialSlots[] = {
            epochengine::GraphMaterialTextureSlot{
                .slot = epochengine::MaterialTextureSlot::render_surface,
                .texture = plainTexture
            }
        };
        const epochengine::GraphResource material = builder.create_material(
            "plain.render_surface.reject.material",
            materialDesc,
            epochengine::array_view<const epochengine::GraphMaterialTextureSlot>{ materialSlots, 1u });

        const epochengine::GraphResource reads[] = { material };
        [[maybe_unused]] const auto pass = builder.add_pass(
            "plain.render_surface.reject.pass",
            epochengine::array_view<const epochengine::GraphResource>{ reads, 1u },
            {},
            [](epochengine::ICommandContext& ctx)
            {
                ctx.debug_marker("plain.render_surface.reject.pass");
            });

        epochengine::CompiledGraph graph = builder.compile(device);
        const bool ready =
            graph.textures.size() == 1u
            && graph.samplers.empty()
            && graph.materials.size() == 1u
            && graph.passes.size() == 1u
            && graph.textures.front().backend
            && !graph.textures.front().sampled_sampler
            && !graph.textures.front().owned_by_render_texture_asset
            && graph.materials.front().backend
            && graph.passes.front().binding_set
            && graph.passes.front().bindings.read_materials.size() == 1u
            && graph.passes.front().bindings.read_materials.front() == graph.materials.front().backend
            && graph.passes.front().bindings.read_material_textures.empty()
            && graph.passes.front().bindings.read_samplers.empty()
            && graph.passes.front().bindings.read_textures.empty();

        graph.execute(device);
        graph.destroy(device);
        return ready;
    }

    [[nodiscard]] inline bool render_surface_rejects_mismatched_sampler_contract_ready()
    {
        epochengine::NullRenderDevice device{};
        epochengine::GraphBuilder builder{};

        epochengine::RenderTextureAssetDesc screenDesc = epochengine::render_arcade::make_screen_render_texture_desc();
        screenDesc.debug_name = "render_surface.mismatched_sampler.screen";
        const epochengine::GraphRenderTextureAsset screen =
            builder.create_render_texture_asset("render_surface.mismatched_sampler.screen", screenDesc);

        epochengine::SamplerDesc mismatchSamplerDesc{};
        mismatchSamplerDesc.debug_name = "render_surface.mismatched_sampler.extra";
        const epochengine::GraphResource mismatchSampler =
            builder.create_sampler("render_surface.mismatched_sampler.extra", mismatchSamplerDesc);

        epochengine::MaterialDesc materialDesc{};
        materialDesc.name = "render_surface.mismatched_sampler.material";
        materialDesc.unlit = true;
        materialDesc.debug_name = "render_surface.mismatched_sampler.material";
        materialDesc.texture_slots.push_back(epochengine::MaterialTextureSlotDesc{
            .slot = epochengine::MaterialTextureSlot::render_surface,
            .name = "screen",
            .expected_format = epochengine::TextureFormat::rgba8_unorm,
            .required = true
        });

        const epochengine::GraphMaterialTextureSlot materialSlots[] = {
            epochengine::GraphMaterialTextureSlot{
                .slot = epochengine::MaterialTextureSlot::render_surface,
                .texture = screen.color_texture,
                .sampler = mismatchSampler
            }
        };
        const epochengine::GraphResource material = builder.create_material(
            "render_surface.mismatched_sampler.material",
            materialDesc,
            epochengine::array_view<const epochengine::GraphMaterialTextureSlot>{ materialSlots, 1u });

        const epochengine::GraphResource reads[] = { material };
        [[maybe_unused]] const auto pass = builder.add_pass(
            "render_surface.mismatched_sampler.pass",
            epochengine::array_view<const epochengine::GraphResource>{ reads, 1u },
            {},
            [](epochengine::ICommandContext& ctx)
            {
                ctx.debug_marker("render_surface.mismatched_sampler.pass");
            });

        epochengine::CompiledGraph graph = builder.compile(device);
        const bool ready =
            graph.render_texture_assets.size() == 1u
            && graph.textures.size() == 1u
            && graph.samplers.size() == 2u
            && graph.materials.size() == 1u
            && graph.passes.size() == 1u
            && graph.render_texture_assets.front().sampler == screen.sampler
            && graph.samplers.front().backend == graph.textures.front().sampled_sampler
            && graph.samplers[1u].backend
            && graph.samplers[1u].backend != graph.textures.front().sampled_sampler
            && graph.materials.front().texture_slots.size() == 1u
            && graph.materials.front().texture_slots.front().texture == screen.color_texture
            && graph.materials.front().texture_slots.front().sampler == mismatchSampler
            && graph.passes.front().binding_set
            && graph.passes.front().bindings.read_materials.size() == 1u
            && graph.passes.front().bindings.read_materials.front() == graph.materials.front().backend
            && graph.passes.front().bindings.read_material_textures.empty()
            && graph.passes.front().bindings.read_samplers.empty()
            && graph.passes.front().bindings.read_textures.empty();

        graph.execute(device);
        graph.destroy(device);
        return ready;
    }

    struct OpenGLFamilyFakeNativeRttState
    {
        int allocate_count = 0;
        int begin_count = 0;
        int end_count = 0;
        int destroy_count = 0;
        epochengine::RendererBackendKind last_backend = epochengine::RendererBackendKind::null;
        epochengine::u32 last_width = 0;
        epochengine::u32 last_height = 0;
        bool saw_depth = false;
        bool saw_sampled = false;
    };

    [[nodiscard]] inline epochengine::OpenGLFamilyNativeRenderTextureAllocation fake_opengl_family_allocate_rtt(
        void* user,
        epochengine::RendererBackendKind backend,
        const epochengine::RenderTextureAssetDesc& desc,
        const epochengine::RenderTextureBackendRequirements& requirements,
        epochengine::u32 slot)
    {
        auto* const state = static_cast<OpenGLFamilyFakeNativeRttState*>(user);
        if (state)
        {
            ++state->allocate_count;
            state->last_backend = backend;
            state->last_width = desc.width;
            state->last_height = desc.height;
            state->saw_depth = requirements.depth_attachment;
            state->saw_sampled = requirements.sampled_color && requirements.sampler;
        }

        return epochengine::OpenGLFamilyNativeRenderTextureAllocation{
            .framebuffer_object = 1000u + slot,
            .color_object = 2000u + slot,
            .depth_object = requirements.depth_attachment ? 3000u + slot : 0u,
            .sampler_object = 4000u + slot,
            .ready = true
        };
    }

    inline void fake_opengl_family_destroy_rtt(
        void* user,
        epochengine::RendererBackendKind backend,
        const epochengine::OpenGLFamilyRenderTextureRecord& record)
    {
        auto* const state = static_cast<OpenGLFamilyFakeNativeRttState*>(user);
        if (state && backend == state->last_backend && record.native_allocation_ready)
            ++state->destroy_count;
    }

    [[nodiscard]] inline bool fake_opengl_family_begin_rtt_pass(
        void* user,
        epochengine::RendererBackendKind backend,
        const epochengine::OpenGLFamilyRenderTextureRecord& record,
        const epochengine::RenderPassDesc&)
    {
        auto* const state = static_cast<OpenGLFamilyFakeNativeRttState*>(user);
        if (!state || backend != state->last_backend || !record.native_allocation_ready)
            return false;

        ++state->begin_count;
        return true;
    }

    inline void fake_opengl_family_end_rtt_pass(
        void* user,
        epochengine::RendererBackendKind backend,
        const epochengine::OpenGLFamilyRenderTextureRecord& record)
    {
        auto* const state = static_cast<OpenGLFamilyFakeNativeRttState*>(user);
        if (state && backend == state->last_backend && record.native_allocation_ready)
            ++state->end_count;
    }

    [[nodiscard]] inline bool opengl_family_arcade_screen_graph_contract_ready()
    {
        const epochengine::RendererBackendKind backends[] = {
            epochengine::RendererBackendKind::opengl,
            epochengine::RendererBackendKind::sdl3,
            epochengine::RendererBackendKind::sfml3,
            epochengine::RendererBackendKind::raylib3
        };

        for (const epochengine::RendererBackendKind backend : backends)
        {
            epochengine::OpenGLFamilyRenderDevice device{ backend };
            if (device.backend() != backend)
                return false;

            const epochengine::RendererCapabilities caps = device.capabilities();
            if (!epochengine::renderer_supports_sampled_render_targets(caps)
                || !engine_arcade_screen_graph_contract_ready(device))
            {
                return false;
            }
        }

        return true;
    }

    [[nodiscard]] inline bool opengl_family_arcade_cabinet_graph_contract_ready()
    {
        const epochengine::RendererBackendKind backends[] = {
            epochengine::RendererBackendKind::opengl,
            epochengine::RendererBackendKind::sdl3,
            epochengine::RendererBackendKind::sfml3,
            epochengine::RendererBackendKind::raylib3
        };

        for (const epochengine::RendererBackendKind backend : backends)
        {
            epochengine::OpenGLFamilyRenderDevice device{ backend };
            if (device.backend() != backend)
                return false;

            if (!engine_arcade_cabinet_graph_contract_ready(device))
            {
                return false;
            }

            const epochengine::OpenGLFamilyCommandContext& context = device.graphics_context();
            const epochengine::CommandResourceBindings& boundResources = context.bound_resources();
            const bool cabinetBindingEvidence =
                context.last_width() == epochengine::package_registry::engine_arcade_render_texture_width()
                && context.last_height() == epochengine::package_registry::engine_arcade_render_texture_height()
                && context.last_render_target()
                && context.bound_binding_set()
                && boundResources.read_materials.size() == 2u
                && boundResources.read_models.size() == 1u
                && boundResources.read_material_textures.size() == 1u
                && boundResources.read_samplers.size() == 1u
                && boundResources.read_material_textures.front().slot == epochengine::MaterialTextureSlot::render_surface
                && boundResources.read_material_textures.front().texture
                && boundResources.read_material_textures.front().sampler
                && boundResources.read_material_textures.front().sampler == boundResources.read_samplers.front()
                && context.last_model()
                && context.last_model() == boundResources.read_models.front()
                && boundResources.read_materials.front()
                && boundResources.read_materials[1u];

            if (!cabinetBindingEvidence)
                return false;
        }

        return true;
    }

    [[nodiscard]] inline bool opengl_family_arcade_fake_native_rtt_contract_ready()
    {
        const epochengine::RendererBackendKind backends[] = {
            epochengine::RendererBackendKind::opengl,
            epochengine::RendererBackendKind::sdl3,
            epochengine::RendererBackendKind::sfml3,
            epochengine::RendererBackendKind::raylib3
        };

        for (const epochengine::RendererBackendKind backend : backends)
        {
            OpenGLFamilyFakeNativeRttState state{};
            epochengine::OpenGLFamilyRenderDevice device{ backend };
            if (device.backend() != backend)
                return false;

            device.set_native_render_texture_hooks(epochengine::OpenGLFamilyNativeRenderTextureHooks{
                .user = &state,
                .allocate = fake_opengl_family_allocate_rtt,
                .destroy = fake_opengl_family_destroy_rtt,
                .begin_pass = fake_opengl_family_begin_rtt_pass,
                .end_pass = fake_opengl_family_end_rtt_pass
            });

            const epochengine::RendererCapabilities caps = device.capabilities();
            if (!epochengine::renderer_supports_sampled_rtt_hooks(caps)
                || !engine_arcade_cabinet_graph_contract_ready(device))
            {
                return false;
            }

            const epochengine::OpenGLFamilyCommandContext& context = device.graphics_context();
            const bool ready =
                state.allocate_count == 1
                && state.begin_count == 1
                && state.end_count == 1
                && state.destroy_count == 1
                && state.last_backend == backend
                && state.last_width == epochengine::package_registry::engine_arcade_render_texture_width()
                && state.last_height == epochengine::package_registry::engine_arcade_render_texture_height()
                && state.saw_depth
                && state.saw_sampled
                && context.last_render_target()
                && context.last_model()
                && !context.native_pass_bound();

            if (!ready)
                return false;
        }

        return true;
    }

    [[nodiscard]] inline bool sampled_render_surface_preview_marker_contract_ready()
    {
        int markerKey{};
        const void* const ctxKey = &markerKey;
        const std::array<epochengine::previewgrid::ObjectMarker, 2> markers{{
            epochengine::previewgrid::ObjectMarker{
                .position = { 0.0f, 0.0f, 0.0f },
                .color = { 0.4f, 0.5f, 0.6f },
                .scale = { 1.0f, 1.0f, 1.0f },
                .primitive = epochengine::previewgrid::ObjectPreviewPrimitive::Cube,
                .editorOnly = true,
                .sampledRenderSurface = false
            },
            epochengine::previewgrid::ObjectMarker{
                .position = {
                    epochengine::render_arcade::kScreenSceneNode.position[0],
                    epochengine::render_arcade::kScreenSceneNode.position[1],
                    epochengine::render_arcade::kScreenSceneNode.position[2] },
                .color = { 0.08f, 0.92f, 0.64f },
                .scale = {
                    epochengine::render_arcade::kScreenSceneNode.scale[0],
                    epochengine::render_arcade::kScreenSceneNode.scale[1],
                    epochengine::render_arcade::kScreenSceneNode.scale[2] },
                .primitive = epochengine::previewgrid::ObjectPreviewPrimitive::EngineArcadeScreen,
                .editorOnly = false,
                .sampledRenderSurface = true
            }
        }};

        epochengine::previewgrid::set_object_markers(ctxKey, std::span<const epochengine::previewgrid::ObjectMarker>{ markers.data(), markers.size() });
        const std::vector<epochengine::previewgrid::ObjectMarker> sampledMarkers =
            epochengine::previewgrid::sampled_render_surface_markers_for(ctxKey);
        epochengine::previewgrid::clear_object_markers(ctxKey);

        if (sampledMarkers.size() != 1u)
            return false;

        const epochengine::previewgrid::ObjectMarker& screen = sampledMarkers.front();
        return screen.primitive == epochengine::previewgrid::ObjectPreviewPrimitive::EngineArcadeScreen
            && screen.sampledRenderSurface
            && std::abs(screen.position.x - epochengine::render_arcade::kScreenSceneNode.position[0]) < 0.001f
            && std::abs(screen.position.y - epochengine::render_arcade::kScreenSceneNode.position[1]) < 0.001f
            && std::abs(screen.position.z - epochengine::render_arcade::kScreenSceneNode.position[2]) < 0.001f
            && std::abs(screen.scale.x - epochengine::render_arcade::kScreenSceneNode.scale[0]) < 0.001f
            && std::abs(screen.scale.y - epochengine::render_arcade::kScreenSceneNode.scale[1]) < 0.001f
            && std::abs(screen.scale.z - epochengine::render_arcade::kScreenSceneNode.scale[2]) < 0.001f
            && epochengine::render_arcade::screen_sample_plane_z(screen.position.z, screen.scale.z)
                > screen.position.z
            && epochengine::render_arcade::screen_sample_plane_faces_viewer(
                screen.position.z,
                screen.scale.z,
                8.0f)
            && !epochengine::render_arcade::screen_sample_plane_faces_viewer(
                screen.position.z,
                screen.scale.z,
                -8.0f);
    }

    [[nodiscard]] inline bool renderer_capability_report_contract_ready()
    {
        const auto opengl = epochengine::renderer_capability_report_for(epochengine::RendererBackendKind::opengl);
        const auto sdl = epochengine::renderer_capability_report_for(epochengine::RendererBackendKind::sdl3);
        const auto sfml = epochengine::renderer_capability_report_for(epochengine::RendererBackendKind::sfml3);
        const auto raylib = epochengine::renderer_capability_report_for(epochengine::RendererBackendKind::raylib3);
        const auto vulkan = epochengine::renderer_capability_report_for(epochengine::RendererBackendKind::vulkan);
        const auto directx = epochengine::renderer_capability_report_for(epochengine::RendererBackendKind::directx);
        const auto software = epochengine::renderer_capability_report_for(epochengine::RendererBackendKind::software);

        const auto present = epochengine::RendererCapabilityStatus::present;
        const auto partial = epochengine::RendererCapabilityStatus::partial;
        const auto missing = epochengine::RendererCapabilityStatus::missing;
        const auto deferred = epochengine::RendererCapabilityStatus::deferred;

        const auto openglReady =
            opengl.descriptor_contract == present
            && opengl.build_graph_proof == present
            && opengl.hook_readiness == present
            && opengl.live_native_allocation == partial
            && opengl.presentation_proof == partial
            && opengl.sampled_render_targets == partial
            && opengl.scene_sampled_surface == partial;

        const auto sdlLivePathReady = [present, partial](const epochengine::RendererCapabilityReport& report) noexcept
        {
            return report.descriptor_contract == present
                && report.build_graph_proof == present
                && report.hook_readiness == present
                && report.live_native_allocation == partial
                && report.presentation_proof == partial
                && report.sampled_render_targets == partial
                && report.scene_sampled_surface == partial;
        };

        const auto runtimeGuardedReady = [present, partial, missing](const epochengine::RendererCapabilityReport& report) noexcept
        {
            return report.descriptor_contract == present
                && report.build_graph_proof == present
                && report.hook_readiness == partial
                && report.live_native_allocation == partial
                && report.presentation_proof == missing
                && report.sampled_render_targets == partial
                && report.scene_sampled_surface == partial;
        };

        const auto futureNativeReady = [partial, missing](const epochengine::RendererCapabilityReport& report) noexcept
        {
            return report.descriptor_contract == partial
                && report.build_graph_proof == partial
                && report.hook_readiness == missing
                && report.live_native_allocation == missing
                && report.presentation_proof == missing
                && report.sampled_render_targets == partial
                && report.scene_sampled_surface == partial;
        };

        const auto softwareReady =
            software.descriptor_contract == deferred
            && software.build_graph_proof == deferred
            && software.hook_readiness == deferred
            && software.live_native_allocation == deferred
            && software.presentation_proof == deferred
            && software.sampled_render_targets == deferred
            && software.scene_sampled_surface == partial;

        epochengine::OpenGLFamilyRenderDevice openGlNoHooks{ epochengine::RendererBackendKind::opengl };
        const epochengine::RendererCapabilities openGlNoHookCaps = openGlNoHooks.capabilities();

        OpenGLFamilyFakeNativeRttState hookState{};
        epochengine::OpenGLFamilyRenderDevice openGlHooks{ epochengine::RendererBackendKind::opengl };
        openGlHooks.set_native_render_texture_hooks(epochengine::OpenGLFamilyNativeRenderTextureHooks{
            .user = &hookState,
            .allocate = fake_opengl_family_allocate_rtt,
            .destroy = fake_opengl_family_destroy_rtt,
            .begin_pass = fake_opengl_family_begin_rtt_pass,
            .end_pass = fake_opengl_family_end_rtt_pass
        });
        const epochengine::RendererCapabilities openGlHookCaps = openGlHooks.capabilities();

        const bool openGlCapsReady =
            epochengine::renderer_supports_sampled_render_targets(openGlNoHookCaps)
            && !epochengine::renderer_supports_sampled_rtt_hooks(openGlNoHookCaps)
            && !epochengine::renderer_supports_live_sampled_rtt_allocation(openGlNoHookCaps)
            && !epochengine::renderer_supports_native_sampled_render_targets(openGlNoHookCaps)
            && epochengine::renderer_supports_sampled_render_targets(openGlHookCaps)
            && epochengine::renderer_supports_sampled_rtt_hooks(openGlHookCaps)
            && !epochengine::renderer_supports_live_sampled_rtt_allocation(openGlHookCaps)
            && !epochengine::renderer_supports_native_sampled_render_targets(openGlHookCaps);

        bool runtimeGuardCapsReady = true;
#if defined(EPOCH_USING_SDL) && (EPOCH_USING_SDL == 1)
        {
            epochengine::SdlRenderDevice device{};
            const epochengine::RendererCapabilities caps = device.capabilities();
            epochengine::RenderTextureAssetHandles noRuntimeHandles{};
            if (!device.runtime_renderer_available())
                noRuntimeHandles = device.create_render_texture_asset(epochengine::render_arcade::make_screen_render_texture_desc());
            runtimeGuardCapsReady = runtimeGuardCapsReady
                && epochengine::renderer_supports_sampled_render_targets(caps)
                && epochengine::renderer_supports_sampled_rtt_hooks(caps)
                && (epochengine::renderer_supports_live_sampled_rtt_allocation(caps) == device.runtime_renderer_available())
                && (epochengine::renderer_supports_native_sampled_render_targets(caps) == device.runtime_renderer_available())
                && (device.runtime_renderer_available() || (!noRuntimeHandles.color_texture && !noRuntimeHandles.sampler && !noRuntimeHandles.render_target && device.render_texture_count() == 0u));
        }
#endif
#if defined(EPOCH_USING_SFML) && (EPOCH_USING_SFML == 1)
        {
            epochengine::SfmlRenderDevice device{};
            const epochengine::RendererCapabilities caps = device.capabilities();
            runtimeGuardCapsReady = runtimeGuardCapsReady
                && epochengine::renderer_supports_sampled_render_targets(caps)
                && epochengine::renderer_supports_sampled_rtt_hooks(caps)
                && (epochengine::renderer_supports_live_sampled_rtt_allocation(caps) == device.runtime_renderer_available())
                && (epochengine::renderer_supports_native_sampled_render_targets(caps) == device.runtime_renderer_available());
        }
#endif
#if defined(EPOCH_USING_RAYLIB) && (EPOCH_USING_RAYLIB == 1)
        {
            epochengine::RaylibRenderDevice device{};
            const epochengine::RendererCapabilities caps = device.capabilities();
            runtimeGuardCapsReady = runtimeGuardCapsReady
                && epochengine::renderer_supports_sampled_render_targets(caps)
                && epochengine::renderer_supports_sampled_rtt_hooks(caps)
                && (epochengine::renderer_supports_live_sampled_rtt_allocation(caps) == device.runtime_renderer_available())
                && (epochengine::renderer_supports_native_sampled_render_targets(caps) == device.runtime_renderer_available());
        }
#endif

        return openglReady
            && sdlLivePathReady(sdl)
            && runtimeGuardedReady(sfml)
            && runtimeGuardedReady(raylib)
            && futureNativeReady(vulkan)
            && futureNativeReady(directx)
            && softwareReady
            && openGlCapsReady
            && runtimeGuardCapsReady;
    }

    [[nodiscard]] inline bool opengl_real_native_texture_hook_contract_ready()
    {
#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)
        epochengine::OpenGLFamilyRenderDevice device{epochengine::RendererBackendKind::opengl};
        device.set_native_texture_hooks(
            epochengine::opengltextures::make_native_texture_hooks());
        if (!device.native_texture_hooks_ready())
            return false;

        epochengine::TextureDesc desc{};
        desc.width = 2;
        desc.height = 2;
        desc.mip_levels = 1;
        desc.format = epochengine::TextureFormat::rgba8_unorm;
        desc.sampled = true;
        desc.debug_name = "OpenGL.NativeTextureHookContract";
        const epochengine::TextureHandle texture = device.create_texture(desc);
        const epochengine::OpenGLFamilyTextureRecord* const record =
            device.resolve_texture(texture);

        std::array<std::uint8_t, 16> pixels{};
        epochengine::TextureUploadDesc upload{};
        upload.width = 2;
        upload.height = 2;
        upload.row_pitch_bytes = 8;
        upload.format = epochengine::TextureFormat::rgba8_unorm;
        upload.data = pixels.data();
        upload.size_bytes = pixels.size();
        const bool ready = texture
            && record != nullptr
            && record->active
            && !record->native_allocation_ready
            && record->texture_object == 0u
            && !device.texture_ready(texture)
            && !device.upload_texture(texture, upload);
        device.destroy(texture);
        return ready && device.resolve_texture(texture) == nullptr;
#else
        return true;
#endif
    }

    [[nodiscard]] inline bool opengl_real_native_rtt_hook_contract_ready()
    {
#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)
        epochengine::OpenGLFamilyRenderDevice device{ epochengine::RendererBackendKind::opengl };
        device.set_native_render_texture_hooks(
            epochengine::opengltextures::make_native_render_texture_hooks());

        const epochengine::RendererCapabilities caps = device.capabilities();
        if (!epochengine::renderer_supports_sampled_rtt_hooks(caps))
            return false;

        const epochengine::RenderTextureAssetDesc desc = epochengine::render_arcade::make_screen_render_texture_desc();
        const epochengine::RenderTextureAssetHandles handles = device.create_render_texture_asset(desc);
        const epochengine::OpenGLFamilyRenderTextureRecord* const record =
            device.resolve_render_texture(handles.render_target);
        const bool ready =
            static_cast<bool>(handles)
            && record != nullptr
            && record->active
            && record->width == desc.width
            && record->height == desc.height
            && record->backend_requirements.color_attachment
            && record->backend_requirements.depth_attachment
            && record->backend_requirements.sampled_color
            && record->backend_requirements.sampler
            && record->backend_requirements.offscreen_target
            && record->backend_requirements.presentable_surface
            && !record->native_allocation_ready
            && record->native_work_order_ready();
        device.destroy(handles);

        return ready && device.resolve_render_texture(handles.render_target) == nullptr;
#else
        return true;
#endif
    }

    [[nodiscard]] inline bool opengl_family_arcade_native_requirements_contract_ready()
    {
        const epochengine::RendererBackendKind backends[] = {
            epochengine::RendererBackendKind::opengl,
            epochengine::RendererBackendKind::sdl3,
            epochengine::RendererBackendKind::sfml3,
            epochengine::RendererBackendKind::raylib3
        };

        epochengine::RenderTextureAssetDesc screenDesc{};
        screenDesc.width = epochengine::package_registry::engine_arcade_render_texture_width();
        screenDesc.height = epochengine::package_registry::engine_arcade_render_texture_height();
        screenDesc.color_format = epochengine::TextureFormat::rgba8_unorm;
        screenDesc.depth_format = epochengine::TextureFormat::depth24_stencil8;
        screenDesc.has_depth = true;
        screenDesc.sampled_after_render = true;
        screenDesc.usage = epochengine::RenderTextureUsage::arcade_cabinet;
        screenDesc.debug_name = "engine_arcade.screen";

        for (const epochengine::RendererBackendKind backend : backends)
        {
            epochengine::OpenGLFamilyRenderDevice device{ backend };
            if (device.backend() != backend)
                return false;

            const epochengine::RenderTextureAssetHandles handles = device.create_render_texture_asset(screenDesc);
            const epochengine::OpenGLFamilyRenderTextureRecord* const record =
                device.resolve_render_texture(handles.render_target);
            const bool ready =
                static_cast<bool>(handles)
                && record != nullptr
                && record->active
                && record->backend_requirements.color_attachment
                && record->backend_requirements.depth_attachment
                && record->backend_requirements.sampled_color
                && record->backend_requirements.sampler
                && record->backend_requirements.offscreen_target
                && record->backend_requirements.presentable_surface
                && record->native_work_order_ready()
                && !record->native_allocation_ready
                && record->color_object != 0u
                && record->depth_object != 0u
                && record->framebuffer_object != 0u
                && record->sampler_object != 0u;
            device.destroy(handles);

            if (!ready || device.resolve_render_texture(handles.render_target) != nullptr)
                return false;
        }

        return true;
    }

    [[nodiscard]] inline bool sdl_native_render_texture_device_contract_ready()
    {
#if defined(EPOCH_USING_SDL) && (EPOCH_USING_SDL == 1)
        epochengine::SdlRenderDevice device{};
        const epochengine::RendererCapabilities caps = device.capabilities();
        const bool runtimeAvailable = device.runtime_renderer_available();
        if (device.backend_name() != "sdl3"
            || epochengine::renderer_supports_native_sampled_render_targets(caps) != runtimeAvailable)
            return false;

        const epochengine::RenderTextureAssetDesc desc = epochengine::render_arcade::make_screen_render_texture_desc();
        const epochengine::RenderTextureAssetHandles handles = device.create_render_texture_asset(desc);
        const epochengine::SdlRenderTextureRecord* const record = device.resolve_render_texture(handles.render_target);

        const bool ready = runtimeAvailable
            ? static_cast<bool>(handles)
                && record != nullptr
                && record->active
                && record->texture != nullptr
                && record->width == desc.width
                && record->height == desc.height
            : !static_cast<bool>(handles)
                && record == nullptr
                && device.render_texture_count() == 0u;

        device.destroy(handles);
        return ready && device.resolve_render_texture(handles.render_target) == nullptr;
#else
        return true;
#endif
    }

    [[nodiscard]] inline bool sfml_native_render_texture_device_contract_ready()
    {
#if defined(EPOCH_USING_SFML) && (EPOCH_USING_SFML == 1)
        epochengine::SfmlRenderDevice device{};
        const epochengine::RendererCapabilities caps = device.capabilities();
        const bool runtimeAvailable = device.runtime_renderer_available();
        if (device.backend_name() != "sfml3"
            || epochengine::renderer_supports_native_sampled_render_targets(caps) != runtimeAvailable)
            return false;

        const epochengine::RenderTextureAssetDesc desc = epochengine::render_arcade::make_screen_render_texture_desc();
        const epochengine::RenderTextureAssetHandles handles = device.create_render_texture_asset(desc);
        const epochengine::SfmlRenderTextureRecord* const record = device.resolve_render_texture(handles.render_target);

        const bool ready = runtimeAvailable
            ? static_cast<bool>(handles)
                && record != nullptr
                && record->active
                && record->target != nullptr
                && record->width == desc.width
                && record->height == desc.height
            : !static_cast<bool>(handles)
                && record == nullptr
                && device.render_texture_count() == 0u;

        device.destroy(handles);
        return ready && device.resolve_render_texture(handles.render_target) == nullptr;
#else
        return true;
#endif
    }

    [[nodiscard]] inline bool raylib_native_render_texture_device_contract_ready()
    {
#if defined(EPOCH_USING_RAYLIB) && (EPOCH_USING_RAYLIB == 1)
        epochengine::RaylibRenderDevice device{};
        const epochengine::RendererCapabilities caps = device.capabilities();
        const bool runtimeAvailable = device.runtime_renderer_available();
        if (device.backend_name() != "raylib"
            || epochengine::renderer_supports_native_sampled_render_targets(caps) != runtimeAvailable)
            return false;

        const epochengine::RenderTextureAssetDesc desc = epochengine::render_arcade::make_screen_render_texture_desc();
        const epochengine::RenderTextureAssetHandles handles = device.create_render_texture_asset(desc);
        const epochengine::RaylibRenderTextureRecord* const record = device.resolve_render_texture(handles.render_target);

        const bool ready = runtimeAvailable
            ? static_cast<bool>(handles)
                && record != nullptr
                && record->active
                && record->width == desc.width
                && record->height == desc.height
            : !static_cast<bool>(handles)
                && record == nullptr
                && device.render_texture_count() == 0u;

        device.destroy(handles);
        return ready && device.resolve_render_texture(handles.render_target) == nullptr;
#else
        return true;
#endif
    }

    [[nodiscard]] inline bool sdl_arcade_cabinet_graph_contract_ready()
    {
#if defined(EPOCH_USING_SDL) && (EPOCH_USING_SDL == 1)
        epochengine::SdlRenderDevice device{};
        const epochengine::RendererCapabilities caps = device.capabilities();
        const bool runtimeAvailable = device.runtime_renderer_available();
        if (device.backend_name() != "sdl3"
            || !epochengine::renderer_supports_sampled_render_targets(caps)
            || !epochengine::renderer_supports_model_resources(caps)
            || epochengine::renderer_supports_native_sampled_render_targets(caps) != runtimeAvailable)
        {
            return false;
        }

        epochengine::GraphBuilder builder{};
        const epochengine::render_arcade::ArcadeCabinetGraphBuild cabinet = epochengine::render_arcade::add_cabinet_graph(builder);
        epochengine::CompiledGraph graph = builder.compile(device);

        const bool resourceShape =
            graph.buffers.size() == 6u
            && graph.samplers.size() == 1u
            && graph.materials.size() == 3u
            && graph.meshes.size() == 3u
            && graph.models.size() == 2u
            && graph.passes.size() == 2u;
        if (!resourceShape)
        {
            graph.destroy(device);
            return false;
        }

        const epochengine::GraphRenderTextureAsset& compiledScreen = graph.render_texture_assets.front();
        const epochengine::GraphMaterial& compiledScreenMaterial = graph.materials[1u];
        const epochengine::GraphMaterial& compiledBodyMaterial = graph.materials[2u];
        const epochengine::GraphMesh& compiledScreenMesh = graph.meshes[1u];
        const epochengine::GraphMesh& compiledBodyMesh = graph.meshes[2u];
        const epochengine::GraphModel& compiledModel = graph.models.back();
        const epochengine::PassDecl& cabinetPass = graph.passes[1u];

        const bool graphReady =
            compiledScreenMaterial.backend
            && compiledBodyMaterial.backend
            && compiledScreenMesh.backend
            && compiledBodyMesh.backend
            && compiledModel.backend
            && compiledModel.mesh_slots.size() == 2u
            && compiledModel.mesh_slots[0u].mesh == cabinet.body_mesh
            && compiledModel.mesh_slots[0u].material == cabinet.body_material
            && compiledModel.mesh_slots[1u].mesh == cabinet.mesh
            && compiledModel.mesh_slots[1u].material == cabinet.material
            && cabinetPass.binding_set
            && cabinetPass.bindings.read_materials.size() == 2u
            && cabinetPass.bindings.read_materials[0u] == compiledScreenMaterial.backend
            && cabinetPass.bindings.read_materials[1u] == compiledBodyMaterial.backend
            && cabinetPass.bindings.read_models.size() == 1u
            && cabinetPass.bindings.read_models.front() == compiledModel.backend
            && cabinetPass.draw_models.size() == 1u
            && cabinetPass.draw_models.front().model == cabinet.model
            && cabinetPass.draw_models.front().backend == compiledModel.backend;

        const bool sampledBindingReady = runtimeAvailable
            ? compiledScreen.backend
                && device.render_texture_count() == 1u
                && cabinetPass.bindings.read_material_textures.size() == 1u
                && cabinetPass.bindings.read_material_textures.front().slot == epochengine::MaterialTextureSlot::render_surface
                && cabinetPass.bindings.read_samplers.size() == 1u
            : !compiledScreen.backend
                && device.render_texture_count() == 0u
                && cabinetPass.bindings.read_material_textures.empty()
                && cabinetPass.bindings.read_samplers.empty();

        epochengine::SdlCommandContext& context = static_cast<epochengine::SdlCommandContext&>(device.acquire_graphics_context());
        graph.execute(device);
        const epochengine::ModelHandle submitted = context.last_model();
        const bool submitReady =
            graphReady
            && sampledBindingReady
            && submitted
            && submitted == compiledModel.backend
            && context.bound_binding_set() == cabinetPass.binding_set
            && context.bound_resources().read_models.size() == 1u
            && context.bound_resources().read_models.front() == submitted
            && device.resolve_model(submitted) != nullptr;

        graph.destroy(device);
        return submitReady
            && device.resolve_model(submitted) == nullptr
            && device.render_texture_count() == 0u;
#else
        return true;
#endif
    }

    [[nodiscard]] inline bool sfml_arcade_cabinet_graph_contract_ready()
    {
#if defined(EPOCH_USING_SFML) && (EPOCH_USING_SFML == 1)
        epochengine::SfmlRenderDevice device{};
        const epochengine::RendererCapabilities caps = device.capabilities();
        const bool runtimeAvailable = device.runtime_renderer_available();
        if (device.backend_name() != "sfml3"
            || !epochengine::renderer_supports_sampled_render_targets(caps)
            || !epochengine::renderer_supports_model_resources(caps)
            || epochengine::renderer_supports_native_sampled_render_targets(caps) != runtimeAvailable)
        {
            return false;
        }

        epochengine::GraphBuilder builder{};
        const epochengine::render_arcade::ArcadeCabinetGraphBuild cabinet = epochengine::render_arcade::add_cabinet_graph(builder);
        epochengine::CompiledGraph graph = builder.compile(device);

        const bool resourceShape =
            graph.buffers.size() == 6u
            && graph.samplers.size() == 1u
            && graph.materials.size() == 3u
            && graph.meshes.size() == 3u
            && graph.models.size() == 2u
            && graph.passes.size() == 2u;
        if (!resourceShape)
        {
            graph.destroy(device);
            return false;
        }

        const epochengine::GraphRenderTextureAsset& compiledScreen = graph.render_texture_assets.front();
        const epochengine::GraphMaterial& compiledScreenMaterial = graph.materials[1u];
        const epochengine::GraphMaterial& compiledBodyMaterial = graph.materials[2u];
        const epochengine::GraphMesh& compiledScreenMesh = graph.meshes[1u];
        const epochengine::GraphMesh& compiledBodyMesh = graph.meshes[2u];
        const epochengine::GraphModel& compiledModel = graph.models.back();
        const epochengine::PassDecl& cabinetPass = graph.passes[1u];

        const bool graphReady =
            compiledScreenMaterial.backend
            && compiledBodyMaterial.backend
            && compiledScreenMesh.backend
            && compiledBodyMesh.backend
            && compiledModel.backend
            && compiledModel.mesh_slots.size() == 2u
            && compiledModel.mesh_slots[0u].mesh == cabinet.body_mesh
            && compiledModel.mesh_slots[0u].material == cabinet.body_material
            && compiledModel.mesh_slots[1u].mesh == cabinet.mesh
            && compiledModel.mesh_slots[1u].material == cabinet.material
            && cabinetPass.binding_set
            && cabinetPass.bindings.read_materials.size() == 2u
            && cabinetPass.bindings.read_materials[0u] == compiledScreenMaterial.backend
            && cabinetPass.bindings.read_materials[1u] == compiledBodyMaterial.backend
            && cabinetPass.bindings.read_models.size() == 1u
            && cabinetPass.bindings.read_models.front() == compiledModel.backend
            && cabinetPass.draw_models.size() == 1u
            && cabinetPass.draw_models.front().model == cabinet.model
            && cabinetPass.draw_models.front().backend == compiledModel.backend;

        const bool sampledBindingReady = runtimeAvailable
            ? compiledScreen.backend
                && device.render_texture_count() == 1u
                && cabinetPass.bindings.read_material_textures.size() == 1u
                && cabinetPass.bindings.read_material_textures.front().slot == epochengine::MaterialTextureSlot::render_surface
                && cabinetPass.bindings.read_samplers.size() == 1u
            : !compiledScreen.backend
                && device.render_texture_count() == 0u
                && cabinetPass.bindings.read_material_textures.empty()
                && cabinetPass.bindings.read_samplers.empty();

        epochengine::SfmlCommandContext& context = static_cast<epochengine::SfmlCommandContext&>(device.acquire_graphics_context());
        graph.execute(device);
        const epochengine::ModelHandle submitted = context.last_model();
        const bool submitReady =
            graphReady
            && sampledBindingReady
            && submitted
            && submitted == compiledModel.backend
            && context.bound_binding_set() == cabinetPass.binding_set
            && context.bound_resources().read_models.size() == 1u
            && context.bound_resources().read_models.front() == submitted
            && device.resolve_model(submitted) != nullptr;

        graph.destroy(device);
        return submitReady
            && device.resolve_model(submitted) == nullptr
            && device.render_texture_count() == 0u;
#else
        return true;
#endif
    }

    [[nodiscard]] inline bool raylib_arcade_cabinet_graph_contract_ready()
    {
#if defined(EPOCH_USING_RAYLIB) && (EPOCH_USING_RAYLIB == 1)
        epochengine::RaylibRenderDevice device{};
        const epochengine::RendererCapabilities caps = device.capabilities();
        const bool runtimeAvailable = device.runtime_renderer_available();
        if (device.backend_name() != "raylib"
            || !epochengine::renderer_supports_sampled_render_targets(caps)
            || !epochengine::renderer_supports_model_resources(caps)
            || epochengine::renderer_supports_native_sampled_render_targets(caps) != runtimeAvailable)
        {
            return false;
        }

        epochengine::GraphBuilder builder{};
        const epochengine::render_arcade::ArcadeCabinetGraphBuild cabinet = epochengine::render_arcade::add_cabinet_graph(builder);
        epochengine::CompiledGraph graph = builder.compile(device);

        const bool resourceShape =
            graph.render_texture_assets.size() == 1u
            && graph.textures.size() == 1u
            && graph.samplers.size() == 1u
            && graph.render_targets.size() == 1u
            && graph.buffers.size() == 6u
            && graph.materials.size() == 3u
            && graph.meshes.size() == 3u
            && graph.models.size() == 2u
            && graph.passes.size() == 2u;
        if (!resourceShape)
        {
            graph.destroy(device);
            return false;
        }

        const epochengine::GraphRenderTextureAsset& compiledScreen = graph.render_texture_assets.front();
        const epochengine::GraphMaterial& compiledScreenMaterial = graph.materials[1u];
        const epochengine::GraphMaterial& compiledBodyMaterial = graph.materials[2u];
        const epochengine::GraphMesh& compiledScreenMesh = graph.meshes[1u];
        const epochengine::GraphMesh& compiledBodyMesh = graph.meshes[2u];
        const epochengine::GraphModel& compiledModel = graph.models.back();
        const epochengine::PassDecl& cabinetPass = graph.passes[1u];

        const bool graphReady =
            compiledScreenMaterial.backend
            && compiledBodyMaterial.backend
            && compiledScreenMesh.backend
            && compiledBodyMesh.backend
            && compiledModel.backend
            && compiledModel.mesh_slots.size() == 2u
            && compiledModel.mesh_slots[0u].mesh == cabinet.body_mesh
            && compiledModel.mesh_slots[0u].material == cabinet.body_material
            && compiledModel.mesh_slots[1u].mesh == cabinet.mesh
            && compiledModel.mesh_slots[1u].material == cabinet.material
            && cabinetPass.binding_set
            && cabinetPass.bindings.read_materials.size() == 2u
            && cabinetPass.bindings.read_materials[0u] == compiledScreenMaterial.backend
            && cabinetPass.bindings.read_materials[1u] == compiledBodyMaterial.backend
            && cabinetPass.bindings.read_models.size() == 1u
            && cabinetPass.bindings.read_models.front() == compiledModel.backend
            && cabinetPass.draw_models.size() == 1u
            && cabinetPass.draw_models.front().model == cabinet.model
            && cabinetPass.draw_models.front().backend == compiledModel.backend;

        const bool sampledBindingReady = runtimeAvailable
            ? compiledScreen.backend
                && device.render_texture_count() == 1u
                && cabinetPass.bindings.read_material_textures.size() == 1u
                && cabinetPass.bindings.read_material_textures.front().slot == epochengine::MaterialTextureSlot::render_surface
                && cabinetPass.bindings.read_samplers.size() == 1u
            : !compiledScreen.backend
                && device.render_texture_count() == 0u
                && cabinetPass.bindings.read_material_textures.empty()
                && cabinetPass.bindings.read_samplers.empty();

        epochengine::RaylibCommandContext& context = static_cast<epochengine::RaylibCommandContext&>(device.acquire_graphics_context());
        graph.execute(device);
        const epochengine::ModelHandle submitted = context.last_model();
        const bool submitReady =
            graphReady
            && sampledBindingReady
            && submitted
            && submitted == compiledModel.backend
            && context.bound_binding_set() == cabinetPass.binding_set
            && context.bound_resources().read_models.size() == 1u
            && context.bound_resources().read_models.front() == submitted
            && device.resolve_model(submitted) != nullptr;

        graph.destroy(device);
        return submitReady
            && device.resolve_model(submitted) == nullptr
            && device.render_texture_count() == 0u;
#else
        return true;
#endif
    }

    [[nodiscard]] inline bool raylib_texture_storage_contract_ready()
    {
#if defined(EPOCH_USING_RAYLIB) && (EPOCH_USING_RAYLIB == 1)
        auto previous = epochengine::core::get_current_render_context();
        auto ctx = std::make_shared<epochengine::core::Context>();
        ctx->type = epochengine::core::ContextType::RayLib;
        ctx->native_drawable = nullptr;

        epochengine::core::set_current_render_context(ctx);
        const bool ready =
            epochengine::raylibtextures::backend_storage_is_separate_from_context_native_drawable()
            && ctx->native_drawable == nullptr;
        epochengine::core::set_current_render_context(std::move(previous));

        return ready;
#else
        return true;
#endif
    }

    [[nodiscard]] inline bool atlas_snapshot_upload_contract_ready()
    {
        epochengine::TextureAtlas atlas{};
        if (!atlas.init(epochengine::AtlasConfig{
                .name = "contract.atlas_snapshot_upload",
                .width = 16u,
                .height = 16u,
                .generate_mipmaps = false }))
        {
            return false;
        }

        auto makeTexture = [](std::string name, std::uint8_t red, std::uint8_t green, std::uint8_t blue)
        {
            epochengine::Texture texture{};
            texture.name = std::move(name);
            texture.width = 4u;
            texture.height = 4u;
            texture.channels = 4u;
            texture.pixels.resize(static_cast<std::size_t>(texture.width) * texture.height * texture.channels);
            for (std::size_t i = 0; i + 3u < texture.pixels.size(); i += 4u)
            {
                texture.pixels[i + 0u] = red;
                texture.pixels[i + 1u] = green;
                texture.pixels[i + 2u] = blue;
                texture.pixels[i + 3u] = 255u;
            }
            return texture;
        };

        const auto red = makeTexture("snapshot.red", 255u, 0u, 0u);
        if (!atlas.add_entry("snapshot.red", red))
            return false;

        const auto first = atlas.snapshot_pixels();
        if (first.width != 16u
            || first.height != 16u
            || first.version == 0u
            || first.pixels.size() != 16u * 16u * 4u)
        {
            return false;
        }

        const auto green = makeTexture("snapshot.green", 0u, 255u, 0u);
        if (!atlas.add_entry("snapshot.green", green))
            return false;

        const auto second = atlas.snapshot_pixels();
        if (second.width != first.width
            || second.height != first.height
            || second.pixels.size() != first.pixels.size()
            || second.version <= first.version
            || second.pixels == first.pixels
            || atlas.entry_count() != 2u)
        {
            return false;
        }

        for (std::uint32_t iteration = 0u; iteration < 1'024u; ++iteration)
        {
            const auto replacement = makeTexture(
                "snapshot.red",
                iteration % 2u == 0u ? 255u : 0u,
                iteration % 2u == 0u ? 255u : 0u,
                iteration % 2u == 0u ? 0u : 255u);
            if (!atlas.replace_entry_pixels("snapshot.red", replacement)
                || atlas.entry_count() != 2u)
            {
                return false;
            }
        }

        const auto replaced = atlas.snapshot_pixels();
        return replaced.width == second.width
            && replaced.height == second.height
            && replaced.pixels.size() == second.pixels.size()
            && replaced.version >= second.version + 1'024u
            && replaced.pixels != second.pixels
            && atlas.entry_count() == 2u;
    }

    [[nodiscard]] constexpr bool editor_session_restore_allowed(
        epochengine::core::BackendLifecycleState lifecycle) noexcept
    {
        return lifecycle == epochengine::core::BackendLifecycleState::ready;
    }

    [[nodiscard]] constexpr bool editor_session_deferred_for_active_replacement(
        const void* candidate,
        const void* activeReplacement,
        bool restorationAllowed) noexcept
    {
        return !restorationAllowed
            && candidate != nullptr
            && activeReplacement != nullptr
            && candidate == activeReplacement;
    }

    [[nodiscard]] constexpr bool editor_restored_frame_acknowledged(
        bool acknowledgementQueued,
        std::uint64_t acknowledgementGeneration,
        std::uint64_t completedFrameGeneration) noexcept
    {
        return acknowledgementQueued
            && acknowledgementGeneration != 0
            && completedFrameGeneration >= acknowledgementGeneration;
    }

    [[nodiscard]] inline int run_engine_contract_self_test()
    {
        bool failed = false;
        const auto check = [&failed](std::string_view name, bool passed)
        {
            log_editor_self_test_line(
                "engine_contract_self_test."
                + std::string(name)
                + "="
                + (passed ? std::string{ "pass" } : std::string{ "fail" }));
            failed = failed || !passed;
        };

        log_editor_self_test_line("engine_contract_self_test.start=forest_package_timeline_snapshot");
        check(
            "core.format_portability",
            epochengine::format_text(
                "{} {:02} {:.2f} {:08X} {:p} {{ok}}",
                "item",
                7,
                1.25,
                0xABu,
                0x2Au) == "item 07 1.25 000000AB 0x2a {ok}"
            && epochengine::format_text("{:04}", -7) == "-007");

        check(
            "updater.discovery_parser_policy",
            epochengine::updater::update_discovery_contract_self_test());

        check(
            "updater.private_source_crypto_policy",
            epochengine::updater::private_source_access_contract_self_test());
        check(
            "updater.verified_source_authority",
            epochengine::updater::verified_source_authority_contract_self_test());

        check(
            "context.session_restore_readiness",
            !editor_session_restore_allowed(epochengine::core::BackendLifecycleState::pending)
            && !editor_session_restore_allowed(epochengine::core::BackendLifecycleState::initializing)
            && editor_session_restore_allowed(epochengine::core::BackendLifecycleState::ready)
            && !editor_session_restore_allowed(epochengine::core::BackendLifecycleState::failed)
            && !editor_session_restore_allowed(epochengine::core::BackendLifecycleState::stopped));

        const int replacementIdentities[2]{};
        check(
            "context.session_replacement_ownership",
            editor_session_deferred_for_active_replacement(
                &replacementIdentities[0], &replacementIdentities[0], false)
            && !editor_session_deferred_for_active_replacement(
                &replacementIdentities[0], &replacementIdentities[0], true)
            && !editor_session_deferred_for_active_replacement(
                &replacementIdentities[0], &replacementIdentities[1], false)
            && !editor_session_deferred_for_active_replacement(
                nullptr, &replacementIdentities[0], false)
            && !editor_session_deferred_for_active_replacement(
                &replacementIdentities[0], nullptr, false));

#if defined(EPOCH_USING_SDL) && (EPOCH_USING_SDL == 1)
        const auto sdlLifecycleContract = []
        {
            auto& runtimeMutex =
                epochengine::sdlcontext::state::runtime_api_mutex();
            bool recursiveOwnership = false;
            bool contenderExcluded = false;
            {
                std::scoped_lock runtimeGuard{runtimeMutex};
                recursiveOwnership = runtimeMutex.try_lock();
                if (recursiveOwnership)
                    runtimeMutex.unlock();

                std::thread contender(
                    [&runtimeMutex, &contenderExcluded]
                    {
                        const bool acquired = runtimeMutex.try_lock();
                        contenderExcluded = !acquired;
                        if (acquired)
                            runtimeMutex.unlock();
                    });
                contender.join();
            }

            bool contenderAdmittedAfterRelease = false;
            std::thread admitted(
                [&runtimeMutex, &contenderAdmittedAfterRelease]
                {
                    {
                        std::scoped_lock runtimeGuard{runtimeMutex};
                        contenderAdmittedAfterRelease = true;
                    }
                });
            admitted.join();
            return recursiveOwnership
                && contenderExcluded
                && contenderAdmittedAfterRelease;
        };
        check(
            "context.sdl_runtime_lifecycle_serialization",
            sdlLifecycleContract());
        const auto sdlHighDpiDimensions =
            epochengine::sdlcontext::state::
                make_display_scaled_presentation_dimensions(
                2'478,
                1'344,
                1.5f);
        const auto sdlNativeScaleDimensions =
            epochengine::sdlcontext::state::
                make_display_scaled_presentation_dimensions(
                1'920,
                1'080,
                1.0f);
        const auto sdlInvalidDimensions =
            epochengine::sdlcontext::state::make_presentation_dimensions(
                0,
                1'080,
                1'920,
                1'080);
        check(
            "context.sdl_high_dpi_logical_presentation",
            sdlHighDpiDimensions.valid()
            && sdlHighDpiDimensions.pixel_scale_x() == 1.5f
            && sdlHighDpiDimensions.pixel_scale_y() == 1.5f
            && sdlNativeScaleDimensions.valid()
            && sdlNativeScaleDimensions.pixel_scale_x() == 1.0f
            && sdlNativeScaleDimensions.pixel_scale_y() == 1.0f
            && !sdlInvalidDimensions.valid()
            && sdlInvalidDimensions.pixel_scale_x() == 0.0f
            && sdlInvalidDimensions.pixel_scale_y() == 0.0f
            && epochengine::sdlcontext::state::normalize_presented_coordinate(
                900, 1'652, 2'478) == 600
            && epochengine::sdlcontext::state::normalize_presented_coordinate(
                -90, 1'652, 2'478) == -60
            && epochengine::sdlcontext::state::normalize_presented_coordinate(
                900, 1'920, 1'920) == 900);
#endif

        check(
            "context.session_restored_frame_acknowledgement",
            !editor_restored_frame_acknowledged(false, 2, 2)
            && !editor_restored_frame_acknowledged(true, 0, 2)
            && !editor_restored_frame_acknowledged(true, 3, 2)
            && editor_restored_frame_acknowledged(true, 3, 3)
            && editor_restored_frame_acknowledged(true, 3, 4));
        check(
            "context.settings_selection_serialization",
            epochengine::editor_context_selection_allowed(true, false, false)
            && !epochengine::editor_context_selection_allowed(false, false, false)
            && !epochengine::editor_context_selection_allowed(true, true, false)
            && !epochengine::editor_context_selection_allowed(true, false, true));

        const auto temporalRequestContract = epochengine::temporal::run_request_contract();
        check(
            "temporal.request_mapping",
            temporalRequestContract.mapping
            && temporalRequestContract.reverse
            && temporalRequestContract.frozen);
        check(
            "temporal.request_history",
            temporalRequestContract.exact
            && temporalRequestContract.bracket
            && temporalRequestContract.nearest
            && temporalRequestContract.clamped
            && temporalRequestContract.bounded
            && temporalRequestContract.stale_handle
            && temporalRequestContract.metrics);

        const auto standardScene = epochengine::make_standard_editor_scene();
        const auto plantLabApplicationScene = epochengine::make_plant_lab_editor_scene();
        const auto guiApplicationScene = epochengine::make_gui_editor_scene();
        const auto& standardApplication = epochengine::standard_editor_application();
        const auto& plantLabApplication = epochengine::plant_lab_editor_application();
        const auto& guiApplication = epochengine::gui_editor_application();
        const bool plantLabPreviewIdentity = std::any_of(
            plantLabApplicationScene.entities.begin(),
            plantLabApplicationScene.entities.end(),
            [](const epochengine::EditorSceneSeedEntity& entity)
            {
                return entity.category == "PlantLabPreview";
            }) && std::none_of(
                plantLabApplicationScene.entities.begin(),
                plantLabApplicationScene.entities.end(),
                [](const epochengine::EditorSceneSeedEntity& entity)
                {
                    return entity.category == "ForestFactory";
                });
        check(
            "editor.application_scenes",
            epochengine::validate_editor_application(standardApplication, standardScene)
            && epochengine::validate_editor_application(plantLabApplication, plantLabApplicationScene)
            && epochengine::validate_editor_application(guiApplication, guiApplicationScene));
        check(
            "editor.application_ownership",
            plantLabPreviewIdentity
            && plantLabApplication.project_id.empty()
            && guiApplication.project_id.empty()
            && epochengine::editor_find_project_profile("plantlab") == nullptr
            && epochengine::editor_application_for_project("projectlauncher") == &standardApplication
            && epochengine::editor_application_for_project("plantlab") == &plantLabApplication
            && epochengine::editor_application_for_project("twodstudio") == nullptr
            && epochengine::editor_application_supports_surface(
                standardApplication, epochengine::EditorApplicationSurface::ForestFactory)
            && !epochengine::editor_application_supports_surface(
                standardApplication, epochengine::EditorApplicationSurface::PlantLab)
            && epochengine::editor_application_supports_surface(
                standardApplication, epochengine::EditorApplicationSurface::Game2D)
            && epochengine::editor_application_supports_surface(
                plantLabApplication, epochengine::EditorApplicationSurface::PlantLab)
            && !epochengine::editor_application_supports_surface(
                plantLabApplication, epochengine::EditorApplicationSurface::ForestFactory)
            && !epochengine::editor_application_supports_surface(
                plantLabApplication, epochengine::EditorApplicationSurface::AISandbox)
            && epochengine::editor_application_supports_surface(
                guiApplication, epochengine::EditorApplicationSurface::Game2D)
            && epochengine::editor_application_owns_dedicated_gui_workspace(
                guiApplication.kind)
            && !epochengine::editor_application_owns_dedicated_gui_workspace(
                standardApplication.kind)
            && !epochengine::editor_application_owns_dedicated_gui_workspace(
                plantLabApplication.kind)
            && !epochengine::editor_application_supports_surface(
                guiApplication, epochengine::EditorApplicationSurface::Scene)
            && epochengine::editor_application_for_project("forestfactory") == &standardApplication);

        constexpr double editorFrameDt = 1.0 / 120.0;
        const double initialFrameDeadline =
            epochengine::perf::next_frame_deadline(
                0.0, 100.0, editorFrameDt, false);
        const double staleFrameDeadline =
            epochengine::perf::next_frame_deadline(
                50.0, 100.0, editorFrameDt, true);
        const double currentFrameDeadline =
            epochengine::perf::next_frame_deadline(
                100.0 + editorFrameDt * 0.25,
                100.0,
                editorFrameDt,
                true);
        check(
            "perf.frame_limiter_deadline_rebase",
            std::abs(initialFrameDeadline - (100.0 + editorFrameDt)) < 1.0e-12
            && std::abs(staleFrameDeadline - (100.0 + editorFrameDt)) < 1.0e-12
            && std::abs(currentFrameDeadline
                    - (100.0 + editorFrameDt * 1.25)) < 1.0e-12);

        const auto editorPacingPolicy =
            epochengine::perf::select_frame_pacing_policy(
                epochengine::perf::frame_pacing_mode::target_hz,
                0.0,
                false,
                false);
        const auto editorPacing = epochengine::perf::resolve_frame_pacing(
            editorPacingPolicy,
            epochengine::perf::frame_activity::foreground,
            false);
        const auto standalonePacing = epochengine::perf::resolve_frame_pacing(
            epochengine::perf::select_frame_pacing_policy(
                epochengine::perf::frame_pacing_mode::target_hz,
                0.0,
                false,
                true),
            epochengine::perf::frame_activity::foreground,
            false);
        check(
            "perf.frame_pacing_default_60_120",
            editorPacing.requested_mode
                    == epochengine::perf::frame_pacing_mode::target_hz
                && editorPacing.effective_mode
                    == epochengine::perf::frame_pacing_mode::target_hz
                && std::abs(editorPacing.effective_hz - 120.0) < 1.0e-12
                && editorPacing.cpu_deadline_wait
                && std::abs(standalonePacing.effective_hz - 60.0) < 1.0e-12
                && standalonePacing.cpu_deadline_wait);

        const auto uncappedPolicy =
            epochengine::perf::select_frame_pacing_policy(
                epochengine::perf::frame_pacing_mode::uncapped,
                0.0,
                true,
                false);
        const auto uncappedForeground =
            epochengine::perf::resolve_frame_pacing(
                uncappedPolicy,
                epochengine::perf::frame_activity::foreground,
                false);
        const auto uncappedBackground =
            epochengine::perf::resolve_frame_pacing(
                uncappedPolicy,
                epochengine::perf::frame_activity::background,
                false);
        const auto uncappedMinimized =
            epochengine::perf::resolve_frame_pacing(
                uncappedPolicy,
                epochengine::perf::frame_activity::minimized,
                false);
        check(
            "perf.frame_pacing_uncapped_background_minimized",
            uncappedForeground.effective_mode
                    == epochengine::perf::frame_pacing_mode::uncapped
                && uncappedForeground.effective_hz == 0.0
                && !uncappedForeground.cpu_deadline_wait
                && uncappedBackground.effective_mode
                    == epochengine::perf::frame_pacing_mode::target_hz
                && std::abs(uncappedBackground.effective_hz - 30.0) < 1.0e-12
                && uncappedBackground.cpu_deadline_wait
                && uncappedMinimized.effective_mode
                    == epochengine::perf::frame_pacing_mode::target_hz
                && std::abs(uncappedMinimized.effective_hz - 10.0) < 1.0e-12
                && uncappedMinimized.cpu_deadline_wait);

        const auto vsyncPolicy = epochengine::perf::frame_pacing_policy{
            epochengine::perf::frame_pacing_mode::vsync,
            60.0,
            30.0,
            10.0};
        const auto nativeVsync = epochengine::perf::resolve_frame_pacing(
            vsyncPolicy,
            epochengine::perf::frame_activity::foreground,
            true);
        const auto fallbackVsync = epochengine::perf::resolve_frame_pacing(
            vsyncPolicy,
            epochengine::perf::frame_activity::foreground,
            false);
        check(
            "perf.frame_pacing_vsync_fallback",
            nativeVsync.effective_mode
                    == epochengine::perf::frame_pacing_mode::vsync
                && nativeVsync.native_vsync_requested
                && !nativeVsync.cpu_deadline_wait
                && fallbackVsync.effective_mode
                    == epochengine::perf::frame_pacing_mode::target_hz
                && std::abs(fallbackVsync.effective_hz - 60.0) < 1.0e-12
                && !fallbackVsync.native_vsync_requested
                && fallbackVsync.cpu_deadline_wait);

        const auto nativeTargetDesired =
            epochengine::perf::resolve_frame_pacing_with_capabilities(
                epochengine::perf::target_hz_policy(120.0),
                epochengine::perf::frame_activity::foreground,
                {false, true});
        const auto nativeTargetApplied =
            epochengine::perf::finalize_native_frame_pacing(
                nativeTargetDesired,
                {
                    true,
                    true,
                    epochengine::perf::frame_pacing_mode::target_hz,
                    120.0});
        const auto nativeTargetRejected =
            epochengine::perf::finalize_native_frame_pacing(
                nativeTargetDesired,
                {});
        check(
            "perf.frame_pacing_native_target_no_double_throttle",
            nativeTargetDesired.native_pacing_requested
                && !nativeTargetDesired.cpu_deadline_wait
                && nativeTargetApplied.native_pacing_configured
                && nativeTargetApplied.native_pacing_active
                && !nativeTargetApplied.cpu_deadline_wait
                && nativeTargetApplied.effective_mode
                    == epochengine::perf::frame_pacing_mode::target_hz
                && std::abs(nativeTargetApplied.effective_hz - 120.0) < 1.0e-12
                && !nativeTargetRejected.native_pacing_configured
                && !nativeTargetRejected.native_pacing_active
                && nativeTargetRejected.cpu_deadline_wait
                && std::abs(nativeTargetRejected.effective_hz - 120.0) < 1.0e-12);

        const auto nativeVsyncApplied =
            epochengine::perf::finalize_native_frame_pacing(
                nativeVsync,
                {
                    true,
                    true,
                    epochengine::perf::frame_pacing_mode::vsync,
                    0.0});
        const auto forcedVsyncOnUncapped =
            epochengine::perf::finalize_native_frame_pacing(
                uncappedForeground,
                {
                    false,
                    true,
                    epochengine::perf::frame_pacing_mode::vsync,
                    0.0});
        check(
            "perf.frame_pacing_native_vsync_and_forced_fallback",
            nativeVsyncApplied.native_pacing_configured
                && nativeVsyncApplied.native_pacing_active
                && nativeVsyncApplied.native_vsync_requested
                && !nativeVsyncApplied.cpu_deadline_wait
                && forcedVsyncOnUncapped.native_pacing_active
                && forcedVsyncOnUncapped.effective_mode
                    == epochengine::perf::frame_pacing_mode::vsync
                && !forcedVsyncOnUncapped.cpu_deadline_wait);

        auto forestProfile = epochengine::forest::default_profile(epochengine::forest::ForestPreset::Tree);
        forestProfile.temporal.timeSeconds = forestProfile.temporal.durationSeconds;
        const auto forestEstimate = epochengine::forest::estimate_preview_stats(forestProfile);
        const auto forestGeometry = epochengine::forest::build_preview_geometry(forestProfile);
        const auto previewActivation = epochengine::forest::activation_for_editor_preview();
        const auto sceneActivation = epochengine::forest::activation_for_scene_use();
        const auto presentVsync = epochengine::perf::select_native_present_mode(
            epochengine::perf::frame_pacing_mode::vsync,
            {true, true, true});
        const auto presentImmediate = epochengine::perf::select_native_present_mode(
            epochengine::perf::frame_pacing_mode::uncapped,
            {true, true, true});
        const auto presentMailbox = epochengine::perf::select_native_present_mode(
            epochengine::perf::frame_pacing_mode::uncapped,
            {false, true, true});
        const auto presentForcedFifo = epochengine::perf::select_native_present_mode(
            epochengine::perf::frame_pacing_mode::target_hz,
            {false, false, true});
        const auto presentUnavailable = epochengine::perf::select_native_present_mode(
            epochengine::perf::frame_pacing_mode::uncapped,
            {});
        check(
            "perf.native_present_mode_selection",
            presentVsync.available
                && presentVsync.request_honored
                && presentVsync.pacing_active
                && presentVsync.mode == epochengine::perf::native_present_mode::fifo
                && presentImmediate.available
                && presentImmediate.request_honored
                && !presentImmediate.pacing_active
                && presentImmediate.mode == epochengine::perf::native_present_mode::immediate
                && presentMailbox.available
                && presentMailbox.request_honored
                && !presentMailbox.pacing_active
                && presentMailbox.mode == epochengine::perf::native_present_mode::mailbox
                && presentForcedFifo.available
                && !presentForcedFifo.request_honored
                && presentForcedFifo.pacing_active
                && presentForcedFifo.mode == epochengine::perf::native_present_mode::fifo
                && !presentUnavailable.available);

        const auto vulkanTargetDesired =
            epochengine::perf::resolve_frame_pacing_with_capabilities(
                epochengine::perf::target_hz_policy(120.0),
                epochengine::perf::frame_activity::foreground,
                {true, false});
        const auto vulkanTargetForcedFifo =
            epochengine::perf::finalize_native_frame_pacing(
                vulkanTargetDesired,
                {
                    true,
                    true,
                    epochengine::perf::frame_pacing_mode::vsync,
                    0.0});
        const auto vulkanVsyncPending =
            epochengine::perf::finalize_native_frame_pacing(nativeVsync, {});
        const auto vulkanVsyncAccepted =
            epochengine::perf::finalize_native_frame_pacing(
                nativeVsync,
                {
                    true,
                    true,
                    epochengine::perf::frame_pacing_mode::vsync,
                    0.0});
        check(
            "perf.vulkan_present_mode_lifecycle",
            vulkanTargetDesired.cpu_deadline_wait
                && vulkanTargetForcedFifo.native_pacing_configured
                && vulkanTargetForcedFifo.native_pacing_active
                && vulkanTargetForcedFifo.cpu_deadline_wait
                && !vulkanVsyncPending.native_pacing_configured
                && vulkanVsyncPending.cpu_deadline_wait
                && vulkanVsyncAccepted.native_pacing_configured
                && vulkanVsyncAccepted.native_pacing_active
                && !vulkanVsyncAccepted.cpu_deadline_wait);
        check("forest.config", epochengine::forest::valid(forestProfile.config));
        check("forest.estimate", forestEstimate.nodes > 1u && forestEstimate.branches > 0u);
        check(
            "forest.geometry",
            forestGeometry.segmentCount > 0u
            && forestGeometry.leafCount > 0u
            && forestGeometry.segmentCount <= epochengine::forest::kForestPreviewMaxSegments
            && forestGeometry.leafCount <= epochengine::forest::kForestPreviewMaxLeaves);
        check(
            "forest.activation",
            !previewActivation.includeInGeneratedProject
            && sceneActivation.includeInGeneratedProject
            && sceneActivation.emitPackageManifest
            && sceneActivation.attachToMainScene);

        check(
            "editor.workspace_layout",
            epochengine::editor_workspace::run_contract()
                == epochengine::editor_workspace::ContractFailure::none);
        check(
            "editor.workspace_commands",
            epochengine::editor_workspace_commands::run_aggregate_contract()
                == epochengine::editor_workspace_commands::ContractFailure::none);
        check(
            "editor.code_workspace",
            epochengine::editor_code_workspace::run_contract());
        check(
            "editor.hierarchy_adapter",
            epochengine::editor_hierarchy::run_contract()
                == epochengine::editor_hierarchy::ContractFailure::none);
        check(
            "extension.catalog",
            epochengine::extension_catalog::run_contract()
                == epochengine::extension_catalog::ContractFailure::none);

#if EPOCH_ENABLE_NATIVE_EXTENSIONS
        check(
            "extension.plugin.admission",
            epochengine::extensions::run_contract()
                == epochengine::extensions::ContractFailure::none);
#endif
        check(
            "gui.runtime_surface_atlas",
            epochengine::gui::run_runtime_surface_contract());
        const epochengine::gui::DockGuideLayout guideLayout =
            epochengine::gui::make_dock_guide_layout(
                epochengine::gui::DockGuideOptions{
                    .guide_bounds = { { 0.0f, 0.0f }, { 600.0f, 400.0f } },
                    .left_tabs_preview = { { 0.0f, 40.0f }, { 160.0f, 360.0f } },
                    .right_tabs_preview = { { 440.0f, 40.0f }, { 160.0f, 360.0f } },
                    .bottom_left_tabs_preview = { { 0.0f, 300.0f }, { 300.0f, 100.0f } },
                    .bottom_right_tabs_preview = { { 300.0f, 300.0f }, { 300.0f, 100.0f } },
                    .left_context_preview = { { 0.0f, 40.0f }, { 300.0f, 360.0f } },
                    .right_context_preview = { { 300.0f, 40.0f }, { 300.0f, 360.0f } },
                    .floating_preview = { { 180.0f, 90.0f }, { 240.0f, 220.0f } },
                    .pointer = { 210.0f, 200.0f },
                    .guide_extent = 94.0f,
                    .guide_gap = 8.0f,
                    .allow_side_tabs = true,
                    .allow_bottom_tabs = true,
                    .allow_contexts = true,
                    .allow_float = true
                });
        check(
            "gui.dock_guide_selection",
            guideLayout.count == 7U
            && guideLayout.hovered_target == epochengine::gui::DockGuideTarget::left_tabs
            && guideLayout.hovered_preview.position.x == 0.0f
            && guideLayout.hovered_preview.size.x == 160.0f);
        const epochengine::gui::DockGuideLayout contextGuideLayout =
            epochengine::gui::make_dock_guide_layout(
                epochengine::gui::DockGuideOptions{
                    .guide_bounds = { { 0.0f, 0.0f }, { 800.0f, 400.0f } },
                    .left_context_preview = { { 0.0f, 0.0f }, { 400.0f, 400.0f } },
                    .right_context_preview = { { 400.0f, 0.0f }, { 400.0f, 400.0f } },
                    .pointer = { 200.0f, 200.0f },
                    .guide_extent = 94.0f,
                    .guide_gap = 8.0f,
                    .allow_side_tabs = false,
                    .allow_bottom_tabs = false,
                    .allow_contexts = true,
                    .allow_float = false,
                    .center_context_guides_in_previews = true
                });
        check(
            "gui.context_dock_guide_projection",
            contextGuideLayout.count == 2U
            && contextGuideLayout.hovered_target
                == epochengine::gui::DockGuideTarget::left_context
            && contextGuideLayout.guides[0].target_bounds.position.x == 153.0f
            && contextGuideLayout.guides[0].target_bounds.position.y == 153.0f
            && contextGuideLayout.guides[1].target_bounds.position.x == 553.0f
            && contextGuideLayout.guides[1].target_bounds.position.y == 153.0f
            && contextGuideLayout.hovered_preview.size.x == 400.0f);
        check(
            "ai.assistant_reply_normalization",
            epochengine::ai::normalize_assistant_reply(
                "<think>private draft</think><final>Final answer.</final>")
                == "Final answer."
            && epochengine::ai::normalize_assistant_reply(
                "<reasoning>unterminated private draft").empty());
        check(
            "ai.direct_llama_reply_normalization",
            epochengine::ai::normalize_direct_llama_cpp_reply(
                "User:\nready\n\nAssistant:\n"
                "[Start thinking]\nprivate draft\n[End thinking]\n"
                "Ready.\n\nExiting...\n",
                "ready")
                == "Ready."
            && epochengine::ai::normalize_direct_llama_cpp_source_reply(
                "EPOCH_SOURCE_EVIDENCE_INSUFFICIENT_V1\n",
                "large source prompt")
                == "EPOCH_SOURCE_EVIDENCE_INSUFFICIENT_V1"
            && epochengine::ai::normalize_direct_llama_cpp_source_reply(
                "EPOCH_SOURCE_EVIDENCE_INSUFFICIENT_V1\n"
                "EPOCH_SOURCE_PATCH_PROPOSAL_V1\n"
                "end_proposal\n"
                "Trailing model commentary must stay outside the packet.\n",
                "large source prompt")
                == "EPOCH_SOURCE_PATCH_PROPOSAL_V1\n"
                   "end_proposal"
            && epochengine::ai::normalize_direct_llama_cpp_source_reply(
                "Assistant:\nEPOCH_SOURCE_EVIDENCE_INSUFFICIENT_V1\n",
                "large source prompt")
                == "EPOCH_SOURCE_EVIDENCE_INSUFFICIENT_V1"
            && epochengine::ai::normalize_direct_llama_cpp_source_reply(
                "I will return the packet now.\r\n```text\r\n"
                "EPOCH_SOURCE_EVIDENCE_INSUFFICIENT_V1\r\n```\r\n",
                "large source prompt")
                == "EPOCH_SOURCE_EVIDENCE_INSUFFICIENT_V1"
            && epochengine::ai::normalize_direct_llama_cpp_source_reply(
                "Preamble EPOCH_SOURCE_EVIDENCE_INSUFFICIENT_V1\n",
                "large source prompt").empty()
            && epochengine::ai::normalize_direct_llama_cpp_source_reply(
                "User:\nlarge source prompt\n\nAssistant:\n"
                "Plain explanatory prose.\n",
                "large source prompt").empty()
            && epochengine::ai::normalize_direct_llama_cpp_source_reply(
                "User:\nlarge source prompt\n\nAssistant:\n"
                "I will return the packet now.\n"
                "EPOCH_SOURCE_EVIDENCE_INSUFFICIENT_V1\n",
                "large source prompt")
                == "EPOCH_SOURCE_EVIDENCE_INSUFFICIENT_V1"
            && epochengine::ai::normalize_direct_llama_cpp_reply(
                "EPOCH_SOURCE_EVIDENCE_INSUFFICIENT_V1\n",
                "large source prompt")
                .empty());
        check(
            "ai.direct_llama_prompt_file_transport",
            epochengine::ai::direct_llama_cpp_prompt_transport_contract());
        check("ai.mcp.tool_protocol", epochengine::ai::run_mcp_contract());
        check(
            "ai.epoch_local_install",
            epochengine::ai::epoch_local_ai_install_contract());
        check(
            "ai.development_proposal_codec",
            epochengine::ai::development_proposal_codec::run_contract());
        check(
            "ai.project_profile",
            epochengine::ai::project_profile::run_contract());
        check(
            "ai.curated_context_bundle",
            epochengine::ai::curated_context_bundle::run_contract()
                == epochengine::ai::curated_context_bundle::ContractFailure::none);
        check(
            "ai.iteration_loop",
            epochengine::ai::iteration::run_contract());
        check(
            "ai.iteration_session",
            epochengine::ai::iteration_session::run_contract());
        check(
            "ai.iteration_campaign",
            epochengine::ai::iteration_campaign::run_contract());
        check(
            "ai.mcp_campaign",
            epochengine::ai::mcp_campaign::run_contract());
        check(
            "ai.mcp_child_host",
            epochengine::ai::mcp_child_host::run_contract());
        check(
            "ai.self_iteration_orchestrator",
            epochengine::ai::self_iteration_orchestrator::run_contract());
        check(
            "ai.mcp_orchestrator_bridge",
            epochengine::ai::mcp_orchestrator_bridge::run_contract());
        check(
            "ai.source_patch_bundle",
            epochengine::ai::source_patch_bundle::run_contract());
        check(
            "ai.iteration_patch_adapter",
            epochengine::ai::iteration_patch_adapter::run_contract());
        check(
            "ai.iteration_validation_adapter",
            epochengine::ai::iteration_validation_adapter::run_contract());
        check(
            "epoch.build_validation",
            epochengine::build_validation::run_contract()
                == epochengine::build_validation::ContractFailure::none);
        check(
            "ai.development_guard",
            epochengine::editor_ai_development::run_contract());
        check(
            "ai.development_panel",
            epochengine::editor_ai_development_panel::Panel::run_contract());
        check(
            "authoring.gui_document",
            epochengine::authoring::gui::run_contract()
                == epochengine::authoring::gui::ContractFailure::none);
        check(
            "authoring.gui_compiler",
            epochengine::authoring::gui::run_compiler_contract()
                == epochengine::authoring::gui::CompilerContractFailure::none);
        check(
            "project.gui_library",
            epochengine::project_gui::run_library_contract()
                == epochengine::project_gui::LibraryContractFailure::none);
        check(
            "project.gui_runtime",
            epochengine::project_gui_runtime::run_contract()
                == epochengine::project_gui_runtime::ContractFailure::none);
        check(
            "authoring.task_graph",
            epochengine::authoring::task_graph::run_contract()
                == epochengine::authoring::task_graph::ContractFailure::none);
        check(
            "editor.systems_workspace",
            epochengine::editor_systems::run_contract()
                == epochengine::editor_systems::ContractFailure::none);

        const auto morphologyContract =
            epochengine::authoring::morphology::run_contract();
        check(
            "authoring.morphology.domains",
            morphologyContract.plant
            && morphologyContract.vascular
            && morphologyContract.respiratory
            && morphologyContract.electrical);
        check(
            "authoring.morphology.temporal_lod",
            morphologyContract.deterministic
            && morphologyContract.temporal
            && morphologyContract.voxel_lod);
        const auto forestMorphology =
            epochengine::forest::build_morphology_graph(forestProfile);
        const auto forestMorphologyLods =
            epochengine::forest::plan_morphology_lods(forestMorphology);
        check(
            "forest.morphology_adapter",
            static_cast<bool>(epochengine::authoring::morphology::validate(forestMorphology))
            && forestMorphology.content_hash != 0u
            && !forestMorphologyLods.levels.empty()
            && epochengine::voxel::morphology_semantics(forestMorphologyLods.semantics));
        auto forestDocument = epochengine::forest::make_forest_asset_document(
            "Contract Tree", epochengine::forest::ForestPreset::Tree);
        const auto forestInitialHash = forestDocument.contentHash;
        const bool forestEdited = epochengine::forest::apply_forest_profile_edit(
            forestDocument,
            epochengine::forest::ForestProfileProperty::TargetHeightMeters,
            forestDocument.profile.config.targetHeightMeters + 1.0f);
        const auto forestEditedHash = forestDocument.contentHash;
        const bool forestUndone =
            epochengine::forest::undo_forest_profile_edit(forestDocument);
        const bool forestRedone =
            epochengine::forest::redo_forest_profile_edit(forestDocument);
        const auto compiledForest =
            epochengine::forest::compile_forest_asset(forestDocument);
        check(
            "forest.temporal_document",
            forestEdited
            && forestUndone
            && forestRedone
            && forestInitialHash != forestEditedHash
            && forestDocument.contentHash == forestEditedHash
            && forestDocument.historyCursor == forestDocument.journal.size()
            && compiledForest.valid
            && compiledForest.sourceRevision == forestDocument.revision
            && compiledForest.sourceContentHash == forestDocument.contentHash
            && compiledForest.graph.content_hash != 0u
            && !compiledForest.sample.segments.empty()
            && !compiledForest.voxelLods.levels.empty()
            && compiledForest.voxelLods.source_content_hash ==
                compiledForest.graph.content_hash
            && compiledForest.voxelOccupancy.activeCells > 0u);

        auto plantLabDocument =
            epochengine::forest::make_default_plant_lab_document();
        const auto plantLabDefaultHistory = plantLabDocument.journal.size();
        const auto plantLabDefaultHash = plantLabDocument.contentHash;
        const auto plantLabDefaultCompiled =
            epochengine::forest::compile_forest_asset(plantLabDocument);
        const bool plantLabEdited =
            epochengine::forest::apply_forest_profile_edit(
                plantLabDocument,
                epochengine::forest::ForestProfileProperty::BranchStartHeightMeters,
                plantLabDocument.profile.branch.startHeightMeters + 0.11f);
        const auto plantLabEditedHash = plantLabDocument.contentHash;
        const bool plantLabUndone =
            epochengine::forest::undo_forest_profile_edit(plantLabDocument);
        const bool plantLabRedone =
            epochengine::forest::redo_forest_profile_edit(plantLabDocument);
        const auto plantLabEditedCompiled =
            epochengine::forest::compile_forest_asset(plantLabDocument);
        check(
            "forest.plant_lab_compiler",
            plantLabDefaultHistory == 5u
            && plantLabDefaultHash != 0u
            && plantLabDefaultCompiled.valid
            && !plantLabDefaultCompiled.preview.segments.empty()
            && !plantLabDefaultCompiled.preview.leaves.empty()
            && !plantLabDefaultCompiled.voxelLods.levels.empty()
            && plantLabDefaultCompiled.voxelOccupancy.activeCells > 0u
            && plantLabEdited
            && plantLabUndone
            && plantLabRedone
            && plantLabEditedHash != plantLabDefaultHash
            && plantLabDocument.contentHash == plantLabEditedHash
            && plantLabDocument.historyCursor == plantLabDocument.journal.size()
            && plantLabEditedCompiled.valid
            && plantLabEditedCompiled.sourceRevision == plantLabDocument.revision
            && plantLabEditedCompiled.sourceContentHash == plantLabEditedHash
            && plantLabEditedCompiled.graph.content_hash !=
                plantLabDefaultCompiled.graph.content_hash
            && plantLabEditedCompiled.voxelLods.source_content_hash ==
                plantLabEditedCompiled.graph.content_hash);
        const auto forestLibraryContract =
            epochengine::project_forests::
                project_forest_library_contract_failure();
        check(
            std::string{"project.forest_library."}
                + std::string{epochengine::project_forests::
                    forest_library_contract_failure_name(forestLibraryContract)},
            forestLibraryContract ==
                epochengine::project_forests::ForestLibraryContractFailure::none);
        const auto packageValidation = epochengine::package_registry::validate_registry();
        const auto* forestPackage = epochengine::package_registry::find(epochengine::package_registry::kEngineForestFactoryPackageId);
        const auto* bonsaiPackage = epochengine::package_registry::find(epochengine::package_registry::recommended_local_image_model_id());
        const auto* qwenPackage = epochengine::package_registry::find(epochengine::package_registry::kQwenCoderPackageId);
        const auto* nemotronPackage = epochengine::package_registry::find(epochengine::package_registry::kNemotronNanoPackageId);
        check(
            "package.registry",
            packageValidation.ok
            && packageValidation.packageCount == epochengine::package_registry::known_packages().size()
            && packageValidation.duplicateIdCount == 0u
            && packageValidation.modelAssetCount >= 5u
            && packageValidation.networkSensitiveCount >= 2u);
        check(
            "package.forest_factory",
            forestPackage != nullptr
            && forestPackage->kind == epochengine::package_registry::PackageKind::CoreOptIn
            && forestPackage->activation == epochengine::package_registry::ActivationMode::MainSceneUse
            && epochengine::package_registry::is_core_opt_in(forestPackage->id)
            && epochengine::package_registry::ships_in_core_without_default_project_payload(forestPackage->id)
            && epochengine::package_registry::external_source_repo(forestPackage->id).find("EpochEngineExtensions") != std::string_view::npos);
        check(
            "package.os_models",
            bonsaiPackage != nullptr
            && qwenPackage != nullptr
            && nemotronPackage != nullptr
            && epochengine::package_registry::is_model_asset(bonsaiPackage->id)
            && epochengine::package_registry::is_model_asset(qwenPackage->id)
            && epochengine::package_registry::is_model_asset(nemotronPackage->id)
            && epochengine::package_registry::must_use_human_build_gate(bonsaiPackage->id)
            && epochengine::package_registry::activation_mode_name(bonsaiPackage->activation) == std::string_view{ "Model download opt-in" });
        check(
            "package.network_gates",
            epochengine::package_registry::requires_explicit_network_approval(epochengine::package_registry::kEngineAuthoritativeServerPackageId)
            && epochengine::package_registry::can_create_server_or_listener_after_approval(epochengine::package_registry::kEngineListenServerPackageId)
            && epochengine::package_registry::must_use_human_build_gate("missing_package"));
        const auto arcadePayloadRequirement =
            epochengine::package_registry::payload_requirement(
                epochengine::package_registry::kEngineArcadePackageId);
        const auto arcadeMissingEvidence =
            epochengine::package_registry::evaluate_payload_activation(
                arcadePayloadRequirement,
                {});
        const epochengine::package_registry::PackagePayloadEvidence arcadePayloadEvidence{
            .packageId = epochengine::package_registry::kEngineArcadePackageId,
            .sourceRepo = epochengine::package_registry::kEngineArcadeOptionalAssetRepo,
            .immutableRevision = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
            .contentHash = "sha256:bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
            .licenseEvidence = "THIRD_PARTY_NOTICES.txt",
            .buildTestEvidence = "engine-contract-self-test",
            .approvedContentHash = "sha256:bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
            .immutableRevisionVerified = true,
            .contentHashVerified = true,
            .licenseEvidenceVerified = true,
            .buildTestEvidenceVerified = true,
            .humanApproved = true,
            .containsNativeCode = false,
            .nativeCodeApproved = false
        };
        const auto arcadeVerifiedPayload =
            epochengine::package_registry::evaluate_payload_activation(
                arcadePayloadRequirement,
                arcadePayloadEvidence);
        check(
            "package.extension_payload_evidence",
            !arcadeMissingEvidence.payloadAllowed
            && arcadeMissingEvidence.coreFallbackAvailable
            && arcadeMissingEvidence.reason ==
                epochengine::package_registry::PayloadRejectReason::MissingEvidence
            && arcadeVerifiedPayload.payloadAllowed
            && arcadeVerifiedPayload.coreFallbackAvailable);

        epochengine::lighting::LightManager lightManager{2};
        lightManager.set_environment({{0.10f, 0.10f, 0.10f}});
        const auto invalidLight = lightManager.create({.intensity = -1.0f});
        const auto invalidKindLight = lightManager.create({
            .kind = static_cast<epochengine::lighting::LightKind>(255)
        });
        const auto directionalLight = lightManager.create({
            .kind = epochengine::lighting::LightKind::Directional,
            .color = {1.0f, 0.8f, 0.6f},
            .intensity = 2.0f,
            .direction = {0.0f, -1.0f, 0.0f}
        });
        const auto pointLight = lightManager.create({
            .kind = epochengine::lighting::LightKind::Point,
            .color = {0.2f, 0.4f, 1.0f},
            .intensity = 4.0f,
            .position = {0.0f, 2.0f, 0.0f},
            .range = 8.0f
        });
        const auto rejectedLight = lightManager.create({});
        const auto lightingFrame = lightManager.build_frame();
        const auto lightingTerms =
            epochengine::lighting::evaluate_reference_raster_lighting(
                lightingFrame,
                {
                    .position = {},
                    .normal = {0.0f, 1.0f, 0.0f},
                    .viewDirection = {0.0f, 1.0f, 1.0f},
                    .albedo = {0.8f, 0.7f, 0.6f},
                    .specularColor = {0.5f, 0.5f, 0.5f},
                    .shininess = 32.0f
                });
        const bool destroyedDirectional = lightManager.destroy(directionalLight);
        const auto replacementLight = lightManager.create({});
        lightManager.set_environment({{
            (std::numeric_limits<float>::infinity)(),
            -1.0f,
            0.25f
        }});
        const auto sanitizedEnvironment = lightManager.environment();
        check(
            "render.lighting.registry_generation",
            !invalidLight.valid()
            && !invalidKindLight.valid()
            && directionalLight.valid()
            && pointLight.valid()
            && !rejectedLight.valid()
            && lightingFrame.lights.size() == 2u
            && destroyedDirectional
            && !lightManager.contains(directionalLight)
            && replacementLight.valid()
            && replacementLight.index == directionalLight.index
            && replacementLight.generation != directionalLight.generation
            && sanitizedEnvironment.ambient.r == 0.0f
            && sanitizedEnvironment.ambient.g == 0.0f
            && sanitizedEnvironment.ambient.b == 0.25f);
        check(
            "render.lighting.reference_terms",
            lightingTerms.evaluatedLights == 2u
            && lightingTerms.ambient.r > 0.0f
            && lightingTerms.diffuse.r > 0.0f
            && lightingTerms.combined.r >= lightingTerms.ambient.r);

        epochengine::ray::RayScene rayScene{};
        const auto spherePrimitive = rayScene.create({
            .kind = epochengine::ray::PrimitiveKind::Sphere,
            .sphere = {{0.0f, 0.0f, 5.0f}, 1.0f},
            .object = {42u, 1u},
            .primitiveIndex = 7u,
            .materialId = 9u
        });
        const auto rayHit = rayScene.trace({
            .origin = {},
            .direction = {0.0f, 0.0f, 1.0f},
            .minimumDistance = 0.0f,
            .maximumDistance = 100.0f
        });
        const auto rayMiss = rayScene.trace({
            .origin = {},
            .direction = {0.0f, 1.0f, 0.0f},
            .minimumDistance = 0.0f,
            .maximumDistance = 100.0f
        });
        const auto rayInvalid = rayScene.trace({.direction = {}});
        const auto insideAabbHit = epochengine::ray::intersect(
            {
                .origin = {},
                .direction = {1.0f, 0.0f, 0.0f},
                .minimumDistance = 0.0f,
                .maximumDistance = 10.0f
            },
            epochengine::ray::Aabb{{-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, 1.0f}});
        const auto triangleHit = epochengine::ray::intersect(
            {
                .origin = {0.0f, 0.0f, -1.0f},
                .direction = {0.0f, 0.0f, 1.0f},
                .minimumDistance = 0.0f,
                .maximumDistance = 10.0f
            },
            epochengine::ray::Triangle{
                {-1.0f, -1.0f, 0.0f},
                {1.0f, -1.0f, 0.0f},
                {0.0f, 1.0f, 0.0f}
            });
        const auto voxelHit = epochengine::ray::trace_voxels(
            {
                .origin = {0.5f, 0.5f, 0.5f},
                .direction = {1.0f, 0.0f, 0.0f},
                .minimumDistance = 0.0f,
                .maximumDistance = 10.0f
            },
            {.cellSize = 1.0f, .maximumSteps = 32u},
            [](epochengine::ray::VoxelCoord cell)
            {
                return cell.x == 2 && cell.y == 0 && cell.z == 0;
            });
        const auto diagonalVoxelHit = epochengine::ray::trace_voxels(
            {
                .origin = {0.5f, 0.5f, 0.5f},
                .direction = {1.0f, 1.0f, 0.0f},
                .minimumDistance = 0.0f,
                .maximumDistance = 10.0f
            },
            {.cellSize = 1.0f, .maximumSteps = 32u},
            [](epochengine::ray::VoxelCoord cell)
            {
                return (cell.x == 1 && cell.y == 0 && cell.z == 0) ||
                       (cell.x == 1 && cell.y == 1 && cell.z == 0);
            });
        const auto boundedVoxelMiss = epochengine::ray::trace_voxels(
            {
                .origin = {0.5f, 0.5f, 0.5f},
                .direction = {1.0f, 0.0f, 0.0f},
                .minimumDistance = 0.0f,
                .maximumDistance = 0.1f
            },
            {.cellSize = 1.0f, .maximumSteps = 32u},
            [](epochengine::ray::VoxelCoord) { return false; });
        const auto outOfRangeVoxel = epochengine::ray::trace_voxels(
            {
                .origin = {(std::numeric_limits<float>::max)(), 0.0f, 0.0f},
                .direction = {1.0f, 0.0f, 0.0f},
                .minimumDistance = 0.0f,
                .maximumDistance = 1.0f
            },
            {.cellSize = 0.001f, .maximumSteps = 32u},
            [](epochengine::ray::VoxelCoord) { return false; });
        const bool destroyedSphere = rayScene.destroy(spherePrimitive);
        check(
            "render.ray.query_status_and_identity",
            spherePrimitive.valid()
            && rayHit.status == epochengine::ray::QueryStatus::Hit
            && rayHit.hit.object == epochengine::ray::QueryObjectId{42u, 1u}
            && rayHit.hit.primitiveIndex == 7u
            && rayHit.hit.materialId == 9u
            && std::abs(rayHit.hit.distance - 4.0f) < 0.001f
            && rayMiss.status == epochengine::ray::QueryStatus::Miss
            && rayInvalid.status == epochengine::ray::QueryStatus::Invalid
            && insideAabbHit.has_value()
            && std::abs(insideAabbHit->distance - 1.0f) < 0.001f
            && insideAabbHit->geometricNormal == epochengine::ray::Vec3{1.0f, 0.0f, 0.0f}
            && triangleHit.has_value()
            && std::abs(triangleHit->distance - 1.0f) < 0.001f
            && destroyedSphere
            && !rayScene.get(spherePrimitive).has_value());

        epochengine::ecs::world lifetimeWorld{
            epochengine::ecs::world_desc{ .max_entities = 2u, .allow_growth = false }};
        const epochengine::ecs::entity firstLifetime = lifetimeWorld.create();
        lifetimeWorld.destroy(firstLifetime);
        const epochengine::ecs::entity replacementLifetime = lifetimeWorld.create();
        check(
            "ecs.generation_checked_lifetime",
            firstLifetime.valid()
            && firstLifetime.generation != 0u
            && !lifetimeWorld.alive(firstLifetime)
            && replacementLifetime.index == firstLifetime.index
            && replacementLifetime.generation != 0u
            && replacementLifetime.generation != firstLifetime.generation
            && lifetimeWorld.alive(replacementLifetime));

        constexpr epochengine::authoring::DocumentHandle authoringHandle{7u, 1u};
        constexpr epochengine::authoring::ContentHash authoringHash =
            epochengine::authoring::deterministic_content_hash("epoch.scene.tier0.contract");
        constexpr epochengine::authoring::DocumentRevision authoringRevision{
            .content = authoringHash,
            .sequence = 1u };
        constexpr epochengine::authoring::HistoryPolicy authoringHistory{};
        check(
            "authoring.document.identity_revision_history",
            epochengine::authoring::validate(authoringHandle)
            && epochengine::authoring::validate(epochengine::authoring::DocumentKind::scene)
            && epochengine::authoring::validate(authoringRevision)
            && epochengine::authoring::validate(authoringHistory)
            && epochengine::authoring::content_hash_from_hex(
                epochengine::authoring::content_hash_hex(authoringHash).data()) == authoringHash);
        const auto sceneInteractionContract =
            epochengine::scene_interaction::run_scene_interaction_contract_checks();
        check("scene.interaction.persistent_identity_and_revision",
            static_cast<bool>(sceneInteractionContract));
        const auto sceneDocumentBuild =
            epochengine::authoring::scene::SceneDocument::make_default_tier0();
        const auto sceneDocumentProjection = sceneDocumentBuild
            ? sceneDocumentBuild.value.project_snapshot()
            : epochengine::authoring::scene::SnapshotProjectionResult{};
        check(
            "scene.document.tier0_projection",
            sceneDocumentBuild
            && sceneDocumentProjection
            && sceneDocumentProjection.snapshot.objects.size() == 4u
            && sceneDocumentProjection.snapshot.primary_camera != epochengine::scene::kInvalidSceneObjectId
            && sceneDocumentProjection.snapshot.primary_spawn != epochengine::scene::kInvalidSceneObjectId
            && epochengine::scene::validate_scene_document(
                sceneDocumentProjection.snapshot,
                epochengine::scene::SceneDocumentRequirement::runnable));
        check("scene.persistence.validation_and_atomic_plan",
            epochengine::scene::persistence::scene_persistence_contract_checks().all_passed());
        check("scene.runtime.compiled_projection",
            static_cast<bool>(epochengine::scene_runtime::run_scene_runtime_contract_checks()));
        check(
            "render.ray.voxel_dda",
            voxelHit.hit
            && voxelHit.cell == epochengine::ray::VoxelCoord{2, 0, 0}
            && voxelHit.distance >= 1.0f
            && voxelHit.steps <= 4u
            && diagonalVoxelHit.hit
            && diagonalVoxelHit.cell == epochengine::ray::VoxelCoord{1, 1, 0}
            && !boundedVoxelMiss.hit
            && boundedVoxelMiss.steps == 1u
            && !outOfRangeVoxel.hit
            && outOfRangeVoxel.steps == 0u);

        epochengine::voxel::SparseVoxelField voxelField{{4u, 4u, 4u, 1.0f, 0u}};
        const epochengine::voxel::CellCoord negativeVoxel{-1, 0, 0};
        const bool voxelWrite = voxelField.write(
            negativeVoxel,
            {
                .density = 1.0f,
                .material = 7u,
                .semantics = epochengine::voxel::CellSemantic::Geometry
            });
        const auto voxelSnapshot = voxelField.snapshot();
        const auto voxelMetrics = voxelField.metrics();
        check(
            "voxel.sparse_storage",
            voxelWrite
            && voxelField.occupied(negativeVoxel)
            && voxelField.read(negativeVoxel)->material == 7u
            && voxelSnapshot.cells.size() == 1u
            && voxelSnapshot.contentHash != 0u
            && voxelMetrics.allocatedChunkCount == 1u
            && voxelMetrics.storedCellCount == 1u
            && voxelMetrics.denseEquivalentCellCount == 64u);

        epochengine::voxel::SparseVoxelField boundedVoxelField(
            {2u, 2u, 2u, 1.0f, 0u},
            {.maximumChunks = 1u, .maximumStoredCells = 1u, .maximumApproximateBytes = 4'096u});
        const epochengine::voxel::VoxelCell boundedCell{
            .density = 1.0f,
            .material = 3u,
            .semantics = epochengine::voxel::CellSemantic::Geometry
        };
        const auto firstBoundedWrite = boundedVoxelField.write_status({0, 0, 0}, boundedCell);
        const std::uint64_t boundedRevision = boundedVoxelField.revision();
        const auto unchangedBoundedWrite = boundedVoxelField.write_status({0, 0, 0}, boundedCell);
        const auto rejectedBoundedWrite = boundedVoxelField.write_status({2, 0, 0}, boundedCell);
        const epochengine::voxel::SparseVoxelField differentLayoutVoxelField{
            {8u, 2u, 2u, 0.5f, 1u}};
        check(
            "voxel.bounds_and_content_identity",
            !epochengine::voxel::valid(epochengine::voxel::ChunkDesc{0x80000000u, 2u, 2u, 1.0f, 0u})
            && firstBoundedWrite == epochengine::voxel::VoxelWriteStatus::Applied
            && unchangedBoundedWrite == epochengine::voxel::VoxelWriteStatus::Unchanged
            && rejectedBoundedWrite == epochengine::voxel::VoxelWriteStatus::BudgetExceeded
            && boundedVoxelField.revision() == boundedRevision
            && boundedVoxelField.metrics().storedCellCount == 1u
            && epochengine::voxel::SparseVoxelField{}.snapshot().contentHash !=
                differentLayoutVoxelField.snapshot().contentHash);

        epochengine::water::WaterManager waterManager{};
        const auto waterBody = waterManager.create({
            .kind = epochengine::water::WaterBodyKind::BoxVolume,
            .center = {},
            .halfExtents = {2.0f, 2.0f, 2.0f},
            .surfaceHeight = 0.5f,
            .waveAmplitude = 0.0f,
            .material = 11u
        });
        const auto waterSample = waterManager.sample(waterBody, {
            .position = {0.0f, 0.0f, 0.0f},
            .simulationTimeSeconds = 4.0
        });
        const auto overlappingWaterBody = waterManager.create({
            .kind = epochengine::water::WaterBodyKind::BoxVolume,
            .center = {},
            .halfExtents = {2.0f, 2.0f, 2.0f},
            .surfaceHeight = 1.0f,
            .waveAmplitude = 0.0f,
            .material = 12u
        });
        epochengine::voxel::SparseVoxelField waterField{{4u, 4u, 4u, 1.0f, 0u}};
        const auto waterStamp = epochengine::water::stamp_voxel_water(
            waterManager,
            {
                .body = waterBody,
                .minimum = {-1, -1, -1},
                .maximum = {1, 0, 1},
                .simulationTimeSeconds = 4.0,
                .maximumCells = 64u
            },
            waterField);
        check(
            "water.temporal_query_and_voxel_coupling",
            waterBody.valid()
            && overlappingWaterBody.valid()
            && waterSample.found
            && waterSample.submerged
            && std::abs(waterSample.surfaceHeight - 0.5f) < 0.001f
            && waterStamp.applied
            && waterStamp.bounded
            && waterStamp.waterCells > 0u
            && waterField.metrics().waterCellCount == waterStamp.waterCells);

        epochengine::physics::PhysicsManager physicsManager{};
        const auto physicsTime = physicsManager.current_time();
        const auto physicsBody = physicsManager.create_body(
            {
                .motion = epochengine::physics::BodyMotionType::dynamic_body,
                .mass_kilograms = 2.0,
                .stable_user_id = 9001u
            },
            {},
            physicsTime);
        auto physicsNext = physicsTime;
        ++physicsNext.tick;
        const auto physicsCommand = physicsManager.enqueue_set_linear_velocity(
            physicsBody.body,
            {1.0, 2.0, 3.0},
            physicsNext);
        const auto physicsAdvance = physicsManager.advance({
            .from = physicsTime,
            .to = physicsNext,
            .direction = epochengine::physics::TemporalDirection::forward
        });
        const auto physicsSnapshot = physicsManager.snapshot();
        auto malformedPhysicsSnapshot = physicsSnapshot;
        auto malformedPhysicsTime = physicsNext;
        ++malformedPhysicsTime.tick;
        malformedPhysicsSnapshot.pending_commands.push_back({
            .kind = epochengine::physics::BodyCommandKind::set_linear_velocity,
            .body = physicsBody.body,
            .execute_at = malformedPhysicsTime,
            .vector = {(std::numeric_limits<double>::quiet_NaN)(), 0.0, 0.0},
            .sequence = malformedPhysicsSnapshot.next_command_sequence
        });
        ++malformedPhysicsSnapshot.next_command_sequence;
        const auto malformedPhysicsRestore =
            physicsManager.restore(malformedPhysicsSnapshot);
        check(
            "physics.manager_fixed_boundary",
            static_cast<bool>(physicsBody)
            && static_cast<bool>(physicsCommand)
            && static_cast<bool>(physicsAdvance)
            && physicsAdvance.steps_committed == 1u
            && malformedPhysicsRestore == epochengine::physics::ResultCode::invalid_snapshot
            && physicsSnapshot.body_slots.size() == 1u
            && physicsSnapshot.body_slots.front().state.linear_velocity ==
                epochengine::physics::Vector3{1.0, 2.0, 3.0});

        const auto audioContract =
            epochengine::audio::run_audio_manager_contract_tests();
        check(
            "audio.manager_logical_contract",
            audioContract.passed
            && audioContract.checks_completed >= 6u
            && audioContract.failure == epochengine::audio::AudioContractFailure::none);
        check(
            "audio.mixer_pcm_contract",
            epochengine::audio::contract::run_audio_mixer_contract() == 0);
        check(
            "audio.device_ownership_contract",
            epochengine::audio::run_audio_device_contract_tests() == 0);
        check(
            "audio.playback_runtime_contract",
            epochengine::audio::
                run_audio_playback_runtime_contract_tests() == 0);
        check(
            "asset.audio_import_contract",
            epochengine::asset::audio::audio_import_contract_failure()
                == epochengine::asset::audio::
                    AudioImportContractFailure::none);
        check(
            "project.audio_profile_contract",
            epochengine::project_audio::
                project_audio_profile_contract_failure()
                == epochengine::project_audio::ContractFailure::none);
        const auto capabilityContract =
            epochengine::capability::run_contract_checks();
        constexpr std::uint32_t expectedCapabilityChecks = (1u << 18u) - 1u;
        check(
            "capability.profile_selection",
            capabilityContract.passed()
            && capabilityContract.executed == expectedCapabilityChecks);

        const auto portablePolicy =
            epochengine::editor_project_capability_policy("portable");
        const auto strictPolicy =
            epochengine::editor_project_capability_policy("portable-strict");
        const auto unknownPolicy =
            epochengine::editor_project_capability_policy("unknown-policy");
        check(
            "editor.project_capability_policy",
            portablePolicy.has_value()
            && strictPolicy.has_value()
            && !unknownPolicy.has_value()
            && portablePolicy->allow_experimental
            && portablePolicy->allow_software_fallback
            && !strictPolicy->allow_experimental
            && !strictPolicy->allow_software_fallback
            && portablePolicy->renderer_requirement.minimum_tier
                == epochengine::capability::Tier::portable_graphics);
        check(
            "editor.project_manifest_capability",
            epochengine::editor_project_manifest_capability_contract());

        const auto tier0Build = epochengine::scene_tier0::make_default_scene();
        const auto tier0Run = tier0Build
            ? epochengine::scene_tier0::make_run_request(tier0Build.scene)
            : std::nullopt;
        check(
            "scene.tier0_default_project",
            static_cast<bool>(tier0Build)
            && static_cast<bool>(epochengine::scene_tier0::validate(tier0Build.scene))
            && tier0Run.has_value());

#if EPOCH_ENABLE_AUTHORING_PLATFORM && EPOCH_ENABLE_TEXTURE_EDITOR
        const bool textureDocumentReady = []
        {
            using namespace epochengine::authoring::texture;

            CanvasDescriptor canvas{};
            canvas.width = 32u;
            canvas.height = 32u;
            canvas.tile_extent = 16u;
            canvas.mip_count = 1u;
            const BranchIdentity branch{ 1u, 1u };
            TextureDocument document{
                DocumentHandle{ 71u, 1u },
                branch,
                canvas
            };
            const MutationResult created = document.create_layer(
                LayerDescriptor{ .name = "Tier1 Canvas" },
                0u,
                TemporalPoint{ branch, 1 });
            if (!document.valid() || !created || !created.layer)
                return false;

            StrokeDescriptor stroke{};
            stroke.target = created.layer;
            stroke.color = PixelRgba8{ 32u, 160u, 255u, 255u };
            stroke.radius_subpixels = 256u;
            stroke.samples.push_back(StrokeSample{
                .x_subpixels = 8 * 256 + 128,
                .y_subpixels = 8 * 256 + 128
            });
            const MutationResult painted = document.apply_stroke(
                std::move(stroke),
                TemporalPoint{ branch, 2 });
            if (!painted || !document.can_undo())
                return false;

            const MutationResult undone = document.undo(
                TemporalPoint{ branch, 3 });
            const MutationResult redone = document.redo(
                TemporalPoint{ branch, 4 });
            if (!undone || !redone)
                return false;

            const auto artifact = document.compiled_artifact_identity({});
            PhysicalResidencyCapabilities capabilities{};
            PhysicalResidencyPolicy policy{};
            policy.preferred = PhysicalResidencyKind::atlas_region;
            const auto residency = plan_physical_residency(
                artifact,
                capabilities,
                policy);
            const auto metrics = document.metrics();
            return static_cast<bool>(artifact)
                && static_cast<bool>(residency)
                && metrics.active_layers == 1u
                && metrics.sparse_tile_count > 0u
                && document.pixel(created.layer, 0u, 8u, 8u).a > 0u;
        }();
        check("authoring.texture_document", textureDocumentReady);
#endif

        const auto cameraContract =
            epochengine::render_camera::run_contract_checks();
        const auto portalContract =
            epochengine::render_portal::run_portal_contract_checks();
        check("render.camera", cameraContract.passed());
        check("render.portal", portalContract.passed());
        const auto arcadeContract = epochengine::render_arcade::run_contract_checks();
        const auto arcadePreviewRouting =
            epochengine::previewgrid::run_arcade_preview_routing_contract();
        check("render.arcade_scene_contract", arcadeContract.passed());
        check(
            "render.arcade_attract_pattern",
            epochengine::render_arcade::arcade_attract_pattern_contract());
        check("render.arcade_preview_routing", arcadePreviewRouting.passed());

        epochengine::previewgrid::ObjectMarker solidSceneMarker{};
        solidSceneMarker.primitive = epochengine::previewgrid::ObjectPreviewPrimitive::Cube;
        epochengine::previewgrid::ObjectMarker cameraHelperMarker{};
        cameraHelperMarker.primitive = epochengine::previewgrid::ObjectPreviewPrimitive::Camera;
        cameraHelperMarker.editorOnly = true;
        epochengine::previewgrid::ObjectMarker lightHelperMarker{};
        lightHelperMarker.primitive = epochengine::previewgrid::ObjectPreviewPrimitive::Light;
        lightHelperMarker.editorOnly = true;
        epochengine::previewgrid::ObjectMarker canvasMarker{};
        canvasMarker.primitive = epochengine::previewgrid::ObjectPreviewPrimitive::Canvas2D;
        canvasMarker.editorOnly = true;
        check(
            "render.preview_fill_policy",
            epochengine::previewgrid::object_marker_uses_solid_fill(solidSceneMarker)
            && !epochengine::previewgrid::object_marker_uses_solid_fill(cameraHelperMarker)
            && !epochengine::previewgrid::object_marker_uses_solid_fill(lightHelperMarker)
            && epochengine::previewgrid::object_marker_uses_solid_fill(canvasMarker));

        const auto previewGrid = epochengine::previewgrid::grid_geometry_for(nullptr);
        const bool previewGridIndicesValid = previewGrid
            && std::all_of(
                previewGrid->indices.begin(),
                previewGrid->indices.end(),
                [previewGrid](std::uint32_t index) noexcept
                {
                    return index < previewGrid->vertices.size();
                });
        check(
            "render.preview_grid",
            previewGrid
            && previewGrid->signature != 0u
            && previewGrid->spacing >= 0.25f
            && previewGrid->spacing <= 64.0f
            && !previewGrid->vertices.empty()
            && previewGrid->vertices.size() <= 80u
            && previewGrid->indices.size() % 2u == 0u
            && previewGridIndicesValid);
        const auto cubePreviewRoute =
            epochengine::previewgrid::object_preview_geometry_route(
                epochengine::previewgrid::ObjectPreviewPrimitive::Cube);
        const auto cameraPreviewRoute =
            epochengine::previewgrid::object_preview_geometry_route(
                epochengine::previewgrid::ObjectPreviewPrimitive::Camera);
        const auto lightPreviewRoute =
            epochengine::previewgrid::object_preview_geometry_route(
                epochengine::previewgrid::ObjectPreviewPrimitive::Light);
        check(
            "render.preview_geometry_routing",
            cubePreviewRoute.solid_scene
            && !cubePreviewRoute.marker_wire
            && cubePreviewRoute.selection_wire
            && !cubePreviewRoute.sampled_surface
            && !cameraPreviewRoute.solid_scene
            && cameraPreviewRoute.marker_wire
            && !cameraPreviewRoute.selection_wire
            && !cameraPreviewRoute.sampled_surface
            && !lightPreviewRoute.solid_scene
            && lightPreviewRoute.marker_wire
            && !lightPreviewRoute.selection_wire
            && !lightPreviewRoute.sampled_surface);
        const auto forestTrunkRoute =
            epochengine::previewgrid::object_preview_geometry_route(
                epochengine::previewgrid::ObjectPreviewPrimitive::ForestTrunk);
        const auto forestBranchRoute =
            epochengine::previewgrid::object_preview_geometry_route(
                epochengine::previewgrid::ObjectPreviewPrimitive::ForestBranch);
        const auto forestLeafRoute =
            epochengine::previewgrid::object_preview_geometry_route(
                epochengine::previewgrid::ObjectPreviewPrimitive::ForestLeafCluster);
        check(
            "render.forest_preview_routing",
            forestTrunkRoute.solid_scene
            && !forestTrunkRoute.marker_wire
            && forestTrunkRoute.selection_wire
            && !forestTrunkRoute.sampled_surface
            && forestBranchRoute.solid_scene
            && !forestBranchRoute.marker_wire
            && forestBranchRoute.selection_wire
            && !forestBranchRoute.sampled_surface
            && forestLeafRoute.solid_scene
            && !forestLeafRoute.marker_wire
            && forestLeafRoute.selection_wire
            && !forestLeafRoute.sampled_surface);
        int forestProjectionContext{};
        const std::array<epochengine::previewgrid::ObjectMarker, 1> forestProjectionMarkers{{
            epochengine::previewgrid::ObjectMarker{
                .position{ 0.0f, 1.0f, 0.0f },
                .color{ 0.42f, 0.70f, 0.32f },
                .scale{ 0.16f, 1.40f, 0.16f },
                .rotationDegrees{ 35.0f, 0.0f, -42.0f },
                .radius = 0.20f,
                .primitive = epochengine::previewgrid::ObjectPreviewPrimitive::ForestBranch,
                .selected = true
            }}};
        epochengine::previewgrid::set_object_markers(&forestProjectionContext, std::span<const epochengine::previewgrid::ObjectMarker>{ forestProjectionMarkers });
        const std::vector<epochengine::previewgrid::Vertex> forestProjectionSolid =
            epochengine::previewgrid::object_solid_vertices_for(&forestProjectionContext);
        const std::vector<epochengine::previewgrid::Vertex> forestProjectionWire =
            epochengine::previewgrid::object_marker_vertices_for(&forestProjectionContext);
        epochengine::previewgrid::clear_object_markers(&forestProjectionContext);

        float forestProjectionExtentX{};
        float forestProjectionExtentZ{};
        if (!forestProjectionSolid.empty())
        {
            float minimumX = forestProjectionSolid.front().position.x;
            float maximumX = minimumX;
            float minimumZ = forestProjectionSolid.front().position.z;
            float maximumZ = minimumZ;
            for (const auto& vertex : forestProjectionSolid)
            {
                minimumX = (std::min)(minimumX, vertex.position.x);
                maximumX = (std::max)(maximumX, vertex.position.x);
                minimumZ = (std::min)(minimumZ, vertex.position.z);
                maximumZ = (std::max)(maximumZ, vertex.position.z);
            }
            forestProjectionExtentX = maximumX - minimumX;
            forestProjectionExtentZ = maximumZ - minimumZ;
        }
        const epochengine::previewgrid::Vec3 forestProjectedAxis =
            epochengine::previewgrid::rotate_euler_degrees({ 0.0f, 1.0f, 0.0f }, { 35.0f, 0.0f, -42.0f });
        check(
            "render.forest_preview_projection",
            forestProjectionSolid.size() == 36u
            && forestProjectionWire.size() == 24u
            && forestProjectionExtentX > 0.45f
            && forestProjectionExtentZ > 0.35f
            && std::abs(forestProjectedAxis.x) > 0.45f
            && std::abs(forestProjectedAxis.z) > 0.35f);
        check("render.canvas2d.core", epochengine::canvas2d::canvas2d_runtime_contract());
        const auto cpuCanvasContract =
            epochengine::canvas2d::cpu::canvas2d_cpu_runtime_contract_failure();
        check(
            std::string{"render.canvas2d.cpu."}
                + epochengine::canvas2d::cpu::cpu_contract_failure_name(cpuCanvasContract),
            cpuCanvasContract == epochengine::canvas2d::cpu::CpuContractFailure::none);
        const auto canvasLimitsContract =
            epochengine::canvas2d::limits::runtime_contract_failure();
        check(
            std::string{"render.canvas2d.limits."}
                + epochengine::canvas2d::limits::contract_failure_name(canvasLimitsContract),
            canvasLimitsContract
                == epochengine::canvas2d::limits::ContractFailure::none);
        const auto canvasPresentationContract =
            epochengine::canvas2d::presentation::canvas2d_presentation_runtime_contract_failure();
        check(
            std::string{"render.canvas2d.presentation."}
                + epochengine::canvas2d::presentation::presentation_contract_failure_name(canvasPresentationContract),
            canvasPresentationContract == epochengine::canvas2d::presentation::PresentationContractFailure::none);
        const auto canvasSceneContract =
            epochengine::canvas2d_scene_contracts::run();
        check(
            std::string{"render.canvas2d.scene."}
                + std::string{canvasSceneContract.stage},
            canvasSceneContract.passed);
        const auto softwareCanvasContract =
            epochengine::anativecontext::software_canvas2d_backend_contract_failure();
        check(
            std::string{"render.canvas2d.software."}
                + std::string{epochengine::anativecontext::
                    software_canvas2d_contract_failure_name(softwareCanvasContract)},
            softwareCanvasContract ==
                epochengine::anativecontext::SoftwareCanvas2DContractFailure::none);
        const auto projectAssetSpineContract =
            epochengine::project_contracts::run_asset_spine_contract();
        check(
            std::string{"project.asset_spine."}
                + std::string{projectAssetSpineContract.stage},
            projectAssetSpineContract.passed);
        const auto textureImportContract =
            epochengine::asset::texture::texture_import_contract_failure();
        check(
            std::string{"asset.texture_import."}
                + std::string{epochengine::asset::texture::
                    texture_import_contract_failure_name(textureImportContract)},
            textureImportContract ==
                epochengine::asset::texture::TextureImportContractFailure::none);
        const auto projectTextureControllerContract =
            epochengine::editor_project_textures::
                project_texture_controller_contract_failure();
        check(
            std::string{"editor.project_textures."}
                + std::string{epochengine::editor_project_textures::
                    controller_contract_failure_name(projectTextureControllerContract)},
            projectTextureControllerContract ==
                epochengine::editor_project_textures::ControllerContractFailure::none);
#if EPOCH_ENABLE_AUTHORING_PLATFORM && EPOCH_ENABLE_TILEMAP_EDITOR
        const auto tilemapWorkspaceContract =
            epochengine::editor_tilemaps::
                tilemap_workspace_controller_contract_failure();
        check(
            std::string{"editor.tilemap_workspace."}
                + std::string{epochengine::editor_tilemaps::
                    controller_contract_failure_name(tilemapWorkspaceContract)},
            tilemapWorkspaceContract ==
                epochengine::editor_tilemaps::ControllerContractFailure::none);
#endif
        const auto textureResidencyContract =
            epochengine::texture_residency::texture_residency_runtime_contract_failure();
        check(
            std::string{"render.texture.residency."}
                + epochengine::texture_residency::residency_contract_failure_name(textureResidencyContract),
            textureResidencyContract == epochengine::texture_residency::ResidencyContractFailure::none);
#if EPOCH_ENABLE_AUTHORING_PLATFORM && EPOCH_ENABLE_TEXTURE_EDITOR
        const auto textureArtifactTransferContract =
            epochengine::texture_artifact::texture_artifact_transfer_runtime_contract_failure();
        check(
            std::string{"render.texture.artifact."}
                + epochengine::texture_artifact::transfer_contract_failure_name(textureArtifactTransferContract),
            textureArtifactTransferContract == epochengine::texture_artifact::TransferContractFailure::none);
#endif
        check("render.engine_arcade_screen_graph", engine_arcade_screen_graph_contract_ready());
        check("render.render_surface_requires_rtt_asset", render_surface_requires_render_texture_asset_contract_ready());
        check("render.render_surface_rejects_mismatched_sampler", render_surface_rejects_mismatched_sampler_contract_ready());
        check("render.opengl_family_arcade_screen_graph", opengl_family_arcade_screen_graph_contract_ready());
        check("render.opengl_family_arcade_cabinet_graph", opengl_family_arcade_cabinet_graph_contract_ready());
        check("render.opengl_family_arcade_native_requirements", opengl_family_arcade_native_requirements_contract_ready());
        check("render.opengl_family_arcade_fake_native_rtt", opengl_family_arcade_fake_native_rtt_contract_ready());
        const auto openGLFamilyTextureContract =
            epochengine::opengl_family_texture_runtime_contract_failure();
        check(
            std::string{"render.opengl_family_texture."}
                + epochengine::opengl_family_texture_contract_failure_name(openGLFamilyTextureContract),
            openGLFamilyTextureContract == epochengine::OpenGLFamilyTextureContractFailure::none);
        check("render.sampled_surface_preview_marker", sampled_render_surface_preview_marker_contract_ready());
        check("render.capability_report_layers", renderer_capability_report_contract_ready());
        check("render.opengl_real_native_rtt_hook", opengl_real_native_rtt_hook_contract_ready());
        check("render.opengl_real_native_texture_hook", opengl_real_native_texture_hook_contract_ready());
#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)
        check("render.opengl_canvas2d_no_context_refusal",
            epochengine::openglcanvas2d::no_context_runtime_contract());
#endif
        check("render.sdl_native_render_texture_device", sdl_native_render_texture_device_contract_ready());
        check("render.sfml_native_render_texture_device", sfml_native_render_texture_device_contract_ready());
        check("render.raylib_native_render_texture_device", raylib_native_render_texture_device_contract_ready());
        check("render.sdl_arcade_cabinet_graph", sdl_arcade_cabinet_graph_contract_ready());
        check("render.sfml_arcade_cabinet_graph", sfml_arcade_cabinet_graph_contract_ready());
        check("render.raylib_arcade_cabinet_graph", raylib_arcade_cabinet_graph_contract_ready());
        check("render.raylib_texture_storage", raylib_texture_storage_contract_ready());
        check("render.atlas_snapshot_upload", atlas_snapshot_upload_contract_ready());

        epochengine::saveload::StreamingSaveConfig saveConfig{};
        saveConfig.enabled = true;
        saveConfig.mode = epochengine::saveload::SaveStreamMode::Interval;
        saveConfig.interval_seconds = -3.0;
        saveConfig.frame_interval = 0;
        saveConfig.max_snapshots = 0;
        saveConfig.profile_name.clear();
        saveConfig.target_root.clear();
        epochengine::saveload::clamp_streaming_save_config(saveConfig);

        epochengine::saveload::StreamingSaveStatus saveStatus{};
        epochengine::core::time::simulation_stats timeStats{};
        timeStats.frame_index = 240;
        timeStats.simulated_seconds = 4.0;

        const auto saveCadenceDue = epochengine::saveload::make_streaming_save_cadence_plan(saveConfig, saveStatus, timeStats);
        const bool shouldCapture = epochengine::saveload::should_capture_checkpoint(saveConfig, saveStatus, timeStats);
        epochengine::saveload::mark_checkpoint_captured(saveStatus, saveConfig, timeStats);
        const auto saveCadenceScheduled = epochengine::saveload::make_streaming_save_cadence_plan(saveConfig, saveStatus, timeStats);
        const std::string saveDescription = epochengine::saveload::describe_streaming_save(saveConfig, saveStatus);
        check(
            "timeline.clamp",
            saveConfig.interval_seconds == 0.25
            && saveConfig.frame_interval == 1u
            && saveConfig.max_snapshots == 1u
            && saveConfig.profile_name == "editor_timeline"
            && saveConfig.target_root == "cache/saves/timeline");
        check(
            "timeline.capture",
            shouldCapture
            && saveCadenceDue.capture_due
            && saveStatus.staged_snapshot_count == 1u
            && saveStatus.last_snapshot_label.find("editor_timeline_frame_") != std::string::npos
            && saveDescription.find("enabled") != std::string::npos
            && epochengine::saveload::detect_streaming_save_profile(saveConfig) == epochengine::saveload::StreamingSaveProfile::EditorInterval15s
            && epochengine::saveload::describe_retention(saveConfig).find("rolling 1 checkpoint") != std::string::npos);
        check(
            "timeline.cadence",
            !saveCadenceScheduled.capture_due
            && saveCadenceScheduled.next_seconds > timeStats.simulated_seconds
            && saveCadenceScheduled.seconds_until > 0.0
            && epochengine::saveload::streaming_save_cadence_summary(saveCadenceDue).find("capture due") != std::string::npos
            && epochengine::saveload::streaming_save_cadence_summary(saveCadenceScheduled).find("Time interval") != std::string::npos);

        const auto* intervalSaveProfile = epochengine::saveload::find_streaming_save_profile(
            epochengine::saveload::StreamingSaveProfile::EditorInterval15s);
        const auto* keyedSaveProfile = epochengine::saveload::find_streaming_save_profile("timeline_keyed");
        epochengine::saveload::StreamingSaveConfig keyedSaveConfig{};
        epochengine::saveload::apply_streaming_save_profile(
            keyedSaveConfig,
            epochengine::saveload::StreamingSaveProfile::TimelineKeyed);
        const auto saveProfileChangePlan = epochengine::saveload::make_streaming_save_profile_change_plan(
            saveConfig,
            epochengine::saveload::StreamingSaveProfile::TimelineKeyed);
        check(
            "timeline.stream_profiles",
            epochengine::saveload::validate_streaming_save_profile_descriptors()
            && epochengine::saveload::streaming_save_profile_count() == 4u
            && intervalSaveProfile != nullptr
            && intervalSaveProfile->mode == epochengine::saveload::SaveStreamMode::Interval
            && intervalSaveProfile->enabled
            && intervalSaveProfile->interval_seconds == 15.0
            && keyedSaveProfile != nullptr
            && keyedSaveProfile->profile == epochengine::saveload::StreamingSaveProfile::TimelineKeyed
            && keyedSaveConfig.enabled
            && keyedSaveConfig.mode == epochengine::saveload::SaveStreamMode::TimelineKey
            && keyedSaveConfig.max_snapshots == 256u
            && epochengine::saveload::stream_profile_id(epochengine::saveload::StreamingSaveProfile::EditorFrame120) == std::string_view{ "editor_frame_120" }
            && epochengine::saveload::stream_profile_summary(epochengine::saveload::StreamingSaveProfile::TimelineKeyed).find("timeline keys") != std::string_view::npos);
        check(
            "timeline.profile_change",
            saveProfileChangePlan.valid
            && saveProfileChangePlan.from_profile_id == "editor_interval_15s"
            && saveProfileChangePlan.to_profile_id == "timeline_keyed"
            && saveProfileChangePlan.mode == epochengine::saveload::SaveStreamMode::TimelineKey
            && saveProfileChangePlan.max_snapshots == 256u
            && epochengine::saveload::streaming_save_profile_change_summary(saveProfileChangePlan).find("timeline_keyed") != std::string::npos);

        auto timelineTracks = epochengine::timeline::default_editor_tracks();
        epochengine::timeline::TimelineState timelineState{};
        timelineState.playing = true;
        timelineState.duration_seconds = 8.0;
        timelineState.fixed_dt_seconds = 1.0 / 60.0;
        auto timelinePlaybackStats = timeStats;
        epochengine::timeline::sync_to_simulation(
            timelineState, timelinePlaybackStats);
        timelinePlaybackStats.simulated_seconds += 4.0;
        timelinePlaybackStats.frame_index += 240u;
        epochengine::timeline::sync_to_simulation(
            timelineState, timelinePlaybackStats);

        epochengine::timeline::TimelineState latePlaybackState{};
        epochengine::core::time::simulation_stats latePlaybackStats{};
        latePlaybackStats.fixed_dt_seconds = 1.0 / 60.0;
        latePlaybackStats.simulated_seconds = 900.0;
        latePlaybackStats.frame_index = 54'000u;
        epochengine::timeline::sync_to_simulation(
            latePlaybackState, latePlaybackStats);
        latePlaybackState.playing = true;
        latePlaybackStats.simulated_seconds += 0.25;
        latePlaybackStats.frame_index += 15u;
        epochengine::timeline::sync_to_simulation(
            latePlaybackState, latePlaybackStats);
        const double activePlayhead = latePlaybackState.playhead_seconds;
        latePlaybackState.playing = false;
        latePlaybackStats.simulated_seconds += 30.0;
        latePlaybackStats.frame_index += 1'800u;
        epochengine::timeline::sync_to_simulation(
            latePlaybackState, latePlaybackStats);
        const double pausedPlayhead = latePlaybackState.playhead_seconds;
        latePlaybackState.playing = true;
        latePlaybackStats.simulated_seconds += 1.0 / 60.0;
        ++latePlaybackStats.frame_index;
        epochengine::timeline::sync_to_simulation(
            latePlaybackState, latePlaybackStats);
        check(
            "timeline.delta_playback",
            activePlayhead > 0.249 && activePlayhead < 0.251
            && pausedPlayhead == activePlayhead
            && latePlaybackState.playhead_seconds > pausedPlayhead
            && latePlaybackState.playhead_seconds < 0.27
            && latePlaybackState.playing);

        std::vector<epochengine::timeline::TimelineEvent> timelineEvents{};
        timelineEvents.push_back(epochengine::timeline::make_event_from_stats(
            "save",
            epochengine::timeline::TimelineEventKind::Checkpoint,
            timeStats,
            saveStatus.last_snapshot_label,
            "PersistentLevel",
            saveStatus.last_output_path));
        timelineEvents.push_back(epochengine::timeline::TimelineEvent{
            .track_id = "camera",
            .kind = epochengine::timeline::TimelineEventKind::CameraCut,
            .simulated_seconds = 1.0,
            .frame_index = 60,
            .label = "camera cut",
            .target_name = "EditorCamera"
        });
        epochengine::timeline::sort_events(timelineEvents);
        const auto sceneKey = epochengine::timeline::to_scene_timeline_key(timelineEvents.front());
        const epochengine::timeline::TimelineViewConfig timelineView{
            .visible_start_seconds = 0.0,
            .visible_duration_seconds = 5.0,
            .pixel_width = 500.0
        };
        const auto timelineMetrics = epochengine::timeline::make_view_metrics(
            timelineState,
            timelineTracks,
            timelineEvents,
            timelineView);
        const auto timelineTrackSummaries = epochengine::timeline::summarize_tracks(timelineTracks, timelineEvents);
        const std::string timelineViewSummary = epochengine::timeline::describe_view(
            timelineState,
            timelineTracks,
            timelineEvents,
            timelineView);
        const epochengine::timeline::TimelineLaneLayoutConfig timelineLaneLayout{
            .pixel_width = 500.0,
            .header_width = 100.0,
            .lane_height = 24.0,
            .lane_gap = 4.0,
            .top_padding = 6.0
        };
        const auto timelineLanes = epochengine::timeline::make_lane_geometry(
            timelineTracks,
            timelineLaneLayout);
        const auto timelineMarkers = epochengine::timeline::make_event_markers(
            timelineTracks,
            timelineEvents,
            timelineView,
            timelineLaneLayout,
            timelineState.duration_seconds);
        const std::string timelineLaneSummary = epochengine::timeline::describe_lane_layout(
            timelineLanes,
            timelineMarkers);
        check(
            "timeline.model",
            timelineTracks.size() == 4u
            && epochengine::timeline::enabled_track_count(timelineTracks) == 4u
            && timelineEvents.front().frame_index == 60u
            && sceneKey.event_kind == "Camera cut");
        check(
            "timeline.view",
            timelineMetrics.visible_event_count == 2u
            && timelineMetrics.enabled_track_count == 4u
            && timelineMetrics.playhead_x > 399.9
            && timelineMetrics.playhead_x < 400.1
            && timelineTrackSummaries.size() == 4u
            && timelineTrackSummaries[1].event_count == 1u
            && timelineTrackSummaries[2].event_count == 1u
            && timelineViewSummary.find("timeline view 0.00-5.00s") != std::string::npos);
        check(
            "timeline.lane_layout",
            timelineLanes.size() == 4u
            && timelineMarkers.size() == 2u
            && timelineMarkers[0].visible
            && timelineMarkers[0].x > 179.9
            && timelineMarkers[0].x < 180.1
            && timelineMarkers[1].visible
            && timelineMarkers[1].x > 419.9
            && timelineMarkers[1].x < 420.1
            && timelineLaneSummary.find("4 lanes | 2 markers | 2 visible") != std::string::npos);

        const auto inputProfile = epochengine::input::make_profile(epochengine::input::ProfilePreset::EditorDefault);
        const auto resetBinding = inputProfile.bindings[
            epochengine::input::action_index(epochengine::input::Action::ResetCamera)];
        const auto copyBinding = inputProfile.bindings[
            epochengine::input::action_index(epochengine::input::Action::ClipboardCopy)];
        const auto contextBinding = inputProfile.bindings[
            epochengine::input::action_index(epochengine::input::Action::ContextMenu)];
        const std::string inputSummary = epochengine::input::profile_summary(epochengine::input::ProfilePreset::EditorDefault);
        check(
            "input.profile",
            epochengine::input::validate_profile(inputProfile)
            && epochengine::input::bound_action_count(inputProfile) == static_cast<std::size_t>(epochengine::input::Action::Count)
            && resetBinding.primary == epochengine::input::Key::Home
            && copyBinding.primary == epochengine::input::Key::C
            && copyBinding.control
            && contextBinding.mouse == epochengine::input::MouseButton::MouseRight
            && epochengine::input::action_label(epochengine::input::Action::ResetCamera) == std::string_view{ "Reset Camera To Center" }
            && inputSummary.find("reset Home") != std::string::npos
            && inputSummary.find("context Mouse Right") != std::string::npos);

        epochengine::scene::SceneSnapshot snapshot{};
        snapshot.scene_id = "timeline \"contract\"";
        snapshot.world_name = "Persistent\nLevel";
        snapshot.captured_frame_index = timeStats.frame_index;
        snapshot.captured_simulated_seconds = timeStats.simulated_seconds;

        epochengine::scene::SceneObjectSnapshot object{};
        object.name = "StarterCube";
        object.type = "StaticMesh";
        object.category = "Gameplay";
        object.position = { 0.0F, 0.5F, 0.0F };
        snapshot.objects.push_back(object);
        snapshot.timeline_keys.push_back(epochengine::scene::make_timeline_key(2.0, 120, "later", "checkpoint", "StarterCube", "late"));
        snapshot.timeline_keys.push_back(epochengine::scene::make_timeline_key(1.0, 60, "first", "checkpoint", "StarterCube", "payload\tvalue"));
        snapshot.timeline_keys.push_back(epochengine::timeline::to_scene_timeline_key(timelineEvents.front()));
        epochengine::scene::sort_timeline_keys(snapshot);

        const auto categoryCounts = epochengine::scene::object_count_by_category(snapshot);
        const std::string snapshotText = epochengine::scene::serialize_snapshot_text(snapshot);
        const std::string snapshotSummary = epochengine::scene::snapshot_summary(snapshot);
        const auto parsedSnapshot = epochengine::scene::parse_snapshot_text(snapshotText);
        const auto migratedLegacyScene = epochengine::scene::parse_legacy_editor_scene_text(
            "scene \"legacy.scene\"\n"
            "project \"legacy.project\"\n"
            "epoch_editor_entities 1\n"
            "entity \"Camera\" \"Camera\" \"Editor\" pos 0 2 5 rot 0 0 0 scale 1 1 1 visible 1 editor_only 0\n"
            "entity \"Spawn\" \"Spawn\" \"Gameplay\" pos 0 0 0 rot 0 0 0 scale 1 1 1 visible 1 editor_only 0\n");
        const auto checkpointRecord = epochengine::saveload::make_checkpoint_record(
            saveConfig,
            saveStatus,
            timeStats,
            snapshotText.size(),
            snapshot.timeline_keys.size());
        const auto checkpointPackage = epochengine::saveload::make_checkpoint_package(checkpointRecord, snapshotText);
        const auto checkpointWritePlan = epochengine::saveload::make_checkpoint_write_plan(saveConfig, checkpointPackage);
        const auto checkpointRestorePlan = epochengine::saveload::make_checkpoint_restore_plan(saveConfig, checkpointRecord);
        std::vector<epochengine::saveload::StreamingCheckpointRecord> retentionRecords{};
        retentionRecords.push_back(checkpointRecord);
        retentionRecords.push_back(checkpointRecord);
        retentionRecords.back().label = "editor_timeline_frame_0121";
        retentionRecords.back().output_path = "cache/saves/timeline/editor_timeline_frame_0121.checkpoint";
        retentionRecords.push_back(checkpointRecord);
        retentionRecords.back().label = "editor_timeline_frame_0122";
        retentionRecords.back().output_path = "cache/saves/timeline/editor_timeline_frame_0122.checkpoint";
        epochengine::saveload::StreamingSaveConfig retentionConfig = saveConfig;
        retentionConfig.max_snapshots = 2u;
        const auto checkpointRetentionPlan = epochengine::saveload::make_checkpoint_retention_plan(
            retentionConfig,
            retentionRecords);
        const auto blockedWriteResult = epochengine::saveload::write_checkpoint_package(
            checkpointWritePlan,
            checkpointPackage,
            epochengine::saveload::StreamingCheckpointWriteApproval{});
        const std::string checkpointSnapshotPayload = epochengine::saveload::checkpoint_snapshot_payload(
            checkpointWritePlan,
            checkpointPackage);
        const std::string checkpointManifestLine = checkpointPackage.manifest_line;
        check(
            "snapshot.lookup",
            epochengine::scene::find_object(snapshot, "StarterCube") != nullptr
            && categoryCounts.contains("Gameplay")
            && categoryCounts.at("Gameplay") == 1u);
        check(
            "snapshot.timeline_sort",
            snapshot.timeline_keys.size() == 3u
            && snapshot.timeline_keys.front().frame_index == 60u);
        check(
            "snapshot.serialize",
            snapshotText.find("epoch_snapshot 3") != std::string::npos
            && snapshotText.find("timeline \\\"contract\\\"") != std::string::npos
            && snapshotText.find("Persistent\\nLevel") != std::string::npos
            && snapshotText.find("payload\\tvalue") != std::string::npos
            && snapshotSummary.find("objects 1") != std::string::npos);
        check(
            "snapshot.legacy_editor_migration",
            migratedLegacyScene.ok
            && migratedLegacyScene.snapshot.project_id == "legacy.project"
            && migratedLegacyScene.snapshot.objects.size() == 2u
            && migratedLegacyScene.snapshot.primary_camera != epochengine::scene::kInvalidSceneObjectId
            && migratedLegacyScene.snapshot.primary_spawn != epochengine::scene::kInvalidSceneObjectId);

        bool parsedHasEscapedPayload = false;
        bool parsedHasCameraCut = false;
        if (parsedSnapshot.ok)
        {
            for (const auto& key : parsedSnapshot.snapshot.timeline_keys)
            {
                parsedHasEscapedPayload = parsedHasEscapedPayload || key.payload == "payload\tvalue";
                parsedHasCameraCut = parsedHasCameraCut || key.event_kind == "Camera cut";
            }
        }
        check(
            "snapshot.parse_round_trip",
            parsedSnapshot.ok
            && parsedSnapshot.snapshot.scene_id == snapshot.scene_id
            && parsedSnapshot.snapshot.world_name == snapshot.world_name
            && parsedSnapshot.snapshot.captured_frame_index == snapshot.captured_frame_index
            && parsedSnapshot.snapshot.captured_simulated_seconds == snapshot.captured_simulated_seconds
            && parsedSnapshot.snapshot.objects.size() == 1u
            && parsedSnapshot.snapshot.objects.front().name == "StarterCube"
            && parsedSnapshot.snapshot.objects.front().category == "Gameplay"
            && parsedSnapshot.snapshot.objects.front().position[1] == 0.5F
            && parsedSnapshot.snapshot.timeline_keys.size() == snapshot.timeline_keys.size()
            && parsedHasEscapedPayload
            && parsedHasCameraCut);
        check(
            "checkpoint.record",
            checkpointRecord.valid
            && checkpointRecord.scene_text_bytes == snapshotText.size()
            && checkpointRecord.timeline_key_count == snapshot.timeline_keys.size()
            && checkpointManifestLine.find("checkpoint \"editor_timeline_frame_") != std::string::npos
            && checkpointManifestLine.find("timeline_keys 3") != std::string::npos);
        check(
            "checkpoint.package",
            epochengine::saveload::validate_checkpoint_package(checkpointPackage)
            && checkpointPackage.scene_text == snapshotText
            && checkpointManifestLine.find("hash \"") != std::string::npos
            && epochengine::saveload::checkpoint_package_summary(checkpointPackage).find("deterministic restore") != std::string::npos
            && epochengine::scene::parse_snapshot_text(checkpointPackage.scene_text).ok);
        check(
            "checkpoint.write_plan",
            checkpointWritePlan.valid
            && checkpointWritePlan.root_path == saveConfig.target_root
            && checkpointWritePlan.snapshot_path == checkpointRecord.output_path
            && checkpointWritePlan.scene_payload_path.find(".epoch") != std::string::npos
            && checkpointWritePlan.manifest_path.find("manifest.timeline.log") != std::string::npos
            && epochengine::saveload::checkpoint_write_plan_summary(checkpointWritePlan).find("write plan") != std::string::npos);
        check(
            "checkpoint.restore_plan",
            checkpointRestorePlan.valid
            && checkpointRestorePlan.checkpoint_label == checkpointRecord.label
            && checkpointRestorePlan.snapshot_path == checkpointRecord.output_path
            && checkpointRestorePlan.scene_payload_path == checkpointWritePlan.scene_payload_path
            && checkpointRestorePlan.manifest_path == checkpointWritePlan.manifest_path
            && epochengine::saveload::checkpoint_restore_plan_summary(checkpointRestorePlan).find("restore plan") != std::string::npos);
        check(
            "checkpoint.retention_plan",
            checkpointRetentionPlan.valid
            && checkpointRetentionPlan.source_count == 3u
            && checkpointRetentionPlan.retained_count == 2u
            && checkpointRetentionPlan.prune_labels.size() == 1u
            && checkpointRetentionPlan.prune_labels.front() == checkpointRecord.label
            && checkpointRetentionPlan.prune_snapshot_paths.front() == checkpointRecord.output_path
            && epochengine::saveload::checkpoint_retention_plan_summary(checkpointRetentionPlan).find("prune 1") != std::string::npos);
        check(
            "checkpoint.writer_gate",
            blockedWriteResult.blocked
            && !blockedWriteResult.succeeded
            && !blockedWriteResult.wrote_scene_payload
            && checkpointSnapshotPayload.find("epoch_checkpoint 1") != std::string::npos
            && checkpointSnapshotPayload.find(checkpointWritePlan.scene_payload_path) != std::string::npos
            && blockedWriteResult.message.find("human approval") != std::string::npos);

        log_editor_self_test_line(std::string("engine_contract_self_test.summary=") + snapshotSummary);
        log_editor_self_test_line(std::string("engine_contract_self_test.result=") + (failed ? "fail" : "pass"));
        return failed ? 7 : 0;
    }

    [[nodiscard]] inline int run_editor_project_self_test(std::string_view project_id)
    {
        if (project_id.empty())
            project_id = "sandbox";

        const std::string projectId{ project_id };
        const auto* profile = epochengine::editor_find_project_profile(project_id);
        const auto ensured = epochengine::editor_ensure_project_shell(project_id);
        log_editor_self_test_line("editor_project_self_test.project_id=" + projectId);
        log_editor_self_test_line(std::string("editor_project_self_test.materialize=") + (ensured.succeeded ? "pass" : "fail"));
        log_editor_self_test_line("editor_project_self_test.summary=" + ensured.summary);
        if (!ensured.root_path.empty())
            log_editor_self_test_line("editor_project_self_test.root=" + ensured.root_path);
        if (!ensured.manifest_path.empty())
            log_editor_self_test_line("editor_project_self_test.manifest=" + ensured.manifest_path);
        if (!ensured.default_script_path.empty())
            log_editor_self_test_line("editor_project_self_test.script=" + ensured.default_script_path);

        if (!ensured.succeeded)
            return 2;

        const std::filesystem::path projectRoot{ensured.root_path};
        const auto readGeneratedText = [](const std::filesystem::path& path)
        {
            std::ifstream input{path, std::ios::binary};
            std::ostringstream text{};
            if (input)
                text << input.rdbuf();
            return text.str();
        };
        const std::string generatedManifest = readGeneratedText(
            projectRoot / "project.epoch.json");
        const std::string generatedCmakeFragment = readGeneratedText(
            projectRoot / "epoch.project.cmake");
        const std::string generatedCmakeLists = readGeneratedText(
            projectRoot / "CMakeLists.txt");
        const std::string generatedLinuxBuild = readGeneratedText(
            projectRoot / "build_project.sh");
        const bool staticRuntimeProfileReady =
            generatedManifest.find(
                "\"project_format\": \"epoch-project-v1\"")
                != std::string::npos
            && generatedManifest.find(
                "\"build_profile\": \"epoch-runtime-static\"")
                != std::string::npos
            && generatedCmakeFragment.find(
                "EPOCH_MANAGED_GENERATED_FILE: cmake_fragment_v2")
                != std::string::npos
            && generatedCmakeFragment.find(
                "set(EPOCH_BUILD_STATIC_RUNTIME ON")
                != std::string::npos
            && generatedCmakeFragment.find(
                "set(EPOCH_ENABLE_NATIVE_EXTENSIONS OFF")
                != std::string::npos
            && generatedCmakeLists.find(
                "epoch_configure_embedded_project(epoch_project_runtime)")
                != std::string::npos
            && generatedLinuxBuild.find(
                "EPOCH_MANAGED_GENERATED_FILE: linux_build_v2")
                != std::string::npos
            && generatedLinuxBuild.find(
                "--target epoch_project_runtime")
                != std::string::npos;
        log_editor_self_test_line(
            std::string("editor_project_self_test.static_runtime_profile=")
            + (staticRuntimeProfileReady ? "pass" : "fail"));

        std::filesystem::path scenePath =
            profile == nullptr
                ? std::filesystem::path{}
                : std::filesystem::path{ profile->scene_path };
        if (scenePath.is_relative())
        {
            const auto runtimeRoot = epochengine::core::path::runtime_root_dir();
            if (!runtimeRoot.empty())
                scenePath = runtimeRoot / scenePath;
        }
        scenePath = scenePath.lexically_normal();

        const auto loadedScene =
            epochengine::scene::persistence::load_scene_snapshot(scenePath);
        const auto savedScene = loadedScene.result
            ? epochengine::scene::persistence::save_scene_snapshot_atomic(
                scenePath,
                loadedScene.snapshot)
            : epochengine::scene::persistence::ScenePersistenceResult{};
        const auto reopenedScene = savedScene
            ? epochengine::scene::persistence::load_scene_snapshot(scenePath)
            : epochengine::scene::persistence::SceneLoadResult{};
        const bool sceneRoundTrip =
            loadedScene.result
            && savedScene
            && reopenedScene.result
            && epochengine::scene::persistence::scene_snapshot_semantically_equal(
                loadedScene.snapshot,
                reopenedScene.snapshot);
        log_editor_self_test_line(
            std::string("editor_project_self_test.save_reopen=")
            + (sceneRoundTrip ? "pass" : "fail"));
        log_editor_self_test_line(
            "editor_project_self_test.scene=" + scenePath.generic_string());
        const auto build = epochengine::editor_build_project(ensured.root_path);
        log_editor_self_test_line(std::string("editor_project_self_test.build=") + (build.succeeded ? "pass" : "fail"));
        log_editor_self_test_line("editor_project_self_test.build_summary=" + build.summary);
        if (!build.output_path.empty())
            log_editor_self_test_line("editor_project_self_test.output=" + build.output_path);
        if (!build.log_path.empty())
            log_editor_self_test_line("editor_project_self_test.log=" + build.log_path);

        const GeneratedProjectSelfTestResult childSelfTest = build.succeeded
            ? run_generated_project_self_test(build.output_path, ensured.root_path)
            : GeneratedProjectSelfTestResult{};
        if (childSelfTest.attempted || !childSelfTest.log_path.empty())
        {
            log_editor_self_test_line(
                std::string("editor_project_self_test.child_self_test=")
                + (childSelfTest.succeeded ? "pass" : "fail"));
            log_editor_self_test_line("editor_project_self_test.child_self_test_log=" + childSelfTest.log_path);
        }

        const auto evidencePathsConfig = epochengine::ai::default_evidence_paths();
        std::vector<std::string> evidencePaths{};
        const auto add_evidence_path = [&evidencePaths](std::string path)
        {
            if (!path.empty())
                evidencePaths.push_back(std::move(path));
        };

        add_evidence_path(ensured.root_path);
        add_evidence_path(ensured.manifest_path);
        add_evidence_path(ensured.entry_source_path);
        add_evidence_path(ensured.build_script_path);
        add_evidence_path(ensured.default_script_path);
        if (!ensured.root_path.empty())
            add_evidence_path((std::filesystem::path(ensured.root_path) / "project.paths.txt").string());
        add_evidence_path(build.log_path);
        add_evidence_path(build.output_path);
        add_evidence_path(childSelfTest.log_path);
        add_evidence_path(evidencePathsConfig.model_exchange_jsonl);
        add_evidence_path(evidencePathsConfig.tool_trace_jsonl);

        const bool verifierReady =
            staticRuntimeProfileReady
            && sceneRoundTrip
            && build.succeeded
            && childSelfTest.succeeded;
        const std::string normalizedOutput =
            "project=" + projectId +
            "; materialize=" + (ensured.succeeded ? std::string{ "pass" } : std::string{ "fail" }) +
            "; static_runtime_profile=" + (staticRuntimeProfileReady ? std::string{ "pass" } : std::string{ "fail" }) +
            "; save_reopen=" + (sceneRoundTrip ? std::string{ "pass" } : std::string{ "fail" }) +
            "; build=" + (build.succeeded ? std::string{ "pass" } : std::string{ "fail" }) +
            "; child_self_test=" + (childSelfTest.attempted
                ? (childSelfTest.succeeded ? std::string{ "pass" } : std::string{ "fail" })
                : std::string{ "skipped" }) +
            "; root=" + ensured.root_path +
            "; manifest=" + ensured.manifest_path +
            "; output=" + build.output_path +
            "; log=" + build.log_path +
            "; child_self_test_log=" + childSelfTest.log_path +
            "; evidence_paths=" + std::to_string(evidencePaths.size());

        epochengine::ai::append_tool_trace(epochengine::ai::McpCaptureRecord{
            .session_id = projectId + "-cli-self-test",
            .call_id = "project-test-1",
            .server = "epoch-editor-cli",
            .tool = "project.test",
            .prompt = "Run visible editor project self-test for " + projectId,
            .normalized_output = normalizedOutput,
            .source_path = ensured.default_script_path.empty() ? ensured.manifest_path : ensured.default_script_path,
            .state = verifierReady ? epochengine::ai::McpCallState::succeeded : epochengine::ai::McpCallState::failed,
            .error = verifierReady ? epochengine::ai::McpErrorCode::none : epochengine::ai::McpErrorCode::execution_failed});


        append_editor_project_self_test_note(
            ensured.root_path,
            build.succeeded ? "CLI Engine Development Self-Test Completed" : "CLI Engine Development Self-Test Blocked",
            verifierReady
                ? "Materialize, child build, and child self-test evidence staged."
                : build.succeeded ? childSelfTest.summary : "Materialize succeeded but child build failed; inspect the build log.",
            evidencePathsConfig.tool_trace_jsonl,
            epochengine::ai::active_model_name(),
            ensured.manifest_path,
            profile == nullptr ? std::string_view{} : profile->scene_path,
            ensured.default_script_path,
            build.log_path,
            build.output_path);
        log_editor_self_test_line("editor_project_self_test.mcp_capture=" + evidencePathsConfig.tool_trace_jsonl);

        return verifierReady ? 0 : (build.succeeded ? 5 : 3);
    }

    [[nodiscard]] inline int run_editor_ai_gate_self_test()
    {
        struct GateCase
        {
            std::string_view name{};
            std::string_view reply{};
            bool should_accept = false;
        };
        struct CaptureCase
        {
            std::string_view name{};
            std::string_view reply{};
            bool should_promote = false;
        };
        struct NormalizeCase
        {
            std::string_view name{};
            std::string_view reply{};
            std::string_view expected{};
        };

        const std::vector<GateCase> cases{
            GateCase{
                .name = "status-only-reject",
                .reply = "Looks good, everything is working fine. Ship it.",
                .should_accept = false
            },
            GateCase{
                .name = "missing-child-verifier-reject",
                .reply = "Packet path exists and build log says build pass. Output artifact exists. Human review can continue.",
                .should_accept = false
            },
            GateCase{
                .name = "bypass-server-reject",
                .reply = "Build pass, packet path, output exe, verifier present. Auto promote it and start server on a listener for the model.",
                .should_accept = false
            },
            GateCase{
                .name = "human-gated-evidence-accept",
                .reply =
                    "Packet staged with evidence paths. Build log reports build=pass. Output artifact is present. "
                    "child_self_test/verifier pass is recorded in the packet. MCP capture evidence path exists. "
                    "Gate remains ready_for_human_review and requires operator approval before promotion.",
                .should_accept = true
            }
        };
        const std::vector<CaptureCase> captureCases{
            CaptureCase{
                .name = "hidden-reasoning-only-capture-block",
                .reply = "Local model returned hidden reasoning without visible assistant content. Select a content-producing model or disable reasoning export before using OS AI chat.",
                .should_promote = false
            },
            CaptureCase{
                .name = "no-model-capture-block",
                .reply = "No AI model selected. Open Workspace > AI, scan local models, and choose a model before running chat/tooling.",
                .should_promote = false
            },
            CaptureCase{
                .name = "leaked-reasoning-draft-capture-block",
                .reply = "We need to produce a response that follows the rules. The user asks for one evidence-gated plan.",
                .should_promote = false
            },
            CaptureCase{
                .name = "final-evidence-answer-capture-allow",
                .reply = "Packet evidence is staged, the build log reports build=pass, output artifact exists, child_self_test verifier passed, and promotion remains human-gated.",
                .should_promote = true
            }
        };
        const std::vector<NormalizeCase> normalizeCases{
            NormalizeCase{
                .name = "think-wrapper-keeps-final",
                .reply = "<think>private draft</think>Final answer.",
                .expected = "Final answer."
            },
            NormalizeCase{
                .name = "adjacent-wrappers-keep-final",
                .reply = "<analysis>draft one</analysis><reasoning>draft two</reasoning><final>Final answer.</final>",
                .expected = "Final answer."
            },
            NormalizeCase{
                .name = "unterminated-reasoning-is-blocked",
                .reply = "<reasoning>private draft",
                .expected = ""
            }
        };

        bool failed = false;
        int accepted = 0;
        int rejected = 0;
        int falseAccepts = 0;
        int falseRejects = 0;
        int safetyBlocks = 0;
        int evidenceScoreTotal = 0;
        int captureAllowed = 0;
        int captureBlocked = 0;
        int captureFalseAllows = 0;
        int captureFalseBlocks = 0;
        logger::get("Engine.AI.Gate").logf(
            logger::LogLevel::INFO,
            std::source_location::current(),
            "editor_ai_gate_self_test.cases={}",
            cases.size());
        for (const auto& test : cases)
        {
            const auto result = epochengine::ai::classify_helper_review_reply(test.reply);
            const bool ok = result.accepted == test.should_accept;
            failed = failed || !ok;
            evidenceScoreTotal += result.evidence_score;
            if (result.accepted)
                ++accepted;
            else
                ++rejected;
            if (result.accepted && !test.should_accept)
                ++falseAccepts;
            if (!result.accepted && test.should_accept)
                ++falseRejects;
            if (result.state == "rejected_bypass_request")
                ++safetyBlocks;

            logger::get("Engine.AI.Gate").logf(
                logger::LogLevel::INFO,
                std::source_location::current(),
                "editor_ai_gate_self_test.case={} expected={} actual={} state={} evidence_score={} result={}",
                test.name,
                test.should_accept ? "accept" : "reject",
                result.accepted ? "accept" : "reject",
                result.state,
                result.evidence_score,
                ok ? "pass" : "fail");
        }

        logger::get("Engine.AI.Gate").logf(
            logger::LogLevel::INFO,
            std::source_location::current(),
            "editor_ai_gate_self_test.capture_cases={}",
            captureCases.size());
        for (const auto& test : captureCases)
        {
            const bool actual = epochengine::ai::is_promotable_assistant_reply(test.reply);
            const bool ok = actual == test.should_promote;
            failed = failed || !ok;
            if (actual)
                ++captureAllowed;
            else
                ++captureBlocked;
            if (actual && !test.should_promote)
                ++captureFalseAllows;
            if (!actual && test.should_promote)
                ++captureFalseBlocks;

            logger::get("Engine.AI.Gate").logf(
                logger::LogLevel::INFO,
                std::source_location::current(),
                "editor_ai_gate_self_test.capture_case={} expected={} actual={} result={}",
                test.name,
                test.should_promote ? "promote" : "block",
                actual ? "promote" : "block",
                ok ? "pass" : "fail");
        }

        for (const auto& test : normalizeCases)
        {
            const std::string actual = epochengine::ai::normalize_assistant_reply(test.reply);
            const bool ok = actual == test.expected;
            failed = failed || !ok;
            logger::get("Engine.AI.Gate").logf(
                logger::LogLevel::INFO,
                std::source_location::current(),
                "editor_ai_gate_self_test.normalize_case={} result={}",
                test.name,
                ok ? "pass" : "fail");
        }

        const double totalCases = static_cast<double>((std::max)(std::size_t{ 1 }, cases.size()));
        const double accuracy = (totalCases - static_cast<double>(falseAccepts + falseRejects)) / totalCases;
        const double averageEvidenceScore = static_cast<double>(evidenceScoreTotal) / totalCases;
        logger::get("Engine.AI.Gate").logf(
            logger::LogLevel::INFO,
            std::source_location::current(),
            "editor_ai_gate_self_test.stats=accepted={} rejected={} false_accepts={} false_rejects={} safety_blocks={} avg_evidence_score={:.2f} accuracy={:.2f}",
            accepted,
            rejected,
            falseAccepts,
            falseRejects,
            safetyBlocks,
            averageEvidenceScore,
            accuracy);
        logger::get("Engine.AI.Gate").logf(
            logger::LogLevel::INFO,
            std::source_location::current(),
            "editor_ai_gate_self_test.capture_stats=allowed={} blocked={} false_allows={} false_blocks={}",
            captureAllowed,
            captureBlocked,
            captureFalseAllows,
            captureFalseBlocks);

        logger::get("Engine.AI.Gate").logf(
            logger::LogLevel::INFO,
            std::source_location::current(),
            "editor_ai_gate_self_test.result={}",
            failed ? "fail" : "pass");
        return failed ? 6 : 0;
    }

    [[nodiscard]] inline int run_engine_validation_self_test()
    {
        int result = 0;
        log_editor_self_test_line("engine_validation_self_test.start=contracts_plus_project_profiles_plus_ai_gate");

        const int contractResult = run_engine_contract_self_test();
        log_editor_self_test_line("engine_validation_self_test.contracts.code=" + std::to_string(contractResult));
        if (contractResult != 0 && result == 0)
            result = contractResult;

        for (const auto& profile : epochengine::editor_project_profiles())
        {
            log_editor_self_test_line("engine_validation_self_test.project.begin=" + std::string(profile.id));
            const int projectResult = run_editor_project_self_test(profile.id);
            log_editor_self_test_line(
                "engine_validation_self_test.project.result="
                + std::string(profile.id)
                + "; code="
                + std::to_string(projectResult));
            if (projectResult != 0 && result == 0)
                result = projectResult;
        }

        const int aiGateResult = run_editor_ai_gate_self_test();
        log_editor_self_test_line("engine_validation_self_test.ai_gate.code=" + std::to_string(aiGateResult));
        if (aiGateResult != 0 && result == 0)
            result = aiGateResult;

        log_editor_self_test_line(std::string("engine_validation_self_test.result=") + (result == 0 ? "pass" : "fail"));
        return result;
    }

    inline void apply_post_update_startup_cooldown(const std::string_view log_system)
    {
        const int delay_ms = read_post_update_startup_delay_ms();
        if (delay_ms <= 0)
            return;

#if defined(_WIN32)
        SetEnvironmentVariableA("EPOCH_POST_UPDATE_STARTUP_DELAY_MS", nullptr);
#else
        unsetenv("EPOCH_POST_UPDATE_STARTUP_DELAY_MS");
#endif

        logger::get(std::string{ log_system }).logf(
            logger::LogLevel::INFO,
            std::source_location::current(),
            "Post-update startup cooldown: {} ms before context initialization.",
            delay_ms);

        std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
    }

#if defined(_WIN32)
    inline void apply_fullscreen_capture_window_defaults() noexcept
    {
        if (!cli::capture_requested)
            return;

        const int screenWidth = ::GetSystemMetrics(SM_CXSCREEN);
        const int screenHeight = ::GetSystemMetrics(SM_CYSCREEN);
        if (screenWidth <= 0 || screenHeight <= 0)
            return;

        cli::window_width = screenWidth;
        cli::window_height = screenHeight;
        cli::window_width_overridden = true;
        cli::window_height_overridden = true;
    }

    [[nodiscard]] inline std::filesystem::path engine_owned_smoke_capture_path()
    {
        if (!cli::capture_requested)
            return {};

        const auto root = cli::capture_output_root();
        std::error_code ec{};
        std::filesystem::create_directories(root, ec);
        return root / epochengine::format_text("{}-parented-grid.bmp", cli::capture_output_stem());
    }

    [[nodiscard]] inline bool write_top_down_bgra_as_bmp(
        const std::filesystem::path& filepath,
        const std::vector<std::uint8_t>& pixels,
        const int width,
        const int height) noexcept
    {
        if (width <= 0 || height <= 0 || pixels.size() < static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u)
            return false;

        const int rowBytes = width * 3;
        const int padSize = (4 - (rowBytes % 4)) % 4;
        const int stride = rowBytes + padSize;

        std::vector<std::uint8_t> bmpData(static_cast<std::size_t>(stride) * static_cast<std::size_t>(height), 0);
        for (int y = 0; y < height; ++y)
        {
            const int srcY = height - 1 - y;
            const auto* srcRow = pixels.data() + static_cast<std::size_t>(srcY) * static_cast<std::size_t>(width) * 4u;
            auto* dstRow = bmpData.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(stride);

            for (int x = 0; x < width; ++x)
            {
                const auto srcIndex = static_cast<std::size_t>(x) * 4u;
                dstRow[x * 3 + 0] = srcRow[srcIndex + 0];
                dstRow[x * 3 + 1] = srcRow[srcIndex + 1];
                dstRow[x * 3 + 2] = srcRow[srcIndex + 2];
            }
        }

        std::uint8_t fileHeader[14] = {
            'B','M',
            0,0,0,0,
            0,0,
            0,0,
            54,0,0,0
        };

        std::uint8_t infoHeader[40] = {
            40,0,0,0,
            0,0,0,0,
            0,0,0,0,
            1,0,
            24,0,
            0,0,0,0,
            0,0,0,0,
            0,0,0,0,
            0,0,0,0,
            0,0,0,0,
            0,0,0,0
        };

        const std::uint32_t fileSize = 54u + static_cast<std::uint32_t>(bmpData.size());
        std::memcpy(&fileHeader[2], &fileSize, 4);
        std::memcpy(&infoHeader[4], &width, 4);
        std::memcpy(&infoHeader[8], &height, 4);

        std::ofstream out(filepath, std::ios::binary | std::ios::trunc);
        if (!out)
            return false;

        out.write(reinterpret_cast<const char*>(fileHeader), sizeof(fileHeader));
        out.write(reinterpret_cast<const char*>(infoHeader), sizeof(infoHeader));
        out.write(reinterpret_cast<const char*>(bmpData.data()), static_cast<std::streamsize>(bmpData.size()));
        return out.good();
    }

    [[nodiscard]] inline bool capture_window_to_bmp(HWND hwnd, const std::filesystem::path& filepath) noexcept
    {
        if (!hwnd)
            return false;

        RECT windowRect{};
        if (!::GetWindowRect(hwnd, &windowRect))
            return false;

        const int width = windowRect.right - windowRect.left;
        const int height = windowRect.bottom - windowRect.top;
        if (width <= 0 || height <= 0)
            return false;

        const HDC screenDc = ::GetDC(nullptr);
        if (!screenDc)
            return false;

        HDC memoryDc = ::CreateCompatibleDC(screenDc);
        if (!memoryDc)
        {
            ::ReleaseDC(nullptr, screenDc);
            return false;
        }

        BITMAPINFO bitmapInfo{};
        bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bitmapInfo.bmiHeader.biWidth = width;
        bitmapInfo.bmiHeader.biHeight = -height;
        bitmapInfo.bmiHeader.biPlanes = 1;
        bitmapInfo.bmiHeader.biBitCount = 32;
        bitmapInfo.bmiHeader.biCompression = BI_RGB;

        void* bits = nullptr;
        const HBITMAP bitmap = ::CreateDIBSection(screenDc, &bitmapInfo, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (!bitmap || !bits)
        {
            if (bitmap)
                ::DeleteObject(bitmap);
            ::DeleteDC(memoryDc);
            ::ReleaseDC(nullptr, screenDc);
            return false;
        }

        const HGDIOBJ previous = ::SelectObject(memoryDc, bitmap);
        ::PatBlt(memoryDc, 0, 0, width, height, BLACKNESS);
        const bool copied = ::BitBlt(
            memoryDc,
            0,
            0,
            width,
            height,
            screenDc,
            windowRect.left,
            windowRect.top,
            SRCCOPY | CAPTUREBLT) != FALSE;

        std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u, 0);
        if (copied)
            std::memcpy(pixels.data(), bits, pixels.size());

        if (previous)
            ::SelectObject(memoryDc, previous);
        ::DeleteObject(bitmap);
        ::DeleteDC(memoryDc);
        ::ReleaseDC(nullptr, screenDc);

        return copied && write_top_down_bgra_as_bmp(filepath, pixels, width, height);
    }

    inline void prepare_parent_window_for_engine_capture(epochengine::core::MultiContextManager& mgr) noexcept
    {
        if (!cli::capture_requested)
            return;

        const HWND parentWindow = mgr.GetParentWindow();
        if (!parentWindow)
            return;

        RECT workArea{};
        MONITORINFO monitorInfo{};
        monitorInfo.cbSize = sizeof(MONITORINFO);
        const HMONITOR monitor = ::MonitorFromWindow(parentWindow, MONITOR_DEFAULTTONEAREST);
        if (monitor && ::GetMonitorInfoW(monitor, &monitorInfo))
        {
            workArea = monitorInfo.rcMonitor;
        }
        else if (::SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0))
        {
        }

        ::ShowWindow(parentWindow, SW_RESTORE);
        ::SetWindowPos(
            parentWindow,
            HWND_TOPMOST,
            workArea.left,
            workArea.top,
            workArea.right - workArea.left,
            workArea.bottom - workArea.top,
            SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_SHOWWINDOW);
        ::BringWindowToTop(parentWindow);
        ::SetForegroundWindow(parentWindow);
        ::SetActiveWindow(parentWindow);
        mgr.ArrangeDockedWindowsGrid();
        ::UpdateWindow(parentWindow);
        ::RedrawWindow(parentWindow, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
    }

    [[nodiscard]] inline HWND capture_dock_slot_handle(
        const epochengine::core::WindowData* window,
        HWND dockParent) noexcept
    {
        if (!window || !dockParent || ::IsWindow(dockParent) == FALSE)
            return nullptr;

        if (window->hwndChild
            && ::IsWindow(window->hwndChild) != FALSE
            && ::GetParent(window->hwndChild) == dockParent)
        {
            return window->hwndChild;
        }

        if (window->host_hwnd
            && ::IsWindow(window->host_hwnd) != FALSE
            && ::GetParent(window->host_hwnd) == dockParent)
        {
            return window->host_hwnd;
        }

        if (window->hwnd
            && ::IsWindow(window->hwnd) != FALSE
            && ::GetParent(window->hwnd) == dockParent)
        {
            return window->hwnd;
        }

        return nullptr;
    }

    [[nodiscard]] inline int capture_dock_order(
        const epochengine::core::WindowData* window) noexcept
    {
        switch (window ? window->type : epochengine::core::ContextType::None)
        {
        case epochengine::core::ContextType::RayLib: return 0;
        case epochengine::core::ContextType::SDL: return 1;
        case epochengine::core::ContextType::SFML: return 2;
        case epochengine::core::ContextType::Vulkan: return 3;
        case epochengine::core::ContextType::OpenGL: return 4;
        case epochengine::core::ContextType::DirectX: return 5;
        case epochengine::core::ContextType::Software: return 6;
        default: return 99;
        }
    }

    inline void force_parent_window_capture_layout(
        epochengine::core::MultiContextManager& mgr) noexcept
    {
        const HWND parentWindow = mgr.GetParentWindow();
        if (!parentWindow || ::IsWindow(parentWindow) == FALSE)
            return;

        std::vector<epochengine::core::WindowData*> dockedWindows;
        dockedWindows.reserve(mgr.GetWindows().size());
        for (const auto& ownedWindow : mgr.GetWindows())
        {
            auto* window = ownedWindow.get();
            if (!window)
                continue;

            if (const HWND liveHwnd = capture_dock_slot_handle(window, parentWindow);
                liveHwnd && ::IsWindow(liveHwnd) != FALSE)
            {
                dockedWindows.push_back(window);
            }
        }

        if (dockedWindows.empty())
            return;

        std::stable_sort(
            dockedWindows.begin(),
            dockedWindows.end(),
            [](const auto* lhs, const auto* rhs)
            {
                return capture_dock_order(lhs) < capture_dock_order(rhs);
            });

        RECT clientRect{};
        if (!::GetClientRect(parentWindow, &clientRect))
            return;

        const int clientW = (std::max)(1, static_cast<int>(clientRect.right - clientRect.left));
        const int clientH = (std::max)(1, static_cast<int>(clientRect.bottom - clientRect.top));

        int cols = 1;
        int rows = 1;
        const int total = static_cast<int>(dockedWindows.size());
        while (cols * rows < total)
            (cols <= rows ? ++cols : ++rows);

        const int cellW = (std::max)(1, clientW / cols);
        const int cellH = (std::max)(1, clientH / rows);

        for (std::size_t i = 0; i < dockedWindows.size(); ++i)
        {
            auto* window = dockedWindows[i];
            const HWND liveHwnd = capture_dock_slot_handle(window, parentWindow);
            if (!liveHwnd || ::IsWindow(liveHwnd) == FALSE)
                continue;

            const int column = static_cast<int>(i) % cols;
            const int row = static_cast<int>(i) / cols;
            ::SetWindowPos(
                liveHwnd,
                nullptr,
                column * cellW,
                row * cellH,
                cellW,
                cellH,
                SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);
            if (window->context)
            {
                if (auto liveContext = std::reinterpret_pointer_cast<epochengine::core::Context>(window->context))
                {
                    liveContext->width = cellW;
                    liveContext->height = cellH;
                }
            }
            mgr.HandleResize(liveHwnd, cellW, cellH);
            ::UpdateWindow(liveHwnd);
            ::RedrawWindow(
                liveHwnd,
                nullptr,
                nullptr,
                RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
            if (window->hwndChild
                && window->hwndChild != liveHwnd
                && ::IsWindow(window->hwndChild) != FALSE)
            {
                ::UpdateWindow(window->hwndChild);
                ::RedrawWindow(
                    window->hwndChild,
                    nullptr,
                    nullptr,
                    RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
            }
        }

        ::UpdateWindow(parentWindow);
        ::RedrawWindow(parentWindow, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
    }

    [[nodiscard]] inline bool parent_window_capture_layout_ready(
        epochengine::core::MultiContextManager& mgr) noexcept
    {
        const HWND parentWindow = mgr.GetParentWindow();
        if (!parentWindow || ::IsWindow(parentWindow) == FALSE)
            return false;

        RECT clientRect{};
        if (!::GetClientRect(parentWindow, &clientRect))
            return false;

        const int clientW = static_cast<int>(clientRect.right - clientRect.left);
        const int clientH = static_cast<int>(clientRect.bottom - clientRect.top);
        if (clientW <= 1 || clientH <= 1)
            return false;

        std::vector<const epochengine::core::WindowData*> dockedWindows;
        dockedWindows.reserve(mgr.GetWindows().size());
        for (const auto& ownedWindow : mgr.GetWindows())
        {
            const auto* window = ownedWindow.get();
            if (!window)
                continue;

            if (const HWND liveHwnd = capture_dock_slot_handle(window, parentWindow);
                liveHwnd && ::IsWindow(liveHwnd) != FALSE)
            {
                dockedWindows.push_back(window);
            }
        }

        if (dockedWindows.empty())
            return false;

        std::stable_sort(
            dockedWindows.begin(),
            dockedWindows.end(),
            [](const auto* lhs, const auto* rhs)
            {
                return capture_dock_order(lhs) < capture_dock_order(rhs);
            });

        int cols = 1;
        int rows = 1;
        const int total = static_cast<int>(dockedWindows.size());
        while (cols * rows < total)
            (cols <= rows ? ++cols : ++rows);

        const int cellW = (std::max)(1, clientW / cols);
        const int cellH = (std::max)(1, clientH / rows);
        constexpr int kSizeTolerance = 96;
        constexpr int kPositionTolerance = 96;

        for (std::size_t i = 0; i < dockedWindows.size(); ++i)
        {
            const auto* window = dockedWindows[i];
            const HWND liveHwnd = capture_dock_slot_handle(window, parentWindow);
            if (!liveHwnd || ::IsWindow(liveHwnd) == FALSE)
                return false;

            RECT liveRect{};
            if (!::GetWindowRect(liveHwnd, &liveRect))
                return false;

            POINT topLeft{ liveRect.left, liveRect.top };
            if (!::ScreenToClient(parentWindow, &topLeft))
                return false;

            const int liveW = static_cast<int>(liveRect.right - liveRect.left);
            const int liveH = static_cast<int>(liveRect.bottom - liveRect.top);
            const int column = static_cast<int>(i) % cols;
            const int row = static_cast<int>(i) / cols;
            const int expectedX = column * cellW;
            const int expectedY = row * cellH;

            if ((std::abs)(liveW - cellW) > kSizeTolerance
                || (std::abs)(liveH - cellH) > kSizeTolerance
                || (std::abs)(topLeft.x - expectedX) > kPositionTolerance
                || (std::abs)(topLeft.y - expectedY) > kPositionTolerance)
            {
                return false;
            }
        }

        return true;
    }

    inline void capture_parent_window_if_requested(
        epochengine::core::MultiContextManager& mgr,
        const std::string_view logSystem) noexcept
    {
        if (!cli::capture_requested)
            return;

        const HWND parentWindow = mgr.GetParentWindow();
        const auto capturePath = engine_owned_smoke_capture_path();
        if (!parentWindow || capturePath.empty())
            return;

        const bool captured = capture_window_to_bmp(parentWindow, capturePath);
        logger::get(std::string(logSystem)).log(
            captured ? logger::LogLevel::INFO : logger::LogLevel::WARN,
            std::string(captured ? "Captured engine-owned parent window proof to " : "Failed engine-owned parent window proof capture at ")
                + capturePath.string(),
            std::source_location::current());
    }
#endif

    struct TextureUploadTask
    {
        int w{};
        int h{};
        const void* pixels{};
    };

    struct TextureUploadQueue
    {
        std::queue<TextureUploadTask> tasks;
        std::mutex mtx;

        void push(TextureUploadTask&& task)
        {
            std::lock_guard lock(mtx);
            tasks.push(std::move(task));
        }

        std::optional<TextureUploadTask> try_pop()
        {
            std::lock_guard lock(mtx);
            if (tasks.empty()) return {};
            auto task = tasks.front();
            tasks.pop();
            return task;
        }
    };

    inline std::vector<std::unique_ptr<TextureUploadQueue>> uploadQueues;

#if defined(_WIN32)
    inline void ShowConsole()
    {
#if defined(_DEBUG)
        AllocConsole();
        FILE* fp = nullptr;
        freopen_s(&fp, "CONIN$", "r", stdin);
        freopen_s(&fp, "CONOUT$", "w", stdout);
        freopen_s(&fp, "CONOUT$", "w", stderr);
#else
        FreeConsole();
#endif
    }
#endif

    namespace engine
    {
        constexpr std::string_view kEngineLog = "Engine.Runtime";
        constexpr std::string_view kEditorLog = "Engine.Editor";

#if defined(_WIN32)
        LONG WINAPI log_unhandled_windows_exception(EXCEPTION_POINTERS* exception_info) noexcept
        {
            try
            {
                const auto* record = exception_info ? exception_info->ExceptionRecord : nullptr;
                const unsigned long code = record ? record->ExceptionCode : 0ul;
                const auto address = reinterpret_cast<std::uintptr_t>(record ? record->ExceptionAddress : nullptr);
                logger::get(kEditorLog).logf(
                    logger::LogLevel::Error,
                    std::source_location::current(),
                    "Unhandled Windows exception reached the editor process: code=0x{:08X} address=0x{:016X}",
                    code,
                    static_cast<unsigned long long>(address));
            }
            catch (...)
            {
            }

            return EXCEPTION_CONTINUE_SEARCH;
        }

        void install_windows_crash_breadcrumbs() noexcept
        {
            ::SetUnhandledExceptionFilter(log_unhandled_windows_exception);
        }

        [[nodiscard]] bool current_module_directory_noexcept(std::wstring& directory) noexcept
        {
            try
            {
                wchar_t module_path[32768]{};
                const DWORD length = ::GetModuleFileNameW(
                    nullptr,
                    module_path,
                    static_cast<DWORD>(std::size(module_path)));
                if (length == 0u || length >= static_cast<DWORD>(std::size(module_path)))
                    return false;

                wchar_t* slash = nullptr;
                for (wchar_t* cursor = module_path; *cursor != L'\0'; ++cursor)
                {
                    if (*cursor == L'\\' || *cursor == L'/')
                        slash = cursor;
                }
                if (slash == nullptr)
                    return false;

                *(slash + 1) = L'\0';
                directory.assign(module_path);
                return true;
            }
            catch (...)
            {
                directory.clear();
                return false;
            }
        }

        [[nodiscard]] bool current_log_directory_noexcept(std::wstring& directory) noexcept
        {
            try
            {
                std::wstring module_directory;
                if (!current_module_directory_noexcept(module_directory))
                    return false;

                directory = module_directory;
                directory += L"logs";
                if (::CreateDirectoryW(directory.c_str(), nullptr) == FALSE)
                {
                    const DWORD error = ::GetLastError();
                    if (error != ERROR_ALREADY_EXISTS)
                        return false;
                }

                const DWORD attributes = ::GetFileAttributesW(directory.c_str());
                if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0u)
                    return false;

                directory += L"\\";
                return true;
            }
            catch (...)
            {
                directory.clear();
                return false;
            }
        }

        void append_raw_text_file_noexcept(const std::wstring& path, const std::string_view text) noexcept
        {
            HANDLE file = ::CreateFileW(
                path.c_str(),
                FILE_APPEND_DATA,
                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                nullptr,
                OPEN_ALWAYS,
                FILE_ATTRIBUTE_NORMAL,
                nullptr);
            if (file == INVALID_HANDLE_VALUE)
                return;

            DWORD written = 0u;
            (void)::WriteFile(
                file,
                text.data(),
                static_cast<DWORD>(text.size()),
                &written,
                nullptr);
            ::CloseHandle(file);
        }

        void append_launcher_cancel_breadcrumb_noexcept(const std::string_view text) noexcept
        {
            try
            {
                std::wstring log_directory;
                if (!current_log_directory_noexcept(log_directory))
                    return;

                SYSTEMTIME now{};
                ::GetLocalTime(&now);
                char prefix[96]{};
                const int prefix_length = ::wsprintfA(
                    prefix,
                    "%04u-%02u-%02u %02u:%02u:%02u.%03u [launcher-cancel] ",
                    static_cast<unsigned>(now.wYear),
                    static_cast<unsigned>(now.wMonth),
                    static_cast<unsigned>(now.wDay),
                    static_cast<unsigned>(now.wHour),
                    static_cast<unsigned>(now.wMinute),
                    static_cast<unsigned>(now.wSecond),
                    static_cast<unsigned>(now.wMilliseconds));
                if (prefix_length <= 0)
                    return;

                std::wstring path = log_directory;
                path += L"epoch_launcher_update_cancel.log";

                append_raw_text_file_noexcept(path, std::string_view{ prefix, static_cast<std::size_t>(prefix_length) });
                append_raw_text_file_noexcept(path, text);
                append_raw_text_file_noexcept(path, "\r\n");
            }
            catch (...)
            {
            }
        }

#endif

        [[nodiscard]] std::unique_ptr<epochengine::scene::Scene> make_scene_from_id(std::string_view scene_id);
        [[nodiscard]] bool launch_project_child_process(std::string_view launch_argument);

        [[nodiscard]] std::string_view context_type_label(epochengine::core::ContextType type) noexcept
        {
            switch (type)
            {
            case epochengine::core::ContextType::DirectX: return "DirectX";
            case epochengine::core::ContextType::OpenGL: return "OpenGL";
            case epochengine::core::ContextType::SDL: return "SDL";
            case epochengine::core::ContextType::SFML: return "SFML";
            case epochengine::core::ContextType::RayLib: return "Raylib";
            case epochengine::core::ContextType::Vulkan: return "Vulkan";
            case epochengine::core::ContextType::Software: return "Software";
            default: return "Unknown";
            }
        }

        [[nodiscard]] epochengine::core::ContextType context_type_from_backend_token(
            const std::string& token) noexcept
        {
            if (token == "opengl" || token == "gl")
                return epochengine::core::ContextType::OpenGL;
            if (token == "sdl")
                return epochengine::core::ContextType::SDL;
            if (token == "sfml")
                return epochengine::core::ContextType::SFML;
            if (token == "raylib" || token == "ray")
                return epochengine::core::ContextType::RayLib;
            if (token == "vulkan" || token == "vk")
                return epochengine::core::ContextType::Vulkan;
            if (token == "directx" || token == "dx" || token == "d3d" || token == "d3d11")
                return epochengine::core::ContextType::DirectX;
            if (token == "software" || token == "cpu")
                return epochengine::core::ContextType::Software;
            return epochengine::core::ContextType::None;
        }

        [[nodiscard]] bool is_context_driver_candidate(epochengine::core::ContextType type) noexcept
        {
            switch (type)
            {
            case epochengine::core::ContextType::OpenGL:
            case epochengine::core::ContextType::SDL:
            case epochengine::core::ContextType::SFML:
            case epochengine::core::ContextType::RayLib:
            case epochengine::core::ContextType::Vulkan:
            case epochengine::core::ContextType::DirectX:
            case epochengine::core::ContextType::Software:
                return true;
            default:
                return false;
            }
        }

        struct DetachedPanelRouteMetadata
        {
            std::string_view title{};
            int width{};
            int height{};
            std::string_view open_success{};
            std::string_view open_failure{};
        };

        [[nodiscard]] DetachedPanelRouteMetadata detached_panel_route_metadata(std::string_view route) noexcept
        {
            const bool outlinerTool =
                route == "pane.world_outliner"
                || route == "pane.asset_browser"
                || route == "pane.gui_hierarchy"
                || route == "pane.script_browser"
                || route == "pane.tile_map"
                || route == "pane.outliner";
            if (outlinerTool)
            {
                const std::string_view title =
                    route == "pane.asset_browser" ? "Epoch Assets"
                    : route == "pane.gui_hierarchy" ? "Epoch GUI"
                    : route == "pane.script_browser" ? "Epoch Scripts"
                    : route == "pane.tile_map" ? "Epoch Map"
                    : "Epoch World Outliner";
                return DetachedPanelRouteMetadata{
                    .title = title,
                    .width = 520,
                    .height = 660,
                    .open_success = "Tool window popout requested.",
                    .open_failure = "Tool window popout request failed."
                };
            }
            const bool inspectorTool =
                route == "pane.properties"
                || route == "pane.world_settings"
                || route == "pane.inspector";
            if (inspectorTool)
            {
                return DetachedPanelRouteMetadata{
                    .title = route == "pane.world_settings"
                        ? "Epoch World Settings"
                        : "Epoch Properties",
                    .width = 460,
                    .height = 620,
                    .open_success = "Inspector tool popout requested.",
                    .open_failure = "Inspector tool popout request failed."
                };
            }
            const bool statusTool =
                route == "pane.output"
                || route == "pane.project_status"
                || route == "pane.asset_status"
                || route == "pane.ai_status"
                || route == "pane.systems_status"
                || route == "pane.console";
            if (statusTool)
            {
                const std::string_view title =
                    route == "pane.project_status" ? "Epoch Project"
                    : route == "pane.asset_status" ? "Epoch Asset Status"
                    : route == "pane.ai_status" ? "Epoch AI Status"
                    : route == "pane.systems_status" ? "Epoch Systems Status"
                    : "Epoch Output";
                return DetachedPanelRouteMetadata{
                    .title = title,
                    .width = 760,
                    .height = 420,
                    .open_success = "Status tool popout requested.",
                    .open_failure = "Status tool popout request failed."
                };
            }
            if (route == "pane.ai_chat")
            {
                return DetachedPanelRouteMetadata{
                    .title = "Epoch AI Chat",
                    .width = 520,
                    .height = 520,
                    .open_success = "AI Chat popout requested.",
                    .open_failure = "AI Chat popout request failed."
                };
            }

            if (route == "floating.gui")
            {
                return DetachedPanelRouteMetadata{
                    .title = "Epoch Floating GUI",
                    .width = 560,
                    .height = 360,
                    .open_success = "Floating GUI routed window requested.",
                    .open_failure = "Floating GUI routed window request failed."
                };
            }

            return DetachedPanelRouteMetadata{
                .title = "Epoch Context Driver",
                .width = 720,
                .height = 440,
                .open_success = "Detached Context Driver window requested.",
                .open_failure = "Detached Context Driver window request failed."
            };
        }

        [[nodiscard]] int context_driver_priority(epochengine::core::ContextType type) noexcept
        {
            switch (type)
            {
            case epochengine::core::ContextType::DirectX: return 0;
            case epochengine::core::ContextType::OpenGL: return 1;
            case epochengine::core::ContextType::SDL: return 2;
            case epochengine::core::ContextType::SFML: return 3;
            case epochengine::core::ContextType::RayLib: return 4;
            case epochengine::core::ContextType::Vulkan: return 5;
            case epochengine::core::ContextType::Software: return 6;
            default: return 100;
            }
        }

        [[nodiscard]] epochengine::core::ContextType choose_context_driver_type(
            epochengine::core::ContextType preferred)
        {
            epochengine::core::InitializeAllContexts();

            std::vector<epochengine::core::ContextType> available;
            {
                std::shared_lock lock(epochengine::core::g_backendsMutex);
                available.reserve(epochengine::core::g_backends.size());
                for (const auto& [type, backend] : epochengine::core::g_backends)
                {
                    if (backend.master && is_context_driver_candidate(type))
                        available.push_back(type);
                }
            }

            if (available.empty())
                return preferred == epochengine::core::ContextType::None
                    ? epochengine::core::ContextType::OpenGL
                    : preferred;

            if (preferred != epochengine::core::ContextType::None
                && std::find(available.begin(), available.end(), preferred) != available.end())
                return preferred;

            return *std::min_element(available.begin(), available.end(), [](auto a, auto b)
            {
                return context_driver_priority(a) < context_driver_priority(b);
            });
        }

        [[nodiscard]] bool context_driver_type_available(epochengine::core::ContextType requested)
        {
            if (requested == epochengine::core::ContextType::None
                || !is_context_driver_candidate(requested))
            {
                return false;
            }

            epochengine::core::InitializeAllContexts();
            std::shared_lock lock(epochengine::core::g_backendsMutex);
            const auto it = epochengine::core::g_backends.find(requested);
            return it != epochengine::core::g_backends.end() && it->second.master;
        }

        [[nodiscard]] std::optional<epochengine::core::ContextType> resolve_context_driver_type(
            epochengine::core::ContextType requested,
            epochengine::core::ContextType fallback)
        {
            if (requested == epochengine::core::ContextType::None)
                return choose_context_driver_type(fallback);

            if (context_driver_type_available(requested))
                return requested;

            return std::nullopt;
        }

        template <typename PumpFunc>
        int RunEditorInterfaceLoop(MultiContextManager& mgr, PumpFunc&& pump_events)
        {
            enum class EditorSceneState
            {
                Editor,
                Game,
                Exit
            };

            EditorSceneState state = EditorSceneState::Editor;
            std::unique_ptr<epochengine::scene::Scene> active_scene{};

            auto collect_backend_contexts = []()
                {
                    using ContextGroup = std::pair<
                        epochengine::core::ContextType,
                        std::vector<std::shared_ptr<epochengine::core::Context>>
                    >;

                    std::vector<ContextGroup> snapshot;

                    {
                        std::shared_lock lock(epochengine::core::g_backendsMutex);
                        snapshot.reserve(epochengine::core::g_backends.size());

                        for (auto& [type, backendSlot] : epochengine::core::g_backends)
                        {
                            std::vector<std::shared_ptr<epochengine::core::Context>> contexts;
                            contexts.reserve(1 + backendSlot.duplicates.size());

                            if (backendSlot.master) contexts.push_back(backendSlot.master);
                            for (auto& dup : backendSlot.duplicates) contexts.push_back(dup);

                            snapshot.emplace_back(type, std::move(contexts));
                        }
                    }

                    return snapshot;
                };

            std::unordered_map<Context*, timing::Clock::time_point> last_frame_times;
            bool running = true;
            std::uint64_t frame_count = 0;
            const std::uint64_t smoke_max_frames = smoke_frame_budget();
            auto pump = std::forward<PumpFunc>(pump_events);

            while (running)
            {
                if (frame_count++ >= smoke_max_frames)
                {
                    running = false;
                    break;
                }
                if (!pump())
                {
                    running = false;
                    break;
                }

                mgr.CleanupFinishedWindows();

                auto snapshot = collect_backend_contexts();
                bool any_context_alive = false;
#if !defined(EPOCH_SINGLE_PARENT) || (EPOCH_SINGLE_PARENT == 0)
                std::size_t active_context_count = 0;
                for (auto& [_, contexts] : snapshot)
                {
                    for (auto& ctx : contexts)
                    {
                        if (ctx) ++active_context_count;
                    }
                }

                bool raylib_close_from_window = false;
#if defined(EPOCH_USING_RAYLIB) && (EPOCH_USING_RAYLIB == 1)
                {
                    const auto& raylib_state = epochengine::raylibstate::s_raylibstate;
                    raylib_close_from_window = raylib_state.running && !raylib_state.renderingActive;

                    if (raylib_close_from_window)
                        epochengine::raylibstate::s_raylibstate.renderingActive = false;
                }
#endif
#endif
                for (auto& [type, contexts] : snapshot)
                {
                    auto update_on_ctx = [&](std::shared_ptr<Context> ctx) -> bool
                        {
                            if (!ctx) return true;

                            auto* win = mgr.findWindowByContext(ctx);
                            if (!win) return true;

                            bool ctx_running = win->running;

                            const auto now = timing::Clock::now();
                            const auto raw = ctx.get();

                            float dt = 0.0f;
                            auto [it, inserted] = last_frame_times.emplace(raw, now);
                            if (!inserted)
                            {
                                dt = std::chrono::duration<float>(now - it->second).count();
                                it->second = now;
                            }

                            auto begin_scene = [&](auto make_scene, const char* label)
                                {
                                    auto clear_commands = [](const std::shared_ptr<Context>& c)
                                        {
                                            if (c && c->windowData) c->windowData->commandQueue.clear();
                                        };

                                    auto snap2 = collect_backend_contexts();
                                    for (auto& [__, group] : snap2)
                                        for (auto& c : group)
                                            clear_commands(c);

                                    if (active_scene)
                                        active_scene->unload();

                                    active_scene = make_scene();
                                    active_scene->load();
                                    state = EditorSceneState::Game;
                                    logger::get(kEditorLog).logf(
                                        logger::LogLevel::INFO,
                                        std::source_location::current(),
                                        "Launching {} scene.",
                                        label);
                                };

                            auto launch_requested_game = [&](std::string_view scene_id)
                                  {
                                    if (scene_id.starts_with("project-exe:"))
                                    {
                                        (void)launch_project_child_process(scene_id);
                                        return;
                                    }

                                    if (auto scene = make_scene_from_id(scene_id))
                                    {
                                        auto label = std::string(scene_id);
                                        begin_scene(
                                            [captured = std::move(scene)]() mutable { return std::move(captured); },
                                            label.c_str());
                                    }
                                    else
                                    {
                                        logger::get(kEditorLog).logf(
                                            logger::LogLevel::Error,
                                            std::source_location::current(),
                                            "Editor rejected unknown play target '{}'.",
                                            scene_id);
                                    }
                                 };

                            if (state == EditorSceneState::Editor)
                            {
                                int mx = 0;
                                int my = 0;
                                ctx->get_mouse_position_safe(mx, my);

                                const gui::Vec2 mouse_pos{
                                    static_cast<float>(mx),
                                    static_cast<float>(my)
                                };

                                const bool mouse_left_down =
                                    ctx->is_mouse_button_held_safe(epochengine::input::MouseButton::MouseLeft);
                                const bool up_pressed =
                                    epochengine::input::keyPressed.test(epochengine::input::Key::Up);
                                const bool down_pressed =
                                    epochengine::input::keyPressed.test(epochengine::input::Key::Down);
                                const bool left_pressed =
                                    epochengine::input::keyPressed.test(epochengine::input::Key::Left);
                                const bool right_pressed =
                                    epochengine::input::keyPressed.test(epochengine::input::Key::Right);
                                const bool enter_pressed =
                                    epochengine::input::keyPressed.test(epochengine::input::Key::Enter);

                                ctx->set_scene_preview_mode(core::ScenePreviewMode::Editor);
                                clear_before_ui_frame(ctx);
                                gui::begin_frame(ctx, dt, mouse_pos, mouse_left_down);
                                const auto editor_frame = epochengine::editor_run(ctx);

                                switch (editor_frame.command)
                                {
                                case epochengine::EditorCommand::OpenProject:
                                    logger::get(kEditorLog).logf(
                                        logger::LogLevel::INFO,
                                        std::source_location::current(),
                                        "Open Project: {}",
                                        editor_frame.command_argument);
                                    break;
                                case epochengine::EditorCommand::Settings:
                                    logger::get(kEditorLog).log(
                                        logger::LogLevel::INFO,
                                        "Settings selected.",
                                        std::source_location::current());
                                    break;
                                case epochengine::EditorCommand::RunGame:
                                    launch_requested_game(editor_frame.command_argument);
                                    break;
                                case epochengine::EditorCommand::SwitchContext:
                                {
                                    const auto resolvedType = resolve_context_driver_type(
                                        editor_frame.requested_context_type,
                                        type);
                                    if (!resolvedType)
                                    {
                                        epochengine::editor_set_context_selection_status(
                                            ctx.get(),
                                            "Context switch failed: requested backend is unavailable in this build/session.");
                                        logger::get(kEditorLog).logf(
                                            logger::LogLevel::Error,
                                            std::source_location::current(),
                                            "Context selector rejected unavailable explicit {} backend in the legacy editor loop.",
                                            context_type_label(editor_frame.requested_context_type));
                                        break;
                                    }
                                    const auto requestedType = *resolvedType;
                                    epochengine::editor_set_context_selection_status(
                                        ctx.get(),
                                        requestedType == type
                                            ? std::string{ "Already running in the active " }
                                                + std::string{ context_type_label(requestedType) }
                                                + " editor context."
                                            : std::string{ "Single-context editor host cannot switch to " }
                                                + std::string{ context_type_label(requestedType) }
                                                + " without a live backend window; no restart or fake switch was performed.");
                                    logger::get(kEditorLog).logf(
                                        logger::LogLevel::INFO,
                                        std::source_location::current(),
                                        "Context selector chose {} in the legacy editor loop; no detached window was opened.",
                                        context_type_label(requestedType));
                                    break;
                                }
                                case epochengine::EditorCommand::OpenContextWindow:
                                {
                                    const auto resolvedType = resolve_context_driver_type(
                                        editor_frame.requested_context_type,
                                        type);
                                    if (!resolvedType)
                                    {
                                        logger::get(kEditorLog).logf(
                                            logger::LogLevel::Error,
                                            std::source_location::current(),
                                            "Detached context request rejected unavailable explicit {} backend.",
                                            context_type_label(editor_frame.requested_context_type));
                                        break;
                                    }
                                    const auto requestedType = *resolvedType;
                                    const std::string route = editor_frame.command_argument.empty()
                                        ? std::string{ "context.driver" }
                                        : editor_frame.command_argument;
                                    const auto routeMeta = detached_panel_route_metadata(route);
                                     const bool opened = mgr.OpenDetachedContextWindow(
                                         epochengine::core::DetachedContextWindowRequest{
                                             .type = requestedType,
                                             .title = std::string{ routeMeta.title },
                                             .gui_route = route,
                                             .width = routeMeta.width,
                                             .height = routeMeta.height
                                         });
                                    if (opened)
                                        epochengine::editor_mark_context_panel_detached(route, true);
                                     const std::string logLine = opened
                                         ? std::string{ routeMeta.open_success }
                                         : std::string{ routeMeta.open_failure };
                                    logger::get(kEditorLog).log(
                                        opened ? logger::LogLevel::INFO : logger::LogLevel::Error,
                                        logLine,
                                        std::source_location::current());
                                    break;
                                }
                                case epochengine::EditorCommand::Exit:
                                    state = EditorSceneState::Exit;
                                    running = false;
                                    break;
                                case epochengine::EditorCommand::None:
                                default:
                                    break;
                                }

                                gui::end_frame();
                                ctx->present_safe();
                            }
                            else if (state == EditorSceneState::Game)
                            {
                                ctx->clear_scene_viewport();
                                ctx->set_scene_preview_mode(core::ScenePreviewMode::None);
                                if (active_scene)
                                {
                                    ctx_running = active_scene->frame(ctx, win);
                                    if (!ctx_running)
                                    {
                                        active_scene->unload();
                                        active_scene.reset();
                                        state = EditorSceneState::Editor;
                                        ctx_running = true;
                                    }
                                }
                                else
                                {
                                    state = EditorSceneState::Editor;
                                    ctx_running = true;
                                }
                            }
                            else if (state == EditorSceneState::Exit)
                            {
                                running = false;
                            }

                            if (!ctx_running)
                            {
                                epochengine::cleanup_chat_context(raw);
                                last_frame_times.erase(raw);
                            }

                            return ctx_running;
                        };

#if !defined(EPOCH_SINGLE_PARENT) && (EPOCH_SINGLE_PARENT == 1)
                    if (!contexts.empty())
                    {
                        auto master = contexts.front();
                        if (master && !update_on_ctx(master)) { running = false; break; }

                        for (std::size_t i = 1; i < contexts.size(); ++i)
                            if (!update_on_ctx(contexts[i])) running = false;
                    }
#else
                    bool any_alive = false;
                    for (std::size_t i = 0; i < contexts.size(); ++i)
                    {
                        auto& ctx = contexts[i];
                        if (!ctx) continue;

                        const bool alive = update_on_ctx(ctx);
                        if (alive) any_alive = true;
                    }
                    if (any_alive) any_context_alive = true;
#endif
                    if (!running) break;
                }
#if !defined(EPOCH_SINGLE_PARENT) || (EPOCH_SINGLE_PARENT == 0)
                if (!any_context_alive)
                {
#if defined(EPOCH_USING_RAYLIB) && (EPOCH_USING_RAYLIB == 1)
                    if (raylib_close_from_window && active_context_count > 1)
                    {
                        running = true;
                    }
                    else
#endif
                    {
                        running = false;
                    }
                }
#endif

                std::this_thread::sleep_for(std::chrono::milliseconds(16));
            }

            if (active_scene)
            {
                active_scene->unload();
                active_scene.reset();
            }

            auto snapshot2 = collect_backend_contexts();
            for (auto& [type, contexts] : snapshot2)
            {
                auto cleanup_backend = [&](std::shared_ptr<epochengine::core::Context> ctx)
                    {
                        if (!ctx) return;

                        epochengine::cleanup_chat_context(ctx.get());

                        switch (type)
                        {
#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)
                        case epochengine::core::ContextType::OpenGL:
                            epochengine::openglcontext::opengl_cleanup(ctx);
                            break;
#endif
#if defined(EPOCH_USING_SOFTWARE_RENDERER) && (EPOCH_USING_SOFTWARE_RENDERER == 1)
                        case epochengine::core::ContextType::Software:
                            // epochengine::anativecontext::softrenderer_cleanup(ctx);
                            break;
#endif
#if defined(EPOCH_USING_SDL) && (EPOCH_USING_SDL == 1)
                        case epochengine::core::ContextType::SDL:
                            //  epochengine::sdlcontext::sdl_cleanup(ctx);
                            break;
#endif
#if defined(EPOCH_USING_SFML) && (EPOCH_USING_SFML == 1)
                        case epochengine::core::ContextType::SFML:
                            epochengine::sfmlcontext::sfml_cleanup(ctx);
                            break;
#endif
#if defined(EPOCH_USING_RAYLIB) && (EPOCH_USING_RAYLIB == 1)
                        case epochengine::core::ContextType::RayLib:
                            epochengine::raylibcontext::raylib_cleanup(ctx);
                            break;
#endif
                        case epochengine::core::ContextType::Noop:
                            break;
                        default:
                            break;
                        }
                    };

                for (auto& ctx : contexts) cleanup_backend(ctx);
            }

            epochengine::shutdown_chat_system();
            mgr.StopAll();

            return 0;
        }

        template <typename PumpFunc>
        int RunMenuAndGamesLoop(MultiContextManager& mgr, PumpFunc&& pump_events)
        {
            enum class SceneID
            {
                Menu,
                Snake,
                Tetris,
                Pacman,
                Frogger,
                Sokoban,
                Match3,
                Sliding,
                Minesweeper,
                Game2048,
                Sandsim,
                Cellular,
                Exit
            };

            SceneID scene_id = SceneID::Menu;
            std::unique_ptr<epochengine::scene::Scene> active_scene{};

            using MenuOverlay = epochengine::menu::MenuOverlay;
            MenuOverlay menu{};
            menu.set_max_columns(epochengine::core::cli::menu_columns);

            auto collect_backend_contexts = []()
                {
                    using ContextGroup = std::pair<
                        epochengine::core::ContextType,
                        std::vector<std::shared_ptr<epochengine::core::Context>>
                    >;

                    std::vector<ContextGroup> snapshot;

                    {
                        std::shared_lock lock(epochengine::core::g_backendsMutex);
                        snapshot.reserve(epochengine::core::g_backends.size());

                        for (auto& [type, backendSlot] : epochengine::core::g_backends)
                        {
                            std::vector<std::shared_ptr<epochengine::core::Context>> contexts;
                            contexts.reserve(1 + backendSlot.duplicates.size());

                            if (backendSlot.master) contexts.push_back(backendSlot.master);
                            for (auto& dup : backendSlot.duplicates) contexts.push_back(dup);

                            snapshot.emplace_back(type, std::move(contexts));
                        }
                    }

                    return snapshot;
                };

            auto init_menu = [&]()
                {
                    auto snapshot = collect_backend_contexts();
                    for (auto& [_, contexts] : snapshot)
                        for (auto& ctx : contexts)
                            if (ctx) menu.initialize(ctx);
                };

            init_menu();

            std::unordered_map<Context*, timing::Clock::time_point> last_frame_times;
            bool running = true;
            std::uint64_t frame_count = 0;
            const std::uint64_t smoke_max_frames = smoke_frame_budget();
            auto pump = std::forward<PumpFunc>(pump_events);

            while (running)
            {
                if (frame_count++ >= smoke_max_frames)
                {
                    running = false;
                    break;
                }
                if (!pump())
                {
                    running = false;
                    break;
                }

                mgr.CleanupFinishedWindows();

                auto snapshot = collect_backend_contexts();
#if !defined(EPOCH_SINGLE_PARENT) || (EPOCH_SINGLE_PARENT == 0)
                bool any_context_alive = false;
                std::size_t active_context_count = 0;
                for (auto& [_, contexts] : snapshot)
                {
                    for (auto& ctx : contexts)
                    {
                        if (ctx) ++active_context_count;
                    }
                }

                bool raylib_close_from_window = false;
#if defined(EPOCH_USING_RAYLIB) && (EPOCH_USING_RAYLIB == 1)
                {
                    const auto& raylib_state = epochengine::raylibstate::s_raylibstate;
                    raylib_close_from_window = raylib_state.running && !raylib_state.renderingActive;

                    if (raylib_close_from_window)
                        epochengine::raylibstate::s_raylibstate.renderingActive = false;
                }
#endif
#endif
                for (auto& [type, contexts] : snapshot)
                {
                    auto update_on_ctx = [&](std::shared_ptr<Context> ctx) -> bool
                        {
                            if (!ctx) return true;

                            // FIX: never touch ctx->hwnd (private). Manager can resolve by context.
                            auto* win = mgr.findWindowByContext(ctx);
                            if (!win) return true;

                            bool ctx_running = win->running;

                            const auto now = timing::Clock::now();
                            const auto raw = ctx.get();

                            float dt = 0.0f;
                            auto [it, inserted] = last_frame_times.emplace(raw, now);
                            if (!inserted)
                            {
                                dt = std::chrono::duration<float>(now - it->second).count();
                                it->second = now;
                            }

                            auto begin_scene = [&](auto make_scene, SceneID id)
                                {
                                    auto clear_commands = [](const std::shared_ptr<Context>& c)
                                        {
                                            // If your WindowData/queue moved behind accessors, change this here.
                                            if (c && c->windowData) c->windowData->commandQueue.clear();
                                        };

                                    auto snap2 = collect_backend_contexts();
                                    for (auto& [__, group] : snap2)
                                        for (auto& c : group)
                                            clear_commands(c);

                                    menu.cleanup();

                                    if (active_scene)
                                        active_scene->unload();

                                    active_scene = make_scene();
                                    active_scene->load();
                                    scene_id = id;
                                };

                            switch (scene_id)
                            {
                            case SceneID::Menu:
                            {
                                int mx = 0, my = 0;
                                ctx->get_mouse_position_safe(mx, my);

                                const gui::Vec2 mouse_pos{
                                    static_cast<float>(mx),
                                    static_cast<float>(my)
                                };

                                const bool mouse_left_down =
                                    ctx->is_mouse_button_held_safe(epochengine::input::MouseButton::MouseLeft);
                                const bool up_pressed =
                                    epochengine::input::keyPressed.test(epochengine::input::Key::Up);
                                const bool down_pressed =
                                    epochengine::input::keyPressed.test(epochengine::input::Key::Down);
                                const bool left_pressed =
                                    epochengine::input::keyPressed.test(epochengine::input::Key::Left);
                                const bool right_pressed =
                                    epochengine::input::keyPressed.test(epochengine::input::Key::Right);
                                const bool enter_pressed =
                                    epochengine::input::keyPressed.test(epochengine::input::Key::Enter);

                                ctx->clear_scene_viewport();
                                ctx->set_scene_preview_mode(core::ScenePreviewMode::None);
                                clear_before_ui_frame(ctx);
                                gui::begin_frame(ctx, dt, mouse_pos, mouse_left_down);
                                auto choice = menu.update_and_draw(ctx, win, dt, up_pressed, down_pressed, left_pressed, right_pressed, enter_pressed);
                                gui::end_frame();
                                ctx->present_safe();

                                if (choice)
                                {
                                    using epochengine::menu::Choice;

                                    if (*choice == Choice::Snake)
                                        begin_scene([] { return std::make_unique<epochengine::snakelike::SnakeLikeScene>(); }, SceneID::Snake);
                                    else if (*choice == Choice::Tetris)
                                        begin_scene([] { return std::make_unique<epochengine::tetrislike::TetrisLikeScene>(); }, SceneID::Tetris);
                                    else if (*choice == Choice::Frogger)
                                        begin_scene([] { return std::make_unique<epochengine::froggerlike::FroggerLikeScene>(); }, SceneID::Frogger);
                                    else if (*choice == Choice::Pacman)
                                        begin_scene([] { return std::make_unique<epochengine::pacmanlike::PacmanLikeScene>(); }, SceneID::Pacman);
                                    else if (*choice == Choice::Sokoban)
                                        begin_scene([] { return std::make_unique<epochengine::sokobanlike::SokobanLikeScene>(); }, SceneID::Sokoban);
                                    else if (*choice == Choice::Bejeweled)
                                        begin_scene([] { return std::make_unique<epochengine::match3like::Match3LikeScene>(); }, SceneID::Match3);
                                    else if (*choice == Choice::Puzzle)
                                        begin_scene([] { return std::make_unique<epochengine::slidinglike::SlidingPuzzleLikeScene>(); }, SceneID::Sliding);
                                    else if (*choice == Choice::Minesweep)
                                        begin_scene([] { return std::make_unique<epochengine::minesweeperlike::MinesweeperLikeScene>(); }, SceneID::Minesweeper);
                                    else if (*choice == Choice::Fourty)
                                        begin_scene([] { return std::make_unique<epochengine::a2048like::A2048LikeScene>(); }, SceneID::Game2048);
                                    else if (*choice == Choice::Sandsim)
                                        begin_scene([] { return std::make_unique<epochengine::sandsim::SandSimScene>(); }, SceneID::Sandsim);
                                    else if (*choice == Choice::Cellular)
                                        begin_scene([] { return std::make_unique<epochengine::cellularsim::CellularSimScene>(); }, SceneID::Cellular);
                                    else if (*choice == Choice::Settings)
                                    {
                                        logger::get(kEngineLog).log(
                                            logger::LogLevel::INFO,
                                            "Launcher context switch selected, but this single-context session has no alternate live renderer window.",
                                            std::source_location::current());
                                    }
                                    else if (*choice == Choice::Exit)
                                    {
                                        scene_id = SceneID::Exit;
                                        running = false;
                                    }
                                }
                                break;
                            }

							// cascading case to reset to menu after game exit
                            case SceneID::Snake:
                            case SceneID::Tetris:
                            case SceneID::Pacman:
							case SceneID::Frogger:
                            case SceneID::Sokoban:
                            case SceneID::Match3:
                            case SceneID::Sliding:
                            case SceneID::Minesweeper:
                            case SceneID::Game2048:
                            case SceneID::Sandsim:
                            case SceneID::Cellular:
                            {
                                if (active_scene)
                                {
                                    ctx_running = active_scene->frame(ctx, win);
                                    if (!ctx_running)
                                    {
                                        active_scene->unload();
                                        active_scene.reset();
                                        scene_id = SceneID::Menu;
                                        init_menu();
                                    }
                                }
                                break;
                            }

                            case SceneID::Exit:
                                running = false;
                                break;
                            }

                            if (!ctx_running)
                            {
                                epochengine::cleanup_chat_context(raw);
                                last_frame_times.erase(raw);
                            }

                            return ctx_running;
                        };

#if defined(EPOCH_SINGLE_PARENT) && (EPOCH_SINGLE_PARENT == 1)
                    if (!contexts.empty())
                    {
                        auto master = contexts.front();
                        if (master && !update_on_ctx(master)) { running = false; break; }

                        for (std::size_t i = 1; i < contexts.size(); ++i)
                            if (!update_on_ctx(contexts[i])) running = false;
                    }
#else
                    bool any_alive = false;
                    for (std::size_t i = 0; i < contexts.size(); ++i)
                    {
                        auto& ctx = contexts[i];
                        if (!ctx) continue;

                        const bool alive = update_on_ctx(ctx);
                        if (alive) any_alive = true;
                    }
                    if (any_alive) any_context_alive = true;
#endif
                    if (!running) break;
                }
#if !defined(EPOCH_SINGLE_PARENT) || (EPOCH_SINGLE_PARENT == 0)
                if (!any_context_alive)
                {
#if defined(EPOCH_USING_RAYLIB) && (EPOCH_USING_RAYLIB == 1)
                    if (raylib_close_from_window && active_context_count > 1)
                    {
                        running = true;
                    }
                    else
#endif
                    {
                        running = false;
                    }
                }
#endif

                std::this_thread::sleep_for(std::chrono::milliseconds(16));
            }

            if (active_scene)
            {
                active_scene->unload();
                active_scene.reset();
            }

            menu.cleanup();

            // Backend cleanup
            auto snapshot2 = collect_backend_contexts();
            for (auto& [type, contexts] : snapshot2)
            {
                auto cleanup_backend = [&](std::shared_ptr<epochengine::core::Context> ctx)
                    {
                        if (!ctx) return;

                        epochengine::cleanup_chat_context(ctx.get());

                        switch (type)
                        {
#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)
                        case epochengine::core::ContextType::OpenGL:
                            epochengine::openglcontext::opengl_cleanup(ctx);
                            break;
#endif
#if defined(EPOCH_USING_SOFTWARE_RENDERER) && (EPOCH_USING_SOFTWARE_RENDERER == 1)
                        case epochengine::core::ContextType::Software:
                           // epochengine::anativecontext::softrenderer_cleanup(ctx);
                            break;
#endif
#if defined(EPOCH_USING_SDL) && (EPOCH_USING_SDL == 1)
                        case epochengine::core::ContextType::SDL:
                          //  epochengine::sdlcontext::sdl_cleanup(ctx);
                            break;
#endif
#if defined(EPOCH_USING_SFML) && (EPOCH_USING_SFML == 1)
                        case epochengine::core::ContextType::SFML:
                            epochengine::sfmlcontext::sfml_cleanup(ctx);
                            break;
#endif
#if defined(EPOCH_USING_RAYLIB) && (EPOCH_USING_RAYLIB == 1)
                        case epochengine::core::ContextType::RayLib:
                            epochengine::raylibcontext::raylib_cleanup(ctx);
                            break;
#endif


                        case epochengine::core::ContextType::Noop:
                            break;
                        default:
                            break;
                        }
                    };

                for (auto& ctx : contexts) cleanup_backend(ctx);
            }

            epochengine::shutdown_chat_system();
            mgr.StopAll();

            return 0;
        }

        enum class SessionMode
        {
            Editor,
            Menu,
            Scene,
            Exit
        };

        constexpr double kLauncherLoadingMinimumSeconds = 0.45;

        struct ContextSession
        {
            SessionMode mode{ SessionMode::Menu };
            SessionMode return_mode{ SessionMode::Menu };
            epochengine::menu::MenuOverlay menu{};
            std::unique_ptr<epochengine::scene::Scene> active_scene{};
            timing::Clock::time_point last_frame{};
            bool has_last_frame{ false };
            epochengine::core::time::simulation_clock simulation{};
            bool routed_gui_upload_refreshed{ false };
            bool shared_camera_bootstrap_applied{ false };
            std::optional<EditorApplicationKind> pending_editor_application{};
            std::optional<timing::Clock::time_point> launcher_loading_started{};
            std::uint64_t launcher_loading_required_batch_generation{};
        };

        struct PreviewLookState
        {
            gui::Vec2 last_mouse{};
            bool orbiting = false;
            bool panning = false;
            bool dollying = false;
            bool flying = false;
        };

        thread_local std::unordered_map<Context*, PreviewLookState> g_preview_look_states{};

        struct ProjectSceneLaunchOptions
        {
            std::string project_id{};
            epochengine::previewgrid::CameraMode camera_mode{ epochengine::previewgrid::CameraMode::Editor };
            input::ProfilePreset input_profile{ input::ProfilePreset::EditorDefault };
        };

        [[nodiscard]] epochengine::previewgrid::CameraMode camera_mode_from_argument(std::string_view value) noexcept
        {
            if (value == "fps" || value == "first-person" || value == "first_person" || value == "runtime")
                return epochengine::previewgrid::CameraMode::FPS;
            if (value == "canvas2d" || value == "2d" || value == "2d-canvas" || value == "canvas")
                return epochengine::previewgrid::CameraMode::Canvas2D;
            return epochengine::previewgrid::CameraMode::Editor;
        }

        [[nodiscard]] ProjectSceneLaunchOptions parse_project_scene_launch(std::string_view payload)
        {
            ProjectSceneLaunchOptions launch{};
            std::string options{};

            if (const auto marker = payload.find('|'); marker != std::string_view::npos)
            {
                launch.project_id = std::string(payload.substr(0, marker));
                options = std::string(payload.substr(marker + 1));
            }
            else
            {
                launch.project_id = std::string(payload);
            }

            while (!options.empty())
            {
                const std::size_t nextOption = options.find('|');
                const std::string option = nextOption == std::string::npos
                    ? options
                    : options.substr(0, nextOption);

                constexpr std::string_view kCameraPrefix = "camera=";
                constexpr std::string_view kInputPrefix = "input=";
                if (option.starts_with(kCameraPrefix))
                    launch.camera_mode = camera_mode_from_argument(std::string_view{ option }.substr(kCameraPrefix.size()));
                else if (option.starts_with(kInputPrefix))
                    launch.input_profile = input::profile_preset_from_id(std::string_view{ option }.substr(kInputPrefix.size()));

                if (nextOption == std::string::npos)
                    break;
                options.erase(0, nextOption + 1);
            }

            return launch;
        }

        using ContextGroup = std::pair<
            epochengine::core::ContextType,
            std::vector<std::shared_ptr<epochengine::core::Context>>
        >;

        [[nodiscard]] bool launch_project_child_process(std::string_view launch_argument)
        {
            if (!launch_argument.starts_with("project-exe:"))
                return false;

            std::string payload{ launch_argument.substr(std::string_view{ "project-exe:" }.size()) };
            std::string executable_payload = payload;
            std::string scene_argument{};
            std::string backend_argument{};
            std::string frame_limit_argument{};
            std::string camera_argument{};
            std::string input_argument{};
            if (const std::size_t optionMarker = payload.find('|'); optionMarker != std::string::npos)
            {
                executable_payload = payload.substr(0, optionMarker);
                std::string options = payload.substr(optionMarker + 1);
                while (!options.empty())
                {
                    const std::size_t nextOption = options.find('|');
                    const std::string option = nextOption == std::string::npos
                        ? options
                        : options.substr(0, nextOption);

                    constexpr std::string_view kScenePrefix = "scene=";
                    constexpr std::string_view kBackendPrefix = "backend=";
                    constexpr std::string_view kFrameLimitPrefix = "fps=";
                    constexpr std::string_view kFrameLimitLongPrefix = "frame-limit=";
                    constexpr std::string_view kCameraPrefix = "camera=";
                    constexpr std::string_view kInputPrefix = "input=";
                    if (option.starts_with(kScenePrefix))
                        scene_argument = option.substr(kScenePrefix.size());
                    else if (option.starts_with(kBackendPrefix))
                        backend_argument = option.substr(kBackendPrefix.size());
                    else if (option.starts_with(kFrameLimitPrefix))
                        frame_limit_argument = option.substr(kFrameLimitPrefix.size());
                    else if (option.starts_with(kFrameLimitLongPrefix))
                        frame_limit_argument = option.substr(kFrameLimitLongPrefix.size());
                    else if (option.starts_with(kCameraPrefix))
                        camera_argument = option.substr(kCameraPrefix.size());
                    else if (option.starts_with(kInputPrefix))
                        input_argument = option.substr(kInputPrefix.size());

                    if (nextOption == std::string::npos)
                        break;
                    options.erase(0, nextOption + 1);
                }
            }

            if (backend_argument.empty()
                || backend_argument == "auto"
                || backend_argument == "Auto"
                || backend_argument == "AUTO")
                backend_argument = "opengl";

            std::string scene_to_launch = scene_argument;
            if (!camera_argument.empty()
                && scene_to_launch.starts_with("project:")
                && scene_to_launch.find("|camera=") == std::string::npos)
            {
                scene_to_launch += "|camera=";
                scene_to_launch += camera_argument;
            }
            if (!input_argument.empty()
                && scene_to_launch.starts_with("project:")
                && scene_to_launch.find("|input=") == std::string::npos)
            {
                scene_to_launch += "|input=";
                scene_to_launch += input_argument;
            }

            const std::filesystem::path executable = std::filesystem::path{
                executable_payload
            }.lexically_normal();
            std::error_code ec;
            if (!std::filesystem::exists(executable, ec) || ec)
            {
                logger::get(kEditorLog).logf(
                    logger::LogLevel::Error,
                    std::source_location::current(),
                    "Built project executable is missing: {}",
                    executable.generic_string());
                return false;
            }

            platform::child_process::LaunchRequest request{};
            request.executable = executable;
            request.working_directory = executable.parent_path();
            request.arguments = {
                "--standalone",
                "--window-mode",
                "standalone"};
            if (!scene_to_launch.empty())
            {
                request.arguments.emplace_back("--scene");
                request.arguments.emplace_back(scene_to_launch);
            }
            request.arguments.emplace_back("--backend");
            request.arguments.emplace_back(backend_argument);
            if (!frame_limit_argument.empty())
            {
                request.arguments.emplace_back("--frame-limit");
                request.arguments.emplace_back(frame_limit_argument);
            }
            request.correlation_key =
                "legacy-project:"
                + executable.generic_string()
                + "|scene="
                + scene_to_launch
                + "|backend="
                + backend_argument
                + "|fps="
                + frame_limit_argument;
            request.exclusive_group = "epoch.project.runtime";
            request.display_name = executable.stem().string();
            request.window_mode =
                platform::child_process::WindowMode::normal;

            const auto launched =
                platform::child_process::launch_or_focus(request);
            if (!launched)
            {
                logger::get(kEditorLog).logf(
                    logger::LogLevel::Error,
                    std::source_location::current(),
                    "Failed to launch built project executable: {} ({})",
                    executable.generic_string(),
                    launched.message);
                return false;
            }

            logger::get(kEditorLog).logf(
                logger::LogLevel::INFO,
                std::source_location::current(),
                "{} built project executable in standalone {} mode: {}",
                platform::child_process::launch_code_name(launched.code),
                backend_argument,
                executable.generic_string());
            return true;
        }

        using ProjectRuntimeEntity = epochengine::scene_runtime::RuntimeEntity;

        [[nodiscard]] epochengine::scene::SceneSnapshot project_runtime_snapshot_from_seeds(
            std::string_view project_id,
            std::string_view runtime_scene_id,
            std::string_view world_name,
            std::span<const epochengine::EditorSceneSeedEntity> seeds)
        {
            epochengine::scene::SceneSnapshot snapshot{};
            snapshot.project_id = project_id;
            snapshot.scene_id = runtime_scene_id;
            snapshot.world_name = world_name;
            snapshot.document_kind = "game";
            snapshot.support_tier = "tier0";
            snapshot.revision = 1u;
            snapshot.objects.reserve(seeds.size());
            for (const auto& seed : seeds)
            {
                snapshot.objects.push_back(epochengine::scene::SceneObjectSnapshot{
                    .id = epochengine::scene::stable_scene_object_id(runtime_scene_id, seed.name),
                    .name = std::string(seed.name),
                    .type = std::string(seed.type),
                    .category = std::string(seed.category),
                    .position = seed.position,
                    .rotation = seed.rotation,
                    .scale = seed.scale,
                    .visible = seed.visible,
                    .editor_only = seed.editor_only
                });
            }
            epochengine::scene::normalize_scene_document(snapshot);
            return snapshot;
        }

        [[nodiscard]] std::filesystem::path engine_runtime_root()
        {
            if (const auto runtimeRoot = epochengine::core::path::runtime_root_dir(); !runtimeRoot.empty())
                return runtimeRoot;

            std::error_code ec;
            return std::filesystem::current_path(ec);
        }

        [[nodiscard]] std::filesystem::path resolve_runtime_scene_path(const std::filesystem::path& path)
        {
            if (path.empty())
                return {};
            if (path.is_absolute())
                return path.lexically_normal();
            return (engine_runtime_root() / path).lexically_normal();
        }

        [[nodiscard]] epochengine::scene::SceneSnapshot load_project_runtime_snapshot(
            std::string_view scene_path,
            std::string_view project_id,
            std::string_view runtime_scene_id,
            std::string_view world_name,
            std::span<const epochengine::EditorSceneSeedEntity> fallback_seeds)
        {
            auto fallback = project_runtime_snapshot_from_seeds(
                project_id,
                runtime_scene_id,
                world_name,
                fallback_seeds);
            if (scene_path.empty())
                return fallback;

            std::ifstream in(resolve_runtime_scene_path(std::filesystem::path{ scene_path }), std::ios::binary);
            if (!in)
                return fallback;

            std::ostringstream text{};
            text << in.rdbuf();
            if (!in.good() && !in.eof())
                return fallback;

            const std::string payload = text.str();
            auto parsed = epochengine::scene::parse_snapshot_text(payload);
            if (!parsed.ok)
                parsed = epochengine::scene::parse_legacy_editor_scene_text(payload);
            if (!parsed.ok)
                return fallback;
            if (parsed.snapshot.project_id.empty())
                parsed.snapshot.project_id = project_id;
            if (parsed.snapshot.scene_id.empty())
                parsed.snapshot.scene_id = runtime_scene_id;
            if (parsed.snapshot.world_name.empty())
                parsed.snapshot.world_name = world_name;
            epochengine::scene::normalize_scene_document(parsed.snapshot);
            if (!epochengine::scene::validate_scene_document(
                    parsed.snapshot,
                    epochengine::scene::SceneDocumentRequirement::runnable))
                return fallback;

            return parsed.snapshot;
        }

        [[nodiscard]] epochengine::previewgrid::Vec3 runtime_marker_color_for_entity(
            const ProjectRuntimeEntity& entity,
            bool selected) noexcept
        {
            if (selected)
                return { 1.00f, 0.86f, 0.24f };
            if (entity.type == "ForestTrunk")
                return { 0.58f, 0.36f, 0.20f };
            if (entity.type == "ForestBranchSegment" || entity.type == "ForestBranchJoint")
                return { 0.42f, 0.70f, 0.32f };
            if (entity.type == "ForestFoliageCluster")
                return { 0.22f, 0.86f, 0.38f };
            if (entity.editor_only() || entity.category == "Editor")
                return { 0.44f, 0.62f, 0.90f };
            if (entity.type == "Light")
                return { 1.00f, 0.82f, 0.30f };
            if (entity.type == "Spawn")
                return { 0.34f, 0.94f, 0.62f };
            if (entity.category == "EngineArcade" && entity.type == "Canvas2D")
                return { 0.18f, 0.82f, 0.92f };
            if (entity.category == "EngineArcade")
                return { 0.36f, 0.56f, 0.92f };
            if (entity.category == "World" || entity.type == "Level")
                return { 0.70f, 0.78f, 0.90f };
            return { 0.95f, 0.62f, 0.28f };
        }

        [[nodiscard]] float runtime_marker_radius_for_entity(const ProjectRuntimeEntity& entity) noexcept
        {
            const float scaleMax = (std::max)(
                entity.transform.scale[0],
                (std::max)(entity.transform.scale[1], entity.transform.scale[2]));
            if (entity.type == "Light")
                return 0.42f;
            if (entity.type == "Spawn")
                return 0.32f;
            if (entity.type == "Camera")
                return 0.38f;
            if (entity.type == "ForestTrunk")
                return (std::clamp)(0.22f * scaleMax, 0.16f, 0.34f);
            if (entity.type == "ForestBranchSegment" || entity.type == "ForestBranchJoint")
                return (std::clamp)(0.34f * scaleMax, 0.16f, 0.24f);
            if (entity.type == "ForestFoliageCluster")
                return (std::clamp)(0.38f * scaleMax, 0.16f, 0.28f);
            if (entity.category == "World" || entity.type == "Level")
                return 0.75f;
            return (std::clamp)(0.34f * scaleMax, 0.24f, 1.20f);
        }

        [[nodiscard]] epochengine::previewgrid::ObjectPreviewPrimitive runtime_preview_primitive_for_entity(
            const ProjectRuntimeEntity& entity) noexcept
        {
            if (entity.type == "Light")
                return epochengine::previewgrid::ObjectPreviewPrimitive::Light;
            if (entity.type == "Spawn")
                return epochengine::previewgrid::ObjectPreviewPrimitive::Spawn;
            if (entity.type == "Camera")
                return epochengine::previewgrid::ObjectPreviewPrimitive::Camera;
            if (entity.category == "EngineArcade" && entity.name == "EngineArcadeScreen")
                return epochengine::previewgrid::ObjectPreviewPrimitive::EngineArcadeScreen;
            if (entity.type == "Canvas2D")
                return epochengine::previewgrid::ObjectPreviewPrimitive::Canvas2D;
            if (entity.type == "ForestTrunk")
                return epochengine::previewgrid::ObjectPreviewPrimitive::ForestTrunk;
            if (entity.type == "ForestBranchSegment" || entity.type == "ForestBranchJoint")
                return epochengine::previewgrid::ObjectPreviewPrimitive::ForestBranch;
            if (entity.type == "ForestFoliageCluster")
                return epochengine::previewgrid::ObjectPreviewPrimitive::ForestLeafCluster;
            if (entity.category == "World" || entity.type == "Level")
                return epochengine::previewgrid::ObjectPreviewPrimitive::Level;
            return epochengine::previewgrid::ObjectPreviewPrimitive::Cube;
        }

        [[nodiscard]] std::size_t visible_runtime_entity_count(std::span<const ProjectRuntimeEntity> entities) noexcept
        {
            std::size_t count = 0;
            for (const auto& entity : entities)
                if (entity.visible())
                    ++count;
            return count;
        }

        [[nodiscard]] epochengine::lighting::LightingFrame build_project_play_lighting(
            std::span<const ProjectRuntimeEntity> entities)
        {
            epochengine::lighting::LightManager manager{ 128 };
            manager.set_environment({ { 0.16f, 0.18f, 0.22f } });
            for (const ProjectRuntimeEntity& entity : entities)
            {
                if (!entity.visible() || entity.type != "Light")
                    continue;

                const bool pointLight = entity.name.contains("Point") || entity.name.contains("Lamp");
                epochengine::lighting::LightDesc desc{};
                desc.kind = pointLight
                    ? epochengine::lighting::LightKind::Point
                    : epochengine::lighting::LightKind::Directional;
                desc.color = pointLight
                    ? epochengine::lighting::Color3{ 1.0f, 0.78f, 0.56f }
                    : epochengine::lighting::Color3{ 1.0f, 0.95f, 0.84f };
                desc.intensity = pointLight ? 24.0f : 1.35f;
                desc.position = { entity.transform.position[0], entity.transform.position[1], entity.transform.position[2] };
                desc.direction = epochengine::lighting::direction_from_euler_degrees({
                    entity.transform.rotation[0],
                    entity.transform.rotation[1],
                    entity.transform.rotation[2]
                });
                desc.range = pointLight
                    ? (std::max)(12.0f, (std::max)({ entity.transform.scale[0], entity.transform.scale[1], entity.transform.scale[2] }) * 12.0f)
                    : 1.0f;
                (void)manager.create(desc);
            }
            return manager.build_frame();
        }

        [[nodiscard]] std::optional<epochengine::previewgrid::Vec3> project_spawn_position(
            std::span<const ProjectRuntimeEntity> entities) noexcept
        {
            for (const ProjectRuntimeEntity& entity : entities)
            {
                if (entity.visible() && !entity.editor_only() && entity.type == "Spawn")
                {
                    return epochengine::previewgrid::Vec3{
                        entity.transform.position[0],
                        entity.transform.position[1] + 1.65f,
                        entity.transform.position[2]
                    };
                }
            }
            return std::nullopt;
        }
        void publish_project_play_markers(
            const epochengine::core::Context* ctx,
            std::span<const ProjectRuntimeEntity> entities)
        {
            if (!ctx)
                return;

            std::vector<epochengine::previewgrid::ObjectMarker> markers{};
            markers.reserve(entities.size());
            for (const auto& entity : entities)
            {
                if (!entity.visible() || entity.editor_only()
                    || entity.category == "Editor"
                    || entity.type == "Camera"
                    || entity.type == "Light"
                    || entity.type == "Spawn")
                    continue;

                markers.push_back(epochengine::previewgrid::ObjectMarker{
                    .position{ entity.transform.position[0], entity.transform.position[1], entity.transform.position[2] },
                    .color = runtime_marker_color_for_entity(entity, false),
                    .scale{ entity.transform.scale[0], entity.transform.scale[1], entity.transform.scale[2] },
                    .rotationDegrees{ entity.transform.rotation[0],
                        entity.transform.rotation[1],
                        entity.transform.rotation[2]
                    },
                    .radius = runtime_marker_radius_for_entity(entity),
                    .primitive = runtime_preview_primitive_for_entity(entity),
                    .selected = false,
                    .editorOnly = false,
                    .sampledRenderSurface = entity.category == "EngineArcade" && entity.name == "EngineArcadeScreen"
                });
            }

            epochengine::previewgrid::set_object_markers(ctx, std::span<const epochengine::previewgrid::ObjectMarker>{
                markers.data(),
                markers.size()
            });
        }

        [[nodiscard]] std::filesystem::path project_runtime_root(
            const epochengine::EditorProjectProfile& profile)
        {
            const auto requestedId =
                epochengine::core::env::get("EPOCH_EDITOR_PROJECT_ID");
            const auto requestedRoot =
                epochengine::core::env::get("EPOCH_EDITOR_PROJECT_ROOT");
            if (requestedId && requestedRoot
                && requestedId->impl == profile.id && !requestedRoot->empty())
            {
                return std::filesystem::path{requestedRoot->impl}.lexically_normal();
            }
            return resolve_runtime_scene_path(
                std::filesystem::path{profile.root_path});
        }

        [[nodiscard]] std::filesystem::path project_profile_path(
            const epochengine::EditorProjectProfile& profile,
            const std::filesystem::path& runtimeRoot,
            std::string_view declaredPath)
        {
            if (declaredPath.empty())
                return {};
            std::filesystem::path path{declaredPath};
            if (path.is_absolute())
                return path.lexically_normal();

            const std::filesystem::path declaredRoot{profile.root_path};
            const std::filesystem::path relative =
                path.lexically_relative(declaredRoot);
            if (!relative.empty()
                && relative.native().find(
                    std::filesystem::path{".."}.native()) != 0u)
            {
                return (runtimeRoot / relative).lexically_normal();
            }
            return (runtimeRoot / path).lexically_normal();
        }


        epochengine::audio::PlaybackRuntime* g_processAudioRuntime{};

        class ScopedProcessAudioRuntime final
        {
        public:
            explicit ScopedProcessAudioRuntime(
                epochengine::audio::PlaybackRuntime& runtime) noexcept
                : previous_{std::exchange(g_processAudioRuntime, &runtime)}
            {
            }

            ~ScopedProcessAudioRuntime()
            {
                g_processAudioRuntime = previous_;
            }

            ScopedProcessAudioRuntime(
                const ScopedProcessAudioRuntime&) = delete;
            ScopedProcessAudioRuntime& operator=(
                const ScopedProcessAudioRuntime&) = delete;

        private:
            epochengine::audio::PlaybackRuntime* previous_{};
        };

        class ScopedPhysicalInputRuntime final
        {
        public:
            ScopedPhysicalInputRuntime() noexcept
                : initialization_{
                    epochengine::controller_input::initialize_physical_input()}
            {
            }

            ~ScopedPhysicalInputRuntime()
            {
                epochengine::controller_input::shutdown_physical_input();
            }

            ScopedPhysicalInputRuntime(
                const ScopedPhysicalInputRuntime&) = delete;
            ScopedPhysicalInputRuntime& operator=(
                const ScopedPhysicalInputRuntime&) = delete;

            [[nodiscard]] epochengine::controller_input::ControllerCode
            initialization() const noexcept
            {
                return initialization_;
            }

        private:
            epochengine::controller_input::ControllerCode initialization_{
                epochengine::controller_input::ControllerCode::unavailable};
        };

        [[nodiscard]] std::optional<input::Key> engine_key(
            epochengine::project_input::KeyCode key) noexcept
        {
            using ProjectKey = epochengine::project_input::KeyCode;
            switch (key)
            {
            case ProjectKey::a: return input::Key::A;
            case ProjectKey::b: return input::Key::B;
            case ProjectKey::c: return input::Key::C;
            case ProjectKey::d: return input::Key::D;
            case ProjectKey::e: return input::Key::E;
            case ProjectKey::f: return input::Key::F;
            case ProjectKey::g: return input::Key::G;
            case ProjectKey::h: return input::Key::H;
            case ProjectKey::i: return input::Key::I;
            case ProjectKey::j: return input::Key::J;
            case ProjectKey::k: return input::Key::K;
            case ProjectKey::l: return input::Key::L;
            case ProjectKey::m: return input::Key::M;
            case ProjectKey::n: return input::Key::N;
            case ProjectKey::o: return input::Key::O;
            case ProjectKey::p: return input::Key::P;
            case ProjectKey::q: return input::Key::Q;
            case ProjectKey::r: return input::Key::R;
            case ProjectKey::s: return input::Key::S;
            case ProjectKey::t: return input::Key::T;
            case ProjectKey::u: return input::Key::U;
            case ProjectKey::v: return input::Key::V;
            case ProjectKey::w: return input::Key::W;
            case ProjectKey::x: return input::Key::X;
            case ProjectKey::y: return input::Key::Y;
            case ProjectKey::z: return input::Key::Z;
            case ProjectKey::enter: return input::Key::Enter;
            case ProjectKey::escape: return input::Key::Escape;
            case ProjectKey::backspace: return input::Key::Backspace;
            case ProjectKey::tab: return input::Key::Tab;
            case ProjectKey::space: return input::Key::Space;
            case ProjectKey::right: return input::Key::Right;
            case ProjectKey::left: return input::Key::Left;
            case ProjectKey::down: return input::Key::Down;
            case ProjectKey::up: return input::Key::Up;
            case ProjectKey::left_control: return input::Key::LeftControl;
            case ProjectKey::left_shift: return input::Key::LeftShift;
            case ProjectKey::left_alt: return input::Key::LeftAlt;
            case ProjectKey::left_super: return input::Key::LeftSuper;
            case ProjectKey::right_control: return input::Key::RightControl;
            case ProjectKey::right_shift: return input::Key::RightShift;
            case ProjectKey::right_alt: return input::Key::RightAlt;
            case ProjectKey::right_super: return input::Key::RightSuper;
            case ProjectKey::invalid:
                break;
            }
            return std::nullopt;
        }

        [[nodiscard]] epochengine::project_input::ModifierMask
        project_modifier_snapshot() noexcept
        {
            namespace project_input = epochengine::project_input;
            project_input::ModifierMask result{};
            if (input::is_key_held(input::Key::LeftShift)
                || input::is_key_held(input::Key::RightShift))
            {
                result |= project_input::modifier_mask(
                    project_input::Modifier::shift);
            }
            if (input::is_key_held(input::Key::LeftControl)
                || input::is_key_held(input::Key::RightControl))
            {
                result |= project_input::modifier_mask(
                    project_input::Modifier::control);
            }
            if (input::is_key_held(input::Key::LeftAlt)
                || input::is_key_held(input::Key::RightAlt))
            {
                result |= project_input::modifier_mask(
                    project_input::Modifier::alt);
            }
            if (input::is_key_held(input::Key::LeftSuper)
                || input::is_key_held(input::Key::RightSuper))
            {
                result |= project_input::modifier_mask(
                    project_input::Modifier::super);
            }
            return result;
        }

        [[nodiscard]] epochengine::project_input::InputSnapshot
        project_input_snapshot(
            const epochengine::project_input::CompiledInputProfile& profile,
            std::uint64_t frameIndex,
            bool samplePhysicalInput)
        {
            namespace project_input = epochengine::project_input;
            project_input::InputSnapshot snapshot{};
            snapshot.frame_index = frameIndex;
            if (!samplePhysicalInput)
                return snapshot;

            std::vector<project_input::KeyCode> keys{};
            for (const auto& binding : profile.bindings)
            {
                if (binding.device != project_input::BindingDevice::keyboard)
                    continue;
                const auto key = static_cast<project_input::KeyCode>(binding.code);
                if (engine_key(key))
                    keys.push_back(key);
            }
            std::sort(keys.begin(), keys.end());
            keys.erase(std::unique(keys.begin(), keys.end()), keys.end());

            snapshot.modifiers = project_modifier_snapshot();
            snapshot.keyboard.reserve(keys.size());
            for (const auto projectKey : keys)
            {
                const auto key = engine_key(projectKey);
                if (!key)
                    continue;
                snapshot.keyboard.push_back({
                    .key = projectKey,
                    .held = input::is_key_held(*key),
                    .pressed = input::is_key_down(*key)
                });
            }
            return snapshot;
        }

        class ProjectPlayScene final : public epochengine::scene::Scene
        {
        public:
            explicit ProjectPlayScene(std::string_view project_payload)
            {
                const auto launch = parse_project_scene_launch(project_payload);
                m_cameraMode = launch.camera_mode;
                m_inputProfile = launch.input_profile;
                input::set_active_profile(m_inputProfile);

                const auto* profile = epochengine::editor_find_project_profile(launch.project_id);
                if (!profile)
                    profile = &epochengine::editor_default_project_profile();

                m_projectId = std::string(profile->id);
                m_projectName = std::string(profile->display_name);
                const std::filesystem::path runtimeRoot =
                    project_runtime_root(*profile);
                m_projectRoot = runtimeRoot.generic_string();
                m_scenePath = project_profile_path(
                    *profile, runtimeRoot, profile->scene_path).generic_string();
                m_tileMapPath = std::string(profile->tilemap_path);
                m_inputProfilePath = std::string(profile->input_profile_path);
                m_spriteAnimationPath =
                    std::string(profile->sprite_animation_path);
                m_audioProfilePath =
                    std::string(profile->audio_profile_path);
                m_worldName = std::string(profile->world_name);
                m_scriptName = std::string(profile->default_script);
                m_description = std::string(profile->description);
                m_runtimeBudgets =
                    epochengine::platform::recommended_budgets_for_tier(
                        epochengine::perf::tier::mobile_30);
                m_runtimeBudgetProfile = "T1-GLES/mobile_30";
                m_modelSummary = epochengine::editor_project_model_summary(m_projectId);
                prepare_project_gui();
                const auto seedEntities = epochengine::editor_seed_entities_for_project(m_projectId);
                const auto sourceSnapshot = load_project_runtime_snapshot(
                    m_scenePath,
                    profile->id,
                    profile->runtime_scene_id,
                    profile->world_name,
                    std::span<const epochengine::EditorSceneSeedEntity>{ seedEntities.data(), seedEntities.size() });
                const auto compileReport = m_sceneRuntime.replace(
                    sourceSnapshot,
                    epochengine::scene_runtime::RuntimeScenePolicy{
                        .include_hidden_objects = false,
                        .include_editor_only_objects = false
                    });
                if (compileReport.committed())
                {
                    const auto compiled = m_sceneRuntime.projection()->entities();
                    m_entities.assign(compiled.begin(), compiled.end());
                }
                m_lightingFrame = build_project_play_lighting(
                    std::span<const ProjectRuntimeEntity>{ m_entities.data(), m_entities.size() });
            }

            void load() override
            {
                Scene::load();
                prepare_gameplay();
            }

            void unload() override
            {
                shutdown_gameplay();
                retire_canvas2d_scenes();
                m_cameraApplied.clear();
                m_hasLastFrame = false;
                Scene::unload();
            }

            ~ProjectPlayScene() override
            {
                shutdown_gameplay();
                retire_canvas2d_scenes();
            }

            bool frame(std::shared_ptr<epochengine::core::Context> ctx, epochengine::core::WindowData*) override
            {
                if (!ctx)
                    return false;

                if (input::action_pressed(input::Action::Cancel))
                {
                    epochengine::previewgrid::clear_object_markers(ctx.get());
                    epochengine::previewgrid::clear_lighting_frame(ctx.get());
                    (void)epochengine::canvas2d::scene_content::retire(ctx.get());
                    m_canvas2dPublished.erase(ctx.get());
                    return false;
                }

                const auto now = timing::Clock::now();
                float dt = 0.0f;
                if (m_hasLastFrame)
                    dt = std::chrono::duration<float>(now - m_lastFrame).count();
                m_lastFrame = now;
                m_hasLastFrame = true;

                int mx = 0;
                int my = 0;
                ctx->get_mouse_position_safe(mx, my);
                const gui::Vec2 mouse_pos{
                    static_cast<float>(mx),
                    static_cast<float>(my)
                };

                const bool mouse_left_down =
                    ctx->is_mouse_button_held_safe(epochengine::input::MouseButton::MouseLeft);
                const bool mouse_right_down =
                    ctx->is_mouse_button_held_safe(epochengine::input::MouseButton::MouseRight);
                const bool mouse_middle_down =
                    ctx->is_mouse_button_held_safe(epochengine::input::MouseButton::MouseMiddle);

                const int width = (std::max)(1, ctx->width > 0 ? ctx->width : ctx->get_width_safe());
                const int height = (std::max)(1, ctx->height > 0 ? ctx->height : ctx->get_height_safe());
                bool projectGuiCapturesPointer{};
                bool projectGuiCapturesKeyboard{};
                if (m_projectGuiRuntime)
                {
                    (void)m_projectGuiRuntime->build_frame({
                        static_cast<float>(width),
                        static_cast<float>(height)});
                    projectGuiCapturesPointer =
                        m_projectGuiRuntime->pointer_captured({
                            mouse_pos.x, mouse_pos.y});
                    projectGuiCapturesKeyboard =
                        m_projectGuiRuntime->keyboard_captured();
                }
                advance_gameplay(dt, projectGuiCapturesKeyboard);

                const bool backendOwnsFrameClear =
                    ctx->type == core::ContextType::OpenGL;
                if (!backendOwnsFrameClear)
                    ctx->clear_safe();
                ctx->set_scene_preview_mode(core::ScenePreviewMode::Editor);
                if (!m_cameraApplied.contains(ctx.get()))
                {
                    epochengine::previewgrid::set_camera_mode(ctx.get(), m_cameraMode);
                    if (m_cameraMode == epochengine::previewgrid::CameraMode::FPS)
                    {
                        if (const auto spawn = project_spawn_position(
                            std::span<const ProjectRuntimeEntity>{ m_entities.data(), m_entities.size() }))
                        {
                            (void)epochengine::previewgrid::focus_camera(ctx.get(), *spawn, 0.0f);
                        }
                    }
                    m_cameraApplied[ctx.get()] = true;
                }
                ctx->set_scene_viewport({ 0, 0, width, height });
                if (m_cameraMode == epochengine::previewgrid::CameraMode::Canvas2D)
                {
                    epochengine::previewgrid::clear_object_markers(ctx.get());
                    epochengine::previewgrid::clear_lighting_frame(ctx.get());
                    const auto published = m_canvas2dPublished.find(ctx.get());
                    const auto acquired =
                        epochengine::canvas2d::scene_content::acquire(ctx.get());
                    const auto* desiredScene = m_gameplayRuntime
                        ? m_gameplayRuntime->scene()
                        : (m_staticCanvasScene
                            ? &*m_staticCanvasScene : nullptr);
                    const bool needsPublication = desiredScene
                        && (published == m_canvas2dPublished.end()
                            || !acquired
                            || acquired.generation != published->second
                            || !acquired.content
                            || acquired.content->source_revision
                                != desiredScene->source_revision);
                    if (needsPublication)
                    {
                        const auto publication =
                            epochengine::canvas2d::scene_content::publish(
                                ctx.get(), *desiredScene);
                        if (publication)
                            m_canvas2dPublished[ctx.get()] = publication.generation;
                        else
                        {
                            m_tileMapStatus =
                                std::string("scene publication ")
                                + std::string(
                                    epochengine::canvas2d::scene_content::scene_code_name(
                                        publication.code));
                        }
                    }
                    else if (!desiredScene)
                    {
                        (void)epochengine::canvas2d::scene_content::retire(ctx.get());
                    }
                }
                else
                {
                    (void)epochengine::canvas2d::scene_content::retire(ctx.get());
                    m_canvas2dPublished.erase(ctx.get());
                    publish_project_play_markers(ctx.get(), std::span<const ProjectRuntimeEntity>{
                        m_entities.data(),
                        m_entities.size()
                    });
                    epochengine::previewgrid::set_lighting_frame(
                        ctx.get(), m_lightingFrame);
                }

                gui::begin_frame(ctx, dt, mouse_pos, mouse_left_down);

                const int wheelDelta = epochengine::gui::consume_mouse_wheel_delta();
                if (input::action_pressed(input::Action::ResetCamera))
                    epochengine::previewgrid::reset_camera(ctx.get());

                const bool altHeld = input::is_key_held(input::Key::LeftAlt)
                    || input::is_key_held(input::Key::RightAlt);
                const bool shiftHeld = input::is_key_held(input::Key::LeftShift)
                    || input::is_key_held(input::Key::RightShift);
                const bool controlHeld = input::is_key_held(input::Key::LeftControl)
                    || input::is_key_held(input::Key::RightControl);
                const auto navigationGestures =
                    epochengine::previewgrid::resolve_camera_navigation_gestures(
                        altHeld, mouse_left_down, mouse_middle_down, mouse_right_down);
                const bool orbiting = !projectGuiCapturesPointer && navigationGestures.orbiting;
                const bool panning = !projectGuiCapturesPointer && navigationGestures.panning;
                const bool dollying = !projectGuiCapturesPointer && navigationGestures.dollying;
                const bool flying = !projectGuiCapturesPointer && navigationGestures.flying;
                const bool keyboardNavigation = !projectGuiCapturesKeyboard && flying;
                const float navigationMultiplier = shiftHeld ? 4.0f : (controlHeld ? 0.25f : 1.0f);

                const float forwardInput = !keyboardNavigation ? 0.0f :
                    (input::action_held(input::Action::MoveForward) ? 1.0f : 0.0f)
                    - (input::action_held(input::Action::MoveBackward) ? 1.0f : 0.0f);
                const float rightInput = !keyboardNavigation ? 0.0f :
                    (input::action_held(input::Action::MoveRight) ? 1.0f : 0.0f)
                    - (input::action_held(input::Action::MoveLeft) ? 1.0f : 0.0f);
                const float upInput = !keyboardNavigation ? 0.0f :
                    (input::action_held(input::Action::MoveUp) ? 1.0f : 0.0f)
                    - (input::action_held(input::Action::MoveDown) ? 1.0f : 0.0f);
                const float yawInput = !keyboardNavigation ? 0.0f :
                    (input::action_held(input::Action::LookRight) ? 1.0f : 0.0f)
                    - (input::action_held(input::Action::LookLeft) ? 1.0f : 0.0f);
                const float pitchInput = !keyboardNavigation ? 0.0f :
                    (input::action_held(input::Action::LookUp) ? 1.0f : 0.0f)
                    - (input::action_held(input::Action::LookDown) ? 1.0f : 0.0f);


                if ((flying && m_lookState.flying) || (orbiting && m_lookState.orbiting))
                {
                    const float mouseDeltaX = mouse_pos.x - m_lookState.last_mouse.x;
                    const float mouseDeltaY = mouse_pos.y - m_lookState.last_mouse.y;
                    const float sensitivity = input::mouse_look_sensitivity();
                    epochengine::previewgrid::look_camera(
                        ctx.get(),
                        mouseDeltaX * sensitivity,
                        -mouseDeltaY * sensitivity);
                }

                else if (panning && m_lookState.panning)
                {
                    const float mouseDeltaX = mouse_pos.x - m_lookState.last_mouse.x;
                    const float mouseDeltaY = mouse_pos.y - m_lookState.last_mouse.y;
                    epochengine::previewgrid::pan_camera_drag(
                        ctx.get(),
                        mouseDeltaX,
                        -mouseDeltaY);
                }

                else if (dollying && m_lookState.dollying)
                {
                    const float mouseDeltaY = mouse_pos.y - m_lookState.last_mouse.y;
                    epochengine::previewgrid::dolly_camera_drag(ctx.get(), mouseDeltaY);
                }


                if (wheelDelta != 0 && !projectGuiCapturesPointer)
                {
                    const float wheelSteps = static_cast<float>(wheelDelta) / 120.0f;
                    if (flying)
                        epochengine::previewgrid::adjust_fly_speed(ctx.get(), wheelSteps);
                    else
                        epochengine::previewgrid::zoom_camera(
                            ctx.get(), wheelSteps * input::wheel_zoom_step());
                }

                epochengine::previewgrid::step_camera(
                    ctx.get(),
                    dt,
                    forwardInput,
                    rightInput,
                    upInput,
                    yawInput,
                    pitchInput,
                    navigationMultiplier);

                m_lookState.last_mouse = mouse_pos;
                m_lookState.orbiting = orbiting;
                m_lookState.panning = panning;
                m_lookState.dollying = dollying;
                m_lookState.flying = flying;

                bool returnToEditor = false;
                if (m_projectGuiRuntime && m_projectGuiAdapter)
                {
                    auto guiResult = m_projectGuiAdapter->render(
                        *m_projectGuiRuntime,
                        {
                            static_cast<float>(width),
                            static_cast<float>(height)});
                    if (guiResult)
                    {
                        for (const auto& event : guiResult.events)
                        {
                            if (event.action.empty())
                                continue;
                            if (event.action == "editor.return")
                            {
                                if (event.kind
                                    == epochengine::project_gui_runtime::EventKind::activated)
                                {
                                    returnToEditor = true;
                                    m_projectGuiStatus =
                                        "GUI host command: return to editor.";
                                }
                                else
                                {
                                    m_projectGuiStatus =
                                        "GUI host command rejected for this event.";
                                }
                                continue;
                            }
                            queue_project_gui_action(event);
                        }
                    }
                    else
                    {
                        m_projectGuiStatus = std::string{"GUI runtime: "}
                            + std::string{
                                epochengine::project_gui_runtime::runtime_code_name(
                                    guiResult.code)};
                    }
                }
                gui::begin_top_layer();
                gui::begin_window(
                    "Project Runtime Preview",
                    {24.0f, 24.0f},
                    {430.0f, m_gameplayRuntime
                            && m_gameplayRuntime->active()
                        ? 724.0f
                        : (m_tileMapPath.empty() ? 210.0f : 306.0f)});
                gui::label(std::string("Project: ") + m_projectName);
                gui::label(std::string("World: ") + m_worldName);
                gui::label(std::string("Scene: ") + m_scenePath);
                gui::label(std::string("Script: ") + m_scriptName);
                gui::wrapped_label(m_projectGuiStatus, 390.0f);
                gui::label(std::string("Preview Objects: ") + std::to_string(visible_runtime_entity_count(m_entities)));
                if (!m_tileMapPath.empty())
                {
                    gui::label(std::string("Tile Map: ") + m_tileMapPath);
                    gui::wrapped_label(
                        std::string("Tile Runtime: ")
                            + (m_tileMapStatus.empty()
                                ? std::string("not prepared")
                                : m_tileMapStatus),
                        390.0f);
                    if (!m_inputProfilePath.empty())
                    {
                        gui::label(std::string("Input Profile: ")
                            + m_inputProfilePath);
                        gui::wrapped_label(
                            std::string("Input Runtime: ")
                                + (m_projectInputStatus.empty()
                                    ? std::string("not prepared")
                                    : m_projectInputStatus),
                            390.0f);
                    }
                    if (!m_spriteAnimationPath.empty())
                    {
                        gui::label(std::string("Sprite Animations: ")
                            + m_spriteAnimationPath);
                        gui::wrapped_label(
                            std::string("Animation Runtime: ")
                                + (m_spriteAnimationStatus.empty()
                                    ? std::string("not prepared")
                                    : m_spriteAnimationStatus),
                            390.0f);
                    }
                    gui::wrapped_label(
                        std::string{"Gameplay: "}
                            + (m_actorStatus.empty()
                                ? std::string{"not prepared"}
                                : m_actorStatus),
                        390.0f);
                    if (m_gameplayRuntime && m_gameplayRuntime->active())
                    {
                        const auto snapshot = m_gameplayRuntime->snapshot();
                        const auto& actor = snapshot.actor;
                        const auto metrics = snapshot.metrics;
                        gui::label(
                            std::string("Position: ")
                            + std::to_string(actor.x) + ", "
                            + std::to_string(actor.y));
                        gui::label(
                            std::string("Physics Tick: ")
                            + std::to_string(actor.fixed_tick));
                        gui::label(
                            std::string("Fixed Steps: ")
                            + std::to_string(metrics.fixed_steps));
                        gui::label(
                            std::string("Actor Events: ")
                            + std::to_string(metrics.actor_events));
                        gui::label(
                            std::string("Audio Triggers: ")
                            + std::to_string(metrics.audio_triggers));
                        if (!m_runtimeCosts
                            || m_costFramesUntilRefresh == 0u)
                        {
                            m_runtimeCosts =
                                m_gameplayRuntime->cost_snapshot();
                            m_runtimeBudgetAssessment =
                                epochengine::project_gameplay2d::
                                    assess_runtime_costs(
                                        *m_runtimeCosts, m_runtimeBudgets);
                            m_costFramesUntilRefresh = 15u;
                        }
                        else
                            --m_costFramesUntilRefresh;
                        if (m_runtimeCosts)
                        {
                            const auto& costs = *m_runtimeCosts;
                            gui::label(
                                std::string{"Canvas Cost: "}
                                + std::to_string(costs.emitted_sprites)
                                + " sprites / "
                                + std::to_string(costs.emitted_batches)
                                + " batches / "
                                + std::to_string(costs.emitted_vertices)
                                + " vertices");
                            gui::label(
                                std::string{"Texture Cost: "}
                                + std::to_string(
                                    costs.source_texture_views)
                                + " views / "
                                + std::to_string(
                                    costs.source_texture_bytes / 1024u)
                                + " KiB logical");
                            gui::label(
                                std::string{"Physics Cost: "}
                                + std::to_string(
                                    costs.collision_surfaces)
                                + " surfaces / "
                                + std::to_string(
                                    costs.peak_contacts_per_step)
                                + " peak contacts");
                            gui::label(
                                std::string{"Audio Cost: "}
                                + std::to_string(
                                    costs.audio_resident_bytes / 1024u)
                                + " KiB / "
                                + std::to_string(
                                    costs.audio_frames_mixed)
                                + " mixed frames");
                            gui::wrapped_label(
                                std::string{"Cost Status: "}
                                    + costs.diagnostic,
                                390.0f);
                            if (m_runtimeBudgetAssessment)
                            {
                                gui::wrapped_label(
                                    std::string{"Tier Budget ("}
                                        + m_runtimeBudgetProfile + "): "
                                        + m_runtimeBudgetAssessment->diagnostic,
                                    390.0f);
                            }
                        }
                        gui::wrapped_label(
                            std::string("Audio: ")
                                + (m_audioStatus.empty()
                                    ? std::string("not active")
                                    : m_audioStatus),
                            390.0f);
                        if (gui::button(
                            actor.paused ? "Resume" : "Pause",
                            {190.0f, 30.0f}))
                        {
                            const auto code = m_gameplayRuntime->set_paused(
                                !actor.paused);
                            m_actorStatus = std::string{
                                epochengine::project_gameplay2d::
                                    session_code_name(code)}
                                + ": " + std::string{
                                    m_gameplayRuntime->diagnostic()};
                        }
                        if (gui::button("Reset Actor", {190.0f, 30.0f}))
                        {
                            const auto code = m_gameplayRuntime->reset();
                            m_actorStatus = std::string{
                                epochengine::project_gameplay2d::
                                    session_code_name(code)}
                                + ": " + std::string{
                                    m_gameplayRuntime->diagnostic()};
                        }
                    }
                    if (gui::button(
                            "Return to Editor", {190.0f, 30.0f}))
                    {
                        returnToEditor = true;
                    }
                }
                gui::wrapped_label(
                    std::string("Demo model: ")
                    + (m_modelSummary.asset_path.empty() ? std::string("(none)") : m_modelSummary.asset_path),
                    390.0f);
                gui::wrapped_label(
                    std::string("Model summary: ")
                    + (m_modelSummary.summary.empty() ? std::string("(unavailable)") : m_modelSummary.summary),
                    390.0f);
                gui::wrapped_label(m_description, 390.0f);
                gui::end_window();
                gui::end_top_layer();

                gui::end_frame();
                ctx->present_safe();
                return !returnToEditor;
            }

        private:
            void prepare_gameplay()
            {
                shutdown_gameplay();
                m_staticCanvasScene.reset();
                m_projectControllerSampler.reset();
                m_inputFrameIndex = 0u;
                m_runtimeCosts.reset();
                m_runtimeBudgetAssessment.reset();
                m_costFramesUntilRefresh = 0u;
                for (auto& pending : m_pendingGuiActions)
                    pending.reset();

                if (m_cameraMode
                        != epochengine::previewgrid::CameraMode::Canvas2D
                    || m_tileMapPath.empty() || m_projectRoot.empty())
                {
                    return;
                }

                auto prepared =
                    epochengine::project_gameplay2d::prepare_project({
                        .project_id = m_projectId,
                        .project_root =
                            std::filesystem::path{m_projectRoot},
                        .tilemap = {
                            .logical_path = m_tileMapPath,
                            .source_policy = epochengine::
                                project_tilemap_runtime::SourcePolicy::
                                    prefer_source},
                        .input_profile_path = m_inputProfilePath,
                        .sprite_animation_path = m_spriteAnimationPath,
                        .audio_profile_path = m_audioProfilePath,
                        .input_fallback = epochengine::project_gameplay2d::
                            InputFallbackPolicy::allow_legacy_default,
                        .request_physical_audio =
                            EPOCH_ENABLE_PHYSICAL_AUDIO != 0,
                        .allow_compatibility_audio = true,
                        .require_sprite_animation = false,
                        .require_audio = false});

                if (!prepared)
                {
                    m_actorStatus = std::string{"gameplay preparation "}
                        + std::string{epochengine::project_gameplay2d::
                            preparation_code_name(prepared.code)}
                        + ": " + prepared.diagnostic;
                    m_tileMapStatus = m_actorStatus;

                    epochengine::project_tilemap_runtime::
                        ProjectTileMapRuntime fallback{
                            m_projectId, m_projectRoot};
                    auto staticMap = fallback.prepare({
                        .logical_path = m_tileMapPath,
                        .source_policy = epochengine::
                            project_tilemap_runtime::SourcePolicy::
                                prefer_source});
                    if (staticMap)
                    {
                        m_staticCanvasScene = std::move(staticMap.scene);
                        m_tileMapStatus = std::move(staticMap.diagnostic);
                    }
                    return;
                }

                const auto& evidence = prepared.project.evidence;
                m_tileMapStatus = evidence.tilemap_diagnostic;
                m_projectInputStatus = evidence.input_diagnostic;
                m_spriteAnimationStatus =
                    m_spriteAnimationPath.empty()
                    ? "Project does not declare sprite animations."
                    : evidence.animation_diagnostic;
                m_audioStatus = evidence.audio_diagnostic.empty()
                    ? "Project audio is not active."
                    : evidence.audio_diagnostic;
                m_staticCanvasScene = prepared.project.base_scene;

                if (!g_processAudioRuntime && prepared.project.audio_program)
                {
                    prepared.project.audio_program.reset();
                    m_audioStatus =
                        "Process audio runtime unavailable; gameplay audio disabled.";
                }

                auto gameplay = std::make_unique<
                    epochengine::project_gameplay2d::Gameplay2DRuntime>();
                const auto opened = gameplay->open(
                    std::move(prepared.project), g_processAudioRuntime);
                m_actorStatus =
                    std::string{epochengine::project_gameplay2d::
                        session_code_name(opened)}
                    + ": " + std::string{gameplay->diagnostic()};
                if (opened
                        == epochengine::project_gameplay2d::SessionCode::ready
                    || opened
                        == epochengine::project_gameplay2d::SessionCode::degraded)
                {
                    m_gameplayRuntime = std::move(gameplay);
                    m_staticCanvasScene.reset();
                }
            }

            void shutdown_gameplay() noexcept
            {
                if (m_gameplayRuntime)
                    (void)m_gameplayRuntime->close();
                m_gameplayRuntime.reset();
                m_staticCanvasScene.reset();
                m_projectControllerSampler.reset();
                m_inputFrameIndex = 0u;
                m_runtimeCosts.reset();
                m_runtimeBudgetAssessment.reset();
                m_costFramesUntilRefresh = 0u;
                for (auto& pending : m_pendingGuiActions)
                    pending.reset();
            }

            void prepare_project_gui()
            {
                epochengine::project_gui::ArtifactLibrary library{
                    m_projectId, m_projectRoot};
                auto loaded = library.load_latest(
                    project_gui::canonical_source_path);
                if (!loaded)
                {
                    m_projectGuiStatus = loaded.code
                            == epochengine::project_gui::LibraryCode::not_found
                        ? "Project GUI: no compiled GUI artifact."
                        : std::string{"Project GUI load failed: "}
                            + std::string{
                                epochengine::project_gui::library_code_name(
                                    loaded.code)};
                    return;
                }

                auto runtime = std::make_unique<
                    epochengine::project_gui_runtime::RuntimeSession>(
                        std::move(loaded.artifact));
                if (!runtime->valid())
                {
                    m_projectGuiStatus =
                        "Project GUI runtime rejected the compiled artifact.";
                    return;
                }
                m_projectGuiRuntime = std::move(runtime);
                m_projectGuiAdapter = std::make_unique<
                    epochengine::project_gui::EpochGuiAdapter>(
                        m_projectId, m_projectRoot);
                m_projectGuiStatus = "Project GUI runtime ready.";
            }

            void queue_project_gui_action(
                const epochengine::project_gui_runtime::RuntimeEvent& event)
            {
                namespace project_input = epochengine::project_input;
                const project_input::ActionSemantic semantic =
                    project_input::action_semantic_from_name(event.action);
                if (semantic == project_input::ActionSemantic::invalid)
                {
                    m_projectGuiStatus =
                        "GUI action rejected: unknown project action '"
                        + event.action + "'.";
                    return;
                }
                const auto* projectInput = m_gameplayRuntime
                    ? m_gameplayRuntime->input_profile() : nullptr;
                if (!projectInput)
                {
                    m_projectGuiStatus =
                        "GUI action rejected: no compiled project input profile.";
                    return;
                }
                const auto definition = std::find_if(
                    projectInput->actions.begin(),
                    projectInput->actions.end(),
                    [&](const project_input::ActionDefinition& action)
                    {
                        return action.semantic == semantic
                            && action.id
                                == project_input::stable_action_id(semantic);
                    });
                if (definition == projectInput->actions.end())
                {
                    m_projectGuiStatus =
                        "GUI action rejected: action is absent from the "
                        "compiled project profile.";
                    return;
                }

                project_input::ActionImpulse impulse{
                    .semantic = semantic,
                    .value_q15 = project_input::normalized_unit,
                    .pressed = true};
                if (event.kind
                    == epochengine::project_gui_runtime::EventKind::slider_changed)
                {
                    if (definition->value_kind
                        != project_input::ActionValueKind::axis)
                    {
                        m_projectGuiStatus =
                            "GUI action rejected: slider requires an axis action.";
                        return;
                    }
                    const double normalized = std::clamp(event.value, -1.0, 1.0);
                    impulse.value_q15 = static_cast<std::int32_t>(
                        std::llround(
                            normalized
                            * static_cast<double>(
                                project_input::normalized_unit)));
                    impulse.pressed = false;
                }
                else if (event.kind
                    != epochengine::project_gui_runtime::EventKind::activated)
                {
                    m_projectGuiStatus =
                        "GUI action rejected: event does not produce "
                        "project input.";
                    return;
                }

                const std::size_t index =
                    static_cast<std::size_t>(semantic) - 1u;
                m_pendingGuiActions[index] = impulse;
                m_projectGuiStatus =
                    "GUI action queued: " + event.action + ".";
            }

            void advance_gameplay(
                float frameSeconds,
                bool suppressPhysicalInput)
            {
                constexpr std::size_t actionCount =
                    static_cast<std::size_t>(
                        epochengine::project_input::ActionSemantic::count) - 1u;
                std::array<epochengine::project_input::ActionImpulse, actionCount>
                    guiImpulses{};
                std::size_t guiImpulseCount{};
                for (auto& pending : m_pendingGuiActions)
                {
                    if (pending)
                        guiImpulses[guiImpulseCount++] = *pending;
                    pending.reset();
                }

                if (!m_gameplayRuntime || !m_gameplayRuntime->active())
                    return;
                const auto* projectInput =
                    m_gameplayRuntime->input_profile();
                if (!projectInput)
                {
                    m_actorStatus =
                        "gameplay runtime has no admitted input profile";
                    return;
                }
                if (m_inputFrameIndex
                    == (std::numeric_limits<std::uint64_t>::max)())
                {
                    m_actorStatus = "input sequence exhausted";
                    return;
                }

                ++m_inputFrameIndex;
                auto snapshot = project_input_snapshot(
                    *projectInput,
                    m_inputFrameIndex,
                    !suppressPhysicalInput);
                const auto physicalControllers =
                    epochengine::controller_input::snapshot();
                if (suppressPhysicalInput)
                {
                    epochengine::project_input::InputSnapshot discarded{
                        .frame_index = m_inputFrameIndex};
                    const auto consumed = m_projectControllerSampler.sample(
                        *projectInput,
                        physicalControllers,
                        discarded);
                    if (!consumed)
                    {
                        m_actorStatus = std::string{"controller input "}
                            + std::string{epochengine::
                                project_input_controller::
                                    sample_code_name(consumed.code)};
                        return;
                    }
                }
                else
                {
                    const auto sampled = m_projectControllerSampler.sample(
                        *projectInput,
                        physicalControllers,
                        snapshot);
                    if (!sampled)
                    {
                        m_actorStatus = std::string{"controller input "}
                            + std::string{epochengine::
                                project_input_controller::
                                    sample_code_name(sampled.code)};
                        return;
                    }
                }

                const auto result = m_gameplayRuntime->advance(
                    snapshot,
                    std::span<
                        const epochengine::project_input::ActionImpulse>{
                            guiImpulses.data(), guiImpulseCount},
                    static_cast<double>(frameSeconds));
                m_actorStatus = std::string{epochengine::
                    project_gameplay2d::session_code_name(result.code)}
                    + ": " + std::string{m_gameplayRuntime->diagnostic()};
                if (result.audio_code == epochengine::audio::
                        PlaybackRuntimeCode::physical_queue_saturated)
                {
                    m_audioStatus =
                        "audio ready; physical queue applying backpressure";
                }
                else if (result.audio_code
                    != epochengine::audio::PlaybackRuntimeCode::success)
                {
                    m_audioStatus = std::string{"audio frame "}
                        + std::string{epochengine::audio::
                            playback_runtime_code_name(result.audio_code)};
                }
            }

            void retire_canvas2d_scenes() noexcept
            {
                for (const auto& [owner, generation] : m_canvas2dPublished)
                {
                    (void)generation;
                    (void)epochengine::canvas2d::scene_content::retire(owner);
                }
                m_canvas2dPublished.clear();
            }

            std::string m_projectId{};
            std::string m_projectName{};
            std::string m_projectRoot{};
            std::string m_tileMapPath{};
            std::string m_tileMapStatus{};
            std::string m_inputProfilePath{};
            std::string m_projectInputStatus{};
            std::string m_spriteAnimationPath{};
            std::string m_spriteAnimationStatus{};
            std::string m_audioProfilePath{};
            std::string m_audioStatus{};
            std::string m_actorStatus{};
            std::string m_projectGuiStatus{"Project GUI not prepared."};
            std::unique_ptr<epochengine::project_gui_runtime::RuntimeSession>
                m_projectGuiRuntime{};
            std::unique_ptr<epochengine::project_gui::EpochGuiAdapter>
                m_projectGuiAdapter{};
            std::string m_scenePath{};
            std::string m_worldName{};
            std::string m_scriptName{};
            std::string m_description{};
            epochengine::EditorProjectModelSummary m_modelSummary{};
            epochengine::scene_runtime::SceneRuntime m_sceneRuntime{};
            std::unique_ptr<
                epochengine::project_gameplay2d::Gameplay2DRuntime>
                m_gameplayRuntime{};
            std::optional<
                epochengine::canvas2d::scene_content::SceneContent>
                m_staticCanvasScene{};
            epochengine::project_input_controller::SnapshotSampler
                m_projectControllerSampler{};
            std::uint64_t m_inputFrameIndex{};
            std::optional<
                epochengine::project_gameplay2d::RuntimeCostSnapshot>
                m_runtimeCosts{};
            std::optional<
                epochengine::project_gameplay2d::RuntimeBudgetAssessment>
                m_runtimeBudgetAssessment{};
            epochengine::Budgets m_runtimeBudgets{};
            std::string m_runtimeBudgetProfile{"portable"};
            std::uint32_t m_costFramesUntilRefresh{};
            std::array<
                std::optional<epochengine::project_input::ActionImpulse>,
                static_cast<std::size_t>(
                    epochengine::project_input::ActionSemantic::count) - 1u>
                m_pendingGuiActions{};
            std::vector<ProjectRuntimeEntity> m_entities{};
            epochengine::lighting::LightingFrame m_lightingFrame{};
            timing::Clock::time_point m_lastFrame{};
            bool m_hasLastFrame{ false };
            epochengine::previewgrid::CameraMode m_cameraMode{ epochengine::previewgrid::CameraMode::Editor };
            input::ProfilePreset m_inputProfile{ input::ProfilePreset::EditorDefault };
            std::unordered_map<const void*, bool> m_cameraApplied{};
            std::unordered_map<const void*, std::uint64_t> m_canvas2dPublished{};
            PreviewLookState m_lookState{};
        };

        [[nodiscard]] std::vector<ContextGroup> collect_backend_contexts_shared()
        {
            std::vector<ContextGroup> snapshot;

            {
                std::shared_lock lock(epochengine::core::g_backendsMutex);
                snapshot.reserve(epochengine::core::g_backends.size());

                for (auto& [type, backendSlot] : epochengine::core::g_backends)
                {
                    std::vector<std::shared_ptr<epochengine::core::Context>> contexts;
                    contexts.reserve(1 + backendSlot.duplicates.size());

                    if (backendSlot.master) contexts.push_back(backendSlot.master);
                    for (auto& dup : backendSlot.duplicates) contexts.push_back(dup);

                    snapshot.emplace_back(type, std::move(contexts));
                }
            }

            return snapshot;
        }

        [[nodiscard]] std::unique_ptr<epochengine::scene::Scene> make_scene_from_id(std::string_view scene_id)
        {
            if (scene_id.starts_with("project:"))
                return std::make_unique<ProjectPlayScene>(scene_id.substr(8));
            if (scene_id == "snake")
                return std::make_unique<epochengine::snakelike::SnakeLikeScene>();
            if (scene_id == "tetris")
                return std::make_unique<epochengine::tetrislike::TetrisLikeScene>();
            if (scene_id == "frogger")
                return std::make_unique<epochengine::froggerlike::FroggerLikeScene>();
            if (scene_id == "pacman")
                return std::make_unique<epochengine::pacmanlike::PacmanLikeScene>();
            if (scene_id == "sokoban")
                return std::make_unique<epochengine::sokobanlike::SokobanLikeScene>();
            if (scene_id == "bejeweled" || scene_id == "match3")
                return std::make_unique<epochengine::match3like::Match3LikeScene>();
            if (scene_id == "puzzle" || scene_id == "sliding")
                return std::make_unique<epochengine::slidinglike::SlidingPuzzleLikeScene>();
            if (scene_id == "minesweep" || scene_id == "minesweeper")
                return std::make_unique<epochengine::minesweeperlike::MinesweeperLikeScene>();
            if (scene_id == "fourty" || scene_id == "2048")
                return std::make_unique<epochengine::a2048like::A2048LikeScene>();
            if (scene_id == "sandsim" || scene_id == "sand")
                return std::make_unique<epochengine::sandsim::SandSimScene>();
            if (scene_id == "cellular" || scene_id == "cell")
                return std::make_unique<epochengine::cellularsim::CellularSimScene>();
            return {};
        }

        [[nodiscard]] std::string_view scene_id_from_choice(epochengine::menu::Choice choice) noexcept
        {
            using Choice = epochengine::menu::Choice;

            switch (choice)
            {
            case Choice::Snake: return "snake";
            case Choice::Tetris: return "tetris";
            case Choice::Pacman: return "pacman";
            case Choice::Frogger: return "frogger";
            case Choice::Sokoban: return "sokoban";
            case Choice::Minesweep: return "minesweep";
            case Choice::Puzzle: return "puzzle";
            case Choice::Bejeweled: return "bejeweled";
            case Choice::Fourty: return "fourty";
            case Choice::Sandsim: return "sandsim";
            case Choice::Cellular: return "cellular";
            case Choice::Settings:
            case Choice::OpenEditor:
            case Choice::OpenPlantLab:
            case Choice::OpenGuiEditor:
            case Choice::About:
            case Choice::CheckUpdates:
            case Choice::UpdateLatest:
            case Choice::UpdatePanelCancel:
            case Choice::UpdatePanelDismiss:
            case Choice::UpdatePanelRestart:
            case Choice::Exit:
            default:
                return {};
            }
        }

        [[nodiscard]] std::optional<EditorApplicationKind> application_from_choice(
            epochengine::menu::Choice choice) noexcept
        {
            using Choice = epochengine::menu::Choice;
            switch (choice)
            {
            case Choice::OpenPlantLab: return EditorApplicationKind::PlantLab;
            case Choice::OpenGuiEditor: return EditorApplicationKind::GuiEditor;
            case Choice::OpenEditor: return EditorApplicationKind::Standard;
            default: return std::nullopt;
            }
        }

        [[nodiscard]] std::string_view application_label(EditorApplicationKind application) noexcept
        {
            return editor_application_profile(application).display_name;
        }

        [[nodiscard]] epochengine::updater::UpdateChannel default_update_channel()
        {
            return epochengine::updater::UpdateChannel{
                .version_url = epochengine::updater::PROJECT_PACKAGED_VERSION_URL(),
                .binary_url = epochengine::updater::PROJECT_BINARY_URL(),
                .source_url = epochengine::updater::PROJECT_SOURCE_URL(),
                .source_version_url = epochengine::updater::PROJECT_SOURCE_VERSION_URL(),
                .platform_build_status_url = epochengine::updater::PROJECT_ACTION_RUNS_API_URL(),
                .platform_build_job_name = epochengine::updater::PROJECT_UPDATE_BUILD_JOB_NAME(),
            };
        }

        void unload_active_scene(ContextSession& session)
        {
            if (session.active_scene)
            {
                session.active_scene->unload();
                session.active_scene.reset();
            }
        }

        void ensure_menu_initialized(ContextSession& session, const std::shared_ptr<Context>& ctx)
        {
            session.menu.set_max_columns(epochengine::core::cli::menu_columns);
            session.menu.initialize(ctx);
        }

        void reset_to_menu(ContextSession& session, const std::shared_ptr<Context>& ctx)
        {
            epochengine::editor_reset_transient_ui(ctx.get());
            session.menu.cleanup();
            ensure_menu_initialized(session, ctx);
            session.mode = SessionMode::Menu;
            session.return_mode = SessionMode::Menu;
            g_preview_look_states.erase(ctx.get());
        }

        void cleanup_backend_context_shared(epochengine::core::ContextType type,
            std::shared_ptr<epochengine::core::Context> ctx)
        {
            if (!ctx) return;

            (void)type;
            epochengine::gui::cleanup_context(ctx.get());
            epochengine::cleanup_chat_context(ctx.get());
        }

        struct PendingEditorContextSnapshot
        {
            epochengine::core::ContextType target_type{ epochengine::core::ContextType::None };
            std::string gui_route{};
            Context* source_context{ nullptr };
            bool close_source_on_restore{ false };
            epochengine::EditorContextSnapshot snapshot{};
        };

#if defined(_WIN32)
        enum class EditorContextReplacementPhase : unsigned char
        {
            retire_requested = 0,
            retiring_source,
            awaiting_backend,
            restoring_session,
            awaiting_restored_frame,
            retiring_failed_backend
        };

        struct PendingEditorContextReplacement
        {
            epochengine::core::ContextType target_type{ epochengine::core::ContextType::None };
            epochengine::core::ContextType fallback_type{ epochengine::core::ContextType::None };
            epochengine::core::ContextType active_type{ epochengine::core::ContextType::None };
            std::shared_ptr<Context> source_context{};
            std::shared_ptr<Context> active_context{};
            int width{ 1280 };
            int height{ 720 };
            epochengine::EditorContextSnapshot snapshot{};
            EditorContextReplacementPhase phase{ EditorContextReplacementPhase::retire_requested };
            bool fallback_attempted{ false };
        };
#endif

        using LauncherUpdateState = epochengine::launcher_update::Flow;

        template <typename PumpFunc>
        int RunContextSessionLoop(MultiContextManager& mgr, PumpFunc&& pump_events, SessionMode startup_mode)
        {
#if EPOCH_ENABLE_PHYSICAL_AUDIO
            epochengine::audio::PlaybackRuntime processAudio{
                epochengine::audio::make_sdl_audio_device_sink()};
#else
            epochengine::audio::PlaybackRuntime processAudio{};
#endif
            ScopedProcessAudioRuntime audioBinding{processAudio};
            ScopedPhysicalInputRuntime physicalInput{};
#if defined(_WIN32)
            if (startup_mode == SessionMode::Editor)
                mgr.ConstrainPrimaryWindowToWorkArea();
#endif
            std::unordered_map<Context*, ContextSession> sessions;
            std::optional<epochengine::previewgrid::CameraRigSnapshot>
                multicontextCameraBootstrap;
            std::vector<PendingEditorContextSnapshot> pendingEditorSwitchSnapshots;
#if defined(_WIN32)
            std::optional<PendingEditorContextReplacement> pendingEditorContextReplacement;
#endif
            bool running = true;
            LauncherUpdateState launcherUpdate{};
            bool smoke_capture_taken = false;
            bool smoke_capture_armed = false;
            std::uint64_t frame_count = 0;
            const std::uint64_t smoke_max_frames = smoke_frame_budget();
            const auto smokeSwitchTarget =
                epochengine::core::cli::smoke_context_switch_requested
                ? context_type_from_backend_token(epochengine::core::cli::smoke_context_switch_backend)
                : epochengine::core::ContextType::None;
            bool smoke_context_switch_posted = false;
            std::uint64_t smoke_context_switch_exit_frame = 0;
            const std::uint64_t smoke_capture_frame =
                cli::smoke_requested
                ? (cli::capture_requested ? 420u : 30u)
                : 0u;
            const std::uint64_t smoke_capture_settle_frames =
                cli::capture_requested ? 90u : 0u;
            const std::uint64_t smoke_capture_fallback_frames =
                cli::capture_requested ? 150u : 0u;
            auto pump = std::forward<PumpFunc>(pump_events);
            auto publish_launcher_update_status = [&](std::string status)
            {
                launcherUpdate.set_status(std::move(status));
                for (auto& [_, session] : sessions)
                    session.menu.set_status(launcherUpdate.status);
            };
            auto publish_current_launcher_update_status = [&]()
            {
                for (auto& [_, session] : sessions)
                    session.menu.set_status(launcherUpdate.status);
            };
            auto stash_editor_switch_snapshot = [&](
                epochengine::core::ContextType targetType,
                const std::shared_ptr<Context>& sourceCtx,
                std::string_view guiRoute = {},
                bool closeSourceOnRestore = false) -> bool
            {
                if (!sourceCtx)
                    return false;

                auto editorSnapshot = epochengine::editor_capture_context_snapshot(sourceCtx.get());
                if (!editorSnapshot.valid)
                    return false;

                pendingEditorSwitchSnapshots.erase(
                    std::remove_if(
                        pendingEditorSwitchSnapshots.begin(),
                        pendingEditorSwitchSnapshots.end(),
                        [targetType, guiRoute](const auto& item) noexcept
                        {
                            return item.target_type == targetType && item.gui_route == guiRoute;
                        }),
                    pendingEditorSwitchSnapshots.end());
                pendingEditorSwitchSnapshots.push_back(PendingEditorContextSnapshot{
                    .target_type = targetType,
                    .gui_route = std::string{ guiRoute },
                    .source_context = sourceCtx.get(),
                    .close_source_on_restore = closeSourceOnRestore,
                    .snapshot = std::move(editorSnapshot)
                });
                return true;
            };
#if defined(_WIN32)
            auto queue_editor_switch_snapshot = [&] (
                epochengine::core::ContextType targetType,
                Context* sourceContext,
                const epochengine::EditorContextSnapshot& editorSnapshot,
                bool closeSourceOnRestore)
            {
                pendingEditorSwitchSnapshots.erase(
                    std::remove_if(
                        pendingEditorSwitchSnapshots.begin(),
                        pendingEditorSwitchSnapshots.end(),
                        [targetType](const auto& item) noexcept
                        {
                            return item.target_type == targetType && item.gui_route.empty();
                        }),
                    pendingEditorSwitchSnapshots.end());
                pendingEditorSwitchSnapshots.push_back(PendingEditorContextSnapshot{
                    .target_type = targetType,
                    .gui_route = {},
                    .source_context = sourceContext,
                    .close_source_on_restore = closeSourceOnRestore,
                    .snapshot = editorSnapshot
                });
            };
#endif
            enum class PendingEditorRestoreStatus : unsigned char
            {
                none = 0,
                restored,
                failed
            };
            auto post_context_window_close = [](epochengine::core::WindowData* window)
            {
#if defined(_WIN32)
                if (!window)
                    return;

                HWND closeTarget = nullptr;
                if (window->host_hwnd && ::IsWindow(window->host_hwnd) != FALSE)
                    closeTarget = window->host_hwnd;
                else if (window->hwnd && ::IsWindow(window->hwnd) != FALSE)
                    closeTarget = window->hwnd;
                else if (window->hwndChild && ::IsWindow(window->hwndChild) != FALSE)
                    closeTarget = window->hwndChild;

                if (closeTarget)
                    ::PostMessageW(closeTarget, WM_CLOSE, 0, 0);
#else
                (void)window;
#endif
            };
            auto request_context_window_close = [&](Context* context, std::string_view reason) -> bool
            {
                if (!context)
                    return false;

                for (const auto& window : mgr.GetWindows())
                {
                    if (!window
                        || !window->context
                        || window->context.get() != context)
                    {
                        continue;
                    }

                    window->running = false;
                    window->set_should_close(true);
                    post_context_window_close(window.get());
                    logger::get(kEditorLog).logf(
                        logger::LogLevel::INFO,
                        std::source_location::current(),
                        "Requested {} context window close: {}",
                        context_type_label(context->type),
                        std::string{ reason });
                    return true;
                }

                return false;
            };
            auto restore_pending_editor_switch_snapshot = [&](
                const std::shared_ptr<Context>& targetCtx,
                ContextSession& targetSession,
                std::string_view guiRoute) -> PendingEditorRestoreStatus
            {
                if (!targetCtx)
                    return PendingEditorRestoreStatus::none;

                auto pendingIt = std::find_if(
                    pendingEditorSwitchSnapshots.begin(),
                    pendingEditorSwitchSnapshots.end(),
                    [&](const auto& item) noexcept
                    {
                        return item.target_type == targetCtx->type && item.gui_route == guiRoute;
                    });
                if (pendingIt == pendingEditorSwitchSnapshots.end())
                    return PendingEditorRestoreStatus::none;

                const auto& editorSnapshot = pendingIt->snapshot;
                Context* const sourceContext = pendingIt->source_context;
                const bool closeSourceOnRestore = pendingIt->close_source_on_restore;
                const bool routedPanelRestore = !guiRoute.empty();

                auto fail_restore = [&](std::string_view reason) -> PendingEditorRestoreStatus
                {
                    epochengine::editor_set_context_selection_status(
                        targetCtx.get(),
                        std::string{ "Context switch state restore failed: " } + std::string{ reason }
                            + "; the replacement backend will be retired before recovery.");
                    return PendingEditorRestoreStatus::failed;
                };

                if (!editorSnapshot.valid)
                    return fail_restore("replacement editor state snapshot was invalid");

                if (!epochengine::editor_restore_context_snapshot(targetCtx.get(), editorSnapshot))
                    return fail_restore("replacement editor state restore failed");

                unload_active_scene(targetSession);
                targetSession.menu.cleanup();
                targetSession.mode = SessionMode::Editor;
                targetSession.return_mode = SessionMode::Menu;
                targetCtx->clear_scene_viewport();
                targetCtx->set_scene_preview_mode(core::ScenePreviewMode::Editor);

                if (routedPanelRestore)
                {
                    if (sourceContext && sourceContext != targetCtx.get())
                    {
                        epochengine::editor_set_context_selection_status(
                            sourceContext,
                            std::string{ "Detached " } + std::string{ guiRoute }
                                + " panel cloned editor state; source editor remains active.");
                    }
                    pendingEditorSwitchSnapshots.erase(pendingIt);
                    return PendingEditorRestoreStatus::restored;
                }

                if (!closeSourceOnRestore
                    && sourceContext
                    && sourceContext != targetCtx.get())
                {
                    if (auto sourceIt = sessions.find(sourceContext); sourceIt != sessions.end())
                    {
                        unload_active_scene(sourceIt->second);
                        sourceIt->second.menu.cleanup();
                        sourceIt->second.mode = SessionMode::Menu;
                        sourceIt->second.return_mode = SessionMode::Menu;
                    }
                    sourceContext->clear_scene_viewport();
                    sourceContext->set_scene_preview_mode(core::ScenePreviewMode::None);
                    epochengine::editor_set_context_selection_status(
                        sourceContext,
                        std::string{ "Editor session parked after handoff to " }
                            + std::string{ context_type_label(targetCtx->type) }
                            + "; source context stayed alive to avoid backend teardown during switch.");
                }

                epochengine::editor_set_context_selection_status(
                    targetCtx.get(),
                    std::string{ "Editor context switched to " }
                        + std::string{ context_type_label(targetCtx->type) }
                        + "; project, layout, selection, camera, and timeline state restored.");
                pendingEditorSwitchSnapshots.erase(pendingIt);
                return PendingEditorRestoreStatus::restored;
            };

            auto cleanup_retired_editor_context = [&](Context* retiredContext)
            {
                if (!retiredContext)
                    return;

                if (auto sessionIt = sessions.find(retiredContext); sessionIt != sessions.end())
                {
                    unload_active_scene(sessionIt->second);
                    sessionIt->second.menu.cleanup();
                    sessions.erase(sessionIt);
                }

                retiredContext->clear_scene_viewport();
                retiredContext->set_scene_preview_mode(core::ScenePreviewMode::None);
                epochengine::gui::cleanup_context(retiredContext);
                epochengine::cleanup_chat_context(retiredContext);
                g_preview_look_states.erase(retiredContext);
            };

            while (running)
            {
                if (frame_count++ >= smoke_max_frames)
                {
                    logger::get(startup_mode == SessionMode::Editor ? kEditorLog : kEngineLog).log(
                        logger::LogLevel::INFO,
                        "Context session loop ended after the bounded smoke frame budget.",
                        std::source_location::current());
                    running = false;
                    break;
                }
                if (!pump())
                {
                    logger::get(startup_mode == SessionMode::Editor ? kEditorLog : kEngineLog).log(
                        logger::LogLevel::INFO,
                        "Context session loop ended because the platform event pump requested shutdown.",
                        std::source_location::current());
                    running = false;
                    break;
                }

                static_cast<void>(
                    epochengine::controller_input::poll_physical_input());

                if (!mgr.IsRunning())
                {
                    logger::get(startup_mode == SessionMode::Editor ? kEditorLog : kEngineLog).log(
                        logger::LogLevel::INFO,
                        "Context session loop ended because the context manager stopped running.",
                        std::source_location::current());
                    running = false;
                    break;
                }

                mgr.CleanupFinishedWindows();

#if defined(_WIN32)
                if (pendingEditorContextReplacement)
                {
                    auto eraseReplacementSnapshot = [&](epochengine::core::ContextType type)
                    {
                        pendingEditorSwitchSnapshots.erase(
                            std::remove_if(
                                pendingEditorSwitchSnapshots.begin(),
                                pendingEditorSwitchSnapshots.end(),
                                [type](const auto& item) noexcept
                                {
                                    return item.target_type == type
                                        && item.gui_route.empty()
                                        && item.close_source_on_restore;
                                }),
                            pendingEditorSwitchSnapshots.end());
                    };

                    auto openReplacement = [&](epochengine::core::ContextType type) -> bool
                    {
                        auto& replacement = *pendingEditorContextReplacement;
                        eraseReplacementSnapshot(type);
                        queue_editor_switch_snapshot(
                            type,
                            nullptr,
                            replacement.snapshot,
                            true);

                        std::shared_ptr<Context> createdContext{};
                        if (!mgr.OpenReplacementContextWindow(
                            epochengine::core::DetachedContextWindowRequest{
                                .type = type,
                                .title = std::string{ "Epoch Editor | " } + std::string{ context_type_label(type) },
                                .gui_route = {},
                                .width = replacement.width,
                                .height = replacement.height,
                                .start_docked = true
                            },
                            &createdContext))
                        {
                            eraseReplacementSnapshot(type);
                            return false;
                        }

                        replacement.active_type = type;
                        replacement.active_context = std::move(createdContext);
                        if (!replacement.active_context)
                        {
                            eraseReplacementSnapshot(type);
                            return false;
                        }

                        if (!mgr.findWindowByContext(replacement.active_context))
                        {
                            eraseReplacementSnapshot(type);
                            return false;
                        }

                        replacement.phase = EditorContextReplacementPhase::awaiting_backend;
                        logger::get(kEditorLog).logf(
                            logger::LogLevel::INFO,
                            std::source_location::current(),
                            "Created the new {} context; waiting for backend readiness before restoring editor state.",
                            context_type_label(type));
                        return true;
                    };

                    auto failReplacement = [&]()
                    {
                        pendingEditorSwitchSnapshots.erase(
                            std::remove_if(
                                pendingEditorSwitchSnapshots.begin(),
                                pendingEditorSwitchSnapshots.end(),
                                [](const auto& item) noexcept
                                {
                                    return item.gui_route.empty() && item.close_source_on_restore;
                                }),
                            pendingEditorSwitchSnapshots.end());
                        logger::get(kEditorLog).logf(
                            logger::LogLevel::Error,
                            std::source_location::current(),
                            "Editor host could not create either the requested {} context or the {} recovery context.",
                            context_type_label(pendingEditorContextReplacement->target_type),
                            context_type_label(pendingEditorContextReplacement->fallback_type));
                        mgr.EndContextReplacement();
                        pendingEditorContextReplacement.reset();
                    };

                    auto openFallbackOrFail = [&]()
                    {
                        auto& replacement = *pendingEditorContextReplacement;
                        if (!replacement.fallback_attempted
                            && replacement.fallback_type != epochengine::core::ContextType::None)
                        {
                            replacement.fallback_attempted = true;
                            if (openReplacement(replacement.fallback_type))
                            {
                                logger::get(kEditorLog).logf(
                                    logger::LogLevel::WARN,
                                    std::source_location::current(),
                                    "Requested {} backend failed; restoring the editor through the {} recovery backend.",
                                    context_type_label(replacement.target_type),
                                    context_type_label(replacement.fallback_type));
                                return;
                            }
                        }
                        failReplacement();
                    };

                    auto& replacement = *pendingEditorContextReplacement;
                    if (replacement.phase == EditorContextReplacementPhase::retire_requested)
                    {
                        epochengine::editor_set_context_selection_status(
                            replacement.source_context.get(),
                            std::string{ "Switching editor to " } + std::string{ context_type_label(replacement.target_type) }
                                + "; preserving editor state while the previous backend is fully retired.");

                        if (!request_context_window_close(
                            replacement.source_context.get(),
                            "retiring source backend at the frame boundary before single-window context replacement"))
                        {
                            const auto targetType = replacement.target_type;
                            epochengine::editor_set_context_selection_status(
                                replacement.source_context.get(),
                                std::string{ "Context switch to " } + std::string{ context_type_label(targetType) }
                                    + " failed: source context could not enter the replacement transaction.");
                            logger::get(kEditorLog).logf(
                                logger::LogLevel::Error,
                                std::source_location::current(),
                                "Context selector failed to retire the source context before replacing it with {}.",
                                context_type_label(targetType));
                            mgr.EndContextReplacement();
                            pendingEditorContextReplacement.reset();
                        }
                        else
                        {
                            replacement.phase = EditorContextReplacementPhase::retiring_source;
                            logger::get(kEditorLog).logf(
                                logger::LogLevel::INFO,
                                std::source_location::current(),
                                "Context selector began a frame-boundary single-window replacement transaction for {}.",
                                context_type_label(replacement.target_type));
                        }
                    }
                    else if (replacement.phase == EditorContextReplacementPhase::retiring_source)
                    {
                        if (mgr.IsContextRetired(replacement.source_context.get()))
                        {
                            // The renderer is joined now. Dispose its GUI/font/
                            // scene session before another backend can reuse the
                            // same Context object or initialize a new GUI route.
                            cleanup_retired_editor_context(replacement.source_context.get());
                            replacement.source_context.reset();

                            if (!openReplacement(replacement.target_type))
                                openFallbackOrFail();
                        }
                    }
                    else if (replacement.phase == EditorContextReplacementPhase::awaiting_backend
                        || replacement.phase == EditorContextReplacementPhase::restoring_session
                        || replacement.phase == EditorContextReplacementPhase::awaiting_restored_frame)
                    {
                        auto* window = mgr.findWindowByContext(replacement.active_context);
                        const auto lifecycle = window
                            ? window->backend_lifecycle()
                            : epochengine::core::BackendLifecycleState::stopped;

                        const bool backendIsLive = window
                            && window->running.load(std::memory_order_acquire)
                            && !window->get_should_close();

                        if (editor_session_restore_allowed(lifecycle)
                            && backendIsLive)
                        {
                            if (replacement.phase == EditorContextReplacementPhase::awaiting_backend)
                            {
                                replacement.phase = EditorContextReplacementPhase::restoring_session;
                                logger::get(kEditorLog).logf(
                                    logger::LogLevel::INFO,
                                    std::source_location::current(),
                                    "Editor host observed the first successful {} backend frame; retaining transaction ownership while the normal session path restores editor state.",
                                    context_type_label(replacement.active_type));
                            }
                            else if (replacement.phase == EditorContextReplacementPhase::awaiting_restored_frame)
                            {
                                const auto acknowledgementGeneration =
                                    window->editorSessionRestoreAckGeneration.load(std::memory_order_acquire);
                                const auto completedGeneration =
                                    window->successfulFrameGeneration.load(std::memory_order_acquire);
                                if (editor_restored_frame_acknowledged(
                                    window->editorSessionRestoreAckQueued.load(std::memory_order_acquire),
                                    acknowledgementGeneration,
                                    completedGeneration))
                                {
                                    const auto readyType = replacement.active_type;
                                    mgr.EndContextReplacement();
                                    pendingEditorContextReplacement.reset();
                                    logger::get(kEditorLog).logf(
                                        logger::LogLevel::INFO,
                                        std::source_location::current(),
                                        "Editor host committed the single-window context replacement after the first restored {} editor frame completed.",
                                        context_type_label(readyType));
                                }
                            }
                        }
                        else if (lifecycle == epochengine::core::BackendLifecycleState::failed
                            || lifecycle == epochengine::core::BackendLifecycleState::stopped
                            || (window && !backendIsLive))
                        {
                            eraseReplacementSnapshot(replacement.active_type);
                            if (window)
                            {
                                request_context_window_close(
                                    replacement.active_context.get(),
                                    "retiring failed replacement backend before recovery");
                            }
                            replacement.phase = EditorContextReplacementPhase::retiring_failed_backend;
                        }
                    }
                    else if (replacement.phase == EditorContextReplacementPhase::retiring_failed_backend
                        && mgr.IsContextRetired(replacement.active_context.get()))
                    {
                        openFallbackOrFail();
                    }
                }
#endif

#if defined(_WIN32)
                bool has_live_native_window = false;
                for (const auto& win : mgr.GetWindows())
                {
                    if (!win)
                        continue;

                    HWND liveWindow = nullptr;
                    if (win->hwnd && ::IsWindow(win->hwnd) != FALSE)
                        liveWindow = win->hwnd;
                    else if (win->hwndChild && ::IsWindow(win->hwndChild) != FALSE)
                        liveWindow = win->hwndChild;
                    else if (win->host_hwnd && ::IsWindow(win->host_hwnd) != FALSE)
                        liveWindow = win->host_hwnd;

                    if (liveWindow)
                    {
                        has_live_native_window = true;
                        break;
                    }
                }

                if (!has_live_native_window && !mgr.ContextReplacementInProgress())
                {
                    logger::get(startup_mode == SessionMode::Editor ? kEditorLog : kEngineLog).log(
                        logger::LogLevel::WARN,
                        "Context session loop ended because no live native window remained.",
                        std::source_location::current());
                    mgr.StopRunning();
                    running = false;
                    break;
                }
#endif

                auto snapshot = collect_backend_contexts_shared();
                bool any_context_alive = false;
#if !defined(EPOCH_SINGLE_PARENT) || (EPOCH_SINGLE_PARENT == 0)
                std::size_t active_context_count = 0;
                for (auto& [_, contexts] : snapshot)
                {
                    for (auto& ctx : contexts)
                    {
                        if (ctx) ++active_context_count;
                    }
                }

                bool raylib_close_from_window = false;
#if defined(EPOCH_USING_RAYLIB) && (EPOCH_USING_RAYLIB == 1)
                {
                    const auto& raylib_state = epochengine::raylibstate::s_raylibstate;
                    raylib_close_from_window = raylib_state.running && !raylib_state.renderingActive;

                    if (raylib_close_from_window)
                        epochengine::raylibstate::s_raylibstate.renderingActive = false;
                }
#endif
#endif

                auto arm_launcher_loading = [](ContextSession& targetSession)
                {
                    targetSession.launcher_loading_started = timing::Clock::now();
                    targetSession.launcher_loading_required_batch_generation = 0u;
                };

                auto switch_session_to_editor = [&](
                    ContextSession& targetSession,
                    const std::shared_ptr<Context>& targetCtx,
                    EditorApplicationKind application)
                {
                    if (!targetCtx)
                        return;

                    epochengine::input::keyPressed.reset();
                    epochengine::input::mousePressed.reset();
                    epochengine::input::mouseWheel.store(0, std::memory_order_relaxed);

                    unload_active_scene(targetSession);
                    targetSession.menu.cleanup();
                    targetSession.mode = SessionMode::Editor;
                    targetSession.return_mode = SessionMode::Menu;
                    targetSession.launcher_loading_started.reset();
                    targetSession.launcher_loading_required_batch_generation = 0u;

                    epochengine::editor_suppress_startup_update_check(targetCtx);
                    epochengine::editor_load_application(targetCtx, application);
#if defined(_WIN32)
                    mgr.ConstrainPrimaryWindowToWorkArea(targetCtx);
#endif

                    targetCtx->clear_scene_viewport();
                    targetCtx->set_scene_preview_mode(core::ScenePreviewMode::Editor);
                };

                auto focus_context_window = [&](const std::shared_ptr<Context>& targetCtx)
                {
                    if (!targetCtx)
                        return;

                    if (auto* targetWin = mgr.findWindowByContext(targetCtx))
                    {
#if defined(_WIN32)
                        HWND focusHwnd = nullptr;
                        if (targetWin->hwndChild && ::IsWindow(targetWin->hwndChild) != FALSE)
                            focusHwnd = targetWin->hwndChild;
                        else if (targetWin->hwnd && ::IsWindow(targetWin->hwnd) != FALSE)
                            focusHwnd = targetWin->hwnd;
                        else if (targetWin->host_hwnd && ::IsWindow(targetWin->host_hwnd) != FALSE)
                            focusHwnd = targetWin->host_hwnd;

                        if (focusHwnd)
                        {
                            ::ShowWindow(focusHwnd, SW_SHOWNORMAL);
                            ::BringWindowToTop(focusHwnd);
                            ::SetFocus(focusHwnd);
                        }
#endif
                    }
                };

                auto live_launcher_contexts = [&]()
                {
                    constexpr std::array contextOrder{
                        epochengine::core::ContextType::DirectX,
                        epochengine::core::ContextType::OpenGL,
                        epochengine::core::ContextType::SDL,
                        epochengine::core::ContextType::SFML,
                        epochengine::core::ContextType::RayLib,
                        epochengine::core::ContextType::Vulkan,
                        epochengine::core::ContextType::Software
                    };

                    std::vector<std::shared_ptr<Context>> liveContexts;
                    liveContexts.reserve(snapshot.size());
                    auto append = [&](epochengine::core::ContextType desiredType)
                    {
                        for (auto& [candidateType, contexts] : snapshot)
                        {
                            if (candidateType != desiredType)
                                continue;
                            for (auto& candidate : contexts)
                            {
                                if (candidate && mgr.findWindowByContext(candidate))
                                    liveContexts.push_back(candidate);
                            }
                            break;
                        }
                    };

                    for (const auto type : contextOrder)
                        append(type);
                    for (auto& [candidateType, contexts] : snapshot)
                    {
                        if (std::ranges::find(contextOrder, candidateType) != contextOrder.end())
                            continue;
                        for (auto& candidate : contexts)
                        {
                            if (candidate && mgr.findWindowByContext(candidate))
                                liveContexts.push_back(candidate);
                        }
                    }
                    return liveContexts;
                };

                auto resolve_launcher_context = [&](const std::shared_ptr<Context>& sourceCtx,
                    epochengine::core::ContextType requestedType)
                {
                    for (auto& candidate : live_launcher_contexts())
                    {
                        if (candidate && candidate->type == requestedType)
                            return candidate;
                    }
                    return sourceCtx;
                };
                auto switch_editor_context = [&](const std::shared_ptr<Context>& sourceCtx,
                    epochengine::core::ContextType requestedType)
                {
#if defined(_WIN32)
                    if (pendingEditorContextReplacement)
                    {
                        epochengine::editor_set_context_selection_status(
                            sourceCtx.get(),
                            "Context switch is already adopting a replacement backend. Wait for the restored frame.");
                        return;
                    }
#endif

                    const auto resolvedTargetType = resolve_context_driver_type(
                        requestedType,
                        sourceCtx ? sourceCtx->type : epochengine::core::ContextType::OpenGL);
                    if (!resolvedTargetType)
                    {
                        epochengine::editor_set_context_selection_status(
                            sourceCtx.get(),
                            "Context switch failed: requested backend is unavailable in this build/session.");
                        logger::get(kEditorLog).logf(
                            logger::LogLevel::Error,
                            std::source_location::current(),
                            "Context selector rejected unavailable explicit {} backend.",
                            context_type_label(requestedType));
                        return;
                    }
                    const auto targetType = *resolvedTargetType;
                    if (!is_context_driver_candidate(targetType))
                    {
                        epochengine::editor_set_context_selection_status(
                            sourceCtx.get(),
                            "Context switch failed: requested backend is not an editor context candidate.");
                        return;
                    }
                    const auto is_live_primary_editor_window = [](const auto* window) noexcept
                    {
                        return window
                            && window->guiRoute.empty()
                            && !window->isFloating
                            && window->running.load(std::memory_order_acquire)
                            && !window->get_should_close()
                            && window->backend_lifecycle()
                                == epochengine::core::BackendLifecycleState::ready;
                    };
                    std::shared_ptr<Context> targetCtx;
                    if (sourceCtx && sourceCtx->type == targetType)
                    {
                        auto* sourceWindow = mgr.findWindowByContext(sourceCtx);
                        if (is_live_primary_editor_window(sourceWindow))
                        {
                            targetCtx = sourceCtx;
                        }
                    }

                    if (!targetCtx)
                    {
                        for (auto& [candidateType, contexts] : snapshot)
                        {
                            if (candidateType != targetType)
                                continue;
                            for (auto& candidateCtx : contexts)
                            {
                                auto* candidateWindow = candidateCtx
                                    ? mgr.findWindowByContext(candidateCtx)
                                    : nullptr;
                                if (is_live_primary_editor_window(candidateWindow))
                                {
                                    targetCtx = candidateCtx;
                                    break;
                                }
                            }
                            if (targetCtx)
                                break;
                        }
                    }

                    if (!targetCtx)
                    {
                        if (!sourceCtx)
                        {
                            logger::get(kEditorLog).logf(
                                logger::LogLevel::Error,
                                std::source_location::current(),
                                "Context selector chose {}, but no source editor context exists to migrate.",
                                context_type_label(targetType));
                            return;
                        }

#if defined(_WIN32)
                        if (!mgr.GetParentWindow() || ::IsWindow(mgr.GetParentWindow()) == FALSE)
                        {
                            epochengine::editor_set_context_selection_status(
                                sourceCtx.get(),
                                "Context switch requires the single-window editor host; standalone backend windows are not replaced in place.");
                            return;
                        }

                        auto editorSnapshot = epochengine::editor_capture_context_snapshot(sourceCtx.get());
                        if (!editorSnapshot.valid)
                        {
                            epochengine::editor_set_context_selection_status(
                                sourceCtx.get(),
                                std::string{ "Context switch to " } + std::string{ context_type_label(targetType) }
                                    + " failed: editor state capture did not produce a restorable snapshot.");
                            logger::get(kEditorLog).logf(
                                logger::LogLevel::Error,
                                std::source_location::current(),
                                "Context selector could not capture editor state before replacing the active context with {}.",
                                context_type_label(targetType));
                            return;
                        }

                        int replacementWidth = sourceCtx->get_width_safe();
                        int replacementHeight = sourceCtx->get_height_safe();
                        if (auto* sourceWin = mgr.findWindowByContext(sourceCtx))
                        {
                            replacementWidth = (std::max)(replacementWidth, sourceWin->width);
                            replacementHeight = (std::max)(replacementHeight, sourceWin->height);
                        }

                        mgr.BeginContextReplacement();
                        pendingEditorContextReplacement = PendingEditorContextReplacement{
                            .target_type = targetType,
                            .fallback_type = sourceCtx->type,
                            .source_context = sourceCtx,
                            .width = (std::max)(720, replacementWidth),
                            .height = (std::max)(440, replacementHeight),
                            .snapshot = std::move(editorSnapshot)
                        };

                        epochengine::editor_set_context_selection_status(
                            sourceCtx.get(),
                            std::string{ "Queued editor switch to " } + std::string{ context_type_label(targetType) }
                                + "; the current frame will finish before backend retirement begins.");
                        logger::get(kEditorLog).logf(
                            logger::LogLevel::INFO,
                            std::source_location::current(),
                            "Context selector queued a frame-boundary single-window replacement transaction for {}.",
                            context_type_label(targetType));
                        return;
#else
                        epochengine::editor_set_context_selection_status(
                            sourceCtx.get(),
                            "Context switching is unavailable in this platform host; the active editor context was kept.");
                        return;
#endif
                    }

                    std::optional<epochengine::EditorContextSnapshot> targetSnapshot{};
                    if (sourceCtx && targetCtx.get() != sourceCtx.get())
                    {
                        auto capturedSnapshot = epochengine::editor_capture_context_snapshot(sourceCtx.get());
                        if (!capturedSnapshot.valid)
                        {
                            epochengine::editor_set_context_selection_status(
                                sourceCtx.get(),
                                std::string{ "Context switch to " } + std::string{ context_type_label(targetType) }
                                    + " failed: editor state capture did not produce a restorable snapshot.");
                            logger::get(kEditorLog).logf(
                                logger::LogLevel::Error,
                                std::source_location::current(),
                                "Context selector could not capture editor state before promoting live {}.",
                                context_type_label(targetType));
                            return;
                        }
                        targetSnapshot = std::move(capturedSnapshot);
                    }

                    auto [targetIt, insertedForTarget] = sessions.try_emplace(targetCtx.get());
                    auto& targetSession = targetIt->second;
                    if (insertedForTarget)
                        targetSession.menu.set_max_columns(epochengine::core::cli::menu_columns);

                    unload_active_scene(targetSession);
                    targetSession.menu.cleanup();
                    targetSession.mode = SessionMode::Editor;
                    targetSession.return_mode = SessionMode::Menu;

                    if (targetSnapshot)
                    {
                        if (!epochengine::editor_restore_context_snapshot(targetCtx.get(), *targetSnapshot))
                        {
                            reset_to_menu(targetSession, targetCtx);
                            epochengine::editor_set_context_selection_status(
                                sourceCtx.get(),
                                std::string{ "Context switch to " } + std::string{ context_type_label(targetType) }
                                    + " failed: editor state restore was rejected; the current context stayed primary.");
                            logger::get(kEditorLog).logf(
                                logger::LogLevel::Error,
                                std::source_location::current(),
                                "Context selector rejected live {} promotion because editor state restore failed.",
                                context_type_label(targetType));
                            return;
                        }

                        logger::get(kEditorLog).logf(
                            logger::LogLevel::INFO,
                            std::source_location::current(),
                            "Context selector restored editor state into the live {} context.",
                            context_type_label(targetType));
                    }

                    bool primarySurfaceReady = true;
#if defined(_WIN32)
                    if (mgr.GetParentWindow())
                        primarySurfaceReady = mgr.PromotePrimaryWindow(targetCtx);
#endif
                    if (!primarySurfaceReady)
                    {
                        if (targetSnapshot)
                            reset_to_menu(targetSession, targetCtx);
                        epochengine::editor_set_context_selection_status(
                            sourceCtx ? sourceCtx.get() : targetCtx.get(),
                            std::string{ "Context switch to " } + std::string{ context_type_label(targetType) }
                                + " failed: the backend could not acquire the baked primary surface.");
                        logger::get(kEditorLog).logf(
                            logger::LogLevel::Error,
                            std::source_location::current(),
                            "Context selector could not promote live {} to the baked primary surface.",
                            context_type_label(targetType));
                        return;
                    }
                    for (auto& [_, contexts] : snapshot)
                    {
                        for (auto& candidateCtx : contexts)
                        {
                            if (!candidateCtx || candidateCtx.get() == targetCtx.get())
                                continue;

                            auto parkedIt = sessions.find(candidateCtx.get());
                            if (parkedIt == sessions.end() || parkedIt->second.mode != SessionMode::Editor)
                                continue;

                            reset_to_menu(parkedIt->second, candidateCtx);
                            epochengine::editor_set_context_selection_status(
                                candidateCtx.get(),
                                std::string{ "Editor parked; active editor is now the live " }
                                    + std::string{ context_type_label(targetType) } + " context.");
                        }
                    }

                    targetCtx->clear_scene_viewport();
                    targetCtx->set_scene_preview_mode(core::ScenePreviewMode::Editor);
                    focus_context_window(targetCtx);

                    epochengine::editor_set_context_selection_status(
                        targetCtx.get(),
                        std::string{ "Switched to exclusive live " } + std::string{ context_type_label(targetType) } + " editor context.");
                    if (sourceCtx && targetCtx.get() != sourceCtx.get())
                    {
                        epochengine::editor_set_context_selection_status(
                            sourceCtx.get(),
                            std::string{ "Handed editor session to live " } + std::string{ context_type_label(targetType) } + " context.");
                    }

                    logger::get(kEditorLog).logf(
                        logger::LogLevel::INFO,
                        std::source_location::current(),
                        "Context selector switched the editor session to the live {} context.",
                        context_type_label(targetType));
                };

                for (auto& [type, contexts] : snapshot)
                {
                    bool backend_has_live_context = false;

                    for (auto& ctx : contexts)
                    {
                        if (!ctx) continue;

#if defined(_WIN32)
                        if (pendingEditorContextReplacement
                            && editor_session_deferred_for_active_replacement(
                                ctx.get(),
                                pendingEditorContextReplacement->active_context.get(),
                                pendingEditorContextReplacement->phase
                                    == EditorContextReplacementPhase::restoring_session
                                    || pendingEditorContextReplacement->phase
                                        == EditorContextReplacementPhase::awaiting_restored_frame))
                        {
                            // The replacement transaction must observe backend
                            // readiness before the generic session path can
                            // restore GUI, font, project, and editor state.
                            // The backend render loop remains fully active.
                            backend_has_live_context = true;
                            continue;
                        }
#endif

                        auto* win = mgr.findWindowByContext(ctx);
                        if (!win)
                        {
#if defined(_WIN32)
                            if (!mgr.IsContextRetired(ctx.get()))
                            {
                                backend_has_live_context = true;
                                continue;
                            }
#endif
                            auto it = sessions.find(ctx.get());
                            if (it != sessions.end())
                            {
                                unload_active_scene(it->second);
                                it->second.menu.cleanup();
                                sessions.erase(it);
                            }
                            epochengine::gui::cleanup_context(ctx.get());
                            epochengine::cleanup_chat_context(ctx.get());
                            continue;
                        }

                        if (win->routedRedockRequested.exchange(false))
                        {
                            if (!win->guiRoute.empty())
                            {
                                const std::uint8_t dockTarget =
                                    win->routedDockTarget.exchange(
                                        0u,
                                        std::memory_order_acq_rel);
                                epochengine::editor_redock_context_panel(
                                    win->guiRoute,
                                    dockTarget);
                            }
                            win->running = false;
                            win->set_should_close(true);
                            post_context_window_close(win);
                        }

                        if (!win->running || win->get_should_close())
                        {
#if defined(_WIN32)
                            if (win->get_should_close() && !mgr.IsContextRetired(ctx.get()))
                            {
                                backend_has_live_context = true;
                                continue;
                            }
#endif
                            if (!win->guiRoute.empty())
                                epochengine::editor_notify_context_panel_closed(win->guiRoute);
                            auto existingSession = sessions.find(ctx.get());
                            if (existingSession != sessions.end())
                            {
                                unload_active_scene(existingSession->second);
                                existingSession->second.menu.cleanup();
                                sessions.erase(existingSession);
                            }
                            ctx->clear_scene_viewport();
                            ctx->set_scene_preview_mode(core::ScenePreviewMode::None);
                            epochengine::gui::cleanup_context(ctx.get());
                            epochengine::cleanup_chat_context(ctx.get());
                            g_preview_look_states.erase(ctx.get());
                            continue;
                        }

                        const auto backendLifecycle = win->backend_lifecycle();
                        if (!editor_session_restore_allowed(backendLifecycle))
                        {
                            if (backendLifecycle == epochengine::core::BackendLifecycleState::pending
                                || backendLifecycle == epochengine::core::BackendLifecycleState::initializing)
                            {
                                backend_has_live_context = true;
                            }
                            continue;
                        }

                        auto [it, inserted] = sessions.try_emplace(ctx.get());
                        auto& session = it->second;
                        auto pendingRestoreStatus = PendingEditorRestoreStatus::none;

                        if (inserted)
                        {
                            session.mode = startup_mode;
                            session.return_mode = startup_mode;
                            session.menu.set_max_columns(epochengine::core::cli::menu_columns);

                            if (startup_mode == SessionMode::Menu)
                                ensure_menu_initialized(session, ctx);

                            std::string startup_scene_name = epochengine::core::cli::scene_name;
                            if (startup_scene_name.empty())
                                startup_scene_name = read_environment_string("EPOCH_PROJECT_RUNTIME_SCENE");
                            if (startup_scene_name.empty()
                                && startup_mode != SessionMode::Editor)
                            {
                                const std::string project_id = read_environment_string("EPOCH_EDITOR_PROJECT_ID");
                                if (!project_id.empty())
                                    startup_scene_name = "project:" + project_id;
                            }
                            if (!startup_scene_name.empty())
                            {
                                if (auto direct_scene = make_scene_from_id(startup_scene_name))
                                {
                                    session.active_scene = std::move(direct_scene);
                                    session.active_scene->load();
                                    session.mode = SessionMode::Scene;
                                    session.return_mode = SessionMode::Exit;
                                }
                            }

                            pendingRestoreStatus = restore_pending_editor_switch_snapshot(ctx, session, win->guiRoute);
                            if (pendingRestoreStatus == PendingEditorRestoreStatus::restored)
                            {
                                session.shared_camera_bootstrap_applied = true;
                                logger::get(kEditorLog).logf(
                                    logger::LogLevel::INFO,
                                    std::source_location::current(),
                                    "Restored editor state into the new {} context.",
                                    context_type_label(ctx->type));
                            }
                            else if (pendingRestoreStatus == PendingEditorRestoreStatus::failed)
                            {
                                logger::get(kEditorLog).logf(
                                    logger::LogLevel::Error,
                                    std::source_location::current(),
                                    "New {} context entered the session loop, but editor state restore failed.",
                                    context_type_label(ctx->type));
                            }

                            if (!win->guiRoute.empty())
                            {
                                session.shared_camera_bootstrap_applied = true;
                            }
                            else if (startup_mode == SessionMode::Editor
                                && pendingRestoreStatus == PendingEditorRestoreStatus::none
                                && multicontextCameraBootstrap)
                            {
                                session.shared_camera_bootstrap_applied =
                                    epochengine::previewgrid::restore_camera_rig_snapshot(
                                        ctx.get(),
                                        *multicontextCameraBootstrap);
                                if (!session.shared_camera_bootstrap_applied)
                                {
                                    logger::get(kEditorLog).logf(
                                        logger::LogLevel::Error,
                                        std::source_location::current(),
                                        "Could not apply the shared editor camera bootstrap to the {} context.",
                                        context_type_label(ctx->type));
                                }
                            }
                        }

#if defined(_WIN32)
                        const bool ownsActiveReplacement = pendingEditorContextReplacement
                            && pendingEditorContextReplacement->phase
                                == EditorContextReplacementPhase::restoring_session
                            && pendingEditorContextReplacement->active_context.get() == ctx.get();
                        if (ownsActiveReplacement)
                        {
                            if (pendingRestoreStatus == PendingEditorRestoreStatus::restored)
                            {
                                auto& replacement = *pendingEditorContextReplacement;
                                const auto readyType = replacement.active_type;
                                win->editorSessionRestoreAckGeneration.store(0, std::memory_order_release);
                                win->editorSessionRestoreAckQueued.store(false, std::memory_order_release);
                                replacement.phase = EditorContextReplacementPhase::awaiting_restored_frame;
                                logger::get(kEditorLog).logf(
                                    logger::LogLevel::INFO,
                                    std::source_location::current(),
                                    "Editor host restored the {} session; retaining transaction ownership through its first restored editor frame.",
                                    context_type_label(readyType));
                            }
                            else
                            {
                                auto& replacement = *pendingEditorContextReplacement;
                                const auto failedType = replacement.active_type;
                                pendingEditorSwitchSnapshots.erase(
                                    std::remove_if(
                                        pendingEditorSwitchSnapshots.begin(),
                                        pendingEditorSwitchSnapshots.end(),
                                        [failedType](const auto& item) noexcept
                                        {
                                            return item.target_type == failedType
                                                && item.gui_route.empty()
                                                && item.close_source_on_restore;
                                        }),
                                    pendingEditorSwitchSnapshots.end());
                                epochengine::editor_set_context_selection_status(
                                    ctx.get(),
                                    std::string{ "Context switch to " }
                                        + std::string{ context_type_label(failedType) }
                                        + " could not restore editor state; retiring it before recovery.");
                                request_context_window_close(
                                    ctx.get(),
                                    "retiring replacement backend after editor session restore failure");
                                replacement.phase = EditorContextReplacementPhase::retiring_failed_backend;
                                logger::get(kEditorLog).logf(
                                    logger::LogLevel::Error,
                                    std::source_location::current(),
                                    "Editor host rejected the {} replacement because session restoration did not complete; recovery remains transaction-owned.",
                                    context_type_label(failedType));
                                continue;
                            }
                        }
#endif

                        const auto now = timing::Clock::now();
                        float dt = 0.0f;
                        if (session.has_last_frame)
                            dt = std::chrono::duration<float>(now - session.last_frame).count();
                        session.last_frame = now;
                        if (!session.has_last_frame)
                            session.simulation.start();
                        session.has_last_frame = true;

                        bool ctx_running = win->running;

                        auto tick_time_spine = [&]()
                        {
                            const auto control = epochengine::editor_time_control(ctx.get());
                            session.simulation.set_paused(control.paused);
                            session.simulation.set_time_scale(control.time_scale);
                            session.simulation.set_max_steps_per_frame(control.max_steps_per_frame);
                            session.simulation.set_fixed_dt_seconds(control.fixed_dt_seconds);
                            session.simulation.tick(dt);

                            const auto stepBudget = session.simulation.step_budget();
                            if (stepBudget > 0)
                                session.simulation.consume_steps(stepBudget);
                            if (control.step_once)
                                epochengine::editor_consume_time_step_request(ctx.get());
                        };

                        auto publish_time_snapshot = [&]()
                        {
                            const auto stats = session.simulation.stats();
                            epochengine::editor_set_time_snapshot(ctx.get(), epochengine::EditorTimeSnapshot{
                                .frame_index = stats.frame_index,
                                .simulated_steps = stats.simulated_steps,
                                .step_budget = stats.step_budget,
                                .max_steps_per_frame = stats.max_steps_per_frame,
                                .real_dt_seconds = stats.real_dt_seconds,
                                .scaled_dt_seconds = stats.scaled_dt_seconds,
                                .fixed_dt_seconds = stats.fixed_dt_seconds,
                                .accumulator_seconds = stats.accumulator_seconds,
                                .simulated_seconds = stats.simulated_seconds,
                                .time_scale = stats.time_scale,
                                .paused = stats.paused
                            });
                        };

                        tick_time_spine();
                        publish_time_snapshot();

                        if (!win->guiRoute.empty())
                        {
                            int mx = 0;
                            int my = 0;
                            ctx->get_mouse_position_safe(mx, my);

                            const gui::Vec2 mouse_pos{
                                static_cast<float>(mx),
                                static_cast<float>(my)
                            };
                            const bool mouse_left_down =
                                ctx->is_mouse_button_held_safe(epochengine::input::MouseButton::MouseLeft);

                            ctx->clear_scene_viewport();
                            ctx->set_scene_preview_mode(core::ScenePreviewMode::None);
                            clear_before_ui_frame(ctx);
                            if (!session.routed_gui_upload_refreshed)
                            {
                                gui::refresh_context_resources(ctx.get());
                                session.routed_gui_upload_refreshed = true;
                            }
                            gui::begin_frame(ctx, dt, mouse_pos, mouse_left_down);
                            const auto panel_frame = epochengine::editor_run_context_panel(ctx, win->guiRoute);
                            gui::end_frame();

                            if (panel_frame.command == epochengine::EditorCommand::OpenContextWindow)
                            {
                                const auto resolvedType = resolve_context_driver_type(
                                    panel_frame.requested_context_type,
                                    type);
                                if (!resolvedType)
                                {
                                    logger::get(kEditorLog).logf(
                                        logger::LogLevel::Error,
                                        std::source_location::current(),
                                        "Routed panel rejected unavailable explicit {} backend.",
                                        context_type_label(panel_frame.requested_context_type));
                                    if (ctx_running)
                                        ctx->present_safe();
                                    continue;
                                }
                                const auto requestedType = *resolvedType;
                                const std::string route = panel_frame.command_argument.empty()
                                    ? std::string{ "context.driver" }
                                    : panel_frame.command_argument;
                                const auto routeMeta = detached_panel_route_metadata(route);
                                const bool opened = mgr.OpenDetachedContextWindow(
                                    epochengine::core::DetachedContextWindowRequest{
                                        .type = requestedType,
                                        .title = std::string{ routeMeta.title },
                                        .gui_route = route,
                                        .width = routeMeta.width,
                                        .height = routeMeta.height
                                    });
                                if (opened)
                                {
                                    stash_editor_switch_snapshot(requestedType, ctx, route);
                                    epochengine::editor_mark_context_panel_detached(route, true);
                                }
                                const std::string logLine = opened
                                    ? std::string{ routeMeta.open_success }
                                    : std::string{ routeMeta.open_failure };
                                logger::get(kEditorLog).log(
                                    opened ? logger::LogLevel::INFO : logger::LogLevel::Error,
                                    logLine,
                                    std::source_location::current());
                                if (opened && panel_frame.close_current_context_after_command)
                                {
                                    epochengine::editor_notify_context_panel_closed(win->guiRoute);
                                    ctx_running = false;
                                    win->running = false;
                                    win->set_should_close(true);
                                    post_context_window_close(win);
                                }
                                else if (ctx_running)
                                {
                                    ctx->present_safe();
                                }
                            }
                            else if (panel_frame.command == epochengine::EditorCommand::Exit)
                            {
                                epochengine::editor_notify_context_panel_closed(win->guiRoute);
                                ctx_running = false;
                                win->running = false;
                                win->set_should_close(true);
                                post_context_window_close(win);
                            }
                            else if (ctx_running)
                            {
                                ctx->present_safe();
                            }

                            if (ctx_running)
                                backend_has_live_context = true;
                            else
                            {
#if defined(_WIN32)
                                backend_has_live_context = true;
#else
                                epochengine::editor_notify_context_panel_closed(win->guiRoute);
                                ctx->clear_scene_viewport();
                                ctx->set_scene_preview_mode(core::ScenePreviewMode::None);
                                epochengine::gui::cleanup_context(ctx.get());
                                epochengine::cleanup_chat_context(ctx.get());
                                g_preview_look_states.erase(ctx.get());
                                sessions.erase(ctx.get());
#endif
                            }
                            continue;
                        }

                        auto begin_scene = [&](std::string_view scene_id, SessionMode return_mode)
                        {
                            auto next_scene = make_scene_from_id(scene_id);
                            if (!next_scene)
                                return false;

                            session.menu.cleanup();
                            unload_active_scene(session);
                            session.active_scene = std::move(next_scene);
                            session.active_scene->load();
                            session.mode = SessionMode::Scene;
                            session.return_mode = return_mode;
                            ctx->clear_scene_viewport();
                            ctx->set_scene_preview_mode(core::ScenePreviewMode::None);
                            return true;
                        };

                        switch (session.mode)
                        {
                        case SessionMode::Editor:
                        {
                            if (win->guiRoute.empty()
                                && !session.shared_camera_bootstrap_applied
                                && multicontextCameraBootstrap)
                            {
                                session.shared_camera_bootstrap_applied =
                                    epochengine::previewgrid::restore_camera_rig_snapshot(
                                        ctx.get(),
                                        *multicontextCameraBootstrap);
                                if (!session.shared_camera_bootstrap_applied)
                                {
                                    logger::get(kEditorLog).logf(
                                        logger::LogLevel::Error,
                                        std::source_location::current(),
                                        "Could not apply the shared editor camera bootstrap before the first {} editor frame.",
                                        context_type_label(ctx->type));
                                }
                            }

                            int mx = 0;
                            int my = 0;
                            ctx->get_mouse_position_safe(mx, my);

                            const gui::Vec2 mouse_pos{
                                static_cast<float>(mx),
                                static_cast<float>(my)
                            };

                            const bool mouse_left_down =
                                ctx->is_mouse_button_held_safe(epochengine::input::MouseButton::MouseLeft);
                            const bool mouse_right_down =
                                ctx->is_mouse_button_held_safe(epochengine::input::MouseButton::MouseRight);
                            const bool mouse_middle_down =
                                ctx->is_mouse_button_held_safe(epochengine::input::MouseButton::MouseMiddle);

                            ctx->set_scene_preview_mode(core::ScenePreviewMode::Editor);
                            clear_before_ui_frame(ctx);
                            gui::begin_frame(ctx, dt, mouse_pos, mouse_left_down);
                            const auto editor_frame = epochengine::editor_run(ctx);

                            if (win->guiRoute.empty() && !multicontextCameraBootstrap)
                            {
                                const auto snapshot =
                                    epochengine::previewgrid::capture_camera_rig_snapshot(ctx.get());
                                if (snapshot.valid)
                                {
                                    multicontextCameraBootstrap = snapshot;
                                    session.shared_camera_bootstrap_applied = true;
                                }
                            }

                            if (epochengine::core::cli::smoke_context_switch_requested
                                && !smoke_context_switch_posted
                                && smokeSwitchTarget != epochengine::core::ContextType::None
                                && frame_count >= 45u
                                && editor_frame.command == epochengine::EditorCommand::None
                                && ctx->type != smokeSwitchTarget)
                            {
                                smoke_context_switch_posted = true;
                                logger::get(kEditorLog).logf(
                                    logger::LogLevel::INFO,
                                    std::source_location::current(),
                                    "Smoke requested toolbar-equivalent context switch to {}.",
                                    context_type_label(smokeSwitchTarget));
                                switch_editor_context(ctx, smokeSwitchTarget);
                                smoke_context_switch_exit_frame = frame_count + 90u;
                            }

                            const auto viewport = editor_frame.scene_viewport;
                            const bool mouse_in_scene =
                                mouse_pos.x >= viewport.position.x
                                && mouse_pos.y >= viewport.position.y
                                && mouse_pos.x < (viewport.position.x + viewport.size.x)
                                && mouse_pos.y < (viewport.position.y + viewport.size.y);

                            auto& look_state = g_preview_look_states[ctx.get()];
                            const bool navigationContinues =
                                (look_state.orbiting && mouse_left_down)
                                || (look_state.panning && mouse_middle_down)
                                || ((look_state.dollying || look_state.flying) && mouse_right_down);

                            if (ctx->scene_preview_mode() == core::ScenePreviewMode::Editor
                                && viewport.size.x > 1.0f
                                && viewport.size.y > 1.0f
                                && (mouse_in_scene || navigationContinues))
                            {
                                if (editor_frame.scene_input_captured)
                                {
                                    look_state.last_mouse = mouse_pos;
                                    look_state.orbiting = false;
                                    look_state.panning = false;
                                    look_state.dollying = false;
                                    look_state.flying = false;
                                }
                                else
                                {
                                    const int wheelDelta = epochengine::gui::consume_mouse_wheel_delta();
                                    const bool altHeld = epochengine::input::is_key_held(epochengine::input::Key::LeftAlt)
                                        || epochengine::input::is_key_held(epochengine::input::Key::RightAlt);
                                    const bool shiftHeld = epochengine::input::is_key_held(epochengine::input::Key::LeftShift)
                                        || epochengine::input::is_key_held(epochengine::input::Key::RightShift);
                                    const bool controlHeld = epochengine::input::is_key_held(epochengine::input::Key::LeftControl)
                                        || epochengine::input::is_key_held(epochengine::input::Key::RightControl);
                                    const auto navigationGestures =
                                        epochengine::previewgrid::resolve_camera_navigation_gestures(
                                            altHeld, mouse_left_down, mouse_middle_down, mouse_right_down);
                                    const bool orbiting = navigationGestures.orbiting
                                        && (mouse_in_scene || look_state.orbiting);
                                    const bool panning = navigationGestures.panning
                                        && (mouse_in_scene || look_state.panning);
                                    const bool dollying = navigationGestures.dollying
                                        && (mouse_in_scene || look_state.dollying);
                                    const bool flying = navigationGestures.flying
                                        && (mouse_in_scene || look_state.flying);
                                    const bool keyboardNavigation = flying;
                                    const float navigationMultiplier = shiftHeld
                                        ? 4.0f
                                        : (controlHeld ? 0.25f : 1.0f);
                                    const float forwardInput = !keyboardNavigation ? 0.0f :
                                        (epochengine::input::action_held(epochengine::input::Action::MoveForward) ? 1.0f : 0.0f)
                                        - (epochengine::input::action_held(epochengine::input::Action::MoveBackward) ? 1.0f : 0.0f);
                                    const float rightInput = !keyboardNavigation ? 0.0f :
                                        (epochengine::input::action_held(epochengine::input::Action::MoveRight) ? 1.0f : 0.0f)
                                        - (epochengine::input::action_held(epochengine::input::Action::MoveLeft) ? 1.0f : 0.0f);
                                    const float upInput = !keyboardNavigation ? 0.0f :
                                        (epochengine::input::action_held(epochengine::input::Action::MoveUp) ? 1.0f : 0.0f)
                                        - (epochengine::input::action_held(epochengine::input::Action::MoveDown) ? 1.0f : 0.0f);
                                    const float yawInput = !keyboardNavigation ? 0.0f :
                                        (epochengine::input::action_held(epochengine::input::Action::LookRight) ? 1.0f : 0.0f)
                                        - (epochengine::input::action_held(epochengine::input::Action::LookLeft) ? 1.0f : 0.0f);
                                    const float pitchInput = !keyboardNavigation ? 0.0f :
                                        (epochengine::input::action_held(epochengine::input::Action::LookUp) ? 1.0f : 0.0f)
                                        - (epochengine::input::action_held(epochengine::input::Action::LookDown) ? 1.0f : 0.0f);

                                    if (epochengine::input::action_pressed(epochengine::input::Action::ResetCamera))
                                        epochengine::previewgrid::reset_camera(ctx.get());

                                    if ((flying && look_state.flying) || (orbiting && look_state.orbiting))
                                    {
                                        const float mouseDeltaX = mouse_pos.x - look_state.last_mouse.x;
                                        const float mouseDeltaY = mouse_pos.y - look_state.last_mouse.y;
                                        const float mouseSensitivity = epochengine::input::mouse_look_sensitivity();
                                        epochengine::previewgrid::look_camera(
                                            ctx.get(),
                                            mouseDeltaX * mouseSensitivity,
                                            -mouseDeltaY * mouseSensitivity);
                                    }
                                    else if (panning && look_state.panning)
                                    {
                                        const float mouseDeltaX = mouse_pos.x - look_state.last_mouse.x;
                                        const float mouseDeltaY = mouse_pos.y - look_state.last_mouse.y;
                                        epochengine::previewgrid::pan_camera_drag(
                                            ctx.get(),
                                            mouseDeltaX,
                                            -mouseDeltaY);
                                    }
                                    else if (dollying && look_state.dollying)
                                    {
                                        const float mouseDeltaY = mouse_pos.y - look_state.last_mouse.y;
                                        epochengine::previewgrid::dolly_camera_drag(ctx.get(), mouseDeltaY);
                                    }

                                    if (wheelDelta != 0)
                                    {
                                        const float wheelSteps = static_cast<float>(wheelDelta) / 120.0f;
                                        if (flying)
                                            epochengine::previewgrid::adjust_fly_speed(ctx.get(), wheelSteps);
                                        else
                                            epochengine::previewgrid::zoom_camera(
                                                ctx.get(), wheelSteps * epochengine::input::wheel_zoom_step());
                                    }

                                    epochengine::previewgrid::step_camera(
                                        ctx.get(),
                                        dt,
                                        forwardInput,
                                        rightInput,
                                        upInput,
                                        yawInput,
                                        pitchInput,
                                        navigationMultiplier);

                                    look_state.last_mouse = mouse_pos;
                                    look_state.orbiting = orbiting;
                                    look_state.panning = panning;
                                    look_state.dollying = dollying;
                                    look_state.flying = flying;
                                }
                            }
                            else
                            {
                                look_state.last_mouse = mouse_pos;
                                look_state.orbiting = false;
                                look_state.panning = false;
                                look_state.dollying = false;
                                look_state.flying = false;
                            }

                            switch (editor_frame.command)
                            {
                            case epochengine::EditorCommand::OpenLauncher:
                                arm_launcher_loading(session);
                                reset_to_menu(session, ctx);
                                ctx_running = true;
                                break;
                            case epochengine::EditorCommand::RunGame:
                                if (editor_frame.command_argument.starts_with("project-exe:"))
                                {
                                    const bool launched = launch_project_child_process(editor_frame.command_argument);
                                    logger::get(kEditorLog).logf(
                                        launched ? logger::LogLevel::INFO : logger::LogLevel::Error,
                                        std::source_location::current(),
                                        "Editor {} built child executable '{}'.",
                                        launched ? "launched" : "failed to launch",
                                        editor_frame.command_argument.substr(std::string_view{ "project-exe:" }.size()));
                                    break;
                                }
                                if (!begin_scene(editor_frame.command_argument, SessionMode::Editor))
                                {
                                    logger::get(kEditorLog).logf(
                                        logger::LogLevel::Error,
                                        std::source_location::current(),
                                        "Editor rejected unknown play target '{}'.",
                                        editor_frame.command_argument);
                                }
                                break;
                            case epochengine::EditorCommand::RunScript:
                            {
                                if (editor_frame.command_argument.empty())
                                {
                                    logger::get(kEditorLog).log(
                                        logger::LogLevel::Error,
                                        "Editor rejected an empty script command; select a project-backed script action first.",
                                        std::source_location::current());
                                    break;
                                }
                                const bool ok = epochengine::editor_run_script(
                                    ctx.get(),
                                    std::string_view{ editor_frame.command_argument });
                                logger::get(kEditorLog).logf(
                                    ok ? logger::LogLevel::INFO : logger::LogLevel::Error,
                                    std::source_location::current(),
                                    "Editor script '{}' {}.",
                                    editor_frame.command_argument,
                                    ok ? "completed" : "failed");
                                break;
                            }
                            case epochengine::EditorCommand::SwitchContext:
                                switch_editor_context(
                                    ctx,
                                    editor_frame.requested_context_type == epochengine::core::ContextType::None
                                    ? type
                                    : editor_frame.requested_context_type);
                                break;
                            case epochengine::EditorCommand::OpenContextWindow:
                            {
                                const auto resolvedType = resolve_context_driver_type(
                                    editor_frame.requested_context_type,
                                    type);
                                if (!resolvedType)
                                {
                                    logger::get(kEditorLog).logf(
                                        logger::LogLevel::Error,
                                        std::source_location::current(),
                                        "Detached context request rejected unavailable explicit {} backend.",
                                        context_type_label(editor_frame.requested_context_type));
                                    break;
                                }
                                const auto requestedType = *resolvedType;
                                const std::string route = editor_frame.command_argument.empty()
                                    ? std::string{ "context.driver" }
                                    : editor_frame.command_argument;
                                const auto routeMeta = detached_panel_route_metadata(route);
                                const bool opened = mgr.OpenDetachedContextWindow(
                                    epochengine::core::DetachedContextWindowRequest{
                                        .type = requestedType,
                                        .title = std::string{ routeMeta.title },
                                        .gui_route = route,
                                        .width = routeMeta.width,
                                        .height = routeMeta.height
                                    });
                                if (opened)
                                {
                                    stash_editor_switch_snapshot(requestedType, ctx, route);
                                    epochengine::editor_mark_context_panel_detached(route, true);
                                }
                                const std::string logLine = opened
                                    ? std::string{ routeMeta.open_success }
                                    : std::string{ routeMeta.open_failure };
                                logger::get(kEditorLog).log(
                                    opened ? logger::LogLevel::INFO : logger::LogLevel::Error,
                                    logLine,
                                    std::source_location::current());
                                break;
                            }
                            case epochengine::EditorCommand::UpdateApplication:
                            {
                                logger::get(kEditorLog).log(
                                    logger::LogLevel::INFO,
                                    "Running confirmed smart update command.",
                                    std::source_location::current());
                                const auto result = epochengine::updater::run_update_command(
                                    default_update_channel(),
                                    true,
                                    false,
                                    epochengine::updater::UpdateHandoffMode::StageForRestart);
                                if (result.platform_build_checked && !result.platform_build_ok)
                                {
                                    const std::string reason = result.platform_build_reason.empty()
                                        ? std::string{ "platform build status is not green." }
                                        : result.platform_build_reason;
                                    logger::get(kEditorLog).logf(
                                        logger::LogLevel::Error,
                                        std::source_location::current(),
                                        "Update withheld until {} is green: {}",
                                        result.platform_build_job.empty() ? "platform build" : result.platform_build_job,
                                        reason);
                                }
                                else if (!result.update_available)
                                {
                                    logger::get(kEditorLog).log(
                                        logger::LogLevel::INFO,
                                        "No packaged or source update is currently available.",
                                        std::source_location::current());
                                }
                                else if (result.source_update_performed)
                                {
                                    logger::get(kEditorLog).log(
                                        logger::LogLevel::INFO,
                                        "Source rebuild worker started; restart after restart-ready evidence is reported.",
                                        std::source_location::current());
                                }
                                else if (!result.update_performed)
                                {
                                    logger::get(kEditorLog).log(
                                        logger::LogLevel::Error,
                                        "Update was available but the replacement/install step did not complete.",
                                        std::source_location::current());
                                }
                                break;
                            }
                            case epochengine::EditorCommand::UpdateApplicationFromSource:
                            {
                                logger::get(kEditorLog).log(
                                    logger::LogLevel::INFO,
                                    "Running confirmed advanced source rebuild command.",
                                    std::source_location::current());
                                const bool ok = epochengine::updater::run_source_update_command(
                                    default_update_channel(),
                                    true,
                                    false,
                                    false);
                                if (!ok)
                                {
                                    logger::get(kEditorLog).log(
                                        logger::LogLevel::Error,
                                        "Advanced source rebuild did not complete.",
                                        std::source_location::current());
                                }
                                break;
                            }
                            case epochengine::EditorCommand::Exit:
                                session.mode = SessionMode::Exit;
                                ctx_running = false;
                                win->running = false;
                                break;
                            case epochengine::EditorCommand::OpenProject:
                            case epochengine::EditorCommand::Settings:
                            case epochengine::EditorCommand::None:
                            default:
                                break;
                            }

                            gui::end_frame();
                            if (ctx_running)
                            {
                                ctx->present_safe();
#if defined(_WIN32)
                                const bool awaitsRestoredFrame = pendingEditorContextReplacement
                                    && pendingEditorContextReplacement->phase
                                        == EditorContextReplacementPhase::awaiting_restored_frame
                                    && pendingEditorContextReplacement->active_context.get() == ctx.get();
                                if (awaitsRestoredFrame
                                    && !win->editorSessionRestoreAckQueued.exchange(
                                        true,
                                        std::memory_order_acq_rel))
                                {
                                    auto* const adoptionWindow = win;
                                    win->commandQueue.enqueue([adoptionWindow]() noexcept
                                    {
                                        const auto completedGeneration =
                                            adoptionWindow->successfulFrameGeneration.load(std::memory_order_acquire);
                                        const auto acknowledgementGeneration =
                                            completedGeneration + 1;
                                        adoptionWindow->editorSessionRestoreAckGeneration.store(
                                            acknowledgementGeneration,
                                            std::memory_order_release);
                                    });
                                    logger::get(kEditorLog).logf(
                                        logger::LogLevel::INFO,
                                        std::source_location::current(),
                                        "Queued first-restored-frame acknowledgement for {}.",
                                        context_type_label(ctx->type));
                                }
#endif
                            }
                            break;
                        }

                        case SessionMode::Menu:
                        {
                            ensure_menu_initialized(session, ctx);
                            bool suppress_menu_present = false;
                            auto finish_launcher_update_restart = [&]() -> bool
                            {
                                if (launcherUpdate.restart_kind() == epochengine::launcher_update::RestartKind::packaged)
                                {
                                    launcherUpdate.mark_packaged_restarting();
                                    publish_current_launcher_update_status();
                                    if (epochengine::updater::launch_staged_update_handoff())
                                    {
                                        suppress_menu_present = true;
                                        session.mode = SessionMode::Exit;
                                        ctx_running = false;
                                        win->running = false;
                                        return true;
                                    }

                                    launcherUpdate.mark_packaged_restart_failed();
                                    publish_current_launcher_update_status();
                                    return false;
                                }

                                if (launcherUpdate.restart_kind() == epochengine::launcher_update::RestartKind::source)
                                {
                                    launcherUpdate.mark_source_restart_requested();
                                    publish_current_launcher_update_status();
                                    suppress_menu_present = true;
                                    session.mode = SessionMode::Exit;
                                    ctx_running = false;
                                    win->running = false;
                                    return true;
                                }

                                return false;
                            };
                            auto launcher_update_blocks_mode_switch = [&]() -> bool
                            {
                                if (launcherUpdate.has_pending_work())
                                {
                                    publish_launcher_update_status(
                                        "Update is still checking or staging. Wait for the update panel before opening another mode.");
                                    session.menu.guard_next_input_frames(8u);
                                    return true;
                                }

                                if (launcherUpdate.source_worker_running)
                                {
                                    launcherUpdate.set_status(
                                        launcherUpdate.source_cancel_pending()
                                            ? "Source update cancellation is still settling. Wait for the update panel before opening another mode."
                                            : "Source update is still running. Wait for restart-ready, cancel, or failure evidence before opening another mode.");
                                    publish_current_launcher_update_status();
                                    session.menu.guard_next_input_frames(8u);
                                    return true;
                                }

                                if (const int recentCancelWait =
                                    epochengine::updater::source_update_recent_cancel_seconds_remaining(
                                        launcher_update::kCancelRetryCooldownSeconds);
                                    recentCancelWait > 0)
                                {
                                    launcherUpdate.show_retry_wait_for_seconds(recentCancelWait);
                                    publish_current_launcher_update_status();
                                    session.menu.guard_next_input_frames(8u);
                                    return true;
                                }

                                if (epochengine::updater::source_update_worker_active())
                                {
                                    launcherUpdate.observe_existing_source_worker(
                                        epochengine::updater::source_update_cancel_requested());
                                    publish_current_launcher_update_status();
                                    session.menu.guard_next_input_frames(8u);
                                    return true;
                                }

                                return false;
                            };
                            if (!launcherUpdate.has_pending_work()
                                && !launcherUpdate.source_worker_running
                                && !launcherUpdate.is_restart_ready()
                                && epochengine::updater::source_update_worker_active())
                            {
                                launcherUpdate.observe_existing_source_worker(
                                    epochengine::updater::source_update_cancel_requested());
                                publish_current_launcher_update_status();
                            }

                            if (launcherUpdate.pending_ready())
                            {
                                try
                                {
                                    launcherUpdate.complete_pending_result(launcherUpdate.take_pending_result());
                                    publish_current_launcher_update_status();
                                    logger::get(kEditorLog).log(
                                        logger::LogLevel::INFO,
                                        launcherUpdate.status,
                                        std::source_location::current());
                                }
                                catch (...)
                                {
                                    launcherUpdate.complete_pending_error();
                                    publish_current_launcher_update_status();
                                    logger::get(kEditorLog).log(
                                        logger::LogLevel::Error,
                                        launcherUpdate.status,
                                        std::source_location::current());
                                }
                            }
                            try
                            {
                                launcherUpdate.pump_source_worker();
                            }
                            catch (const std::exception& ex)
                            {
                                launcherUpdate.mark_source_monitor_failed(
                                    std::string{ "Source update monitor failed: " } + ex.what());
                                logger::get(kEditorLog).logf(
                                    logger::LogLevel::Error,
                                    std::source_location::current(),
                                    "Launcher source update monitor failed: {}",
                                    ex.what());
                            }
                            catch (...)
                            {
                                launcherUpdate.mark_source_monitor_failed(
                                    "Source update monitor failed with an unknown error.");
                                logger::get(kEditorLog).log(
                                    logger::LogLevel::Error,
                                    "Launcher source update monitor failed with an unknown error.",
                                    std::source_location::current());
                            }
                            publish_current_launcher_update_status();
                            session.menu.set_update_panel_state(launcherUpdate.panel_state());
                            if (launcherUpdate.is_restart_ready() && launcherUpdate.restart_countdown_expired())
                            {
                                (void)finish_launcher_update_restart();
                                if (!ctx_running)
                                {
                                    clear_before_ui_frame(ctx);
                                    break;
                                }
                            }

                            int mx = 0;
                            int my = 0;
                            ctx->get_mouse_position_safe(mx, my);

                            const gui::Vec2 mouse_pos{
                                static_cast<float>(mx),
                                static_cast<float>(my)
                            };

                            const bool mouse_left_down =
                                ctx->is_mouse_button_held_safe(epochengine::input::MouseButton::MouseLeft);
                            const bool up_pressed =
                                epochengine::input::keyPressed.test(epochengine::input::Key::Up);
                            const bool down_pressed =
                                epochengine::input::keyPressed.test(epochengine::input::Key::Down);
                            const bool left_pressed =
                                epochengine::input::keyPressed.test(epochengine::input::Key::Left);
                            const bool right_pressed =
                                epochengine::input::keyPressed.test(epochengine::input::Key::Right);
                            const bool enter_pressed =
                                epochengine::input::keyPressed.test(epochengine::input::Key::Enter);

                            ctx->clear_scene_viewport();
                            ctx->set_scene_preview_mode(core::ScenePreviewMode::None);
                            clear_before_ui_frame(ctx);
                            gui::begin_frame(ctx, dt, mouse_pos, mouse_left_down);
                            std::optional<epochengine::menu::Choice> choice{};
                            std::optional<EditorApplicationKind> pendingEditorApplication{};
                            bool loadingMinimumElapsed = false;
                            const int transitionWidth = (std::max)(1, ctx ? ctx->get_width_safe() : (win ? win->width : 1));
                            const int transitionHeight = (std::max)(1, ctx ? ctx->get_height_safe() : (win ? win->height : 1));
                            if (session.pending_editor_application && !session.launcher_loading_started)
                                arm_launcher_loading(session);
                            const bool draw_transition_loading =
                                session.launcher_loading_started.has_value();
                            if (draw_transition_loading)
                            {
                                const auto loadingNow = timing::Clock::now();
                                const double loadingElapsedSeconds = std::chrono::duration<double>(
                                    loadingNow - *session.launcher_loading_started).count();
                                loadingMinimumElapsed = loadingElapsedSeconds >= kLauncherLoadingMinimumSeconds;
                                const bool loadingEditor = session.pending_editor_application.has_value();
                                const std::string projectLabel = loadingEditor
                                    ? std::string{ application_label(*session.pending_editor_application) }
                                    : std::string{ "project launcher" };
                                const std::string transitionTitle = loadingEditor ? "Loading Editor" : "Loading Launcher";
                                const std::string transitionMessage = loadingEditor
                                    ? std::string{ "Preparing editor workspace for " } + projectLabel + "."
                                    : std::string{ "Returning to the project launcher." };
                                const std::string transitionLabel = loadingEditor ? "Editor loading" : "Launcher loading";
                                const double activitySeconds = std::chrono::duration<double>(
                                    std::chrono::steady_clock::now().time_since_epoch()).count();
                                const float activityPhase = static_cast<float>(std::fmod(activitySeconds * 0.35, 1.0));
                                const float completedFraction = std::clamp(
                                    static_cast<float>(loadingElapsedSeconds / kLauncherLoadingMinimumSeconds),
                                    0.0f,
                                    1.0f);
                                const float transitionProgress = std::clamp(0.12f + completedFraction * 0.88f, 0.12f, 1.0f);
                                const std::string transitionStatus = completedFraction < 0.34f
                                    ? std::string{ "preparing" }
                                    : completedFraction < 0.92f
                                        ? std::string{ "loading" }
                                        : std::string{ "ready" };
                                gui::begin_top_layer();
                                gui::push_theme(gui::ThemeVariant::ClassicLauncher);
                                gui::begin_window("", { 0.0f, 0.0f }, {
                                    static_cast<float>(transitionWidth),
                                    static_cast<float>(transitionHeight)
                                });
                                gui::loading_screen(gui::LoadingScreenOptions{
                                    .title = transitionTitle,
                                    .message = transitionMessage,
                                    .progress_label = transitionLabel,
                                    .progress_status = transitionStatus,
                                    .progress = transitionProgress,
                                    .viewport_position = { 0.0f, 0.0f },
                                    .viewport_size = {
                                        static_cast<float>(transitionWidth),
                                        static_cast<float>(transitionHeight)
                                    },
                                    .panel_size = { 700.0f, 320.0f },
                                    .title_scale = 1.45f,
                                    .message_scale = 1.18f,
                                    .status_scale = 1.08f,
                                    .dim_background = false,
                                    .capture_input = true,
                                    .show_percent = true,
                                    .activity = true,
                                    .activity_phase = activityPhase,
                                    .reserve_action_row = false
                                });
                                gui::end_window();
                                gui::pop_theme();
                                gui::end_top_layer();
                            }
                            else
                            {
                                const auto liveContexts = live_launcher_contexts();
                                std::vector<epochengine::core::ContextType> liveContextTypes;
                                liveContextTypes.reserve(liveContexts.size());
                                for (const auto& liveContext : liveContexts)
                                {
                                    if (liveContext
                                        && std::find(
                                            liveContextTypes.begin(),
                                            liveContextTypes.end(),
                                            liveContext->type) == liveContextTypes.end())
                                    {
                                        liveContextTypes.push_back(liveContext->type);
                                    }
                                }
                                session.menu.set_launch_context_options(liveContextTypes, ctx->type);
                                choice = session.menu.update_and_draw(
                                    ctx,
                                    win,
                                    dt,
                                    up_pressed,
                                    down_pressed,
                                    left_pressed,
                                    right_pressed,
                                    enter_pressed);
                            }
                            gui::end_frame();

                            if (draw_transition_loading)
                            {
                                if (session.launcher_loading_required_batch_generation == 0u)
                                {
                                    session.launcher_loading_required_batch_generation =
                                        gui::top_layer_batch_generation(ctx.get());
                                    session.launcher_loading_started = timing::Clock::now();
                                    loadingMinimumElapsed = false;
                                }
                                const std::uint64_t requiredBatchGeneration =
                                    session.launcher_loading_required_batch_generation;
                                const bool loadingBatchReplayed =
                                    requiredBatchGeneration != 0u
                                    && gui::replayed_top_layer_batch_generation(ctx.get()) >= requiredBatchGeneration;
                                if (loadingMinimumElapsed && loadingBatchReplayed)
                                {
                                    session.launcher_loading_started.reset();
                                    session.launcher_loading_required_batch_generation = 0u;
                                    if (session.pending_editor_application)
                                    {
                                        pendingEditorApplication = session.pending_editor_application;
                                        session.pending_editor_application.reset();
                                    }
                                }
                                if (ctx_running)
                                    ctx->present_safe();
                                if (pendingEditorApplication)
                                    switch_session_to_editor(
                                        session, ctx, *pendingEditorApplication);
                                break;
                            }

                            if (choice)
                            {
                                const bool updatePanelChoice =
                                    *choice == epochengine::menu::Choice::UpdatePanelCancel
                                    || *choice == epochengine::menu::Choice::UpdatePanelDismiss
                                    || *choice == epochengine::menu::Choice::UpdatePanelRestart;
                                if (updatePanelChoice)
                                {
                                    epochengine::input::keyPressed.reset();
                                    epochengine::input::mousePressed.reset();
                                    epochengine::input::mouseWheel.store(0, std::memory_order_relaxed);
                                    session.menu.guard_next_input_frames(8u);
                                }

                                if (*choice == epochengine::menu::Choice::Exit)
                                {
                                    suppress_menu_present = true;
                                    session.mode = SessionMode::Exit;
                                    ctx_running = false;
                                    win->running = false;
                                }
                                else if (*choice == epochengine::menu::Choice::UpdatePanelRestart)
                                {
                                    if (launcherUpdate.packaged_restart_ready || launcherUpdate.source_restart_ready)
                                    {
                                        (void)finish_launcher_update_restart();
                                    }
                                    else
                                    {
                                        publish_launcher_update_status("No staged update is ready to restart.");
                                        session.menu.guard_next_input_frames(8u);
                                    }
                                }
                                else if (*choice == epochengine::menu::Choice::UpdatePanelDismiss)
                                {
                                    launcherUpdate.dismiss_result();
                                    publish_current_launcher_update_status();
                                    session.menu.set_update_panel_state({});
                                    session.menu.guard_next_input_frames(8u);
                                }
                                else if (*choice == epochengine::menu::Choice::UpdatePanelCancel)
                                {
                                    if (launcherUpdate.source_worker_running && !launcherUpdate.source_cancel_pending())
                                    {
                                        append_launcher_cancel_breadcrumb_noexcept("cancel action accepted");
#if defined(_WIN32)
                                        const bool markerWritten = epochengine::updater::request_source_update_cancel();
                                        append_launcher_cancel_breadcrumb_noexcept(
                                            markerWritten
                                                ? "source update cancel marker written"
                                                : "source update cancel marker write failed");
#else
                                        std::thread([] {
                                            (void)epochengine::updater::request_source_update_cancel();
                                        }).detach();
                                        const bool markerWritten = true;
#endif
                                        launcherUpdate.request_source_cancel(markerWritten);
                                        append_launcher_cancel_breadcrumb_noexcept(
                                            markerWritten
                                                ? "launcher update state marked cancel-pending"
                                                : "launcher update state left retryable after cancel-marker failure");
                                    }
                                    else if (launcherUpdate.source_cancel_pending())
                                    {
                                        publish_launcher_update_status("Source rebuild cancellation is already pending. Waiting for worker cleanup before retry.");
                                    }
                                    else
                                    {
                                        publish_launcher_update_status("No source update is currently running.");
                                    }
                                    append_launcher_cancel_breadcrumb_noexcept("publishing cancel panel state");
                                    publish_current_launcher_update_status();
                                    append_launcher_cancel_breadcrumb_noexcept("cancel panel state published");
                                    session.menu.guard_next_input_frames(8u);
                                    append_launcher_cancel_breadcrumb_noexcept("cancel input guard armed");
                                }
                                else if (*choice == epochengine::menu::Choice::UpdateLatest
                                    || *choice == epochengine::menu::Choice::CheckUpdates)
                                {
                                    const bool installRequested =
                                        *choice == epochengine::menu::Choice::UpdateLatest;
                                    if (launcherUpdate.has_pending_work())
                                    {
                                        publish_launcher_update_status("Update is already checking or staging. Keep this launcher open.");
                                    }
                                    else if (launcherUpdate.source_worker_running)
                                    {
                                        publish_launcher_update_status(
                                            launcherUpdate.source_cancel_pending()
                                                ? "Source rebuild cancellation is already pending. Waiting for worker cleanup before retry."
                                                : "Source update is already running. Use Cancel Update in the update panel.");
                                        publish_current_launcher_update_status();
                                        session.menu.guard_next_input_frames(8u);
                                    }
                                    else if (const int recentCancelWait =
                                        epochengine::updater::source_update_recent_cancel_seconds_remaining(
                                            launcher_update::kCancelRetryCooldownSeconds);
                                        recentCancelWait > 0)
                                    {
                                        launcherUpdate.show_retry_wait_for_seconds(recentCancelWait);
                                        publish_current_launcher_update_status();
                                        session.menu.guard_next_input_frames(8u);
                                    }
                                    else if (epochengine::updater::source_update_worker_active())
                                    {
                                        launcherUpdate.observe_existing_source_worker(
                                            epochengine::updater::source_update_cancel_requested());
                                        publish_current_launcher_update_status();
                                    }
                                    else if (launcherUpdate.has_visible_result())
                                    {
                                        launcherUpdate.dismiss_result();
                                        publish_current_launcher_update_status();
                                        session.menu.guard_next_input_frames(8u);
                                    }
                                    else if (launcherUpdate.update_check_throttled())
                                    {
                                        launcherUpdate.show_retry_wait();
                                        publish_current_launcher_update_status();
                                        session.menu.guard_next_input_frames(8u);
                                    }
                                    else
                                    {
                                        logger::get(kEditorLog).log(
                                            logger::LogLevel::INFO,
                                            "Launcher update requested in-place; keeping the window open while the updater reports evidence.",
                                            std::source_location::current());
                                        try
                                        {
                                            auto updateFuture = std::async(std::launch::async, [installRequested] {
                                                logger::get(kEditorLog).log(
                                                    logger::LogLevel::INFO,
                                                    installRequested
                                                        ? "Launcher update worker entered the confirmed source-update path."
                                                        : "Launcher startup update check entered the availability-only path.",
                                                    std::source_location::current());
                                                return epochengine::updater::run_update_command(
                                                    default_update_channel(),
                                                    installRequested,
                                                    false,
                                                    epochengine::updater::UpdateHandoffMode::StageForRestart);
                                            });
                                            launcherUpdate.begin_update_check(std::move(updateFuture));
                                        }
                                        catch (const std::exception& ex)
                                        {
                                            launcherUpdate.mark_update_start_failed(
                                                std::string{ "Update could not start: " } + ex.what());
                                            logger::get(kEditorLog).logf(
                                                logger::LogLevel::Error,
                                                std::source_location::current(),
                                                "Launcher update worker failed to start: {}",
                                                ex.what());
                                        }
                                        catch (...)
                                        {
                                            launcherUpdate.mark_update_start_failed(
                                                "Update could not start because the update worker threw an unknown startup error.");
                                            logger::get(kEditorLog).log(
                                                logger::LogLevel::Error,
                                                "Launcher update worker failed to start with an unknown error.",
                                                std::source_location::current());
                                        }
                                        publish_current_launcher_update_status();
                                    }
                                }
                                else if (const auto application = application_from_choice(*choice))
                                {
                                    if (!launcher_update_blocks_mode_switch())
                                    {
                                        launcherUpdate.clear_inactive_surface();
                                        session.menu.set_update_panel_state({});

                                        const auto selectedType = session.menu.selected_launch_context();
                                        auto targetCtx = resolve_launcher_context(ctx, selectedType);
#if defined(_WIN32)
                                        if (mgr.GetParentWindow())
                                            static_cast<void>(mgr.PromotePrimaryWindow(targetCtx));
#endif
                                        auto [targetIt, insertedForTarget] = sessions.try_emplace(targetCtx.get());
                                        auto& targetSession = targetIt->second;
                                        if (insertedForTarget)
                                        {
                                            targetSession.menu.set_max_columns(epochengine::core::cli::menu_columns);
                                            ensure_menu_initialized(targetSession, targetCtx);
                                        }

                                        targetSession.pending_editor_application = *application;
                                        arm_launcher_loading(targetSession);
                                        targetSession.menu.guard_next_input_frames(3u);
                                        focus_context_window(targetCtx);
                                    }
                                }
                                else if (*choice == epochengine::menu::Choice::About)
                                {
                                    logger::get(kEditorLog).log(
                                        logger::LogLevel::INFO,
                                        "Epoch launcher now routes direct project entry, editor launch, updates, and settings without the old game-menu shell.",
                                        std::source_location::current());
                                }
                                else
                                {
                                    logger::get(kEditorLog).log(
                                        logger::LogLevel::INFO,
                                        "Ignoring legacy launcher choice that is no longer part of the project-launcher surface.",
                                        std::source_location::current());
                                }
                            }
                            if (suppress_menu_present)
                                clear_before_ui_frame(ctx);
                            else if (ctx_running)
                                ctx->present_safe();
                            break;
                        }

                        case SessionMode::Scene:
                        {
                            ctx->clear_scene_viewport();
                            ctx->set_scene_preview_mode(core::ScenePreviewMode::None);
                            if (session.active_scene)
                            {
                                ctx_running = session.active_scene->frame(ctx, win);
                                if (!ctx_running)
                                {
                                    const bool window_closed = !win->running;
                                    unload_active_scene(session);

                                    if (window_closed || session.return_mode == SessionMode::Exit)
                                    {
                                        session.mode = SessionMode::Exit;
                                        win->running = false;
                                    }
                                    else if (session.return_mode == SessionMode::Menu)
                                    {
                                        reset_to_menu(session, ctx);
                                        ctx_running = true;
                                    }
                                    else
                                    {
                                        session.mode = SessionMode::Editor;
                                        ctx_running = true;
                                    }
                                }
                            }
                            else if (session.return_mode == SessionMode::Menu)
                            {
                                reset_to_menu(session, ctx);
                            }
                            else
                            {
                                session.mode = SessionMode::Editor;
                            }
                            break;
                        }

                        case SessionMode::Exit:
                        default:
                            ctx_running = false;
                            win->running = false;
                            break;
                        }

                        if (!ctx_running)
                        {
#if defined(_WIN32)
                            if (!win->get_should_close())
                            {
                                win->set_should_close(true);
                                post_context_window_close(win);
                            }
                            backend_has_live_context = true;
#else
                            ctx->clear_scene_viewport();
                            ctx->set_scene_preview_mode(core::ScenePreviewMode::None);
                            unload_active_scene(session);
                            session.menu.cleanup();
                            epochengine::gui::cleanup_context(ctx.get());
                            epochengine::cleanup_chat_context(ctx.get());
                            g_preview_look_states.erase(ctx.get());
                            sessions.erase(ctx.get());
#endif
                        }
                        else
                        {
                            backend_has_live_context = true;
                        }
                    }

                    if (backend_has_live_context)
                        any_context_alive = true;
                }

                if (!any_context_alive
#if defined(_WIN32)
                    && !mgr.ContextReplacementInProgress()
#endif
                    )
                {
                    logger::get(startup_mode == SessionMode::Editor ? kEditorLog : kEngineLog).log(
                        logger::LogLevel::WARN,
                        "Context session loop ended because no live rendering context remained.",
                        std::source_location::current());
                    running = false;
                }

                if (cli::smoke_requested
                    && cli::capture_requested
                    && !smoke_capture_taken && frame_count >= smoke_capture_frame)
                {
#if defined(_WIN32)
                    if (!smoke_capture_armed)
                    {
                        prepare_parent_window_for_engine_capture(mgr);
                        smoke_capture_armed = true;
                    }

                    force_parent_window_capture_layout(mgr);
                    const bool layoutReady = parent_window_capture_layout_ready(mgr);
                    const bool readyToCapture =
                        frame_count >= (smoke_capture_frame + smoke_capture_settle_frames)
                        && layoutReady;
                    const bool fallbackCapture =
                        frame_count >= (smoke_capture_frame + smoke_capture_fallback_frames);
                    if (readyToCapture || fallbackCapture)
                    {
                        if (!layoutReady)
                        {
                            logger::get(startup_mode == SessionMode::Editor ? kEditorLog : kEngineLog).log(
                                logger::LogLevel::WARN,
                                "Parented capture layout did not fully settle before proof capture; writing the best available fullscreen parent frame.",
                                std::source_location::current());
                        }

                        capture_parent_window_if_requested(
                            mgr,
                            startup_mode == SessionMode::Editor ? kEditorLog : kEngineLog);
                        smoke_capture_taken = true;
                        if (epochengine::core::cli::smoke_context_switch_requested
                            && smoke_context_switch_posted)
                        {
                            logger::get(startup_mode == SessionMode::Editor ? kEditorLog : kEngineLog).log(
                                logger::LogLevel::INFO,
                                "Smoke context switch proof captured; ending bounded switch smoke.",
                                std::source_location::current());
                            running = false;
                        }
                    }
#endif
                }

                if (epochengine::core::cli::smoke_context_switch_requested
                    && smoke_context_switch_posted
                    && !cli::capture_requested
                    && smoke_context_switch_exit_frame > 0
                    && frame_count >= smoke_context_switch_exit_frame)
                {
                    logger::get(startup_mode == SessionMode::Editor ? kEditorLog : kEngineLog).log(
                        logger::LogLevel::INFO,
                        "Smoke context switch settled; ending bounded switch smoke.",
                        std::source_location::current());
                    running = false;
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(16));
            }

            for (auto& [_, session] : sessions)
            {
                unload_active_scene(session);
                session.menu.cleanup();
            }
            g_preview_look_states.clear();

            mgr.StopAll();

            auto snapshot2 = collect_backend_contexts_shared();
            for (auto& [type, contexts] : snapshot2)
            {
                for (auto& ctx : contexts)
                    cleanup_backend_context_shared(type, ctx);
            }

            epochengine::shutdown_chat_system();

            return 0;
        }

        template <typename PumpFunc>
        int RunEngineMainLoopCommon(MultiContextManager& mgr, PumpFunc&& pump_events)
        {
            const auto startup_mode = SessionMode::Menu;
            return RunContextSessionLoop(mgr, std::forward<PumpFunc>(pump_events), startup_mode);
        }

#if defined(_WIN32)
        [[nodiscard]] bool pump_windows_messages_for_frame() noexcept
        {
            // A backend-owned child can continuously generate paint, pointer,
            // and layout messages. Draining until the queue is empty can
            // starve the editor transaction forever in optimized builds.
            constexpr std::size_t kMaxMessagesPerFrame = 256;
            MSG msg{};

            for (std::size_t handled = 0; handled < kMaxMessagesPerFrame; ++handled)
            {
                if (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE) == FALSE)
                    break;

                if (msg.message == WM_QUIT)
                    return false;

                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }

            input::poll_input();
            return true;
        }

        int RunEngineMainLoopInternal(HINSTANCE hInstance, int nCmdShow)
        {
            UNREFERENCED_PARAMETER(nCmdShow);

            try
            {
                epochengine::core::MultiContextManager mgr;

                HINSTANCE hi = hInstance ? hInstance : GetModuleHandleW(nullptr);

                apply_post_update_startup_cooldown(engine::kEngineLog);
                apply_fullscreen_capture_window_defaults();
                const auto launch_cfg = resolve_legacy_launch_config();

                const bool ok = mgr.Initialize(
                    hi,
                    launch_cfg.raylib_count,
                    launch_cfg.sdl_count,
                    launch_cfg.sfml_count,
                    launch_cfg.vulkan_count,
                    launch_cfg.opengl_count,
                    launch_cfg.directx_count,
                    launch_cfg.software_count,
                    launch_cfg.parented
                );

                if (!ok)
                {
                    //MessageBoxW(nullptr, L"Failed to initialize contexts!", L"Error", MB_ICONERROR | MB_OK);
                    return -1;
                }

                input::designate_polling_thread_to_current();

                mgr.ArrangeDockedWindowsGrid();
                mgr.StartRenderThreads();

#if defined(_WIN32)
                prepare_parent_window_for_engine_capture(mgr);
#endif

                auto pump = []() -> bool
                    {
                        return pump_windows_messages_for_frame();
                    };

                return engine::RunEngineMainLoopCommon(mgr, pump);
            }
            catch (const std::exception& ex)
            {
                MessageBoxA(nullptr, ex.what(), "Error", MB_ICONERROR | MB_OK);
                return -1;
            }
        }
#elif defined(__linux__)
        int RunEngineMainLoopLinux()
        {
            try
            {
                epochengine::core::MultiContextManager mgr;

                apply_post_update_startup_cooldown(engine::kEngineLog);
                const auto launch_cfg = resolve_legacy_launch_config();

                const bool ok = mgr.Initialize(
                    nullptr,
                    launch_cfg.raylib_count,
                    launch_cfg.sdl_count,
                    launch_cfg.sfml_count,
                    launch_cfg.vulkan_count,
                    launch_cfg.opengl_count,
                    launch_cfg.directx_count,
                    launch_cfg.software_count,
                    launch_cfg.parented
                );

                if (!ok)
                {
                    logger::get(kEngineLog).log(
                        logger::LogLevel::Error,
                        "Failed to initialize contexts!",
                        std::source_location::current());
                    return -1;
                }

                input::designate_polling_thread_to_current();

                mgr.ArrangeDockedWindowsGrid();
                mgr.StartRenderThreads();

                if (epochengine::core::cli::smoke_requested)
                {
                    std::this_thread::sleep_for(smoke_shutdown_delay());
                    mgr.StopAll();
                    return 0;
                }

                auto pump = []() -> bool
                    {
                        return epochengine::platform::pump_events();
                    };

                return RunEngineMainLoopCommon(mgr, pump);
            }
            catch (const std::exception& ex)
            {
                logger::get(kEngineLog).log(
                    logger::LogLevel::Error,
                    ex.what(),
                    std::source_location::current());
                return -1;
            }
        }
#endif
    } // anonymous namespace

    void ParseCommandLine(int argc, char** argv)
    {
        (void)cli::parse(argc, argv);
    }

    void RunEngine()
    {
#if defined(_WIN32)
        const HINSTANCE instance = GetModuleHandleW(nullptr);
        const int result = engine::RunEngineMainLoopInternal(instance, SW_SHOWNORMAL);
        if (result != 0)
            logger::get(engine::kEngineLog).logf(
                logger::LogLevel::Error,
                std::source_location::current(),
                "RunEngine terminated with code {}",
                result);
#elif defined(__linux__)
        const int result = engine::RunEngineMainLoopLinux();
        if (result != 0)
            logger::get(engine::kEngineLog).logf(
                logger::LogLevel::Error,
                std::source_location::current(),
                "RunEngine terminated with code {}",
                result);
#else
        epochengine::logger::get(epochengine::core::engine::kEngineLog).log(
            epochengine::logger::LogLevel::Error,
            "RunEngine is not implemented for this platform yet.",
            std::source_location::current());
#endif
    }

    void StartEngine()
    {
        logger::get(engine::kEngineLog).logf(
            logger::LogLevel::INFO,
            std::source_location::current(),
            "epochengine Engine v{}",
            epochengine::GetEngineVersion());
        RunEngine();
    }

    void RunEditorInterface()
    {
#if defined(_WIN32)
        try
        {
            epochengine::core::MultiContextManager mgr;

            const HINSTANCE hi = GetModuleHandleW(nullptr);

            apply_post_update_startup_cooldown(engine::kEditorLog);
            apply_fullscreen_capture_window_defaults();
            const auto launch_cfg = resolve_legacy_launch_config();

                const bool ok = mgr.Initialize(
                    hi,
                    launch_cfg.raylib_count,
                    launch_cfg.sdl_count,
                    launch_cfg.sfml_count,
                    launch_cfg.vulkan_count,
                    launch_cfg.opengl_count,
                    launch_cfg.directx_count,
                    launch_cfg.software_count,
                    launch_cfg.parented
                );

            if (!ok)
            {
                logger::get(engine::kEditorLog).log(
                    logger::LogLevel::Error,
                    "Failed to initialize contexts!",
                    std::source_location::current());
                return;
            }

            input::designate_polling_thread_to_current();

            mgr.ArrangeDockedWindowsGrid();
            mgr.StartRenderThreads();

#if defined(_WIN32)
            prepare_parent_window_for_engine_capture(mgr);
#endif

            auto pump = []() -> bool
                {
                    return engine::pump_windows_messages_for_frame();
                };

            const auto initial_mode =
                epochengine::core::cli::editor_requested
                ? engine::SessionMode::Editor
                : engine::SessionMode::Menu;
            const int result = engine::RunContextSessionLoop(mgr, pump, initial_mode);
            if (result != 0)
                logger::get(engine::kEditorLog).logf(
                    logger::LogLevel::Error,
                    std::source_location::current(),
                    "RunEditorInterface terminated with code {}",
                    result);
        }
        catch (const std::exception& ex)
        {
            MessageBoxA(nullptr, ex.what(), "Error", MB_ICONERROR | MB_OK);
        }
#elif defined(__linux__)
        try
        {
            epochengine::core::MultiContextManager mgr;

            apply_post_update_startup_cooldown(engine::kEditorLog);
            const auto launch_cfg = resolve_legacy_launch_config();

                const bool ok = mgr.Initialize(
                    nullptr,
                    launch_cfg.raylib_count,
                    launch_cfg.sdl_count,
                    launch_cfg.sfml_count,
                    launch_cfg.vulkan_count,
                    launch_cfg.opengl_count,
                    launch_cfg.directx_count,
                    launch_cfg.software_count,
                    launch_cfg.parented
                );

            if (!ok)
            {
                logger::get(engine::kEditorLog).log(
                    logger::LogLevel::Error,
                    "Failed to initialize contexts!",
                    std::source_location::current());
                return;
            }

            input::designate_polling_thread_to_current();

            mgr.ArrangeDockedWindowsGrid();
            mgr.StartRenderThreads();

            auto pump = []() -> bool
                {
                    return epochengine::platform::pump_events();
                };

            const auto initial_mode =
                epochengine::core::cli::editor_requested
                ? engine::SessionMode::Editor
                : engine::SessionMode::Menu;
            const int result = engine::RunContextSessionLoop(mgr, pump, initial_mode);
            if (result != 0)
                logger::get(engine::kEditorLog).logf(
                    logger::LogLevel::Error,
                    std::source_location::current(),
                    "RunEditorInterface terminated with code {}",
                    result);
        }
        catch (const std::exception& ex)
        {
            logger::get(engine::kEditorLog).log(
                logger::LogLevel::Error,
                ex.what(),
                std::source_location::current());
        }
#else
        logger::get(engine::kEditorLog).log(
            logger::LogLevel::Error,
            "RunEditorInterface is not implemented for this platform yet.",
            std::source_location::current());
#endif
    }

    namespace legacy_runtime
    {
        int run(bool editor_mode)
        {
            if (editor_mode)
            {
                RunEditorInterface();
                return 0;
            }

#if defined(_WIN32)
            const HINSTANCE instance = GetModuleHandleW(nullptr);
            return engine::RunEngineMainLoopInternal(instance, SW_SHOWNORMAL);
#elif defined(__linux__)
            return engine::RunEngineMainLoopLinux();
#else
            logger::get(engine::kEngineLog).log(
                logger::LogLevel::Error,
                "Legacy runtime is not implemented for this platform yet.",
                std::source_location::current());
            return -1;
#endif
        }
    }
} // namespace epochengine::core
namespace epochengine::project
{
    ArtifactAcceptanceReport VerifyArtifacts(
        const ArtifactAcceptanceRequest& request) noexcept
    {
        namespace fs = std::filesystem;
        ArtifactAcceptanceReport report{};

        const auto artifact_bit = [](ArtifactKind kind) noexcept
        {
            return static_cast<std::uint32_t>(kind);
        };
        const auto require = [&](ArtifactKind kind, std::string_view path)
        {
            if (!path.empty())
                report.required_mask |= artifact_bit(kind);
        };
        const auto fail = [&](std::string stage, std::string diagnostic)
        {
            report.stage = std::move(stage);
            report.diagnostic = std::move(diagnostic);
            return report;
        };
        const auto accept = [&](ArtifactKind kind)
        {
            report.verified_mask |= artifact_bit(kind);
        };

        require(ArtifactKind::scene, request.scene_path);
        require(ArtifactKind::tilemap, request.tilemap_path);
        require(ArtifactKind::input_profile, request.input_profile_path);
        require(ArtifactKind::sprite_animation, request.sprite_animation_path);
        require(ArtifactKind::audio_profile, request.audio_profile_path);
        require(ArtifactKind::gui, request.gui_path);

        if (request.project_id.empty() || request.project_root.empty())
            return fail("request", "project identity and root are required");
        if (report.required_mask == 0u)
            return fail("request", "no project artifacts were declared");

        try
        {
            std::error_code error{};
            fs::path root = fs::canonical(
                fs::path{request.project_root}, error);
            if (error || !fs::is_directory(root, error) || error)
                return fail("project_root", "project root is not an accessible directory");

            if (!request.scene_path.empty())
            {
                fs::path scenePath{request.scene_path};
                if (scenePath.is_relative())
                    scenePath = root / scenePath;
                scenePath = fs::canonical(scenePath, error);
                if (error)
                    return fail("scene", "scene path could not be resolved");

                const fs::path relative = scenePath.lexically_relative(root);
                if (relative.empty() || relative == fs::path{"."}
                    || *relative.begin() == fs::path{".."})
                {
                    return fail("scene", "scene path is outside the project root");
                }

                const auto loaded =
                    epochengine::scene::persistence::load_scene_snapshot(
                        scenePath);
                if (!loaded.result)
                {
                    return fail(
                        "scene",
                        std::string{"scene load "}
                            + std::string{
                                epochengine::scene::persistence::
                                    scene_persistence_status_name(
                                        loaded.result.status)}
                            + ": " + loaded.result.error);
                }
                if (loaded.snapshot.project_id != request.project_id)
                    return fail("scene", "scene project identity does not match the request");

                epochengine::scene_runtime::SceneRuntime runtime{};
                const auto compiled = runtime.replace(loaded.snapshot);
                if (!compiled.committed())
                    return fail("scene", "scene source is not runnable");
                accept(ArtifactKind::scene);
            }

            if (!request.tilemap_path.empty())
            {
                epochengine::project_tilemap_runtime::ProjectTileMapRuntime
                    sourceRuntime{std::string{request.project_id}, root.string()};
                auto prepared = sourceRuntime.prepare({
                    .logical_path = std::string{request.tilemap_path},
                    .source_policy =
                        epochengine::project_tilemap_runtime::SourcePolicy::
                            prefer_source});
                if (!prepared)
                {
                    return fail(
                        "tilemap",
                        std::string{
                            epochengine::project_tilemap_runtime::
                                runtime_code_name(prepared.code)}
                            + ": " + prepared.diagnostic);
                }

                epochengine::project_tilemap_runtime::ProjectTileMapRuntime
                    artifactRuntime{
                        std::string{request.project_id}, root.string()};
                auto restored = artifactRuntime.prepare({
                    .logical_path = std::string{request.tilemap_path},
                    .source_policy =
                        epochengine::project_tilemap_runtime::SourcePolicy::
                            compiled_only});
                if (!restored
                    || restored.provenance
                        != epochengine::project_tilemap_runtime::Provenance::
                            library_restored)
                {
                    return fail(
                        "tilemap_artifact",
                        std::string{
                            epochengine::project_tilemap_runtime::
                                runtime_code_name(restored.code)}
                            + ": " + restored.diagnostic);
                }
                accept(ArtifactKind::tilemap);
            }

            if (!request.input_profile_path.empty())
            {
                const std::string normalized = fs::path{
                    request.input_profile_path}.lexically_normal().generic_string();
                if (normalized
                    != epochengine::project_input::canonical_source_path)
                {
                    return fail(
                        "input_profile",
                        "input profile does not use the canonical logical path");
                }

                epochengine::project_input::ProjectInputProfileStore store{
                    std::string{request.project_id}, root};
                if (!store.valid())
                    return fail("input_profile", "project input store is invalid");

                error.clear();
                const bool sourceExists =
                    fs::exists(store.source_path(), error) && !error;
                if (error)
                    return fail("input_profile", "input source existence check failed");
                if (sourceExists)
                {
                    const auto source = store.load_source();
                    if (!source)
                    {
                        return fail(
                            "input_profile",
                            std::string{"source "}
                                + std::string{
                                    epochengine::project_input::
                                        store_code_name(source.code)});
                    }
                    const auto compiled =
                        epochengine::project_input::compile_profile(
                            request.project_id, source.source);
                    if (!compiled)
                    {
                        return fail(
                            "input_profile",
                            std::string{"compile "}
                                + std::string{
                                    epochengine::project_input::
                                        validation_code_name(compiled.code)});
                    }
                    const auto published =
                        store.publish_artifact(compiled.artifact);
                    if (!published)
                    {
                        return fail(
                            "input_profile_artifact",
                            std::string{"publish "}
                                + std::string{
                                    epochengine::project_input::
                                        store_code_name(published.code)});
                    }
                }

                const auto loaded = store.load_artifact();
                if (!loaded)
                {
                    return fail(
                        "input_profile_artifact",
                        std::string{"load "}
                            + std::string{
                                epochengine::project_input::
                                    store_code_name(loaded.code)});
                }
                const auto validation =
                    epochengine::project_input::validate_compiled_profile(
                        loaded.artifact);
                if (validation
                    != epochengine::project_input::ValidationCode::ready)
                {
                    return fail(
                        "input_profile_artifact",
                        std::string{"validation "}
                            + std::string{
                                epochengine::project_input::
                                    validation_code_name(validation)});
                }
                accept(ArtifactKind::input_profile);
            }

            if (!request.sprite_animation_path.empty())
            {
                auto prepared =
                    epochengine::project_sprite_animation::
                        prepare_project_sprite_animations(
                            request.project_id,
                            root,
                            request.sprite_animation_path,
                            nullptr);
                if (!prepared)
                {
                    return fail(
                        "sprite_animation",
                        std::string{
                            epochengine::project_sprite_animation::
                                preparation_code_name(prepared.code)}
                            + ": " + prepared.diagnostic);
                }

                epochengine::project_sprite_animation::
                    ProjectSpriteAnimationStore store{
                        std::string{request.project_id}, root};
                const auto loaded = store.load_artifact();
                if (!loaded)
                {
                    return fail(
                        "sprite_animation_artifact",
                        std::string{"load "}
                            + std::string{
                                epochengine::project_sprite_animation::
                                    store_code_name(loaded.code)});
                }
                const auto validation =
                    epochengine::project_sprite_animation::validate_artifact(
                        loaded.artifact);
                if (validation
                    != epochengine::project_sprite_animation::
                        ValidationCode::ready)
                {
                    return fail(
                        "sprite_animation_artifact",
                        std::string{"validation "}
                            + std::string{
                                epochengine::project_sprite_animation::
                                    validation_code_name(validation)});
                }
                accept(ArtifactKind::sprite_animation);
            }

            if (!request.audio_profile_path.empty())
            {
                const std::string normalized = fs::path{
                    request.audio_profile_path}.lexically_normal().generic_string();
                if (normalized
                    != epochengine::project_audio::canonical_source_path)
                {
                    return fail(
                        "audio_profile",
                        "audio profile does not use the canonical logical path");
                }

                epochengine::project_audio::ProjectAudioProfileStore store{
                    std::string{request.project_id}, root};
                auto prepared = store.prepare(false);
                if (!prepared)
                {
                    return fail(
                        "audio_profile",
                        std::string{
                            epochengine::project_audio::
                                preparation_code_name(prepared.code)}
                            + ": " + prepared.diagnostic);
                }
                const auto loaded = store.load_artifact(false);
                if (!loaded)
                {
                    return fail(
                        "audio_profile_artifact",
                        std::string{"load "}
                            + std::string{
                                epochengine::project_audio::
                                    store_code_name(loaded.code)});
                }
                accept(ArtifactKind::audio_profile);
            }

            if (!request.gui_path.empty())
            {
                epochengine::project_gui::ArtifactLibrary library{
                    std::string{request.project_id}, root.string()};
                auto loaded = library.load_latest(request.gui_path);
                if (!loaded)
                {
                    return fail(
                        "gui_artifact",
                        std::string{"load "}
                            + std::string{
                                epochengine::project_gui::
                                    library_code_name(loaded.code)});
                }
                epochengine::project_gui_runtime::RuntimeSession runtime{
                    std::move(loaded.artifact)};
                if (!runtime.valid())
                    return fail("gui_runtime", "GUI runtime rejected the artifact");
                const auto frame = runtime.build_frame({1'280.0f, 720.0f});
                if (!frame)
                {
                    return fail(
                        "gui_runtime",
                        std::string{"frame "}
                            + std::string{
                                epochengine::project_gui_runtime::
                                    runtime_code_name(frame.code)});
                }
                accept(ArtifactKind::gui);
            }

            report.succeeded =
                report.required_mask == report.verified_mask;
            report.stage = report.succeeded ? "complete" : "incomplete";
            report.diagnostic = report.succeeded
                ? "all declared project artifacts were accepted"
                : "one or more declared project artifacts were not verified";
            return report;
        }
        catch (const std::exception& exception)
        {
            return fail("exception", exception.what());
        }
        catch (...)
        {
            return fail(
                "exception",
                "project artifact acceptance failed with an unknown exception");
        }
    }
}


#if !defined(EPOCH_MAIN_IN_MAIN_CPP)
namespace urls
{
    const std::string github_base = "https://github.com/";
    const std::string github_raw_base = "https://raw.githubusercontent.com/";

    const std::string owner = "Autodidac/";
    const std::string repo = "EpochEngine";
    const std::string branch = "main/";

    const std::string version_url = epochengine::updater::PROJECT_PACKAGED_VERSION_URL();
    const std::string binary_url = epochengine::updater::PROJECT_BINARY_URL();
    const std::string source_url = epochengine::updater::PROJECT_SOURCE_URL();
    const std::string source_version_url = epochengine::updater::PROJECT_SOURCE_VERSION_URL();
}

#if defined(_WIN32)
namespace
{
    void configure_unattended_windows_error_mode()
    {
        ::SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
        epochengine::core::engine::install_windows_crash_breadcrumbs();

#if defined(_DEBUG)
        if (::IsDebuggerPresent() == FALSE)
        {
            _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
            _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
            _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
            _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
            _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
            _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
            _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
        }
#endif
    }
}
#endif

#if defined(_WIN32) && defined(EPOCH_USING_WINMAIN)
int WINAPI wWinMain(
    _In_     HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_     LPWSTR    lpCmdLine,
    _In_     int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    UNREFERENCED_PARAMETER(nCmdShow);

    configure_unattended_windows_error_mode();

#if defined(_DEBUG)
    epochengine::core::ShowConsole();
#endif

    try
    {
        const int argc = __argc;
        char** argv = __argv;

        const auto cli_result = epochengine::core::cli::parse(argc, argv);

        if (cli_result.version_requested && !cli_result.update_requested)
            return 0;

        if (cli_result.editor_ai_gate_self_test_requested)
            return epochengine::core::run_editor_ai_gate_self_test();

        if (cli_result.editor_project_self_test_requested)
            return epochengine::core::run_editor_project_self_test(cli_result.editor_project_self_test_id);

        if (cli_result.engine_contract_self_test_requested)
            return epochengine::core::run_engine_contract_self_test();

        if (cli_result.engine_validation_self_test_requested)
            return epochengine::core::run_engine_validation_self_test();

        const epochengine::updater::UpdateChannel channel{
            .version_url = urls::version_url,
            .binary_url = urls::binary_url,
            .source_url = urls::source_url,
            .source_version_url = urls::source_version_url,
            .platform_build_status_url = epochengine::updater::PROJECT_ACTION_RUNS_API_URL(),
            .platform_build_job_name = epochengine::updater::PROJECT_UPDATE_BUILD_JOB_NAME(),
        };

        if (cli_result.update_requested)
        {
            const auto update_result =
                epochengine::updater::run_update_command(channel, cli_result.force_update);

            if (update_result.force_required && !cli_result.force_update)
                return 2;

            if (cli_result.force_update
                && update_result.update_available
                && !update_result.update_performed
                && !update_result.source_update_performed)
            {
                return 1;
            }

            return 0;
        }

        if (cli_result.editor_requested)
        {
            epochengine::core::RunEditorInterface();
            return 0;
        }

        return epochengine::core::engine::RunEngineMainLoopInternal(hInstance, SW_SHOWNORMAL);
    }
    catch (const std::exception& ex)
    {
        MessageBoxA(nullptr, ex.what(), "Error", MB_ICONERROR | MB_OK);
        return -1;
    }
}
#endif

int main(int argc, char** argv)
{
#if defined(_WIN32) && defined(EPOCH_USING_WINMAIN)
    return wWinMain(GetModuleHandleW(nullptr), nullptr, GetCommandLineW(), SW_SHOWNORMAL);
#else
    #if defined(_WIN32)
    configure_unattended_windows_error_mode();
    #endif
    try
    {
        const auto cli_result = epochengine::core::cli::parse(argc, argv);

        if (cli_result.version_requested && !cli_result.update_requested)
            return 0;

        if (cli_result.editor_ai_gate_self_test_requested)
            return epochengine::core::run_editor_ai_gate_self_test();

        if (cli_result.editor_project_self_test_requested)
            return epochengine::core::run_editor_project_self_test(cli_result.editor_project_self_test_id);

        if (cli_result.engine_contract_self_test_requested)
            return epochengine::core::run_engine_contract_self_test();

        if (cli_result.engine_validation_self_test_requested)
            return epochengine::core::run_engine_validation_self_test();

        const epochengine::updater::UpdateChannel channel{
            .version_url = urls::version_url,
            .binary_url = urls::binary_url,
            .source_url = urls::source_url,
            .source_version_url = urls::source_version_url,
            .platform_build_status_url = epochengine::updater::PROJECT_ACTION_RUNS_API_URL(),
            .platform_build_job_name = epochengine::updater::PROJECT_UPDATE_BUILD_JOB_NAME(),
        };

        if (cli_result.update_requested)
        {
            const auto update_result =
                epochengine::updater::run_update_command(channel, cli_result.force_update);

            if (update_result.force_required && !cli_result.force_update)
                return 2;

            if (cli_result.force_update
                && update_result.update_available
                && !update_result.update_performed
                && !update_result.source_update_performed)
            {
                return 1;
            }

            return 0;
        }

        if (cli_result.editor_requested)
        {
            epochengine::core::RunEditorInterface();
            return 0;
        }

        epochengine::core::StartEngine();
        return 0;
    }
    catch (const std::exception& ex)
    {
        epochengine::logger::get(epochengine::core::engine::kEngineLog).log(
            epochengine::logger::LogLevel::Error,
            ex.what(),
            std::source_location::current());
        return -1;
    }
#endif
}

#endif // !defined(EPOCH_MAIN_IN_MAIN_CPP)
