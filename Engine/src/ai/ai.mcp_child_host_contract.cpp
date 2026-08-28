/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
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
        namespace fs = std::filesystem;

        class FixtureRoot final
        {
        public:
            FixtureRoot()
            {
                const auto sequence = std::chrono::steady_clock::now()
                    .time_since_epoch().count();
                root = fs::temp_directory_path()
                    / ("epoch_mcp_child_host_" + std::to_string(sequence));
                fs::create_directories(root / "cache");
                executable = root / "approved-mcp-fixture.bin";
                std::ofstream output{executable, std::ios::binary};
                output << "not an executable; the fake port never launches it\n";
                output.close();
                std::ifstream input{executable, std::ios::binary};
                const std::string bytes{
                    std::istreambuf_iterator<char>{input},
                    std::istreambuf_iterator<char>{}};
                sha256 = core::sha256::hex(core::sha256::hash(bytes));
                std::error_code error{};
                root = fs::weakly_canonical(root, error);
                executable = fs::weakly_canonical(executable, error);
            }

            ~FixtureRoot()
            {
                std::error_code error{};
                fs::remove_all(root, error);
            }

            fs::path root{};
            fs::path executable{};
            std::string sha256{};
        };

        class FakeProcess final : public ChildProcessPort
        {
        public:
            bool launch(const ApprovedLaunchPlan& plan, std::string& status) override
            {
                ++launches;
                launched_path = plan.executable();
                direct_stdio = plan.stdio_only();
                child_state = ChildState::running;
                status = "Fake child accepted without creating a process.";
                return allow_launch;
            }

            ChannelRead read_stdout(std::size_t maximum) override
            {
                if (stdout_chunks.empty())
                    return {};
                std::string bytes = std::move(stdout_chunks.front());
                stdout_chunks.erase(stdout_chunks.begin());
                if (bytes.size() > maximum)
                {
                    stdout_chunks.insert(
                        stdout_chunks.begin(), bytes.substr(maximum));
                    bytes.resize(maximum);
                }
                return {.code = ChannelCode::ready, .bytes = std::move(bytes)};
            }

            ChannelRead read_stderr(std::size_t maximum) override
            {
                if (stderr_chunks.empty())
                    return {};
                std::string bytes = std::move(stderr_chunks.front());
                stderr_chunks.erase(stderr_chunks.begin());
                if (bytes.size() > maximum)
                    bytes.resize(maximum);
                return {.code = ChannelCode::ready, .bytes = std::move(bytes)};
            }

            bool write_stdin(std::string_view bytes, std::string&) override
            {
                writes.append(bytes);
                return allow_write;
            }

            bool request_graceful_stop(std::string&) override
            {
                ++graceful_stops;
                return allow_graceful;
            }

            bool force_stop(std::string&) override
            {
                ++forced_stops;
                child_state = ChildState::exited;
                return true;
            }

            ChildState state() const noexcept override
            {
                return child_state;
            }

            bool allow_launch{true};
            bool allow_write{true};
            bool allow_graceful{true};
            ChildState child_state{ChildState::idle};
            std::size_t launches{};
            std::size_t graceful_stops{};
            std::size_t forced_stops{};
            bool direct_stdio{};
            fs::path launched_path{};
            std::vector<std::string> stdout_chunks{};
            std::vector<std::string> stderr_chunks{};
            std::string writes{};
        };

        [[nodiscard]] HostConfiguration configuration(const FixtureRoot& fixture)
        {
            return {
                .enabled = true,
                .configuration_id = "contract-local-mcp",
                .generation = 7u,
                .transport_session_id = "contract-mcp-session",
                .executable = fixture.executable,
                .executable_sha256 = fixture.sha256,
                .working_root = fixture.root,
                .arguments = {"--stdio"},
                .environment = {{"NO_COLOR", "1"}},
                .executable_allowlist = {{fixture.executable, fixture.sha256}},
                .replace_environment = true,
                .stdio_only = true};
        }

        [[nodiscard]] mcp_campaign::HostSnapshot campaign_host(
            const FixtureRoot& fixture)
        {
            using namespace iteration_session;
            return {
                .snapshot_id = "contract-host-snapshot",
                .generation = 3u,
                .now_unix_seconds = 1'800'000'000u,
                .targets = {{
                    .handle_id = "project-target",
                    .authority = {
                        .target_kind = IterationTargetKind::project_source,
                        .kind = SourceAuthorityKind::verified_project,
                        .root = fixture.root,
                        .project_id = "contract-project",
                        .project_manifest_digest = std::string(64u, 'a'),
                        .project_profile_digest = std::string(64u, 'b'),
                        .verified = true},
                    .curated_files = {{
                        .relative_path = "Source/main.cpp",
                        .sha256 = std::string(64u, 'c'),
                        .byte_count = 32u}},
                    .cache_root = (fixture.root / "cache").lexically_normal(),
                    .host_generation = 3u,
                    .available = true}}};
        }

        [[nodiscard]] std::string initialize_packet()
        {
            return mcp_stdio::encode_frame(
                "{\"jsonrpc\":\"2.0\",\"id\":\"init\","
                "\"method\":\"initialize\",\"params\":{"
                "\"protocolVersion\":\"2025-11-25\","
                "\"capabilities\":{},\"clientInfo\":{"
                "\"name\":\"Fake process\",\"version\":\"1\"}}}");
        }

        [[nodiscard]] std::string initialized_packet()
        {
            return mcp_stdio::encode_frame(
                "{\"jsonrpc\":\"2.0\","
                "\"method\":\"notifications/initialized\"}");
        }

        [[nodiscard]] std::string start_packet()
        {
            return mcp_stdio::encode_frame(
                "{\"jsonrpc\":\"2.0\",\"id\":\"start-one\","
                "\"method\":\"tools/call\",\"params\":{"
                "\"name\":\"campaign.start\",\"arguments\":{"
                "\"target_handle\":\"project-target\","
                "\"objective\":\"Repair one bounded project defect\","
                "\"model\":\"operator-selected-local-model\"}}}");
        }
    }

    bool run_contract()
    {
        FixtureRoot fixture{};
        const HostConfiguration config = configuration(fixture);
        const ConfigurationValidation validated = validate_configuration(config);
        if (!validated || !validated.plan->stdio_only()
            || validated.plan->configuration_sha256().size() != 64u)
            return false;

        HostConfiguration unsafe = config;
        unsafe.arguments = {"--listen=127.0.0.1:9000"};
        HostConfiguration shell = config;
        shell.shell_enabled = true;
        HostConfiguration disabled{};
        if (validate_configuration(unsafe).code
                != ConfigurationCode::invalid_configuration
            || validate_configuration(shell).code
                != ConfigurationCode::invalid_configuration
            || validate_configuration(disabled).code != ConfigurationCode::disabled)
            return false;

        McpSessionAuthority authority{
            .session_id = config.transport_session_id,
            .granted_capabilities = McpToolCapability::inspect
                | McpToolCapability::author
                | McpToolCapability::build
                | McpToolCapability::capture};
        LocalChildHost host{config, authority};
        FakeProcess process{};
        const LaunchApproval wrong{
            .configuration_id = config.configuration_id,
            .configuration_sha256 = std::string(64u, '0'),
            .generation = config.generation,
            .operator_approved = true};
        if (host.start(process, wrong, 100u).code != HostCode::approval_required
            || process.launches != 0u)
            return false;
        const LaunchApproval approved{
            .configuration_id = config.configuration_id,
            .configuration_sha256 = validated.plan->configuration_sha256(),
            .generation = config.generation,
            .operator_approved = true};
        if (!host.start(process, approved, 100u)
            || process.launches != 1u || !process.direct_stdio
            || process.launched_path != fixture.executable)
            return false;

        process.stderr_chunks.push_back("bounded diagnostic\n");
        process.stdout_chunks.push_back(
            initialize_packet() + initialized_packet() + start_packet());
        const HostResult proposed = host.pump(campaign_host(fixture), 101u);
        if (proposed.code != HostCode::pending_host_action
            || proposed.pending_actions.size() != 1u
            || proposed.snapshot.pending_host_actions != 1u
            || proposed.snapshot.stderr_evidence.observed_bytes == 0u
            || process.writes.find("protocolVersion") == std::string::npos
            || process.writes.find("pending_host_action") != std::string::npos)
            return false;

        const auto& action = proposed.pending_actions.front();
        McpToolResult terminalResult{
            .session_id = config.transport_session_id,
            .call_id = action.call_id(),
            .state = McpCallState::succeeded,
            .output = "Host stored the bounded campaign after explicit approval."};
        if (!host.complete_host_action(
                action.action_id(), terminalResult, 102u)
            || host.complete_host_action(
                action.action_id(), terminalResult, 103u).code
                != HostCode::replay_denied
            || process.writes.find("Host stored") == std::string::npos)
            return false;

        const HostResult cancelled = host.cancel("contract complete", 104u);
        if (cancelled.code != HostCode::cancelled
            || process.graceful_stops != 1u || process.forced_stops != 0u)
            return false;
        if (host.poll_stop(107u).code != HostCode::cancelled
            || process.forced_stops != 1u)
            return false;
        return host.snapshot().state == HostState::stop_requested
            && process.state() == ChildState::exited;
    }
}
