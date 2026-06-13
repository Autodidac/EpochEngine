        // Included from editor.cpp inside the editor implementation-detail namespace.
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

        [[nodiscard]] std::string describe_update_result(const updater::UpdateCommandResult& result)
        {
            if (!result.status_message.empty())
                return result.status_message;

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

        constexpr auto kEditorUpdateRestartDelay = std::chrono::seconds{ 10 };

        void clear_editor_update_restart_countdown(EditorState& editor) noexcept
        {
            editor.updateRestartReadyAt = {};
        }

        void arm_editor_update_restart_countdown(EditorState& editor)
        {
            if (editor.updateRestartReadyAt == std::chrono::steady_clock::time_point{})
                editor.updateRestartReadyAt = std::chrono::steady_clock::now();
        }

        [[nodiscard]] int editor_update_restart_countdown_seconds(const EditorState& editor)
        {
            if (editor.updateRestartReadyAt == std::chrono::steady_clock::time_point{})
                return static_cast<int>(kEditorUpdateRestartDelay.count());

            const auto elapsed = std::chrono::steady_clock::now() - editor.updateRestartReadyAt;
            if (elapsed >= kEditorUpdateRestartDelay)
                return 0;

            const auto remaining = kEditorUpdateRestartDelay - elapsed;
            const auto remainingMs = std::chrono::duration_cast<std::chrono::milliseconds>(remaining).count();
            return static_cast<int>((remainingMs + 999) / 1000);
        }

        [[nodiscard]] bool editor_update_restart_countdown_elapsed(const EditorState& editor)
        {
            return editor.updateRestartReadyAt != std::chrono::steady_clock::time_point{}
                && std::chrono::steady_clock::now() - editor.updateRestartReadyAt >= kEditorUpdateRestartDelay;
        }

        [[nodiscard]] float editor_update_progress_value(const EditorState& editor)
        {
            if (editor.updateState == EditorUpdateState::SourceWorkerRunning)
            {
                const double elapsed = editor_update_elapsed_seconds(editor);
                return static_cast<float>(std::clamp(0.14 + elapsed * 0.004, 0.14, 0.98));
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
            if (editor.updateSourceInstallPending)
                return "Source rebuild is running. Reading epoch_source_update.log and epoch_update_handoff.log for live evidence; Cancel asks the worker to stop at the next safe checkpoint.";

            if (editor.updateInstallPending)
                return "Installing update. Checking platform release, replacing stale cache, and staging handoff.";

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

        [[nodiscard]] bool update_log_contains(std::string_view text, std::string_view needle) noexcept
        {
            return text.find(needle) != std::string_view::npos;
        }

        [[nodiscard]] std::atomic<bool>& editor_startup_update_check_claimed() noexcept
        {
            static std::atomic<bool> claimed{ false };
            return claimed;
        }

        [[nodiscard]] bool try_claim_editor_startup_update_check() noexcept
        {
            bool expected = false;
            return editor_startup_update_check_claimed().compare_exchange_strong(
                expected,
                true,
                std::memory_order_acq_rel);
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

        void start_editor_update_check(EditorState& editor)
        {
            if (editor.updateCheckPending.has_value())
            {
                push_editor_log(editor, "[update] Update check is already running.");
                return;
            }

            if (!try_claim_editor_update_operation())
            {
                editor.updateStatus = "Another editor pane is already checking or installing updates.";
                push_editor_log(editor, std::string{ "[update] " } + editor.updateStatus);
                return;
            }

            editor.updateState = EditorUpdateState::Checking;
            editor.updateStatus = "Checking for updates...";
            editor.updateInstallPending = false;
            editor.updateSourceInstallPending = false;
            clear_editor_update_restart_countdown(editor);
            editor.updateOperationStartedAt = std::chrono::steady_clock::now();
            push_editor_log(editor, "[update] Checking for available Epoch updates.");

            try
            {
                editor.updateCheckPending.emplace(std::async(std::launch::async, [] {
                    ScopedEditorUpdateOperation updateOperation{};
                    epoch::systems::threading::ScopedThreadActivity threadActivity{};
                    return updater::run_update_command(editor_update_channel(), false, false);
                }));
            }
            catch (...)
            {
                release_editor_update_operation();
                throw;
            }
        }

        void start_editor_update_install(EditorState& editor)
        {
            if (editor.updateCheckPending.has_value())
            {
                push_editor_log(editor, "[update] Update worker is already running.");
                return;
            }

            if (!try_claim_editor_update_operation())
            {
                editor.updateStatus = "Another editor pane is already checking or installing updates.";
                push_editor_log(editor, std::string{ "[update] " } + editor.updateStatus);
                return;
            }

            editor.updateState = EditorUpdateState::Checking;
            editor.updateStatus = "Installing the best available update. Epoch checks packaged releases first, then falls back to source only when no newer package exists.";
            editor.updateInstallPending = true;
            editor.updateSourceInstallPending = false;
            clear_editor_update_restart_countdown(editor);
            editor.updateOperationStartedAt = std::chrono::steady_clock::now();
            push_editor_log(editor, "[update] Installing through the binary-first update gate.");

            try
            {
                editor.updateCheckPending.emplace(std::async(std::launch::async, [] {
                    ScopedEditorUpdateOperation updateOperation{};
                    epoch::systems::threading::ScopedThreadActivity threadActivity{};
                    return updater::run_update_command(editor_update_channel(), true, false);
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
            if (editor.updateCheckPending.has_value())
            {
                push_editor_log(editor, "[update] Source update worker is already running.");
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
            clear_editor_update_restart_countdown(editor);
            editor.updateOperationStartedAt = std::chrono::steady_clock::now();
            push_editor_log(editor, "[update] Launching advanced source rebuild worker.");

            try
            {
                editor.updateCheckPending.emplace(std::async(std::launch::async, [] {
                    ScopedEditorUpdateOperation updateOperation{};
                    epoch::systems::threading::ScopedThreadActivity threadActivity{};
                    updater::UpdateCommandResult result{};
                    result.update_available = true;
                    result.source_update_available = true;
                    result.source_fallback_attempted = true;
                    const bool workerLaunched = updater::run_source_update_command(editor_update_channel(), false, false, false);
                    result.update_performed = false;
                    result.source_update_performed = workerLaunched;
                    result.status_message = workerLaunched
                        ? "Source rebuild worker started. Epoch will restart automatically only after build and handoff evidence succeeds; watch epoch_source_update.log beside the executable."
                        : "Source update failed to start. Check epoch_source_update.log and epoch_update_handoff.log beside the executable.";
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
                editor.updateStatus = editor_update_running_status(editor);
                return;
            }

            const bool installPending = editor.updateInstallPending;
            const bool sourceInstallPending = editor.updateSourceInstallPending;

            try
            {
                editor.lastUpdateCheck = editor.updateCheckPending->get();
                editor.updateCheckPending.reset();
                const bool sourceWorkerRunning =
                    installPending
                    && editor.lastUpdateCheck.source_fallback_attempted
                    && editor.lastUpdateCheck.source_update_performed
                    && !editor.lastUpdateCheck.packaged_update_performed;
                editor.updateInstallPending = false;
                editor.updateSourceInstallPending = false;
                if (!sourceWorkerRunning)
                    editor.updateOperationStartedAt = {};

                if (installPending)
                {
                    if (sourceWorkerRunning)
                    {
                        editor.updateState = EditorUpdateState::SourceWorkerRunning;
                        editor.updateStatus = describe_update_result(editor.lastUpdateCheck);
                        editor.showUpdateConfirmModal = true;
                        push_editor_log(editor, std::string{ "[update] " } + editor.updateStatus);
                        return;
                    }

                    if (sourceInstallPending
                        && editor.lastUpdateCheck.source_fallback_attempted
                        && !editor.lastUpdateCheck.source_update_performed)
                    {
                        editor.updateState = EditorUpdateState::Failed;
                        editor.updateStatus = describe_update_result(editor.lastUpdateCheck);
                        push_editor_log(editor, std::string{ "[update] " } + editor.updateStatus);
                        return;
                    }

                    if (editor.lastUpdateCheck.platform_build_checked
                        && !editor.lastUpdateCheck.platform_build_ok)
                    {
                        editor.updateState = EditorUpdateState::Failed;
                        editor.updateStatus = describe_update_result(editor.lastUpdateCheck);
                        push_editor_log(editor, std::string{ "[update] " } + editor.updateStatus);
                        return;
                    }

                    if (editor.lastUpdateCheck.update_performed)
                    {
                        editor.updateState = EditorUpdateState::RestartReady;
                        editor.updateStatus = describe_update_result(editor.lastUpdateCheck);
                        arm_editor_update_restart_countdown(editor);
                        push_editor_log(editor, std::string{ "[update] " } + editor.updateStatus);
                        return;
                    }

                    editor.updateState = EditorUpdateState::Failed;
                    editor.updateStatus = describe_update_result(editor.lastUpdateCheck);
                    push_editor_log(editor, std::string{ "[update] " } + editor.updateStatus);
                    return;
                }

                if (editor.lastUpdateCheck.update_available || editor.lastUpdateCheck.force_required)
                {
                    editor.updateState = EditorUpdateState::Available;
                    editor.updateStatus = describe_update_result(editor.lastUpdateCheck);
                    push_editor_log(editor, std::string{ "[update] " } + editor.updateStatus);
                    return;
                }

                editor.updateState = EditorUpdateState::Idle;
                editor.updateStatus = describe_update_result(editor.lastUpdateCheck);
                push_editor_log(editor, std::string{ "[update] " } + editor.updateStatus);
            }
            catch (...)
            {
                editor.updateCheckPending.reset();
                editor.updateInstallPending = false;
                editor.updateSourceInstallPending = false;
                editor.updateOperationStartedAt = {};
                editor.updateState = EditorUpdateState::Failed;
                editor.updateStatus = "Update check failed; see updater logs for details.";
                push_editor_log(editor, std::string{ "[update] " } + editor.updateStatus);
            }
        }

        void pump_editor_source_update_worker(EditorState& editor)
        {
            if (editor.updateState != EditorUpdateState::SourceWorkerRunning)
                return;

            const std::string handoffTail = read_update_log_tail(updater::update_handoff_log_path());
            const std::string sourceTail = read_update_log_tail(updater::source_update_log_path());
            const std::string_view evidence = !handoffTail.empty() ? std::string_view{ handoffTail } : std::string_view{ sourceTail };

            if (update_log_contains(handoffTail, "Source update cancel")
                || update_log_contains(sourceTail, "Source update cancel"))
            {
                editor.updateState = EditorUpdateState::Available;
                editor.updateStatus = "Source update canceled. Update remains available if you want to retry.";
                editor.showUpdateConfirmModal = false;
                editor.updateOperationStartedAt = {};
                clear_editor_update_restart_countdown(editor);
                push_editor_log(editor, std::string{ "[update] " } + editor.updateStatus);
                return;
            }

            if (update_log_contains(handoffTail, "[ERROR]") || update_log_contains(sourceTail, "[ERROR]"))
            {
                const std::string lastLine = last_nonempty_update_log_line(!handoffTail.empty() ? handoffTail : sourceTail);
                editor.updateState = EditorUpdateState::Failed;
                editor.updateStatus = lastLine.empty()
                    ? "Source update failed. Check epoch_source_update.log and epoch_update_handoff.log beside the executable."
                    : std::string{ "Source update failed: " } + lastLine;
                editor.updateOperationStartedAt = {};
                clear_editor_update_restart_countdown(editor);
                push_editor_log(editor, std::string{ "[update] " } + editor.updateStatus);
                return;
            }

            if (update_log_contains(handoffTail, "Waiting for runtime handoff")
                || update_log_contains(sourceTail, "Built runtime ready at"))
            {
                editor.updateState = EditorUpdateState::RestartReady;
                editor.updateStatus = "Source rebuild is ready for runtime handoff. Press Restart to close Epoch and let the worker replace the executable.";
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
                editor.updateStatus = "Source update handoff completed. Restart Epoch if this window did not close automatically.";
                editor.updateOperationStartedAt = {};
                editor.showUpdateConfirmModal = true;
                arm_editor_update_restart_countdown(editor);
                push_editor_log(editor, std::string{ "[update] " } + editor.updateStatus);
                return;
            }

            const std::string lastLine = last_nonempty_update_log_line(evidence);
            if (!lastLine.empty())
                editor.updateStatus = std::string{ "Source rebuild running. Latest evidence: " } + lastLine;
        }
