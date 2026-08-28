/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

module ai.mcp_child_host;

import core.sha256;

namespace epochengine::ai::mcp_child_host
{
    namespace
    {
        [[nodiscard]] bool identifier(
            const std::string_view value,
            const std::size_t maximum = 96u) noexcept
        {
            if (value.empty() || value.size() > maximum)
                return false;
            return std::all_of(value.begin(), value.end(), [](const unsigned char c)
            {
                return std::isalnum(c) != 0 || c == '.' || c == '_' || c == '-';
            });
        }

        [[nodiscard]] bool lowercase_hex(
            const std::string_view value,
            const std::size_t size) noexcept
        {
            return value.size() == size
                && std::all_of(value.begin(), value.end(), [](const char c)
                {
                    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
                });
        }

        [[nodiscard]] bool bounded_text(
            const std::string_view value,
            const std::size_t maximum,
            const bool empty_allowed = false) noexcept
        {
            if ((!empty_allowed && value.empty()) || value.size() > maximum)
                return false;
            return std::none_of(value.begin(), value.end(), [](const unsigned char c)
            {
                return c == 0u || c == '\r' || c == '\n'
                    || (c < 0x20u && c != '\t');
            });
        }

        [[nodiscard]] bool environment_name(const std::string_view value) noexcept
        {
            if (value.empty() || value.size() > 64u
                || !(std::isalpha(static_cast<unsigned char>(value.front()))
                    || value.front() == '_'))
                return false;
            return std::all_of(value.begin() + 1, value.end(), [](const unsigned char c)
            {
                return std::isalnum(c) != 0 || c == '_';
            });
        }

        [[nodiscard]] bool dangerous_environment(const std::string_view name) noexcept
        {
            constexpr std::array blocked{
                std::string_view{"COMSPEC"}, std::string_view{"SHELL"},
                std::string_view{"LD_PRELOAD"},
                std::string_view{"LD_LIBRARY_PATH"},
                std::string_view{"DYLD_INSERT_LIBRARIES"},
                std::string_view{"DYLD_LIBRARY_PATH"}};
            return std::find(blocked.begin(), blocked.end(), name) != blocked.end();
        }

        [[nodiscard]] bool server_argument(std::string_view value)
        {
            std::string normalized{value};
            std::transform(
                normalized.begin(), normalized.end(), normalized.begin(),
                [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });
            constexpr std::array blocked{
                std::string_view{"--listen"}, std::string_view{"--server"},
                std::string_view{"--socket"}, std::string_view{"--port"},
                std::string_view{"serve"}, std::string_view{"listen"}};
            return std::any_of(blocked.begin(), blocked.end(), [&](const auto token)
            {
                return normalized == token
                    || normalized.starts_with(std::string{token} + "=");
            });
        }

        [[nodiscard]] std::string hash_file(const std::filesystem::path& path)
        {
            std::ifstream input{path, std::ios::binary};
            if (!input)
                return {};
            core::sha256::Hasher hasher{};
            std::array<char, 64u * 1024u> buffer{};
            while (input)
            {
                input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
                const auto count = input.gcount();
                if (count > 0)
                {
                    hasher.update(std::string_view{
                        buffer.data(), static_cast<std::size_t>(count)});
                }
            }
            return input.eof() ? core::sha256::hex(hasher.finish()) : std::string{};
        }

        void field(
            std::string& bytes,
            const std::string_view name,
            const std::string_view value)
        {
            bytes += std::to_string(name.size()) + ":" + std::string{name};
            bytes += std::to_string(value.size()) + ":" + std::string{value};
        }

        [[nodiscard]] std::string configuration_digest(
            const ApprovedLaunchPlan& plan)
        {
            std::string bytes{"epoch.ai.mcp_child_host.configuration.v1"};
            field(bytes, "configuration", plan.configuration_id());
            field(bytes, "generation", std::to_string(plan.generation()));
            field(bytes, "executable", plan.executable().generic_string());
            field(bytes, "executable_sha256", plan.executable_sha256());
            field(bytes, "working_root", plan.working_root().generic_string());
            for (const auto& argument : plan.arguments())
                field(bytes, "argument", argument);
            for (const auto& environment : plan.environment())
            {
                field(bytes, "environment_name", environment.name);
                field(bytes, "environment_value", environment.value);
            }
            field(bytes, "replace_environment",
                plan.replaces_environment() ? "1" : "0");
            field(bytes, "stdio_only", plan.stdio_only() ? "1" : "0");
            return core::sha256::hex(core::sha256::hash(bytes));
        }

        [[nodiscard]] McpToolRegistry proposal_registry()
        {
            const McpToolRegistry source = mcp_campaign::make_tool_registry();
            McpToolRegistry result{source.limits()};
            for (const auto& descriptor : source.tools())
            {
                McpToolDescriptor proposal = descriptor;
                // Transport admits proposals only. CampaignAdapter applies the
                // original risk and emits an immutable pending host action.
                proposal.risk = McpToolRisk::read_only;
                (void)result.register_tool(std::move(proposal));
            }
            return result;
        }

        [[nodiscard]] bool terminal(const McpCallState state) noexcept
        {
            return state == McpCallState::succeeded
                || state == McpCallState::failed
                || state == McpCallState::cancelled
                || state == McpCallState::rejected;
        }

        [[nodiscard]] std::string retained_digest(const std::string_view bytes)
        {
            return core::sha256::hex(core::sha256::hash(bytes));
        }
    }

    const std::string& ApprovedLaunchPlan::configuration_id() const noexcept
    {
        return configuration_id_;
    }

    const std::string& ApprovedLaunchPlan::configuration_sha256() const noexcept
    {
        return configuration_sha256_;
    }

    std::uint64_t ApprovedLaunchPlan::generation() const noexcept
    {
        return generation_;
    }

    const std::filesystem::path& ApprovedLaunchPlan::executable() const noexcept
    {
        return executable_;
    }

    const std::string& ApprovedLaunchPlan::executable_sha256() const noexcept
    {
        return executable_sha256_;
    }

    const std::filesystem::path& ApprovedLaunchPlan::working_root() const noexcept
    {
        return working_root_;
    }

    const std::vector<std::string>& ApprovedLaunchPlan::arguments() const noexcept
    {
        return arguments_;
    }

    const std::vector<EnvironmentEntry>& ApprovedLaunchPlan::environment() const noexcept
    {
        return environment_;
    }

    bool ApprovedLaunchPlan::replaces_environment() const noexcept
    {
        return replace_environment_;
    }

    bool ApprovedLaunchPlan::stdio_only() const noexcept
    {
        return stdio_only_;
    }

    ConfigurationValidation validate_configuration(
        const HostConfiguration& configuration,
        const HostLimits limits)
    {
        const auto fail = [](const ConfigurationCode code, std::string status)
        {
            return ConfigurationValidation{.code = code, .status = std::move(status)};
        };
        if (!limits.valid())
            return fail(ConfigurationCode::invalid_limits,
                "MCP child-host limits are invalid.");
        if (!configuration.enabled)
            return fail(ConfigurationCode::disabled,
                "The local MCP child host is disabled.");
        if (!identifier(configuration.configuration_id)
            || configuration.generation == 0u
            || !identifier(configuration.transport_session_id)
            || !lowercase_hex(configuration.executable_sha256, 64u)
            || !configuration.replace_environment || !configuration.stdio_only
            || configuration.shell_enabled || configuration.network_enabled
            || configuration.listener_enabled || configuration.automatic_launch)
        {
            return fail(ConfigurationCode::invalid_configuration,
                "The MCP child host requires explicit stdio-only, no-shell, no-network, manual-launch authority.");
        }
        if (configuration.arguments.size() > limits.maximum_arguments
            || configuration.environment.size() > limits.maximum_environment_entries
            || configuration.executable_allowlist.empty()
            || configuration.executable_allowlist.size()
                > limits.maximum_approved_executables)
            return fail(ConfigurationCode::invalid_configuration,
                "Configured argv, environment, or executable approvals exceed bounds.");

        std::size_t argumentBytes{};
        for (const auto& argument : configuration.arguments)
        {
            if (!bounded_text(argument, 4096u, true)
                || server_argument(argument)
                || argument.size() > limits.maximum_argument_bytes
                || argumentBytes > limits.maximum_argument_bytes - argument.size())
                return fail(ConfigurationCode::invalid_configuration,
                    "Configured argv is invalid, server-capable, or over budget.");
            argumentBytes += argument.size();
        }
        std::size_t environmentBytes{};
        for (std::size_t i = 0u; i < configuration.environment.size(); ++i)
        {
            const auto& entry = configuration.environment[i];
            const std::size_t entryBytes = entry.name.size() + entry.value.size();
            if (!environment_name(entry.name) || dangerous_environment(entry.name)
                || !bounded_text(entry.value, 4096u, true)
                || entryBytes > limits.maximum_environment_bytes
                || environmentBytes > limits.maximum_environment_bytes - entryBytes)
                return fail(ConfigurationCode::invalid_configuration,
                    "Configured environment is invalid, injection-capable, or over budget.");
            environmentBytes += entryBytes;
            for (std::size_t j = i + 1u; j < configuration.environment.size(); ++j)
                if (entry.name == configuration.environment[j].name)
                    return fail(ConfigurationCode::invalid_configuration,
                        "Configured environment names must be unique.");
        }

        std::error_code error{};
        const auto executable = std::filesystem::weakly_canonical(
            std::filesystem::absolute(configuration.executable, error), error);
        if (error || executable.empty()
            || !std::filesystem::is_regular_file(executable, error) || error)
            return fail(ConfigurationCode::path_denied,
                "The configured MCP executable is not a canonical regular file.");
        error.clear();
        const auto workingRoot = std::filesystem::weakly_canonical(
            std::filesystem::absolute(configuration.working_root, error), error);
        if (error || workingRoot.empty()
            || !std::filesystem::is_directory(workingRoot, error) || error)
            return fail(ConfigurationCode::path_denied,
                "The configured MCP working root is not a canonical directory.");

        bool allowed{};
        for (const auto& approval : configuration.executable_allowlist)
        {
            if (!lowercase_hex(approval.sha256, 64u))
                return fail(ConfigurationCode::invalid_configuration,
                    "An executable approval digest is invalid.");
            error.clear();
            const auto approvedPath = std::filesystem::weakly_canonical(
                std::filesystem::absolute(approval.canonical_path, error), error);
            if (error || approvedPath.empty())
                return fail(ConfigurationCode::path_denied,
                    "An executable approval path is not canonical.");
            allowed = allowed || (approvedPath == executable
                && approval.sha256 == configuration.executable_sha256);
        }
        if (!allowed)
            return fail(ConfigurationCode::executable_denied,
                "The canonical MCP executable and digest are not allowlisted together.");
        if (hash_file(executable) != configuration.executable_sha256)
            return fail(ConfigurationCode::digest_mismatch,
                "The MCP executable bytes do not match their approved SHA-256.");

        ApprovedLaunchPlan plan{};
        plan.configuration_id_ = configuration.configuration_id;
        plan.generation_ = configuration.generation;
        plan.executable_ = executable;
        plan.executable_sha256_ = configuration.executable_sha256;
        plan.working_root_ = workingRoot;
        plan.arguments_ = configuration.arguments;
        plan.environment_ = configuration.environment;
        plan.replace_environment_ = configuration.replace_environment;
        plan.stdio_only_ = configuration.stdio_only;
        plan.configuration_sha256_ = configuration_digest(plan);
        return {
            .code = ConfigurationCode::ready,
            .status = "The explicit MCP child configuration is canonical and digest-bound.",
            .plan = std::move(plan)};
    }

    struct LocalChildHost::Implementation final
    {
        struct Pending final
        {
            std::string action_id{};
            mcp_stdio::RequestId request_id{};
            std::string call_id{};
            mcp_campaign::PendingHostAction action{};
        };

        HostConfiguration configuration{};
        McpSessionAuthority authority{};
        HostLimits limits{};
        mcp_campaign::CampaignAdapter adapter;
        std::unique_ptr<mcp_stdio::ProtocolSession> protocol{};
        ChildProcessPort* process{};
        std::optional<ApprovedLaunchPlan> plan{};
        HostState state{HostState::idle};
        std::uint64_t started_at{};
        std::uint64_t last_activity{};
        std::uint64_t stop_deadline{};
        std::uint64_t total_input{};
        std::uint64_t total_output{};
        std::uint64_t frames{};
        std::string stdout_buffer{};
        std::string stderr_retained{};
        std::uint64_t stderr_observed{};
        bool stderr_truncated{};
        bool force_requested{};
        std::string status{
            "The local MCP child host is idle and will not launch automatically."};
        std::vector<Pending> pending{};

        Implementation(
            HostConfiguration value,
            McpSessionAuthority sessionAuthority,
            HostLimits hostLimits)
            : configuration{std::move(value)},
              authority{std::move(sessionAuthority)},
              limits{hostLimits},
              adapter{configuration.transport_session_id, limits.adapter}
        {
            if (!configuration.enabled)
            {
                state = HostState::disabled;
                status = "The local MCP child host is disabled by policy.";
            }
        }

        [[nodiscard]] HostSnapshot make_snapshot() const
        {
            return {
                .state = state,
                .configuration_id = configuration.configuration_id,
                .configuration_sha256 = plan
                    ? plan->configuration_sha256() : std::string{},
                .transport_session_id = configuration.transport_session_id,
                .started_at_unix_seconds = started_at,
                .last_activity_unix_seconds = last_activity,
                .total_input_bytes = total_input,
                .total_output_bytes = total_output,
                .processed_frames = frames,
                .pending_host_actions = pending.size(),
                .stderr_evidence = {
                    .retained_text = stderr_retained,
                    .retained_sha256 = retained_digest(stderr_retained),
                    .observed_bytes = stderr_observed,
                    .truncated = stderr_truncated},
                .status = status};
        }

        [[nodiscard]] HostResult result(
            const HostCode code,
            std::string message,
            std::vector<mcp_campaign::PendingHostAction> actions = {})
        {
            status = message;
            return {
                .code = code,
                .status = std::move(message),
                .pending_actions = std::move(actions),
                .snapshot = make_snapshot()};
        }

        [[nodiscard]] bool write(std::string_view bytes, std::string& error)
        {
            if (bytes.empty())
                return true;
            if (process == nullptr
                || total_output > limits.maximum_total_output_bytes
                || bytes.size() > limits.maximum_total_output_bytes - total_output)
            {
                error = "MCP child output exceeded the session byte budget.";
                return false;
            }
            if (!process->write_stdin(bytes, error))
                return false;
            total_output += bytes.size();
            return true;
        }

        void retain_stderr(const std::string_view bytes)
        {
            stderr_observed += bytes.size();
            const std::size_t capacity = limits.maximum_stderr_evidence_bytes;
            const std::size_t available = capacity > stderr_retained.size()
                ? capacity - stderr_retained.size() : 0u;
            stderr_retained.append(bytes.substr(0u, available));
            stderr_truncated = stderr_truncated || bytes.size() > available;
        }

        [[nodiscard]] HostResult begin_stop(
            const HostCode code,
            std::string message,
            const std::uint64_t now)
        {
            if (protocol)
                protocol->cancel_session();
            state = HostState::stop_requested;
            stop_deadline = now + limits.graceful_stop_seconds;
            std::string stopStatus{};
            if (process == nullptr || !process->request_graceful_stop(stopStatus))
            {
                if (process != nullptr)
                    force_requested = process->force_stop(stopStatus);
                if (!force_requested)
                    state = HostState::failed;
            }
            return result(code, std::move(message));
        }
    };

    LocalChildHost::LocalChildHost(
        HostConfiguration configuration,
        McpSessionAuthority authority,
        HostLimits limits)
        : implementation_{std::make_unique<Implementation>(
            std::move(configuration), std::move(authority), limits)}
    {
    }

    LocalChildHost::~LocalChildHost() = default;
    LocalChildHost::LocalChildHost(LocalChildHost&&) noexcept = default;
    LocalChildHost& LocalChildHost::operator=(LocalChildHost&&) noexcept = default;

    HostResult LocalChildHost::start(
        ChildProcessPort& process,
        const LaunchApproval& approval,
        const std::uint64_t now)
    {
        auto& value = *implementation_;
        if (value.state == HostState::disabled)
            return value.result(HostCode::disabled,
                "The local MCP child host is disabled.");
        if (value.state != HostState::idle && value.state != HostState::stopped)
            return value.result(HostCode::invalid_state,
                "The MCP child host is already active.");
        if (has_capability(
                value.authority.granted_capabilities, McpToolCapability::network)
            || has_capability(
                value.authority.granted_capabilities, McpToolCapability::execute)
            || value.authority.session_id
                != value.configuration.transport_session_id
            || !value.authority.approved_call_ids.empty())
        {
            return value.result(HostCode::invalid_configuration,
                "Child-host authority must be exact, proposal-only, and contain no network, execute, or preapproved call authority.");
        }
        const ConfigurationValidation validated = validate_configuration(
            value.configuration, value.limits);
        if (!validated)
            return value.result(HostCode::invalid_configuration, validated.status);
        if (!approval.operator_approved
            || approval.configuration_id != validated.plan->configuration_id()
            || approval.generation != validated.plan->generation()
            || approval.configuration_sha256
                != validated.plan->configuration_sha256())
        {
            return value.result(HostCode::approval_required,
                "An exact operator approval for this configuration digest is required for each launch.");
        }
        std::string launchStatus{};
        if (!process.launch(*validated.plan, launchStatus))
        {
            value.state = HostState::failed;
            return value.result(HostCode::io_failed,
                launchStatus.empty()
                    ? "The platform process owner refused launch."
                    : launchStatus);
        }
        value.process = &process;
        value.plan = validated.plan;
        value.protocol = std::make_unique<mcp_stdio::ProtocolSession>(
            mcp_stdio::SessionId{value.configuration.transport_session_id},
            proposal_registry(), value.authority, value.limits.protocol);
        value.adapter = mcp_campaign::CampaignAdapter{
            value.configuration.transport_session_id, value.limits.adapter};
        value.pending.clear();
        value.stdout_buffer.clear();
        value.stderr_retained.clear();
        value.stderr_observed = 0u;
        value.stderr_truncated = false;
        value.total_input = 0u;
        value.total_output = 0u;
        value.frames = 0u;
        value.force_requested = false;
        value.started_at = now;
        value.last_activity = now;
        value.stop_deadline = 0u;
        value.state = HostState::running;
        return value.result(HostCode::ready,
            "The operator-approved local stdio MCP child is running under bounded proposal-only authority.");
    }

    HostResult LocalChildHost::pump(
        const mcp_campaign::HostSnapshot& campaignHost,
        const std::uint64_t now)
    {
        auto& value = *implementation_;
        if (value.state == HostState::stop_requested)
            return poll_stop(now);
        if (value.state != HostState::running || value.process == nullptr
            || !value.protocol)
            return value.result(HostCode::invalid_state,
                "The MCP child host is not running.");
        if (campaignHost.cancellation_requested)
            return value.begin_stop(HostCode::cancelled,
                "The resolved campaign host requested cancellation.", now);
        if (now < value.started_at || now < value.last_activity
            || now - value.started_at >= value.limits.maximum_session_seconds
            || now - value.last_activity >= value.limits.maximum_idle_seconds)
            return value.begin_stop(HostCode::timed_out,
                "The MCP child session reached a bounded lifetime or idle timeout.",
                now);
        const ChildState childState = value.process->state();
        if (childState == ChildState::exited)
        {
            value.state = HostState::stopped;
            return value.result(HostCode::child_exited,
                "The local MCP child exited.");
        }
        if (childState == ChildState::failed)
        {
            value.state = HostState::failed;
            return value.result(HostCode::io_failed,
                "The platform MCP child failed.");
        }

        const ChannelRead stderrRead = value.process->read_stderr(
            value.limits.maximum_read_chunk_bytes);
        if (stderrRead.code == ChannelCode::failed)
            return value.begin_stop(HostCode::io_failed,
                "Reading bounded MCP stderr evidence failed.", now);
        if (!stderrRead.bytes.empty())
        {
            value.retain_stderr(stderrRead.bytes);
            value.last_activity = now;
        }

        const ChannelRead stdoutRead = value.process->read_stdout(
            value.limits.maximum_read_chunk_bytes);
        if (stdoutRead.code == ChannelCode::failed)
            return value.begin_stop(HostCode::io_failed,
                "Reading MCP stdout failed.", now);
        if (stdoutRead.code == ChannelCode::end_of_stream)
        {
            value.state = HostState::stopped;
            return value.result(HostCode::child_exited,
                "The local MCP stdout stream closed.");
        }
        if (!stdoutRead.bytes.empty())
        {
            if (value.total_input > value.limits.maximum_total_input_bytes
                || stdoutRead.bytes.size()
                    > value.limits.maximum_total_input_bytes - value.total_input
                || value.stdout_buffer.size() + stdoutRead.bytes.size()
                    > value.limits.maximum_buffered_stdout_bytes)
                return value.begin_stop(HostCode::budget_exhausted,
                    "MCP child stdout exceeded a bounded byte budget.", now);
            value.stdout_buffer += stdoutRead.bytes;
            value.total_input += stdoutRead.bytes.size();
            value.last_activity = now;
        }

        std::vector<mcp_campaign::PendingHostAction> actions{};
        for (std::size_t processed = 0u;
             processed < value.limits.maximum_frames_per_pump
                && !value.stdout_buffer.empty();
             ++processed)
        {
            mcp_stdio::DispatchResult dispatched =
                value.protocol->dispatch_frame(value.stdout_buffer);
            if (dispatched.code == mcp_stdio::ProtocolCode::need_more)
                break;
            if (dispatched.consumed_bytes == 0u
                || dispatched.consumed_bytes > value.stdout_buffer.size())
                return value.begin_stop(HostCode::io_failed,
                    "The MCP protocol could not make bounded progress on stdout.",
                    now);
            value.stdout_buffer.erase(0u, dispatched.consumed_bytes);
            ++value.frames;
            value.last_activity = now;

            if (dispatched.pending_call)
            {
                McpSessionAuthority proposalAuthority = value.authority;
                proposalAuthority.approved_call_ids = {
                    dispatched.pending_call->call_id};
                mcp_campaign::AdapterResult adapted = value.adapter.dispatch(
                    dispatched.request_id, *dispatched.pending_call,
                    proposalAuthority, campaignHost);
                if (adapted.pending_action)
                {
                    if (value.pending.size()
                        >= value.limits.maximum_pending_host_actions)
                        return value.begin_stop(HostCode::budget_exhausted,
                            "The pending host-action budget is exhausted.", now);
                    value.pending.push_back({
                        .action_id = adapted.pending_action->action_id(),
                        .request_id = dispatched.request_id,
                        .call_id = dispatched.pending_call->call_id,
                        .action = *adapted.pending_action});
                    actions.push_back(*adapted.pending_action);
                    continue;
                }
                mcp_stdio::DispatchResult completed =
                    value.protocol->complete_call(
                        dispatched.request_id, adapted.result);
                std::string writeError{};
                if (!value.write(completed.response_frame, writeError))
                    return value.begin_stop(HostCode::io_failed,
                        "Writing the bounded MCP result failed.", now);
                continue;
            }

            if (dispatched.code == mcp_stdio::ProtocolCode::cancelled)
            {
                const std::string key = dispatched.request_id.key();
                value.pending.erase(std::remove_if(
                    value.pending.begin(), value.pending.end(),
                    [&](const auto& pending)
                    {
                        return pending.request_id.key() == key;
                    }), value.pending.end());
            }
            if (dispatched.has_response())
            {
                std::string writeError{};
                if (!value.write(dispatched.response_frame, writeError))
                    return value.begin_stop(HostCode::io_failed,
                        "Writing the MCP protocol response failed.", now);
            }
        }
        const bool hasActions = !actions.empty();
        return value.result(
            hasActions ? HostCode::pending_host_action : HostCode::ready,
            !hasActions
                ? "The MCP child host processed bounded stdio input."
                : "A bounded campaign proposal awaits explicit host/operator completion.",
            std::move(actions));
    }

    HostResult LocalChildHost::complete_host_action(
        const std::string_view actionId,
        const McpToolResult& terminalResult,
        const std::uint64_t now)
    {
        auto& value = *implementation_;
        if (value.state != HostState::running || !value.protocol
            || value.process == nullptr || !identifier(actionId, 128u))
            return value.result(HostCode::invalid_state,
                "A running MCP child and exact pending action are required.");
        const auto found = std::find_if(
            value.pending.begin(), value.pending.end(),
            [actionId](const auto& pending)
            {
                return pending.action_id == actionId;
            });
        if (found == value.pending.end())
            return value.result(HostCode::replay_denied,
                "No live pending action matches this completion identity.");
        if (!terminal(terminalResult.state)
            || terminalResult.session_id
                != value.configuration.transport_session_id
            || terminalResult.call_id != found->call_id)
            return value.result(HostCode::replay_denied,
                "The completion is nonterminal or not bound to the pending session and call.");
        mcp_stdio::DispatchResult completed = value.protocol->complete_call(
            found->request_id, terminalResult);
        if (completed.code != mcp_stdio::ProtocolCode::ready)
            return value.result(HostCode::replay_denied,
                "The protocol refused a replayed or mismatched host completion.");
        std::string writeError{};
        if (!value.write(completed.response_frame, writeError))
            return value.begin_stop(HostCode::io_failed,
                "Writing the approved host completion failed.", now);
        value.pending.erase(found);
        value.last_activity = now;
        return value.result(HostCode::ready,
            "The exact terminal host result was returned once to the MCP child.");
    }

    HostResult LocalChildHost::cancel(
        const std::string_view reason,
        const std::uint64_t now)
    {
        auto& value = *implementation_;
        if (value.state != HostState::running)
            return value.result(HostCode::invalid_state,
                "Only a running MCP child session can be cancelled.");
        const std::string message = bounded_text(reason, 1024u)
            ? "The MCP child session was cancelled: " + std::string{reason}
            : "The MCP child session was cancelled by the host.";
        return value.begin_stop(HostCode::cancelled, message, now);
    }

    HostResult LocalChildHost::poll_stop(const std::uint64_t now)
    {
        auto& value = *implementation_;
        if (value.state != HostState::stop_requested || value.process == nullptr)
            return value.result(HostCode::invalid_state,
                "The MCP child host has no graceful stop in progress.");
        const ChildState observed = value.process->state();
        if (observed == ChildState::exited)
        {
            value.state = HostState::stopped;
            return value.result(HostCode::ready,
                "The MCP child stopped during the graceful-stop window.");
        }
        if (observed == ChildState::failed)
        {
            value.state = HostState::failed;
            return value.result(HostCode::io_failed,
                "The MCP child failed while stopping.");
        }
        if (now >= value.stop_deadline && !value.force_requested)
        {
            std::string status{};
            value.force_requested = value.process->force_stop(status);
            if (!value.force_requested)
            {
                value.state = HostState::failed;
                return value.result(HostCode::io_failed,
                    "The platform owner could not force-stop the MCP child after grace expired.");
            }
            return value.result(HostCode::cancelled,
                "Grace expired; the platform owner force-stopped the MCP child.");
        }
        return value.result(HostCode::cancelled,
            "The MCP child is within its bounded graceful-stop window.");
    }

    HostSnapshot LocalChildHost::snapshot() const
    {
        return implementation_->make_snapshot();
    }
}
