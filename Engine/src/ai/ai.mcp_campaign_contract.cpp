/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

module ai.mcp_campaign;

namespace epochengine::ai::mcp_campaign
{
    namespace
    {
        using iteration_session::CandidatePolicy;
        using iteration_session::IterationSession;
        using iteration_session::IterationTargetKind;
        using iteration_session::SessionPhase;
        using iteration_session::SourceAuthority;
        using iteration_session::SourceAuthorityKind;

        [[nodiscard]] McpToolCall call(
            std::string id,
            std::string tool,
            std::vector<McpArgument> arguments,
            const std::uint32_t step = 0u)
        {
            return McpToolCall{
                .session_id = "campaign-transport",
                .call_id = "rpc:s:" + id,
                .tool = std::move(tool),
                .arguments = std::move(arguments),
                .step = step};
        }

        [[nodiscard]] mcp_stdio::RequestId request(std::string id)
        {
            return {
                .kind = mcp_stdio::RequestIdKind::string,
                .string_value = std::move(id)};
        }

        struct Cleanup final
        {
            std::filesystem::path root{};
            ~Cleanup()
            {
                std::error_code ec{};
                std::filesystem::remove_all(root, ec);
            }
        };

        [[nodiscard]] SourceAuthority project_authority(
            std::filesystem::path root)
        {
            return SourceAuthority{
                .target_kind = IterationTargetKind::project_source,
                .kind = SourceAuthorityKind::verified_project,
                .root = std::move(root),
                .project_id = "mcp_campaign_project",
                .project_manifest_digest = std::string(64u, 'a'),
                .project_profile_digest = std::string(64u, 'b'),
                .verified = true};
        }

        [[nodiscard]] SourceAuthority engine_authority(
            std::filesystem::path root)
        {
            return SourceAuthority{
                .target_kind = IterationTargetKind::engine_source,
                .kind = SourceAuthorityKind::explicit_checkout,
                .root = std::move(root),
                .source_version = "0.89.31",
                .commit = std::string(40u, 'c'),
                .receipt_digest = std::string(64u, 'd'),
                .verified = true};
        }

        [[nodiscard]] ResolvedTargetHandle target(
            std::string id,
            SourceAuthority authority,
            const std::uint64_t generation)
        {
            std::error_code ec{};
            std::filesystem::create_directories(authority.root / "Source", ec);
            if (ec)
                return {};
            std::ofstream{authority.root / "Source/main.cpp", std::ios::binary}
                << "int epoch_mcp_campaign_contract() { return 31; }\n";
            const auto inspected = iteration_session::inspect_curated_files(
                authority, {"Source/main.cpp"});
            if (!inspected.accepted)
                return {};
            return ResolvedTargetHandle{
                .handle_id = std::move(id),
                .authority = std::move(authority),
                .curated_files = inspected.files,
                .cache_root = (std::filesystem::temp_directory_path()
                    / "epoch_mcp_campaign_cache").lexically_normal(),
                .host_generation = generation,
                .available = true};
        }

        [[nodiscard]] CampaignSnapshot campaign(
            const ResolvedTargetHandle& target_handle,
            const std::uint64_t generation)
        {
            IterationSession session{};
            auto begun = iteration_campaign::begin_campaign(
                iteration_campaign::CampaignConfiguration{
                    .authority = target_handle.authority,
                    .objective = "Improve the bounded project behavior.",
                    .model_name = "Qwen3.8-27B",
                    .curated_files = target_handle.curated_files,
                    .policy = CandidatePolicy::manual_each_candidate,
                    .budgets = iteration_campaign::default_budgets(
                        target_handle.authority.target_kind),
                    .created_at_unix_seconds = 2'000'000'000u},
                session);
            if (!begun)
                return {};
            begun.report.state_digest = std::string(64u, 'f');
            return CampaignSnapshot{
                .target_handle_id = target_handle.handle_id,
                .report = std::move(begun.report),
                .host_generation = generation,
                .available = true};
        }
    }

    bool run_contract()
    {
        constexpr std::uint64_t generation = 7u;
        const auto token = std::chrono::steady_clock::now()
            .time_since_epoch().count();
        const std::filesystem::path root =
            std::filesystem::temp_directory_path()
            / ("epoch_mcp_campaign_" + std::to_string(token));
        Cleanup cleanup{root};
        ResolvedTargetHandle project = target(
            "project-handle", project_authority(root / "project"), generation);
        ResolvedTargetHandle engine = target(
            "engine-handle", engine_authority(root / "engine"), generation);
        CampaignSnapshot active = campaign(project, generation);
        if (active.report.campaign_id.empty())
            return false;
        HostSnapshot host{
            .snapshot_id = "host-snapshot-7",
            .generation = generation,
            .now_unix_seconds = 2'000'000'100u,
            .targets = {project, engine},
            .campaigns = {active}};

        McpSessionAuthority authority{
            .session_id = "campaign-transport",
            .granted_capabilities = McpToolCapability::inspect
                | McpToolCapability::author
                | McpToolCapability::build
                | McpToolCapability::capture
                | McpToolCapability::engine_source,
            .approved_call_ids = {
                "rpc:s:start", "rpc:s:resume", "rpc:s:cancel",
                "rpc:s:validate", "rpc:s:engine-start",
                "rpc:s:raw-path"}};

        CampaignAdapter start_adapter{"campaign-transport"};
        const auto start_call = call("start", std::string{kStartTool}, {
            {"target_handle", "project-handle"},
            {"objective", "Add deterministic project diagnostics."},
            {"model", "Qwen3.8-27B"}});
        const AdapterResult start = start_adapter.dispatch(
            request("start"), start_call, authority, host);
        if (start.code != AdapterCode::pending_host_action
            || !start.pending_action
            || start.pending_action->kind()
                != HostActionKind::start_campaign
            || start.pending_action->target_handle_id() != "project-handle"
            || start.pending_action->start_configuration().authority
                != project.authority
            || start.pending_action->start_configuration().policy
                != CandidatePolicy::manual_each_candidate
            || start.pending_action->start_configuration().
                auto_validation_permit
            || start_adapter.dispatch(
                request("start"), start_call, authority, host).code
                != AdapterCode::replay_denied)
            return false;

        CampaignAdapter read_adapter{"campaign-transport"};
        const AdapterResult inspected = read_adapter.dispatch(
            request("inspect"),
            call("inspect", std::string{kInspectTargetTool},
                {{"target_handle", "project-handle"}}),
            authority, host);
        const AdapterResult status = read_adapter.dispatch(
            request("status"),
            call("status", std::string{kStatusTool},
                {{"campaign_id", active.report.campaign_id}}, 1u),
            authority, host);
        if (inspected.code != AdapterCode::ready
            || inspected.result.output.find("Source/main.cpp")
                != std::string::npos
            || status.code != AdapterCode::ready
            || status.result.output.find(active.report.state_digest)
                == std::string::npos)
            return false;

        CampaignAdapter resume_adapter{"campaign-transport"};
        const AdapterResult resumed = resume_adapter.dispatch(
            request("resume"),
            call("resume", std::string{kResumeTool},
                {{"campaign_id", active.report.campaign_id}}),
            authority, host);
        if (resumed.code != AdapterCode::pending_host_action
            || !resumed.pending_action
            || resumed.pending_action->campaign_id()
                != active.report.campaign_id
            || resumed.pending_action->campaign_session_identity()
                != active.report.session.identity
            || resumed.pending_action->current_authority()
                != project.authority)
            return false;

        CampaignAdapter cancel_adapter{"campaign-transport"};
        const AdapterResult cancelled = cancel_adapter.dispatch(
            request("cancel"),
            call("cancel", std::string{kCancelTool}, {
                {"campaign_id", active.report.campaign_id},
                {"reason", "Operator requested a bounded stop."}}),
            authority, host);
        if (cancelled.code != AdapterCode::pending_host_action
            || !cancelled.pending_action
            || cancelled.pending_action->kind()
                != HostActionKind::cancel_campaign
            || cancelled.pending_action->reason().empty())
            return false;

        HostSnapshot validating_host = host;
        validating_host.generation = generation + 1u;
        validating_host.snapshot_id = "host-snapshot-8";
        validating_host.targets[0].host_generation = generation + 1u;
        validating_host.targets[1].host_generation = generation + 1u;
        validating_host.campaigns[0].host_generation = generation + 1u;
        validating_host.campaigns[0].report.session.phase =
            SessionPhase::validating_candidate;
        validating_host.campaigns[0].report.session.candidate_digest =
            std::string(64u, '1');
        validating_host.campaigns[0].report.session.candidate_approved = true;
        CampaignAdapter validation_adapter{"campaign-transport"};
        const AdapterResult validation = validation_adapter.dispatch(
            request("validate"),
            call("validate", std::string{kValidateCandidateTool},
                {{"campaign_id",
                    validating_host.campaigns[0].report.campaign_id}}),
            authority, validating_host);
        if (validation.code != AdapterCode::pending_host_action
            || !validation.pending_action
            || validation.pending_action->kind()
                != HostActionKind::validate_candidate
            || validation.pending_action->candidate_digest()
                != std::string(64u, '1')
            || validation.pending_action->validation_step()
                != iteration_campaign::ValidationStep::project_compiler
            || validation.pending_action->expected_validation_record_count()
                != 1u)
            return false;

        McpSessionAuthority no_engine = authority;
        no_engine.granted_capabilities =
            McpToolCapability::inspect | McpToolCapability::author;
        CampaignAdapter denied_adapter{"campaign-transport"};
        const AdapterResult denied = denied_adapter.dispatch(
            request("engine-start"),
            call("engine-start", std::string{kStartTool}, {
                {"target_handle", "engine-handle"},
                {"objective", "Inspect and improve bounded engine evidence."},
                {"model", "Qwen3.8-27B"}}),
            no_engine, host);

        CampaignAdapter abuse_adapter{"campaign-transport"};
        const AdapterResult raw_path = abuse_adapter.dispatch(
            request("raw-path"),
            call("raw-path", std::string{kStartTool}, {
                {"target_handle", "project-handle"},
                {"objective", "Bounded objective."},
                {"model", "Qwen3.8-27B"},
                {"path", "C:/untrusted"}}),
            authority, host);
        const AdapterResult forbidden = abuse_adapter.dispatch(
            request("release"),
            call("release", "release.publish", {}),
            authority, host);
        McpToolCall cancelled_call = call(
            "cancelled-call", std::string{kStatusTool},
            {{"campaign_id", active.report.campaign_id}});
        cancelled_call.cancellation_requested = true;
        const AdapterResult pre_cancelled = abuse_adapter.dispatch(
            request("cancelled-call"), cancelled_call, authority, host);

        return denied.code == AdapterCode::capability_denied
            && raw_path.code == AdapterCode::invalid_request
            && forbidden.code == AdapterCode::unknown_tool
            && pre_cancelled.code == AdapterCode::cancelled
            && make_tool_registry().find("release.publish") == nullptr
            && make_tool_registry().find("network.listen") == nullptr;
    }
}
