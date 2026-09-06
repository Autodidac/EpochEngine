/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

module ai.self_iteration_orchestrator;

namespace epochengine::ai::self_iteration_orchestrator
{
    namespace
    {
        [[nodiscard]] ActionToken action(
            const Snapshot& snapshot,
            std::string transition,
            const std::uint64_t now,
            const bool approved = true)
        {
            return ActionToken{
                .expected_generation = snapshot.generation,
                .expected_state_sha256 = snapshot.state_sha256,
                .transition_id = std::move(transition),
                .now_unix_seconds = now,
                .operator_approved = approved};
        }

        [[nodiscard]] OperationReceipt receipt(
            const PendingOperation& operation,
            const std::uint64_t now)
        {
            return OperationReceipt{
                .operation_id = operation.operation_id(),
                .expected_generation = operation.expected_generation(),
                .expected_state_sha256 = operation.expected_state_sha256(),
                .transition_id = operation.transition_id(),
                .now_unix_seconds = now};
        }
    }

    bool run_contract()
    {
        namespace fs = std::filesystem;
        using namespace iteration_session;
        constexpr std::uint64_t now = 2'100'000'000u;
        const auto token = std::chrono::steady_clock::now()
            .time_since_epoch().count();
        const fs::path root = fs::temp_directory_path()
            / ("eai_orch_" + std::to_string(token));
        const fs::path project = root / "Project";
        const fs::path cache = root / "cache/ai";
        std::error_code ec{};
        fs::create_directories(project / "Source", ec);
        std::ofstream{project / "Source/main.cpp", std::ios::binary}
            << "int epoch_orchestrator_contract() { return 89; }\n";

        const auto profile = project_profile::serialize_profile(
            project_profile::make_profile(
                project_profile::Provider::epoch_local_qwen38));
        const SourceAuthority project_authority{
            .target_kind = IterationTargetKind::project_source,
            .kind = SourceAuthorityKind::verified_project,
            .root = fs::weakly_canonical(project, ec),
            .project_id = "orchestrator_contract",
            .project_manifest_digest = std::string(64u, 'a'),
            .project_profile_digest = profile.sha256,
            .verified = !ec};
        const auto inspected = inspect_curated_files(
            project_authority, {"Source/main.cpp"});
        const HostBinding local_host{
            .binding_id = "local-qwen-host",
            .configuration_sha256 = std::string(64u, 'b'),
            .generation = 7u,
            .stdio_only = true};
        const Configuration configuration{
            .authority = project_authority,
            .curated_files = inspected.files,
            .cache_root = cache,
            .objective = "Improve the admitted generated project without touching Engine source.",
            .provider = project_profile::Provider::epoch_local_qwen38,
            .project_profile_bytes = profile.canonical_bytes,
            .host = local_host,
            .budgets = iteration_campaign::default_budgets(
                IterationTargetKind::project_source),
            .created_at_unix_seconds = now,
            .project_source_campaign_permitted = true,
            .sandbox_apply_permitted = true};
        Orchestrator first{};
        Result begun = inspected.accepted ? first.begin(configuration) : Result{};
        if (!begun || begun.snapshot.phase != Phase::awaiting_plan_request
            || begun.snapshot.transport != TransportKind::guarded_local_mcp_child
            || begun.snapshot.campaign.session.source.target_kind
                != IterationTargetKind::project_source
            || begun.snapshot.promotion_permitted || begun.snapshot.git_permitted
            || begun.snapshot.release_permitted
            || begun.snapshot.network_or_server_permitted)
        {
            fs::remove_all(root, ec);
            return false;
        }

        Result plan_requested = first.request_plan(
            action(begun.snapshot, "plan-request-1", now + 1u));
        if (!plan_requested || !plan_requested.pending_operation
            || plan_requested.pending_operation->kind() != OperationKind::model_plan
            || first.request_plan(action(
                begun.snapshot, "plan-request-replay", now + 2u)))
        {
            fs::remove_all(root, ec);
            return false;
        }

        // Simulate a process crash while a model request is pending. Resume must
        // revalidate authority/files and discard every pending request/approval.
        Orchestrator restarted{};
        Result resumed = restarted.resume(
            configuration, plan_requested.state_path, now + 3u);
        if (!resumed || resumed.snapshot.phase != Phase::awaiting_plan_request
            || !resumed.snapshot.pending_operation_id.empty()
            || resumed.snapshot.campaign.session.candidate_approved
            || !resumed.snapshot.campaign.session.validation.empty())
        {
            fs::remove_all(root, ec);
            return false;
        }
        Result fresh_plan = restarted.request_plan(
            action(resumed.snapshot, "plan-request-2", now + 4u));
        if (!fresh_plan || !fresh_plan.pending_operation)
        {
            fs::remove_all(root, ec);
            return false;
        }
        OperationReceipt stale_plan = receipt(*fresh_plan.pending_operation, now + 5u);
        stale_plan.operation_id = std::string(64u, 'f');
        if (restarted.record_plan(stale_plan, "bounded plan", "stale plan"))
        {
            fs::remove_all(root, ec);
            return false;
        }
        Result planned = restarted.record_plan(
            receipt(*fresh_plan.pending_operation, now + 5u),
            "Inspect the admitted file, propose one bounded patch, then validate it.",
            "Local model returned one bounded plan.");
        if (!planned || planned.snapshot.phase != Phase::awaiting_curated_evidence
            || restarted.share_curated_evidence(
                action(planned.snapshot, "share-wrong", now + 6u),
                std::string(64u, '1'), std::string(64u, '2'), "wrong scope"))
        {
            fs::remove_all(root, ec);
            return false;
        }
        Result shared = restarted.share_curated_evidence(
            action(planned.snapshot, "share-exact", now + 6u),
            planned.snapshot.campaign.session.scope_digest,
            std::string(64u, '2'),
            "Operator shared only the exact curated scope.");
        Result proposal_request = shared ? restarted.request_proposal(
            action(shared.snapshot, "proposal-request", now + 7u)) : Result{};
        Result proposed = proposal_request && proposal_request.pending_operation
            ? restarted.record_proposal(
                receipt(*proposal_request.pending_operation, now + 8u),
                "candidate: replace exact admitted function body only",
                "Model returned one digest-bound candidate.") : Result{};
        if (!proposed || proposed.snapshot.phase != Phase::awaiting_manual_review
            || restarted.review_proposal(
                action(proposed.snapshot, "review-no-approval", now + 9u, false),
                true, "approval missing"))
        {
            fs::remove_all(root, ec);
            return false;
        }
        Result reviewed = restarted.review_proposal(
            action(proposed.snapshot, "review-approved", now + 9u),
            true, "Operator reviewed the exact proposal digest.");
        Result apply_request = reviewed ? restarted.decide_apply(
            action(reviewed.snapshot, "apply-approved", now + 10u),
            true, "Operator approved sandbox-only application.") : Result{};
        if (!apply_request || !apply_request.pending_operation
            || apply_request.pending_operation->kind() != OperationKind::sandbox_apply)
        {
            fs::remove_all(root, ec);
            return false;
        }
        Result applied = restarted.record_apply(
            receipt(*apply_request.pending_operation, now + 11u),
            std::string(64u, '3'),
            "Fake host recorded exact disposable-workspace implementation evidence.");
        if (!applied || applied.snapshot.phase != Phase::awaiting_validation_request)
        {
            fs::remove_all(root, ec);
            return false;
        }

        Result current = applied;
        constexpr char validation_hex[] = {'4', '5', '6', '7', '8', '9', 'a'};
        for (std::uint32_t index = 0u; index < 7u; ++index)
        {
            Result requested = restarted.request_validation(action(
                current.snapshot, "validation-request-" + std::to_string(index),
                now + 12u + index * 2u));
            if (!requested || !requested.pending_operation
                || requested.pending_operation->validation_index() != index)
            {
                fs::remove_all(root, ec);
                return false;
            }
            if (index == 0u)
            {
                OperationReceipt stale = receipt(
                    *requested.pending_operation, now + 13u);
                stale.expected_state_sha256 = std::string(64u, '0');
                if (restarted.record_validation(
                    stale, std::string(64u, '4'), "stale evidence", true))
                {
                    fs::remove_all(root, ec);
                    return false;
                }
                const auto beforeRejectedReceipt = restarted.snapshot();
                for (const std::string& malformedSummary : {
                    std::string(2049u, 'x'), std::string(4097u, 'x'),
                    std::string{"bad\0summary", 11u}})
                {
                    const auto refusedSummary = restarted.record_validation(
                        receipt(*requested.pending_operation, now + 13u),
                        std::string(64u, '4'), malformedSummary, false);
                    const auto afterRejectedReceipt = restarted.snapshot();
                    if (refusedSummary
                        || afterRejectedReceipt.generation != beforeRejectedReceipt.generation
                        || afterRejectedReceipt.state_sha256 != beforeRejectedReceipt.state_sha256
                        || afterRejectedReceipt.phase != beforeRejectedReceipt.phase
                        || afterRejectedReceipt.pending_operation_id
                            != beforeRejectedReceipt.pending_operation_id
                        || afterRejectedReceipt.validation_index != 0u
                        || !afterRejectedReceipt.campaign.session.validation.empty())
                    {
                        fs::remove_all(root, ec);
                        return false;
                    }
                }
                // The valid receipt immediately below must still be accepted:
                // rejected outer descriptions cannot consume the inner actor.
            }
            current = restarted.record_validation(
                receipt(*requested.pending_operation, now + 13u + index * 2u),
                std::string(64u, validation_hex[index]),
                "Fake trusted host validated the exact candidate digest.", true);
            if (!current)
            {
                fs::remove_all(root, ec);
                return false;
            }
        }
        Result checkpointed = restarted.checkpoint(action(
            current.snapshot, "checkpoint-approved", now + 30u),
            "Verified candidate recorded as a rollback checkpoint; live promotion remains separate.");
        if (!checkpointed || checkpointed.snapshot.phase != Phase::checkpointed
            || checkpointed.snapshot.promotion_permitted
            || checkpointed.snapshot.git_permitted
            || checkpointed.snapshot.release_permitted)
        {
            fs::remove_all(root, ec);
            return false;
        }
        // External MCP is preserved as a separate project-provider choice; it
        // does not inherit the local child binding or gain network authority.
        const auto external_profile = project_profile::serialize_profile(
            project_profile::make_profile(project_profile::Provider::external_mcp));
        SourceAuthority external_authority = project_authority;
        external_authority.project_id = "external_orchestrator_contract";
        external_authority.project_profile_digest = external_profile.sha256;
        Configuration external = configuration;
        external.authority = external_authority;
        external.provider = project_profile::Provider::external_mcp;
        external.project_profile_bytes = external_profile.canonical_bytes;
        external.operator_model_binding = "Qwen3.8-27B-external-mcp";
        external.host = HostBinding{
            .binding_id = "external-mcp-host",
            .configuration_sha256 = std::string(64u, 'c'),
            .generation = 3u,
            .stdio_only = true};
        external.created_at_unix_seconds = now + 40u;
        Orchestrator external_orchestrator{};
        Result external_begun = external_orchestrator.begin(external);

        const fs::path engine_root = root / "EngineAuthority";
        fs::create_directories(engine_root, ec);
        std::ofstream{engine_root / "owned.cpp", std::ios::binary}
            << "int owned_engine_source() { return 31; }\n";
        const SourceAuthority engine_authority{
            .target_kind = IterationTargetKind::engine_source,
            .kind = SourceAuthorityKind::explicit_checkout,
            .root = fs::weakly_canonical(engine_root, ec),
            .source_version = "0.89.32",
            .commit = std::string(40u, 'd'),
            .receipt_digest = std::string(64u, 'e'),
            .verified = !ec};
        const auto engine_files = inspect_curated_files(
            engine_authority, {"owned.cpp"});
        Configuration engine{
            .authority = engine_authority,
            .curated_files = engine_files.files,
            .cache_root = cache,
            .objective = "Improve one admitted Engine source outcome in a disposable workspace.",
            .provider = project_profile::Provider::epoch_local_qwen38,
            .engine_model_binding = "os_model_qwen_3_8_27b",
            .host = local_host,
            .budgets = iteration_campaign::default_budgets(
                IterationTargetKind::engine_source),
            .created_at_unix_seconds = now + 50u,
            .engine_source_campaign_permitted = true,
            .sandbox_apply_permitted = true};
        Orchestrator engine_orchestrator{};
        Result engine_begun = engine_files.accepted
            ? engine_orchestrator.begin(engine) : Result{};
        Result cancelled = engine_begun ? engine_orchestrator.cancel(
            action(engine_begun.snapshot, "engine-cancel", now + 51u),
            "Operator cancelled before any model or source operation.") : Result{};

        const bool ok = external_begun
            && external_begun.snapshot.transport == TransportKind::external_mcp
            && !external_begun.snapshot.network_or_server_permitted
            && external_begun.snapshot.target_key != begun.snapshot.target_key
            && engine_begun
            && engine_begun.snapshot.campaign.session.source.target_kind
                == IterationTargetKind::engine_source
            && engine_begun.snapshot.target_key != begun.snapshot.target_key
            && cancelled && cancelled.snapshot.phase == Phase::cancelled
            && cancelled.snapshot.campaign.cancelled;
        fs::remove_all(root, ec);
        return ok;
    }
}
