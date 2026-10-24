        template <typename PumpFunc>
        int RunContextSessionLoop(MultiContextManager& mgr, PumpFunc&& pump_events, SessionMode startup_mode)
        {
            // The launcher is an interface, not an editor or game. Opening it
            // must not initialize hardware audio or the physical controller
            // runtime. Services are initialized once on demand by an editor or
            // playable scene, and retained for that session's lifetime.
            std::unique_ptr<epochengine::audio::PlaybackRuntime> processAudio{};
            std::unique_ptr<ScopedProcessAudioRuntime> audioBinding{};
            std::unique_ptr<ScopedPhysicalInputRuntime> physicalInput{};
            auto ensure_editor_services_initialized = [&]()
            {
                // A normal editor needs the controller runtime but not a
                // physical audio device merely to show a workspace.
                if (!physicalInput)
                    physicalInput = std::make_unique<ScopedPhysicalInputRuntime>();
            };
            auto ensure_scene_audio_initialized = [&]()
            {
                // The first playable scene owns audio initialization; the
                // launcher and inactive editors do not open a device.
                if (!processAudio)
                {
#if EPOCH_ENABLE_PHYSICAL_AUDIO
                    processAudio = std::make_unique<epochengine::audio::PlaybackRuntime>(
                        epochengine::audio::make_sdl_audio_device_sink());
#else
                    processAudio = std::make_unique<epochengine::audio::PlaybackRuntime>();
#endif
                }
                if (!audioBinding)
                    audioBinding = std::make_unique<ScopedProcessAudioRuntime>(*processAudio);
            };
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
            epochengine::perf::frame_limiter sessionFrameLimiter{};
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
                    if (replacement.phase == EditorContextReplacementPhase::retire_requested
                        && replacement.source_context
                        && epochengine::editor_engine_work_blocks_context_switch(
                            replacement.source_context.get()))
                    {
                        // Last-chance fence: a chat or candidate may have been
                        // launched after the selection was queued but before
                        // the frame-boundary backend retirement begins.
                        epochengine::editor_set_context_selection_status(
                            replacement.source_context.get(),
                            "Renderer switch postponed because AI Engine Dev started "
                            "before the backend could retire. The original "
                            "editor and worker remain active.");
                        mgr.EndContextReplacement();
                        pendingEditorContextReplacement.reset();
                    }
                    else if (replacement.phase == EditorContextReplacementPhase::retire_requested)
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

                    // The selected editor—not the launcher—owns hardware and
                    // project-facing runtime initialization.
                    ensure_editor_services_initialized();
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
                    if (sourceCtx && sourceCtx->type != targetType
                        && epochengine::editor_engine_work_blocks_context_switch(sourceCtx.get()))
                    {
                        // The old context owns the active worker, candidate,
                        // token stream and native request. Capturing only its
                        // visual snapshot cannot migrate those resources.
                        // Keep it alive rather than calling reset_to_menu(),
                        // which cancels the campaign and retires its children.
                        epochengine::editor_set_context_selection_status(
                            sourceCtx.get(),
                            std::string{"Renderer switch to "}
                                + std::string{context_type_label(targetType)}
                                + " deferred: AI Engine Dev or AI inference is active. "
                                  "Finish or Stop that work before changing backends; "
                                  "the current session and candidate remain intact.");
                        return;
                    }
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
                                const std::uint32_t tabInsertionIndex =
                                    win->routedDockTabInsertion.exchange(
                                        0xffffffffu,
                                        std::memory_order_acq_rel);
                                epochengine::editor_redock_context_panel(
                                    win->guiRoute,
                                    dockTarget,
                                    tabInsertionIndex);
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
                                    ensure_editor_services_initialized();
                                    ensure_scene_audio_initialized();
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

                            ensure_editor_services_initialized();
                            ensure_scene_audio_initialized();
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
                            epoch_editor_perf::ScopedFrameStallWatch frameStallGuard{};
                            // Includes direct --editor startup and restored editor
                            // contexts, not only a launcher menu selection.
                            ensure_editor_services_initialized();
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
                            epoch_editor_perf::current_frame_watch().mark(
                                epoch_editor_perf::FrameStage::gui_begin);
                            gui::begin_frame(ctx, dt, mouse_pos, mouse_left_down);
                            const auto editor_frame = epochengine::editor_run(ctx);
                            epoch_editor_perf::current_frame_watch().mark(
                                epoch_editor_perf::FrameStage::host_after_editor);

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
                                    const bool altHeld = ctx->is_key_held_safe(epochengine::input::Key::LeftAlt)
                                        || ctx->is_key_held_safe(epochengine::input::Key::RightAlt);
                                    const bool shiftHeld = ctx->is_key_held_safe(epochengine::input::Key::LeftShift)
                                        || ctx->is_key_held_safe(epochengine::input::Key::RightShift);
                                    const bool controlHeld = ctx->is_key_held_safe(epochengine::input::Key::LeftControl)
                                        || ctx->is_key_held_safe(epochengine::input::Key::RightControl);
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
                                        (ctx->action_held_safe(epochengine::input::Action::MoveForward) ? 1.0f : 0.0f)
                                        - (ctx->action_held_safe(epochengine::input::Action::MoveBackward) ? 1.0f : 0.0f);
                                    const float rightInput = !keyboardNavigation ? 0.0f :
                                        (ctx->action_held_safe(epochengine::input::Action::MoveRight) ? 1.0f : 0.0f)
                                        - (ctx->action_held_safe(epochengine::input::Action::MoveLeft) ? 1.0f : 0.0f);
                                    const float upInput = !keyboardNavigation ? 0.0f :
                                        (ctx->action_held_safe(epochengine::input::Action::MoveUp) ? 1.0f : 0.0f)
                                        - (ctx->action_held_safe(epochengine::input::Action::MoveDown) ? 1.0f : 0.0f);
                                    const float yawInput = !keyboardNavigation ? 0.0f :
                                        (ctx->action_held_safe(epochengine::input::Action::LookRight) ? 1.0f : 0.0f)
                                        - (ctx->action_held_safe(epochengine::input::Action::LookLeft) ? 1.0f : 0.0f);
                                    const float pitchInput = !keyboardNavigation ? 0.0f :
                                        (ctx->action_held_safe(epochengine::input::Action::LookUp) ? 1.0f : 0.0f)
                                        - (ctx->action_held_safe(epochengine::input::Action::LookDown) ? 1.0f : 0.0f);

                                    if (ctx->action_pressed_safe(epochengine::input::Action::ResetCamera))
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

                            epoch_editor_perf::current_frame_watch().mark(
                                epoch_editor_perf::FrameStage::gui_end);
                            gui::end_frame();
                            if (ctx_running)
                            {
                                epoch_editor_perf::current_frame_watch().mark(
                                    epoch_editor_perf::FrameStage::present);
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

                if (running)
                    wait_for_session_frame(sessionFrameLimiter, mgr, snapshot);
            }

            // Signal an owned model loader before renderer/context retirement.
            // A load is not a chat request and otherwise lives until static
            // service destruction, after GPU/context cleanup has begun.
            epochengine::ai::native_runtime::request_shutdown();
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

