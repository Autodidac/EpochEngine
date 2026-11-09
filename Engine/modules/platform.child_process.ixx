// Epoch system notes (auto-checked):
// Epoch Platform: keep native code isolated behind platform-specific interfaces.
module;

#include <cstdint>
#include <cmath>
#include <filesystem>
#include <memory>
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
        hidden,
        // The owned GUI stays hidden until its verified host attachment places
        // it. Discovery is allowed; supervisor focus/reveal is not.
        deferred
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

    struct ExecutableIdentity final
    {
        std::uint64_t size_bytes{};
        std::string sha256{};

        [[nodiscard]] bool valid() const noexcept
        {
            if (size_bytes == 0u || size_bytes > 512u * 1024u * 1024u
                || sha256.size() != 64u)
                return false;
            bool nonzero{};
            for (const char ch : sha256)
            {
                if (!((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f')))
                    return false;
                nonzero = nonzero || ch != '0';
            }
            return nonzero;
        }

        [[nodiscard]] friend bool operator==(
            const ExecutableIdentity&, const ExecutableIdentity&) noexcept = default;
    };

    struct EnvironmentVariable final
    {
        std::string name{};
        std::string value{};

        [[nodiscard]] friend bool operator==(
            const EnvironmentVariable&, const EnvironmentVariable&) noexcept = default;
    };

    // Explicit OS policy, distinct from environment hygiene or image hashing.
    // Grants are disjoint directory trees below one host-owned generation.
    // Windows uses a fresh capability-free LPAC; unsupported platforms reject
    // the request rather than silently launching under the ordinary user token.
    struct WorkspaceIsolation final
    {
        std::filesystem::path owned_root{};
        std::vector<std::filesystem::path> read_only{};
        std::vector<std::filesystem::path> writable{};

        [[nodiscard]] friend bool operator==(
            const WorkspaceIsolation&, const WorkspaceIsolation&) noexcept = default;
    };

    enum class MeshDrawMode : std::uint8_t { solid, wireframe };
    enum class RuntimeControlStatus : std::uint8_t { unavailable, idle, pending, accepted, rejected, disconnected };
    struct RuntimeControlLease final
    {
        std::uint64_t project_generation{};
        std::string project_id{}, scene_id{};
        [[nodiscard]] friend bool operator==(const RuntimeControlLease&, const RuntimeControlLease&) = default;
    };
    struct RuntimeControlSnapshot final
    {
        RuntimeControlStatus status{RuntimeControlStatus::unavailable};
        std::uint64_t requested_sequence{}, acknowledged_sequence{};
        MeshDrawMode requested_mode{MeshDrawMode::solid}, acknowledged_mode{MeshDrawMode::solid};
        bool presentation_pending{},presentation_accepted{};
        std::uint64_t presentation_requested_sequence{},presentation_acknowledged_sequence{};
    };
    struct RuntimeMeshCommand final
    {
        std::uint64_t sequence{};
        MeshDrawMode mode{MeshDrawMode::solid};
    };
    // A presentation cursor driven by the child's existing monotonic clock.
    // This is not a negative physics step or a second simulation clock.
    struct RuntimePresentation final
    {
        double seconds{},duration_seconds{120.0},rate{1.0};
        bool reverse{},playing{};
        [[nodiscard]] friend bool operator==(const RuntimePresentation&,const RuntimePresentation&)=default;
    };
    [[nodiscard]] inline bool valid_runtime_presentation(const RuntimePresentation& value) noexcept
    {
        return std::isfinite(value.seconds)&&std::isfinite(value.duration_seconds)&&std::isfinite(value.rate)
            &&value.duration_seconds>=1.0&&value.duration_seconds<=86400.0
            &&value.seconds>=0.0&&value.seconds<=value.duration_seconds&&value.rate>=1.0&&value.rate<=8.0;
    }
    struct RuntimePresentationCommand final
    {
        std::uint64_t sequence{};
        RuntimePresentation sample{};
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
        // Optional Windows discovery contract. Empty preserves generic process
        // discovery; a deferred Epoch standalone host requests EpochChild so
        // hidden IME/helper windows cannot become its retained render surface.
        // A matching class is not backend readiness or presentation evidence.
        std::string expected_window_class{};
        bool append_output{};
        // Artifact integrity only; this grants no filesystem/network confinement.
        std::optional<ExecutableIdentity> expected_executable{};
        // nullopt preserves legacy inheritance; an engaged empty vector sends
        // an empty environment. Values are private, not snapshot/log evidence.
        std::optional<std::vector<EnvironmentVariable>> environment{};
        // Requires captured output. Use a fresh NUL stdin, never a duplicate
        // of the host's already-authorized input handle.
        bool disconnect_standard_input{};
        std::optional<WorkspaceIsolation> isolation{};
        // A newly owned process retains this resource until retirement. A
        // focused/busy result never adopts another caller's guard. Its
        // destructor must not reenter the process supervisor.
        std::shared_ptr<void> retirement_guard{};
        // Windows GUI launches can retain captured output without allocating
        // an unrelated console whose Close button would terminate the child.
        bool suppress_console{};
        // Explicit trusted runtime only. Private anonymous command/ack pipes;
        // stdio retains its existing ownership and never becomes a command port.
        std::optional<RuntimeControlLease> runtime_control{};
    };

    struct LaunchResult final
    {
        LaunchCode code{LaunchCode::invalid_request};
        ProcessHandle handle{};
        FocusCode focus{FocusCode::unsupported};
        std::string message{};

        // A fresh reservation transfers cleanup responsibility even when the
        // launch fails: it may still own a process tree or security lease and
        // must be settled before release. Focused/busy results only borrow an
        // existing owner's handle; they do not authorize stopping or releasing it.
        [[nodiscard]] constexpr bool owns_new_process() const noexcept
        {
            return handle.valid()
                && (code == LaunchCode::started
                    || code == LaunchCode::spawn_failed
                    || code == LaunchCode::invalid_request);
        }

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
        std::uint64_t platform_window_id{};
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
        std::optional<ExecutableIdentity> verified_executable{};
        bool environment_replaced{};
        bool standard_input_disconnected{};
        // Set only after inspecting the suspended child's actual token.
        bool restricted_token_verified{};
        // Includes removal of this launch's grants/profile after process exit.
        bool isolation_retired{};
        RuntimeControlSnapshot runtime_control{};

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

    [[nodiscard]] std::optional<ExecutableIdentity> inspect_executable(
        const std::filesystem::path& executable) noexcept;
    // Host-only preparation for a disposable workspace. Replaces ambient
    // credentials/configuration with OS paths and workspace-local temp/profile
    // directories. This is environment hygiene, not OS access confinement.
    [[nodiscard]] std::optional<std::vector<EnvironmentVariable>>
        prepare_workspace_environment(const std::filesystem::path& workspace) noexcept;
    struct RuntimeEnvironment final
    {
        std::filesystem::path data_root{};
        std::vector<EnvironmentVariable> variables{};
    };
    // Creates a fresh sibling of an admitted disposable code workspace. Test
    // and preview writes must not become part of source or validated output.
    // The host retains this directory as disposable evidence; this is path
    // separation, not a substitute for restricted-token filesystem grants.
    [[nodiscard]] std::optional<RuntimeEnvironment> prepare_runtime_environment(
        const std::filesystem::path& code_workspace, std::string_view phase) noexcept;
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
    // One fixed mesh command outstanding. Exact launch/lease identity is
    // rechecked under the supervisor lock; no arbitrary bytes or code API.
    [[nodiscard]] bool request_runtime_mesh_mode(ProcessHandle handle,
        const RuntimeControlLease& lease, const ExecutableIdentity& executable,
        std::string_view correlation, MeshDrawMode mode) noexcept;
    // Explicit transport changes only; retain just the latest waiting sample.
    // Mesh display requests have priority on this same private owned pipe.
    [[nodiscard]] bool request_runtime_presentation(ProcessHandle handle,
        const RuntimeControlLease& lease,const ExecutableIdentity& executable,
        std::string_view correlation,const RuntimePresentation& sample) noexcept;
    [[nodiscard]] bool initialize_inherited_runtime_control(int argc, char* argv[]) noexcept;
    [[nodiscard]] bool claim_inherited_runtime_control(std::string_view project,
        std::string_view scene) noexcept;
    [[nodiscard]] std::optional<RuntimeMeshCommand> poll_runtime_mesh_command() noexcept;
    void acknowledge_runtime_mesh_command(const RuntimeMeshCommand& command,
        bool published, MeshDrawMode actual_mode) noexcept;
    [[nodiscard]] std::optional<RuntimePresentationCommand> poll_runtime_presentation_command() noexcept;
    void acknowledge_runtime_presentation_command(const RuntimePresentationCommand&,bool accepted) noexcept;

}
