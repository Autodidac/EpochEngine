/**************************************************************
 *   █████╗ ██╗     ███╗   ███╗   ███╗   ██╗    ██╗██████╗    *
 *  ██╔══██╗██║     ████╗ ████║ ██╔═══██╗████╗  ██║██╔══██╗   *
 *  ███████║██║     ██╔████╔██║ ██║   ██║██╔██╗ ██║██║  ██║   *
 *  ██╔══██║██║     ██║╚██╔╝██║ ██║   ██║██║╚██╗██║██║  ██║   *
 *  ██║  ██║███████╗██║ ╚═╝ ██║ ╚██████╔╝██║ ╚████║██████╔╝   *
 *  ╚═╝  ╚═╝╚══════╝╚═╝     ╚═╝  ╚═════╝ ╚═╝  ╚═══╝╚═════╝    *
 *                                                            *
 *   This file is part of the Almond Project.                 *
 *   AlmondShell - Modular C++ Framework                      *
 *                                                            *
 *   SPDX-License-Identifier: LicenseRef-MIT-NoSell           *
 *                                                            *
 *   Provided "AS IS", without warranty of any kind.          *
 *   Use permitted for Non-Commercial Purposes ONLY,          *
 *   without prior commercial licensing agreement.            *
 *                                                            *
 *   Redistribution Allowed with This Notice and              *
 *   LICENSE file. No obligation to disclose modifications.   *
 *                                                            *
 *   See LICENSE file for full terms.                         *
 *                                                            *
 **************************************************************/
 //
 // aengine.cppm (converted from legacy aengine.cpp)
 //
 // FIXES APPLIED:
 //  - No direct access to core::Context private members (ctx->hwnd).
 //    We only query windows via MultiContextManager APIs.
 //  - Removed non-constant switch case labels for ContextType::Unknown/Noop
 //    because your ContextType in your current modules is not an enum with those
 //    exact enumerators (or they’re not visible here). Default handles it.
 //
//#include "pch.h"

#include "..\include\aengine.config.hpp"
#include "..\include\aengine.hpp"

#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#endif

// -----------------------------
// Standard library imports
// -----------------------------
import <algorithm>;
import <chrono>;
//import <exception>;
import <format>;
import <iostream>;
import <memory>;
import <mutex>;
import <optional>;
import <queue>;
import <shared_mutex>;
import <string>;
import <string_view>;
import <thread>;
import <unordered_map>;
import <utility>;
import <vector>;

// -----------------------------
// Engine/module imports
// -----------------------------
import aengine.platform;
//import aengine.config;

import almondshell;

import aengine.cli;
import aengine.version;
import aengine.updater;
import aengine.input;
import aengine.engine_components;

import aengine.context.multiplexer;
import aengine.context.type;
import aengine.core.context;

import aengine.gui;
import aengine.gui.menu;
import aeditor;

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

#if defined(ALMOND_USING_OPENGL)
import acontext.opengl.context;
#endif
#if defined(ALMOND_USING_SOFTWARE_RENDERER)
import acontext.softrenderer.context;
#endif
#if defined(ALMOND_USING_SDL)
import acontext.sdl.context;
#endif
#if defined(ALMOND_USING_SFML)
import acontext.sfml.context;
#endif
#if defined(ALMOND_USING_RAYLIB)
import acontext.raylib.context;
#endif

namespace input = almondnamespace::input;
namespace menu = almondnamespace::menu;
namespace gui = almondnamespace::gui;

namespace almondnamespace::core
{
    void RunEngine();
    void StartEngine();
    void RunEditorInterface();

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
            std::unique_ptr<almondnamespace::scene::Scene> active_scene{};

            using MenuOverlay = almondnamespace::menu::MenuOverlay;
            using EditorCommandOverlay = almondnamespace::menu::EditorCommandOverlay;
            MenuOverlay games_menu{};
            EditorCommandOverlay editor_menu{};

            games_menu.set_max_columns(almondnamespace::core::cli::menu_columns);
            editor_menu.initialize();

            auto collect_backend_contexts = []()
                {
                    using ContextGroup = std::pair<
                        almondnamespace::core::ContextType,
                        std::vector<std::shared_ptr<almondnamespace::core::Context>>
                    >;

                    std::vector<ContextGroup> snapshot;

                    {
                        std::shared_lock lock(almondnamespace::core::g_backendsMutex);
                        snapshot.reserve(almondnamespace::core::g_backends.size());

                        for (auto& [type, state] : almondnamespace::core::g_backends)
                        {
                            std::vector<std::shared_ptr<almondnamespace::core::Context>> contexts;
                            contexts.reserve(1 + state.duplicates.size());

                            if (state.master) contexts.push_back(state.master);
                            for (auto& dup : state.duplicates) contexts.push_back(dup);

                            snapshot.emplace_back(type, std::move(contexts));
                        }
                    }

                    return snapshot;
                };

            auto init_menus = [&]()
                {
                    auto snapshot = collect_backend_contexts();
                    for (auto& [_, contexts] : snapshot)
                        for (auto& ctx : contexts)
                        {
                            if (ctx)
                            {
                                games_menu.initialize_game_choices();
                                games_menu.recompute_layout(ctx, ctx->get_width_safe(), ctx->get_height_safe());
                            }
                        }
                };

            init_menus();

            std::unordered_map<Context*, std::chrono::steady_clock::time_point> last_frame_times;
            bool running = true;
            bool show_games_popup = false;
            auto pump = std::forward<PumpFunc>(pump_events);

            while (running)
            {
                if (!pump())
                {
                    running = false;
                    break;
                }

                mgr.CleanupFinishedWindows();

                auto snapshot = collect_backend_contexts();
#if !defined(ALMOND_SINGLE_PARENT)
                bool any_context_alive = false;
#endif
                for (auto& [type, contexts] : snapshot)
                {
                    auto update_on_ctx = [&](std::shared_ptr<Context> ctx) -> bool
                        {
                            if (!ctx) return true;

                            auto* win = mgr.findWindowByContext(ctx);
                            if (!win) return true;

                            bool ctx_running = win->running;

                            const auto now = std::chrono::steady_clock::now();
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

                                    games_menu.cleanup();

                                    if (active_scene)
                                        active_scene->unload();

                                    active_scene = make_scene();
                                    active_scene->load();
                                   // state = EditorSceneState::Game;
                                    state = EditorSceneState::Editor;
                                    show_games_popup = false;
                                    std::cout << "[Editor] Launching " << label << " scene.\n";
                                };

                            if (state == EditorSceneState::Editor)
                            {
                                int mx = 0, my = 0;
                                ctx->get_mouse_position_safe(mx, my);

                                const gui::Vec2 mouse_pos{
                                    static_cast<float>(mx),
                                    static_cast<float>(my)
                                };

                                const bool mouse_left_down =
                                    almondnamespace::input::mouseDown.test(almondnamespace::input::MouseButton::MouseLeft);

                                const bool up_pressed =
                                    almondnamespace::input::keyPressed.test(almondnamespace::input::Key::Up);
                                const bool down_pressed =
                                    almondnamespace::input::keyPressed.test(almondnamespace::input::Key::Down);
                                const bool left_pressed =
                                    almondnamespace::input::keyPressed.test(almondnamespace::input::Key::Left);
                                const bool right_pressed =
                                    almondnamespace::input::keyPressed.test(almondnamespace::input::Key::Right);
                                const bool enter_pressed =
                                    almondnamespace::input::keyPressed.test(almondnamespace::input::Key::Enter);

                                ctx->clear_safe();
                                gui::begin_frame(ctx, dt, mouse_pos, mouse_left_down);
                                gui::WidgetBounds editor_bounds{};
                                const bool editor_clicked = almondnamespace::editor_run(ctx, &editor_bounds);
                                if (editor_clicked)
                                    show_games_popup = !show_games_popup;

                                const bool draw_editor_overlay = !show_games_popup;
                                const bool menu_has_focus = draw_editor_overlay;
                                std::optional<almondnamespace::menu::EditorCommandChoice> command_choice{};
                                if (draw_editor_overlay)
                                {
                                    command_choice = editor_menu.update_and_draw(
                                        ctx,
                                        win,
                                        dt,
                                        menu_has_focus ? up_pressed : false,
                                        menu_has_focus ? down_pressed : false,
                                        menu_has_focus ? enter_pressed : false,
                                        menu_has_focus,
                                        editor_bounds);
                                }

                                if (command_choice)
                                {
                                    using almondnamespace::menu::EditorCommandChoice;

                                    switch (*command_choice)
                                    {
                                    case EditorCommandChoice::OpenProject:
                                        std::cout << "[Editor] Open Project selected.\n";
                                        break;
                                    case EditorCommandChoice::Settings:
                                        std::cout << "[Editor] Settings selected.\n";
                                        break;
                                    case EditorCommandChoice::RunGame:
                                        show_games_popup = !show_games_popup;
                                        break;
                                    case EditorCommandChoice::Exit:
                                        state = EditorSceneState::Exit;
                                        running = false;
                                        break;
                                    }
                                }

                                if (show_games_popup)
                                {
                                    const float popup_width = (std::max)(600.0f, ctx->get_width_safe() * 0.7f);
                                    const float popup_height = (std::max)(360.0f, ctx->get_height_safe() * 0.6f);

                                    const gui::Vec2 popup_size{ popup_width, popup_height };
                                    const gui::Vec2 popup_pos{
                                        (ctx->get_width_safe() - popup_size.x) * 0.5f,
                                        (ctx->get_height_safe() - popup_size.y) * 0.5f
                                    };

                                    auto game_choice = games_menu.update_and_draw_in_window(
                                        ctx,
                                        win,
                                        dt,
                                        up_pressed,
                                        down_pressed,
                                        left_pressed,
                                        right_pressed,
                                        enter_pressed,
                                        "Games",
                                        popup_pos,
                                        popup_size,
                                        true);

                                    if (game_choice)
                                    {
                                        using almondnamespace::menu::Choice;

                                        if (*game_choice == Choice::Snake)
                                            begin_scene([] { return std::make_unique<almondnamespace::snakelike::SnakeLikeScene>(); }, "Snake");
                                        else if (*game_choice == Choice::Tetris)
                                            begin_scene([] { return std::make_unique<almondnamespace::tetrislike::TetrisLikeScene>(); }, "Tetris");
                                        else if (*game_choice == Choice::Frogger)
                                            begin_scene([] { return std::make_unique<almondnamespace::froggerlike::FroggerLikeScene>(); }, "Frogger");
                                        else if (*game_choice == Choice::Pacman)
                                            begin_scene([] { return std::make_unique<almondnamespace::pacmanlike::PacmanLikeScene>(); }, "Pacman");
                                        else if (*game_choice == Choice::Sokoban)
                                            begin_scene([] { return std::make_unique<almondnamespace::sokobanlike::SokobanLikeScene>(); }, "Sokoban");
                                        else if (*game_choice == Choice::Bejeweled)
                                            begin_scene([] { return std::make_unique<almondnamespace::match3like::Match3LikeScene>(); }, "Match-3");
                                        else if (*game_choice == Choice::Puzzle)
                                            begin_scene([] { return std::make_unique<almondnamespace::slidinglike::SlidingPuzzleLikeScene>(); }, "Sliding Puzzle");
                                        else if (*game_choice == Choice::Minesweep)
                                            begin_scene([] { return std::make_unique<almondnamespace::minesweeperlike::MinesweeperLikeScene>(); }, "Minesweeper");
                                        else if (*game_choice == Choice::Fourty)
                                            begin_scene([] { return std::make_unique<almondnamespace::a2048like::A2048LikeScene>(); }, "2048");
                                        else if (*game_choice == Choice::Sandsim)
                                            begin_scene([] { return std::make_unique<almondnamespace::sandsim::SandSimScene>(); }, "Sand Sim");
                                        else if (*game_choice == Choice::Cellular)
                                            begin_scene([] { return std::make_unique<almondnamespace::cellularsim::CellularSimScene>(); }, "Cellular");
                                    }
                                }

                                gui::end_frame();
                                ctx->present_safe();
                            }
                            else if (state == EditorSceneState::Game)
                            {
                                if (active_scene)
                                {
                                    ctx_running = active_scene->frame(ctx, win);
                                    if (!ctx_running)
                                    {
                                        active_scene->unload();
                                        active_scene.reset();
                                        state = EditorSceneState::Editor;
                                        games_menu.cleanup();
                                        games_menu.initialize_game_choices();
                                        editor_menu.reset_selection();
                                    }
                                }
                                else
                                {
                                    state = EditorSceneState::Editor;
                                }
                            }
                            else if (state == EditorSceneState::Exit)
                            {
                                running = false;
                            }

                            if (!ctx_running)
                                last_frame_times.erase(raw);

                            return ctx_running;
                        };

#if defined(ALMOND_SINGLE_PARENT)
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
#if !defined(ALMOND_SINGLE_PARENT)
                if (!any_context_alive) running = false;
#endif

                std::this_thread::sleep_for(std::chrono::milliseconds(16));
            }

            if (active_scene)
            {
                active_scene->unload();
                active_scene.reset();
            }

            games_menu.cleanup();

            auto snapshot2 = collect_backend_contexts();
            for (auto& [type, contexts] : snapshot2)
            {
                auto cleanup_backend = [&](std::shared_ptr<almondnamespace::core::Context> ctx)
                    {
                        if (!ctx) return;

                        switch (type)
                        {
#if defined(ALMOND_USING_OPENGL)
                        case almondnamespace::core::ContextType::OpenGL:
                            almondnamespace::openglcontext::opengl_cleanup(ctx);
                            break;
#endif
#if defined(ALMOND_USING_SOFTWARE_RENDERER)
                        case almondnamespace::core::ContextType::Software:
                            // almondnamespace::anativecontext::softrenderer_cleanup(ctx);
                            break;
#endif
#if defined(ALMOND_USING_SDL)
                        case almondnamespace::core::ContextType::SDL:
                            //  almondnamespace::sdlcontext::sdl_cleanup(ctx);
                            break;
#endif
#if defined(ALMOND_USING_SFML)
                        case almondnamespace::core::ContextType::SFML:
                            almondnamespace::sfmlcontext::sfml_cleanup(ctx);
                            break;
#endif
#if defined(ALMOND_USING_RAYLIB)
                        case almondnamespace::core::ContextType::RayLib:
                            almondnamespace::raylibcontext::raylib_cleanup(ctx);
                            break;
#endif
                        case almondnamespace::core::ContextType::Noop:
                            break;
                        default:
                            break;
                        }
                    };

                for (auto& ctx : contexts) cleanup_backend(ctx);
            }

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
            std::unique_ptr<almondnamespace::scene::Scene> active_scene{};

            using MenuOverlay = almondnamespace::menu::MenuOverlay;
            MenuOverlay menu{};
            menu.set_max_columns(almondnamespace::core::cli::menu_columns);

            auto collect_backend_contexts = []()
                {
                    using ContextGroup = std::pair<
                        almondnamespace::core::ContextType,
                        std::vector<std::shared_ptr<almondnamespace::core::Context>>
                    >;

                    std::vector<ContextGroup> snapshot;

                    {
                        std::shared_lock lock(almondnamespace::core::g_backendsMutex);
                        snapshot.reserve(almondnamespace::core::g_backends.size());

                        for (auto& [type, state] : almondnamespace::core::g_backends)
                        {
                            std::vector<std::shared_ptr<almondnamespace::core::Context>> contexts;
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

            std::unordered_map<Context*, std::chrono::steady_clock::time_point> last_frame_times;
            bool running = true;
            auto pump = std::forward<PumpFunc>(pump_events);

            while (running)
            {
                if (!pump())
                {
                    running = false;
                    break;
                }

                mgr.CleanupFinishedWindows();

                auto snapshot = collect_backend_contexts();
#if !defined(ALMOND_SINGLE_PARENT)
                bool any_context_alive = false;
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

                            const auto now = std::chrono::steady_clock::now();
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
                                    almondnamespace::input::mouseDown.test(almondnamespace::input::MouseButton::MouseLeft);

                                const bool up_pressed =
                                    almondnamespace::input::keyPressed.test(almondnamespace::input::Key::Up);
                                const bool down_pressed =
                                    almondnamespace::input::keyPressed.test(almondnamespace::input::Key::Down);
                                const bool left_pressed =
                                    almondnamespace::input::keyPressed.test(almondnamespace::input::Key::Left);
                                const bool right_pressed =
                                    almondnamespace::input::keyPressed.test(almondnamespace::input::Key::Right);
                                const bool enter_pressed =
                                    almondnamespace::input::keyPressed.test(almondnamespace::input::Key::Enter);

                                ctx->clear_safe();
                                gui::begin_frame(ctx, dt, mouse_pos, mouse_left_down);
                                auto choice = menu.update_and_draw(ctx, win, dt, up_pressed, down_pressed, left_pressed, right_pressed, enter_pressed);
                                gui::end_frame();
                                ctx->present_safe();

                                if (choice)
                                {
                                    using almondnamespace::menu::Choice;

                                    if (*choice == Choice::Snake)
                                        begin_scene([] { return std::make_unique<almondnamespace::snakelike::SnakeLikeScene>(); }, SceneID::Snake);
                                    else if (*choice == Choice::Tetris)
                                        begin_scene([] { return std::make_unique<almondnamespace::tetrislike::TetrisLikeScene>(); }, SceneID::Tetris);
                                    else if (*choice == Choice::Frogger)
                                        begin_scene([] { return std::make_unique<almondnamespace::froggerlike::FroggerLikeScene>(); }, SceneID::Frogger);
                                    else if (*choice == Choice::Pacman)
                                        begin_scene([] { return std::make_unique<almondnamespace::pacmanlike::PacmanLikeScene>(); }, SceneID::Pacman);
                                    else if (*choice == Choice::Sokoban)
                                        begin_scene([] { return std::make_unique<almondnamespace::sokobanlike::SokobanLikeScene>(); }, SceneID::Sokoban);
                                    else if (*choice == Choice::Bejeweled)
                                        begin_scene([] { return std::make_unique<almondnamespace::match3like::Match3LikeScene>(); }, SceneID::Match3);
                                    else if (*choice == Choice::Puzzle)
                                        begin_scene([] { return std::make_unique<almondnamespace::slidinglike::SlidingPuzzleLikeScene>(); }, SceneID::Sliding);
                                    else if (*choice == Choice::Minesweep)
                                        begin_scene([] { return std::make_unique<almondnamespace::minesweeperlike::MinesweeperLikeScene>(); }, SceneID::Minesweeper);
                                    else if (*choice == Choice::Fourty)
                                        begin_scene([] { return std::make_unique<almondnamespace::a2048like::A2048LikeScene>(); }, SceneID::Game2048);
                                    else if (*choice == Choice::Sandsim)
                                        begin_scene([] { return std::make_unique<almondnamespace::sandsim::SandSimScene>(); }, SceneID::Sandsim);
                                    else if (*choice == Choice::Cellular)
                                        begin_scene([] { return std::make_unique<almondnamespace::cellularsim::CellularSimScene>(); }, SceneID::Cellular);
                                    else if (*choice == Choice::Settings)
                                        std::cout << "[Menu] Settings selected.\n";
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
                                last_frame_times.erase(raw);

                            return ctx_running;
                        };

#if defined(ALMOND_SINGLE_PARENT)
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
#if !defined(ALMOND_SINGLE_PARENT)
                if (!any_context_alive) running = false;
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
                auto cleanup_backend = [&](std::shared_ptr<almondnamespace::core::Context> ctx)
                    {
                        if (!ctx) return;

                        switch (type)
                        {
#if defined(ALMOND_USING_OPENGL)
                        case almondnamespace::core::ContextType::OpenGL:
                            almondnamespace::openglcontext::opengl_cleanup(ctx);
                            break;
#endif
#if defined(ALMOND_USING_SOFTWARE_RENDERER)
                        case almondnamespace::core::ContextType::Software:
                           // almondnamespace::anativecontext::softrenderer_cleanup(ctx);
                            break;
#endif
#if defined(ALMOND_USING_SDL)
                        case almondnamespace::core::ContextType::SDL:
                          //  almondnamespace::sdlcontext::sdl_cleanup(ctx);
                            break;
#endif
#if defined(ALMOND_USING_SFML)
                        case almondnamespace::core::ContextType::SFML:
                            almondnamespace::sfmlcontext::sfml_cleanup(ctx);
                            break;
#endif
#if defined(ALMOND_USING_RAYLIB)
                        case almondnamespace::core::ContextType::RayLib:
                            almondnamespace::raylibcontext::raylib_cleanup(ctx);
                            break;
#endif


                        case almondnamespace::core::ContextType::Noop:
                            break;
                        default:
                            break;
                        }
                    };

                for (auto& ctx : contexts) cleanup_backend(ctx);
            }

            mgr.StopAll();

            return 0;
        }

        template <typename PumpFunc>
        int RunEngineMainLoopCommon(MultiContextManager& mgr, PumpFunc&& pump_events)
        {
            if (almondnamespace::core::cli::run_menu_loop)
                return RunMenuAndGamesLoop(mgr, std::forward<PumpFunc>(pump_events));

            return RunEditorInterfaceLoop(mgr, std::forward<PumpFunc>(pump_events));
        }

#if defined(_WIN32)
        int RunEngineMainLoopInternal(HINSTANCE hInstance, int nCmdShow)
        {
            UNREFERENCED_PARAMETER(nCmdShow);

            try
            {
                almondnamespace::core::MultiContextManager mgr;

                HINSTANCE hi = hInstance ? hInstance : GetModuleHandleW(nullptr);

                const bool ok = mgr.Initialize(
                    hi,
                    /*RayLib*/   1,
                    /*SDL*/      1,
                    /*SFML*/     1,
                    /*Vulkan*/   1,
                    /*OpenGL*/   1,
                    /*Software*/ 1,
                    ALMOND_SINGLE_PARENT == 1
                );

                if (!ok)
                {
                    //MessageBoxW(nullptr, L"Failed to initialize contexts!", L"Error", MB_ICONERROR | MB_OK);
                    return -1;
                }

                input::designate_polling_thread_to_current();

                mgr.StartRenderThreads();
                mgr.ArrangeDockedWindowsGrid();

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
                almondnamespace::core::MultiContextManager mgr;

                const bool ok = mgr.Initialize(
                    nullptr,
                    /*RayLib*/   1,
                    /*SDL*/      1,
                    /*SFML*/     1,
                    /*Vulkan*/   1,
                    /*OpenGL*/   1,
                    /*Software*/ 1,
                    ALMOND_SINGLE_PARENT == 1
                );

                if (!ok)
                {
                    std::cerr << "[Engine] Failed to initialize contexts!\n";
                    return -1;
                }

                input::designate_polling_thread_to_current();

                mgr.StartRenderThreads();
                mgr.ArrangeDockedWindowsGrid();

                auto pump = []() -> bool
                    {
                        return almondnamespace::platform::pump_events();
                    };

                return RunEngineMainLoopCommon(mgr, pump);
            }
            catch (const std::exception& ex)
            {
                std::cerr << "[Engine] " << ex.what() << '\n';
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
            std::cerr << "[Engine] RunEngine terminated with code " << result << "\n";
#elif defined(__linux__)
        const int result = RunEngineMainLoopLinux();
        if (result != 0)
            std::cerr << "[Engine] RunEngine terminated with code " << result << "\n";
#else
        std::cerr << "[Engine] RunEngine is not implemented for this platform yet.\n";
#endif
    }

    void StartEngine()
    {
        std::cout << "AlmondShell Engine v" << almondnamespace::GetEngineVersion() << '\n';
        RunEngine();
    }

    void RunEditorInterface()
    {
#if defined(_WIN32)
        try
        {
            almondnamespace::core::MultiContextManager mgr;

            const HINSTANCE hi = GetModuleHandleW(nullptr);

            const bool ok = mgr.Initialize(
                hi,
                /*RayLib*/   1,
                /*SDL*/      1,
                /*SFML*/     1,
                    /*Vulkan*/   1,
                /*OpenGL*/   1,
                /*Software*/ 1,
                ALMOND_SINGLE_PARENT == 1
            );

            if (!ok)
            {
                std::cerr << "[Editor] Failed to initialize contexts!\n";
                return;
            }

            input::designate_polling_thread_to_current();

            mgr.StartRenderThreads();
            mgr.ArrangeDockedWindowsGrid();

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

            const int result = engine::RunEditorInterfaceLoop(mgr, pump);
            if (result != 0)
                std::cerr << "[Editor] RunEditorInterface terminated with code " << result << "\n";
        }
        catch (const std::exception& ex)
        {
            MessageBoxA(nullptr, ex.what(), "Error", MB_ICONERROR | MB_OK);
        }
#elif defined(__linux__)
        try
        {
            almondnamespace::core::MultiContextManager mgr;

            const bool ok = mgr.Initialize(
                nullptr,
                /*RayLib*/   1,
                /*SDL*/      1,
                /*SFML*/     1,
                    /*Vulkan*/   1,
                /*OpenGL*/   1,
                /*Software*/ 1,
                ALMOND_SINGLE_PARENT == 1
            );

            if (!ok)
            {
                std::cerr << "[Editor] Failed to initialize contexts!\n";
                return;
            }

            input::designate_polling_thread_to_current();

            mgr.StartRenderThreads();
            mgr.ArrangeDockedWindowsGrid();

            auto pump = []() -> bool
                {
                    return almondnamespace::platform::pump_events();
                };

            const int result = engine::RunEditorInterfaceLoop(mgr, pump);
            if (result != 0)
                std::cerr << "[Editor] RunEditorInterface terminated with code " << result << "\n";
        }
        catch (const std::exception& ex)
        {
            std::cerr << "[Editor] " << ex.what() << '\n';
        }
#else
        std::cerr << "[Editor] RunEditorInterface is not implemented for this platform yet.\n";
#endif
    }
} // namespace almondnamespace::core

namespace urls
{
    const std::string github_base = "https://github.com/";
    const std::string github_raw_base = "https://raw.githubusercontent.com/";

    const std::string owner = "Autodidac/";
    const std::string repo = "Cpp_Ultimate_Project_Updater";
    const std::string branch = "main/";

    const std::string version_url = github_raw_base + owner + repo + "/" + branch + "/modules/aengine.version.ixx";
    const std::string binary_url = github_base + owner + repo + "/releases/latest/download/ConsoleApplication1.exe";
}

#if defined(_WIN32) && defined(ALMOND_USING_WINMAIN)
int WINAPI wWinMain(
    _In_     HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_     LPWSTR    lpCmdLine,
    _In_     int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    UNREFERENCED_PARAMETER(nCmdShow);

#if defined(_DEBUG)
    almondnamespace::core::ShowConsole();
#endif

    try
    {
        const int argc = __argc;
        char** argv = __argv;

        const auto cli_result = almondnamespace::core::cli::parse(argc, argv);

        const almondnamespace::updater::UpdateChannel channel{
            .version_url = urls::version_url,
            .binary_url = urls::binary_url,
        };

        if (cli_result.update_requested)
        {
            const auto update_result =
                almondnamespace::updater::run_update_command(channel, cli_result.force_update);

            if (update_result.force_required && !cli_result.force_update)
                return 2;

            return 0;
        }

        if (cli_result.editor_requested)
        {
            almondnamespace::core::RunEditorInterface();
            return 0;
        }

        return almondnamespace::core::engine::RunEngineMainLoopInternal(hInstance, SW_SHOWNORMAL);
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
#if defined(_WIN32) && defined(ALMOND_USING_WINMAIN)
    return wWinMain(GetModuleHandleW(nullptr), nullptr, GetCommandLineW(), SW_SHOWNORMAL);
#else
    try
    {
        const auto cli_result = almondnamespace::core::cli::parse(argc, argv);

        const almondnamespace::updater::UpdateChannel channel{
            .version_url = urls::version_url,
            .binary_url = urls::binary_url,
        };

        if (cli_result.update_requested)
        {
            const auto update_result =
                almondnamespace::updater::run_update_command(channel, cli_result.force_update);

            if (update_result.force_required && !cli_result.force_update)
                return 2;

            return 0;
        }

        if (cli_result.editor_requested)
        {
            almondnamespace::core::RunEditorInterface();
            return 0;
        }

        almondnamespace::core::StartEngine();
        return 0;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "[Fatal] " << ex.what() << '\n';
        return -1;
    }
#endif
}
