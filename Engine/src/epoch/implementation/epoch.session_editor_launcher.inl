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
            epochengine::perf::frame_limiter sessionFrameLimiter{};
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
                                    ctx->is_key_down_safe(epochengine::input::Key::Up);
                                const bool down_pressed =
                                    ctx->is_key_down_safe(epochengine::input::Key::Down);
                                const bool left_pressed =
                                    ctx->is_key_down_safe(epochengine::input::Key::Left);
                                const bool right_pressed =
                                    ctx->is_key_down_safe(epochengine::input::Key::Right);
                                const bool enter_pressed =
                                    ctx->is_key_down_safe(epochengine::input::Key::Enter);

                                ctx->set_scene_preview_mode(core::ScenePreviewMode::Editor);
                                clear_before_ui_frame(ctx);
                                gui::begin_frame(ctx, dt, mouse_pos, mouse_left_down);
                                const auto editor_frame = epochengine::editor_run(ctx);
                                for (const auto& route : editor_frame.companion_pane_activations)
                                {
                                    if (route.empty()) continue;
                                    if (!mgr.OpenDetachedContextWindow(
                                            epochengine::core::DetachedContextWindowRequest{
                                                .gui_route = route,
                                                .activate_existing_only = true}))
                                        logger::get(kEditorLog).logf(logger::LogLevel::WARN,
                                            std::source_location::current(),
                                            "Companion pane activation is unavailable for route '{}'; no window was created.", route);
                                }

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

                if (running)
                    wait_for_session_frame(sessionFrameLimiter, mgr, snapshot);
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
            epochengine::perf::frame_limiter sessionFrameLimiter{};
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
                                    ctx->is_key_down_safe(epochengine::input::Key::Up);
                                const bool down_pressed =
                                    ctx->is_key_down_safe(epochengine::input::Key::Down);
                                const bool left_pressed =
                                    ctx->is_key_down_safe(epochengine::input::Key::Left);
                                const bool right_pressed =
                                    ctx->is_key_down_safe(epochengine::input::Key::Right);
                                const bool enter_pressed =
                                    ctx->is_key_down_safe(epochengine::input::Key::Enter);

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

                if (running)
                    wait_for_session_frame(sessionFrameLimiter, mgr, snapshot);
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

