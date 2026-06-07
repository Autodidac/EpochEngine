/************************************************
 *  Â¦Â¦Â¦Â¦Â¦Â¦Â¦+Â¦Â¦Â¦Â¦Â¦Â¦+  Â¦Â¦Â¦Â¦Â¦Â¦+  Â¦Â¦Â¦Â¦Â¦Â¦+Â¦Â¦+  Â¦Â¦+   *
 *  Â¦Â¦+----+Â¦Â¦+--Â¦Â¦+Â¦Â¦+---Â¦Â¦+Â¦Â¦+----+Â¦Â¦Â¦  Â¦Â¦Â¦   *
 *  Â¦Â¦Â¦Â¦Â¦+  Â¦Â¦Â¦Â¦Â¦Â¦++Â¦Â¦Â¦   Â¦Â¦Â¦Â¦Â¦Â¦     Â¦Â¦Â¦Â¦Â¦Â¦Â¦Â¦   *
 *  Â¦Â¦+--+  Â¦Â¦+---+ Â¦Â¦Â¦   Â¦Â¦Â¦Â¦Â¦Â¦     Â¦Â¦+--Â¦Â¦Â¦   *
 *  Â¦Â¦Â¦Â¦Â¦Â¦Â¦+Â¦Â¦Â¦     +Â¦Â¦Â¦Â¦Â¦Â¦+++Â¦Â¦Â¦Â¦Â¦Â¦+Â¦Â¦Â¦  Â¦Â¦Â¦   *
 *  +------++-+      +-----+  +-----++-+  +-+   *
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
 // engine.cpp (module implementation unit; module names remain compatibility-stable)
 //
 // FIXES APPLIED:
 //  - No direct access to core::Context private members (ctx->hwnd).
 //    We only query windows via MultiContextManager APIs.
 //  - Removed non-constant switch case labels for ContextType::Unknown/Noop
 //    because your ContextType in your current modules is not an enum with those
 //    exact enumerators (or theyÃ¢â‚¬â„¢re not visible here). Default handles it.
 //
//#include "pch.h"

#include "../include/engine.config.hpp"
#include "../include/epoch.api_types.hpp"
#include "../include/engine.hpp"
#include "../include/epoch.runtime_bridge.hpp"

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
#endif

// -----------------------------
// Standard library imports
// -----------------------------
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <iomanip>
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
import engine.platform;
//import engine.config;

import epochengine;

import engine.cli;
import engine.version;
import engine.updater;
import engine.input;
import engine.components;

import context.multiplexer;
import context.type;
import context.window;
import core.context;
import core.logger;
import core.path;
import core.time;
import core.timer;

import forest.factory;
import package.registry;
import saveload.system;
import scenesnapshot;
import sceneserializer;
import timeline.system;

import engine.gui;
import gui.menu;
import editor;
import epoch.ai;
import render.device;
import render.device_null;
import render.device_opengl_family;
#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)
import opengl.textures;
#endif
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

import scene;

import snakelike;
import tetrislike;
import pacmanlike;
import froggerlike;
import sokobanlike;
import match3like;

import slidingpuzzlelike;
import minesweeperlike;
import a2048like;

import sandsim;
import cellularsim;

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
#endif

namespace input = epochnamespace::input;
namespace menu = epochnamespace::menu;
namespace gui = epochnamespace::gui;

namespace epochnamespace::core
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

    [[nodiscard]] inline bool engine_arcade_screen_graph_contract_ready(epoch::IRenderDevice& device)
    {
        epoch::GraphBuilder builder{};
        const std::string_view screenNameStd = epoch::package_registry::engine_arcade_render_texture_name();
        const epoch::string_view screenName{ screenNameStd.data(), screenNameStd.size() };
        const epoch::render_arcade::ArcadeScreenGraphBuild screen = epoch::render_arcade::add_screen_graph(builder);

        epoch::CompiledGraph graph = builder.compile(device);

        const bool resourceShape =
            graph.render_texture_assets.size() == 1u
            && graph.textures.size() == 1u
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

        const epoch::GraphRenderTextureAsset& compiledScreen = graph.render_texture_assets.front();
        const epoch::GraphTexture& compiledTexture = graph.textures.front();
        const epoch::GraphRenderTarget& compiledTarget = graph.render_targets.front();
        const epoch::GraphMaterial& compiledMaterial = graph.materials.front();
        const epoch::GraphModel& compiledModel = graph.models.front();
        const epoch::PassDecl& pass = graph.passes.front();
        const auto same_text = [](epoch::string_view left, epoch::string_view right) noexcept
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
            && compiledScreen.desc.width == epoch::package_registry::engine_arcade_render_texture_width()
            && compiledScreen.desc.height == epoch::package_registry::engine_arcade_render_texture_height()
            && compiledScreen.desc.usage == epoch::RenderTextureUsage::arcade_cabinet
            && compiledScreen.backend.color_texture
            && compiledScreen.backend.sampler
            && compiledScreen.backend.render_target
            && compiledTexture.backend == compiledScreen.backend.color_texture
            && compiledTexture.sampled_sampler == compiledScreen.backend.sampler
            && compiledTexture.owned_by_render_texture_asset
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
        epoch::NullRenderDevice nullDevice{};
        return engine_arcade_screen_graph_contract_ready(nullDevice);
    }

    [[nodiscard]] inline bool engine_arcade_cabinet_graph_contract_ready(epoch::IRenderDevice& device)
    {
        const epoch::RendererCapabilities caps = device.capabilities();
        if (!epoch::renderer_supports_sampled_render_targets(caps)
            || !epoch::renderer_supports_model_resources(caps))
        {
            return false;
        }

        epoch::GraphBuilder builder{};
        const epoch::render_arcade::ArcadeCabinetGraphBuild cabinet = epoch::render_arcade::add_cabinet_graph(builder);

        epoch::CompiledGraph graph = builder.compile(device);
        const bool resourceShape =
            graph.render_texture_assets.size() == 1u
            && graph.textures.size() == 1u
            && graph.render_targets.size() == 1u
            && graph.buffers.size() == 4u
            && graph.materials.size() == 2u
            && graph.meshes.size() == 2u
            && graph.models.size() == 2u
            && graph.passes.size() == 2u;
        if (!resourceShape)
        {
            graph.destroy(device);
            return false;
        }

        const epoch::GraphRenderTextureAsset& compiledScreen = graph.render_texture_assets.front();
        const epoch::GraphMaterial& compiledScreenMaterial = graph.materials.front();
        const epoch::GraphModel& compiledScreenModel = graph.models.front();
        const epoch::GraphMaterial& compiledMaterial = graph.materials.back();
        const epoch::GraphMesh& compiledMesh = graph.meshes.back();
        const epoch::GraphModel& compiledModel = graph.models.back();
        const epoch::PassDecl& populatePass = graph.passes.front();
        const epoch::PassDecl& cabinetPass = graph.passes[1u];

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
            && compiledMaterial.texture_slots.front().slot == epoch::MaterialTextureSlot::render_surface
            && compiledMaterial.texture_slots.front().texture == cabinet.screen.color_texture;

        const bool modelReady =
            compiledMesh.backend
            && compiledModel.backend
            && compiledModel.mesh_slots.size() == 1u
            && compiledModel.mesh_slots.front().mesh == cabinet.mesh
            && compiledModel.mesh_slots.front().material == cabinet.material;

        const bool bindingReady =
            cabinetPass.binding_set
            && cabinetPass.bindings.read_materials.size() == 1u
            && cabinetPass.bindings.read_materials.front() == compiledMaterial.backend
            && cabinetPass.bindings.read_models.size() == 1u
            && cabinetPass.bindings.read_models.front() == compiledModel.backend
            && cabinetPass.bindings.read_material_textures.size() == 1u
            && cabinetPass.bindings.read_material_textures.front().slot == epoch::MaterialTextureSlot::render_surface
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
        return populateReady && materialReady && modelReady && bindingReady && drawReady;
    }

    [[nodiscard]] inline bool render_surface_requires_render_texture_asset_contract_ready()
    {
        epoch::NullRenderDevice device{};
        epoch::GraphBuilder builder{};

        epoch::TextureDesc plainTextureDesc{};
        plainTextureDesc.width = 64u;
        plainTextureDesc.height = 64u;
        plainTextureDesc.format = epoch::TextureFormat::rgba8_unorm;
        plainTextureDesc.sampled = true;
        plainTextureDesc.debug_name = "plain.render_surface.reject";
        const epoch::GraphResource plainTexture =
            builder.create_texture("plain.render_surface.reject", plainTextureDesc);

        epoch::MaterialDesc materialDesc{};
        materialDesc.name = "plain.render_surface.reject.material";
        materialDesc.unlit = true;
        materialDesc.debug_name = "plain.render_surface.reject.material";
        materialDesc.texture_slots.push_back(epoch::MaterialTextureSlotDesc{
            .slot = epoch::MaterialTextureSlot::render_surface,
            .name = "screen",
            .expected_format = epoch::TextureFormat::rgba8_unorm,
            .required = true
        });

        const epoch::GraphMaterialTextureSlot materialSlots[] = {
            epoch::GraphMaterialTextureSlot{
                .slot = epoch::MaterialTextureSlot::render_surface,
                .texture = plainTexture
            }
        };
        const epoch::GraphResource material = builder.create_material(
            "plain.render_surface.reject.material",
            materialDesc,
            epoch::array_view<const epoch::GraphMaterialTextureSlot>{ materialSlots, 1u });

        const epoch::GraphResource reads[] = { material };
        builder.add_pass(
            "plain.render_surface.reject.pass",
            epoch::array_view<const epoch::GraphResource>{ reads, 1u },
            {},
            [](epoch::ICommandContext& ctx)
            {
                ctx.debug_marker("plain.render_surface.reject.pass");
            });

        epoch::CompiledGraph graph = builder.compile(device);
        const bool ready =
            graph.textures.size() == 1u
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

    struct OpenGLFamilyFakeNativeRttState
    {
        int allocate_count = 0;
        int begin_count = 0;
        int end_count = 0;
        int destroy_count = 0;
        epoch::RendererBackendKind last_backend = epoch::RendererBackendKind::null;
        epoch::u32 last_width = 0;
        epoch::u32 last_height = 0;
        bool saw_depth = false;
        bool saw_sampled = false;
    };

    [[nodiscard]] inline epoch::OpenGLFamilyNativeRenderTextureAllocation fake_opengl_family_allocate_rtt(
        void* user,
        epoch::RendererBackendKind backend,
        const epoch::RenderTextureAssetDesc& desc,
        const epoch::RenderTextureBackendRequirements& requirements,
        epoch::u32 slot)
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

        return epoch::OpenGLFamilyNativeRenderTextureAllocation{
            .framebuffer_object = 1000u + slot,
            .color_object = 2000u + slot,
            .depth_object = requirements.depth_attachment ? 3000u + slot : 0u,
            .sampler_object = 4000u + slot,
            .ready = true
        };
    }

    inline void fake_opengl_family_destroy_rtt(
        void* user,
        epoch::RendererBackendKind backend,
        const epoch::OpenGLFamilyRenderTextureRecord& record)
    {
        auto* const state = static_cast<OpenGLFamilyFakeNativeRttState*>(user);
        if (state && backend == state->last_backend && record.native_allocation_ready)
            ++state->destroy_count;
    }

    [[nodiscard]] inline bool fake_opengl_family_begin_rtt_pass(
        void* user,
        epoch::RendererBackendKind backend,
        const epoch::OpenGLFamilyRenderTextureRecord& record,
        const epoch::RenderPassDesc&)
    {
        auto* const state = static_cast<OpenGLFamilyFakeNativeRttState*>(user);
        if (!state || backend != state->last_backend || !record.native_allocation_ready)
            return false;

        ++state->begin_count;
        return true;
    }

    inline void fake_opengl_family_end_rtt_pass(
        void* user,
        epoch::RendererBackendKind backend,
        const epoch::OpenGLFamilyRenderTextureRecord& record)
    {
        auto* const state = static_cast<OpenGLFamilyFakeNativeRttState*>(user);
        if (state && backend == state->last_backend && record.native_allocation_ready)
            ++state->end_count;
    }

    [[nodiscard]] inline bool opengl_family_arcade_screen_graph_contract_ready()
    {
        const epoch::RendererBackendKind backends[] = {
            epoch::RendererBackendKind::opengl,
            epoch::RendererBackendKind::sdl3,
            epoch::RendererBackendKind::sfml3
        };

        for (const epoch::RendererBackendKind backend : backends)
        {
            epoch::OpenGLFamilyRenderDevice device{ backend };
            const epoch::RendererCapabilities caps = device.capabilities();
            if (!epoch::renderer_supports_sampled_render_targets(caps)
                || !engine_arcade_screen_graph_contract_ready(device))
            {
                return false;
            }
        }

        return true;
    }

    [[nodiscard]] inline bool opengl_family_arcade_cabinet_graph_contract_ready()
    {
        const epoch::RendererBackendKind backends[] = {
            epoch::RendererBackendKind::opengl,
            epoch::RendererBackendKind::sdl3,
            epoch::RendererBackendKind::sfml3
        };

        for (const epoch::RendererBackendKind backend : backends)
        {
            epoch::OpenGLFamilyRenderDevice device{ backend };
            if (!engine_arcade_cabinet_graph_contract_ready(device))
            {
                return false;
            }

            const epoch::OpenGLFamilyCommandContext& context = device.graphics_context();
            const epoch::CommandResourceBindings& boundResources = context.bound_resources();
            const bool cabinetBindingEvidence =
                context.last_width() == epoch::package_registry::engine_arcade_render_texture_width()
                && context.last_height() == epoch::package_registry::engine_arcade_render_texture_height()
                && context.last_render_target()
                && context.bound_binding_set()
                && boundResources.read_materials.size() == 1u
                && boundResources.read_models.size() == 1u
                && boundResources.read_material_textures.size() == 1u
                && boundResources.read_samplers.size() == 1u
                && boundResources.read_material_textures.front().slot == epoch::MaterialTextureSlot::render_surface
                && boundResources.read_material_textures.front().texture
                && boundResources.read_material_textures.front().sampler
                && boundResources.read_material_textures.front().sampler == boundResources.read_samplers.front()
                && context.last_model()
                && context.last_model() == boundResources.read_models.front();

            if (!cabinetBindingEvidence)
                return false;
        }

        return true;
    }

    [[nodiscard]] inline bool opengl_family_arcade_fake_native_rtt_contract_ready()
    {
        const epoch::RendererBackendKind backends[] = {
            epoch::RendererBackendKind::opengl,
            epoch::RendererBackendKind::sdl3,
            epoch::RendererBackendKind::sfml3
        };

        for (const epoch::RendererBackendKind backend : backends)
        {
            OpenGLFamilyFakeNativeRttState state{};
            epoch::OpenGLFamilyRenderDevice device{ backend };
            device.set_native_render_texture_hooks(epoch::OpenGLFamilyNativeRenderTextureHooks{
                .user = &state,
                .allocate = fake_opengl_family_allocate_rtt,
                .destroy = fake_opengl_family_destroy_rtt,
                .begin_pass = fake_opengl_family_begin_rtt_pass,
                .end_pass = fake_opengl_family_end_rtt_pass
            });

            const epoch::RendererCapabilities caps = device.capabilities();
            if (!epoch::renderer_supports_native_sampled_render_targets(caps)
                || !engine_arcade_cabinet_graph_contract_ready(device))
            {
                return false;
            }

            const epoch::OpenGLFamilyCommandContext& context = device.graphics_context();
            const bool ready =
                state.allocate_count == 1
                && state.begin_count == 1
                && state.end_count == 1
                && state.destroy_count == 1
                && state.last_backend == backend
                && state.last_width == epoch::package_registry::engine_arcade_render_texture_width()
                && state.last_height == epoch::package_registry::engine_arcade_render_texture_height()
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

    [[nodiscard]] inline bool opengl_real_native_rtt_hook_contract_ready()
    {
#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)
        epoch::OpenGLFamilyRenderDevice device{ epoch::RendererBackendKind::opengl };
        device.set_native_render_texture_hooks(
            epochnamespace::opengltextures::make_native_render_texture_hooks());

        const epoch::RendererCapabilities caps = device.capabilities();
        if (!epoch::renderer_supports_native_sampled_render_targets(caps))
            return false;

        const epoch::RenderTextureAssetDesc desc = epoch::render_arcade::make_screen_render_texture_desc();
        const epoch::RenderTextureAssetHandles handles = device.create_render_texture_asset(desc);
        const epoch::OpenGLFamilyRenderTextureRecord* const record =
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
        const epoch::RendererBackendKind backends[] = {
            epoch::RendererBackendKind::opengl,
            epoch::RendererBackendKind::sdl3,
            epoch::RendererBackendKind::sfml3
        };

        epoch::RenderTextureAssetDesc screenDesc{};
        screenDesc.width = epoch::package_registry::engine_arcade_render_texture_width();
        screenDesc.height = epoch::package_registry::engine_arcade_render_texture_height();
        screenDesc.color_format = epoch::TextureFormat::rgba8_unorm;
        screenDesc.depth_format = epoch::TextureFormat::depth24_stencil8;
        screenDesc.has_depth = true;
        screenDesc.sampled_after_render = true;
        screenDesc.usage = epoch::RenderTextureUsage::arcade_cabinet;
        screenDesc.debug_name = "engine_arcade.screen";

        for (const epoch::RendererBackendKind backend : backends)
        {
            epoch::OpenGLFamilyRenderDevice device{ backend };
            const epoch::RenderTextureAssetHandles handles = device.create_render_texture_asset(screenDesc);
            const epoch::OpenGLFamilyRenderTextureRecord* const record =
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
        epoch::SdlRenderDevice device{};
        const epoch::RendererCapabilities caps = device.capabilities();
        const bool runtimeAvailable = device.runtime_renderer_available();
        if (device.backend_name() != "sdl3"
            || epoch::renderer_supports_native_sampled_render_targets(caps) != runtimeAvailable)
            return false;

        const epoch::RenderTextureAssetDesc desc = epoch::render_arcade::make_screen_render_texture_desc();
        const epoch::RenderTextureAssetHandles handles = device.create_render_texture_asset(desc);
        const epoch::SdlRenderTextureRecord* const record = device.resolve_render_texture(handles.render_target);

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
        epoch::SfmlRenderDevice device{};
        const epoch::RendererCapabilities caps = device.capabilities();
        const bool runtimeAvailable = device.runtime_renderer_available();
        if (device.backend_name() != "sfml3"
            || epoch::renderer_supports_native_sampled_render_targets(caps) != runtimeAvailable)
            return false;

        const epoch::RenderTextureAssetDesc desc = epoch::render_arcade::make_screen_render_texture_desc();
        const epoch::RenderTextureAssetHandles handles = device.create_render_texture_asset(desc);
        const epoch::SfmlRenderTextureRecord* const record = device.resolve_render_texture(handles.render_target);

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
        epoch::RaylibRenderDevice device{};
        const epoch::RendererCapabilities caps = device.capabilities();
        const bool runtimeAvailable = device.runtime_renderer_available();
        if (device.backend_name() != "raylib"
            || epoch::renderer_supports_native_sampled_render_targets(caps) != runtimeAvailable)
            return false;

        const epoch::RenderTextureAssetDesc desc = epoch::render_arcade::make_screen_render_texture_desc();
        const epoch::RenderTextureAssetHandles handles = device.create_render_texture_asset(desc);
        const epoch::RaylibRenderTextureRecord* const record = device.resolve_render_texture(handles.render_target);

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
        epoch::SdlRenderDevice device{};
        const epoch::RendererCapabilities caps = device.capabilities();
        if (device.backend_name() != "sdl3"
            || !epoch::renderer_supports_sampled_render_targets(caps)
            || !epoch::renderer_supports_model_resources(caps))
        {
            return false;
        }

        epoch::GraphBuilder builder{};
        const epoch::render_arcade::ArcadeCabinetGraphBuild cabinet = epoch::render_arcade::add_cabinet_graph(builder);
        epoch::CompiledGraph graph = builder.compile(device);

        const bool resourceShape =
            graph.buffers.size() == 4u
            && graph.materials.size() == 2u
            && graph.meshes.size() == 2u
            && graph.models.size() == 2u
            && graph.passes.size() == 2u;
        if (!resourceShape)
        {
            graph.destroy(device);
            return false;
        }

        const epoch::GraphMesh& compiledMesh = graph.meshes.back();
        const epoch::GraphModel& compiledModel = graph.models.back();
        const epoch::PassDecl& cabinetPass = graph.passes[1u];

        const bool graphReady =
            compiledMesh.backend
            && compiledModel.backend
            && compiledModel.mesh_slots.size() == 1u
            && compiledModel.mesh_slots.front().mesh == cabinet.mesh
            && cabinetPass.binding_set
            && cabinetPass.bindings.read_materials.size() == 1u
            && cabinetPass.bindings.read_models.size() == 1u
            && cabinetPass.bindings.read_models.front() == compiledModel.backend
            && cabinetPass.draw_models.size() == 1u
            && cabinetPass.draw_models.front().model == cabinet.model
            && cabinetPass.draw_models.front().backend == compiledModel.backend;

        epoch::SdlCommandContext& context = static_cast<epoch::SdlCommandContext&>(device.acquire_graphics_context());
        graph.execute(device);
        const epoch::ModelHandle submitted = context.last_model();
        const bool submitReady =
            graphReady
            && submitted
            && submitted == compiledModel.backend
            && context.bound_binding_set() == cabinetPass.binding_set
            && context.bound_resources().read_models.size() == 1u
            && context.bound_resources().read_models.front() == submitted
            && device.resolve_model(submitted) != nullptr;

        graph.destroy(device);
        return submitReady && device.resolve_model(submitted) == nullptr;
#else
        return true;
#endif
    }

    [[nodiscard]] inline bool sfml_arcade_cabinet_graph_contract_ready()
    {
#if defined(EPOCH_USING_SFML) && (EPOCH_USING_SFML == 1)
        epoch::SfmlRenderDevice device{};
        const epoch::RendererCapabilities caps = device.capabilities();
        if (device.backend_name() != "sfml3"
            || !epoch::renderer_supports_sampled_render_targets(caps)
            || !epoch::renderer_supports_model_resources(caps))
        {
            return false;
        }

        epoch::GraphBuilder builder{};
        const epoch::render_arcade::ArcadeCabinetGraphBuild cabinet = epoch::render_arcade::add_cabinet_graph(builder);
        epoch::CompiledGraph graph = builder.compile(device);

        const bool resourceShape =
            graph.buffers.size() == 4u
            && graph.materials.size() == 2u
            && graph.meshes.size() == 2u
            && graph.models.size() == 2u
            && graph.passes.size() == 2u;
        if (!resourceShape)
        {
            graph.destroy(device);
            return false;
        }

        const epoch::GraphMesh& compiledMesh = graph.meshes.back();
        const epoch::GraphModel& compiledModel = graph.models.back();
        const epoch::PassDecl& cabinetPass = graph.passes[1u];

        const bool graphReady =
            compiledMesh.backend
            && compiledModel.backend
            && compiledModel.mesh_slots.size() == 1u
            && compiledModel.mesh_slots.front().mesh == cabinet.mesh
            && cabinetPass.binding_set
            && cabinetPass.bindings.read_materials.size() == 1u
            && cabinetPass.bindings.read_models.size() == 1u
            && cabinetPass.bindings.read_models.front() == compiledModel.backend
            && cabinetPass.draw_models.size() == 1u
            && cabinetPass.draw_models.front().model == cabinet.model
            && cabinetPass.draw_models.front().backend == compiledModel.backend;

        epoch::SfmlCommandContext& context = static_cast<epoch::SfmlCommandContext&>(device.acquire_graphics_context());
        graph.execute(device);
        const epoch::ModelHandle submitted = context.last_model();
        const bool submitReady =
            graphReady
            && submitted
            && submitted == compiledModel.backend
            && context.bound_binding_set() == cabinetPass.binding_set
            && context.bound_resources().read_models.size() == 1u
            && context.bound_resources().read_models.front() == submitted
            && device.resolve_model(submitted) != nullptr;

        graph.destroy(device);
        return submitReady && device.resolve_model(submitted) == nullptr;
#else
        return true;
#endif
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

        auto forestProfile = epoch::forest::default_profile(epoch::forest::ForestPreset::Tree);
        forestProfile.temporal.timeSeconds = forestProfile.temporal.durationSeconds;
        const auto forestEstimate = epoch::forest::estimate_preview_stats(forestProfile);
        const auto forestGeometry = epoch::forest::build_preview_geometry(forestProfile);
        const auto previewActivation = epoch::forest::activation_for_editor_preview();
        const auto sceneActivation = epoch::forest::activation_for_scene_use();
        check("forest.config", epoch::forest::valid(forestProfile.config));
        check("forest.estimate", forestEstimate.nodes > 1u && forestEstimate.branches > 0u);
        check(
            "forest.geometry",
            forestGeometry.segmentCount > 0u
            && forestGeometry.leafCount > 0u
            && forestGeometry.segmentCount <= epoch::forest::kForestPreviewMaxSegments
            && forestGeometry.leafCount <= epoch::forest::kForestPreviewMaxLeaves);
        check(
            "forest.activation",
            !previewActivation.includeInGeneratedProject
            && sceneActivation.includeInGeneratedProject
            && sceneActivation.emitPackageManifest
            && sceneActivation.attachToMainScene);

        const auto packageValidation = epoch::package_registry::validate_registry();
        const auto* forestPackage = epoch::package_registry::find(epoch::package_registry::kEngineForestFactoryPackageId);
        const auto* bonsaiPackage = epoch::package_registry::find(epoch::package_registry::recommended_local_image_model_id());
        const auto* qwenPackage = epoch::package_registry::find(epoch::package_registry::kQwenCoderPackageId);
        const auto* nemotronPackage = epoch::package_registry::find(epoch::package_registry::kNemotronNanoPackageId);
        check(
            "package.registry",
            packageValidation.ok
            && packageValidation.packageCount == epoch::package_registry::known_packages().size()
            && packageValidation.duplicateIdCount == 0u
            && packageValidation.modelAssetCount >= 5u
            && packageValidation.networkSensitiveCount >= 2u);
        check(
            "package.forest_factory",
            forestPackage != nullptr
            && forestPackage->kind == epoch::package_registry::PackageKind::CoreOptIn
            && forestPackage->activation == epoch::package_registry::ActivationMode::MainSceneUse
            && epoch::package_registry::is_core_opt_in(forestPackage->id)
            && epoch::package_registry::ships_in_core_without_default_project_payload(forestPackage->id)
            && epoch::package_registry::external_source_repo(forestPackage->id).find("EpochEngineExtensions") != std::string_view::npos);
        check(
            "package.os_models",
            bonsaiPackage != nullptr
            && qwenPackage != nullptr
            && nemotronPackage != nullptr
            && epoch::package_registry::is_model_asset(bonsaiPackage->id)
            && epoch::package_registry::is_model_asset(qwenPackage->id)
            && epoch::package_registry::is_model_asset(nemotronPackage->id)
            && epoch::package_registry::must_use_human_build_gate(bonsaiPackage->id)
            && epoch::package_registry::activation_mode_name(bonsaiPackage->activation) == std::string_view{ "Model download opt-in" });
        check(
            "package.network_gates",
            epoch::package_registry::requires_explicit_network_approval(epoch::package_registry::kEngineAuthoritativeServerPackageId)
            && epoch::package_registry::can_create_server_or_listener_after_approval(epoch::package_registry::kEngineListenServerPackageId)
            && epoch::package_registry::must_use_human_build_gate("missing_package"));
        check("render.engine_arcade_screen_graph", engine_arcade_screen_graph_contract_ready());
        check("render.render_surface_requires_rtt_asset", render_surface_requires_render_texture_asset_contract_ready());
        check("render.opengl_family_arcade_screen_graph", opengl_family_arcade_screen_graph_contract_ready());
        check("render.opengl_family_arcade_cabinet_graph", opengl_family_arcade_cabinet_graph_contract_ready());
        check("render.opengl_family_arcade_native_requirements", opengl_family_arcade_native_requirements_contract_ready());
        check("render.opengl_family_arcade_fake_native_rtt", opengl_family_arcade_fake_native_rtt_contract_ready());
        check("render.opengl_real_native_rtt_hook", opengl_real_native_rtt_hook_contract_ready());
        check("render.sdl_native_render_texture_device", sdl_native_render_texture_device_contract_ready());
        check("render.sfml_native_render_texture_device", sfml_native_render_texture_device_contract_ready());
        check("render.raylib_native_render_texture_device", raylib_native_render_texture_device_contract_ready());
        check("render.sdl_arcade_cabinet_graph", sdl_arcade_cabinet_graph_contract_ready());
        check("render.sfml_arcade_cabinet_graph", sfml_arcade_cabinet_graph_contract_ready());

        epoch::saveload::StreamingSaveConfig saveConfig{};
        saveConfig.enabled = true;
        saveConfig.mode = epoch::saveload::SaveStreamMode::Interval;
        saveConfig.interval_seconds = -3.0;
        saveConfig.frame_interval = 0;
        saveConfig.max_snapshots = 0;
        saveConfig.profile_name.clear();
        saveConfig.target_root.clear();
        epoch::saveload::clamp_streaming_save_config(saveConfig);

        epoch::saveload::StreamingSaveStatus saveStatus{};
        epoch::core::time::simulation_stats timeStats{};
        timeStats.frame_index = 240;
        timeStats.simulated_seconds = 4.0;

        const auto saveCadenceDue = epoch::saveload::make_streaming_save_cadence_plan(saveConfig, saveStatus, timeStats);
        const bool shouldCapture = epoch::saveload::should_capture_checkpoint(saveConfig, saveStatus, timeStats);
        epoch::saveload::mark_checkpoint_captured(saveStatus, saveConfig, timeStats);
        const auto saveCadenceScheduled = epoch::saveload::make_streaming_save_cadence_plan(saveConfig, saveStatus, timeStats);
        const std::string saveDescription = epoch::saveload::describe_streaming_save(saveConfig, saveStatus);
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
            && epoch::saveload::detect_streaming_save_profile(saveConfig) == epoch::saveload::StreamingSaveProfile::EditorInterval15s
            && epoch::saveload::describe_retention(saveConfig).find("rolling 1 checkpoint") != std::string::npos);
        check(
            "timeline.cadence",
            !saveCadenceScheduled.capture_due
            && saveCadenceScheduled.next_seconds > timeStats.simulated_seconds
            && saveCadenceScheduled.seconds_until > 0.0
            && epoch::saveload::streaming_save_cadence_summary(saveCadenceDue).find("capture due") != std::string::npos
            && epoch::saveload::streaming_save_cadence_summary(saveCadenceScheduled).find("Time interval") != std::string::npos);

        const auto* intervalSaveProfile = epoch::saveload::find_streaming_save_profile(
            epoch::saveload::StreamingSaveProfile::EditorInterval15s);
        const auto* keyedSaveProfile = epoch::saveload::find_streaming_save_profile("timeline_keyed");
        epoch::saveload::StreamingSaveConfig keyedSaveConfig{};
        epoch::saveload::apply_streaming_save_profile(
            keyedSaveConfig,
            epoch::saveload::StreamingSaveProfile::TimelineKeyed);
        const auto saveProfileChangePlan = epoch::saveload::make_streaming_save_profile_change_plan(
            saveConfig,
            epoch::saveload::StreamingSaveProfile::TimelineKeyed);
        check(
            "timeline.stream_profiles",
            epoch::saveload::validate_streaming_save_profile_descriptors()
            && epoch::saveload::streaming_save_profile_count() == 4u
            && intervalSaveProfile != nullptr
            && intervalSaveProfile->mode == epoch::saveload::SaveStreamMode::Interval
            && intervalSaveProfile->enabled
            && intervalSaveProfile->interval_seconds == 15.0
            && keyedSaveProfile != nullptr
            && keyedSaveProfile->profile == epoch::saveload::StreamingSaveProfile::TimelineKeyed
            && keyedSaveConfig.enabled
            && keyedSaveConfig.mode == epoch::saveload::SaveStreamMode::TimelineKey
            && keyedSaveConfig.max_snapshots == 256u
            && epoch::saveload::stream_profile_id(epoch::saveload::StreamingSaveProfile::EditorFrame120) == std::string_view{ "editor_frame_120" }
            && epoch::saveload::stream_profile_summary(epoch::saveload::StreamingSaveProfile::TimelineKeyed).find("timeline keys") != std::string_view::npos);
        check(
            "timeline.profile_change",
            saveProfileChangePlan.valid
            && saveProfileChangePlan.from_profile_id == "editor_interval_15s"
            && saveProfileChangePlan.to_profile_id == "timeline_keyed"
            && saveProfileChangePlan.mode == epoch::saveload::SaveStreamMode::TimelineKey
            && saveProfileChangePlan.max_snapshots == 256u
            && epoch::saveload::streaming_save_profile_change_summary(saveProfileChangePlan).find("timeline_keyed") != std::string::npos);

        auto timelineTracks = epoch::timeline::default_editor_tracks();
        epoch::timeline::TimelineState timelineState{};
        timelineState.playing = true;
        timelineState.duration_seconds = 8.0;
        timelineState.fixed_dt_seconds = 1.0 / 60.0;
        epoch::timeline::sync_to_simulation(timelineState, timeStats);
        std::vector<epoch::timeline::TimelineEvent> timelineEvents{};
        timelineEvents.push_back(epoch::timeline::make_event_from_stats(
            "save",
            epoch::timeline::TimelineEventKind::Checkpoint,
            timeStats,
            saveStatus.last_snapshot_label,
            "PersistentLevel",
            saveStatus.last_output_path));
        timelineEvents.push_back(epoch::timeline::TimelineEvent{
            .track_id = "camera",
            .kind = epoch::timeline::TimelineEventKind::CameraCut,
            .simulated_seconds = 1.0,
            .frame_index = 60,
            .label = "camera cut",
            .target_name = "EditorCamera"
        });
        epoch::timeline::sort_events(timelineEvents);
        const auto sceneKey = epoch::timeline::to_scene_timeline_key(timelineEvents.front());
        const epoch::timeline::TimelineViewConfig timelineView{
            .visible_start_seconds = 0.0,
            .visible_duration_seconds = 5.0,
            .pixel_width = 500.0
        };
        const auto timelineMetrics = epoch::timeline::make_view_metrics(
            timelineState,
            timelineTracks,
            timelineEvents,
            timelineView);
        const auto timelineTrackSummaries = epoch::timeline::summarize_tracks(timelineTracks, timelineEvents);
        const std::string timelineViewSummary = epoch::timeline::describe_view(
            timelineState,
            timelineTracks,
            timelineEvents,
            timelineView);
        const epoch::timeline::TimelineLaneLayoutConfig timelineLaneLayout{
            .pixel_width = 500.0,
            .header_width = 100.0,
            .lane_height = 24.0,
            .lane_gap = 4.0,
            .top_padding = 6.0
        };
        const auto timelineLanes = epoch::timeline::make_lane_geometry(
            timelineTracks,
            timelineLaneLayout);
        const auto timelineMarkers = epoch::timeline::make_event_markers(
            timelineTracks,
            timelineEvents,
            timelineView,
            timelineLaneLayout,
            timelineState.duration_seconds);
        const std::string timelineLaneSummary = epoch::timeline::describe_lane_layout(
            timelineLanes,
            timelineMarkers);
        check(
            "timeline.model",
            timelineTracks.size() == 4u
            && epoch::timeline::enabled_track_count(timelineTracks) == 4u
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

        const auto inputProfile = epochnamespace::input::make_profile(epochnamespace::input::ProfilePreset::EditorDefault);
        const auto resetBinding = inputProfile.bindings[
            epochnamespace::input::action_index(epochnamespace::input::Action::ResetCamera)];
        const auto copyBinding = inputProfile.bindings[
            epochnamespace::input::action_index(epochnamespace::input::Action::ClipboardCopy)];
        const auto contextBinding = inputProfile.bindings[
            epochnamespace::input::action_index(epochnamespace::input::Action::ContextMenu)];
        const std::string inputSummary = epochnamespace::input::profile_summary(epochnamespace::input::ProfilePreset::EditorDefault);
        check(
            "input.profile",
            epochnamespace::input::validate_profile(inputProfile)
            && epochnamespace::input::bound_action_count(inputProfile) == static_cast<std::size_t>(epochnamespace::input::Action::Count)
            && resetBinding.primary == epochnamespace::input::Key::Home
            && copyBinding.primary == epochnamespace::input::Key::C
            && copyBinding.control
            && contextBinding.mouse == epochnamespace::input::MouseButton::MouseRight
            && epochnamespace::input::action_label(epochnamespace::input::Action::ResetCamera) == std::string_view{ "Reset Camera To Center" }
            && inputSummary.find("reset Home") != std::string::npos
            && inputSummary.find("context Mouse Right") != std::string::npos);

        epoch::scene::SceneSnapshot snapshot{};
        snapshot.scene_id = "timeline \"contract\"";
        snapshot.world_name = "Persistent\nLevel";
        snapshot.captured_frame_index = timeStats.frame_index;
        snapshot.captured_simulated_seconds = timeStats.simulated_seconds;

        epoch::scene::SceneObjectSnapshot object{};
        object.name = "StarterCube";
        object.type = "StaticMesh";
        object.category = "Gameplay";
        object.position = { 0.0F, 0.5F, 0.0F };
        snapshot.objects.push_back(object);
        snapshot.timeline_keys.push_back(epoch::scene::make_timeline_key(2.0, 120, "later", "checkpoint", "StarterCube", "late"));
        snapshot.timeline_keys.push_back(epoch::scene::make_timeline_key(1.0, 60, "first", "checkpoint", "StarterCube", "payload\tvalue"));
        snapshot.timeline_keys.push_back(epoch::timeline::to_scene_timeline_key(timelineEvents.front()));
        epoch::scene::sort_timeline_keys(snapshot);

        const auto categoryCounts = epoch::scene::object_count_by_category(snapshot);
        const std::string snapshotText = epoch::scene::serialize_snapshot_text(snapshot);
        const std::string snapshotSummary = epoch::scene::snapshot_summary(snapshot);
        const auto parsedSnapshot = epoch::scene::parse_snapshot_text(snapshotText);
        const auto checkpointRecord = epoch::saveload::make_checkpoint_record(
            saveConfig,
            saveStatus,
            timeStats,
            snapshotText.size(),
            snapshot.timeline_keys.size());
        const auto checkpointPackage = epoch::saveload::make_checkpoint_package(checkpointRecord, snapshotText);
        const auto checkpointWritePlan = epoch::saveload::make_checkpoint_write_plan(saveConfig, checkpointPackage);
        const auto checkpointRestorePlan = epoch::saveload::make_checkpoint_restore_plan(saveConfig, checkpointRecord);
        std::vector<epoch::saveload::StreamingCheckpointRecord> retentionRecords{};
        retentionRecords.push_back(checkpointRecord);
        retentionRecords.push_back(checkpointRecord);
        retentionRecords.back().label = "editor_timeline_frame_0121";
        retentionRecords.back().output_path = "cache/saves/timeline/editor_timeline_frame_0121.checkpoint";
        retentionRecords.push_back(checkpointRecord);
        retentionRecords.back().label = "editor_timeline_frame_0122";
        retentionRecords.back().output_path = "cache/saves/timeline/editor_timeline_frame_0122.checkpoint";
        epoch::saveload::StreamingSaveConfig retentionConfig = saveConfig;
        retentionConfig.max_snapshots = 2u;
        const auto checkpointRetentionPlan = epoch::saveload::make_checkpoint_retention_plan(
            retentionConfig,
            retentionRecords);
        const auto blockedWriteResult = epoch::saveload::write_checkpoint_package(
            checkpointWritePlan,
            checkpointPackage,
            epoch::saveload::StreamingCheckpointWriteApproval{});
        const std::string checkpointSnapshotPayload = epoch::saveload::checkpoint_snapshot_payload(
            checkpointWritePlan,
            checkpointPackage);
        const std::string checkpointManifestLine = checkpointPackage.manifest_line;
        check(
            "snapshot.lookup",
            epoch::scene::find_object(snapshot, "StarterCube") != nullptr
            && categoryCounts.contains("Gameplay")
            && categoryCounts.at("Gameplay") == 1u);
        check(
            "snapshot.timeline_sort",
            snapshot.timeline_keys.size() == 3u
            && snapshot.timeline_keys.front().frame_index == 60u);
        check(
            "snapshot.serialize",
            snapshotText.find("epoch_snapshot 1") != std::string::npos
            && snapshotText.find("timeline \\\"contract\\\"") != std::string::npos
            && snapshotText.find("Persistent\\nLevel") != std::string::npos
            && snapshotText.find("payload\\tvalue") != std::string::npos
            && snapshotSummary.find("objects 1") != std::string::npos);

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
            epoch::saveload::validate_checkpoint_package(checkpointPackage)
            && checkpointPackage.scene_text == snapshotText
            && checkpointManifestLine.find("hash \"") != std::string::npos
            && epoch::saveload::checkpoint_package_summary(checkpointPackage).find("deterministic restore") != std::string::npos
            && epoch::scene::parse_snapshot_text(checkpointPackage.scene_text).ok);
        check(
            "checkpoint.write_plan",
            checkpointWritePlan.valid
            && checkpointWritePlan.root_path == saveConfig.target_root
            && checkpointWritePlan.snapshot_path == checkpointRecord.output_path
            && checkpointWritePlan.scene_payload_path.find(".epoch") != std::string::npos
            && checkpointWritePlan.manifest_path.find("manifest.timeline.log") != std::string::npos
            && epoch::saveload::checkpoint_write_plan_summary(checkpointWritePlan).find("write plan") != std::string::npos);
        check(
            "checkpoint.restore_plan",
            checkpointRestorePlan.valid
            && checkpointRestorePlan.checkpoint_label == checkpointRecord.label
            && checkpointRestorePlan.snapshot_path == checkpointRecord.output_path
            && checkpointRestorePlan.scene_payload_path == checkpointWritePlan.scene_payload_path
            && checkpointRestorePlan.manifest_path == checkpointWritePlan.manifest_path
            && epoch::saveload::checkpoint_restore_plan_summary(checkpointRestorePlan).find("restore plan") != std::string::npos);
        check(
            "checkpoint.retention_plan",
            checkpointRetentionPlan.valid
            && checkpointRetentionPlan.source_count == 3u
            && checkpointRetentionPlan.retained_count == 2u
            && checkpointRetentionPlan.prune_labels.size() == 1u
            && checkpointRetentionPlan.prune_labels.front() == checkpointRecord.label
            && checkpointRetentionPlan.prune_snapshot_paths.front() == checkpointRecord.output_path
            && epoch::saveload::checkpoint_retention_plan_summary(checkpointRetentionPlan).find("prune 1") != std::string::npos);
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
        const auto* profile = epochnamespace::editor_find_project_profile(project_id);
        const auto ensured = epochnamespace::editor_ensure_project_shell(project_id);
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

        const auto build = epochnamespace::editor_build_project(ensured.root_path);
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

        const auto trainingPaths = epoch::ai::default_training_paths();
        const auto manifest = epoch::ai::active_model_manifest();
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
        add_evidence_path(trainingPaths.local_capture_jsonl);
        add_evidence_path(trainingPaths.mcp_capture_jsonl);

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

        epoch::ai::append_mcp_capture(epoch::ai::McpCaptureRecord{
            .server = "epoch-editor-cli",
            .tool = "editor-project-self-test",
            .prompt = "Run visible editor project self-test for " + projectId,
            .normalized_output = normalizedOutput,
            .source_path = ensured.default_script_path.empty() ? ensured.manifest_path : ensured.default_script_path
        });

        epoch::ai::IterationPacket packet{};
        packet.packet_name = projectId + "-cli-self-test";
        packet.task_prompt =
            "Review the staged editor project self-test evidence, identify the next safe builder/verifier action, "
            "and do not modify source without an explicit human-approved pass.";
        packet.assistant_hint =
            "Treat compiler output, generated project files, captures, and logs as evidence. "
            "No evidence means no belief; promotion remains human-gated.";
        packet.operator_notes =
            "Generated by --editor-project-self-test so OS AI can learn from the real editor/tool loop instead of a silent build.";
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
        packet.provider_summary = epoch::ai::active_provider_summary();
        packet.active_model = epoch::ai::active_model_name();
        packet.manifest_path = manifest.manifest_path;
        packet.workspace_root = trainingPaths.workspace_root;
        packet.raw_capture_path = trainingPaths.local_capture_jsonl;
        packet.mcp_capture_path = trainingPaths.mcp_capture_jsonl;
        packet.checkpoint_root = trainingPaths.checkpoint_root;
        packet.model_root = trainingPaths.model_root;
        packet.cache_root = trainingPaths.cache_root;
        packet.curated_dataset_root = trainingPaths.curated_dataset_root;
        packet.eval_root = trainingPaths.eval_root;
        packet.evidence_paths = std::move(evidencePaths);

        const std::string packetPath = epoch::ai::stage_iteration_packet(packet);
        append_editor_project_self_test_note(
            ensured.root_path,
            build.succeeded ? "CLI Self-Iteration Self-Test Completed" : "CLI Self-Iteration Self-Test Blocked",
            verifierReady
                ? "Materialize, child build, and child self-test evidence staged."
                : build.succeeded ? childSelfTest.summary : "Materialize succeeded but child build failed; inspect the build log.",
            packetPath,
            epoch::ai::active_model_name(),
            ensured.manifest_path,
            profile == nullptr ? std::string_view{} : profile->scene_path,
            ensured.default_script_path,
            build.log_path,
            build.output_path);
        log_editor_self_test_line("editor_project_self_test.mcp_capture=" + trainingPaths.mcp_capture_jsonl);
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
            const auto result = epoch::ai::classify_helper_review_reply(test.reply);
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
            const bool actual = epoch::ai::is_promotable_assistant_reply(test.reply);
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

        for (const auto& profile : epochnamespace::editor_project_profiles())
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

    inline void prepare_parent_window_for_engine_capture(epochnamespace::core::MultiContextManager& mgr) noexcept
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
        const epochnamespace::core::WindowData* window,
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
        const epochnamespace::core::WindowData* window) noexcept
    {
        switch (window ? window->type : epochnamespace::core::ContextType::None)
        {
        case epochnamespace::core::ContextType::RayLib: return 0;
        case epochnamespace::core::ContextType::SDL: return 1;
        case epochnamespace::core::ContextType::SFML: return 2;
        case epochnamespace::core::ContextType::Vulkan: return 3;
        case epochnamespace::core::ContextType::OpenGL: return 4;
        case epochnamespace::core::ContextType::DirectX: return 5;
        case epochnamespace::core::ContextType::Software: return 6;
        default: return 99;
        }
    }

    inline void force_parent_window_capture_layout(
        epochnamespace::core::MultiContextManager& mgr) noexcept
    {
        const HWND parentWindow = mgr.GetParentWindow();
        if (!parentWindow || ::IsWindow(parentWindow) == FALSE)
            return;

        std::vector<epochnamespace::core::WindowData*> dockedWindows;
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
                if (auto liveContext = std::reinterpret_pointer_cast<epochnamespace::core::Context>(window->context))
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
        epochnamespace::core::MultiContextManager& mgr) noexcept
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

        std::vector<const epochnamespace::core::WindowData*> dockedWindows;
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
        epochnamespace::core::MultiContextManager& mgr,
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

        [[nodiscard]] std::unique_ptr<epochnamespace::scene::Scene> make_scene_from_id(std::string_view scene_id);
        [[nodiscard]] bool launch_project_child_process(std::string_view launch_argument);

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
            std::unique_ptr<epochnamespace::scene::Scene> active_scene{};

            auto collect_backend_contexts = []()
                {
                    using ContextGroup = std::pair<
                        epochnamespace::core::ContextType,
                        std::vector<std::shared_ptr<epochnamespace::core::Context>>
                    >;

                    std::vector<ContextGroup> snapshot;

                    {
                        std::shared_lock lock(epochnamespace::core::g_backendsMutex);
                        snapshot.reserve(epochnamespace::core::g_backends.size());

                        for (auto& [type, backendSlot] : epochnamespace::core::g_backends)
                        {
                            std::vector<std::shared_ptr<epochnamespace::core::Context>> contexts;
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
                    const auto& raylib_state = epochnamespace::raylibstate::s_raylibstate;
                    raylib_close_from_window = raylib_state.running && !raylib_state.renderingActive;

                    if (raylib_close_from_window)
                        epochnamespace::raylibstate::s_raylibstate.renderingActive = false;
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
                                    ctx->is_mouse_button_held_safe(epochnamespace::input::MouseButton::MouseLeft);
                                const bool up_pressed =
                                    epochnamespace::input::keyPressed.test(epochnamespace::input::Key::Up);
                                const bool down_pressed =
                                    epochnamespace::input::keyPressed.test(epochnamespace::input::Key::Down);
                                const bool left_pressed =
                                    epochnamespace::input::keyPressed.test(epochnamespace::input::Key::Left);
                                const bool right_pressed =
                                    epochnamespace::input::keyPressed.test(epochnamespace::input::Key::Right);
                                const bool enter_pressed =
                                    epochnamespace::input::keyPressed.test(epochnamespace::input::Key::Enter);

                                ctx->set_scene_preview_mode(core::ScenePreviewMode::Editor);
                                clear_before_ui_frame(ctx);
                                gui::begin_frame(ctx, dt, mouse_pos, mouse_left_down);
                                const auto editor_frame = epochnamespace::editor_run(ctx);

                                switch (editor_frame.command)
                                {
                                case epochnamespace::EditorCommand::OpenProject:
                                    logger::get(kEditorLog).logf(
                                        logger::LogLevel::INFO,
                                        std::source_location::current(),
                                        "Open Project: {}",
                                        editor_frame.command_argument);
                                    break;
                                case epochnamespace::EditorCommand::Settings:
                                    logger::get(kEditorLog).log(
                                        logger::LogLevel::INFO,
                                        "Settings selected.",
                                        std::source_location::current());
                                    break;
                                case epochnamespace::EditorCommand::RunGame:
                                    launch_requested_game(editor_frame.command_argument);
                                    break;
                                case epochnamespace::EditorCommand::Exit:
                                    state = EditorSceneState::Exit;
                                    running = false;
                                    break;
                                case epochnamespace::EditorCommand::None:
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
                                epochnamespace::cleanup_chat_context(raw);
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
                auto cleanup_backend = [&](std::shared_ptr<epochnamespace::core::Context> ctx)
                    {
                        if (!ctx) return;

                        epochnamespace::cleanup_chat_context(ctx.get());

                        switch (type)
                        {
#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)
                        case epochnamespace::core::ContextType::OpenGL:
                            epochnamespace::openglcontext::opengl_cleanup(ctx);
                            break;
#endif
#if defined(EPOCH_USING_SOFTWARE_RENDERER) && (EPOCH_USING_SOFTWARE_RENDERER == 1)
                        case epochnamespace::core::ContextType::Software:
                            // epochnamespace::anativecontext::softrenderer_cleanup(ctx);
                            break;
#endif
#if defined(EPOCH_USING_SDL) && (EPOCH_USING_SDL == 1)
                        case epochnamespace::core::ContextType::SDL:
                            //  epochnamespace::sdlcontext::sdl_cleanup(ctx);
                            break;
#endif
#if defined(EPOCH_USING_SFML) && (EPOCH_USING_SFML == 1)
                        case epochnamespace::core::ContextType::SFML:
                            epochnamespace::sfmlcontext::sfml_cleanup(ctx);
                            break;
#endif
#if defined(EPOCH_USING_RAYLIB) && (EPOCH_USING_RAYLIB == 1)
                        case epochnamespace::core::ContextType::RayLib:
                            epochnamespace::raylibcontext::raylib_cleanup(ctx);
                            break;
#endif
                        case epochnamespace::core::ContextType::Noop:
                            break;
                        default:
                            break;
                        }
                    };

                for (auto& ctx : contexts) cleanup_backend(ctx);
            }

            epochnamespace::shutdown_chat_system();
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
            std::unique_ptr<epochnamespace::scene::Scene> active_scene{};

            using MenuOverlay = epochnamespace::menu::MenuOverlay;
            MenuOverlay menu{};
            menu.set_max_columns(epochnamespace::core::cli::menu_columns);

            auto collect_backend_contexts = []()
                {
                    using ContextGroup = std::pair<
                        epochnamespace::core::ContextType,
                        std::vector<std::shared_ptr<epochnamespace::core::Context>>
                    >;

                    std::vector<ContextGroup> snapshot;

                    {
                        std::shared_lock lock(epochnamespace::core::g_backendsMutex);
                        snapshot.reserve(epochnamespace::core::g_backends.size());

                        for (auto& [type, backendSlot] : epochnamespace::core::g_backends)
                        {
                            std::vector<std::shared_ptr<epochnamespace::core::Context>> contexts;
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
                    const auto& raylib_state = epochnamespace::raylibstate::s_raylibstate;
                    raylib_close_from_window = raylib_state.running && !raylib_state.renderingActive;

                    if (raylib_close_from_window)
                        epochnamespace::raylibstate::s_raylibstate.renderingActive = false;
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
                                    ctx->is_mouse_button_held_safe(epochnamespace::input::MouseButton::MouseLeft);
                                const bool up_pressed =
                                    epochnamespace::input::keyPressed.test(epochnamespace::input::Key::Up);
                                const bool down_pressed =
                                    epochnamespace::input::keyPressed.test(epochnamespace::input::Key::Down);
                                const bool left_pressed =
                                    epochnamespace::input::keyPressed.test(epochnamespace::input::Key::Left);
                                const bool right_pressed =
                                    epochnamespace::input::keyPressed.test(epochnamespace::input::Key::Right);
                                const bool enter_pressed =
                                    epochnamespace::input::keyPressed.test(epochnamespace::input::Key::Enter);

                                ctx->clear_scene_viewport();
                                ctx->set_scene_preview_mode(core::ScenePreviewMode::None);
                                clear_before_ui_frame(ctx);
                                gui::begin_frame(ctx, dt, mouse_pos, mouse_left_down);
                                auto choice = menu.update_and_draw(ctx, win, dt, up_pressed, down_pressed, left_pressed, right_pressed, enter_pressed);
                                gui::end_frame();
                                ctx->present_safe();

                                if (choice)
                                {
                                    using epochnamespace::menu::Choice;

                                    if (*choice == Choice::Snake)
                                        begin_scene([] { return std::make_unique<epochnamespace::snakelike::SnakeLikeScene>(); }, SceneID::Snake);
                                    else if (*choice == Choice::Tetris)
                                        begin_scene([] { return std::make_unique<epochnamespace::tetrislike::TetrisLikeScene>(); }, SceneID::Tetris);
                                    else if (*choice == Choice::Frogger)
                                        begin_scene([] { return std::make_unique<epochnamespace::froggerlike::FroggerLikeScene>(); }, SceneID::Frogger);
                                    else if (*choice == Choice::Pacman)
                                        begin_scene([] { return std::make_unique<epochnamespace::pacmanlike::PacmanLikeScene>(); }, SceneID::Pacman);
                                    else if (*choice == Choice::Sokoban)
                                        begin_scene([] { return std::make_unique<epochnamespace::sokobanlike::SokobanLikeScene>(); }, SceneID::Sokoban);
                                    else if (*choice == Choice::Bejeweled)
                                        begin_scene([] { return std::make_unique<epochnamespace::match3like::Match3LikeScene>(); }, SceneID::Match3);
                                    else if (*choice == Choice::Puzzle)
                                        begin_scene([] { return std::make_unique<epochnamespace::slidinglike::SlidingPuzzleLikeScene>(); }, SceneID::Sliding);
                                    else if (*choice == Choice::Minesweep)
                                        begin_scene([] { return std::make_unique<epochnamespace::minesweeperlike::MinesweeperLikeScene>(); }, SceneID::Minesweeper);
                                    else if (*choice == Choice::Fourty)
                                        begin_scene([] { return std::make_unique<epochnamespace::a2048like::A2048LikeScene>(); }, SceneID::Game2048);
                                    else if (*choice == Choice::Sandsim)
                                        begin_scene([] { return std::make_unique<epochnamespace::sandsim::SandSimScene>(); }, SceneID::Sandsim);
                                    else if (*choice == Choice::Cellular)
                                        begin_scene([] { return std::make_unique<epochnamespace::cellularsim::CellularSimScene>(); }, SceneID::Cellular);
                                    else if (*choice == Choice::Settings)
                                        logger::get(kEngineLog).log(
                                            logger::LogLevel::INFO,
                                            "Menu settings selected.",
                                            std::source_location::current());
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
                                epochnamespace::cleanup_chat_context(raw);
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
                auto cleanup_backend = [&](std::shared_ptr<epochnamespace::core::Context> ctx)
                    {
                        if (!ctx) return;

                        epochnamespace::cleanup_chat_context(ctx.get());

                        switch (type)
                        {
#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)
                        case epochnamespace::core::ContextType::OpenGL:
                            epochnamespace::openglcontext::opengl_cleanup(ctx);
                            break;
#endif
#if defined(EPOCH_USING_SOFTWARE_RENDERER) && (EPOCH_USING_SOFTWARE_RENDERER == 1)
                        case epochnamespace::core::ContextType::Software:
                           // epochnamespace::anativecontext::softrenderer_cleanup(ctx);
                            break;
#endif
#if defined(EPOCH_USING_SDL) && (EPOCH_USING_SDL == 1)
                        case epochnamespace::core::ContextType::SDL:
                          //  epochnamespace::sdlcontext::sdl_cleanup(ctx);
                            break;
#endif
#if defined(EPOCH_USING_SFML) && (EPOCH_USING_SFML == 1)
                        case epochnamespace::core::ContextType::SFML:
                            epochnamespace::sfmlcontext::sfml_cleanup(ctx);
                            break;
#endif
#if defined(EPOCH_USING_RAYLIB) && (EPOCH_USING_RAYLIB == 1)
                        case epochnamespace::core::ContextType::RayLib:
                            epochnamespace::raylibcontext::raylib_cleanup(ctx);
                            break;
#endif


                        case epochnamespace::core::ContextType::Noop:
                            break;
                        default:
                            break;
                        }
                    };

                for (auto& ctx : contexts) cleanup_backend(ctx);
            }

            epochnamespace::shutdown_chat_system();
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
            epochnamespace::menu::MenuOverlay menu{};
            std::unique_ptr<epochnamespace::scene::Scene> active_scene{};
            timing::Clock::time_point last_frame{};
            bool has_last_frame{ false };
            epoch::core::time::simulation_clock simulation{};
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
            epochnamespace::previewgrid::CameraMode camera_mode{ epochnamespace::previewgrid::CameraMode::Editor };
            input::ProfilePreset input_profile{ input::ProfilePreset::EditorDefault };
        };

        [[nodiscard]] epochnamespace::previewgrid::CameraMode camera_mode_from_argument(std::string_view value) noexcept
        {
            if (value == "fps" || value == "first-person" || value == "first_person" || value == "runtime")
                return epochnamespace::previewgrid::CameraMode::FPS;
            if (value == "canvas2d" || value == "2d" || value == "2d-canvas" || value == "canvas")
                return epochnamespace::previewgrid::CameraMode::Canvas2D;
            return epochnamespace::previewgrid::CameraMode::Editor;
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
            epochnamespace::core::ContextType,
            std::vector<std::shared_ptr<epochnamespace::core::Context>>
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

        struct ProjectRuntimeEntity
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

        [[nodiscard]] std::vector<ProjectRuntimeEntity> project_runtime_entities_from_seeds(
            std::span<const epochnamespace::EditorSceneSeedEntity> seeds)
        {
            std::vector<ProjectRuntimeEntity> entities{};
            entities.reserve(seeds.size());
            for (const auto& seed : seeds)
            {
                entities.push_back(ProjectRuntimeEntity{
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
            return entities;
        }

        [[nodiscard]] std::filesystem::path engine_runtime_root()
        {
            if (const auto runtimeRoot = epoch::core::path::runtime_root_dir(); !runtimeRoot.empty())
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

        [[nodiscard]] std::vector<ProjectRuntimeEntity> load_project_runtime_entities(
            std::string_view scene_path,
            std::span<const epochnamespace::EditorSceneSeedEntity> fallback_seeds)
        {
            auto fallback = project_runtime_entities_from_seeds(fallback_seeds);
            if (scene_path.empty())
                return fallback;

            std::ifstream in(resolve_runtime_scene_path(std::filesystem::path{ scene_path }), std::ios::binary);
            if (!in)
                return fallback;

            std::vector<ProjectRuntimeEntity> loaded{};
            std::string line;
            while (std::getline(in, line))
            {
                std::istringstream row(line);
                std::string tag;
                row >> tag;
                if (tag != "entity")
                    continue;

                ProjectRuntimeEntity entity{};
                std::string posTag;
                std::string rotTag;
                std::string scaleTag;
                std::string visibleTag;
                std::string editorOnlyTag;
                int visible = 1;
                int editorOnly = 0;
                if (!(row
                    >> std::quoted(entity.name)
                    >> std::quoted(entity.type)
                    >> std::quoted(entity.category)
                    >> posTag >> entity.position[0] >> entity.position[1] >> entity.position[2]
                    >> rotTag >> entity.rotation[0] >> entity.rotation[1] >> entity.rotation[2]
                    >> scaleTag >> entity.scale[0] >> entity.scale[1] >> entity.scale[2]
                    >> visibleTag >> visible
                    >> editorOnlyTag >> editorOnly))
                {
                    continue;
                }

                if (posTag != "pos" || rotTag != "rot" || scaleTag != "scale" || visibleTag != "visible" || editorOnlyTag != "editor_only")
                    continue;

                entity.visible = visible != 0;
                entity.editor_only = editorOnly != 0;
                loaded.push_back(std::move(entity));
            }

            return loaded.empty() ? fallback : loaded;
        }

        [[nodiscard]] epochnamespace::previewgrid::Vec3 runtime_marker_color_for_entity(
            const ProjectRuntimeEntity& entity,
            bool selected) noexcept
        {
            if (selected)
                return { 1.00f, 0.86f, 0.24f };
            if (entity.editor_only || entity.category == "Editor")
                return { 0.44f, 0.62f, 0.90f };
            if (entity.type == "Light")
                return { 1.00f, 0.82f, 0.30f };
            if (entity.type == "Spawn")
                return { 0.34f, 0.94f, 0.62f };
            if (entity.category == "World" || entity.type == "Level")
                return { 0.70f, 0.78f, 0.90f };
            return { 0.95f, 0.62f, 0.28f };
        }

        [[nodiscard]] float runtime_marker_radius_for_entity(const ProjectRuntimeEntity& entity) noexcept
        {
            const float scaleMax = (std::max)(entity.scale[0], (std::max)(entity.scale[1], entity.scale[2]));
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

        [[nodiscard]] epochnamespace::previewgrid::ObjectPreviewPrimitive runtime_preview_primitive_for_entity(
            const ProjectRuntimeEntity& entity) noexcept
        {
            if (entity.type == "Light")
                return epochnamespace::previewgrid::ObjectPreviewPrimitive::Light;
            if (entity.type == "Spawn")
                return epochnamespace::previewgrid::ObjectPreviewPrimitive::Spawn;
            if (entity.type == "Camera")
                return epochnamespace::previewgrid::ObjectPreviewPrimitive::Camera;
            if (entity.category == "World" || entity.type == "Level")
                return epochnamespace::previewgrid::ObjectPreviewPrimitive::Level;
            return epochnamespace::previewgrid::ObjectPreviewPrimitive::Cube;
        }

        [[nodiscard]] std::size_t visible_runtime_entity_count(std::span<const ProjectRuntimeEntity> entities) noexcept
        {
            std::size_t count = 0;
            for (const auto& entity : entities)
                if (entity.visible)
                    ++count;
            return count;
        }

        void publish_project_play_markers(
            const epochnamespace::core::Context* ctx,
            std::span<const ProjectRuntimeEntity> entities)
        {
            if (!ctx)
                return;

            std::vector<epochnamespace::previewgrid::ObjectMarker> markers{};
            markers.reserve(entities.size());
            for (std::size_t i = 0; i < entities.size(); ++i)
            {
                const auto& entity = entities[i];
                if (!entity.visible)
                    continue;

                markers.push_back(epochnamespace::previewgrid::ObjectMarker{
                    .position{ entity.position[0], entity.position[1], entity.position[2] },
                    .color = runtime_marker_color_for_entity(entity, i == 0u),
                    .scale{ entity.scale[0], entity.scale[1], entity.scale[2] },
                    .radius = runtime_marker_radius_for_entity(entity),
                    .primitive = runtime_preview_primitive_for_entity(entity),
                    .selected = i == 0u,
                    .editorOnly = entity.editor_only || entity.category == "Editor"
                });
            }

            epochnamespace::previewgrid::set_object_markers(ctx, std::span<const epochnamespace::previewgrid::ObjectMarker>{
                markers.data(),
                markers.size()
            });
        }

        class ProjectPlayScene final : public epochnamespace::scene::Scene
        {
        public:
            explicit ProjectPlayScene(std::string_view project_payload)
            {
                const auto launch = parse_project_scene_launch(project_payload);
                m_cameraMode = launch.camera_mode;
                m_inputProfile = launch.input_profile;
                input::set_active_profile(m_inputProfile);

                const auto* profile = epochnamespace::editor_find_project_profile(launch.project_id);
                if (!profile)
                    profile = &epochnamespace::editor_default_project_profile();

                m_projectId = std::string(profile->id);
                m_projectName = std::string(profile->display_name);
                m_scenePath = std::string(profile->scene_path);
                m_worldName = std::string(profile->world_name);
                m_scriptName = std::string(profile->default_script);
                m_description = std::string(profile->description);
                m_modelSummary = epochnamespace::editor_project_model_summary(m_projectId);
                const auto seedEntities = epochnamespace::editor_seed_entities_for_project(m_projectId);
                m_entities = load_project_runtime_entities(
                    m_scenePath,
                    std::span<const epochnamespace::EditorSceneSeedEntity>{ seedEntities.data(), seedEntities.size() });
            }

            void load() override
            {
                Scene::load();
            }

            bool frame(std::shared_ptr<epochnamespace::core::Context> ctx, epochnamespace::core::WindowData*) override
            {
                if (!ctx)
                    return false;

                if (input::action_pressed(input::Action::Cancel))
                {
                    epochnamespace::previewgrid::clear_object_markers(ctx.get());
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
                    ctx->is_mouse_button_held_safe(epochnamespace::input::MouseButton::MouseLeft);
                const bool mouse_right_down =
                    ctx->is_mouse_button_held_safe(epochnamespace::input::MouseButton::MouseRight);

                const int width = (std::max)(1, ctx->width > 0 ? ctx->width : ctx->get_width_safe());
                const int height = (std::max)(1, ctx->height > 0 ? ctx->height : ctx->get_height_safe());
                const bool backendOwnsFrameClear =
                    ctx->type == core::ContextType::OpenGL;
                if (!backendOwnsFrameClear)
                    ctx->clear_safe();
                ctx->set_scene_preview_mode(core::ScenePreviewMode::Editor);
                if (!m_cameraApplied.contains(ctx.get()))
                {
                    epochnamespace::previewgrid::set_camera_mode(ctx.get(), m_cameraMode);
                    m_cameraApplied[ctx.get()] = true;
                }
                ctx->set_scene_viewport({ 0, 0, width, height });
                publish_project_play_markers(ctx.get(), std::span<const ProjectRuntimeEntity>{
                    m_entities.data(),
                    m_entities.size()
                });

                gui::begin_frame(ctx, dt, mouse_pos, mouse_left_down);

                const int wheelDelta = epochnamespace::gui::consume_mouse_wheel_delta();
                if (input::action_pressed(input::Action::ResetCamera))
                    epochnamespace::previewgrid::reset_camera(ctx.get());

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
                    epochnamespace::previewgrid::look_camera(
                        ctx.get(),
                        mouseDeltaX * sensitivity,
                        -mouseDeltaY * sensitivity);
                }
                else if (mouse_left_down && !mouse_right_down && m_lookState.panning)
                {
                    const float mouseDeltaX = mouse_pos.x - m_lookState.last_mouse.x;
                    const float mouseDeltaY = mouse_pos.y - m_lookState.last_mouse.y;
                    epochnamespace::previewgrid::pan_camera_drag(
                        ctx.get(),
                        mouseDeltaX,
                        -mouseDeltaY);
                }

                if (wheelDelta != 0)
                {
                    epochnamespace::previewgrid::zoom_camera(
                        ctx.get(),
                        (static_cast<float>(wheelDelta) / 120.0f) * input::wheel_zoom_step());
                }

                epochnamespace::previewgrid::step_camera(
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
            epochnamespace::EditorProjectModelSummary m_modelSummary{};
            std::vector<ProjectRuntimeEntity> m_entities{};
            timing::Clock::time_point m_lastFrame{};
            bool m_hasLastFrame{ false };
            epochnamespace::previewgrid::CameraMode m_cameraMode{ epochnamespace::previewgrid::CameraMode::Editor };
            input::ProfilePreset m_inputProfile{ input::ProfilePreset::EditorDefault };
            std::unordered_map<const void*, bool> m_cameraApplied{};
            PreviewLookState m_lookState{};
        };

        [[nodiscard]] std::vector<ContextGroup> collect_backend_contexts_shared()
        {
            std::vector<ContextGroup> snapshot;

            {
                std::shared_lock lock(epochnamespace::core::g_backendsMutex);
                snapshot.reserve(epochnamespace::core::g_backends.size());

                for (auto& [type, backendSlot] : epochnamespace::core::g_backends)
                {
                    std::vector<std::shared_ptr<epochnamespace::core::Context>> contexts;
                    contexts.reserve(1 + backendSlot.duplicates.size());

                    if (backendSlot.master) contexts.push_back(backendSlot.master);
                    for (auto& dup : backendSlot.duplicates) contexts.push_back(dup);

                    snapshot.emplace_back(type, std::move(contexts));
                }
            }

            return snapshot;
        }

        [[nodiscard]] std::unique_ptr<epochnamespace::scene::Scene> make_scene_from_id(std::string_view scene_id)
        {
            if (scene_id.starts_with("project:"))
                return std::make_unique<ProjectPlayScene>(scene_id.substr(8));
            if (scene_id == "snake")
                return std::make_unique<epochnamespace::snakelike::SnakeLikeScene>();
            if (scene_id == "tetris")
                return std::make_unique<epochnamespace::tetrislike::TetrisLikeScene>();
            if (scene_id == "frogger")
                return std::make_unique<epochnamespace::froggerlike::FroggerLikeScene>();
            if (scene_id == "pacman")
                return std::make_unique<epochnamespace::pacmanlike::PacmanLikeScene>();
            if (scene_id == "sokoban")
                return std::make_unique<epochnamespace::sokobanlike::SokobanLikeScene>();
            if (scene_id == "bejeweled" || scene_id == "match3")
                return std::make_unique<epochnamespace::match3like::Match3LikeScene>();
            if (scene_id == "puzzle" || scene_id == "sliding")
                return std::make_unique<epochnamespace::slidinglike::SlidingPuzzleLikeScene>();
            if (scene_id == "minesweep" || scene_id == "minesweeper")
                return std::make_unique<epochnamespace::minesweeperlike::MinesweeperLikeScene>();
            if (scene_id == "fourty" || scene_id == "2048")
                return std::make_unique<epochnamespace::a2048like::A2048LikeScene>();
            if (scene_id == "sandsim" || scene_id == "sand")
                return std::make_unique<epochnamespace::sandsim::SandSimScene>();
            if (scene_id == "cellular" || scene_id == "cell")
                return std::make_unique<epochnamespace::cellularsim::CellularSimScene>();
            return {};
        }

        [[nodiscard]] std::string_view scene_id_from_choice(epochnamespace::menu::Choice choice) noexcept
        {
            using Choice = epochnamespace::menu::Choice;

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
            case Choice::ProjectTwoDStudio:
            case Choice::About:
            case Choice::UpdateLatest:
            case Choice::Exit:
            default:
                return {};
            }
        }

        [[nodiscard]] std::string_view project_id_from_choice(epochnamespace::menu::Choice choice) noexcept
        {
            using Choice = epochnamespace::menu::Choice;

            switch (choice)
            {
            case Choice::ProjectTwoDStudio: return "twodstudio";
            default: return {};
            }
        }

        [[nodiscard]] epochnamespace::updater::UpdateChannel default_update_channel()
        {
            return epochnamespace::updater::UpdateChannel{
                .version_url = epochnamespace::updater::PROJECT_PACKAGED_VERSION_URL(),
                .binary_url = epochnamespace::updater::PROJECT_BINARY_URL(),
                .source_url = epochnamespace::updater::PROJECT_SOURCE_URL(),
                .source_version_url = epochnamespace::updater::PROJECT_SOURCE_VERSION_URL(),
                .platform_build_status_url = epochnamespace::updater::PROJECT_ACTION_RUNS_API_URL(),
                .platform_build_job_name = epochnamespace::updater::PROJECT_UPDATE_BUILD_JOB_NAME(),
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
            session.menu.set_max_columns(epochnamespace::core::cli::menu_columns);
            session.menu.initialize(ctx);
        }

        void reset_to_menu(ContextSession& session, const std::shared_ptr<Context>& ctx)
        {
            epochnamespace::editor_reset_transient_ui(ctx.get());
            session.menu.cleanup();
            ensure_menu_initialized(session, ctx);
            session.mode = SessionMode::Menu;
            session.return_mode = SessionMode::Menu;
            g_preview_look_states.erase(ctx.get());
        }

        void cleanup_backend_context_shared(epochnamespace::core::ContextType type,
            std::shared_ptr<epochnamespace::core::Context> ctx)
        {
            if (!ctx) return;

            (void)type;
            epochnamespace::gui::cleanup_context(ctx.get());
            epochnamespace::cleanup_chat_context(ctx.get());
        }

        template <typename PumpFunc>
        int RunContextSessionLoop(MultiContextManager& mgr, PumpFunc&& pump_events, SessionMode startup_mode)
        {
            std::unordered_map<Context*, ContextSession> sessions;
            bool running = true;
            bool deferred_updater_shell_update = false;
            bool smoke_capture_taken = false;
            bool smoke_capture_armed = false;
            std::uint64_t frame_count = 0;
            const std::uint64_t smoke_max_frames = smoke_frame_budget();
            const std::uint64_t smoke_capture_frame =
                cli::smoke_requested
                ? (cli::capture_requested ? 420u : 30u)
                : 0u;
            const std::uint64_t smoke_capture_settle_frames =
                cli::capture_requested ? 90u : 0u;
            const std::uint64_t smoke_capture_fallback_frames =
                cli::capture_requested ? 150u : 0u;
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

                if (!mgr.IsRunning())
                {
                    running = false;
                    break;
                }

                mgr.CleanupFinishedWindows();

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

                if (!has_live_native_window)
                {
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
                    const auto& raylib_state = epochnamespace::raylibstate::s_raylibstate;
                    raylib_close_from_window = raylib_state.running && !raylib_state.renderingActive;

                    if (raylib_close_from_window)
                        epochnamespace::raylibstate::s_raylibstate.renderingActive = false;
                }
#endif
#endif

                auto switch_all_sessions_to_editor = [&](std::string_view project_id)
                {
                    for (auto& [_, contexts] : snapshot)
                    {
                        for (auto& targetCtx : contexts)
                        {
                            if (!targetCtx)
                                continue;

                            auto [targetIt, insertedForEditor] = sessions.try_emplace(targetCtx.get());
                            auto& targetSession = targetIt->second;
                            if (insertedForEditor)
                                targetSession.menu.set_max_columns(epochnamespace::core::cli::menu_columns);

                            unload_active_scene(targetSession);
                            targetSession.menu.cleanup();
                            targetSession.mode = SessionMode::Editor;
                            targetSession.return_mode = SessionMode::Menu;

                            if (!project_id.empty())
                                epochnamespace::editor_load_project(targetCtx, project_id);
                            else
                                epochnamespace::editor_reset_transient_ui(targetCtx.get());

                            targetCtx->clear_scene_viewport();
                            targetCtx->set_scene_preview_mode(core::ScenePreviewMode::Editor);
                        }
                    }
                };

                for (auto& [type, contexts] : snapshot)
                {
                    bool backend_has_live_context = false;

                    for (auto& ctx : contexts)
                    {
                        if (!ctx) continue;

                        auto* win = mgr.findWindowByContext(ctx);
                        if (!win)
                        {
                            auto it = sessions.find(ctx.get());
                            if (it != sessions.end())
                            {
                                unload_active_scene(it->second);
                                it->second.menu.cleanup();
                                sessions.erase(it);
                            }
                            epochnamespace::gui::cleanup_context(ctx.get());
                            epochnamespace::cleanup_chat_context(ctx.get());
                            continue;
                        }

                        auto [it, inserted] = sessions.try_emplace(ctx.get());
                        auto& session = it->second;

                        if (inserted)
                        {
                            session.mode = startup_mode;
                            session.return_mode = startup_mode;
                            session.menu.set_max_columns(epochnamespace::core::cli::menu_columns);

                            if (startup_mode == SessionMode::Menu)
                                ensure_menu_initialized(session, ctx);

                            std::string startup_scene_name = epochnamespace::core::cli::scene_name;
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
                        }

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
                            const auto control = epochnamespace::editor_time_control(ctx.get());
                            session.simulation.set_paused(control.paused);
                            session.simulation.set_time_scale(control.time_scale);
                            session.simulation.set_max_steps_per_frame(control.max_steps_per_frame);
                            session.simulation.set_fixed_dt_seconds(control.fixed_dt_seconds);
                            session.simulation.tick(dt);

                            const auto stepBudget = session.simulation.step_budget();
                            if (stepBudget > 0)
                                session.simulation.consume_steps(stepBudget);
                            if (control.step_once)
                                epochnamespace::editor_consume_time_step_request(ctx.get());
                        };

                        auto publish_time_snapshot = [&]()
                        {
                            const auto stats = session.simulation.stats();
                            epochnamespace::editor_set_time_snapshot(ctx.get(), epochnamespace::EditorTimeSnapshot{
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
                                ctx->is_mouse_button_held_safe(epochnamespace::input::MouseButton::MouseLeft);
                            const bool mouse_right_down =
                                ctx->is_mouse_button_held_safe(epochnamespace::input::MouseButton::MouseRight);

                            ctx->set_scene_preview_mode(core::ScenePreviewMode::Editor);
                            clear_before_ui_frame(ctx);
                            gui::begin_frame(ctx, dt, mouse_pos, mouse_left_down);
                            const auto editor_frame = epochnamespace::editor_run(ctx);
                            publish_time_snapshot();

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
                                    const int wheelDelta = epochnamespace::gui::consume_mouse_wheel_delta();
                                    const float forwardInput =
                                        (epochnamespace::input::action_held(epochnamespace::input::Action::MoveForward) ? 1.0f : 0.0f)
                                        - (epochnamespace::input::action_held(epochnamespace::input::Action::MoveBackward) ? 1.0f : 0.0f);
                                    const float rightInput =
                                        (epochnamespace::input::action_held(epochnamespace::input::Action::MoveRight) ? 1.0f : 0.0f)
                                        - (epochnamespace::input::action_held(epochnamespace::input::Action::MoveLeft) ? 1.0f : 0.0f);
                                    const float upInput =
                                        (epochnamespace::input::action_held(epochnamespace::input::Action::MoveUp) ? 1.0f : 0.0f)
                                        - (epochnamespace::input::action_held(epochnamespace::input::Action::MoveDown) ? 1.0f : 0.0f);
                                    const float yawInput =
                                        (epochnamespace::input::action_held(epochnamespace::input::Action::LookRight) ? 1.0f : 0.0f)
                                        - (epochnamespace::input::action_held(epochnamespace::input::Action::LookLeft) ? 1.0f : 0.0f);
                                    const float pitchInput =
                                        (epochnamespace::input::action_held(epochnamespace::input::Action::LookUp) ? 1.0f : 0.0f)
                                        - (epochnamespace::input::action_held(epochnamespace::input::Action::LookDown) ? 1.0f : 0.0f);

                                    if (epochnamespace::input::action_pressed(epochnamespace::input::Action::ResetCamera))
                                        epochnamespace::previewgrid::reset_camera(ctx.get());

                                    if (mouse_right_down && look_state.looking)
                                    {
                                        const float mouseDeltaX = mouse_pos.x - look_state.last_mouse.x;
                                        const float mouseDeltaY = mouse_pos.y - look_state.last_mouse.y;
                                        const float mouseSensitivity = epochnamespace::input::mouse_look_sensitivity();
                                        epochnamespace::previewgrid::look_camera(
                                            ctx.get(),
                                            mouseDeltaX * mouseSensitivity,
                                            -mouseDeltaY * mouseSensitivity);
                                    }
                                    else if (mouse_left_down && !mouse_right_down && look_state.panning)
                                    {
                                        const float mouseDeltaX = mouse_pos.x - look_state.last_mouse.x;
                                        const float mouseDeltaY = mouse_pos.y - look_state.last_mouse.y;
                                        epochnamespace::previewgrid::pan_camera_drag(
                                            ctx.get(),
                                            mouseDeltaX,
                                            -mouseDeltaY);
                                    }

                                    if (wheelDelta != 0)
                                    {
                                        epochnamespace::previewgrid::zoom_camera(
                                            ctx.get(),
                                            (static_cast<float>(wheelDelta) / 120.0f) * epochnamespace::input::wheel_zoom_step());
                                    }

                                    epochnamespace::previewgrid::step_camera(
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
                            case epochnamespace::EditorCommand::OpenLauncher:
                                reset_to_menu(session, ctx);
                                ctx_running = true;
                                break;
                            case epochnamespace::EditorCommand::RunGame:
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
                            case epochnamespace::EditorCommand::RunScript:
                            {
                                if (editor_frame.command_argument.empty())
                                {
                                    logger::get(kEditorLog).log(
                                        logger::LogLevel::Error,
                                        "Editor rejected an empty script command; select a project-backed script action first.",
                                        std::source_location::current());
                                    break;
                                }
                                const bool ok = epochnamespace::editor_run_script(
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
                            case epochnamespace::EditorCommand::UpdateApplication:
                            {
                                logger::get(kEditorLog).log(
                                    logger::LogLevel::INFO,
                                    "Running confirmed smart update command.",
                                    std::source_location::current());
                                const auto result = epochnamespace::updater::run_update_command(
                                    default_update_channel(),
                                    true);
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
                                else if (!result.update_performed)
                                {
                                    logger::get(kEditorLog).log(
                                        logger::LogLevel::Error,
                                        "Update was available but the handoff/install step did not complete.",
                                        std::source_location::current());
                                }
                                break;
                            }
                            case epochnamespace::EditorCommand::UpdateApplicationFromSource:
                            {
                                logger::get(kEditorLog).log(
                                    logger::LogLevel::INFO,
                                    "Running confirmed advanced source rebuild command.",
                                    std::source_location::current());
                                const bool ok = epochnamespace::updater::run_source_update_command(
                                    default_update_channel());
                                if (!ok)
                                {
                                    logger::get(kEditorLog).log(
                                        logger::LogLevel::Error,
                                        "Advanced source rebuild did not complete.",
                                        std::source_location::current());
                                }
                                break;
                            }
                            case epochnamespace::EditorCommand::Exit:
                                session.mode = SessionMode::Exit;
                                ctx_running = false;
                                win->running = false;
                                break;
                            case epochnamespace::EditorCommand::OpenProject:
                            case epochnamespace::EditorCommand::Settings:
                            case epochnamespace::EditorCommand::None:
                            default:
                                break;
                            }

                            gui::end_frame();
                            if (ctx_running)
                                ctx->present_safe();
                            break;
                        }

                        case SessionMode::Menu:
                        {
                            ensure_menu_initialized(session, ctx);

                            int mx = 0;
                            int my = 0;
                            ctx->get_mouse_position_safe(mx, my);

                            const gui::Vec2 mouse_pos{
                                static_cast<float>(mx),
                                static_cast<float>(my)
                            };

                            const bool mouse_left_down =
                                ctx->is_mouse_button_held_safe(epochnamespace::input::MouseButton::MouseLeft);
                            const bool up_pressed =
                                epochnamespace::input::keyPressed.test(epochnamespace::input::Key::Up);
                            const bool down_pressed =
                                epochnamespace::input::keyPressed.test(epochnamespace::input::Key::Down);
                            const bool left_pressed =
                                epochnamespace::input::keyPressed.test(epochnamespace::input::Key::Left);
                            const bool right_pressed =
                                epochnamespace::input::keyPressed.test(epochnamespace::input::Key::Right);
                            const bool enter_pressed =
                                epochnamespace::input::keyPressed.test(epochnamespace::input::Key::Enter);

                            ctx->clear_scene_viewport();
                            ctx->set_scene_preview_mode(core::ScenePreviewMode::None);
                            clear_before_ui_frame(ctx);
                            gui::begin_frame(ctx, dt, mouse_pos, mouse_left_down);
                            auto choice = session.menu.update_and_draw(
                                ctx,
                                win,
                                dt,
                                up_pressed,
                                down_pressed,
                                left_pressed,
                                right_pressed,
                                enter_pressed);
                            gui::end_frame();

                            bool suppress_menu_present = false;
                            if (choice)
                            {
                                if (*choice == epochnamespace::menu::Choice::Exit)
                                {
                                    suppress_menu_present = true;
                                    session.mode = SessionMode::Exit;
                                    ctx_running = false;
                                    win->running = false;
                                }
                                else if (*choice == epochnamespace::menu::Choice::UpdateLatest)
                                {
                                    suppress_menu_present = true;
                                    logger::get(kEditorLog).log(
                                        logger::LogLevel::INFO,
                                        "Updater shell requested the newest packaged Epoch release, then main source if it is still newer afterward.",
                                        std::source_location::current());
                                    logger::get(kEditorLog).log(
                                        logger::LogLevel::INFO,
                                        "Closing updater shell window and continuing the update in the console.",
                                        std::source_location::current());
                                    deferred_updater_shell_update = true;
                                    session.mode = SessionMode::Exit;
                                    ctx_running = false;
                                    win->running = false;
                                }
                                else if (*choice == epochnamespace::menu::Choice::OpenEditor)
                                {
                                    switch_all_sessions_to_editor("projectlauncher");
                                }
                                else if (const auto project_id = project_id_from_choice(*choice); !project_id.empty())
                                {
                                    switch_all_sessions_to_editor(project_id);
                                }
                                else if (*choice == epochnamespace::menu::Choice::Settings)
                                {
                                    logger::get(kEditorLog).log(
                                        logger::LogLevel::INFO,
                                        "Launcher contexts/settings selected.",
                                        std::source_location::current());
                                }
                                else if (*choice == epochnamespace::menu::Choice::About)
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
                            ctx->clear_scene_viewport();
                            ctx->set_scene_preview_mode(core::ScenePreviewMode::None);
                            unload_active_scene(session);
                            session.menu.cleanup();
                            epochnamespace::gui::cleanup_context(ctx.get());
                            epochnamespace::cleanup_chat_context(ctx.get());
                            g_preview_look_states.erase(ctx.get());
                            sessions.erase(ctx.get());
                        }
                        else
                        {
                            backend_has_live_context = true;
                        }
                    }

                    if (backend_has_live_context)
                        any_context_alive = true;
                }

                if (!any_context_alive)
                {
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
                    }
#endif
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(16));
            }

            for (auto& [_, session] : sessions)
            {
                unload_active_scene(session);
                session.menu.cleanup();
            }
            g_preview_look_states.clear();

            auto snapshot2 = collect_backend_contexts_shared();
            for (auto& [type, contexts] : snapshot2)
            {
                for (auto& ctx : contexts)
                    cleanup_backend_context_shared(type, ctx);
            }

            epochnamespace::shutdown_chat_system();
            mgr.StopAll();

                if (deferred_updater_shell_update)
                {
                    const auto result = epochnamespace::updater::run_update_command(
                        default_update_channel(),
                        true);
                    if (result.platform_build_checked && !result.platform_build_ok)
                    {
                        const std::string reason = result.platform_build_reason.empty()
                            ? std::string{ "platform build status is not green." }
                            : result.platform_build_reason;
                        logger::get(kEditorLog).logf(
                            logger::LogLevel::Error,
                            std::source_location::current(),
                            "Updater shell withheld update until {} is green: {}",
                            result.platform_build_job.empty() ? "platform build" : result.platform_build_job,
                            reason);
                    }
                    else if (!result.update_available)
                    {
                        if (result.source_update_available)
                        {
                            logger::get(kEditorLog).log(
                                logger::LogLevel::INFO,
                                "A newer source snapshot remains available on main. Run Update again after the packaged restart to continue from source.",
                                std::source_location::current());
                        }
                        else
                        {
                            logger::get(kEditorLog).log(
                                logger::LogLevel::INFO,
                                "Updater shell is already on the newest packaged or source build available from GitHub.",
                                std::source_location::current());
                        }
                    }
                    else if (!result.update_performed)
                    {
                        logger::get(kEditorLog).log(
                            logger::LogLevel::Error,
                        "Updater shell found an update but the install handoff did not complete.",
                        std::source_location::current());
                }
            }

            return 0;
        }

        template <typename PumpFunc>
        int RunEngineMainLoopCommon(MultiContextManager& mgr, PumpFunc&& pump_events)
        {
            const auto startup_mode = SessionMode::Menu;
            return RunContextSessionLoop(mgr, std::forward<PumpFunc>(pump_events), startup_mode);
        }

#if defined(_WIN32)
        int RunEngineMainLoopInternal(HINSTANCE hInstance, int nCmdShow)
        {
            UNREFERENCED_PARAMETER(nCmdShow);

            try
            {
                epochnamespace::core::MultiContextManager mgr;

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
                        MSG msg{};
                        bool keep = true;

                        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
                        {
                            if (msg.message == WM_QUIT) keep = false;
                            else
                            {
                                TranslateMessage(&msg);
                                DispatchMessageW(&msg);
                            }
                        }

                        if (!keep) return false;

                        input::poll_input();
                        return true;
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
                epochnamespace::core::MultiContextManager mgr;

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

                if (epochnamespace::core::cli::smoke_requested)
                {
                    std::this_thread::sleep_for(smoke_shutdown_delay());
                    mgr.StopAll();
                    return 0;
                }

                auto pump = []() -> bool
                    {
                        return epochnamespace::platform::pump_events();
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
        logger::get(engine::kEngineLog).log(
            logger::LogLevel::Error,
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
            epochnamespace::GetEngineVersion());
        RunEngine();
    }

    void RunEditorInterface()
    {
#if defined(_WIN32)
        try
        {
            epochnamespace::core::MultiContextManager mgr;

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
                    MSG msg{};
                    bool keep = true;

                    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
                    {
                        if (msg.message == WM_QUIT) keep = false;
                        else
                        {
                            TranslateMessage(&msg);
                            DispatchMessageW(&msg);
                        }
                    }

                    if (!keep) return false;

                    input::poll_input();
                    return true;
                };

            const auto initial_mode =
                epochnamespace::core::cli::editor_requested
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
            epochnamespace::core::MultiContextManager mgr;

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
                    return epochnamespace::platform::pump_events();
                };

            const auto initial_mode =
                epochnamespace::core::cli::editor_requested
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

    namespace bridge
    {
        int run_legacy_runtime(bool editor_mode)
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
                "Legacy bridge runtime is not implemented for this platform yet.",
                std::source_location::current());
            return -1;
#endif
        }
    }
} // namespace epochnamespace::core


#if !defined(EPOCH_MAIN_IN_MAIN_CPP)
namespace urls
{
    const std::string github_base = "https://github.com/";
    const std::string github_raw_base = "https://raw.githubusercontent.com/";

    const std::string owner = "Autodidac/";
    const std::string repo = "EpochEngine";
    const std::string branch = "main/";

    const std::string version_url = epochnamespace::updater::PROJECT_PACKAGED_VERSION_URL();
    const std::string binary_url = epochnamespace::updater::PROJECT_BINARY_URL();
    const std::string source_url = epochnamespace::updater::PROJECT_SOURCE_URL();
    const std::string source_version_url = epochnamespace::updater::PROJECT_SOURCE_VERSION_URL();
}

#if defined(_WIN32)
namespace
{
    void configure_unattended_windows_error_mode()
    {
        ::SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);

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
    epochnamespace::core::ShowConsole();
#endif

    try
    {
        const int argc = __argc;
        char** argv = __argv;

        const auto cli_result = epochnamespace::core::cli::parse(argc, argv);

        if (cli_result.version_requested && !cli_result.update_requested)
            return 0;

        if (cli_result.editor_ai_gate_self_test_requested)
            return epochnamespace::core::run_editor_ai_gate_self_test();

        if (cli_result.editor_project_self_test_requested)
            return epochnamespace::core::run_editor_project_self_test(cli_result.editor_project_self_test_id);

        if (cli_result.engine_contract_self_test_requested)
            return epochnamespace::core::run_engine_contract_self_test();

        if (cli_result.engine_validation_self_test_requested)
            return epochnamespace::core::run_engine_validation_self_test();

        const epochnamespace::updater::UpdateChannel channel{
            .version_url = urls::version_url,
            .binary_url = urls::binary_url,
            .source_url = urls::source_url,
            .source_version_url = urls::source_version_url,
        };

        if (cli_result.update_requested)
        {
            const auto update_result =
                epochnamespace::updater::run_update_command(channel, cli_result.force_update);

            if (update_result.force_required && !cli_result.force_update)
                return 2;

            if (cli_result.force_update
                && update_result.update_available
                && !update_result.update_performed)
            {
                return 1;
            }

            return 0;
        }

        if (cli_result.editor_requested)
        {
            epochnamespace::core::RunEditorInterface();
            return 0;
        }

        return epochnamespace::core::engine::RunEngineMainLoopInternal(hInstance, SW_SHOWNORMAL);
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
        const auto cli_result = epochnamespace::core::cli::parse(argc, argv);

        if (cli_result.version_requested && !cli_result.update_requested)
            return 0;

        if (cli_result.editor_ai_gate_self_test_requested)
            return epochnamespace::core::run_editor_ai_gate_self_test();

        if (cli_result.editor_project_self_test_requested)
            return epochnamespace::core::run_editor_project_self_test(cli_result.editor_project_self_test_id);

        if (cli_result.engine_contract_self_test_requested)
            return epochnamespace::core::run_engine_contract_self_test();

        if (cli_result.engine_validation_self_test_requested)
            return epochnamespace::core::run_engine_validation_self_test();

        const epochnamespace::updater::UpdateChannel channel{
            .version_url = urls::version_url,
            .binary_url = urls::binary_url,
            .source_url = urls::source_url,
            .source_version_url = urls::source_version_url,
        };

        if (cli_result.update_requested)
        {
            const auto update_result =
                epochnamespace::updater::run_update_command(channel, cli_result.force_update);

            if (update_result.force_required && !cli_result.force_update)
                return 2;

            if (cli_result.force_update
                && update_result.update_available
                && !update_result.update_performed)
            {
                return 1;
            }

            return 0;
        }

        if (cli_result.editor_requested)
        {
            epochnamespace::core::RunEditorInterface();
            return 0;
        }

        epochnamespace::core::StartEngine();
        return 0;
    }
    catch (const std::exception& ex)
    {
        logger::get(engine::kEngineLog).log(
            logger::LogLevel::Error,
            ex.what(),
            std::source_location::current());
        return -1;
    }
#endif
}

#endif // !defined(EPOCH_MAIN_IN_MAIN_CPP)
