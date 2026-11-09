// Epoch feature implementation; included by editor.core application owner only.
// SPDX-License-Identifier: LicenseRef-MIT-NoSell
        [[nodiscard]] std::string project_runtime_scene_id(const EditorState& editor)
        {
            if (!editor.activeRuntimeScene.empty() && is_engine_arcade_scene_id(editor.activeRuntimeScene))
                return editor.activeRuntimeScene;
            if (!editor.projectId.empty())
                return std::string("project:") + editor.projectId;
            return editor.activeRuntimeScene.empty() ? std::string("project:projectlauncher") : editor.activeRuntimeScene;
        }

        struct ProjectBuildFreshness
        {
            bool rebuildRequired{ true };
            std::string reason{ "build state unknown" };
            std::uint64_t inputFingerprint{};
        };

        [[nodiscard]] std::filesystem::path project_linked_engine_library_path(
            const EditorState& editor)
        {
#if defined(_WIN32)
            return build_layout::windows_bin(
                core::path::find_epoch_repo_root(core::path::executable_dir()),
                editor.projectBuildConfiguration) / "EpochEngine.lib";
#else
            // The existing portable helper configures this generated project's
            // static EpochRuntime target in its own epoch-engine subdirectory.
            return resolve_editor_path(std::filesystem::path{editor.projectRoot})
                / "Artifacts" / "Intermediate" / "cmake" / "linux"
                / ("Clang-" + editor.projectBuildConfiguration)
                / "epoch-engine" / "libEpochRuntime.a";
#endif
        }

        [[nodiscard]] bool file_is_newer_than(
            const std::filesystem::path& input,
            const std::filesystem::file_time_type outputTime,
            std::string& changedInput)
        {
            std::error_code ec;
            const std::filesystem::path resolved = resolve_editor_path(input);
            if (resolved.empty() || !std::filesystem::exists(resolved, ec) || ec)
                return false;

            const auto inputTime = std::filesystem::last_write_time(resolved, ec);
            if (ec)
                return false;

            if (inputTime > outputTime)
            {
                changedInput = display_project_path(resolved);
                return true;
            }

            return false;
        }

        [[nodiscard]] std::vector<std::filesystem::path> project_build_input_paths(
            const EditorState& editor)
        {
            const std::filesystem::path root =
                resolve_editor_path(std::filesystem::path{ editor.projectRoot });
            std::vector<std::filesystem::path> buildInputs{
                project_entry_source_path(editor.projectRoot),
                project_windows_vcxproj_path(editor.projectRoot),
                project_windows_build_script_path(editor.projectRoot),
                project_linux_build_script_path(editor.projectRoot),
                root / "CMakeLists.txt",
                root / "Packages" / "epoch.dependencies.cmake",
                root / "Packages" / "epoch.dependencies.props",
                root / "Packages" / "planetary_terrain" / "project.dependency.json",
                root / "project.paths.txt"
            };

            const std::string activeScriptSource =
                editor_resolve_script_source_path(
                    editor.activeScript, editor.projectRoot);
            if (!activeScriptSource.empty())
                buildInputs.emplace_back(activeScriptSource);
            return buildInputs;
        }

        [[nodiscard]] std::uint64_t project_build_input_fingerprint(
            const EditorState& editor,
            const std::vector<std::filesystem::path>& buildInputs)
        {
            constexpr std::uint64_t offsetBasis = 1'469'598'103'934'665'603ull;
            constexpr std::uint64_t prime = 1'099'511'628'211ull;
            std::uint64_t hash = offsetBasis;
            const auto append = [&](std::string_view value)
            {
                for (const unsigned char byte : value)
                {
                    hash ^= static_cast<std::uint64_t>(byte);
                    hash *= prime;
                }
                hash ^= 0xffu;
                hash *= prime;
            };

            append(editor.projectId);
            append(editor.projectRoot);
            append(editor.activeScript);
            append(editor.projectBuildConfiguration);
            for (const auto& input : buildInputs)
                append(ai_build_path_stamp(input));
            return hash == 0u ? 1u : hash;
        }

        [[nodiscard]] ProjectBuildFreshness project_child_build_freshness(
            const EditorState& editor)
        {
            if (editor.projectRoot.empty())
                return { true, "no active project root", 0u };
            const auto persistedConfiguration = editor_project_build_configuration(editor.projectRoot);
            if (!persistedConfiguration || *persistedConfiguration != editor.projectBuildConfiguration)
                return { true, "project build configuration changed or is invalid; select Debug or Release again", 0u };

            const auto buildInputs = project_build_input_paths(editor);
            const std::uint64_t inputFingerprint =
                project_build_input_fingerprint(editor, buildInputs);
            const std::filesystem::path outputExe =
                project_existing_output_exe_path(editor.projectRoot, editor.projectBuildConfiguration);
            std::error_code ec;
            if (!std::filesystem::exists(outputExe, ec) || ec)
                return { true, "child executable missing", inputFingerprint };

            const auto outputTime = std::filesystem::last_write_time(outputExe, ec);
            if (ec)
            {
                return {
                    true,
                    "child executable timestamp unavailable",
                    inputFingerprint};
            }

            // Scene and manifest data are runtime inputs. They are saved before launch
            // but should not force a relink.
            for (const auto& input : buildInputs)
            {
                std::string changedInput;
                if (file_is_newer_than(input, outputTime, changedInput))
                {
                    return {
                        true,
                        std::string("build input changed: ") + changedInput,
                        inputFingerprint};
                }
            }

            // EpochEngine.lib is both a linked dependency and an output of the
            // generated child build. Keep it in timestamp freshness without
            // putting that self-updated product in the immutable input generation.
            const std::filesystem::path linkedEngineLibrary = project_linked_engine_library_path(editor);
            std::string changedEngineLibrary;
            if (file_is_newer_than(
                    linkedEngineLibrary,
                    outputTime,
                    changedEngineLibrary))
            {
                return {
                    true,
                    std::string("build input changed: ")
                        + changedEngineLibrary,
                    inputFingerprint};
            }

            return { false, "child executable is current", inputFingerprint };
        }
        [[nodiscard]] bool project_build_targets_selected_project(
            const EditorState& editor) noexcept
        {
            return editor.projectBuildProjectKey != 0u
                && editor.projectBuildProjectKey
                    == editor.projectLifecycle.evidence()
                        .generations.selected_project.project_key
                && editor.projectBuildRequestedConfiguration == editor.projectBuildConfiguration;
        }

        [[nodiscard]] std::filesystem::path resolved_project_build_output(
            const EditorState& editor,
            const EditorProjectBuildResult& build)
        {
            const std::string_view ownerRoot =
                editor.projectBuildProjectRoot.empty()
                ? std::string_view{ editor.projectRoot }
                : std::string_view{ editor.projectBuildProjectRoot };
            const auto configuration = editor.projectBuildRequestedConfiguration.empty()
                ? std::string_view{editor.projectBuildConfiguration}
                : std::string_view{editor.projectBuildRequestedConfiguration};
            const auto expected = project_existing_output_exe_path(ownerRoot, configuration);
            if (build.output_path.empty()) return expected;
            const auto output = resolve_editor_path(std::filesystem::path{build.output_path});
            const auto candidates = project_output_exe_candidates(ownerRoot, configuration);
            return std::find(candidates.begin(), candidates.end(), output) != candidates.end()
                ? output : std::filesystem::path{};
        }

        [[nodiscard]] std::optional<core::sha256::Digest>
            project_output_digest(const std::filesystem::path& output)
        {
            const std::filesystem::path resolved = resolve_editor_path(output);
            std::error_code ec;
            if (!std::filesystem::is_regular_file(resolved, ec) || ec)
                return std::nullopt;
            const std::uintmax_t size = std::filesystem::file_size(resolved, ec);
            if (ec || size == 0u)
                return std::nullopt;

            std::ifstream input(resolved, std::ios::binary);
            if (!input)
                return std::nullopt;
            core::sha256::Hasher hasher{};
            std::array<std::uint8_t, 64u * 1024u> bytes{};
            while (input)
            {
                input.read(
                    reinterpret_cast<char*>(bytes.data()),
                    static_cast<std::streamsize>(bytes.size()));
                const std::streamsize count = input.gcount();
                if (count > 0)
                {
                    hasher.update(std::span<const std::uint8_t>{
                        bytes.data(), static_cast<std::size_t>(count)});
                }
            }
            if (!input.eof())
                return std::nullopt;
            const core::sha256::Digest digest = hasher.finish();
            return digest.valid()
                ? std::optional<core::sha256::Digest>{digest}
                : std::nullopt;
        }

        inline constexpr std::string_view kProjectRuntimeProcessGroup =
            "epoch.project.runtime";

        void retain_project_build_transcript(EditorState& editor, const EditorProjectBuildResult& build)
        {
            const auto& generations = editor.projectLifecycle.evidence().generations;
            if (build.log_path.empty() || !project_build_targets_selected_project(editor)
                || editor.projectBuildProjectRoot != editor.projectRoot
                || generations.active_build.project != generations.selected_project
                || generations.active_build.build_generation != editor.projectBuildLifecycleGeneration) return;
            editor.projectBuildTranscript = {
                generations.active_build.project,
                editor.projectBuildRequestedConfiguration, build.log_path};
        }

        [[nodiscard]] std::string project_runtime_correlation(
            const EditorState& editor, bool inEditor = false)
        {
            const auto& evidence = editor.projectLifecycle.evidence();
            if (!editor.projectBuildAcceptedOutputDigest.valid()
                || !evidence.generations.selected_project.valid()
                || evidence.generations.output_artifact.artifact_generation == 0u)
            {
                return {};
            }

            return epochengine::format_text(
                "project:{}:configuration:{}:artifact:{}:sha256:{}:presentation:{}",
                evidence.generations.selected_project.project_key,
                editor.projectBuildConfiguration,
                evidence.generations.output_artifact.artifact_generation,
                core::sha256::hex(editor.projectBuildAcceptedOutputDigest),
                inEditor ? "editor" : "separate");
        }

        inline constexpr std::string_view kProjectRuntimeWindowRoute = "project_runtime";

        [[nodiscard]] std::filesystem::path prepare_project_runtime_log(const EditorState& editor)
        {
            namespace fs = std::filesystem;
            auto reports = project_build_log_path(editor.projectRoot, editor.projectBuildConfiguration).parent_path();
            if (!core::path::candidate_data_root().empty())
                reports = editor_resolve_candidate_project_path(reports);
            if (reports.empty() || !reports.is_absolute()) return {};
            std::error_code error;
            // Do not create a Reports directory through an existing alias.
            const auto expected = reports.lexically_normal();
            if (fs::weakly_canonical(reports, error) != expected || error) return {};
            fs::create_directories(reports, error);
            if (error || fs::canonical(reports, error) != expected || error) return {};
            static std::atomic<std::uint64_t> attempts{};
            const auto path = reports / ("runtime-" + editor.projectBuildConfiguration + "-"
                + core::sha256::hex(editor.projectBuildAcceptedOutputDigest) + "-"
                + std::to_string(editor_steady_tick_ns()) + "-"
                + std::to_string(attempts.fetch_add(1u, std::memory_order_relaxed)) + ".log");
            // Reuse the existing non-reparse, single-link file admission. A
            // lease reserves an empty transcript; child output only appends.
            auto reservation = platform::filesystem::ExclusiveFileLease::acquire(path);
            if (!reservation) return {};
            reservation.reset();
            std::ofstream log(path, std::ios::app | std::ios::binary);
            log << "=== Epoch project runtime | configuration=" << editor.projectBuildConfiguration
                << " | artifact=" << core::sha256::hex(editor.projectBuildAcceptedOutputDigest)
                << " ===\n";
            log.flush();
            return log ? path : fs::path{};
        }

        [[nodiscard]] bool project_runtime_embedding_supported()
        {
#if defined(_WIN32)
            const auto* manager = core::GetActiveMultiContextManager();
            return manager && manager->IsRunning() && manager->GetParentWindow();
#else
            return false;
#endif
        }

        [[nodiscard]] unsigned scene_run_mode_index(const EditorState& editor) noexcept
        {
            return !editor.sceneRunFullscreenInEditor ? 0u
                : editor.sceneRunMaximizedInEditor ? 2u : 1u;
        }

        [[nodiscard]] std::string project_scene_run_button_label(const EditorState& editor,
            bool running)
        {
            if (running) return "Stop";
            if (!editor.projectBuildPending) return "Run";
            return editor.projectBuildStatus.starts_with("Cancel") ? "Cancelling..." : "Building...";
        }

        void select_scene_run_mode(EditorState& editor, unsigned index) noexcept
        {
            if (index > 2u) return;
            editor.sceneRunFullscreenInEditor = index != 0u;
            editor.sceneRunMaximizedInEditor = index == 2u;
            if (editor.projectRuntimeInEditor && editor.projectRuntimeProcess.valid())
                editor.projectRuntimeViewportMaximized = index == 2u;
        }

        [[nodiscard]] bool project_runtime_viewport_maximized(const EditorState& editor) noexcept
        {
            return editor.projectRuntimeViewportMaximized && editor.projectRuntimeInEditor
                && editor.projectRuntimeProcess.valid()
                && editor.projectRuntimeAttachmentError.empty()
                && editor.mainSurface == EditorMainSurface::Scene;
        }

        void exit_project_runtime_full_screen(EditorState& editor) noexcept
        {
            if (!editor.projectRuntimeViewportMaximized) return;
            editor.projectRuntimeViewportMaximized = false;
            editor.layoutDrag = EditorLayoutDrag::None;
            editor.surfaceSettleFrames = (std::max)(editor.surfaceSettleFrames, 1);
        }

        void detach_project_runtime_window(EditorState& editor, bool retiring)
        {
            if (retiring) exit_project_runtime_full_screen(editor);
            if (editor.projectRuntimeWindow && editor.projectRuntimeWindowProcess)
                if (auto* manager = core::GetActiveMultiContextManager(); manager
                    && manager->IsExternalProcessWindowAttached(editor.projectRuntimeWindow,
                        editor.projectRuntimeWindowProcess, kProjectRuntimeWindowRoute))
                    manager->RemoveExternalProcessWindow(editor.projectRuntimeWindow, retiring);
            editor.projectRuntimeWindow = 0u;
            editor.projectRuntimeWindowProcess = 0u;
        }

        [[nodiscard]] bool project_runtime_launch_identity_matches(const EditorState& editor,
            const platform::child_process::ProcessSnapshot& observed)
        {
            return editor.projectRuntimeProcess.valid() && observed.handle == editor.projectRuntimeProcess
                && observed.active() && observed.exclusive_group == kProjectRuntimeProcessGroup
                && !editor.projectRuntimeLaunch.correlation.empty()
                && observed.correlation_key == editor.projectRuntimeLaunch.correlation
                && observed.verified_executable && editor.projectRuntimeLaunch.executable.valid()
                && *observed.verified_executable == editor.projectRuntimeLaunch.executable;
        }

        [[nodiscard]] bool project_runtime_identity_current(const EditorState& editor,
            const platform::child_process::ProcessSnapshot& observed)
        {
            return project_runtime_launch_identity_matches(editor, observed)
                && editor.projectRuntimeLaunch.project.valid()
                && editor.projectRuntimeLaunch.project
                    == editor.projectLifecycle.evidence().generations.selected_project;
        }

        [[nodiscard]] platform::child_process::RuntimeControlLease project_runtime_control_lease(const EditorState& editor)
        {
            const auto* profile=editor_find_project_profile(editor.projectId);
            return {.project_generation=editor.projectRuntimeLaunch.project.generation,
                .project_id=editor.projectId,.scene_id=profile?std::string{profile->runtime_scene_id}:std::string{}};
        }

        [[nodiscard]] bool request_project_runtime_mesh_mode(EditorState& editor,bool wireframe)
        {
            if(!editor.projectRuntimeSnapshot||!project_runtime_identity_current(editor,*editor.projectRuntimeSnapshot)
                ||!editor.projectRuntimeInEditor||!editor.projectRuntimeWindow)return false;
            const bool sent=platform::child_process::request_runtime_mesh_mode(editor.projectRuntimeProcess,
                project_runtime_control_lease(editor),editor.projectRuntimeLaunch.executable,
                editor.projectRuntimeLaunch.correlation,wireframe?platform::child_process::MeshDrawMode::wireframe
                    :platform::child_process::MeshDrawMode::solid);
            if(sent)editor.projectRuntimeSnapshot=platform::child_process::snapshot(editor.projectRuntimeProcess);
            return sent;
        }

        [[nodiscard]] bool request_project_runtime_presentation(EditorState& editor)
        {
            if(!editor.projectRuntimeSnapshot||!project_runtime_identity_current(editor,*editor.projectRuntimeSnapshot)
                ||!editor.projectRuntimeInEditor||!editor.projectRuntimeWindow)return false;
            const auto& time=editor.timelineState;
            return platform::child_process::request_runtime_presentation(editor.projectRuntimeProcess,
                project_runtime_control_lease(editor),editor.projectRuntimeLaunch.executable,
                editor.projectRuntimeLaunch.correlation,{time.playhead_seconds,time.duration_seconds,time.playback_rate,
                    time.direction==epochengine::timeline::PlaybackDirection::reverse,time.playing});
        }

        [[nodiscard]] bool present_project_runtime_viewport(const EditorState& editor)
        {
            if (!editor.projectRuntimeInEditor || !editor.projectRuntimeWindow
                || !editor.projectRuntimeWindowProcess) return true;
            auto* manager = core::GetActiveMultiContextManager();
            return manager && manager->UpdateExternalProcessWindowViewport(
                editor.projectRuntimeWindow, editor.projectRuntimeWindowProcess,
                kProjectRuntimeWindowRoute, editor.mainSurface == EditorMainSurface::Scene);
        }

        [[nodiscard]] bool reconcile_project_runtime_window(EditorState& editor,
            const platform::child_process::ProcessSnapshot& observed)
        {
            if (!project_runtime_identity_current(editor, observed))
            {
                if (editor.projectRuntimeAttachmentError.empty())
                {
                    editor.projectRuntimeAttachmentError = "Runtime launch identity no longer matches the selected project. No window was attached.";
                    if (project_runtime_launch_identity_matches(editor, observed))
                    {
                        detach_project_runtime_window(editor, true);
                        (void)platform::child_process::stop(editor.projectRuntimeProcess, platform::child_process::StopMode::force);
                        editor.projectRuntimeForceStopAvailable = true;
                        editor.projectRuntimeAttachmentError += " Stop requested for this Editor's owned runtime.";
                    }
                    logger::get("Engine.Editor.Project").log(logger::LogLevel::Error,
                        editor.projectRuntimeAttachmentError, std::source_location::current());
                }
                editor.projectRuntimeStatus = editor.projectRuntimeAttachmentError;
                return false;
            }
            if (!editor.projectRuntimeInEditor) return true;
            if (!editor.projectRuntimeAttachmentError.empty())
            { editor.projectRuntimeStatus = editor.projectRuntimeAttachmentError; return false; }
            if (observed.state == platform::child_process::ProcessState::stop_requested) return true;
            auto* manager = core::GetActiveMultiContextManager();
            bool attached = editor.projectRuntimeWindow && editor.projectRuntimeWindow == observed.platform_window_id
                && editor.projectRuntimeWindowProcess == observed.platform_process_id && manager
                && manager->IsExternalProcessWindowAttached(editor.projectRuntimeWindow,
                    editor.projectRuntimeWindowProcess, kProjectRuntimeWindowRoute);
            if (!attached)
            {
                detach_project_runtime_window(editor, false);
                const auto viewportContext = editor.projectRuntimeViewportContext.lock();
                if (manager && viewportContext && observed.platform_window_id && observed.platform_process_id
                    && observed.platform_window_id <= std::numeric_limits<std::uintptr_t>::max())
                    try { attached = manager->AddExternalProcessWindow(static_cast<std::uintptr_t>(observed.platform_window_id),
                        observed.platform_process_id, std::string{kProjectRuntimeWindowRoute},
                        core::RoutedPanelDockTarget::none, viewportContext); }
                    catch (...) { attached = false; }
            }
            if (attached)
            {
                if (editor.projectRuntimeWindow != observed.platform_window_id)
                    logger::get("Engine.Editor.Project").log(logger::LogLevel::INFO,
                        "Owned runtime window admitted. " + observed.message,
                        std::source_location::current());
                editor.projectRuntimeWindow = static_cast<std::uintptr_t>(observed.platform_window_id);
                editor.projectRuntimeWindowProcess = observed.platform_process_id;
                if (present_project_runtime_viewport(editor))
                {
                    editor.projectRuntimeStatus = "Project running in the Editor scene viewport.";
                    using Status=platform::child_process::RuntimeControlStatus;
                    const auto& control=observed.runtime_control;
                    if(control.status==Status::pending)editor.projectRuntimeStatus="Mesh display requested; waiting for the running project's frame acknowledgement.";
                    else if(control.status==Status::rejected)editor.projectRuntimeStatus="The running project refused the requested mesh display; its previous view is retained.";
                    else if(control.status==Status::disconnected)editor.projectRuntimeStatus="Running project display control disconnected; Stop and rebuild before changing mesh display.";
                    return true;
                }
            }
            if (manager && !observed.platform_window_id && observed.runtime_ns < 15'000'000'000ull)
            { editor.projectRuntimeStatus = "Starting project; waiting for its window to appear inside the Editor."; return true; }
            editor.projectRuntimeAttachmentError = "Run In Editor could not attach the owned project window. Stop requested; no separate-window fallback was used.";
            if (!observed.message.empty()) editor.projectRuntimeAttachmentError += " " + observed.message;
            if (!editor.projectRuntimeOutputLog.empty())
                editor.projectRuntimeAttachmentError += " Output: " + editor.projectRuntimeOutputLog;
            logger::get("Engine.Editor.Project").log(logger::LogLevel::Error,
                editor.projectRuntimeAttachmentError, std::source_location::current());
            (void)platform::child_process::stop(editor.projectRuntimeProcess, platform::child_process::StopMode::force);
            editor.projectRuntimeForceStopAvailable = true;
            editor.projectRuntimeStatus = editor.projectRuntimeAttachmentError;
            return false;
        }

        void reconcile_project_runtime(EditorState& editor)
        {
            platform::child_process::poll();

            std::optional<platform::child_process::ProcessSnapshot> observed{};
            if (editor.projectRuntimeProcess.valid())
            {
                observed = platform::child_process::snapshot(
                    editor.projectRuntimeProcess);
            }
            if (!observed)
            {
                detach_project_runtime_window(editor, true);
                editor.projectRuntimeInEditor = false;
                if (editor.projectLifecycle.evidence().runtime_active)
                    editor.projectLifecycle.record_runtime_stopped();
                editor.projectRuntimeProcess = {};
                editor.projectRuntimeLaunch = {};
                editor.projectRuntimeSnapshot.reset();
                editor.projectRuntimeForceStopAvailable = false;
                return;
            }

            editor.projectRuntimeSnapshot = *observed;
            if (observed->active())
            {
                if (project_runtime_identity_current(editor, *observed)
                    && editor.projectLifecycle.evidence().generations.output_artifact.artifact_generation
                        == editor.projectRuntimeLaunch.artifact_generation
                    && !editor.projectLifecycle.evidence().runtime_active)
                {
                    (void)editor.projectLifecycle.record_runtime_started();
                }
                editor.projectRuntimeStatus = epochengine::format_text(
                    "{} | PID {} | {:.2f}s",
                    platform::child_process::process_state_name(observed->state),
                    observed->platform_process_id,
                    static_cast<double>(observed->runtime_ns)
                        / 1'000'000'000.0);
                editor.projectRuntimeForceStopAvailable =
                    editor.projectRuntimeForceStopAvailable
                    || observed->state
                        == platform::child_process::ProcessState::stop_requested;
                (void)reconcile_project_runtime_window(editor, *observed);
                return;
            }

            detach_project_runtime_window(editor, true);
            if (editor.projectLifecycle.evidence().runtime_active)
                editor.projectLifecycle.record_runtime_stopped();

            editor.projectRuntimeStatus = observed->exit_code_valid
                ? epochengine::format_text(
                    "{} with code {} after {:.2f}s.",
                    platform::child_process::process_state_name(observed->state),
                    observed->exit_code,
                    static_cast<double>(observed->runtime_ns)
                        / 1'000'000'000.0)
                : epochengine::format_text(
                    "{} after {:.2f}s: {}",
                    platform::child_process::process_state_name(observed->state),
                    static_cast<double>(observed->runtime_ns)
                        / 1'000'000'000.0,
                    observed->message);
            if (!editor.projectRuntimeAttachmentError.empty())
                editor.projectRuntimeStatus = editor.projectRuntimeAttachmentError + " " + editor.projectRuntimeStatus;
            (void)platform::child_process::release(observed->handle);
            editor.projectRuntimeProcess = {};
            editor.projectRuntimeLaunch = {};
            editor.projectRuntimeInEditor = false;
            editor.projectRuntimeForceStopAvailable = false;
        }

        [[nodiscard]] bool focus_project_runtime(EditorState& editor)
        {
            reconcile_project_runtime(editor);
            if (!editor.projectRuntimeProcess.valid())
            {
                editor.projectRuntimeStatus =
                    "No active owned project runtime is available to focus.";
                return false;
            }

            const auto code = platform::child_process::focus(
                editor.projectRuntimeProcess);
            editor.projectRuntimeStatus = epochengine::format_text(
                "Focus request: {}.",
                platform::child_process::focus_code_name(code));
            return code == platform::child_process::FocusCode::focused
                || code == platform::child_process::FocusCode::pending
                || code == platform::child_process::FocusCode::unsupported;
        }

        [[nodiscard]] bool stop_project_runtime(
            EditorState& editor,
            platform::child_process::StopMode mode)
        {
            reconcile_project_runtime(editor);
            if (!editor.projectRuntimeProcess.valid())
            {
                editor.projectRuntimeStatus =
                    "No active owned project runtime is available to stop.";
                return false;
            }

            const auto code = platform::child_process::stop(
                editor.projectRuntimeProcess,
                mode);
            if (code == platform::child_process::StopCode::requested
                || code == platform::child_process::StopCode::forced)
                detach_project_runtime_window(editor, true);
            editor.projectRuntimeForceStopAvailable =
                code == platform::child_process::StopCode::requested
                || code == platform::child_process::StopCode::unsupported
                || code == platform::child_process::StopCode::failed;
            editor.projectRuntimeStatus = epochengine::format_text(
                "{} stop request: {}.",
                mode == platform::child_process::StopMode::force
                    ? "Forced" : "Graceful",
                platform::child_process::stop_code_name(code));
            if (const auto stopped = platform::child_process::snapshot(editor.projectRuntimeProcess);
                stopped && !stopped->message.empty())
            {
                editor.projectRuntimeStatus += " " + stopped->message;
                logger::get("Engine.Editor.Project").log(logger::LogLevel::INFO,
                    editor.projectRuntimeStatus, std::source_location::current());
            }
            return code == platform::child_process::StopCode::requested
                || code == platform::child_process::StopCode::forced;
        }

        [[nodiscard]] bool launch_project_runtime(
            EditorState& editor,
            const std::filesystem::path& executable,
            std::string_view scene,
            std::string_view backend,
            double frameLimit,
            previewgrid::CameraMode cameraMode,
            input::ProfilePreset inputProfile,
            bool inEditor = false)
        {
            if (inEditor && !project_runtime_embedding_supported())
            {
                editor.projectRuntimeStatus = "Run In Editor requires a dock-capable host on this platform. Choose Separate Window to run externally.";
                return false;
            }
            if (resolve_editor_path(executable) != project_existing_output_exe_path(
                    editor.projectRoot, editor.projectBuildConfiguration))
            {
                editor.projectRuntimeStatus = "Launch blocked because the executable belongs to another build configuration.";
                return false;
            }
            const auto currentDigest = project_output_digest(executable);
            if (!currentDigest
                || !editor.projectBuildAcceptedOutputDigest.valid()
                || *currentDigest != editor.projectBuildAcceptedOutputDigest)
            {
                editor.projectLifecycle.observe_output_missing();
                editor.projectBuildAcceptedOutputDigest = {};
                editor.projectRuntimeStatus =
                    "Launch blocked because the child executable no longer matches accepted build evidence.";
                return false;
            }

            const std::string correlation =
                project_runtime_correlation(editor, inEditor);
            if (correlation.empty())
            {
                editor.projectRuntimeStatus =
                    "Launch blocked because artifact identity is incomplete.";
                return false;
            }
            // Preserve the original request/output identity on repeated Run.
            // A different editor's matching process never transfers ownership.
            if (editor.projectRuntimeProcess.valid())
            {
                const auto active = platform::child_process::snapshot(editor.projectRuntimeProcess);
                if (active && active->active())
                {
                    if (editor.projectRuntimeInEditor == inEditor
                        && project_runtime_identity_current(editor, *active))
                        return focus_project_runtime(editor);
                    editor.projectRuntimeStatus = "Stop the current owned runtime before starting another project presentation.";
                    return false;
                }
            }

            const auto outputLog = prepare_project_runtime_log(editor);
            if (outputLog.empty())
            {
                editor.projectRuntimeStatus = "Launch blocked because a safe project runtime transcript could not be reserved.";
                logger::get("Engine.Editor.Project").log(logger::LogLevel::Error,
                    editor.projectRuntimeStatus, std::source_location::current());
                return false;
            }
            editor.projectRuntimeOutputLog = outputLog.generic_string();
            push_editor_log(editor, "[project] Runtime output: " + editor.projectRuntimeOutputLog);

            std::string runtimeScene{scene};
            if (runtimeScene.starts_with("project:"))
            {
                runtimeScene += "|camera=";
                runtimeScene += project_camera_argument(cameraMode);
                runtimeScene += "|input=";
                runtimeScene += input_profile_argument(inputProfile);
            }

            platform::child_process::LaunchRequest request{};
            request.executable = resolve_editor_path(executable);
            request.working_directory = request.executable.parent_path();
            request.merged_output_path = outputLog;
            request.append_output = true;
            request.disconnect_standard_input = true;
            request.suppress_console = true;
            std::error_code imageError;
            const auto imageBytes = std::filesystem::file_size(request.executable, imageError);
            if (imageError) { editor.projectRuntimeStatus = "Launch blocked because the accepted executable cannot be read."; return false; }
            request.expected_executable = platform::child_process::ExecutableIdentity{
                imageBytes, core::sha256::hex(editor.projectBuildAcceptedOutputDigest)};
            request.arguments = {
                "--standalone",
                "--window-mode",
                "standalone",
                "--backend",
                backend.empty() ? std::string{"opengl"} : std::string{backend},
                "--frame-limit",
                std::string{frame_limit_argument(frameLimit)}};
            const auto* projectProfile = editor_find_project_profile(editor.projectId);
            if (!projectProfile || projectProfile->kind != EditorProjectKind::Editor)
            {
                request.arguments.push_back("--scene");
                request.arguments.push_back(std::move(runtimeScene));
            }
            request.correlation_key = correlation;
            request.exclusive_group = kProjectRuntimeProcessGroup;
            request.display_name = editor.projectName.empty()
                ? std::string{"Epoch Project"}
                : editor.projectName;
            request.window_mode =
                inEditor ? platform::child_process::WindowMode::deferred
                         : platform::child_process::WindowMode::normal;
            if (inEditor) request.expected_window_class = "EpochChild";
            if (inEditor) request.arguments.push_back("--defer-window-show");
            if(inEditor&&projectProfile&&projectProfile->kind!=EditorProjectKind::Editor)
                request.runtime_control=platform::child_process::RuntimeControlLease{
                    .project_generation=editor.projectLifecycle.evidence().generations.selected_project.generation,
                    .project_id=editor.projectId,.scene_id=std::string{projectProfile->runtime_scene_id}};

            const auto previousOwned = editor.projectRuntimeProcess;
            const auto launched =
                platform::child_process::launch_or_focus(request);
            if (launched.owns_new_process())
            {
                editor.projectRuntimeLaunch = {
                    .project = editor.projectLifecycle.evidence().generations.selected_project,
                    .artifact_generation = editor.projectLifecycle.evidence().generations.output_artifact.artifact_generation,
                    .executable = *request.expected_executable,
                    .correlation = request.correlation_key};
            }
            editor.projectRuntimeStatus = epochengine::format_text(
                "{}: {}",
                platform::child_process::launch_code_name(launched.code),
                launched.message);
            if (!launched)
            {
                editor.projectRuntimeStatus += " Output: " + editor.projectRuntimeOutputLog;
                logger::get("Engine.Editor.Project").log(logger::LogLevel::Error,
                    editor.projectRuntimeStatus, std::source_location::current());
                if (launched.owns_new_process())
                {
                    editor.projectRuntimeProcess = launched.handle;
                    editor.projectRuntimeInEditor = inEditor;
                    editor.projectRuntimeBuildConfiguration = editor.projectBuildConfiguration;
                    editor.projectRuntimeAttachmentError = launched.message;
                    editor.projectRuntimeSnapshot =
                        platform::child_process::snapshot(launched.handle);
                }
                return false;
            }

            if (!launched.owns_new_process() && launched.handle != previousOwned)
            {
                editor.projectRuntimeStatus = "A matching runtime belongs to another owner. It was focused; no window or stop authority was adopted.";
                return false;
            }
            editor.projectRuntimeProcess = launched.handle;
            editor.projectRuntimeInEditor = inEditor;
            editor.projectRuntimeBuildConfiguration = editor.projectBuildConfiguration;
            editor.projectRuntimeAttachmentError.clear();
            editor.projectRuntimeSnapshot =
                platform::child_process::snapshot(launched.handle);
            editor.projectRuntimeForceStopAvailable = false;
            if (launched.code
                == platform::child_process::LaunchCode::started)
            {
                if (!editor.projectLifecycle.record_runtime_started())
                {
                    (void)platform::child_process::stop(
                        launched.handle,
                        platform::child_process::StopMode::force);
                    editor.projectRuntimeStatus =
                        "Runtime started, but lifecycle rejected its artifact generation; forced stop requested.";
                    return false;
                }
            }
            else if (!editor.projectLifecycle.evidence().runtime_active
                && !editor.projectLifecycle.record_runtime_started())
            {
                editor.projectRuntimeStatus =
                    "Matching runtime is active, but its lifecycle generation could not be adopted.";
                return false;
            }

            editor.projectRuntimeViewportMaximized = inEditor && editor.sceneRunMaximizedInEditor;
            if (inEditor) editor.requestedMainSurface = EditorMainSurface::Scene;
            reconcile_project_runtime(editor);
            editor.projectRuntimeStatus += " Output: " + editor.projectRuntimeOutputLog;
            return true;
        }
        [[nodiscard]] bool settle_project_build_lifecycle(
            EditorState& editor,
            const EditorProjectBuildResult& build)
        {
            const std::filesystem::path output =
                resolved_project_build_output(editor, build);
            const std::uint64_t buildGeneration =
                editor.projectBuildLifecycleGeneration;
            editor.projectBuildLifecycleGeneration = 0u;

            if (!project_build_targets_selected_project(editor))
            {
                push_editor_log(
                    editor,
                    "[project] Build completion rejected because its owning project is no longer selected.");
                return false;
            }

            if (buildGeneration == 0u)
            {
                editor.projectLifecycle.observe_output_missing();
                editor.projectBuildAcceptedOutputDigest = {};
                push_editor_log(
                    editor,
                    "[project] Build completion rejected: no active lifecycle generation owned this result.");
                return false;
            }

            if (build.cancelled)
            {
                (void)editor.projectLifecycle.fail_build(buildGeneration);
                editor.projectLifecycle.observe_output_missing();
                editor.projectBuildAcceptedOutputDigest = {};
                push_editor_log(
                    editor,
                    "[project] Cancelled build retired its lifecycle generation without publishing output evidence.");
                return false;
            }

            const ProjectBuildFreshness current =
                project_child_build_freshness(editor);
            const bool inputsUnchanged =
                current.inputFingerprint != 0u
                && current.inputFingerprint
                    == editor.projectLifecycle.build_input_fingerprint()
                && !current.rebuildRequired;
            const auto outputDigest = project_output_digest(output);
            const bool verifiedOutput = build.succeeded
                && inputsUnchanged
                && outputDigest.has_value();

            const bool accepted = verifiedOutput
                ? editor.projectLifecycle.complete_build(buildGeneration, true)
                : editor.projectLifecycle.fail_build(buildGeneration);
            if (!verifiedOutput)
            {
                editor.projectLifecycle.observe_output_missing();
                editor.projectBuildAcceptedOutputDigest = {};
                push_editor_log(
                    editor,
                    inputsUnchanged
                        ? "[project] Build completion did not produce a verified child executable."
                        : "[project] Build completion rejected because its inputs changed while the build was running.");
                return false;
            }
            if (!accepted)
            {
                editor.projectBuildAcceptedOutputDigest = {};
                push_editor_log(
                    editor,
                    "[project] Build completion rejected as stale or cross-project evidence.");
                return false;
            }

            editor.projectBuildAcceptedOutputDigest = *outputDigest;
            return true;
        }

        void abandon_project_build_lifecycle(EditorState& editor) noexcept
        {
            if (editor.projectBuildLifecycleGeneration != 0u)
            {
                (void)editor.projectLifecycle.fail_build(
                    editor.projectBuildLifecycleGeneration);
            }
            editor.projectBuildLifecycleGeneration = 0u;
            editor.projectLifecycle.observe_output_missing();
            editor.projectBuildAcceptedOutputDigest = {};
        }

        void reset_project_build_request(EditorState& editor)
        {
            editor.projectBuildCancellation = {};
            editor.projectBuildProgressSample = {};
            editor.projectBuildRunAfterBuild = false;
            editor.projectBuildRunInEditor = false;
            editor.projectBuildRunScene.clear();
            editor.projectBuildOutputPath.clear();
            editor.projectBuildRunBackend.clear();
            editor.projectBuildRunFrameLimitFps = editor.projectRunFrameLimitFps;
            editor.projectBuildRunCameraMode = editor.projectCameraMode;
            editor.projectBuildRunInputProfile = editor.inputProfilePreset;
            editor.projectBuildProjectKey = 0u;
            editor.projectBuildProjectId.clear();
            editor.projectBuildProjectRoot.clear();
            editor.projectBuildRequestedConfiguration.clear();
        }
        bool clear_completed_project_build(
            EditorState& editor,
            std::string_view source)
        {
            if (!editor.projectBuildPending
                || editor.projectBuildPending->wait_for(
                    std::chrono::milliseconds(0)) != std::future_status::ready)
            {
                return false;
            }

            try
            {
                const auto build = editor.projectBuildPending->get();
                const bool ownedBySelectedProject =
                    project_build_targets_selected_project(editor);
                retain_project_build_transcript(editor, build);
                const std::string completedProject =
                    editor.projectBuildProjectId;
                const bool accepted =
                    settle_project_build_lifecycle(editor, build);
                if (!ownedBySelectedProject)
                {
                    editor.projectBuildStatus =
                        "Discarded completed build for "
                        + (completedProject.empty()
                            ? std::string("a previous project")
                            : completedProject)
                        + " after project selection changed.";
                }
                else
                {
                    editor.projectBuildStatus = build.summary.empty()
                        ? std::string{ "Project build completed." }
                        : build.summary;
                    if (build.succeeded && !accepted)
                    {
                        editor.projectBuildStatus +=
                            " Lifecycle evidence rejected the result.";
                    }
                }
                if (accepted)
                {
                    editor.projectBuildDiagnosticTail.clear();
                }
                else if (!build.log_path.empty())
                {
                    editor.projectBuildDiagnosticTail = read_text_tail(
                        std::filesystem::path{build.log_path}, 2'400u);
                }
                else
                {
                    editor.projectBuildDiagnosticTail =
                        editor.projectBuildStatus;
                }
                push_editor_log(editor,
                    std::string("[project] Cleared completed build before ")
                    + std::string(source)
                    + ": "
                    + editor.projectBuildStatus);
            }
            catch (const editor_tasks::TaskCancelled&)
            {
                abandon_project_build_lifecycle(editor);
                editor.projectBuildStatus =
                    "Project build cancelled before its process tree started.";
                push_editor_log(
                    editor,
                    "[project] " + editor.projectBuildStatus);
            }
            catch (const std::exception& ex)
            {
                abandon_project_build_lifecycle(editor);
                editor.projectBuildStatus =
                    std::string{ "Project build handle completed with an exception: " }
                    + ex.what();
                push_editor_log(
                    editor, std::string{ "[project] " }
                    + editor.projectBuildStatus);
            }
            catch (...)
            {
                abandon_project_build_lifecycle(editor);
                editor.projectBuildStatus =
                    "Project build handle completed with an unknown exception.";
                push_editor_log(
                    editor, std::string{ "[project] " }
                    + editor.projectBuildStatus);
            }

            editor.projectBuildPending.reset();
            reset_project_build_request(editor);
            return true;
        }
        bool clear_completed_ai_build(EditorState& editor, std::string_view source)
        {
            if (!editor.aiContinuousBuildPending
                || editor.aiContinuousBuildPending->wait_for(std::chrono::milliseconds(0)) != std::future_status::ready)
            {
                return false;
            }

            try
            {
                const auto build = editor.aiContinuousBuildPending->get();
                ++editor.aiContinuousBuildRunCount;
                editor.projectBuildStatus = build.summary.empty()
                    ? std::string{ "Engine-development build completed." }
                    : build.summary;
                editor.aiContinuousBuildStatus = build.succeeded
                    ? "Last sandbox build passed; evidence is ready for operator review."
                    : "Last build failed; inspect the build log before promotion.";
                editor.aiContinuousBuildLastSucceeded = build.succeeded;
                push_editor_log(editor,
                    std::string("[ai-build] Cleared completed engine development build before ")
                    + std::string(source)
                    + ": "
                    + editor.projectBuildStatus);
                if (!build.output_path.empty())
                    push_editor_log(editor, "[ai-build] Output: " + build.output_path);
                if (!build.log_path.empty())
                    push_editor_log(editor, "[ai-build] Log: " + build.log_path);
            }
            catch (const std::exception& ex)
            {
                editor.aiContinuousBuildLastSucceeded = false;
                editor.aiContinuousBuildStatus = std::string{ "Build threw: " } + ex.what();
                push_editor_log(editor, "[ai-build] " + editor.aiContinuousBuildStatus);
            }
            catch (...)
            {
                editor.aiContinuousBuildLastSucceeded = false;
                editor.aiContinuousBuildStatus = "Build threw an unknown exception.";
                push_editor_log(editor, "[ai-build] " + editor.aiContinuousBuildStatus);
            }

            editor.aiContinuousBuildPending.reset();
            editor.aiContinuousBuildCancellation = {};
            return true;
        }

        void clear_completed_editor_builds(EditorState& editor, std::string_view source)
        {
            (void)clear_completed_project_build(editor, source);
            (void)clear_completed_ai_build(editor, source);
        }

        [[nodiscard]] std::string project_build_activity_label(const EditorState& editor)
        {
            if (!editor.projectBuildPending) return {};
            const auto& configuration = editor.projectBuildRequestedConfiguration.empty()
                ? editor.projectBuildConfiguration : editor.projectBuildRequestedConfiguration;
            return std::string{editor.projectBuildStatus.starts_with("Cancel") ? "Cancelling "
                : editor.projectBuildStatus.starts_with("Finishing") ? "Finishing " : "Building "}
                + configuration + "...";
        }

        [[nodiscard]] std::optional<std::string> project_build_task_progress(
            const taskgraph::GraphSnapshot& snapshot,
            taskgraph::NodeHandle ticket,
            std::uint64_t nowNs)
        {
            if (!ticket.valid()) return std::nullopt;
            for (const auto& task : snapshot.nodes)
            {
                if (task.id != ticket.id || task.label != "Project Build") continue;
                if (task.cancellationRequested || task.state == taskgraph::NodeState::cancelled)
                    return "Cancelling project build; waiting for owned workers to retire.";
                if (task.state == taskgraph::NodeState::running)
                {
                    const auto elapsed = task.wasStarted && nowNs >= task.startedNanoseconds
                        ? (nowNs - task.startedNanoseconds) / 1'000'000'000ull : 0u;
                    return "Building project (" + std::to_string(elapsed) + "s elapsed). Cancel is available.";
                }
                if (task.state == taskgraph::NodeState::queued || task.state == taskgraph::NodeState::pending)
                    return "Project build queued; waiting for its worker.";
                return "Finishing project build; checking its result.";
            }
            return std::nullopt;
        }

        void refresh_project_build_progress(EditorState& editor)
        {
            if (!editor.taskScheduler || !editor.projectBuildCancellation.valid()
                || !project_build_targets_selected_project(editor)) return;
            const auto now = std::chrono::steady_clock::now();
            if (editor.projectBuildProgressSample != std::chrono::steady_clock::time_point{}
                && now - editor.projectBuildProgressSample < std::chrono::milliseconds{500}) return;
            editor.projectBuildProgressSample = now;
            const auto tick = static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count());
            if (auto progress = project_build_task_progress(editor.taskScheduler->snapshot(),
                editor.projectBuildCancellation.node, tick))
                editor.projectBuildStatus = std::move(*progress);
        }

        [[nodiscard]] bool cancel_project_build(EditorState& editor)
        {
            if (!editor.projectBuildPending
                || !editor.projectBuildCancellation.valid()
                || !editor.taskScheduler)
            {
                editor.projectBuildStatus =
                    "No active project build is available to cancel.";
                return false;
            }

            const taskgraph::CancelCode code = editor.taskScheduler->cancel(
                editor.projectBuildCancellation);
            if (code == taskgraph::CancelCode::cancelled
                || code == taskgraph::CancelCode::requested)
            {
                editor.projectBuildRunAfterBuild = false;
                editor.projectBuildStatus = code == taskgraph::CancelCode::cancelled
                    ? "Project build cancelled before process start."
                    : "Cancelling project build process tree...";
                push_editor_log(editor, "[project] " + editor.projectBuildStatus);
                return true;
            }

            editor.projectBuildStatus = code == taskgraph::CancelCode::already_terminal
                ? "Project build is already settling."
                : "Project build cancellation ticket is stale.";
            return false;
        }

        [[nodiscard]] bool close_active_project(
            EditorState& editor,
            const EditorApplicationProfile& application,
            bool writeLog)
        {
            clear_completed_editor_builds(editor, "project close");
            reconcile_project_runtime(editor);
            if (editor.projectBuildPending
                || planetary_project_busy(editor)
                || editor.scriptBuildPending
                || editor.aiContinuousBuildPending)
            {
                editor.projectStatus =
                    "Close Project is blocked while a build or script task is active. Cancel or wait for it first.";
                push_editor_log(editor, "[project] " + editor.projectStatus);
                return false;
            }
            if (editor.projectRuntimeSnapshot
                && editor.projectRuntimeSnapshot->active())
            {
                editor.projectStatus =
                    "Close Project is blocked while the project runtime is active. Stop the runtime first.";
                push_editor_log(editor, "[project] " + editor.projectStatus);
                return false;
            }

            const std::string closedId = editor.projectId;
            const std::string closedName = editor.projectName;
            if (!closedId.empty())
                editor.projectPickerId = closedId;

            editor.projectLifecycle.clear_project();
            editor.projectId.clear();
            editor.projectName = "No Project";
            editor.projectRoot.clear();
            editor.projectScenePath.clear();
            editor.projectManifest.clear();
            editor.projectTemplate.clear();
            editor.projectKind.clear();
            editor.activeScript.clear();
            editor.projectBuildAcceptedOutputDigest = {};
            editor.projectBuildDiagnosticTail.clear();
            editor.projectRuntimeProcess = {};
            editor.projectRuntimeLaunch = {};
            editor.projectRuntimeSnapshot.reset();
            editor.projectRuntimeForceStopAvailable = false;
            reset_project_build_request(editor);
            reset_project_bound_editor_state(
                editor,
                "No project GUI document is open.");

            apply_application_profile(editor, application);
            const EditorApplicationScene applicationScene =
                make_editor_application_scene(application.kind);
            editor.activeWorld = applicationScene.world_name;
            editor.activeRuntimeScene = application.scene_id;
            editor.entities.clear();
            editor.entities.reserve(applicationScene.entities.size());
            for (const auto& seed : applicationScene.entities)
                editor.entities.push_back(editor_entity_from_seed(seed));

            (void)rebuild_editor_scene_document(
                editor,
                "Open application scene without project");
            synchronize_editor_selection(editor);
            editor.projectStatus =
                "No project is open. Open a project.epoch.json manifest in the Project workspace.";
            editor.projectBuildStatus =
                "Build and Run remain unavailable until a project is opened.";
            editor.scriptBuildStatus =
                "Project scripts are unavailable until a project is opened.";
            if (writeLog)
            {
                push_editor_log(
                    editor,
                    closedId.empty()
                        ? "[project] No project is open."
                        : "[project] Closed " + closedName + " (" + closedId + ").");
            }
            return true;
        }

        [[nodiscard]] bool select_project_build_configuration(
            EditorState& editor, std::string_view configuration)
        {
            clear_completed_editor_builds(editor, "configuration selection");
            reconcile_project_runtime(editor);
            if (editor.projectBuildPending || planetary_project_busy(editor)
                || editor.scriptBuildPending || editor.aiContinuousBuildPending
                || editor.projectLifecycle.evidence().runtime_active
                || (editor.projectRuntimeSnapshot && editor.projectRuntimeSnapshot->active()))
            {
                editor.projectBuildStatus = "Stop the active runtime or finish the build before changing configuration.";
                return false;
            }
            std::string diagnostic;
            if (!editor_set_project_build_configuration(editor.projectRoot, configuration, diagnostic))
            {
                editor.projectBuildStatus = std::move(diagnostic);
                return false;
            }
            editor.projectBuildConfiguration = configuration;
            editor.projectBuildAcceptedOutputDigest = {};
            editor.projectLifecycle.observe_output_missing();
            const auto freshness = project_child_build_freshness(editor);
            (void)editor.projectLifecycle.observe_build_inputs(freshness.inputFingerprint);
            editor.projectBuildStatus = std::move(diagnostic);
            push_editor_log(editor, "[project] " + editor.projectBuildStatus);
            return true;
        }

        void start_project_build(
            EditorState& editor,
            bool runAfterBuild,
            std::string_view reason,
            bool runInEditor = false)
        {
            clear_completed_editor_builds(editor, "project build request");

            if (editor.projectBuildPending || planetary_project_busy(editor))
            {
                editor.projectBuildStatus =
                    "Project build already running; wait for the current build before pressing Run again.";
                push_editor_log(
                    editor,
                    "[project] Build/run request ignored because a project build is already running.");
                return;
            }

            if (editor.aiContinuousBuildPending)
            {
                editor.projectBuildStatus =
                    "Engine-development build is already running; wait before starting a project build.";
                push_editor_log(
                    editor,
                    "[project] Build/run request blocked while AI engine development build is running.");
                return;
            }

            if (editor.projectRoot.empty())
            {
                editor.projectBuildStatus = "No active project root selected.";
                push_editor_log(
                    editor,
                    "[project] Build/run request failed: no active project root.");
                return;
            }

            repair_active_project_evidence(editor);

            const auto dependency=project_planetary::prepare_runtime_dependency(
                resolve_editor_path(std::filesystem::path{editor.projectRoot}));
            if(!dependency.ok)
            {
                editor.projectBuildStatus=dependency.message;
                push_editor_log(editor,"[project] "+dependency.message);
                return;
            }
            const ProjectBuildFreshness freshness =
                project_child_build_freshness(editor);
            if (!editor.projectLifecycle.observe_build_inputs(
                    freshness.inputFingerprint))
            {
                const auto decision =
                    editor.projectLifecycle.decision(
                        project_lifecycle::Request::build);
                editor.projectBuildStatus = epochengine::format_text(
                    "Build blocked: {} ({}).",
                    project_lifecycle::action_name(decision.action),
                    project_lifecycle::block_reason_name(decision.reason));
                push_editor_log(
                    editor, "[project] " + editor.projectBuildStatus);
                return;
            }

            const auto buildDecision =
                editor.projectLifecycle.decision(
                    project_lifecycle::Request::build);
            if (buildDecision.action != project_lifecycle::Action::build_project)
            {
                editor.projectBuildStatus = epochengine::format_text(
                    "Build blocked: {} ({}).",
                    project_lifecycle::action_name(buildDecision.action),
                    project_lifecycle::block_reason_name(buildDecision.reason));
                push_editor_log(
                    editor, "[project] " + editor.projectBuildStatus);
                return;
            }

            const std::uint64_t buildGeneration =
                editor.projectLifecycle.begin_build();
            if (buildGeneration == 0u)
            {
                editor.projectBuildStatus =
                    "Build blocked: lifecycle could not claim a unique build generation.";
                push_editor_log(
                    editor, "[project] " + editor.projectBuildStatus);
                return;
            }
            editor.projectBuildLifecycleGeneration = buildGeneration;
            editor.projectBuildProjectKey = editor.projectLifecycle.evidence()
                .generations.selected_project.project_key;
            editor.projectBuildProjectId = editor.projectId;
            editor.projectBuildProjectRoot = editor.projectRoot;
            editor.projectBuildRequestedConfiguration = editor.projectBuildConfiguration;

            const std::filesystem::path outputExe =
                project_existing_output_exe_path(editor.projectRoot, editor.projectBuildConfiguration);
            editor.projectBuildRunAfterBuild = runAfterBuild;
            editor.projectBuildRunInEditor = runInEditor;
            editor.projectBuildRunScene = project_runtime_scene_id(editor);
            editor.projectBuildOutputPath = display_project_path(outputExe);
            editor.projectBuildRunBackend = editor.projectRunBackend.empty()
                ? std::string("opengl")
                : editor.projectRunBackend;
            editor.projectBuildRunFrameLimitFps =
                editor.projectRunFrameLimitFps;
            editor.projectBuildRunCameraMode = editor.projectCameraMode;
            editor.projectBuildRunInputProfile = editor.inputProfilePreset;
            editor.projectBuildDiagnosticTail.clear();
            editor.projectBuildProgressSample = {};
            editor.projectBuildStatus =
                std::string(runAfterBuild ? "Build/run queued: " : "Build queued: ")
                + std::string(reason)
                + " [generation "
                + std::to_string(buildGeneration)
                + "]";
            push_editor_log(editor, "[project] " + editor.projectBuildStatus);

            try
            {
                auto submitted = schedule_editor_cancellable_task(
                    editor,
                    "Project Build",
                    [root = editor.projectRoot,
                     configuration = editor.projectBuildRequestedConfiguration](std::stop_token cancellation)
                    {
                        return editor_build_project(root, cancellation, configuration);
                    });
                editor.projectBuildCancellation = submitted.cancellation;
                editor.projectBuildPending.emplace(
                    std::move(submitted.completion));
            }
            catch (const std::exception& ex)
            {
                abandon_project_build_lifecycle(editor);
                reset_project_build_request(editor);
                editor.projectBuildStatus =
                    std::string("Could not schedule project build: ") + ex.what();
                push_editor_log(
                    editor, "[project] " + editor.projectBuildStatus);
            }
            catch (...)
            {
                abandon_project_build_lifecycle(editor);
                reset_project_build_request(editor);
                editor.projectBuildStatus =
                    "Could not schedule project build: unknown scheduler failure.";
                push_editor_log(
                    editor, "[project] " + editor.projectBuildStatus);
            }
        }

