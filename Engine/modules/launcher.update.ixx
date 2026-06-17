module;

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <format>
#include <fstream>
#include <future>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

export module launcher.update;

import engine.updater;
import gui.menu;

export namespace epochnamespace::launcher_update
{
    inline constexpr double kRestartCountdownSeconds = 10.0;
    inline constexpr int kCancelRetryCooldownSeconds = 5;
    inline constexpr double kSourceCancelArmSeconds = 8.0;

    enum class RestartKind : unsigned char
    {
        none = 0,
        packaged,
        source
    };

    class Flow final
    {
    public:
        std::optional<std::future<epochnamespace::updater::UpdateCommandResult>> pending{};
        epochnamespace::updater::UpdateCommandResult last_result{};
        bool surface_active{ false };
        bool packaged_restart_ready{ false };
        bool source_worker_running{ false };
        bool source_restart_ready{ false };
        bool cancel_requested{ false };
        bool result_visible{ false };
        float source_progress{ 0.0f };
        std::chrono::steady_clock::time_point operation_started_at{};
        std::chrono::steady_clock::time_point cancel_requested_at{};
        std::chrono::steady_clock::time_point update_check_retry_after{};
        bool restart_countdown_armed{ false };
        std::chrono::steady_clock::time_point restart_countdown_started_at{};
        std::string status{ "Update checks run in this launcher window." };

        void set_status(std::string value)
        {
            status = std::move(value);
        }

        [[nodiscard]] bool has_pending_work() const noexcept
        {
            return pending.has_value();
        }

        [[nodiscard]] bool is_restart_ready() const noexcept
        {
            return packaged_restart_ready || source_restart_ready;
        }

        [[nodiscard]] bool has_visible_result() const noexcept
        {
            return result_visible;
        }

        [[nodiscard]] bool source_cancel_pending() const noexcept
        {
            return source_worker_running && cancel_requested;
        }

        [[nodiscard]] bool update_check_throttled() const noexcept
        {
            return std::chrono::steady_clock::now() < update_check_retry_after;
        }

        [[nodiscard]] int update_check_retry_seconds() const noexcept
        {
            if (!update_check_throttled())
                return 0;

            const auto remaining = update_check_retry_after - std::chrono::steady_clock::now();
            return static_cast<int>(std::ceil(std::chrono::duration<double>(remaining).count()));
        }

        [[nodiscard]] RestartKind restart_kind() const noexcept
        {
            if (packaged_restart_ready)
                return RestartKind::packaged;
            if (source_restart_ready)
                return RestartKind::source;
            return RestartKind::none;
        }

        [[nodiscard]] bool pending_ready() const
        {
            return pending
                && pending->wait_for(std::chrono::milliseconds{ 0 }) == std::future_status::ready;
        }

        [[nodiscard]] epochnamespace::updater::UpdateCommandResult take_pending_result()
        {
            auto result = pending->get();
            pending.reset();
            return result;
        }

        void begin_update_check(std::future<epochnamespace::updater::UpdateCommandResult> future)
        {
            pending.emplace(std::move(future));
            surface_active = true;
            packaged_restart_ready = false;
            source_worker_running = false;
            source_restart_ready = false;
            cancel_requested = false;
            result_visible = false;
            source_progress = 0.0f;
            operation_started_at = std::chrono::steady_clock::now();
            cancel_requested_at = {};
            update_check_retry_after = {};
            clear_restart_countdown();
            status = "Checking GitHub releases, source version, and platform build gate...";
        }

        void complete_pending_result(epochnamespace::updater::UpdateCommandResult result)
        {
            last_result = std::move(result);
            packaged_restart_ready = last_result.packaged_handoff_staged;
            source_worker_running = last_result.source_update_performed && !last_result.packaged_update_performed;
            source_restart_ready = false;
            cancel_requested = false;
            result_visible = false;
            source_progress = source_worker_running ? 0.08f : 0.0f;
            cancel_requested_at = {};

            if (packaged_restart_ready)
            {
                surface_active = true;
                operation_started_at = {};
                arm_restart_countdown();
            }
            else if (source_worker_running)
            {
                surface_active = true;
                operation_started_at = std::chrono::steady_clock::now();
                clear_restart_countdown();
                make_source_attempt_retryable();
            }
            else
            {
                surface_active = true;
                result_visible = true;
                operation_started_at = {};
                clear_restart_countdown();
                if (!last_result.update_available
                    && !last_result.force_required
                    && !last_result.packaged_handoff_staged
                    && !last_result.source_update_performed)
                {
                    update_check_retry_after = std::chrono::steady_clock::now() + std::chrono::seconds{ 60 };
                }
            }

            status = source_worker_running
                ? "Source rebuild worker started. Keep this launcher open until restart-ready evidence is reported."
                : describe_result(last_result);
        }

        void complete_pending_error()
        {
            pending.reset();
            surface_active = true;
            packaged_restart_ready = false;
            source_worker_running = false;
            source_restart_ready = false;
            cancel_requested = false;
            result_visible = true;
            source_progress = 0.0f;
            operation_started_at = {};
            cancel_requested_at = {};
            clear_restart_countdown();
            update_check_retry_after = std::chrono::steady_clock::now() + std::chrono::seconds{ 15 };
            status = "Update failed before the updater could report evidence.";
        }

        void mark_update_start_failed(std::string message)
        {
            pending.reset();
            surface_active = true;
            packaged_restart_ready = false;
            source_worker_running = false;
            source_restart_ready = false;
            cancel_requested = false;
            result_visible = true;
            source_progress = 0.0f;
            operation_started_at = {};
            cancel_requested_at = {};
            clear_restart_countdown();
            update_check_retry_after = std::chrono::steady_clock::now() + std::chrono::seconds{ 15 };
            status = std::move(message);
            if (status.empty())
                status = "Update failed before the updater worker could start.";
        }

        void clear_inactive_surface()
        {
            if (pending.has_value() || source_worker_running || is_restart_ready())
                return;

            surface_active = false;
            cancel_requested = false;
            result_visible = false;
            source_progress = 0.0f;
            operation_started_at = {};
            cancel_requested_at = {};
            update_check_retry_after = {};
            clear_restart_countdown();
            status = "Update checks run in this launcher window.";
        }

        void dismiss_result()
        {
            if (!result_visible)
                return;

            surface_active = false;
            result_visible = false;
            packaged_restart_ready = false;
            source_worker_running = false;
            source_restart_ready = false;
            cancel_requested = false;
            source_progress = 0.0f;
            operation_started_at = {};
            cancel_requested_at = {};
            clear_restart_countdown();
        }

        void show_retry_wait()
        {
            surface_active = true;
            packaged_restart_ready = false;
            source_worker_running = false;
            source_restart_ready = false;
            cancel_requested = false;
            result_visible = true;
            source_progress = 0.0f;
            operation_started_at = {};
            cancel_requested_at = {};
            clear_restart_countdown();
            status = "Update retry is cooling down so the previous cancel can finish clearing worker state. Try again in "
                + std::to_string(update_check_retry_seconds())
                + "s.";
        }

        void show_retry_wait_for_seconds(const int seconds)
        {
            if (seconds > 0)
            {
                update_check_retry_after =
                    std::chrono::steady_clock::now() + std::chrono::seconds{ seconds };
            }

            show_retry_wait();
        }

        void make_source_attempt_retryable()
        {
            last_result.update_performed = false;
            last_result.packaged_update_performed = false;
            last_result.packaged_handoff_staged = false;
            last_result.source_update_performed = false;
            last_result.source_fallback_attempted = false;
            last_result.status_message.clear();

            if (!last_result.update_available
                && !last_result.force_required
                && !last_result.packaged_update_available
                && !last_result.source_update_available)
            {
                last_result.update_available = true;
                last_result.source_update_available = true;
            }
        }

        void request_source_cancel(const bool requested)
        {
            if (cancel_requested)
            {
                status = "Source rebuild cancellation is already requested. Waiting for the worker to stop at its next safe checkpoint.";
                return;
            }

            cancel_requested = requested;
            if (requested)
                cancel_requested_at = std::chrono::steady_clock::now();
            status = requested
                ? "Source rebuild cancellation requested. Waiting for the worker to stop at its next safe checkpoint."
                : "Source update cancel could not be requested; check updater logs in the logs folder beside EpochEditor.exe.";
        }

        void mark_source_cancel_complete()
        {
            surface_active = true;
            source_worker_running = false;
            source_restart_ready = false;
            cancel_requested = false;
            result_visible = true;
            make_source_attempt_retryable();
            source_progress = 0.0f;
            operation_started_at = {};
            cancel_requested_at = {};
            clear_restart_countdown();
            if constexpr (kCancelRetryCooldownSeconds > 0)
            {
                update_check_retry_after =
                    std::chrono::steady_clock::now() + std::chrono::seconds{ kCancelRetryCooldownSeconds };
            }
            status = "Source update canceled. Update remains available if you want to retry.";
        }

        void mark_packaged_restart_failed()
        {
            status = "Restart failed: staged replacement script was not found.";
        }

        void mark_packaged_restarting()
        {
            clear_restart_countdown();
            status = "Restarting Epoch to finish the staged package update.";
        }

        void mark_source_restart_requested()
        {
            clear_restart_countdown();
            status = "Closing Epoch so the source-built replacement can restart the runtime.";
        }

        void observe_existing_source_worker(const bool cancel_pending)
        {
            surface_active = true;
            packaged_restart_ready = false;
            source_worker_running = true;
            source_restart_ready = false;
            cancel_requested = cancel_pending;
            result_visible = false;
            source_progress = (std::max)(source_progress, 0.08f);
            if (operation_started_at == std::chrono::steady_clock::time_point{})
                operation_started_at = std::chrono::steady_clock::now();
            if (cancel_pending && cancel_requested_at == std::chrono::steady_clock::time_point{})
                cancel_requested_at = std::chrono::steady_clock::now();
            clear_restart_countdown();
            status = cancel_pending
                ? "A source rebuild is already stopping. Waiting for worker cleanup before retry."
                : "A source rebuild is already active. Waiting for worker evidence before starting another update.";
        }

        void pump_source_worker()
        {
            if (!source_worker_running || source_restart_ready)
                return;

            const std::string handoffTail = read_update_log_tail(
                epochnamespace::updater::update_handoff_log_path());
            const std::string sourceTail = read_update_log_tail(
                epochnamespace::updater::source_update_log_path());
            const std::string_view evidence = !sourceTail.empty()
                ? std::string_view{ sourceTail }
                : std::string_view{ handoffTail };

            const bool sourceWorkerCancelEvidence =
                update_log_contains(handoffTail, "Source update cancel complete")
                || update_log_contains(sourceTail, "Source update cancel complete")
                || update_log_contains(handoffTail, "Source update canceled by operator")
                || update_log_contains(sourceTail, "Source update canceled by operator");

            if (sourceWorkerCancelEvidence && cancel_requested)
            {
                mark_source_cancel_complete();
                return;
            }

            const bool sourceWorkerActive =
                epochnamespace::updater::source_update_worker_active();
            const bool sourceWorkerCanceled =
                (sourceWorkerCancelEvidence || cancel_requested)
                && !sourceWorkerActive;

            if (sourceWorkerCanceled)
            {
                mark_source_cancel_complete();
                return;
            }

            if (sourceWorkerCancelEvidence && sourceWorkerActive)
            {
                surface_active = true;
                cancel_requested = true;
                if (cancel_requested_at == std::chrono::steady_clock::time_point{})
                    cancel_requested_at = std::chrono::steady_clock::now();
                source_progress = canceled_source_progress();
                status = "Source update cancel reached the worker. Waiting for process cleanup before retry.";
                return;
            }

            const bool sourceWorkerErrorEvidence =
                update_log_contains(handoffTail, "[ERROR]")
                || update_log_contains(sourceTail, "[ERROR]");
            if (sourceWorkerErrorEvidence && sourceWorkerActive)
            {
                const std::string line = last_nonempty_update_log_line(
                    update_log_contains(sourceTail, "[ERROR]") ? std::string_view{ sourceTail } : std::string_view{ handoffTail });
                status = line.empty()
                    ? "Source update reported an error and is still cleaning up worker state before retry."
                    : status_with_evidence("Source update cleanup after error:", line);
                return;
            }

            if (sourceWorkerErrorEvidence)
            {
                surface_active = false;
                source_worker_running = false;
                source_restart_ready = false;
                cancel_requested = false;
                result_visible = false;
                source_progress = 0.0f;
                operation_started_at = {};
                cancel_requested_at = {};
                clear_restart_countdown();
                update_check_retry_after = std::chrono::steady_clock::now() + std::chrono::seconds{ 15 };
                const std::string line = last_nonempty_update_log_line(
                    update_log_contains(sourceTail, "[ERROR]") ? std::string_view{ sourceTail } : std::string_view{ handoffTail });
                status = line.empty()
                    ? "Source update failed. Check logs/epoch_source_update.log beside EpochEditor.exe."
                    : status_with_evidence("Source update failed:", line);
                return;
            }

            if (update_log_contains(handoffTail, "Waiting for runtime handoff")
                || update_log_contains(sourceTail, "Built runtime ready at"))
            {
                surface_active = true;
                source_worker_running = false;
                source_restart_ready = true;
                cancel_requested = false;
                source_progress = 1.0f;
                operation_started_at = {};
                cancel_requested_at = {};
                arm_restart_countdown();
                status = "Source build is ready. Restart will close this launcher and finish the runtime replacement.";
                return;
            }

            const std::string line = visible_update_evidence_line(evidence, handoffTail);
            source_progress = source_progress_from_evidence(sourceTail, handoffTail, source_progress);
            if (cancel_requested)
                source_progress = canceled_source_progress();
            if (!line.empty())
                status = status_with_evidence("Source update running:", line);
        }

        [[nodiscard]] double update_elapsed_seconds() const
        {
            if (operation_started_at == std::chrono::steady_clock::time_point{})
                return 0.0;

            return std::chrono::duration<double>(
                std::chrono::steady_clock::now() - operation_started_at).count();
        }

        [[nodiscard]] int update_elapsed_seconds_whole() const
        {
            return static_cast<int>((std::max)(0.0, update_elapsed_seconds()));
        }

        [[nodiscard]] static std::string format_elapsed_seconds(const int totalSeconds)
        {
            const int clampedSeconds = (std::max)(0, totalSeconds);
            const int minutes = clampedSeconds / 60;
            const int seconds = clampedSeconds % 60;
            if (minutes <= 0)
                return std::format("{}s", seconds);
            return std::format("{}m {:02}s", minutes, seconds);
        }

        [[nodiscard]] std::string source_progress_phase() const
        {
            const auto has = [&](const std::string_view needle) noexcept
                {
                    return status.find(needle) != std::string::npos;
                };

            if (cancel_requested)
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
            if (pending)
                return "checking";
            return "working";
        }

        [[nodiscard]] std::string source_progress_status() const
        {
            return source_progress_phase() + " " + format_elapsed_seconds(update_elapsed_seconds_whole());
        }

        [[nodiscard]] float activity_phase() const
        {
            if (operation_started_at == std::chrono::steady_clock::time_point{})
                return 0.0f;

            const double elapsed = update_elapsed_seconds();
            return static_cast<float>(std::fmod(elapsed * 0.35, 1.0));
        }

        [[nodiscard]] int source_cancel_arm_seconds_remaining() const
        {
            const double remaining = kSourceCancelArmSeconds - update_elapsed_seconds();
            return static_cast<int>(std::ceil((std::max)(0.0, remaining)));
        }

        [[nodiscard]] bool source_cancel_available() const
        {
            return source_worker_running
                && !cancel_requested
                && operation_started_at != std::chrono::steady_clock::time_point{}
                && update_elapsed_seconds() >= kSourceCancelArmSeconds;
        }

        [[nodiscard]] double restart_seconds_remaining() const
        {
            if (!restart_countdown_armed
                || restart_countdown_started_at == std::chrono::steady_clock::time_point{})
            {
                return kRestartCountdownSeconds;
            }

            const double elapsed = std::chrono::duration<double>(
                std::chrono::steady_clock::now() - restart_countdown_started_at).count();
            return (std::max)(0.0, kRestartCountdownSeconds - elapsed);
        }

        [[nodiscard]] bool restart_countdown_expired() const
        {
            return restart_countdown_armed && restart_seconds_remaining() <= 0.0;
        }

        void clear_restart_countdown() noexcept
        {
            restart_countdown_armed = false;
            restart_countdown_started_at = {};
        }

        void arm_restart_countdown()
        {
            if (restart_countdown_armed)
                return;

            restart_countdown_armed = true;
            restart_countdown_started_at = std::chrono::steady_clock::now();
        }

        [[nodiscard]] float progress_value() const
        {
            if (is_restart_ready())
                return 1.0f;

            const double elapsed = update_elapsed_seconds();
            if (source_worker_running)
                return std::clamp(source_progress_with_elapsed(source_progress), 0.02f, 0.98f);

            if (pending)
                return static_cast<float>((std::clamp)(0.10 + elapsed * 0.025, 0.10, 0.94));

            return 0.0f;
        }

        [[nodiscard]] epochnamespace::menu::LauncherUpdatePanelState panel_state()
        {
            epochnamespace::menu::LauncherUpdatePanelState panel{};
            panel.active = surface_active
                || pending.has_value()
                || source_worker_running
                || is_restart_ready()
                || result_visible;
            if (!panel.active)
                return panel;

            panel.progress = progress_value();
            panel.status = format_log_markers(status);
            panel.progress_active = pending.has_value() || source_worker_running;
            panel.activity_phase = activity_phase();

            if (is_restart_ready())
            {
                arm_restart_countdown();
                const int seconds = static_cast<int>(std::ceil(restart_seconds_remaining()));
                panel.title = packaged_restart_ready ? "Restart Epoch" : "Restart Source-Built Epoch";
                panel.progress_label = "Update staged";
                panel.progress_status = std::format("auto restart in {}s", seconds);
                panel.action_label = std::format("Restart Now ({}s)", seconds);
                panel.action_enabled = true;
                panel.action_choice = epochnamespace::menu::Choice::UpdatePanelRestart;
                panel.restart_ready = true;
                return panel;
            }

            if (result_visible)
            {
                panel.title = "Update Check Complete";
                panel.progress_label = "Update";
                panel.progress_status = "ready";
                panel.progress = 1.0f;
                panel.action_label = "Back To Launcher";
                panel.action_enabled = true;
                panel.action_accepts_enter = true;
                panel.action_choice = epochnamespace::menu::Choice::UpdatePanelDismiss;
                return panel;
            }

            panel.title = "Updating Epoch";
            panel.progress_label = source_worker_running ? "Source rebuild" : "Update";
            panel.show_percent = true;
            if (cancel_requested)
            {
                panel.progress_status = source_progress_status();
                panel.action_label.clear();
                panel.action_enabled = false;
                panel.action_visible = false;
            }
            else if (source_worker_running)
            {
                if (source_cancel_available())
                {
                    panel.progress_status = source_progress_status();
                    panel.action_label = "Cancel Update";
                    panel.action_enabled = true;
                    panel.action_accepts_enter = false;
                    panel.action_choice = epochnamespace::menu::Choice::UpdatePanelCancel;
                    panel.cancel_available = true;
                }
                else
                {
                    panel.progress_status = std::format(
                        "{} - cancel update available in {}s",
                        source_progress_status(),
                        source_cancel_arm_seconds_remaining());
                    panel.action_label.clear();
                    panel.action_enabled = false;
                    panel.action_visible = false;
                    panel.cancel_available = false;
                }
            }
            else
            {
                panel.progress_status = pending ? source_progress_status() : "waiting";
                panel.action_label = pending ? std::string{} : std::string{ "Update Epoch" };
                panel.action_enabled = !pending;
                panel.action_visible = !pending;
            }
            return panel;
        }

    private:
        [[nodiscard]] float canceled_source_progress() const
        {
            if (cancel_requested_at == std::chrono::steady_clock::time_point{})
                return (std::max)(0.04f, source_progress);

            const double elapsed = std::chrono::duration<double>(
                std::chrono::steady_clock::now() - cancel_requested_at).count();
            return (std::max)(0.04f, source_progress - static_cast<float>(elapsed * 0.16));
        }

        [[nodiscard]] float source_progress_soft_cap() const noexcept
        {
            const auto has = [&](const std::string_view needle) noexcept
                {
                    return status.find(needle) != std::string::npos;
                };

            if (cancel_requested)
                return source_progress;
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
            return (std::min)(0.94f, (std::max)(source_progress + 0.06f, 0.14f));
        }

        [[nodiscard]] float source_progress_with_elapsed(const float anchoredProgress) const noexcept
        {
            if (cancel_requested)
                return anchoredProgress;

            const float cap = (std::max)(anchoredProgress, source_progress_soft_cap());
            const float elapsedLift = static_cast<float>((std::max)(0.0, update_elapsed_seconds()) * 0.0028);
            return (std::min)(cap, anchoredProgress + elapsedLift);
        }

        [[nodiscard]] static float source_progress_from_evidence(
            const std::string_view source_text,
            const std::string_view handoff_text,
            const float previous) noexcept
        {
            const auto has = [&](const std::string_view needle) noexcept
                {
                    return update_log_contains(source_text, needle)
                        || update_log_contains(handoff_text, needle);
                };

            float progress = (std::max)(previous, 0.08f);
            if (has("Downloading latest") || has("Source snapshot download URL"))
                progress = (std::max)(progress, 0.16f);
            if (has("downloaded successfully") || has("Source snapshot downloaded and extracted"))
                progress = (std::max)(progress, 0.28f);
            if (has("Source snapshot ready at"))
                progress = (std::max)(progress, 0.34f);
            if (has("Downloading managed vcpkg") || has("managed vcpkg toolchain"))
                progress = (std::max)(progress, 0.38f);
            if (has("Downloading managed vcpkg fallback") || has("managed fallback toolchain"))
                progress = (std::max)(progress, 0.40f);
            if (has("Bootstrapping managed vcpkg") || has("Validating signature"))
                progress = (std::max)(progress, 0.41f);
            if (has("Managed vcpkg ready") || has("Managed vcpkg toolchain is ready"))
                progress = (std::max)(progress, 0.42f);
            if (has("Initializing managed vcpkg git registry") || has("Reconfigured the source snapshot"))
                progress = (std::max)(progress, 0.43f);
            if (has("Restoring source dependencies"))
                progress = (std::max)(progress, 0.44f);
            if (has("Source dependencies restored"))
                progress = (std::max)(progress, 0.54f);
            if (has("MSBuild Release attempt") || has("MSBuild attempt"))
                progress = (std::max)(progress, 0.62f);
            if (has("completed successfully"))
                progress = (std::max)(progress, 0.82f);
            if (has("Built runtime ready at"))
                progress = (std::max)(progress, 0.92f);
            if (has("Waiting for runtime handoff"))
                progress = (std::max)(progress, 0.96f);
            if (has("Source runtime files copied successfully") || has("Restarted updated runtime"))
                progress = 1.0f;
            return std::clamp(progress, 0.02f, 1.0f);
        }

        [[nodiscard]] static std::string describe_result(
            const epochnamespace::updater::UpdateCommandResult& result)
        {
            if (!result.status_message.empty())
                return result.status_message;

            if (result.platform_build_checked && !result.platform_build_ok)
            {
                const std::string job = result.platform_build_job.empty()
                    ? std::string{ "platform build" }
                    : result.platform_build_job;
                const std::string reason = result.platform_build_reason.empty()
                    ? std::string{ "build status could not be proven." }
                    : result.platform_build_reason;
                return "Update withheld until " + job + " is green: " + reason;
            }

            if (result.packaged_handoff_staged)
                return std::string{ "Packaged update is staged. Restart will close this launcher and finish the hidden runtime replacement." };

            if (result.source_update_performed)
                return std::string{ "Source rebuild worker started. Keep this launcher open until restart-ready evidence is reported." };

            if (result.force_required || result.update_available)
            {
                if (result.packaged_update_available && result.source_update_available)
                {
                    const std::string packaged = result.remote_version.empty()
                        ? std::string{ "packaged runtime" }
                        : std::string{ "packaged runtime " } + result.remote_version;
                    const std::string source = result.source_remote_version.empty()
                        ? std::string{ "main source" }
                        : std::string{ "main source " } + result.source_remote_version;
                    return "Update is available: " + packaged + " first; " + source + " is also available after restart.";
                }
                if (result.source_update_available && !result.packaged_update_available)
                    return std::string{ "Source update is available. Update will build current main source locally." };
                return std::string{ "Update is available. Press Update Epoch to install." };
            }

            return std::string{ "Epoch is already current." };
        }

        [[nodiscard]] static std::string format_log_markers(std::string text)
        {
            constexpr std::array<std::string_view, 4> kMarkers{
                "[INFO]",
                "[WARN]",
                "[ERROR]",
                "[FATAL]"
            };

            std::string out;
            out.reserve(text.size() + 8u);

            for (std::size_t i = 0u; i < text.size();)
            {
                bool matched = false;
                for (const std::string_view marker : kMarkers)
                {
                    if (marker.size() <= text.size() - i
                        && text.compare(i, marker.size(), marker) == 0)
                    {
                        if (!out.empty() && out.back() != '\n' && out.back() != '\r')
                            out.push_back('\n');
                        out.append(marker);
                        i += marker.size();
                        matched = true;
                        break;
                    }
                }

                if (!matched)
                {
                    out.push_back(text[i]);
                    ++i;
                }
            }

            return out;
        }

        [[nodiscard]] static std::string status_with_evidence(
            const std::string_view prefix,
            const std::string_view evidence)
        {
            std::string status{ prefix };
            if (!evidence.empty())
            {
                status.push_back('\n');
                status += format_log_markers(std::string{ evidence });
            }
            return status;
        }

        [[nodiscard]] static std::string read_update_log_tail(
            const std::filesystem::path& path,
            const std::uintmax_t maxBytes = 8192)
        {
            std::error_code sizeEc;
            const auto size = std::filesystem::file_size(path, sizeEc);
            if (sizeEc || size == 0)
                return {};

            std::ifstream in(path, std::ios::binary);
            if (!in)
                return {};

            const std::uintmax_t start = size > maxBytes ? size - maxBytes : 0;
            in.seekg(static_cast<std::streamoff>(start), std::ios::beg);
            std::string text;
            text.resize(static_cast<std::size_t>(size - start));
            in.read(text.data(), static_cast<std::streamsize>(text.size()));
            text.resize(static_cast<std::size_t>(in.gcount()));
            return text;
        }

        [[nodiscard]] static bool update_log_contains(
            const std::string_view text,
            const std::string_view needle) noexcept
        {
            return !needle.empty() && text.find(needle) != std::string_view::npos;
        }

        [[nodiscard]] static std::string last_nonempty_update_log_line(std::string_view text)
        {
            while (!text.empty() && (text.back() == '\n' || text.back() == '\r'))
                text.remove_suffix(1);

            const std::size_t pos = text.find_last_of("\r\n");
            std::string_view line = pos == std::string_view::npos ? text : text.substr(pos + 1);
            while (!line.empty() && (line.front() == '\n' || line.front() == '\r'))
                line.remove_prefix(1);
            return std::string{ line };
        }

        [[nodiscard]] static std::string visible_update_evidence_line(
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
    };
}
