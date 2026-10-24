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
        // Exercise actual profile admission without constructing a native device.
        struct CapabilityContractContext final : epochengine::platform::IGraphicsContext
        {
            epochengine::platform::GraphicsBackend api{};
            epochengine::Capabilities enabled{};
            epochengine::core::error::result<void> create_surface(
                epochengine::platform::window_adapter::WindowHandle) noexcept override { return {}; }
            void resize_surface(epochengine::platform::window_adapter::WindowHandle,
                std::int32_t, std::int32_t) noexcept override {}
            void teardown() noexcept override {}
            epochengine::platform::GraphicsBackend backend() const noexcept override { return api; }
            epochengine::Capabilities capabilities() const noexcept override { return enabled; }
        };
        CapabilityContractContext capabilityContext;
        bool capabilityTruth = true;
        for (const auto api : {epochengine::platform::GraphicsBackend::vulkan,
            epochengine::platform::GraphicsBackend::d3d12,
            epochengine::platform::GraphicsBackend::opengl})
        {
            capabilityContext.api = api;
            const auto unknown = epochengine::platform::probe_capabilities(&capabilityContext);
            capabilityTruth = capabilityTruth && !unknown.indirect_draw
                && !unknown.bindless_textures && !unknown.async_compute
                && unknown.max_sampled_images == 0 && unknown.max_samplers == 0;
        }
        capabilityContext.enabled.indirect_draw = true;
        capabilityContext.enabled.max_sampled_images = 73;
        const auto admittedCaps = epochengine::platform::probe_capabilities(&capabilityContext);
        const auto absentCaps = epochengine::platform::probe_capabilities(nullptr);
        check("platform.capability_evidence", capabilityTruth
            && admittedCaps.indirect_draw && admittedCaps.max_sampled_images == 73
            && !admittedCaps.bindless_textures && !absentCaps.indirect_draw
            && absentCaps.max_sampled_images == 0);
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
        auto sdlSurfaceLifecycle =
            epochengine::sdlcontext::state::SurfaceLifecycle{};
        sdlSurfaceLifecycle.reset();
        const bool sdlInitialSurfaceSuspended =
            !sdlSurfaceLifecycle.rendering_allowed()
            && !sdlSurfaceLifecycle.capture_allowed()
            && sdlSurfaceLifecycle.retain_queued_work()
            && sdlSurfaceLifecycle.firstPresentRequired;
        sdlSurfaceLifecycle.observe_drawable_extent(1'920, 1'080);
        const bool sdlInitialSurfaceReady =
            sdlSurfaceLifecycle.rendering_allowed()
            && sdlSurfaceLifecycle.capture_allowed()
            && sdlSurfaceLifecycle.firstPresentRequired;
        sdlSurfaceLifecycle.acknowledge_present();
        const bool sdlInitialPresentAcknowledged =
            !sdlSurfaceLifecycle.firstPresentRequired;
        sdlSurfaceLifecycle.observe(
            epochengine::sdlcontext::state::SurfaceLifecycleEvent::occluded);
        const bool sdlOcclusionPreservesPresentation =
            sdlSurfaceLifecycle.occluded
            && sdlSurfaceLifecycle.rendering_allowed();
        sdlSurfaceLifecycle.observe(
            epochengine::sdlcontext::state::SurfaceLifecycleEvent::minimized);
        sdlSurfaceLifecycle.observe(
            epochengine::sdlcontext::state::SurfaceLifecycleEvent::resized);
        const bool sdlMinimizeRetainsQueuedWork =
            !sdlSurfaceLifecycle.rendering_allowed()
            && !sdlSurfaceLifecycle.capture_allowed()
            && sdlSurfaceLifecycle.retain_queued_work()
            && sdlSurfaceLifecycle.firstPresentRequired;
        sdlSurfaceLifecycle.observe(
            epochengine::sdlcontext::state::SurfaceLifecycleEvent::restored);
        const bool sdlRestoreRequiresDimensions =
            !sdlSurfaceLifecycle.rendering_allowed()
            && sdlSurfaceLifecycle.dimensionsDirty
            && sdlSurfaceLifecycle.firstPresentRequired;
        sdlSurfaceLifecycle.observe_drawable_extent(0, 0);
        const bool sdlZeroDrawableRefusesCapture =
            !sdlSurfaceLifecycle.rendering_allowed()
            && !sdlSurfaceLifecycle.capture_allowed()
            && sdlSurfaceLifecycle.retain_queued_work();
        sdlSurfaceLifecycle.observe_drawable_extent(2'478, 1'344);
        const bool sdlRestoredSurfaceReady =
            sdlSurfaceLifecycle.rendering_allowed()
            && sdlSurfaceLifecycle.capture_allowed()
            && sdlSurfaceLifecycle.firstPresentRequired
            && sdlHighDpiDimensions.logicalWidth == 1'652
            && sdlHighDpiDimensions.logicalHeight == 896;
        sdlSurfaceLifecycle.acknowledge_present();
        check(
            "context.sdl_surface_suspend_restore",
            sdlInitialSurfaceSuspended
            && sdlInitialSurfaceReady
            && sdlInitialPresentAcknowledged
            && sdlOcclusionPreservesPresentation
            && sdlMinimizeRetainsQueuedWork
            && sdlRestoreRequiresDimensions
            && sdlZeroDrawableRefusesCapture
            && sdlRestoredSurfaceReady
            && !sdlSurfaceLifecycle.firstPresentRequired);
        check(
            "context.sdl_first_present_readiness",
            epochengine::core::backend_requires_first_present(
                epochengine::core::ContextType::OpenGL)
            && epochengine::core::backend_requires_first_present(
                epochengine::core::ContextType::SDL)
            && epochengine::core::backend_requires_first_present(
                epochengine::core::ContextType::RayLib)
            && !epochengine::core::backend_requires_first_present(
                epochengine::core::ContextType::SFML)
            && !epochengine::core::backend_requires_first_present(
                epochengine::core::ContextType::Vulkan)
            && !epochengine::core::backend_requires_first_present(
                epochengine::core::ContextType::Software));
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
            && standardApplication.project_id.empty()
            && standardScene.entities.empty()
            && epochengine::editor_seed_entities_for_project("projectlauncher").empty()
            && !epochengine::editor_seed_entities_for_project("platformer").empty()
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
            && std::abs(staleFrameDeadline - 100.0) < 1.0e-12
            && std::abs(currentFrameDeadline
                    - (100.0 + editorFrameDt * 1.25)) < 1.0e-12
            && std::abs(epochengine::perf::next_frame_deadline(
                100.0, 100.0 + editorFrameDt * 0.75, editorFrameDt, true)
                - (100.0 + editorFrameDt)) < 1.0e-12
            && std::abs(epochengine::perf::next_frame_deadline(
                100.0, 100.0 + editorFrameDt * 1.25, editorFrameDt, true)
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
            "perf.frame_pacing_editor_120_project_60",
            editorPacing.requested_mode
                    == epochengine::perf::frame_pacing_mode::target_hz
                && editorPacing.effective_mode
                    == epochengine::perf::frame_pacing_mode::target_hz
                && std::abs(editorPacing.effective_hz - 120.0) < 1.0e-12
                && editorPacing.cpu_deadline_wait
                && std::abs(standalonePacing.effective_hz - 60.0) < 1.0e-12
                && standalonePacing.cpu_deadline_wait);

        constexpr epochengine::perf::frame_pacing_backend editorBackends[]{
            epochengine::perf::frame_pacing_backend::opengl,
            epochengine::perf::frame_pacing_backend::sdl,
            epochengine::perf::frame_pacing_backend::vulkan,
            epochengine::perf::frame_pacing_backend::raylib,
            epochengine::perf::frame_pacing_backend::sfml};
        bool editorBackendTargetsAgree = true;
        for (const auto backend : editorBackends)
        {
            const auto backendPlan =
                epochengine::perf::resolve_frame_pacing_with_capabilities(
                    editorPacingPolicy,
                    epochengine::perf::frame_activity::foreground,
                    epochengine::perf::frame_pacing_capabilities_for(backend));
            editorBackendTargetsAgree = editorBackendTargetsAgree
                && backendPlan.requested_mode
                    == epochengine::perf::frame_pacing_mode::target_hz
                && backendPlan.effective_mode
                    == epochengine::perf::frame_pacing_mode::target_hz
                && std::abs(backendPlan.configured_hz - 120.0) < 1.0e-12
                && std::abs(backendPlan.effective_hz - 120.0) < 1.0e-12
                && !backendPlan.native_vsync_requested;
        }
        check(
            "perf.frame_pacing_editor_backend_target_parity",
            editorBackendTargetsAgree);

        const auto buildLimited = epochengine::perf::with_frame_rate_ceiling(editorPacing, 10.0);
        const auto slowerRequested = epochengine::perf::resolve_frame_pacing(
            epochengine::perf::target_hz_policy(5.0),
            epochengine::perf::frame_activity::foreground, false);
        const auto alreadySlower = epochengine::perf::with_frame_rate_ceiling(slowerRequested, 10.0);
        check("perf.frame_pacing_single_deadline_ceiling",
            buildLimited.cpu_deadline_wait && buildLimited.effective_hz == 10.0
            && alreadySlower.effective_hz == 5.0
            && epochengine::perf::with_frame_rate_ceiling(editorPacing, 0.0) == editorPacing);

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
            uncappedForeground.requested_mode == epochengine::perf::frame_pacing_mode::uncapped
                && uncappedForeground.configured_hz == 0.0
                && uncappedForeground.effective_mode == epochengine::perf::frame_pacing_mode::uncapped
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

        const auto standaloneUncapped = epochengine::perf::resolve_frame_pacing(
            epochengine::perf::select_frame_pacing_policy(
                epochengine::perf::frame_pacing_mode::uncapped, 0.0, true, true),
            epochengine::perf::frame_activity::foreground, false);
        check("perf.frame_pacing_standalone_uncapped_unchanged",
            standaloneUncapped.effective_mode == epochengine::perf::frame_pacing_mode::uncapped
                && standaloneUncapped.effective_hz == 0.0
                && !standaloneUncapped.cpu_deadline_wait);
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

        bool editorBackendVsyncBehaviorAgrees = true;
        for (const auto backend : editorBackends)
        {
            const auto capabilities =
                epochengine::perf::frame_pacing_capabilities_for(backend);
            const auto backendPlan =
                epochengine::perf::resolve_frame_pacing_with_capabilities(
                    vsyncPolicy,
                    epochengine::perf::frame_activity::foreground,
                    capabilities);
            if (capabilities.vsync)
            {
                editorBackendVsyncBehaviorAgrees =
                    editorBackendVsyncBehaviorAgrees
                    && backendPlan.effective_mode
                        == epochengine::perf::frame_pacing_mode::vsync
                    && backendPlan.native_vsync_requested
                    && backendPlan.native_pacing_requested
                    && !backendPlan.cpu_deadline_wait;
            }
            else
            {
                editorBackendVsyncBehaviorAgrees =
                    editorBackendVsyncBehaviorAgrees
                    && backend
                        == epochengine::perf::frame_pacing_backend::raylib
                    && backendPlan.effective_mode
                        == epochengine::perf::frame_pacing_mode::target_hz
                    && std::abs(backendPlan.effective_hz - 60.0) < 1.0e-12
                    && !backendPlan.native_pacing_requested
                    && !backendPlan.native_vsync_requested
                    && backendPlan.cpu_deadline_wait;
            }
        }
        check(
            "perf.frame_pacing_editor_backend_vsync_behavior",
            editorBackendVsyncBehaviorAgrees);

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
        const auto pressureOnNative = epochengine::perf::with_frame_rate_ceiling(nativeTargetApplied, 10.0);
        const auto pressureOnVsync = epochengine::perf::with_frame_rate_ceiling(nativeVsyncApplied, 20.0);
        const auto pressureOnUncapped = epochengine::perf::with_frame_rate_ceiling(uncappedForeground, 30.0);
        check("perf.frame_pacing_pressure_covers_native_and_uncapped",
            pressureOnNative.cpu_deadline_wait && pressureOnNative.effective_hz == 10.0
            && pressureOnNative.native_pacing_active
            && pressureOnVsync.cpu_deadline_wait && pressureOnVsync.effective_hz == 20.0
            && pressureOnUncapped.cpu_deadline_wait && pressureOnUncapped.effective_hz == 30.0
            && epochengine::perf::with_frame_rate_ceiling(nativeTargetApplied, 240.0) == nativeTargetApplied);
        const auto forcedVsyncOnUncapped =
            epochengine::perf::finalize_native_frame_pacing(
                standaloneUncapped,
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
            "editor.authoring_history",
            epochengine::editor_authoring_history::run_contract()
                == epochengine::editor_authoring_history::ContractFailure::none);
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

        check("gui.font_atlas_capacity", epochengine::font::font_atlas_capacity_contract());
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
            && epochengine::ai::normalize_direct_llama_cpp_reply(
                "EPOCH_SOURCE_EVIDENCE_INSUFFICIENT_V1\n",
                "large source prompt")
                .empty());
        check(
            "ai.direct_llama_prompt_file_transport",
            epochengine::ai::direct_llama_cpp_prompt_transport_contract());
        check(
            "editor.ai_request_cancellation",
            epochengine::editor_ai_request_cancellation_contract());
        check(
            "editor.ai_model_selection",
            epochengine::editor_ai_model_selection_contract());
        check(
            "ai.model_request_cancellation",
            epochengine::ai::model_request_cancellation_contract());
        check("ai.mcp.tool_protocol", epochengine::ai::run_mcp_contract());
        check(
            "ai.epoch_local_install",
            epochengine::ai::epoch_local_ai_install_contract());
        check(
            "ai.local_model_preference",
            epochengine::ai::local_model_preference_contract());
        check(
            "ai.model_install",
            epochengine::ai::model_install::run_contract());
        check(
            "ai.project_profile",
            epochengine::ai::project_profile::run_contract());
        check(
            "epoch.build_validation",
            epochengine::build_validation::run_contract()
                == epochengine::build_validation::ContractFailure::none);
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
            "package.catalog",
            epochengine::package_catalog::contract_self_test());
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
        check(
            "scene.surface_alignment.bounds_and_idempotence",
            epochengine::scene::surface_alignment::run_contract().passed());
        check(
            "media.timeline_preview.admission_fit_transport",
            epochengine::media::timeline_preview::run_contract().passed());
        check("scene.persistence.validation_and_atomic_plan",
            epochengine::scene::persistence::scene_persistence_contract_checks().all_passed());
        check("scene.runtime.compiled_projection",
            static_cast<bool>(epochengine::scene_runtime::run_scene_runtime_contract_checks()));
        const auto toolSceneAcceptance = [&]()
        {
            epochengine::scene::SceneSnapshot tool{};
            tool.project_id = "projectlauncher";
            tool.scene_id = "project:projectlauncher";
            tool.world_name = "LauncherWorkspace";
            tool.document_kind = "tool";
            tool.revision = 1u;
            const bool emptyToolAccepted = epochengine::project::scene_acceptance_diagnostic(tool, false).empty();
            const bool gameStillRequiresRuntime = !epochengine::project::scene_acceptance_diagnostic(tool, true).empty();
            tool.document_kind = "game";
            const bool mismatchedKindRejected = !epochengine::project::scene_acceptance_diagnostic(tool, false).empty();
            tool.document_kind = "tool";
            tool.scene_id.clear();
            const bool invalidIdentityRejected = !epochengine::project::scene_acceptance_diagnostic(tool, false).empty();
            check("project.tool_scene_acceptance.empty_tool", emptyToolAccepted);
            check("project.tool_scene_acceptance.game_requires_runtime", gameStillRequiresRuntime);
            check("project.tool_scene_acceptance.kind_mismatch", mismatchedKindRejected);
            check("project.tool_scene_acceptance.invalid_identity", invalidIdentityRejected);
            return emptyToolAccepted && gameStillRequiresRuntime && mismatchedKindRejected && invalidIdentityRejected;
        };
        check("project.tool_scene_acceptance", toolSceneAcceptance());
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
                .scale{ 0.16f, 1.40f, 0.08f },
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
        int forestCapPolicyContext{};
        const std::array<epochengine::previewgrid::ObjectMarker, 2>
            forestCapPolicyMarkers{{
                epochengine::previewgrid::ObjectMarker{
                    .position{-2.0f, 1.0f, 0.0f},
                    .color{0.58f, 0.36f, 0.20f},
                    .scale{0.16f, 1.40f, 0.08f},
                    .primitive = epochengine::previewgrid::ObjectPreviewPrimitive::ForestTrunk},
                epochengine::previewgrid::ObjectMarker{
                    .position{2.0f, 1.0f, 0.0f},
                    .color{0.42f, 0.70f, 0.32f},
                    .scale{0.16f, 1.40f, 0.08f},
                    .primitive = epochengine::previewgrid::ObjectPreviewPrimitive::ForestBranch}}};
        epochengine::previewgrid::set_object_markers(
            &forestCapPolicyContext,
            std::span<const epochengine::previewgrid::ObjectMarker>{
                forestCapPolicyMarkers});
        const std::vector<epochengine::previewgrid::Vertex> forestCapPolicySolid =
            epochengine::previewgrid::object_solid_vertices_for(
                &forestCapPolicyContext);
        epochengine::previewgrid::clear_object_markers(&forestCapPolicyContext);
        const auto count_cap_center = [&](float x, float y) noexcept
        {
            return static_cast<std::size_t>(std::count_if(
                forestCapPolicySolid.begin(),
                forestCapPolicySolid.end(),
                [=](const epochengine::previewgrid::Vertex& vertex) noexcept
                {
                    return std::abs(vertex.position.x - x) < 0.0001f
                        && std::abs(vertex.position.y - y) < 0.0001f
                        && std::abs(vertex.position.z) < 0.0001f;
                }));
        };

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
            forestProjectionSolid.size() == 72u
            && forestProjectionWire.size() == 24u
            && forestProjectionExtentX > 0.45f
            && forestProjectionExtentZ > 0.35f
            && std::abs(forestProjectedAxis.x) > 0.45f
            && std::abs(forestProjectedAxis.z) > 0.35f);
        check(
            "render.forest_preview_cap_policy",
            forestCapPolicySolid.size() == 144u
            && count_cap_center(-2.0f, 0.30f) == 8u
            && count_cap_center(-2.0f, 1.70f) == 0u
            && count_cap_center(2.0f, 0.30f) == 0u
            && count_cap_center(2.0f, 1.70f) == 8u);
        check(
            "render.forest_preview_indexed_mesh",
            epochengine::forest::preview_mesh::run_contract().passed());
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

