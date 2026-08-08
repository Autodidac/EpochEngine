        // Included from editor.application.cpp inside the editor implementation-detail namespace.
        // Owns update checks, workers, progress, and updater log evidence.

        [[nodiscard]] updater::UpdateChannel editor_update_channel()
        {
            updater::UpdateChannel channel{};
            channel.version_url = updater::PROJECT_PACKAGED_VERSION_URL();
            channel.binary_url = updater::PROJECT_BINARY_URL();
            channel.source_url = updater::PROJECT_SOURCE_URL();
            channel.source_version_url = updater::PROJECT_SOURCE_VERSION_URL();
            channel.platform_build_status_url = updater::PROJECT_ACTION_RUNS_API_URL();
            channel.platform_build_job_name = updater::PROJECT_UPDATE_BUILD_JOB_NAME();
            return channel;
        }

        constexpr double kEditorUpdateRestartCountdownSeconds = 10.0;
        constexpr int kEditorUpdateCancelRetryCooldownSeconds = 5;
        constexpr double kEditorSourceCancelArmSeconds = 8.0;

        void clear_editor_update_restart_countdown(EditorState& editor) noexcept
        {
            editor.updateRestartCountdownArmed = false;
            editor.updateRestartCountdownStartedAt = {};
        }

        void clear_editor_update_retry_wait(EditorState& editor) noexcept
        {
            editor.updateRetryAfter = {};
        }

        void arm_editor_update_retry_wait(EditorState& editor)
        {
            if constexpr (kEditorUpdateCancelRetryCooldownSeconds <= 0)
            {
                clear_editor_update_retry_wait(editor);
                return;
            }

            editor.updateRetryAfter =
                std::chrono::steady_clock::now()
                + std::chrono::seconds{ kEditorUpdateCancelRetryCooldownSeconds };
        }

        [[nodiscard]] bool editor_update_retry_waiting(const EditorState& editor)
        {
            return editor.updateRetryAfter != std::chrono::steady_clock::time_point{}
                && std::chrono::steady_clock::now() < editor.updateRetryAfter;
        }

        [[nodiscard]] int editor_update_retry_seconds_remaining(const EditorState& editor)
        {
            if (!editor_update_retry_waiting(editor))
                return 0;

            const double seconds = std::chrono::duration<double>(
                editor.updateRetryAfter - std::chrono::steady_clock::now()).count();
            return static_cast<int>(std::ceil((std::max)(0.0, seconds)));
        }

        [[nodiscard]] bool guard_editor_update_retry_wait(EditorState& editor)
        {
            if (!editor_update_retry_waiting(editor))
            {
                clear_editor_update_retry_wait(editor);
                return false;
            }

            editor.updateState = EditorUpdateState::Available;
            editor.updateInstallPending = false;
            editor.updateSourceInstallPending = false;
            editor.updateProjectSourceDownloadPending = false;
            editor.updateSourceCancelRequested = false;
            editor.showUpdateConfirmModal = true;
            editor.updateOperationStartedAt = {};
            clear_editor_update_restart_countdown(editor);
            editor.updateStatus =
                "Update retry is cooling down so the previous cancel can finish clearing worker state. Try again in "
                + std::to_string(editor_update_retry_seconds_remaining(editor))
                + "s.";
            push_editor_log(editor, std::string{ "[update] " } + editor.updateStatus);
            return true;
        }

        [[nodiscard]] bool guard_editor_recent_source_cancel(EditorState& editor)
        {
            const int waitSeconds =
                updater::source_update_recent_cancel_seconds_remaining(kEditorUpdateCancelRetryCooldownSeconds);
            if (waitSeconds <= 0)
                return false;

            editor.updateRetryAfter =
                std::chrono::steady_clock::now() + std::chrono::seconds{ waitSeconds };
            return guard_editor_update_retry_wait(editor);
        }

        void arm_editor_update_restart_countdown(EditorState& editor)
        {
            if (editor.updateRestartCountdownArmed)
                return;

            editor.updateRestartCountdownArmed = true;
            editor.updateRestartCountdownStartedAt = std::chrono::steady_clock::now();
        }

        [[nodiscard]] double editor_update_restart_seconds_remaining(const EditorState& editor)
        {
            if (!editor.updateRestartCountdownArmed
                || editor.updateRestartCountdownStartedAt == std::chrono::steady_clock::time_point{})
            {
                return kEditorUpdateRestartCountdownSeconds;
            }

            const double elapsed = std::chrono::duration<double>(
                std::chrono::steady_clock::now() - editor.updateRestartCountdownStartedAt).count();
            return (std::max)(0.0, kEditorUpdateRestartCountdownSeconds - elapsed);
        }

        [[nodiscard]] bool editor_update_restart_countdown_expired(const EditorState& editor)
        {
            return editor.updateRestartCountdownArmed
                && editor_update_restart_seconds_remaining(editor) <= 0.0;
        }

        [[nodiscard]] std::string describe_update_result(const updater::UpdateCommandResult& result)
        {
            if (!result.status_message.empty())
                return editor_update_modal::format_status_markers(result.status_message);

            if (!result.platform_build_ok
                && (!result.platform_build_reason.empty() || result.platform_build_checked))
            {
                std::string message = "Update withheld";
                if (!result.platform_build_job.empty())
                    message += " for " + result.platform_build_job;
                if (!result.platform_build_reason.empty())
                    message += ": " + result.platform_build_reason;
                else
                    message += ": platform build status is not proven.";
                return message;
            }

            if (result.packaged_update_available)
            {
                if (result.source_update_available)
                {
                    const std::string packaged = result.remote_version.empty()
                        ? std::string{ "packaged runtime" }
                        : std::string{ "packaged runtime " } + result.remote_version;
                    const std::string source = result.source_remote_version.empty()
                        ? std::string{ "main source" }
                        : std::string{ "main source " } + result.source_remote_version;
                    return "Update available: " + packaged + " first; " + source + " is also available after restart.";
                }
                if (!result.remote_version.empty())
                    return std::string{ "Packaged update available: " } + result.remote_version;
                return "Packaged update available.";
            }

            if (result.source_update_available)
            {
                if (result.packaged_release_missing && !result.packaged_release_reason.empty())
                    return result.packaged_release_reason + " Source update is available.";
                if (!result.source_remote_version.empty())
                    return std::string{ "Source update available: " } + result.source_remote_version;
                return "Source update available.";
            }

            if (!result.packaged_release_reason.empty())
                return result.packaged_release_reason;

            return "Epoch is already current.";
        }

        [[nodiscard]] bool editor_update_result_is_source_only(const updater::UpdateCommandResult& result) noexcept
        {
            return result.source_update_available
                && !result.packaged_update_available
                && !result.packaged_handoff_staged;
        }

        [[nodiscard]] std::mutex& editor_update_discovery_cache_mutex()
        {
            static std::mutex mutex{};
            return mutex;
        }

        [[nodiscard]] std::optional<updater::UpdateCommandResult>& editor_update_discovery_cache()
        {
            static std::optional<updater::UpdateCommandResult> cache{};
            return cache;
        }

        [[nodiscard]] bool editor_update_result_is_discovery_only(
            const updater::UpdateCommandResult& result) noexcept
        {
            return !result.update_performed
                && !result.packaged_update_performed
                && !result.source_update_performed
                && !result.packaged_handoff_staged
                && !result.source_fallback_attempted;
        }

        void remember_editor_update_discovery(const updater::UpdateCommandResult& result)
        {
            if (!editor_update_result_is_discovery_only(result))
                return;

            std::scoped_lock lock{ editor_update_discovery_cache_mutex() };
            editor_update_discovery_cache() = result;
        }

        void apply_editor_update_discovery(EditorState& editor, const updater::UpdateCommandResult& result)
        {
            editor.lastUpdateCheck = result;
            editor.updateCheckPending.reset();
            editor.updateInstallPending = false;
            editor.updateSourceInstallPending = false;
            editor.updateProjectSourceDownloadPending = false;
            editor.updateSourceCancelRequested = false;
            editor.updateOperationStartedAt = {};

            if (editor.lastUpdateCheck.update_available || editor.lastUpdateCheck.force_required)
                editor.updateState = EditorUpdateState::Available;
            else
                editor.updateState = EditorUpdateState::Idle;

            editor.updateStatus = describe_update_result(editor.lastUpdateCheck);
        }

        [[nodiscard]] bool apply_cached_editor_update_discovery(EditorState& editor)
        {
            std::scoped_lock lock{ editor_update_discovery_cache_mutex() };
            if (!editor_update_discovery_cache().has_value())
                return false;

            apply_editor_update_discovery(editor, *editor_update_discovery_cache());
            return true;
        }

        [[nodiscard]] updater::UpdateCommandResult make_editor_update_failure_result(
            std::string message,
            const bool sourceAttempted = false)
        {
            updater::UpdateCommandResult result{};
            result.source_fallback_attempted = sourceAttempted;
            result.status_message = std::move(message);
            return result;
        }

        [[nodiscard]] bool editor_update_result_has_installable_intent(
            const updater::UpdateCommandResult& result) noexcept
        {
            return result.update_available
                || result.force_required
                || result.packaged_update_available
                || result.source_update_available
                || result.packaged_handoff_staged;
        }

        void make_editor_source_attempt_retryable(
            EditorState& editor,
            const updater::UpdateCommandResult& fallback)
        {
            updater::UpdateCommandResult clean = editor.lastUpdateCheck;
            if (!editor_update_result_has_installable_intent(clean))
                clean = fallback;

            clean.update_performed = false;
            clean.packaged_update_performed = false;
            clean.packaged_handoff_staged = false;
            clean.source_update_performed = false;
            clean.source_fallback_attempted = false;
            clean.status_message.clear();

            if (!editor_update_result_has_installable_intent(clean))
            {
                clean.update_available = true;
                clean.source_update_available = true;
            }

            editor.lastUpdateCheck = std::move(clean);
        }

        [[nodiscard]] std::string update_toolbar_button_label(EditorUpdateState state)
        {
            switch (state)
            {
            case EditorUpdateState::Available:
                return "Update Available";
            case EditorUpdateState::RestartReady:
                return "Restart";
            case EditorUpdateState::Checking:
                return "Checking...";
            case EditorUpdateState::SourceWorkerRunning:
                return "Source Update Running";
            case EditorUpdateState::Canceled:
                return "Update Available";
            case EditorUpdateState::Failed:
                return "Update Check Failed";
            case EditorUpdateState::Idle:
            default:
                return "Update";
            }
        }

        [[nodiscard]] double editor_update_elapsed_seconds(const EditorState& editor)
        {
            if (editor.updateOperationStartedAt == std::chrono::steady_clock::time_point{})
                return 0.0;

            return std::chrono::duration<double>(
                std::chrono::steady_clock::now() - editor.updateOperationStartedAt).count();
        }

        [[nodiscard]] float editor_source_worker_progress_from_status(
            const std::string_view status,
            const bool cancelRequested) noexcept
        {
            const auto has = [&](const std::string_view needle) noexcept
                {
                    return status.find(needle) != std::string_view::npos;
                };

            if (cancelRequested)
                return 0.10f;

            float progress = 0.08f;
            if (has("Downloading latest") || has("Source snapshot download URL"))
                progress = 0.16f;
            if (has("downloaded successfully") || has("Source snapshot downloaded and extracted"))
                progress = 0.28f;
            if (has("Source snapshot ready at"))
                progress = 0.34f;
            if (has("Downloading managed vcpkg") || has("managed vcpkg toolchain"))
                progress = 0.38f;
            if (has("Downloading managed vcpkg fallback") || has("managed fallback toolchain"))
                progress = 0.40f;
            if (has("Bootstrapping managed vcpkg") || has("Validating signature"))
                progress = 0.41f;
            if (has("Managed vcpkg ready") || has("Managed vcpkg toolchain is ready"))
                progress = 0.42f;
            if (has("Initializing managed vcpkg git registry") || has("Reconfigured the source snapshot"))
                progress = 0.43f;
            if (has("Restoring source dependencies"))
                progress = 0.44f;
            if (has("Source dependencies restored"))
                progress = 0.54f;
            if (has("MSBuild Release attempt") || has("MSBuild attempt"))
                progress = 0.62f;
            if (has("completed successfully"))
                progress = 0.82f;
            if (has("Built runtime ready at"))
                progress = 0.92f;
            if (has("Waiting for runtime handoff"))
                progress = 0.96f;
            if (has("Source runtime files copied successfully") || has("Restarted updated runtime"))
                progress = 1.0f;
            return std::clamp(progress, 0.02f, 1.0f);
        }

        [[nodiscard]] float editor_source_worker_progress_soft_cap(
            const std::string_view status,
            const float anchoredProgress,
            const bool cancelRequested) noexcept
        {
            const auto has = [&](const std::string_view needle) noexcept
                {
                    return status.find(needle) != std::string_view::npos;
                };

            if (cancelRequested)
                return anchoredProgress;
            if (has("Waiting for runtime handoff") || has("Built runtime ready at"))
                return 0.97f;
            if (has("completed successfully"))
                return 0.92f;
            if (has("MSBuild Release attempt") || has("MSBuild attempt"))
                return 0.90f;
            if (has("Source dependencies restored"))
                return 0.72f;
            if (has("Restoring source dependencies"))
                return 0.68f;
            if (has("Initializing managed vcpkg")
                || has("Reconfigured the source snapshot")
                || has("git registry"))
            {
                return 0.56f;
            }
            if (has("Managed vcpkg")
                || has("Bootstrapping managed vcpkg")
                || has("Downloading managed vcpkg")
                || has("Validating signature")
                || has("toolchain"))
            {
                return 0.50f;
            }
            if (has("Source snapshot ready"))
                return 0.40f;
            if (has("Source snapshot") || has("Downloading latest"))
                return 0.32f;
            return (std::min)(0.94f, (std::max)(anchoredProgress + 0.06f, 0.14f));
        }

        [[nodiscard]] float editor_source_worker_progress_value(const EditorState& editor) noexcept
        {
            const float anchoredProgress = editor_source_worker_progress_from_status(
                editor.updateStatus,
                editor.updateSourceCancelRequested);
            if (editor.updateSourceCancelRequested)
                return anchoredProgress;

            const float cap = (std::max)(
                anchoredProgress,
                editor_source_worker_progress_soft_cap(
                    editor.updateStatus,
                    anchoredProgress,
                    editor.updateSourceCancelRequested));
            const float elapsedLift = static_cast<float>(
                (std::max)(0.0, editor_update_elapsed_seconds(editor)) * 0.0028);
            return std::clamp((std::min)(cap, anchoredProgress + elapsedLift), 0.02f, 1.0f);
        }

        [[nodiscard]] std::string format_editor_update_elapsed_seconds(const int totalSeconds)
        {
            const int clampedSeconds = (std::max)(0, totalSeconds);
            const int minutes = clampedSeconds / 60;
            const int seconds = clampedSeconds % 60;
            if (minutes <= 0)
                return std::format("{}s", seconds);
            return std::format("{}m {:02}s", minutes, seconds);
        }

        [[nodiscard]] std::string editor_source_worker_progress_phase(
            const EditorState& editor,
            const bool cancelRequested)
        {
            const std::string_view status{ editor.updateStatus };
            const auto has = [&](const std::string_view needle) noexcept
                {
                    return status.find(needle) != std::string_view::npos;
                };

            if (cancelRequested)
                return "cancel requested";
            if (has("Downloading managed vcpkg fallback") || has("managed fallback toolchain"))
                return "vcpkg fallback";
            if (has("Bootstrapping managed vcpkg") || has("Validating signature"))
                return "vcpkg bootstrap";
            if (has("Downloading managed vcpkg") || has("managed vcpkg toolchain"))
                return "vcpkg download";
            if (has("Managed vcpkg ready")
                || has("Initializing managed vcpkg")
                || has("Reconfigured the source snapshot")
                || has("git registry"))
            {
                return "vcpkg registry";
            }
            if (has("Restoring source dependencies"))
                return "dependency restore";
            if (has("MSBuild Release attempt") || has("MSBuild attempt"))
                return "compiler";
            if (has("Source snapshot") || has("Downloading latest"))
                return "source download";
            return "working";
        }

        [[nodiscard]] std::string editor_source_worker_progress_status(
            const EditorState& editor,
            const bool cancelRequested)
        {
            const auto elapsed = static_cast<int>((std::max)(0.0, editor_update_elapsed_seconds(editor)));
            return editor_source_worker_progress_phase(editor, cancelRequested)
                + " "
                + format_editor_update_elapsed_seconds(elapsed);
        }

        [[nodiscard]] int editor_source_cancel_arm_seconds_remaining(const EditorState& editor)
        {
            const double remaining = kEditorSourceCancelArmSeconds - editor_update_elapsed_seconds(editor);
            return static_cast<int>(std::ceil((std::max)(0.0, remaining)));
        }

        [[nodiscard]] bool editor_source_cancel_available(const EditorState& editor)
        {
            const bool sourceWorkActive =
                editor.updateState == EditorUpdateState::SourceWorkerRunning
                || editor.updateSourceInstallPending;
            if (!sourceWorkActive
                || editor.updateSourceCancelRequested
                || editor.updateOperationStartedAt == std::chrono::steady_clock::time_point{})
            {
                return false;
            }

            if (editor_update_elapsed_seconds(editor) < kEditorSourceCancelArmSeconds)
                return false;

            return updater::source_update_worker_active();
        }

        [[nodiscard]] float editor_update_activity_phase(const EditorState& editor)
        {
            if (editor.updateOperationStartedAt == std::chrono::steady_clock::time_point{})
            {
                const double nowSeconds = std::chrono::duration<double>(
                    std::chrono::steady_clock::now().time_since_epoch()).count();
                return static_cast<float>(std::fmod(nowSeconds * 0.35, 1.0));
            }

            return static_cast<float>(std::fmod(editor_update_elapsed_seconds(editor) * 0.35, 1.0));
        }

        [[nodiscard]] float editor_update_progress_value(const EditorState& editor)
        {
            if (editor.updateState == EditorUpdateState::SourceWorkerRunning
                || (editor.updateSourceInstallPending && updater::source_update_worker_active()))
            {
                return editor_source_worker_progress_value(editor);
            }

            if (!editor.updateCheckPending)
            {
                return editor.updateState == EditorUpdateState::RestartReady ? 1.0f : 0.0f;
            }

            const double elapsed = editor_update_elapsed_seconds(editor);
            if (!editor.updateInstallPending)
                return static_cast<float>(std::clamp(0.10 + elapsed * 0.08, 0.10, 0.88));

            if (editor.updateSourceInstallPending)
                return static_cast<float>(std::clamp(0.12 + elapsed * 0.012, 0.12, 0.94));

            return static_cast<float>(std::clamp(0.12 + elapsed * 0.025, 0.12, 0.94));
        }

        [[nodiscard]] std::string editor_update_running_status(const EditorState& editor)
        {
            if (editor.updateProjectSourceDownloadPending)
                return "Downloading project source code into the project source cache. This does not update or restart the runtime.";

            if (editor.updateSourceInstallPending)
                return "Source rebuild is running. Reading logs/epoch_source_update.log and logs/epoch_update_handoff.log for live evidence; Cancel asks the worker to stop at the next safe checkpoint.";

            if (editor.updateInstallPending)
                return "Installing update. Checking platform release, replacing stale cache, and staging replacement.";

            return "Checking update availability. Epoch checks this platform's packaged release first.";
        }

        [[nodiscard]] std::string read_update_log_tail(const std::filesystem::path& path, const std::uintmax_t maxBytes = 2400)
        {
            std::error_code sizeEc;
            const auto size = std::filesystem::file_size(path, sizeEc);
            if (sizeEc || size == 0)
                return {};

            std::ifstream stream(path, std::ios::binary);
            if (!stream)
                return {};

            const std::uintmax_t readSize = (std::min)(size, maxBytes);
            if (size > readSize)
                stream.seekg(static_cast<std::streamoff>(size - readSize), std::ios::beg);

            std::string text;
            text.resize(static_cast<std::size_t>(readSize));
            stream.read(text.data(), static_cast<std::streamsize>(text.size()));
            text.resize(static_cast<std::size_t>((std::max)(std::streamsize{ 0 }, stream.gcount())));
            return text;
        }

        [[nodiscard]] std::string last_nonempty_update_log_line(std::string_view text)
        {
            while (!text.empty() && (text.back() == '\n' || text.back() == '\r'))
                text.remove_suffix(1);
            if (text.empty())
                return {};

            const auto pos = text.find_last_of("\r\n");
            const std::string_view line = pos == std::string_view::npos ? text : text.substr(pos + 1);
            return std::string{ line };
        }

        [[nodiscard]] std::string visible_update_evidence_line(
            const std::string_view sourceText,
            const std::string_view handoffText)
        {
            std::string sourceLine = last_nonempty_update_log_line(sourceText);
            const bool sourceLineIsUpdaterEvidence =
                sourceLine.find("[INFO]") != std::string::npos
                || sourceLine.find("[WARN]") != std::string::npos
                || sourceLine.find("[ERROR]") != std::string::npos
                || sourceLine.find("[FATAL]") != std::string::npos;
            if (!sourceLine.empty() && sourceLineIsUpdaterEvidence)
                return sourceLine;

            std::string handoffLine = last_nonempty_update_log_line(handoffText);
            if (!handoffLine.empty())
                return handoffLine;

            return sourceLine;
        }

        [[nodiscard]] bool update_log_contains(std::string_view text, std::string_view needle) noexcept
        {
            return text.find(needle) != std::string_view::npos;
        }

        [[nodiscard]] std::atomic<bool>& editor_update_operation_running() noexcept
        {
            static std::atomic<bool> running{ false };
            return running;
        }

        [[nodiscard]] bool try_claim_editor_update_operation() noexcept
        {
            bool expected = false;
            return editor_update_operation_running().compare_exchange_strong(
                expected,
                true,
                std::memory_order_acq_rel);
        }

        void release_editor_update_operation() noexcept
        {
            editor_update_operation_running().store(false, std::memory_order_release);
        }

        struct ScopedEditorUpdateOperation final
        {
            ~ScopedEditorUpdateOperation()
            {
                release_editor_update_operation();
            }
        };

        void adopt_editor_source_update_worker(EditorState& editor);
        void start_editor_source_update_install(EditorState& editor);
        void start_editor_project_source_code_download(EditorState& editor);

        void start_editor_update_check(EditorState& editor, const bool passive = false)
        {
            if (guard_editor_update_retry_wait(editor))
                return;

            if (editor.updateCheckPending.has_value())
            {
                push_editor_log(editor, "[update] Update check is already running.");
                return;
            }

            if (editor.updateState != EditorUpdateState::RestartReady
                && editor.updateState != EditorUpdateState::SourceWorkerRunning
                && apply_cached_editor_update_discovery(editor))
            {
                if (!passive)
                    push_editor_log(editor, std::string{ "[update] " } + editor.updateStatus);
                return;
            }

            if (guard_editor_recent_source_cancel(editor))
                return;

            if (updater::source_update_worker_active())
            {
                adopt_editor_source_update_worker(editor);
                return;
            }

            if (!try_claim_editor_update_operation())
            {
                editor.autoUpdateCheckQueued = true;
                editor.updateStatus = "Another editor pane is already checking or installing updates.";
                if (!passive)
                    push_editor_log(editor, std::string{ "[update] " } + editor.updateStatus);
                return;
            }

            editor.updateState = EditorUpdateState::Checking;
            editor.updateStatus = "Checking for updates...";
            editor.updateInstallPending = false;
            editor.updateSourceInstallPending = false;
            editor.updateProjectSourceDownloadPending = false;
            editor.updateSourceCancelRequested = false;
            clear_editor_update_retry_wait(editor);
            clear_editor_update_restart_countdown(editor);
            editor.updateOperationStartedAt = std::chrono::steady_clock::now();
            push_editor_log(editor, "[update] Checking for available Epoch updates.");

            try
            {
                editor.updateCheckPending.emplace(std::async(std::launch::async, [] {
                    ScopedEditorUpdateOperation updateOperation{};
                    epochengine::systems::threading::ScopedThreadActivity threadActivity{};
                    try
                    {
                        return updater::run_update_command(editor_update_channel(), false, false);
                    }
                    catch (const std::exception& ex)
                    {
                        return make_editor_update_failure_result(
                            std::string{ "Update check failed: " } + ex.what());
                    }
                    catch (...)
                    {
                        return make_editor_update_failure_result(
                            "Update check failed with an unknown exception.");
                    }
                }));
            }
            catch (...)
            {
                release_editor_update_operation();
                throw;
            }
        }

        void adopt_editor_source_update_worker(EditorState& editor)
        {
            editor.updateState = EditorUpdateState::SourceWorkerRunning;
            editor.updateInstallPending = false;
            editor.updateSourceInstallPending = false;
            editor.updateProjectSourceDownloadPending = false;
            editor.updateSourceCancelRequested = updater::source_update_cancel_requested();
            editor.showUpdateConfirmModal = true;
            if (editor.updateOperationStartedAt == std::chrono::steady_clock::time_point{})
                editor.updateOperationStartedAt = std::chrono::steady_clock::now();
            clear_editor_update_restart_countdown(editor);
            editor.updateStatus = editor.updateSourceCancelRequested
                ? "A source rebuild is already stopping. Waiting for worker cleanup before retry."
                : "A source rebuild is already active. Waiting for worker evidence before starting another update.";
            push_editor_log(editor, std::string{ "[update] " } + editor.updateStatus);
        }

        void start_editor_update_install(EditorState& editor)
        {
            if (guard_editor_update_retry_wait(editor))
                return;

            if (editor.updateCheckPending.has_value())
            {
                push_editor_log(editor, "[update] Update worker is already running.");
                return;
            }

            if (guard_editor_recent_source_cancel(editor))
                return;

            if (updater::source_update_worker_active())
            {
                adopt_editor_source_update_worker(editor);
                return;
            }

            if (editor_update_result_is_source_only(editor.lastUpdateCheck))
            {
                push_editor_log(editor, "[update] Source-only update confirmed from the smart update modal.");
                start_editor_source_update_install(editor);
                return;
            }

            if (!try_claim_editor_update_operation())
            {
                editor.updateStatus = "Another editor pane is already checking or installing updates.";
                push_editor_log(editor, std::string{ "[update] " } + editor.updateStatus);
                return;
            }

            editor.updateState = EditorUpdateState::Checking;
            editor.updateStatus = "Installing the best available update. If main source is newer, Epoch builds it locally; packaged releases are used only when no newer source lane is available.";
            editor.updateInstallPending = true;
            editor.updateSourceInstallPending = false;
            editor.updateProjectSourceDownloadPending = false;
            editor.updateSourceCancelRequested = false;
            clear_editor_update_retry_wait(editor);
            clear_editor_update_restart_countdown(editor);
            editor.updateOperationStartedAt = std::chrono::steady_clock::now();
            push_editor_log(editor, "[update] Installing through the source-preferred update gate.");

            try
            {
                editor.updateCheckPending.emplace(std::async(std::launch::async, [] {
                    ScopedEditorUpdateOperation updateOperation{};
                    epochengine::systems::threading::ScopedThreadActivity threadActivity{};
                    try
                    {
                        return updater::run_update_command(
                            editor_update_channel(),
                            true,
                            false,
                            updater::UpdateHandoffMode::StageForRestart);
                    }
                    catch (const std::exception& ex)
                    {
                        return make_editor_update_failure_result(
                            std::string{ "Update install failed: " } + ex.what());
                    }
                    catch (...)
                    {
                        return make_editor_update_failure_result(
                            "Update install failed with an unknown exception.");
                    }
                }));
            }
            catch (...)
            {
                release_editor_update_operation();
                throw;
            }
        }

        void start_editor_source_update_install(EditorState& editor)
        {
            if (guard_editor_update_retry_wait(editor))
                return;

            if (editor.updateCheckPending.has_value())
            {
                push_editor_log(editor, "[update] Source update worker is already running.");
                return;
            }

            if (guard_editor_recent_source_cancel(editor))
                return;

            if (updater::source_update_worker_active())
            {
                adopt_editor_source_update_worker(editor);
                return;
            }

            if (!try_claim_editor_update_operation())
            {
                editor.updateStatus = "Another editor pane is already checking or installing updates.";
                push_editor_log(editor, std::string{ "[update] " } + editor.updateStatus);
                return;
            }

            editor.updateState = EditorUpdateState::Checking;
            editor.updateStatus = "Launching the advanced source rebuild worker. Use this only when you intentionally want latest main source instead of the packaged platform release.";
            editor.updateInstallPending = true;
            editor.updateSourceInstallPending = true;
            editor.updateProjectSourceDownloadPending = false;
            editor.updateSourceCancelRequested = false;
            clear_editor_update_retry_wait(editor);
            clear_editor_update_restart_countdown(editor);
            editor.updateOperationStartedAt = std::chrono::steady_clock::now();
            push_editor_log(editor, "[update] Launching advanced source rebuild worker.");

            try
            {
                editor.updateCheckPending.emplace(std::async(std::launch::async, [] {
                    ScopedEditorUpdateOperation updateOperation{};
                    epochengine::systems::threading::ScopedThreadActivity threadActivity{};
                    updater::UpdateCommandResult result{};
                    result.update_available = true;
                    result.source_update_available = true;
                    result.source_fallback_attempted = true;
                    try
                    {
                        const bool workerLaunched = updater::run_source_update_command(editor_update_channel(), false, false, false);
                        result.update_performed = false;
                        result.source_update_performed = workerLaunched;
                        result.status_message = workerLaunched
                            ? "Source rebuild worker started. Epoch will restart automatically only after build and restart evidence succeeds; watch logs/epoch_source_update.log beside the executable."
                            : "Source update failed to start. Check logs/epoch_source_update.log and logs/epoch_update_handoff.log beside the executable.";
                    }
                    catch (const std::exception& ex)
                    {
                        result.status_message = std::string{ "Source update failed to start: " } + ex.what();
                    }
                    catch (...)
                    {
                        result.status_message = "Source update failed to start with an unknown exception.";
                    }
                    return result;
                }));
            }
            catch (...)
            {
                release_editor_update_operation();
                throw;
            }
        }

        void start_editor_project_source_code_download(EditorState& editor)
        {
            if (guard_editor_update_retry_wait(editor))
                return;

            if (editor.updateCheckPending.has_value())
            {
                push_editor_log(editor, "[update] Project source code download is already running.");
                return;
            }

            if (guard_editor_recent_source_cancel(editor))
                return;

            if (updater::source_update_worker_active())
            {
                editor.updateStatus = "Project source code download waits while the source update worker is active.";
                push_editor_log(editor, std::string{ "[update] " } + editor.updateStatus);
                adopt_editor_source_update_worker(editor);
                return;
            }

            if (!try_claim_editor_update_operation())
            {
                editor.updateStatus = "Another editor pane is already checking, installing, or downloading source.";
                push_editor_log(editor, std::string{ "[update] " } + editor.updateStatus);
                return;
            }

            editor.updateState = EditorUpdateState::Checking;
            editor.updateStatus = "Downloading project source code into the project source cache. This will not update, rebuild, restart, or replace Epoch.";
            editor.updateInstallPending = false;
            editor.updateSourceInstallPending = false;
            editor.updateProjectSourceDownloadPending = true;
            editor.updateSourceCancelRequested = false;
            clear_editor_update_retry_wait(editor);
            clear_editor_update_restart_countdown(editor);
            editor.updateOperationStartedAt = std::chrono::steady_clock::now();
            push_editor_log(editor, "[update] Project source code download started.");

            try
            {
                editor.updateCheckPending.emplace(std::async(std::launch::async, [] {
                    ScopedEditorUpdateOperation updateOperation{};
                    epochengine::systems::threading::ScopedThreadActivity threadActivity{};
                    updater::UpdateCommandResult result{};
                    try
                    {
                        const updater::ProjectSourceDownloadResult download =
                            updater::download_project_source_code(editor_update_channel());
                        result.update_performed = download.ok;
                        result.status_message = download.status_message;
                    }
                    catch (const std::exception& ex)
                    {
                        result.status_message = std::string{ "Project source code download failed: " } + ex.what();
                    }
                    catch (...)
                    {
                        result.status_message = "Project source code download failed with an unknown exception.";
                    }
                    return result;
                }));
            }
            catch (...)
            {
                release_editor_update_operation();
                throw;
            }
        }

        void pump_editor_update_check(EditorState& editor)
        {
            if (!editor.updateCheckPending.has_value())
                return;

            if (editor.updateCheckPending->wait_for(0s) != std::future_status::ready)
            {
                if (editor.updateSourceInstallPending && updater::source_update_worker_active())
                {
                    const std::string sourceTail = read_update_log_tail(updater::source_update_log_path());
                    const std::string handoffTail = read_update_log_tail(updater::update_handoff_log_path());
                    const std::string evidence = visible_update_evidence_line(sourceTail, handoffTail);
                    editor.updateStatus = evidence.empty()
                        ? editor_update_running_status(editor)
                        : evidence;
                }
                else
                {
                    editor.updateStatus = editor_update_running_status(editor);
                }
                return;
            }

            const bool installPending = editor.updateInstallPending;
            const bool sourceInstallPending = editor.updateSourceInstallPending;
            const bool projectSourceDownloadPending = editor.updateProjectSourceDownloadPending;
            const updater::UpdateCommandResult previousUpdateCheck = editor.lastUpdateCheck;

            try
            {
                updater::UpdateCommandResult completedUpdateCheck = editor.updateCheckPending->get();
                if (!installPending && !projectSourceDownloadPending)
                    remember_editor_update_discovery(completedUpdateCheck);
                editor.lastUpdateCheck = completedUpdateCheck;
                editor.updateCheckPending.reset();
                const bool sourceWorkerRunning =
                    installPending
                    && completedUpdateCheck.source_fallback_attempted
                    && completedUpdateCheck.source_update_performed
                    && !completedUpdateCheck.packaged_update_performed;
                editor.updateInstallPending = false;
                editor.updateSourceInstallPending = false;
                editor.updateProjectSourceDownloadPending = false;
                if (!sourceWorkerRunning)
                    editor.updateSourceCancelRequested = false;
                if (!sourceWorkerRunning)
                    editor.updateOperationStartedAt = {};

                if (projectSourceDownloadPending)
                {
                    editor.updateState = editor.lastUpdateCheck.update_performed
                        ? EditorUpdateState::Idle
                        : EditorUpdateState::Failed;
                    editor.updateSourceCancelRequested = false;
                    editor.updateStatus = describe_update_result(editor.lastUpdateCheck);
                    editor.showUpdateConfirmModal = !editor.lastUpdateCheck.update_performed;
                    push_editor_log(editor, std::string{ "[update] " } + editor.updateStatus);
                    return;
                }

                if (installPending)
                {
                    if (sourceWorkerRunning)
                    {
                        editor.lastUpdateCheck = previousUpdateCheck;
                        make_editor_source_attempt_retryable(editor, completedUpdateCheck);
                        editor.updateState = EditorUpdateState::SourceWorkerRunning;
                        editor.updateSourceCancelRequested = false;
                        editor.updateStatus = describe_update_result(completedUpdateCheck);
                        editor.showUpdateConfirmModal = true;
                        push_editor_log(editor, std::string{ "[update] " } + editor.updateStatus);
                        return;
                    }

                    if (sourceInstallPending
                        && editor.lastUpdateCheck.source_fallback_attempted
                        && !editor.lastUpdateCheck.source_update_performed)
                    {
                        editor.updateState = EditorUpdateState::Failed;
                        editor.updateSourceCancelRequested = false;
                        editor.updateStatus = describe_update_result(editor.lastUpdateCheck);
                        push_editor_log(editor, std::string{ "[update] " } + editor.updateStatus);
                        return;
                    }

                    if (editor.lastUpdateCheck.platform_build_checked
                        && !editor.lastUpdateCheck.platform_build_ok)
                    {
                        editor.updateState = EditorUpdateState::Failed;
                        editor.updateSourceCancelRequested = false;
                        editor.updateStatus = describe_update_result(editor.lastUpdateCheck);
                        push_editor_log(editor, std::string{ "[update] " } + editor.updateStatus);
                        return;
                    }

                    if (editor.lastUpdateCheck.update_performed)
                    {
                        editor.updateState = EditorUpdateState::RestartReady;
                        editor.updateSourceCancelRequested = false;
                        editor.updateStatus = describe_update_result(editor.lastUpdateCheck);
                        arm_editor_update_restart_countdown(editor);
                        push_editor_log(editor, std::string{ "[update] " } + editor.updateStatus);
                        return;
                    }

                    editor.updateState = EditorUpdateState::Failed;
                    editor.updateSourceCancelRequested = false;
                    editor.updateStatus = describe_update_result(editor.lastUpdateCheck);
                    push_editor_log(editor, std::string{ "[update] " } + editor.updateStatus);
                    return;
                }

                if (editor.lastUpdateCheck.update_available || editor.lastUpdateCheck.force_required)
                {
                    editor.updateState = EditorUpdateState::Available;
                    editor.updateSourceCancelRequested = false;
                    editor.updateStatus = describe_update_result(editor.lastUpdateCheck);
                    push_editor_log(editor, std::string{ "[update] " } + editor.updateStatus);
                    return;
                }

                editor.updateState = EditorUpdateState::Idle;
                editor.updateSourceCancelRequested = false;
                editor.updateStatus = describe_update_result(editor.lastUpdateCheck);
                push_editor_log(editor, std::string{ "[update] " } + editor.updateStatus);
            }
            catch (...)
            {
                editor.updateCheckPending.reset();
                editor.updateInstallPending = false;
                editor.updateSourceInstallPending = false;
                editor.updateProjectSourceDownloadPending = false;
                editor.updateSourceCancelRequested = false;
                editor.updateOperationStartedAt = {};
                clear_editor_update_restart_countdown(editor);
                editor.updateState = EditorUpdateState::Failed;
                editor.updateStatus = "Update check failed; see updater logs for details.";
                push_editor_log(editor, std::string{ "[update] " } + editor.updateStatus);
            }
        }

        void pump_editor_source_update_worker_unchecked(EditorState& editor)
        {
            if (editor.updateState != EditorUpdateState::SourceWorkerRunning)
                return;

            const std::string handoffTail = read_update_log_tail(updater::update_handoff_log_path());
            const std::string sourceTail = read_update_log_tail(updater::source_update_log_path());
            const std::string_view evidence = !sourceTail.empty() ? std::string_view{ sourceTail } : std::string_view{ handoffTail };

            const bool sourceWorkerCancelEvidence =
                update_log_contains(handoffTail, "Source update cancel complete")
                || update_log_contains(sourceTail, "Source update cancel complete")
                || update_log_contains(handoffTail, "Source update canceled by operator")
                || update_log_contains(sourceTail, "Source update canceled by operator");

            if (sourceWorkerCancelEvidence && editor.updateSourceCancelRequested)
            {
                make_editor_source_attempt_retryable(editor, editor.lastUpdateCheck);
                editor.updateState = EditorUpdateState::Available;
                editor.updateSourceCancelRequested = false;
                editor.updateStatus = "Source update canceled. Update remains available if you want to retry.";
                editor.showUpdateConfirmModal = true;
                editor.updateOperationStartedAt = {};
                arm_editor_update_retry_wait(editor);
                clear_editor_update_restart_countdown(editor);
                push_editor_log(editor, std::string{ "[update] " } + editor.updateStatus);
                return;
            }

            const bool sourceWorkerActive = updater::source_update_worker_active();
            const bool sourceWorkerCanceled =
                (sourceWorkerCancelEvidence || editor.updateSourceCancelRequested)
                && !sourceWorkerActive;

            if (sourceWorkerCanceled)
            {
                make_editor_source_attempt_retryable(editor, editor.lastUpdateCheck);
                editor.updateState = EditorUpdateState::Available;
                editor.updateSourceCancelRequested = false;
                editor.updateStatus = "Source update canceled. Update remains available if you want to retry.";
                editor.showUpdateConfirmModal = true;
                editor.updateOperationStartedAt = {};
                arm_editor_update_retry_wait(editor);
                clear_editor_update_restart_countdown(editor);
                push_editor_log(editor, std::string{ "[update] " } + editor.updateStatus);
                return;
            }

            if (sourceWorkerCancelEvidence && sourceWorkerActive)
            {
                editor.updateSourceCancelRequested = true;
                if (editor.updateOperationStartedAt == std::chrono::steady_clock::time_point{})
                    editor.updateOperationStartedAt = std::chrono::steady_clock::now();
                editor.updateStatus = "Source update cancel reached the worker. Waiting for process cleanup before retry.";
                return;
            }

            const bool sourceWorkerErrorEvidence =
                update_log_contains(handoffTail, "[ERROR]")
                || update_log_contains(sourceTail, "[ERROR]");
            if (sourceWorkerErrorEvidence && sourceWorkerActive)
            {
                const std::string lastLine = last_nonempty_update_log_line(
                    update_log_contains(sourceTail, "[ERROR]") ? std::string_view{ sourceTail } : std::string_view{ handoffTail });
                editor.updateStatus = lastLine.empty()
                    ? "Source update reported an error and is still cleaning up worker state before retry."
                    : std::string{ "Source update cleanup after error:\n" } + editor_update_modal::format_status_markers(lastLine);
                return;
            }

            if (sourceWorkerErrorEvidence)
            {
                const std::string lastLine = last_nonempty_update_log_line(
                    update_log_contains(sourceTail, "[ERROR]") ? std::string_view{ sourceTail } : std::string_view{ handoffTail });
                editor.updateState = EditorUpdateState::Failed;
                editor.updateSourceCancelRequested = false;
                editor.updateStatus = lastLine.empty()
                    ? "Source update failed. Check logs/epoch_source_update.log and logs/epoch_update_handoff.log beside the executable."
                    : std::string{ "Source update failed:\n" } + editor_update_modal::format_status_markers(lastLine);
                editor.updateOperationStartedAt = {};
                clear_editor_update_restart_countdown(editor);
                push_editor_log(editor, std::string{ "[update] " } + editor.updateStatus);
                return;
            }

            if (update_log_contains(handoffTail, "Waiting for runtime handoff")
                || update_log_contains(sourceTail, "Built runtime ready at"))
            {
                editor.updateState = EditorUpdateState::RestartReady;
                editor.updateSourceCancelRequested = false;
                editor.updateStatus = "Source rebuild is ready for runtime replacement. Press Restart to close Epoch and let the worker replace the executable.";
                editor.updateOperationStartedAt = {};
                editor.showUpdateConfirmModal = true;
                arm_editor_update_restart_countdown(editor);
                push_editor_log(editor, std::string{ "[update] " } + editor.updateStatus);
                return;
            }

            if (update_log_contains(handoffTail, "Restarted updated runtime")
                || update_log_contains(handoffTail, "Source runtime files copied successfully"))
            {
                editor.updateState = EditorUpdateState::RestartReady;
                editor.updateSourceCancelRequested = false;
                editor.updateStatus = "Source update replacement completed. Restart Epoch if this window did not close automatically.";
                editor.updateOperationStartedAt = {};
                editor.showUpdateConfirmModal = true;
                arm_editor_update_restart_countdown(editor);
                push_editor_log(editor, std::string{ "[update] " } + editor.updateStatus);
                return;
            }

            const std::string lastLine = visible_update_evidence_line(evidence, handoffTail);
            if (!lastLine.empty())
                editor.updateStatus = std::string{ "Source rebuild running. Latest evidence:\n" }
                    + editor_update_modal::format_status_markers(lastLine);
        }

        void pump_editor_source_update_worker(EditorState& editor)
        {
            try
            {
                pump_editor_source_update_worker_unchecked(editor);
            }
            catch (const std::exception& ex)
            {
                editor.updateState = EditorUpdateState::Failed;
                editor.updateInstallPending = false;
                editor.updateSourceInstallPending = false;
                editor.updateProjectSourceDownloadPending = false;
                editor.updateSourceCancelRequested = false;
                editor.updateOperationStartedAt = {};
                editor.showUpdateConfirmModal = true;
                clear_editor_update_restart_countdown(editor);
                arm_editor_update_retry_wait(editor);
                editor.updateStatus = std::string{ "Source update monitor failed: " } + ex.what();
                push_editor_log(editor, std::string{ "[update] " } + editor.updateStatus);
            }
            catch (...)
            {
                editor.updateState = EditorUpdateState::Failed;
                editor.updateInstallPending = false;
                editor.updateSourceInstallPending = false;
                editor.updateProjectSourceDownloadPending = false;
                editor.updateSourceCancelRequested = false;
                editor.updateOperationStartedAt = {};
                editor.showUpdateConfirmModal = true;
                clear_editor_update_restart_countdown(editor);
                arm_editor_update_retry_wait(editor);
                editor.updateStatus = "Source update monitor failed with an unknown error.";
                push_editor_log(editor, std::string{ "[update] " } + editor.updateStatus);
            }
        }

        void request_editor_source_update_cancel(EditorState& editor)
        {
            if (editor.updateSourceCancelRequested)
            {
                editor.updateState = EditorUpdateState::SourceWorkerRunning;
                editor.showUpdateConfirmModal = true;
                editor.updateStatus =
                    "Source rebuild cancellation is already requested. Waiting for the worker checkpoint before retry.";
                push_editor_log(editor, std::string{ "[update] " } + editor.updateStatus);
                return;
            }

            editor.updateInstallPending = false;
            editor.updateSourceInstallPending = false;
            editor.updateProjectSourceDownloadPending = false;
            clear_editor_update_restart_countdown(editor);

            editor.updateState = EditorUpdateState::SourceWorkerRunning;
            editor.updateSourceCancelRequested = true;
            editor.showUpdateConfirmModal = true;
            if (editor.updateOperationStartedAt == std::chrono::steady_clock::time_point{})
                editor.updateOperationStartedAt = std::chrono::steady_clock::now();
            editor.updateStatus =
                "Source rebuild cancellation requested. Waiting for the worker to stop at its next safe checkpoint before retry.";
            if (!updater::request_source_update_cancel())
            {
                editor.updateStatus =
                    "Source rebuild cancellation could not write the cancel marker. Check logs/epoch_source_update.log beside the executable.";
            }

            push_editor_log(editor, std::string{ "[update] " } + editor.updateStatus);
        }
