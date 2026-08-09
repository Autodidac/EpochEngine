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
#include <format>
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
//import engine.config;

import epoch.facade;

import epoch.cli;
import epoch.version;
import engine.updater;
import launcher.update;
import input.engine;
import ecs.legacy_components;

import context.multiplexer;
import context.type;
import context.window;
import core.context;
import core.logger;
import core.path;
import core.time;
import core.timer;
import ecs.world;

import audio.manager;
import authoring.document;
import authoring.morphology;
#if EPOCH_ENABLE_AUTHORING_PLATFORM && EPOCH_ENABLE_TEXTURE_EDITOR
import authoring.texture;
#endif
import capability.profile;
import forest.factory;
import package.registry;
import physics.manager;
import render.lighting;
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
import render.device;
import render.device_null;
import render.device_opengl_family;
#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)
import opengl.canvas2d;
import opengl.textures;
#endif
import render.canvas2d;
import render.canvas2d_cpu;
import render.canvas2d_presentation;
import render.texture_residency;
import canvas2d.scene_contracts;
import project.contracts;
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
        std::string_view packet_path,
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
        out << "- Packet: " << (packet_path.empty() ? std::string_view{ "(packet staging failed)" } : packet_path) << "\n";
        out << "- Manifest: " << (manifest_path.empty() ? std::string_view{ "(missing)" } : manifest_path) << "\n";
        out << "- Scene: " << (scene_path.empty() ? std::string_view{ "(missing)" } : scene_path) << "\n";
        out << "- Active script: " << (script_path.empty() ? std::string_view{ "(missing)" } : script_path) << "\n";
        out << "- Build log: " << (build_log_path.empty() ? std::string_view{ "(missing)" } : build_log_path) << "\n";
        out << "- Output: " << (output_path.empty() ? std::string_view{ "(missing)" } : output_path) << "\n";
        out << "- Gate: human review required before source changes, dataset promotion, or eval promotion.\n";
    }

    struct GeneratedProjectSelfTestResult
    {
        bool attempted = false;
        bool succeeded = false;
        std::string log_path{};
        std::string summary = "child self-test skipped";
    };

    [[nodiscard]] inline std::string quote_shell_path(const std::filesystem::path& path)
    {
        return "\"" + path.string() + "\"";
    }

    [[nodiscard]] inline GeneratedProjectSelfTestResult run_generated_project_self_test(
        std::string_view executable_path,
        std::string_view project_root)
    {
        GeneratedProjectSelfTestResult result{};
        if (executable_path.empty() || project_root.empty())
            return result;

        const std::filesystem::path executable{ std::string{ executable_path } };
        const std::filesystem::path log_dir = std::filesystem::path{ std::string{ project_root } } / "logs";
        const std::filesystem::path log_path = log_dir / "project-self-test.log";
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

#if defined(_WIN32)
        SECURITY_ATTRIBUTES securityAttributes{};
        securityAttributes.nLength = sizeof(securityAttributes);
        securityAttributes.bInheritHandle = TRUE;

        HANDLE logFile = CreateFileW(
            log_path.wstring().c_str(),
            GENERIC_WRITE,
            FILE_SHARE_READ,
            &securityAttributes,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
        if (logFile == INVALID_HANDLE_VALUE)
        {
            result.summary = "failed to create child self-test log file: win32=" + std::to_string(GetLastError());
            return result;
        }

        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        startup.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
        startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        startup.hStdOutput = logFile;
        startup.hStdError = logFile;
        startup.wShowWindow = SW_HIDE;

        PROCESS_INFORMATION process{};
        std::wstring commandLine = L"\"" + executable.wstring() + L"\" --project-self-test";
        const std::wstring currentDirectory = executable.parent_path().wstring();

        const BOOL launched = CreateProcessW(
            executable.wstring().c_str(),
            commandLine.data(),
            nullptr,
            nullptr,
            TRUE,
            CREATE_NO_WINDOW,
            nullptr,
            currentDirectory.empty() ? nullptr : currentDirectory.c_str(),
            &startup,
            &process);
        if (!launched)
        {
            const DWORD error = GetLastError();
            CloseHandle(logFile);
            std::ofstream out(log_path, std::ios::app | std::ios::binary);
            if (out)
                out << "CreateProcessW failed: win32=" << error << "\n";
            result.summary = "failed to launch child self-test: win32=" + std::to_string(error);
            return result;
        }

        constexpr DWORD kChildSelfTestTimeoutMs = 30000;
        const DWORD waitResult = WaitForSingleObject(process.hProcess, kChildSelfTestTimeoutMs);
        if (waitResult == WAIT_TIMEOUT)
        {
            TerminateProcess(process.hProcess, 124);
            WaitForSingleObject(process.hProcess, 5000);
            CloseHandle(process.hThread);
            CloseHandle(process.hProcess);
            CloseHandle(logFile);
            std::ofstream out(log_path, std::ios::app | std::ios::binary);
            if (out)
                out << "Child self-test timed out after " << kChildSelfTestTimeoutMs << " ms.\n";
            result.summary = "child self-test timed out";
            return result;
        }

        DWORD exitCode = 1;
        if (!GetExitCodeProcess(process.hProcess, &exitCode))
            exitCode = GetLastError();

        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        CloseHandle(logFile);

        result.succeeded = (exitCode == 0);
        result.summary = result.succeeded
            ? "child self-test passed"
            : "child self-test failed with exit code " + std::to_string(exitCode);
        return result;
#else
        const std::string command =
            quote_shell_path(executable) + " --project-self-test > " + quote_shell_path(log_path) + " 2>&1";
        const int code = std::system(command.c_str());
        result.succeeded = (code == 0);
        result.summary = result.succeeded
            ? "child self-test passed"
            : "child self-test failed with exit code " + std::to_string(code);
        return result;
#endif
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
        return second.width == first.width
            && second.height == first.height
            && second.pixels.size() == first.pixels.size()
            && second.version > first.version
            && second.pixels != first.pixels;
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
            && epochengine::editor_application_for_project("projectlauncher") == &standardApplication
            && epochengine::editor_application_for_project("plantlab") == &plantLabApplication
            && epochengine::editor_application_for_project("twodstudio") == &guiApplication
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
            && !epochengine::editor_application_supports_surface(
                guiApplication, epochengine::EditorApplicationSurface::Scene)
            && epochengine::editor_application_for_project("forestfactory") == &standardApplication);

        auto forestProfile = epochengine::forest::default_profile(epochengine::forest::ForestPreset::Tree);
        forestProfile.temporal.timeSeconds = forestProfile.temporal.durationSeconds;
        const auto forestEstimate = epochengine::forest::estimate_preview_stats(forestProfile);
        const auto forestGeometry = epochengine::forest::build_preview_geometry(forestProfile);
        const auto previewActivation = epochengine::forest::activation_for_editor_preview();
        const auto sceneActivation = epochengine::forest::activation_for_scene_use();
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

        check("ai.mcp.tool_protocol", epochengine::ai::run_mcp_contract());

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
        const auto capabilityContract =
            epochengine::capability::run_contract_checks();
        constexpr std::uint32_t expectedCapabilityChecks = (1u << 17u) - 1u;
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

        const auto arcadeContract = epochengine::render_arcade::run_contract_checks();
        const auto arcadePreviewRouting =
            epochengine::previewgrid::run_arcade_preview_routing_contract();
        check("render.arcade_scene_contract", arcadeContract.passed());
        check(
            "render.arcade_attract_pattern",
            epochengine::render_arcade::arcade_attract_pattern_contract());
        check("render.arcade_preview_routing", arcadePreviewRouting.passed());
        check("render.canvas2d.core", epochengine::canvas2d::canvas2d_runtime_contract());
        const auto cpuCanvasContract =
            epochengine::canvas2d::cpu::canvas2d_cpu_runtime_contract_failure();
        check(
            std::string{"render.canvas2d.cpu."}
                + epochengine::canvas2d::cpu::cpu_contract_failure_name(cpuCanvasContract),
            cpuCanvasContract == epochengine::canvas2d::cpu::CpuContractFailure::none);
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
        const auto projectTextureSpineContract =
            epochengine::project_contracts::run_texture_spine_contract();
        check(
            std::string{"project.texture_spine."}
                + std::string{projectTextureSpineContract.stage},
            projectTextureSpineContract.passed);
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
        epochengine::timeline::sync_to_simulation(timelineState, timeStats);
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
            snapshotText.find("epoch_snapshot 2") != std::string::npos
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
        const auto manifest = epochengine::ai::active_model_manifest();
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

        const bool verifierReady = build.succeeded && childSelfTest.succeeded;
        const std::string normalizedOutput =
            "project=" + projectId +
            "; materialize=" + (ensured.succeeded ? std::string{ "pass" } : std::string{ "fail" }) +
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

        epochengine::ai::IterationPacket packet{};
        packet.packet_name = projectId + "-cli-self-test";
        packet.task_prompt =
            "Review the staged editor project self-test evidence, identify the next safe builder/verifier action, "
            "and do not modify source without an explicit human-approved pass.";
        packet.assistant_hint =
            "Treat compiler output, generated project files, captures, and logs as evidence. "
            "No evidence means no belief; promotion remains human-gated.";
        packet.operator_notes =
            "Generated by --editor-project-self-test so the selected model can reason from the real editor/tool loop instead of a silent build.";
        packet.control_loop_stage = verifierReady
            ? "Verifier staged: materialize, build, and child self-test evidence available"
            : build.succeeded
                ? "Verifier blocked: generated child self-test failed"
            : "Builder blocked: inspect generated project build log";
        packet.review_gate_state = verifierReady
            ? "ready_for_human_review"
            : build.succeeded ? "blocked_child_self_test_failed" : "blocked_build_failed";
        packet.review_gate_evidence = normalizedOutput;
        packet.project_id = projectId;
        packet.project_name = profile == nullptr ? projectId : std::string{ profile->display_name };
        packet.scene_id = profile == nullptr ? std::string{} : std::string{ profile->runtime_scene_id };
        packet.project_root = ensured.root_path;
        packet.scene_path = profile == nullptr ? std::string{} : std::string{ profile->scene_path };
        packet.active_script = ensured.default_script_path;
        packet.build_log_path = build.log_path;
        packet.output_path = build.output_path;
        packet.provider_summary = epochengine::ai::active_provider_summary();
        packet.active_model = epochengine::ai::active_model_name();
        packet.manifest_path = manifest.manifest_path;
        packet.workspace_root = evidencePathsConfig.workspace_root;
        packet.model_exchange_path = evidencePathsConfig.model_exchange_jsonl;
        packet.tool_trace_path = evidencePathsConfig.tool_trace_jsonl;
        packet.session_root = evidencePathsConfig.session_root;
        packet.model_root = evidencePathsConfig.model_root;
        packet.cache_root = evidencePathsConfig.cache_root;
        packet.curated_dataset_root = evidencePathsConfig.curated_dataset_root;
        packet.eval_root = evidencePathsConfig.eval_root;
        packet.evidence_paths = std::move(evidencePaths);

        const std::string packetPath = epochengine::ai::stage_iteration_packet(packet);
        append_editor_project_self_test_note(
            ensured.root_path,
            build.succeeded ? "CLI Self-Iteration Self-Test Completed" : "CLI Self-Iteration Self-Test Blocked",
            verifierReady
                ? "Materialize, child build, and child self-test evidence staged."
                : build.succeeded ? childSelfTest.summary : "Materialize succeeded but child build failed; inspect the build log.",
            packetPath,
            epochengine::ai::active_model_name(),
            ensured.manifest_path,
            profile == nullptr ? std::string_view{} : profile->scene_path,
            ensured.default_script_path,
            build.log_path,
            build.output_path);
        log_editor_self_test_line("editor_project_self_test.mcp_capture=" + evidencePathsConfig.tool_trace_jsonl);
        log_editor_self_test_line("editor_project_self_test.packet=" + (packetPath.empty() ? std::string{ "fail" } : packetPath));
        if (packetPath.empty())
            return build.succeeded ? 4 : 3;

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
        return root / std::format("{}-parented-grid.bmp", cli::capture_output_stem());
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
            if (route == "pane.outliner")
            {
                return DetachedPanelRouteMetadata{
                    .title = "Epoch World Outliner",
                    .width = 430,
                    .height = 620,
                    .open_success = "World Outliner popout requested.",
                    .open_failure = "World Outliner popout request failed."
                };
            }
            if (route == "pane.inspector")
            {
                return DetachedPanelRouteMetadata{
                    .title = "Epoch Inspector",
                    .width = 460,
                    .height = 620,
                    .open_success = "Inspector popout requested.",
                    .open_failure = "Inspector popout request failed."
                };
            }
            if (route == "pane.console")
            {
                return DetachedPanelRouteMetadata{
                    .title = "Epoch Console Dock",
                    .width = 760,
                    .height = 420,
                    .open_success = "Console Dock popout requested.",
                    .open_failure = "Console Dock popout request failed."
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
            std::optional<std::string> pending_editor_project_id{};
            EditorApplicationKind pending_editor_application{ EditorApplicationKind::Standard };
            std::uint32_t launcher_loading_frames{ 0 };
            std::uint32_t launcher_loading_total_frames{ 0 };
        };

        struct PreviewLookState
        {
            gui::Vec2 last_mouse{};
            bool looking = false;
            bool panning = false;
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

#if defined(_WIN32)
            STARTUPINFOW startup{};
            startup.cb = sizeof(startup);
            startup.dwFlags = STARTF_USESHOWWINDOW;
            startup.wShowWindow = SW_SHOWNORMAL;

            PROCESS_INFORMATION process{};
            std::wstring command_line = L"\"" + executable.wstring() + L"\"";
            command_line += L" --standalone --window-mode standalone";
            if (!scene_to_launch.empty())
            {
                const std::wstring scene_wide{ scene_to_launch.begin(), scene_to_launch.end() };
                command_line += L" --scene \"" + scene_wide + L"\"";
            }
            if (!backend_argument.empty())
            {
                const std::wstring backend_wide{ backend_argument.begin(), backend_argument.end() };
                command_line += L" --backend \"" + backend_wide + L"\"";
            }
            if (!frame_limit_argument.empty())
            {
                const std::wstring frame_limit_wide{ frame_limit_argument.begin(), frame_limit_argument.end() };
                command_line += L" --frame-limit \"" + frame_limit_wide + L"\"";
            }
            std::wstring working_directory = executable.parent_path().wstring();
            const BOOL created = CreateProcessW(
                executable.wstring().c_str(),
                command_line.data(),
                nullptr,
                nullptr,
                FALSE,
                0,
                nullptr,
                working_directory.empty() ? nullptr : working_directory.c_str(),
                &startup,
                &process);

            if (!created)
            {
                logger::get(kEditorLog).logf(
                    logger::LogLevel::Error,
                    std::source_location::current(),
                    "Failed to launch built project executable: {} (GetLastError={})",
                    executable.generic_string(),
                    static_cast<unsigned long>(GetLastError()));
                return false;
            }

            logger::get(kEditorLog).logf(
                logger::LogLevel::INFO,
                std::source_location::current(),
                "Launched built project executable in standalone {} mode: {}",
                backend_argument,
                executable.generic_string());

            CloseHandle(process.hThread);
            CloseHandle(process.hProcess);
            return true;
#else
            std::string command = "\"" + executable.string() + "\" --standalone --window-mode standalone";
            if (!scene_to_launch.empty())
                command += " --scene \"" + scene_to_launch + "\"";
            if (!backend_argument.empty())
                command += " --backend \"" + backend_argument + "\"";
            if (!frame_limit_argument.empty())
                command += " --frame-limit \"" + frame_limit_argument + "\"";
            command += " &";
            if (std::system(command.c_str()) != 0)
            {
                logger::get(kEditorLog).logf(
                    logger::LogLevel::Error,
                    std::source_location::current(),
                    "Failed to launch built project executable: {}",
                    executable.generic_string());
                return false;
            }
            logger::get(kEditorLog).logf(
                logger::LogLevel::INFO,
                std::source_location::current(),
                "Launched built project executable in standalone {} mode: {}",
                backend_argument,
                executable.generic_string());
            return true;
#endif
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
            for (std::size_t i = 0; i < entities.size(); ++i)
            {
                const auto& entity = entities[i];
                if (!entity.visible())
                    continue;

                markers.push_back(epochengine::previewgrid::ObjectMarker{
                    .position{ entity.transform.position[0], entity.transform.position[1], entity.transform.position[2] },
                    .color = runtime_marker_color_for_entity(entity, i == 0u),
                    .scale{ entity.transform.scale[0], entity.transform.scale[1], entity.transform.scale[2] },
                    .radius = runtime_marker_radius_for_entity(entity),
                    .primitive = runtime_preview_primitive_for_entity(entity),
                    .selected = i == 0u,
                    .editorOnly = entity.editor_only() || entity.category == "Editor",
                    .sampledRenderSurface = entity.category == "EngineArcade" && entity.name == "EngineArcadeScreen"
                });
            }

            epochengine::previewgrid::set_object_markers(ctx, std::span<const epochengine::previewgrid::ObjectMarker>{
                markers.data(),
                markers.size()
            });
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
                m_scenePath = std::string(profile->scene_path);
                m_worldName = std::string(profile->world_name);
                m_scriptName = std::string(profile->default_script);
                m_description = std::string(profile->description);
                m_modelSummary = epochengine::editor_project_model_summary(m_projectId);
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
                        .include_editor_only_objects = true
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
            }

            bool frame(std::shared_ptr<epochengine::core::Context> ctx, epochengine::core::WindowData*) override
            {
                if (!ctx)
                    return false;

                if (input::action_pressed(input::Action::Cancel))
                {
                    epochengine::previewgrid::clear_object_markers(ctx.get());
                    epochengine::previewgrid::clear_lighting_frame(ctx.get());
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

                const int width = (std::max)(1, ctx->width > 0 ? ctx->width : ctx->get_width_safe());
                const int height = (std::max)(1, ctx->height > 0 ? ctx->height : ctx->get_height_safe());
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
                publish_project_play_markers(ctx.get(), std::span<const ProjectRuntimeEntity>{
                    m_entities.data(),
                    m_entities.size()
                });
                epochengine::previewgrid::set_lighting_frame(ctx.get(), m_lightingFrame);

                gui::begin_frame(ctx, dt, mouse_pos, mouse_left_down);

                const int wheelDelta = epochengine::gui::consume_mouse_wheel_delta();
                if (input::action_pressed(input::Action::ResetCamera))
                    epochengine::previewgrid::reset_camera(ctx.get());

                const float forwardInput =
                    (input::action_held(input::Action::MoveForward) ? 1.0f : 0.0f)
                    - (input::action_held(input::Action::MoveBackward) ? 1.0f : 0.0f);
                const float rightInput =
                    (input::action_held(input::Action::MoveRight) ? 1.0f : 0.0f)
                    - (input::action_held(input::Action::MoveLeft) ? 1.0f : 0.0f);
                const float upInput =
                    (input::action_held(input::Action::MoveUp) ? 1.0f : 0.0f)
                    - (input::action_held(input::Action::MoveDown) ? 1.0f : 0.0f);
                const float yawInput =
                    (input::action_held(input::Action::LookRight) ? 1.0f : 0.0f)
                    - (input::action_held(input::Action::LookLeft) ? 1.0f : 0.0f);
                const float pitchInput =
                    (input::action_held(input::Action::LookUp) ? 1.0f : 0.0f)
                    - (input::action_held(input::Action::LookDown) ? 1.0f : 0.0f);

                if (mouse_right_down && m_lookState.looking)
                {
                    const float mouseDeltaX = mouse_pos.x - m_lookState.last_mouse.x;
                    const float mouseDeltaY = mouse_pos.y - m_lookState.last_mouse.y;
                    const float sensitivity = input::mouse_look_sensitivity();
                    epochengine::previewgrid::look_camera(
                        ctx.get(),
                        mouseDeltaX * sensitivity,
                        -mouseDeltaY * sensitivity);
                }
                else if (mouse_left_down && !mouse_right_down && m_lookState.panning)
                {
                    const float mouseDeltaX = mouse_pos.x - m_lookState.last_mouse.x;
                    const float mouseDeltaY = mouse_pos.y - m_lookState.last_mouse.y;
                    epochengine::previewgrid::pan_camera_drag(
                        ctx.get(),
                        mouseDeltaX,
                        -mouseDeltaY);
                }

                if (wheelDelta != 0)
                {
                    epochengine::previewgrid::zoom_camera(
                        ctx.get(),
                        (static_cast<float>(wheelDelta) / 120.0f) * input::wheel_zoom_step());
                }

                epochengine::previewgrid::step_camera(
                    ctx.get(),
                    dt,
                    forwardInput,
                    rightInput,
                    upInput,
                    yawInput,
                    pitchInput);

                m_lookState.last_mouse = mouse_pos;
                m_lookState.looking = mouse_right_down;
                m_lookState.panning = mouse_left_down && !mouse_right_down;

                gui::begin_top_layer();
                gui::begin_window("Project Runtime Preview", { 24.0f, 24.0f }, { 430.0f, 210.0f });
                gui::label(std::string("Project: ") + m_projectName);
                gui::label(std::string("World: ") + m_worldName);
                gui::label(std::string("Scene: ") + m_scenePath);
                gui::label(std::string("Script: ") + m_scriptName);
                gui::label(std::string("Preview Objects: ") + std::to_string(visible_runtime_entity_count(m_entities)));
                gui::wrapped_label(
                    std::string("Demo model: ")
                    + (m_modelSummary.asset_path.empty() ? std::string("(none)") : m_modelSummary.asset_path),
                    390.0f);
                gui::wrapped_label(
                    std::string("Model summary: ")
                    + (m_modelSummary.summary.empty() ? std::string("(unavailable)") : m_modelSummary.summary),
                    390.0f);
                gui::wrapped_label(m_description, 390.0f);
                gui::wrapped_label("Esc returns to the editor. WASD/QE move, arrows look, and Home resets the active project camera through the shared input profile.", 390.0f);
                gui::end_window();
                gui::end_top_layer();

                gui::end_frame();
                ctx->present_safe();
                return true;
            }

        private:
            std::string m_projectId{};
            std::string m_projectName{};
            std::string m_scenePath{};
            std::string m_worldName{};
            std::string m_scriptName{};
            std::string m_description{};
            epochengine::EditorProjectModelSummary m_modelSummary{};
            epochengine::scene_runtime::SceneRuntime m_sceneRuntime{};
            std::vector<ProjectRuntimeEntity> m_entities{};
            epochengine::lighting::LightingFrame m_lightingFrame{};
            timing::Clock::time_point m_lastFrame{};
            bool m_hasLastFrame{ false };
            epochengine::previewgrid::CameraMode m_cameraMode{ epochengine::previewgrid::CameraMode::Editor };
            input::ProfilePreset m_inputProfile{ input::ProfilePreset::EditorDefault };
            std::unordered_map<const void*, bool> m_cameraApplied{};
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

        [[nodiscard]] std::string_view project_id_from_choice(epochengine::menu::Choice choice) noexcept
        {
            using Choice = epochengine::menu::Choice;

            switch (choice)
            {
            case Choice::OpenEditor: return "projectlauncher";
            case Choice::OpenPlantLab: return "plantlab";
            case Choice::OpenGuiEditor: return "twodstudio";
            default: return {};
            }
        }

        [[nodiscard]] EditorApplicationKind application_from_choice(
            epochengine::menu::Choice choice) noexcept
        {
            using Choice = epochengine::menu::Choice;
            switch (choice)
            {
            case Choice::OpenPlantLab: return EditorApplicationKind::PlantLab;
            case Choice::OpenGuiEditor: return EditorApplicationKind::GuiEditor;
            case Choice::OpenEditor:
            default: return EditorApplicationKind::Standard;
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
            std::unordered_map<Context*, ContextSession> sessions;
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
                                .start_docked = true,
                                .pinned_to_parent = true
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

                    epochengine::editor_suppress_startup_update_check(targetCtx);
                    epochengine::editor_load_application(targetCtx, application);

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
                                epochengine::editor_notify_context_panel_closed(win->guiRoute);
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
                            if (startup_scene_name.empty())
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

                            ctx->set_scene_preview_mode(core::ScenePreviewMode::Editor);
                            clear_before_ui_frame(ctx);
                            gui::begin_frame(ctx, dt, mouse_pos, mouse_left_down);
                            const auto editor_frame = epochengine::editor_run(ctx);

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

                            if (ctx->scene_preview_mode() == core::ScenePreviewMode::Editor
                                && viewport.size.x > 1.0f
                                && viewport.size.y > 1.0f
                                && mouse_in_scene)
                            {
                                auto& look_state = g_preview_look_states[ctx.get()];
                                if (editor_frame.scene_input_captured)
                                {
                                    look_state.last_mouse = mouse_pos;
                                    look_state.looking = false;
                                    look_state.panning = false;
                                }
                                else
                                {
                                    const int wheelDelta = epochengine::gui::consume_mouse_wheel_delta();
                                    const float forwardInput =
                                        (epochengine::input::action_held(epochengine::input::Action::MoveForward) ? 1.0f : 0.0f)
                                        - (epochengine::input::action_held(epochengine::input::Action::MoveBackward) ? 1.0f : 0.0f);
                                    const float rightInput =
                                        (epochengine::input::action_held(epochengine::input::Action::MoveRight) ? 1.0f : 0.0f)
                                        - (epochengine::input::action_held(epochengine::input::Action::MoveLeft) ? 1.0f : 0.0f);
                                    const float upInput =
                                        (epochengine::input::action_held(epochengine::input::Action::MoveUp) ? 1.0f : 0.0f)
                                        - (epochengine::input::action_held(epochengine::input::Action::MoveDown) ? 1.0f : 0.0f);
                                    const float yawInput =
                                        (epochengine::input::action_held(epochengine::input::Action::LookRight) ? 1.0f : 0.0f)
                                        - (epochengine::input::action_held(epochengine::input::Action::LookLeft) ? 1.0f : 0.0f);
                                    const float pitchInput =
                                        (epochengine::input::action_held(epochengine::input::Action::LookUp) ? 1.0f : 0.0f)
                                        - (epochengine::input::action_held(epochengine::input::Action::LookDown) ? 1.0f : 0.0f);

                                    if (epochengine::input::action_pressed(epochengine::input::Action::ResetCamera))
                                        epochengine::previewgrid::reset_camera(ctx.get());

                                    if (mouse_right_down && look_state.looking)
                                    {
                                        const float mouseDeltaX = mouse_pos.x - look_state.last_mouse.x;
                                        const float mouseDeltaY = mouse_pos.y - look_state.last_mouse.y;
                                        const float mouseSensitivity = epochengine::input::mouse_look_sensitivity();
                                        epochengine::previewgrid::look_camera(
                                            ctx.get(),
                                            mouseDeltaX * mouseSensitivity,
                                            -mouseDeltaY * mouseSensitivity);
                                    }
                                    else if (mouse_left_down && !mouse_right_down && look_state.panning)
                                    {
                                        const float mouseDeltaX = mouse_pos.x - look_state.last_mouse.x;
                                        const float mouseDeltaY = mouse_pos.y - look_state.last_mouse.y;
                                        epochengine::previewgrid::pan_camera_drag(
                                            ctx.get(),
                                            mouseDeltaX,
                                            -mouseDeltaY);
                                    }

                                    if (wheelDelta != 0)
                                    {
                                        epochengine::previewgrid::zoom_camera(
                                            ctx.get(),
                                            (static_cast<float>(wheelDelta) / 120.0f) * epochengine::input::wheel_zoom_step());
                                    }

                                    epochengine::previewgrid::step_camera(
                                        ctx.get(),
                                        dt,
                                        forwardInput,
                                        rightInput,
                                        upInput,
                                        yawInput,
                                        pitchInput);

                                    look_state.last_mouse = mouse_pos;
                                    look_state.looking = mouse_right_down;
                                    look_state.panning = mouse_left_down && !mouse_right_down;
                                }
                            }
                            else
                            {
                                auto& look_state = g_preview_look_states[ctx.get()];
                                look_state.last_mouse = mouse_pos;
                                look_state.looking = false;
                                look_state.panning = false;
                            }

                            switch (editor_frame.command)
                            {
                            case epochengine::EditorCommand::OpenLauncher:
                                session.launcher_loading_frames = (std::max)(session.launcher_loading_frames, std::uint32_t{ 18 });
                                session.launcher_loading_total_frames =
                                    (std::max)(session.launcher_loading_total_frames, session.launcher_loading_frames);
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
                            std::optional<std::string> pendingEditorProject{};
                            EditorApplicationKind pendingEditorApplication = session.pending_editor_application;
                            const int transitionWidth = (std::max)(1, ctx ? ctx->get_width_safe() : (win ? win->width : 1));
                            const int transitionHeight = (std::max)(1, ctx ? ctx->get_height_safe() : (win ? win->height : 1));
                            if (session.pending_editor_project_id && session.launcher_loading_frames == 0)
                            {
                                session.launcher_loading_frames = 18;
                                session.launcher_loading_total_frames = 18;
                            }
                            if (session.launcher_loading_frames > 0 && session.launcher_loading_total_frames == 0)
                                session.launcher_loading_total_frames = session.launcher_loading_frames;
                            const bool draw_transition_loading =
                                session.launcher_loading_frames > 0
                                || session.pending_editor_project_id.has_value();
                            if (draw_transition_loading)
                            {
                                const bool loadingEditor = session.pending_editor_project_id.has_value();
                                const std::string projectLabel = loadingEditor
                                    ? std::string{ application_label(session.pending_editor_application) }
                                    : std::string{ "project launcher" };
                                const std::string transitionTitle = loadingEditor ? "Loading Editor" : "Loading Launcher";
                                const std::string transitionMessage = loadingEditor
                                    ? std::string{ "Preparing editor workspace for " } + projectLabel + "."
                                    : std::string{ "Returning to the project launcher." };
                                const std::string transitionLabel = loadingEditor ? "Editor loading" : "Launcher loading";
                                const double activitySeconds = std::chrono::duration<double>(
                                    std::chrono::steady_clock::now().time_since_epoch()).count();
                                const float activityPhase = static_cast<float>(std::fmod(activitySeconds * 0.35, 1.0));
                                const float remainingFrames = static_cast<float>(session.launcher_loading_frames);
                                const float totalFrames = static_cast<float>((std::max)(std::uint32_t{ 1 }, session.launcher_loading_total_frames));
                                const float finishHoldFrames = (std::min)(1.0f, totalFrames);
                                const float activeFrames = (std::max)(1.0f, totalFrames - finishHoldFrames);
                                const float activeRemaining = (std::max)(0.0f, remainingFrames - finishHoldFrames);
                                const float completedFraction = std::clamp(1.0f - activeRemaining / activeFrames, 0.0f, 1.0f);
                                const float transitionProgress = std::clamp(0.12f + completedFraction * 0.88f, 0.12f, 1.0f);
                                const std::string transitionStatus = completedFraction < 0.34f
                                    ? std::string{ "preparing" }
                                    : completedFraction < 0.92f
                                        ? std::string{ "loading" }
                                        : std::string{ "ready" };
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

                                if (session.launcher_loading_frames > 0)
                                    --session.launcher_loading_frames;
                                if (session.launcher_loading_frames == 0)
                                    session.launcher_loading_total_frames = 0;
                                if (session.pending_editor_project_id && session.launcher_loading_frames == 0)
                                {
                                    pendingEditorProject = std::move(session.pending_editor_project_id);
                                    pendingEditorApplication = session.pending_editor_application;
                                    session.pending_editor_project_id.reset();
                                    session.pending_editor_application = EditorApplicationKind::Standard;
                                }
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
                                if (ctx_running)
                                    ctx->present_safe();
                                if (pendingEditorProject)
                                    switch_session_to_editor(session, ctx, pendingEditorApplication);
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
                                else if (const auto project_id = project_id_from_choice(*choice); !project_id.empty())
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

                                        targetSession.pending_editor_project_id = project_id;
                                        targetSession.pending_editor_application = application_from_choice(*choice);
                                        targetSession.launcher_loading_frames = 18;
                                        targetSession.launcher_loading_total_frames = 18;
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

                if (cli::smoke_requested && !smoke_capture_taken && frame_count >= smoke_capture_frame)
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
