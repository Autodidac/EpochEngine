    void MultiContextManager::RenderLoop(WindowData& win)
    {
        const auto publishLifecycle = [&]() noexcept
        {
            request_parent_layout(parent);
        };

        auto ctx = typed_context(win.context);
        if (!ctx)
        {
            win.running = false;
            win.set_backend_lifecycle(BackendLifecycleState::failed);
            publishLifecycle();
            return;
        }

        ctx->windowData = &win;
        MultiContextManager::SetCurrent(ctx);
        win.set_backend_lifecycle(BackendLifecycleState::initializing);
        publishLifecycle();
        ctx->init_failed = false;

        struct ResetGuard { ~ResetGuard() { MultiContextManager::SetCurrent(nullptr); } } resetGuard;

        if (!running.load(std::memory_order_acquire) || win.get_should_close())
        {
            win.running = false;
            win.set_backend_lifecycle(BackendLifecycleState::stopped);
            publishLifecycle();
            return;
        }

        // Raylib must be created+initialized on the SAME thread that will render it.
        // SDL/SFML use their registered backend lifecycle directly.
#if defined(EPOCH_USING_RAYLIB) && (EPOCH_USING_RAYLIB == 1)
        if (ctx->type == ContextType::RayLib)
        {
#if EPOCH_ENABLE_BACKEND_CONTEXT_CONFIRMATION_LOGS
            epochengine::logger::get(kLogSys).logf(
                epochengine::logger::LogLevel::INFO,
                std::source_location::current(),
                "Raylib init. host={}",
                static_cast<void*>(win.hwnd));
#endif
            const bool initialized = epochengine::raylibcontext::raylib_initialize(
                ctx,
                win.hwnd,
                static_cast<unsigned>(ctx->width),
                static_cast<unsigned>(ctx->height),
                win.onResize,
                win.titleNarrow);

            if (!initialized)
            {
                win.running = false;
                win.set_backend_lifecycle(BackendLifecycleState::failed);
                epochengine::raylibcontext::raylib_cleanup(ctx);
                publishLifecycle();
                return;
            }
        }
#endif

        // skipGenericInit for backends that do their own init above
        const bool skipGenericInit =
#if defined(EPOCH_USING_RAYLIB) && (EPOCH_USING_RAYLIB == 1)
            (ctx->type == ContextType::RayLib) ||
#endif
            false;

        if (!skipGenericInit)
        {
            if (ctx->initialize) ctx->initialize_safe();
            else
            {
                win.running = false;
                win.set_backend_lifecycle(BackendLifecycleState::failed);
                publishLifecycle();
                return;
            }
        }

        if (ctx->init_failed)
        {
            epochengine::logger::get(kLogSys).logf(
                epochengine::logger::LogLevel::Error,
                std::source_location::current(),
                "Backend init failed for {}. Rejecting the replacement window.",
                ctx->backendName);
            win.running = false;
            win.set_backend_lifecycle(BackendLifecycleState::failed);
            if (ctx->cleanup)
                ctx->cleanup_safe();
            publishLifecycle();
            return;
        }

        if (!ctx->process)
        {
            epochengine::logger::get(kLogSys).logf(
                epochengine::logger::LogLevel::Error,
                std::source_location::current(),
                "Backend {} has no frame processor. Rejecting the replacement window.",
                ctx->backendName);
            win.running = false;
            win.set_backend_lifecycle(BackendLifecycleState::failed);
            if (ctx->cleanup)
                ctx->cleanup_safe();
            publishLifecycle();
            return;
        }

#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)
        // This dedicated thread owns its native GL context until frame work
        // ends. Pacing configuration also requires that context to be current.
        epochengine::openglcontext::PlatformGL::ScopedContext dedicatedOpenGLContext;
        if (ctx->type == ContextType::OpenGL)
        {
            epochengine::openglcontext::PlatformGL::PlatformGLContext nativeContext{};
            nativeContext.device = static_cast<HDC>(ctx->native_drawable);
            nativeContext.context = static_cast<HGLRC>(ctx->native_gl_context);
            if (!dedicatedOpenGLContext.set(nativeContext))
            {
                epochengine::logger::get(kLogSys).log(
                    epochengine::logger::LogLevel::Error,
                    "OpenGL dedicated render thread could not activate its native context.",
                    std::source_location::current());
                win.running = false;
                win.set_backend_lifecycle(BackendLifecycleState::failed);
                if (ctx->cleanup)
                    ctx->cleanup_safe();
                publishLifecycle();
                return;
            }
        }
#endif

        const bool requiresFirstPresent =
            epochengine::core::backend_requires_first_present(ctx->type);
        const auto publishRenderReady = [&]()
        {
            win.set_backend_lifecycle(BackendLifecycleState::ready);
            publishLifecycle();
            epochengine::logger::get(kLogSys).logf(
                epochengine::logger::LogLevel::INFO,
                std::source_location::current(),
                "Backend {} reached render-ready state for hwnd={}.",
                ctx->backendName,
                static_cast<void*>(win.hwnd));
        };

        const ScopedWindowsTimerResolution coreFrameTimerResolution{};
        epochengine::perf::frame_limiter coreFrameLimiter{};
        epochengine::perf::frame_pacing_plan activeDesiredFramePlan{};
        epochengine::perf::frame_pacing_plan activeCoreFramePlan{};
        bool hasActiveCoreFramePlan = false;
        epochengine::rendercontext::logging::WindowStateLogGate<
            epochengine::rendercontext::WindowState> windowLogGate{};
        epochengine::perf::frame_pacing_plan lastLoggedFramePlan{};
        bool hasLoggedFramePlan = false;
        using HostPhaseClock = std::chrono::steady_clock;
        enum HostPhase : std::size_t
        {
            hostQuery, hostDwmCloak, hostDpi, hostPublication, hostConfiguration,
            hostRenderCallback, hostTitle, hostAdmission, hostCpuPacing, hostTurn,
            hostPhaseCount
        };
        struct HostPhaseEvidence final
        {
            std::array<double, hostPhaseCount> total{}, maximum{};
            epochengine::perf::frame_pacing_plan plan{};
            HostPhaseClock::time_point began{};
            std::uint64_t frames{};
            std::uint32_t reports{};
            bool hasPlan{};
        } hostEvidence{};
        std::array<double, hostPhaseCount> hostFrame{};
        bool measureHostPhases = true;
        const auto hostMicros = [](HostPhaseClock::time_point begin,
                                   HostPhaseClock::time_point end) noexcept
        {
            return std::chrono::duration<double, std::micro>(end - begin).count();
        };
        const auto resolveCoreFramePolicy = [&win]() noexcept
        {
            const bool standaloneProject =
                !epochengine::core::cli::parented_mode
                && !epochengine::core::cli::editor_requested
                && !epochengine::core::cli::run_menu_loop;
            const double requestedHz = epochengine::core::cli::frame_limit_fps;
            const auto requestedMode = epochengine::core::cli::frame_vsync_requested
                ? epochengine::perf::frame_pacing_mode::vsync
                : requestedHz > 0.0
                ? epochengine::perf::frame_pacing_mode::target_hz
                : epochengine::perf::frame_pacing_mode::uncapped;
            const auto startup = epochengine::perf::select_frame_pacing_policy(
                requestedMode,
                requestedHz,
                epochengine::core::cli::frame_limit_explicit,
                standaloneProject);
            return epochengine::perf::with_frame_pacing_override(
                startup, win.editorFramePacingOverride);
        };
        const auto resolveFrameWindowState = [&]() noexcept
        {
            const auto queryStarted = measureHostPhases ? HostPhaseClock::now() : HostPhaseClock::time_point{};
            HWND observedWindow = win.host_hwnd ? win.host_hwnd : win.hwnd;
            HWND rootWindow = observedWindow
                ? ::GetAncestor(observedWindow, GA_ROOT)
                : nullptr;
            if (!rootWindow)
                rootWindow = observedWindow;

            const bool validWindow = rootWindow
                && ::IsWindow(rootWindow) != FALSE;
            const HWND foreground = ::GetForegroundWindow();
            DWORD cloaked = 0u;
            const auto cloakStarted = measureHostPhases ? HostPhaseClock::now() : HostPhaseClock::time_point{};
            const HRESULT cloakResult = validWindow
                ? ::DwmGetWindowAttribute(
                    rootWindow,
                    DWMWA_CLOAKED,
                    &cloaked,
                    sizeof(cloaked))
                : E_FAIL;
            const auto cloakFinished = measureHostPhases ? HostPhaseClock::now() : HostPhaseClock::time_point{};
            const UINT dpi = validWindow ? ::GetDpiForWindow(rootWindow) : 0u;
            const auto dpiFinished = measureHostPhases ? HostPhaseClock::now() : HostPhaseClock::time_point{};
            const bool minimized = validWindow
                && ::IsIconic(rootWindow) != FALSE;

            epochengine::rendercontext::WindowObservation observation{};
            observation.logical_width = minimized ? 0 : (std::max)(0, win.width);
            observation.logical_height = minimized ? 0 : (std::max)(0, win.height);
            observation.framebuffer_width = minimized
                ? 0 : (std::max)(0, ctx->framebufferWidth);
            observation.framebuffer_height = minimized
                ? 0 : (std::max)(0, ctx->framebufferHeight);
            observation.dpi_milli = dpi > 0u
                ? static_cast<std::uint32_t>((dpi * 1000u + 48u) / 96u)
                : 1000u;
            observation.resize_generation =
                win.resizeGeneration.load(std::memory_order_acquire);
            observation.visible = !validWindow
                || ::IsWindowVisible(rootWindow) != FALSE;
            observation.focused = !validWindow || !foreground
                || foreground == rootWindow
                || ::IsChild(rootWindow, foreground) != FALSE;
            observation.minimized = minimized;
            observation.occluded = SUCCEEDED(cloakResult) && cloaked != 0u;
            observation.visibility_known = validWindow;
            observation.focus_known = validWindow && foreground;
            observation.minimized_known = validWindow;
            observation.occlusion_known = SUCCEEDED(cloakResult);
            observation.dpi_known = dpi > 0u;
            const auto state = epochengine::rendercontext::resolve_window_state(observation);
            if (measureHostPhases)
            {
                hostFrame[hostQuery] = hostMicros(queryStarted, HostPhaseClock::now());
                hostFrame[hostDwmCloak] = hostMicros(cloakStarted, cloakFinished);
                hostFrame[hostDpi] = hostMicros(cloakFinished, dpiFinished);
            }
            return state;
        };
        const auto publishFrameWindowState = [&]()
        {
            const auto state = resolveFrameWindowState();
            const auto publicationStarted = measureHostPhases ? HostPhaseClock::now() : HostPhaseClock::time_point{};
            ctx->publish_frame_window_state(state);
            if (measureHostPhases)
                hostFrame[hostPublication] = hostMicros(publicationStarted, HostPhaseClock::now());
            // Always publish fresh dimensions to rendering; only diagnostics
            // wait for resize activity to settle.
            if (windowLogGate.should_report(state))
            {
                epochengine::logger::get(kLogSys).logf(
                    epochengine::logger::LogLevel::INFO,
                    std::source_location::current(),
                    "Backend {} window state activity={} presentable={} logical={}x{} framebuffer={}x{} dpi_milli={} dpi_known={} visibility_known={} focus_known={} occlusion_known={} resize_generation={}.",
                    ctx->backendName,
                    epochengine::rendercontext::window_activity_name(state.activity),
                    state.presentable,
                    state.logical_width,
                    state.logical_height,
                    state.framebuffer_width,
                    state.framebuffer_height,
                    state.dpi_milli,
                    state.dpi_known,
                    state.visibility_known,
                    state.focus_known,
                    state.occlusion_known,
                    state.resize_generation);
            }
        };
        const auto configureFramePacing = [&]()
        {
            const auto desiredPlan =
                epochengine::perf::resolve_frame_pacing_with_capabilities(
                    resolveCoreFramePolicy(),
                    ctx->frame_pacing_activity(),
                    ctx->frame_pacing_capabilities);
            if (hasActiveCoreFramePlan && desiredPlan == activeDesiredFramePlan)
                return;

            epochengine::perf::native_frame_pacing_result nativeResult{};
            if (ctx->apply_frame_pacing)
            {
                nativeResult = ctx->apply_frame_pacing(
                    desiredPlan.effective_mode,
                    desiredPlan.effective_hz);
            }
            const auto framePlan =
                epochengine::perf::finalize_native_frame_pacing(
                    desiredPlan,
                    nativeResult);
            coreFrameLimiter.set_plan(framePlan);
            win.editorFramePacingHz.store(framePlan.effective_hz, std::memory_order_relaxed);
            win.uncappedFrameSubmission.store(
                framePlan.requested_mode == epochengine::perf::frame_pacing_mode::uncapped
                    && framePlan.effective_mode == epochengine::perf::frame_pacing_mode::uncapped
                    && framePlan.activity == epochengine::perf::frame_activity::foreground
                    && !framePlan.native_pacing_active,
                std::memory_order_relaxed);
            if (!ctx->apply_frame_pacing || nativeResult.configured)
                activeDesiredFramePlan = desiredPlan;
            activeCoreFramePlan = framePlan;
            hasActiveCoreFramePlan = !ctx->apply_frame_pacing || nativeResult.configured;
            // Failed backend pacing configuration may retry every frame.
            // Report actual plan transitions, not identical retry results.
            if (!hasLoggedFramePlan || framePlan != lastLoggedFramePlan)
            {
                lastLoggedFramePlan = framePlan;
                hasLoggedFramePlan = true;
                epochengine::logger::get(kLogSys).logf(
                    epochengine::logger::LogLevel::INFO,
                    std::source_location::current(),
                    "Backend {} pacing requested={} configured_hz={} effective={} effective_hz={} activity={} native_requested={} native_configured={} native_active={} cpu_wait={}.",
                    ctx->backendName,
                    epochengine::perf::to_string(framePlan.requested_mode),
                    framePlan.configured_hz,
                    epochengine::perf::to_string(framePlan.effective_mode),
                    framePlan.effective_hz,
                    epochengine::perf::to_string(framePlan.activity),
                    framePlan.native_pacing_requested,
                    framePlan.native_pacing_configured,
                    framePlan.native_pacing_active,
                    framePlan.cpu_deadline_wait);
            }
        };

        while (running.load(std::memory_order_acquire) && win.running && !win.get_should_close())
        {
            measureHostPhases = hostEvidence.reports < 6u;
            hostFrame = {};
            const auto turnStarted = measureHostPhases ? HostPhaseClock::now() : HostPhaseClock::time_point{};
            bool keepRunning = true;

            try
            {
                // Native owner-thread work must finish before a backend opens
                // its frame. In particular, a GLFW child cannot be reparented
                // or resized between Raylib BeginDrawing/EndDrawing.
                win.ownerThreadCommandQueue.drain();
            }
            catch (const std::exception& e)
            {
                epochengine::logger::get(kLogSys).logf(
                    epochengine::logger::LogLevel::Error,
                    std::source_location::current(),
                    "Backend {} owner-thread command failed: {}",
                    ctx->backendName,
                    e.what());
                win.set_backend_lifecycle(BackendLifecycleState::failed);
                win.running = false;
                break;
            }

            {
                const std::size_t depth = win.commandQueue.depth();
                telemetry::emit_gauge(
                    "renderer.command_queue.depth",
                    static_cast<std::int64_t>(depth),
                    telemetry::RendererTelemetryTags{
                        ctx->type,
                        reinterpret_cast<std::uintptr_t>(win.hwnd)
                    });
            }

            publishFrameWindowState();
            const auto configurationStarted = measureHostPhases ? HostPhaseClock::now() : HostPhaseClock::time_point{};
            configureFramePacing();
            const auto renderStarted = measureHostPhases ? HostPhaseClock::now() : HostPhaseClock::time_point{};
            if (measureHostPhases)
                hostFrame[hostConfiguration] = hostMicros(configurationStarted, renderStarted);

            keepRunning = ctx->process_safe(ctx, win.commandQueue);
            if (measureHostPhases)
                hostFrame[hostRenderCallback] = hostMicros(renderStarted, HostPhaseClock::now());

            if (!keepRunning)
            {
                if (win.backend_lifecycle() == BackendLifecycleState::initializing
                    && !win.get_should_close()
                    && running.load(std::memory_order_acquire))
                {
                    epochengine::logger::get(kLogSys).logf(
                        epochengine::logger::LogLevel::Error,
                        std::source_location::current(),
                        "Backend {} stopped before its first successful frame. Rejecting the replacement window.",
                        ctx->backendName);
                    win.set_backend_lifecycle(BackendLifecycleState::failed);
                }
                win.running = false;
                break;
            }

            win.successfulFrameGeneration.fetch_add(1, std::memory_order_acq_rel);
            if (win.backend_lifecycle() == BackendLifecycleState::initializing
                && (!requiresFirstPresent
                    || win.firstPresentComplete.load(std::memory_order_acquire)))
            {
                publishRenderReady();
            }

            const auto titleStarted = measureHostPhases ? HostPhaseClock::now() : HostPhaseClock::time_point{};
            record_native_title_frame(this, win);
            const auto admissionStarted = measureHostPhases ? HostPhaseClock::now() : HostPhaseClock::time_point{};
            if (measureHostPhases)
                hostFrame[hostTitle] = hostMicros(titleStarted, admissionStarted);
            // An interactive editor should not render 80-120 frames per second
            // while MSBuild or model loading saturates the CPU. Throttle only
            // the editor host; native model/build threads retain their cores.
            // The resource sampler caches OS counters across calls.
            const bool editorHost = epochengine::core::cli::editor_requested
                || epochengine::core::cli::run_menu_loop;
            double allowedEditorHz = 0.0;
            if (editorHost)
            {
                // Compiler activity is a stronger signal than sampled host load:
                // keep the editor responsive but never render over 10 FPS during
                // a guarded AI candidate build, even if CPU pressure reads low.
                if (epochengine::editor::build_pressure::active())
                    allowedEditorHz = 10.0;
                else
                {
                    const auto pressure = epochengine::platform::work_admission::sample_host_resources();
                    const bool cpuMeasured = pressure.cpu_valid
                        && std::isfinite(pressure.cpu_busy_fraction);
                    if (cpuMeasured && pressure.cpu_busy_fraction >= 0.90)
                        allowedEditorHz = 20.0;
                    else if (cpuMeasured && pressure.cpu_busy_fraction >= 0.75)
                        allowedEditorHz = 30.0;
                }
            }
            win.editorThrottleHz.store(allowedEditorHz, std::memory_order_relaxed);
            const auto limitedPlan = epochengine::perf::with_frame_rate_ceiling(
                activeCoreFramePlan, allowedEditorHz);
            if (limitedPlan != coreFrameLimiter.plan())
                coreFrameLimiter.set_plan(limitedPlan);
            const auto pacingStarted = measureHostPhases ? HostPhaseClock::now() : HostPhaseClock::time_point{};
            if (measureHostPhases)
                hostFrame[hostAdmission] = hostMicros(admissionStarted, pacingStarted);
            // The selected limiter owns pacing. A positive OS timer here adds
            // milliseconds even when the operator explicitly selects uncapped.
            coreFrameLimiter.wait_for_next_frame();
            if (!hostEvidence.hasPlan || hostEvidence.plan != limitedPlan)
            {
                hostEvidence = {};
                hostEvidence.plan = limitedPlan;
                hostEvidence.hasPlan = true;
                hostEvidence.began = measureHostPhases ? turnStarted : HostPhaseClock::now();
            }
            if (measureHostPhases)
            {
                const auto turnFinished = HostPhaseClock::now();
                hostFrame[hostCpuPacing] = hostMicros(pacingStarted, turnFinished);
                hostFrame[hostTurn] = hostMicros(turnStarted, turnFinished);
                for (std::size_t phase = 0; phase < hostPhaseCount; ++phase)
                {
                    hostEvidence.total[phase] += hostFrame[phase];
                    hostEvidence.maximum[phase] = (std::max)(hostEvidence.maximum[phase], hostFrame[phase]);
                }
                ++hostEvidence.frames;
                if (turnFinished - hostEvidence.began >= std::chrono::seconds(1))
                {
                    const double frames = static_cast<double>(hostEvidence.frames);
                    epochengine::logger::get(kLogSys).logf(
                        epochengine::logger::LogLevel::INFO,
                        std::source_location::current(),
                        "Backend {} host phases: {} frames; query={:.3f} us (DWM cloak={:.3f}, DPI={:.3f}), publication={:.3f}, configure={:.3f}, render callback={:.3f}, title={:.3f}, admission={:.3f}, CPU pacing={:.3f}, turn={:.3f}; max query={:.3f}, DWM={:.3f}, title={:.3f}, turn={:.3f} us; requested={} effective={} hz={} activity={}.",
                        ctx->backendName, hostEvidence.frames,
                        hostEvidence.total[hostQuery] / frames,
                        hostEvidence.total[hostDwmCloak] / frames,
                        hostEvidence.total[hostDpi] / frames,
                        hostEvidence.total[hostPublication] / frames,
                        hostEvidence.total[hostConfiguration] / frames,
                        hostEvidence.total[hostRenderCallback] / frames,
                        hostEvidence.total[hostTitle] / frames,
                        hostEvidence.total[hostAdmission] / frames,
                        hostEvidence.total[hostCpuPacing] / frames,
                        hostEvidence.total[hostTurn] / frames,
                        hostEvidence.maximum[hostQuery], hostEvidence.maximum[hostDwmCloak],
                        hostEvidence.maximum[hostTitle], hostEvidence.maximum[hostTurn],
                        epochengine::perf::to_string(limitedPlan.requested_mode),
                        epochengine::perf::to_string(limitedPlan.effective_mode),
                        limitedPlan.effective_hz, epochengine::perf::to_string(limitedPlan.activity));
                    hostEvidence.total = {};
                    hostEvidence.maximum = {};
                    hostEvidence.frames = 0u;
                    hostEvidence.began = turnFinished;
                    ++hostEvidence.reports;
                }
            }
        }

        win.ownerThreadCommandQueue.clear();
        if (win.running && !win.get_should_close())
            win.commandQueue.drain();
        else
            win.commandQueue.clear();

#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)
        dedicatedOpenGLContext.release();
#endif
        if (ctx->cleanup) ctx->cleanup_safe();
        if (win.backend_lifecycle() != BackendLifecycleState::failed)
            win.set_backend_lifecycle(BackendLifecycleState::stopped);
        publishLifecycle();
    }

