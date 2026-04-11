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
 // aengine.cppm (converted from legacy aengine.cpp)
 //
 // FIXES APPLIED:
 //  - No direct access to core::Context private members (ctx->hwnd).
 //    We only query windows via MultiContextManager APIs.
 //  - Removed non-constant switch case labels for ContextType::Unknown/Noop
 //    because your ContextType in your current modules is not an enum with those
 //    exact enumerators (or theyÃ¢â‚¬â„¢re not visible here). Default handles it.
 //
//#include "pch.h"

#include "../include/aengine.config.hpp"
#include "../include/aengine.hpp"

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
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <shared_mutex>
#include <source_location>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

// -----------------------------
// Engine/module imports
// -----------------------------
import aengine.platform;
//import aengine.config;

import epochengine;

import aengine.cli;
import aengine.version;
import aengine.updater;
import aengine.input;
import engine.components;

import context.multiplexer;
import context.type;
import context.window;
import core.context;
import core.logger;
import core.time;
import core.timer;

import aengine.gui;
import gui.menu;
import aeditor;
import render.preview_grid;

import ascene;

import asnakelike;
import atetrislike;
import apacmanlike;
import afroggerlike;
import asokobanlike;
import amatch3like;

import aslidingpuzzlelike;
import aminesweeperlike;
import a2048like;

import asandsim;
import acellularsim;

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
    namespace bridge
    {
        int run_legacy_runtime(bool editor_mode);
    }

    struct LegacyLaunchConfig
    {
        int raylib_count = 1;
        int sdl_count = 1;
        int sfml_count = 1;
        int vulkan_count = 1;
        int opengl_count = 1;
        int software_count = 1;
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
        cfg.software_count = (std::max)(0, cli::software_window_count);
        cfg.parented = cli::parented_mode;

        const int total_requested =
            cfg.raylib_count +
            cfg.sdl_count +
            cfg.sfml_count +
            cfg.vulkan_count +
            cfg.opengl_count +
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
        return cli::smoke_requested ? 300u : (std::numeric_limits<std::uint64_t>::max)();
    }

    [[nodiscard]] inline auto smoke_shutdown_delay() noexcept
    {
        return cli::capture_requested
            ? std::chrono::milliseconds(650)
            : std::chrono::milliseconds(100);
    }

#if defined(_WIN32)
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

    [[nodiscard]] inline bool capture_client_window_to_bmp(HWND hwnd, const std::filesystem::path& filepath) noexcept
    {
        if (!hwnd)
            return false;

        RECT clientRect{};
        if (!::GetClientRect(hwnd, &clientRect))
            return false;

        const int width = clientRect.right - clientRect.left;
        const int height = clientRect.bottom - clientRect.top;
        if (width <= 0 || height <= 0)
            return false;

        POINT clientOrigin{};
        if (!::ClientToScreen(hwnd, &clientOrigin))
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
            clientOrigin.x,
            clientOrigin.y,
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
        if (::SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0))
        {
            ::ShowWindow(parentWindow, SW_RESTORE);
            ::SetWindowPos(
                parentWindow,
                HWND_TOP,
                workArea.left,
                workArea.top,
                workArea.right - workArea.left,
                workArea.bottom - workArea.top,
                SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_SHOWWINDOW);
        }

        ::BringWindowToTop(parentWindow);
        ::SetForegroundWindow(parentWindow);
        ::SetActiveWindow(parentWindow);
        mgr.ArrangeDockedWindowsGrid();
        ::UpdateWindow(parentWindow);
        ::RedrawWindow(parentWindow, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
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

        const bool captured = capture_client_window_to_bmp(parentWindow, capturePath);
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

                        for (auto& [type, state] : epochnamespace::core::g_backends)
                        {
                            std::vector<std::shared_ptr<epochnamespace::core::Context>> contexts;
                            contexts.reserve(1 + state.duplicates.size());

                            if (state.master) contexts.push_back(state.master);
                            for (auto& dup : state.duplicates) contexts.push_back(dup);

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
                                ctx->clear_safe();
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

                        for (auto& [type, state] : epochnamespace::core::g_backends)
                        {
                            std::vector<std::shared_ptr<epochnamespace::core::Context>> contexts;
                            contexts.reserve(1 + state.duplicates.size());

                            if (state.master) contexts.push_back(state.master);
                            for (auto& dup : state.duplicates) contexts.push_back(dup);

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
                                ctx->clear_safe();
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

        using ContextGroup = std::pair<
            epochnamespace::core::ContextType,
            std::vector<std::shared_ptr<epochnamespace::core::Context>>
        >;

        class ProjectPlayScene final : public epochnamespace::scene::Scene
        {
        public:
            explicit ProjectPlayScene(std::string_view project_id)
                : m_projectId(project_id)
            {
                const auto* profile = epochnamespace::editor_find_project_profile(project_id);
                if (!profile)
                    profile = &epochnamespace::editor_default_project_profile();

                m_projectId = std::string(profile->id);
                m_projectName = std::string(profile->display_name);
                m_scenePath = std::string(profile->scene_path);
                m_worldName = std::string(profile->world_name);
                m_scriptName = std::string(profile->default_script);
                m_description = std::string(profile->description);
            }

            void load() override
            {
                Scene::load();
            }

            bool frame(std::shared_ptr<epochnamespace::core::Context> ctx, epochnamespace::core::WindowData*) override
            {
                if (!ctx)
                    return false;

                if (ctx->is_key_down_safe(input::Key::Escape))
                    return false;

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
                ctx->clear_safe();
                ctx->set_scene_preview_mode(core::ScenePreviewMode::Editor);
                ctx->set_scene_viewport({ 0, 0, width, height });

                gui::begin_frame(ctx, dt, mouse_pos, mouse_left_down);

                const int wheelDelta = epochnamespace::gui::consume_mouse_wheel_delta();
                const float forwardInput =
                    (ctx->is_key_held_safe(epochnamespace::input::Key::W) ? 1.0f : 0.0f)
                    - (ctx->is_key_held_safe(epochnamespace::input::Key::S) ? 1.0f : 0.0f);
                const float rightInput =
                    (ctx->is_key_held_safe(epochnamespace::input::Key::D) ? 1.0f : 0.0f)
                    - (ctx->is_key_held_safe(epochnamespace::input::Key::A) ? 1.0f : 0.0f);
                const float upInput =
                    (ctx->is_key_held_safe(epochnamespace::input::Key::E) ? 1.0f : 0.0f)
                    - (ctx->is_key_held_safe(epochnamespace::input::Key::Q) ? 1.0f : 0.0f);
                const float yawInput =
                    (ctx->is_key_held_safe(epochnamespace::input::Key::Right) ? 1.0f : 0.0f)
                    - (ctx->is_key_held_safe(epochnamespace::input::Key::Left) ? 1.0f : 0.0f);
                const float pitchInput =
                    (ctx->is_key_held_safe(epochnamespace::input::Key::Up) ? 1.0f : 0.0f)
                    - (ctx->is_key_held_safe(epochnamespace::input::Key::Down) ? 1.0f : 0.0f);

                if (mouse_right_down && m_looking)
                {
                    const float mouseDeltaX = mouse_pos.x - m_lastMouse.x;
                    const float mouseDeltaY = mouse_pos.y - m_lastMouse.y;
                    constexpr float kMouseSensitivity = 0.20f;
                    epochnamespace::previewgrid::look_camera(
                        ctx.get(),
                        mouseDeltaX * kMouseSensitivity,
                        -mouseDeltaY * kMouseSensitivity);
                }
                else if (mouse_left_down && !mouse_right_down && m_panning)
                {
                    const float mouseDeltaX = mouse_pos.x - m_lastMouse.x;
                    const float mouseDeltaY = mouse_pos.y - m_lastMouse.y;
                    epochnamespace::previewgrid::pan_camera_drag(
                        ctx.get(),
                        mouseDeltaX,
                        -mouseDeltaY);
                }

                if (wheelDelta != 0)
                {
                    constexpr float kWheelZoomStep = 1.3f;
                    epochnamespace::previewgrid::zoom_camera(
                        ctx.get(),
                        (static_cast<float>(wheelDelta) / 120.0f) * kWheelZoomStep);
                }

                epochnamespace::previewgrid::step_camera(
                    ctx.get(),
                    dt,
                    forwardInput,
                    rightInput,
                    upInput,
                    yawInput,
                    pitchInput);

                m_lastMouse = mouse_pos;
                m_looking = mouse_right_down;
                m_panning = mouse_left_down && !mouse_right_down;

                gui::begin_window("Project Runtime", { 24.0f, 24.0f }, { 430.0f, 210.0f });
                gui::label(std::string("Project: ") + m_projectName);
                gui::label(std::string("World: ") + m_worldName);
                gui::label(std::string("Scene: ") + m_scenePath);
                gui::label(std::string("Script: ") + m_scriptName);
                gui::wrapped_label(m_description, 390.0f);
                gui::wrapped_label("Esc returns to the editor. Use LMB pan, RMB orbit, wheel zoom, and WASD/QE for play-preview navigation.", 390.0f);
                gui::end_window();

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
            gui::Vec2 m_lastMouse{};
            timing::Clock::time_point m_lastFrame{};
            bool m_hasLastFrame{ false };
            bool m_looking{ false };
            bool m_panning{ false };
        };

        [[nodiscard]] std::vector<ContextGroup> collect_backend_contexts_shared()
        {
            std::vector<ContextGroup> snapshot;

            {
                std::shared_lock lock(epochnamespace::core::g_backendsMutex);
                snapshot.reserve(epochnamespace::core::g_backends.size());

                for (auto& [type, state] : epochnamespace::core::g_backends)
                {
                    std::vector<std::shared_ptr<epochnamespace::core::Context>> contexts;
                    contexts.reserve(1 + state.duplicates.size());

                    if (state.master) contexts.push_back(state.master);
                    for (auto& dup : state.duplicates) contexts.push_back(dup);

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
            std::uint64_t frame_count = 0;
            const std::uint64_t smoke_max_frames = smoke_frame_budget();
            const std::uint64_t smoke_capture_frame = cli::smoke_requested ? 90u : 0u;
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

                            if (!epochnamespace::core::cli::scene_name.empty())
                            {
                                if (auto direct_scene = make_scene_from_id(epochnamespace::core::cli::scene_name))
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
                            ctx->clear_safe();
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
                                const int wheelDelta = epochnamespace::gui::consume_mouse_wheel_delta();
                                const float forwardInput =
                                    (ctx->is_key_held_safe(epochnamespace::input::Key::W) ? 1.0f : 0.0f)
                                    - (ctx->is_key_held_safe(epochnamespace::input::Key::S) ? 1.0f : 0.0f);
                                const float rightInput =
                                    (ctx->is_key_held_safe(epochnamespace::input::Key::D) ? 1.0f : 0.0f)
                                    - (ctx->is_key_held_safe(epochnamespace::input::Key::A) ? 1.0f : 0.0f);
                                const float upInput =
                                    (ctx->is_key_held_safe(epochnamespace::input::Key::E) ? 1.0f : 0.0f)
                                    - (ctx->is_key_held_safe(epochnamespace::input::Key::Q) ? 1.0f : 0.0f);
                                const float yawInput =
                                    (ctx->is_key_held_safe(epochnamespace::input::Key::Right) ? 1.0f : 0.0f)
                                    - (ctx->is_key_held_safe(epochnamespace::input::Key::Left) ? 1.0f : 0.0f);
                                const float pitchInput =
                                    (ctx->is_key_held_safe(epochnamespace::input::Key::Up) ? 1.0f : 0.0f)
                                    - (ctx->is_key_held_safe(epochnamespace::input::Key::Down) ? 1.0f : 0.0f);

                                if (mouse_right_down && look_state.looking)
                                {
                                    const float mouseDeltaX = mouse_pos.x - look_state.last_mouse.x;
                                    const float mouseDeltaY = mouse_pos.y - look_state.last_mouse.y;
                                    constexpr float kMouseSensitivity = 0.20f;
                                    epochnamespace::previewgrid::look_camera(
                                        ctx.get(),
                                        mouseDeltaX * kMouseSensitivity,
                                        -mouseDeltaY * kMouseSensitivity);
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
                                    constexpr float kWheelZoomStep = 1.3f;
                                    epochnamespace::previewgrid::zoom_camera(
                                        ctx.get(),
                                        (static_cast<float>(wheelDelta) / 120.0f) * kWheelZoomStep);
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
                                if (!editor_frame.command_argument.starts_with("project:"))
                                {
                                    logger::get(kEditorLog).logf(
                                        logger::LogLevel::Error,
                                        std::source_location::current(),
                                        "Editor rejected non-project play target '{}'.",
                                        editor_frame.command_argument);
                                    break;
                                }
                                begin_scene(editor_frame.command_argument, SessionMode::Editor);
                                break;
                            case epochnamespace::EditorCommand::RunScript:
                            {
                                const bool ok = epochnamespace::editor_run_script(
                                    ctx.get(),
                                    editor_frame.command_argument.empty()
                                    ? std::string_view{ "rotate_all_entities" }
                                    : std::string_view{ editor_frame.command_argument });
                                logger::get(kEditorLog).logf(
                                    ok ? logger::LogLevel::INFO : logger::LogLevel::Error,
                                    std::source_location::current(),
                                    "Editor script '{}' {}.",
                                    editor_frame.command_argument.empty() ? "rotate_all_entities" : editor_frame.command_argument,
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
                                if (!result.update_available)
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
                            ctx->clear_safe();
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
                            if (ctx_running)
                                ctx->present_safe();

                            if (choice)
                            {
                                if (*choice == epochnamespace::menu::Choice::Exit)
                                {
                                    session.mode = SessionMode::Exit;
                                    ctx_running = false;
                                    win->running = false;
                                }
                                else if (*choice == epochnamespace::menu::Choice::UpdateLatest)
                                {
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
                    prepare_parent_window_for_engine_capture(mgr);
                    std::this_thread::sleep_for(std::chrono::milliseconds(120));
                    capture_parent_window_if_requested(
                        mgr,
                        startup_mode == SessionMode::Editor ? kEditorLog : kEngineLog);
#endif
                    smoke_capture_taken = true;
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
                    if (!result.update_available)
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

                const auto launch_cfg = resolve_legacy_launch_config();

                const bool ok = mgr.Initialize(
                    hi,
                    launch_cfg.raylib_count,
                    launch_cfg.sdl_count,
                    launch_cfg.sfml_count,
                    launch_cfg.vulkan_count,
                    launch_cfg.opengl_count,
                    launch_cfg.software_count,
                    launch_cfg.parented
                );

                if (!ok)
                {
                    //MessageBoxW(nullptr, L"Failed to initialize contexts!", L"Error", MB_ICONERROR | MB_OK);
                    return -1;
                }

                input::designate_polling_thread_to_current();

                mgr.StartRenderThreads();
                mgr.ArrangeDockedWindowsGrid();

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

                const auto launch_cfg = resolve_legacy_launch_config();

                const bool ok = mgr.Initialize(
                    nullptr,
                    launch_cfg.raylib_count,
                    launch_cfg.sdl_count,
                    launch_cfg.sfml_count,
                    launch_cfg.vulkan_count,
                    launch_cfg.opengl_count,
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

                mgr.StartRenderThreads();
                mgr.ArrangeDockedWindowsGrid();

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

            const auto launch_cfg = resolve_legacy_launch_config();

                const bool ok = mgr.Initialize(
                    hi,
                    launch_cfg.raylib_count,
                    launch_cfg.sdl_count,
                    launch_cfg.sfml_count,
                    launch_cfg.vulkan_count,
                    launch_cfg.opengl_count,
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

            mgr.StartRenderThreads();
            mgr.ArrangeDockedWindowsGrid();

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

            const auto launch_cfg = resolve_legacy_launch_config();

                const bool ok = mgr.Initialize(
                    nullptr,
                    launch_cfg.raylib_count,
                    launch_cfg.sdl_count,
                    launch_cfg.sfml_count,
                    launch_cfg.vulkan_count,
                    launch_cfg.opengl_count,
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

            mgr.StartRenderThreads();
            mgr.ArrangeDockedWindowsGrid();

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

