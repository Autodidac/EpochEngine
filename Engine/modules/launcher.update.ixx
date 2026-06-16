module;

#include <algorithm>
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
        std::chrono::steady_clock::time_point operation_started_at{};
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
            operation_started_at = std::chrono::steady_clock::now();
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
            }
            else
            {
                surface_active = false;
                operation_started_at = {};
                clear_restart_countdown();
            }

            status = describe_result(last_result);
        }

        void complete_pending_error()
        {
            pending.reset();
            surface_active = false;
            packaged_restart_ready = false;
            source_worker_running = false;
            source_restart_ready = false;
            cancel_requested = false;
            operation_started_at = {};
            clear_restart_countdown();
            status = "Update failed before the updater could report evidence.";
        }

        void request_source_cancel(const bool requested)
        {
            cancel_requested = requested;
            status = requested
                ? "Source update cancel requested. The worker will stop at its next safe checkpoint."
                : "Source update cancel could not be requested; check updater logs beside EpochEditor.exe.";
        }

        void mark_packaged_restart_failed()
        {
            status = "Restart failed: staged update handoff script was not found.";
        }

        void mark_packaged_restarting()
        {
            clear_restart_countdown();
            status = "Restarting Epoch to finish the staged package update.";
        }

        void mark_source_restart_requested()
        {
            clear_restart_countdown();
            status = "Closing Epoch so the source handoff can replace and restart the runtime.";
        }

        void pump_source_worker()
        {
            if (!source_worker_running || source_restart_ready)
                return;

            const std::string handoffTail = read_update_log_tail(
                epochnamespace::updater::update_handoff_log_path());
            const std::string sourceTail = read_update_log_tail(
                epochnamespace::updater::source_update_log_path());
            const std::string_view evidence = !handoffTail.empty()
                ? std::string_view{ handoffTail }
                : std::string_view{ sourceTail };

            if (update_log_contains(handoffTail, "Source update cancel")
                || update_log_contains(sourceTail, "Source update cancel"))
            {
                surface_active = false;
                source_worker_running = false;
                source_restart_ready = false;
                cancel_requested = false;
                operation_started_at = {};
                clear_restart_countdown();
                status = "Source update canceled. Update remains available if you want to retry.";
                return;
            }

            if (update_log_contains(handoffTail, "[ERROR]") || update_log_contains(sourceTail, "[ERROR]"))
            {
                surface_active = false;
                source_worker_running = false;
                source_restart_ready = false;
                cancel_requested = false;
                operation_started_at = {};
                clear_restart_countdown();
                const std::string line = last_nonempty_update_log_line(evidence);
                status = line.empty()
                    ? "Source update failed. Check epoch_source_update.log beside EpochEditor.exe."
                    : "Source update failed: " + line;
                return;
            }

            if (update_log_contains(handoffTail, "Waiting for runtime handoff")
                || update_log_contains(sourceTail, "Built runtime ready at"))
            {
                surface_active = true;
                source_worker_running = false;
                source_restart_ready = true;
                cancel_requested = false;
                operation_started_at = {};
                arm_restart_countdown();
                status = "Source build is ready. Restart will close this launcher and let the handoff finish.";
                return;
            }

            const std::string line = last_nonempty_update_log_line(evidence);
            if (!line.empty())
                status = "Source update running: " + line;
        }

        [[nodiscard]] double update_elapsed_seconds() const
        {
            if (operation_started_at == std::chrono::steady_clock::time_point{})
                return 0.0;

            return std::chrono::duration<double>(
                std::chrono::steady_clock::now() - operation_started_at).count();
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
                return static_cast<float>((std::clamp)(0.14 + elapsed * 0.004, 0.14, 0.98));

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
                || is_restart_ready();
            if (!panel.active)
                return panel;

            panel.progress = progress_value();
            panel.status = status;

            if (is_restart_ready())
            {
                arm_restart_countdown();
                const int seconds = static_cast<int>(std::ceil(restart_seconds_remaining()));
                panel.title = packaged_restart_ready ? "Restart Epoch" : "Restart Source-Built Epoch";
                panel.progress_label = "Update staged";
                panel.progress_status = std::format("auto restart in {}s", seconds);
                panel.action_label = std::format("Restart Now ({}s)", seconds);
                panel.action_enabled = true;
                panel.restart_ready = true;
                return panel;
            }

            panel.title = "Updating Epoch";
            panel.progress_label = source_worker_running ? "Source rebuild" : "Update";
            if (cancel_requested)
            {
                panel.progress_status = "cancel requested";
                panel.action_label = "Cancel Requested";
                panel.action_enabled = false;
            }
            else if (source_worker_running)
            {
                panel.progress_status = "cancel available";
                panel.action_label = "Cancel Update";
                panel.action_enabled = true;
                panel.cancel_available = true;
            }
            else
            {
                panel.progress_status = pending ? "checking / staging" : "waiting";
                panel.action_label = pending ? "Preparing Update" : "Update Epoch";
                panel.action_enabled = !pending;
            }
            return panel;
        }

    private:
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
                return std::string{ "Packaged update is staged. Restart will close this launcher and let the hidden handoff replace the runtime." };

            if (result.source_update_performed)
                return std::string{ "Source rebuild worker started. Keep this launcher open until it reports handoff-ready evidence." };

            if (result.force_required || result.update_available)
            {
                if (result.source_update_available && !result.packaged_update_available)
                    return std::string{ "Source update is available. Update will build current main source locally." };
                return std::string{ "Update is available. Press Update Epoch to install." };
            }

            return std::string{ "Epoch is already current." };
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
    };
}
