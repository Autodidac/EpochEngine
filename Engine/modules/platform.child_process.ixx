module;

#include <cstdint>
#include <filesystem>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

export module platform.child_process;

export namespace epochengine::platform::child_process
{
    inline constexpr std::size_t maximum_processes = 8u;
    inline constexpr std::size_t maximum_arguments = 64u;

    struct ProcessHandle final
    {
        std::uint32_t slot{};
        std::uint32_t generation{};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return generation != 0u;
        }

        [[nodiscard]] friend constexpr bool operator==(
            const ProcessHandle&,
            const ProcessHandle&) noexcept = default;
    };

    enum class ProcessState : std::uint8_t
    {
        idle,
        starting,
        running,
        stop_requested,
        exited,
        failed
    };

    enum class WaitCode : std::uint8_t
    {
        exited,
        cancelled,
        timed_out,
        stale_handle,
        failed
    };
    enum class WindowMode : std::uint8_t
    {
        normal,
        hidden
    };

    enum class LaunchCode : std::uint8_t
    {
        started,
        focused_existing,
        active_group_busy,
        invalid_request,
        capacity_exhausted,
        spawn_failed
    };

    enum class FocusCode : std::uint8_t
    {
        focused,
        pending,
        unsupported,
        stale_handle,
        not_running,
        failed
    };

    enum class StopMode : std::uint8_t
    {
        graceful,
        force
    };

    enum class StopCode : std::uint8_t
    {
        requested,
        forced,
        unsupported,
        stale_handle,
        not_running,
        failed
    };

    struct LaunchRequest final
    {
        std::filesystem::path executable{};
        std::filesystem::path working_directory{};
        std::filesystem::path merged_output_path{};
        std::vector<std::string> arguments{};
        std::string correlation_key{};
        std::string exclusive_group{};
        std::string display_name{};
        WindowMode window_mode{WindowMode::normal};
        bool append_output{};
    };

    struct LaunchResult final
    {
        LaunchCode code{LaunchCode::invalid_request};
        ProcessHandle handle{};
        FocusCode focus{FocusCode::unsupported};
        std::string message{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == LaunchCode::started
                || code == LaunchCode::focused_existing;
        }
    };

    struct ProcessSnapshot final
    {
        ProcessHandle handle{};
        ProcessState state{ProcessState::idle};
        std::uint64_t platform_process_id{};
        std::filesystem::path executable{};
        std::string correlation_key{};
        std::string exclusive_group{};
        std::string display_name{};
        std::uint64_t started_tick_ns{};
        std::uint64_t finished_tick_ns{};
        std::uint64_t runtime_ns{};
        std::int32_t exit_code{};
        bool exit_code_valid{};
        bool focus_supported{};
        bool focus_pending{};
        bool stop_supported{};
        std::string message{};

        [[nodiscard]] constexpr bool active() const noexcept
        {
            return state == ProcessState::starting
                || state == ProcessState::running
                || state == ProcessState::stop_requested;
        }
    };

    struct WaitResult final
    {
        WaitCode code{WaitCode::failed};
        std::optional<ProcessSnapshot> process{};
        std::string message{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == WaitCode::exited;
        }
    };
    struct SupervisorMetrics final
    {
        std::uint64_t revision{};
        std::uint64_t launch_requests{};
        std::uint64_t launches{};
        std::uint64_t launch_failures{};
        std::uint64_t focus_requests{};
        std::uint64_t focus_successes{};
        std::uint64_t stop_requests{};
        std::uint64_t exits{};
        std::uint32_t active_processes{};
        std::uint32_t peak_active_processes{};
    };

    [[nodiscard]] std::string_view process_state_name(ProcessState state) noexcept;
    [[nodiscard]] std::string_view launch_code_name(LaunchCode code) noexcept;
    [[nodiscard]] std::string_view focus_code_name(FocusCode code) noexcept;
    [[nodiscard]] std::string_view stop_code_name(StopCode code) noexcept;
    [[nodiscard]] std::string_view wait_code_name(WaitCode code) noexcept;

    [[nodiscard]] LaunchResult launch_or_focus(const LaunchRequest& request) noexcept;
    [[nodiscard]] FocusCode focus(ProcessHandle handle) noexcept;
    [[nodiscard]] StopCode stop(ProcessHandle handle, StopMode mode) noexcept;
    [[nodiscard]] WaitResult wait(
        ProcessHandle handle,
        std::stop_token cancellation = {},
        std::uint64_t timeout_ns = 0u) noexcept;
    void poll() noexcept;
    [[nodiscard]] std::optional<ProcessSnapshot> snapshot(ProcessHandle handle) noexcept;
    [[nodiscard]] std::optional<ProcessSnapshot> active_in_group(std::string_view group) noexcept;
    [[nodiscard]] std::vector<ProcessSnapshot> snapshots() noexcept;
    [[nodiscard]] SupervisorMetrics metrics() noexcept;
    [[nodiscard]] bool release(ProcessHandle handle) noexcept;
}
