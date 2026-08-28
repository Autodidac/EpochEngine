/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

export module ai.mcp_child_host;

export import ai.mcp_campaign;

export namespace epochengine::ai::mcp_child_host
{
    struct HostLimits final
    {
        std::size_t maximum_arguments{32u};
        std::size_t maximum_argument_bytes{16u * 1024u};
        std::size_t maximum_environment_entries{16u};
        std::size_t maximum_environment_bytes{16u * 1024u};
        std::size_t maximum_approved_executables{8u};
        std::size_t maximum_read_chunk_bytes{64u * 1024u};
        std::size_t maximum_buffered_stdout_bytes{512u * 1024u};
        std::size_t maximum_stderr_evidence_bytes{64u * 1024u};
        std::size_t maximum_total_input_bytes{8u * 1024u * 1024u};
        std::size_t maximum_total_output_bytes{8u * 1024u * 1024u};
        std::size_t maximum_frames_per_pump{16u};
        std::size_t maximum_pending_host_actions{32u};
        std::uint64_t maximum_session_seconds{30u * 60u};
        std::uint64_t maximum_idle_seconds{2u * 60u};
        std::uint64_t graceful_stop_seconds{2u};
        mcp_stdio::ProtocolLimits protocol{};
        mcp_campaign::AdapterLimits adapter{};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return maximum_arguments > 0u && maximum_arguments <= 64u
                && maximum_argument_bytes >= 256u
                && maximum_argument_bytes <= 64u * 1024u
                && maximum_environment_entries <= 32u
                && maximum_environment_bytes <= 64u * 1024u
                && maximum_approved_executables > 0u
                && maximum_approved_executables <= 16u
                && maximum_read_chunk_bytes >= 1024u
                && maximum_read_chunk_bytes <= maximum_buffered_stdout_bytes
                && maximum_buffered_stdout_bytes <= 2u * 1024u * 1024u
                && maximum_stderr_evidence_bytes <= 256u * 1024u
                && maximum_total_input_bytes >= maximum_buffered_stdout_bytes
                && maximum_total_output_bytes >= protocol.maximum_line_bytes
                && maximum_frames_per_pump > 0u
                && maximum_frames_per_pump <= 64u
                && maximum_pending_host_actions > 0u
                && maximum_pending_host_actions <= adapter.maximum_pending_actions
                && maximum_session_seconds >= 10u
                && maximum_session_seconds <= 24u * 60u * 60u
                && maximum_idle_seconds > 0u
                && maximum_idle_seconds <= maximum_session_seconds
                && graceful_stop_seconds > 0u
                && graceful_stop_seconds <= 30u
                && protocol.valid() && adapter.valid();
        }
    };

    struct EnvironmentEntry final
    {
        std::string name{};
        std::string value{};

        [[nodiscard]] friend bool operator==(
            const EnvironmentEntry&,
            const EnvironmentEntry&) noexcept = default;
    };

    struct ApprovedExecutable final
    {
        std::filesystem::path canonical_path{};
        std::string sha256{};
    };

    struct HostConfiguration final
    {
        bool enabled{};
        std::string configuration_id{};
        std::uint64_t generation{};
        std::string transport_session_id{};
        std::filesystem::path executable{};
        std::string executable_sha256{};
        std::filesystem::path working_root{};
        std::vector<std::string> arguments{};
        std::vector<EnvironmentEntry> environment{};
        std::vector<ApprovedExecutable> executable_allowlist{};
        bool replace_environment{true};
        bool stdio_only{true};
        bool shell_enabled{};
        bool network_enabled{};
        bool listener_enabled{};
        bool automatic_launch{};
    };

    struct LaunchApproval final
    {
        std::string configuration_id{};
        std::string configuration_sha256{};
        std::uint64_t generation{};
        bool operator_approved{};
    };

    struct ConfigurationValidation;

    class ApprovedLaunchPlan final
    {
    public:
        [[nodiscard]] const std::string& configuration_id() const noexcept;
        [[nodiscard]] const std::string& configuration_sha256() const noexcept;
        [[nodiscard]] std::uint64_t generation() const noexcept;
        [[nodiscard]] const std::filesystem::path& executable() const noexcept;
        [[nodiscard]] const std::string& executable_sha256() const noexcept;
        [[nodiscard]] const std::filesystem::path& working_root() const noexcept;
        [[nodiscard]] const std::vector<std::string>& arguments() const noexcept;
        [[nodiscard]] const std::vector<EnvironmentEntry>& environment() const noexcept;
        [[nodiscard]] bool replaces_environment() const noexcept;
        [[nodiscard]] bool stdio_only() const noexcept;

    private:
        friend ConfigurationValidation validate_configuration(
            const HostConfiguration&, HostLimits);
        friend class LocalChildHost;
        std::string configuration_id_{};
        std::string configuration_sha256_{};
        std::uint64_t generation_{};
        std::filesystem::path executable_{};
        std::string executable_sha256_{};
        std::filesystem::path working_root_{};
        std::vector<std::string> arguments_{};
        std::vector<EnvironmentEntry> environment_{};
        bool replace_environment_{};
        bool stdio_only_{};
    };

    enum class ConfigurationCode : std::uint8_t
    {
        ready,
        disabled,
        invalid_limits,
        invalid_configuration,
        path_denied,
        executable_denied,
        digest_mismatch
    };

    struct ConfigurationValidation final
    {
        ConfigurationCode code{ConfigurationCode::invalid_configuration};
        std::string status{};
        std::optional<ApprovedLaunchPlan> plan{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == ConfigurationCode::ready && plan.has_value();
        }
    };

    [[nodiscard]] ConfigurationValidation validate_configuration(
        const HostConfiguration& configuration,
        HostLimits limits = {});

    enum class ChannelCode : std::uint8_t
    {
        ready,
        would_block,
        end_of_stream,
        failed
    };

    struct ChannelRead final
    {
        ChannelCode code{ChannelCode::would_block};
        std::string bytes{};
        std::string status{};
    };

    enum class ChildState : std::uint8_t
    {
        idle,
        running,
        exited,
        failed
    };

    // Implemented by the platform owner. It must launch the approved executable
    // directly with redirected standard streams; it must never invoke a shell.
    class ChildProcessPort
    {
    public:
        virtual ~ChildProcessPort() = default;
        [[nodiscard]] virtual bool launch(
            const ApprovedLaunchPlan& plan,
            std::string& status) = 0;
        [[nodiscard]] virtual ChannelRead read_stdout(
            std::size_t maximum_bytes) = 0;
        [[nodiscard]] virtual ChannelRead read_stderr(
            std::size_t maximum_bytes) = 0;
        [[nodiscard]] virtual bool write_stdin(
            std::string_view bytes,
            std::string& status) = 0;
        [[nodiscard]] virtual bool request_graceful_stop(
            std::string& status) = 0;
        [[nodiscard]] virtual bool force_stop(std::string& status) = 0;
        [[nodiscard]] virtual ChildState state() const noexcept = 0;
    };

    enum class HostState : std::uint8_t
    {
        disabled,
        idle,
        running,
        stop_requested,
        stopped,
        failed
    };

    enum class HostCode : std::uint8_t
    {
        ready,
        pending_host_action,
        disabled,
        approval_required,
        invalid_configuration,
        invalid_state,
        budget_exhausted,
        timed_out,
        cancelled,
        io_failed,
        child_exited,
        replay_denied
    };

    struct StderrEvidence final
    {
        std::string retained_text{};
        std::string retained_sha256{};
        std::uint64_t observed_bytes{};
        bool truncated{};
    };

    struct HostSnapshot final
    {
        HostState state{HostState::idle};
        std::string configuration_id{};
        std::string configuration_sha256{};
        std::string transport_session_id{};
        std::uint64_t started_at_unix_seconds{};
        std::uint64_t last_activity_unix_seconds{};
        std::uint64_t total_input_bytes{};
        std::uint64_t total_output_bytes{};
        std::uint64_t processed_frames{};
        std::size_t pending_host_actions{};
        StderrEvidence stderr_evidence{};
        std::string status{};
    };

    struct HostResult final
    {
        HostCode code{HostCode::invalid_state};
        std::string status{};
        std::vector<mcp_campaign::PendingHostAction> pending_actions{};
        HostSnapshot snapshot{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == HostCode::ready
                || code == HostCode::pending_host_action;
        }
    };

    class LocalChildHost final
    {
    public:
        LocalChildHost(
            HostConfiguration configuration,
            McpSessionAuthority authority,
            HostLimits limits = {});
        ~LocalChildHost();
        LocalChildHost(const LocalChildHost&) = delete;
        LocalChildHost& operator=(const LocalChildHost&) = delete;
        LocalChildHost(LocalChildHost&&) noexcept;
        LocalChildHost& operator=(LocalChildHost&&) noexcept;

        [[nodiscard]] HostResult start(
            ChildProcessPort& process,
            const LaunchApproval& approval,
            std::uint64_t now_unix_seconds);
        [[nodiscard]] HostResult pump(
            const mcp_campaign::HostSnapshot& campaign_host,
            std::uint64_t now_unix_seconds);
        [[nodiscard]] HostResult complete_host_action(
            std::string_view action_id,
            const McpToolResult& terminal_result,
            std::uint64_t now_unix_seconds);
        [[nodiscard]] HostResult cancel(
            std::string_view reason,
            std::uint64_t now_unix_seconds);
        [[nodiscard]] HostResult poll_stop(std::uint64_t now_unix_seconds);
        [[nodiscard]] HostSnapshot snapshot() const;

    private:
        struct Implementation;
        std::unique_ptr<Implementation> implementation_{};
    };

    [[nodiscard]] bool run_contract();
}
