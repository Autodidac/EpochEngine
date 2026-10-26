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
// Epoch Context: window/context state, dispatch and input ownership remain scoped.
// epoch.engine_legacy.cpp (module implementation unit; module names remain compatibility-stable)
 //
 // FIXES APPLIED:
 //  - No direct access to core::Context private members (ctx->hwnd).
 //    We only query windows via MultiContextManager APIs.
 //  - Removed non-constant switch case labels for ContextType::Unknown/Noop
 //    because your ContextType in your current modules is not an enum with those
 //    exact enumerators (or they are not visible here). Default handles it.
 //
#include "../../include/core.format_text.hpp"
//#include "pch.h"

#include "../../include/engine.config.hpp"
#include "../../include/epoch.edition.hpp"
#include "core.abi_types.hpp"
#include "../../include/epoch.engine.hpp"
#include "../../include/epoch.runtime_legacy.hpp"
#include "../editor/editor.frame_stall_watch.hpp"

#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  include <dbghelp.h> // Only types; MiniDumpWriteDump is resolved dynamically.
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
#include "../ai/ai.native_runtime.hpp"

// -----------------------------
// Engine/module imports
// -----------------------------
import platform.engine;
import platform.window;
import platform.context;
import platform.runtime;
import platform.capabilities;
import core.error;
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
import core.log;
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
import editor.authoring_history;
import editor.code_workspace;
import editor.hierarchy_adapter;
#if EPOCH_ENABLE_AUTHORING_PLATFORM && EPOCH_ENABLE_TILEMAP_EDITOR
import editor.tilemap_workspace;
#endif
import forest.factory;
import forest.preview_mesh;
import project.forest_library;
import package.registry;
import package.catalog;
import extension.catalog;
import physics.manager;
import render.camera;
import render.lighting;
import render.portal;
import render.ray;
import scene.document;
import scene.interaction;
import scene.persistence;
import scene.runtime;
import scene.surface_alignment;
import scene.tier0;
import terrain.foundation;
import voxel.field;
import voxel.sdf;
import voxel.storage;
import water.system;
import saveload.system;
import scene.snapshot;
import scene.serializer;
import temporal.request;
import timeline.system;
import media.timeline_preview;

import gui.engine;
import font.renderer;
import gui.menu;
import editor.core;
import ai.engine;
import ai.model_install;
import ai.project_profile;
import epoch.build_validation;
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

namespace epochengine::project
{
    [[nodiscard]] static std::string scene_acceptance_diagnostic(
        const scene::SceneSnapshot& snapshot, bool requiresRuntime)
    {
        if (!requiresRuntime)
        {
            if (snapshot.document_kind != "tool")
                return "tool profile requires a tool scene document";
            if (!scene::validate_scene_document(snapshot,
                    scene::SceneDocumentRequirement::authoring))
                return "tool scene document is invalid";
            return {};
        }
        scene_runtime::SceneRuntime runtime{};
        const auto compiled = runtime.replace(snapshot);
        return compiled.committed() ? std::string{}
            : std::string{"scene source is not runnable"};
    }
}

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
        cfg.parented = false;
        // Default Linux startup is one context. An explicit backend request
        // must retain its parsed counts instead of silently opening OpenGL.
        if (!cli::backend_selection_explicit)
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
            // Persist the failing run separately from the success-only 2-run
            // aggregate. The primary path must exist on runtime failure too.
            const auto persist_failure_log = [&]() noexcept
            {
                std::error_code copyError{};
                std::filesystem::copy_file(
                    runLogPath, log_path,
                    std::filesystem::copy_options::overwrite_existing,
                    copyError);
                if (copyError)
                {
                    std::ofstream fallback{log_path, std::ios::trunc};
                    if (fallback)
                        fallback << "failed to copy child self-test output: "
                            << copyError.message() << '\n';
                }
            };
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
                // Carry the child's first causal result into the parent log.
                // An exit code alone sends source repair back into discovery.
                std::ifstream failedOutput{runLogPath, std::ios::binary};
                std::array<char, 2048> diagnosticLine{};
                std::size_t scanned{};
                while (failedOutput.good() && scanned < maximumLogBytes)
                {
                    failedOutput.getline(diagnosticLine.data(), diagnosticLine.size());
                    scanned += static_cast<std::size_t>(failedOutput.gcount());
                    const std::string_view line{diagnosticLine.data()};
                    for (const auto marker : {"artifact_acceptance.diagnostic=",
                            "library_regeneration.diagnostic=", "gameplay_acceptance.diagnostic="})
                    {
                        const auto pos = line.find(marker);
                        if (pos != line.npos)
                        {
                            result.summary += " | ";
                            result.summary.append(line.substr(pos, 1024u));
                            break;
                        }
                    }
                    if (result.summary.size() >= 4096u) break;
                }
                persist_failure_log();
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
                + count_marker("library_regeneration.stage=not_required")
                != requiredRuns
            || count_marker("gameplay_acceptance.stage=complete")
                + count_marker("gameplay_acceptance.stage=not_required")
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

#include "implementation/epoch.contracts_renderer.inl"
#include "implementation/epoch.contracts_engine.inl"
#include "implementation/epoch.contracts_project.inl"
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
        // Bound once before crash handlers or worker threads are installed.
        // Crash breadcrumbs must not fall back into a validated image folder.
        std::wstring candidate_breadcrumb_directory{};
        // Preload DbgHelp before a crash; do not load DLLs from a compromised
        // exception handler or require a project-level DbgHelp.lib link.
        HMODULE crash_dump_library{};
        using WriteMiniDumpFn = decltype(&::MiniDumpWriteDump);
        WriteMiniDumpFn crash_dump_writer{};

        void write_windows_crash_dump_noexcept(EXCEPTION_POINTERS* details) noexcept
        {
            if (!crash_dump_writer || !details) return;
            wchar_t executable[32768]{};
            const DWORD length = ::GetModuleFileNameW(nullptr, executable,
                static_cast<DWORD>(std::size(executable)));
            if (!length || length >= std::size(executable)) return;
            wchar_t* slash = nullptr;
            for (auto* pos = executable; *pos; ++pos)
                if (*pos == L'\\' || *pos == L'/') slash = pos;
            if (!slash) return;
            *(slash + 1) = L'\0';
            const std::wstring folder = std::wstring(executable) + L"logs";
            if (!::CreateDirectoryW(folder.c_str(), nullptr)
                && ::GetLastError() != ERROR_ALREADY_EXISTS) return;
            const std::wstring target = folder + L"\\epoch_editor_crash_last.dmp";
            const HANDLE output = ::CreateFileW(target.c_str(), GENERIC_WRITE, 0,
                nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (output == INVALID_HANDLE_VALUE) return;
            MINIDUMP_EXCEPTION_INFORMATION exception{};
            exception.ThreadId = ::GetCurrentThreadId();
            exception.ExceptionPointers = details;
            exception.ClientPointers = FALSE;
            const BOOL written = crash_dump_writer(::GetCurrentProcess(),
                ::GetCurrentProcessId(), output, MiniDumpNormal, &exception,
                nullptr, nullptr);
            ::CloseHandle(output);
            if (!written) (void)::DeleteFileW(target.c_str());
        }

        LONG WINAPI log_unhandled_windows_exception(EXCEPTION_POINTERS* exception_info) noexcept
        {
            try
            {
                const auto* record = exception_info ? exception_info->ExceptionRecord : nullptr;
                const unsigned long code = record ? record->ExceptionCode : 0ul;
                const auto address = reinterpret_cast<std::uintptr_t>(record ? record->ExceptionAddress : nullptr);
                const auto base = reinterpret_cast<std::uintptr_t>(::GetModuleHandleW(nullptr));
                logger::get(kEditorLog).logf(
                    logger::LogLevel::Error,
                    std::source_location::current(),
                    "Unhandled Windows exception: code=0x{:08X} address=0x{:016X} image_base=0x{:016X} image_rva=0x{:X}; writing logs/epoch_editor_crash_last.dmp",
                    code,
                    static_cast<unsigned long long>(address),
                    static_cast<unsigned long long>(base),
                    static_cast<unsigned long long>(address >= base ? address - base : 0));
            }
            catch (...)
            {
            }

            // Do not rely on the logger to survive arbitrary invalid memory:
            // a dump retains the faulting thread, exception and stack frames.
            write_windows_crash_dump_noexcept(exception_info);
            return EXCEPTION_CONTINUE_SEARCH;
        }

        void install_windows_crash_breadcrumbs() noexcept
        {
            if (!crash_dump_library)
            {
                crash_dump_library = ::LoadLibraryW(L"dbghelp.dll");
                if (crash_dump_library)
                    crash_dump_writer = reinterpret_cast<WriteMiniDumpFn>(
                        ::GetProcAddress(crash_dump_library, "MiniDumpWriteDump"));
            }
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
                if (!candidate_breadcrumb_directory.empty())
                {
                    directory = candidate_breadcrumb_directory;
                    directory += L"\\";
                    return true;
                }
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
            if (route == "application.plant_lab")
                return DetachedPanelRouteMetadata{
                    .title = "Plant Lab",
                    .width = 1280,
                    .height = 800,
                    .open_success = "Plant Lab application window requested.",
                    .open_failure = "Plant Lab application window could not be opened."
                };
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
                        ? "Epoch Global Settings"
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
                    : route == "pane.ai_status" ? "Assistant Status"
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
                    .title = "Assistant",
                    .width = 520,
                    .height = 520,
                    .open_success = "Assistant popout requested.",
                    .open_failure = "Assistant popout request failed."
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

        template <typename ContextSnapshot>
        void wait_for_session_frame(
            epochengine::perf::frame_limiter& limiter, MultiContextManager& mgr,
            const ContextSnapshot& snapshot)
        {
            using namespace epochengine::perf;
            const auto requestedMode = cli::frame_vsync_requested
                ? frame_pacing_mode::vsync
                : cli::frame_limit_fps > 0.0
                ? frame_pacing_mode::target_hz : frame_pacing_mode::uncapped;
            const auto policy = select_frame_pacing_policy(
                requestedMode, cli::frame_limit_fps, cli::frame_limit_explicit,
                !cli::parented_mode && !cli::editor_requested && !cli::run_menu_loop);
            double fastestHz = 0.0;
            bool anyContext = false;
            bool uncapped = false;
            for (const auto& [_, contexts] : snapshot)
            {
                for (const auto& ctx : contexts)
                {
                    auto* window = ctx ? mgr.findWindowByContext(ctx) : nullptr;
                    if (!window || !window->running.load(std::memory_order_acquire)
                        || window->get_should_close())
                        continue;
                    // This thread produces UI/simulation work, not native
                    // presents. It needs its own deadline, never a post-frame
                    // fixed sleep. Shared policy preserves inactive-window caps.
                    const auto plan = with_frame_rate_ceiling(
                        resolve_frame_pacing(
                            with_frame_pacing_override(policy, window->editorFramePacingOverride),
                            ctx->frame_pacing_activity(), false),
                        window->editorThrottleHz.load(std::memory_order_relaxed));
                    anyContext = true;
                    uncapped = uncapped || !(plan.effective_hz > 0.0);
                    fastestHz = (std::max)(fastestHz, plan.effective_hz);
                }
            }
            // Keep event pumping bounded while a replacement context starts.
            const double targetHz = !anyContext ? policy.background_hz : uncapped ? 0.0 : fastestHz;
            const double targetDt = targetHz > 0.0 ? 1.0 / targetHz : 0.0;
            if (limiter.target_dt != targetDt)
                limiter.set_target_fps(targetHz);
            // Do not add a second timer to the explicit frame-limiter policy.
            limiter.wait_for_next_frame();
        }

#include "implementation/epoch.session_editor_launcher.inl"
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

        [[nodiscard]] bool runtime_entity_is_forest_preview(
            const ProjectRuntimeEntity& entity) noexcept
        {
            return entity.type == "ForestTrunk"
                || entity.type == "ForestBranchSegment"
                || entity.type == "ForestBranchJoint"
                || entity.type == "ForestFoliageCluster";
        }

        [[nodiscard]] bool runtime_forest_projection_admitted(
            const ProjectRuntimeEntity& entity)
        {
            if (entity.type == "ForestFoliageCluster")
                return true;
            if (!runtime_entity_is_forest_preview(entity))
                return true;
            return forest::preview_mesh::segment_projection_admitted(
                std::abs(entity.transform.scale[1]),
                std::abs(entity.transform.scale[0]) * 0.5f,
                std::abs(entity.transform.scale[2]) * 0.5f);
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
                if (runtime_entity_is_forest_preview(entity)
                    && !runtime_forest_projection_admitted(entity))
                {
                    continue;
                }

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
            // Preview project identity comes only from its private project
            // admission. An ambient override must not reopen the parent project.
            if (!epochengine::core::path::candidate_data_root().empty())
                return epochengine::editor_resolve_candidate_project_path(
                    std::filesystem::path{profile.root_path});
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
            if (declaredPath.empty() || runtimeRoot.empty())
                return {};
            std::filesystem::path path{declaredPath};
            if (!epochengine::core::path::candidate_data_root().empty())
            {
                const auto declared = path.is_absolute() || (!path.empty() && *path.begin() == "Projects")
                    ? path : runtimeRoot / path;
                return epochengine::editor_resolve_candidate_project_path(declared);
            }
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
        project_modifier_snapshot(const epochengine::core::Context* ctx) noexcept
        {
            namespace project_input = epochengine::project_input;
            project_input::ModifierMask result{};
            if (!ctx)
                return result;
            if (ctx->is_key_held_safe(input::Key::LeftShift)
                || ctx->is_key_held_safe(input::Key::RightShift))
            {
                result |= project_input::modifier_mask(
                    project_input::Modifier::shift);
            }
            if (ctx->is_key_held_safe(input::Key::LeftControl)
                || ctx->is_key_held_safe(input::Key::RightControl))
            {
                result |= project_input::modifier_mask(
                    project_input::Modifier::control);
            }
            if (ctx->is_key_held_safe(input::Key::LeftAlt)
                || ctx->is_key_held_safe(input::Key::RightAlt))
            {
                result |= project_input::modifier_mask(
                    project_input::Modifier::alt);
            }
            if (ctx->is_key_held_safe(input::Key::LeftSuper)
                || ctx->is_key_held_safe(input::Key::RightSuper))
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
            const epochengine::core::Context* ctx,
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

            snapshot.modifiers = project_modifier_snapshot(ctx);
            snapshot.keyboard.reserve(keys.size());
            for (const auto projectKey : keys)
            {
                const auto key = engine_key(projectKey);
                if (!key)
                    continue;
                snapshot.keyboard.push_back({
                    .key = projectKey,
                    .held = ctx && ctx->is_key_held_safe(*key),
                    .pressed = ctx && ctx->is_key_down_safe(*key)
                });
            }
            return snapshot;
        }

#include "implementation/epoch.project_play_scene.inl"
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

#include "implementation/epoch.session_runtime.inl"
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

                const auto sceneDiagnostic = scene_acceptance_diagnostic(
                    loaded.snapshot, request.scene_requires_runtime);
                if (!sceneDiagnostic.empty())
                    return fail("scene", sceneDiagnostic);
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

namespace
{
    [[nodiscard]] bool initialize_candidate_runtime(int& argc, char** argv) noexcept
    {
        try
        {
            const auto arguments = epochengine::core::path::preflight_candidate_data_arguments(argc, argv);
            if (!arguments.ok) return false;
            if (arguments.option_index == 0) return true;
            const auto root = epochengine::core::path::candidate_data_root();
            if (root.empty()) return false;
            const auto logs = root / "logs";
            std::error_code error{};
            (void)std::filesystem::create_directory(logs, error);
            if (error) return false;
            const auto status = std::filesystem::symlink_status(logs, error);
            if (error || !std::filesystem::is_directory(status)
                || std::filesystem::is_symlink(status)) return false;
#if defined(_WIN32)
            const auto attributes = ::GetFileAttributesW(logs.c_str());
            if (attributes == INVALID_FILE_ATTRIBUTES
                || (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0u) return false;
            epochengine::core::engine::candidate_breadcrumb_directory = logs.native();
#endif
            epochengine::logger::LogConfig config{};
            config.root_dir = logs;
            epochengine::logger::init(std::move(config));
            // The host-only startup binding has already been consumed. Keep it
            // out of ordinary CLI parsing and source/model diagnostic packets.
            for (int i = arguments.option_index; i + 2 <= argc; ++i)
                argv[i] = argv[i + 2];
            argc -= 2;
            return true;
        }
        catch (...) { return false; }
    }

    int candidate_startup_failure()
    {
        epochengine::core::log::enable_console(true);
        epochengine::core::log::error("Engine.Startup",
            "Candidate data-root binding failed before editor startup; no fallback runtime was started.");
        return 2;
    }

#if defined(_WIN32)
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
#endif
}

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

    int argc = __argc;
    char** argv = __argv;
    if (!initialize_candidate_runtime(argc, argv)) return candidate_startup_failure();
    configure_unattended_windows_error_mode();

#if defined(_DEBUG)
    epochengine::core::ShowConsole();
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
    if (!initialize_candidate_runtime(argc, argv)) return candidate_startup_failure();
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
