/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>

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

        [[nodiscard]] std::string read_bytes(const std::filesystem::path& path)
        {
            std::ifstream input{path, std::ios::binary};
            return {std::istreambuf_iterator<char>{input},
                std::istreambuf_iterator<char>{}};
        }

        [[nodiscard]] bool write_bytes(
            const std::filesystem::path& path, const std::string_view bytes)
        {
            std::error_code ec{};
            std::filesystem::create_directories(path.parent_path(), ec);
            if (ec) return false;
            std::ofstream output{path, std::ios::binary | std::ios::trunc};
            output << bytes;
            output.close();
            return static_cast<bool>(output);
        }

        [[nodiscard]] bool same_validation_state(const Snapshot& before, const Snapshot& after)
        {
            return before.generation == after.generation
                && before.state_sha256 == after.state_sha256
                && before.previous_state_sha256 == after.previous_state_sha256
                && before.phase == after.phase && before.status == after.status
                && before.pending_operation_id == after.pending_operation_id
                && before.validation_index == after.validation_index
                && before.evidence.size() == after.evidence.size()
                && before.candidate_sha256 == after.candidate_sha256
                && before.proposal_sha256 == after.proposal_sha256
                && before.campaign.record_generation == after.campaign.record_generation
                && before.campaign.state_digest == after.campaign.state_digest
                && before.campaign.counters == after.campaign.counters
                && before.campaign.session.identity == after.campaign.session.identity
                && before.campaign.session.phase == after.campaign.session.phase
                && before.campaign.session.status == after.campaign.session.status
                && before.campaign.session.repair_attempt == after.campaign.session.repair_attempt
                && before.campaign.session.candidate_approved == after.campaign.session.candidate_approved
                && before.campaign.session.validation.size() == after.campaign.session.validation.size();
        }

        [[nodiscard]] std::filesystem::path record_path(
            const Configuration& configuration, const Snapshot& snapshot)
        {
            return configuration.cache_root / "orchestrator_records"
                / std::to_string(snapshot.campaign.record_generation)
                / (snapshot.campaign.state_digest + ".epochai");
        }

        [[nodiscard]] bool failed_publication_contract(
            Orchestrator& orchestrator, const Result& requested,
            const std::uint64_t now, const std::string& evidence,
            const std::string& summary, const bool passed)
        {
            namespace fs = std::filesystem;
            const Snapshot before = orchestrator.snapshot();
            const std::string bytes = read_bytes(requested.state_path);
            const fs::path backup = requested.state_path.string() + ".previous";
            std::error_code ec{};
            fs::rename(requested.state_path, backup, ec);
            if (ec) return false;
            // A directory at the publication destination forces the real
            // atomic-replace primitive to fail on both Windows and POSIX.
            const bool blocked = fs::create_directory(requested.state_path, ec) && !ec;
            Result refused{};
            if (blocked) refused = orchestrator.record_validation(
                receipt(*requested.pending_operation, now), evidence, summary, passed);
            const bool unchanged = blocked && !refused
                && refused.status.find("atomically replaced") != std::string::npos
                && same_validation_state(before, refused.snapshot)
                && same_validation_state(before, orchestrator.snapshot())
                && read_bytes(backup) == bytes;
            if (blocked) fs::remove(requested.state_path, ec);
            fs::rename(backup, requested.state_path, ec);
            return unchanged && !ec && read_bytes(requested.state_path) == bytes;
        }

        [[nodiscard]] bool durable_reference_contract(
            const Configuration& configuration, const Result& requested,
            const std::uint64_t now)
        {
            namespace fs = std::filesystem;
            const Snapshot& before = requested.snapshot;
            const std::string checkpoint = read_bytes(requested.state_path);
            const fs::path exact = record_path(configuration, before);
            const std::string exact_bytes = read_bytes(exact);
            if (checkpoint.empty() || exact_bytes.empty()) return false;
            const fs::path next_generation = configuration.cache_root / "orchestrator_records"
                / std::to_string(before.campaign.record_generation + 1u);
            iteration_campaign::CampaignResult unpublished{};
            std::error_code ec{};
            fs::directory_iterator entries{next_generation, ec};
            if (ec) return false;
            for (const auto& entry : entries)
            {
                auto candidate = iteration_campaign::load_report(entry.path());
                if (candidate && candidate.report.campaign_id == before.campaign.campaign_id
                    && !candidate.report.session.validation.empty()
                    && candidate.report.session.validation.back().passed)
                    unpublished = std::move(candidate);
            }
            if (!unpublished || unpublished.state_digest == before.campaign.state_digest)
                return false;
            auto shared = iteration_campaign::save_report(
                configuration.cache_root, unpublished.report);
            if (!shared) return false;

            auto reload = [&](const std::string_view name, const std::string& bytes,
                              const bool accepted)
            {
                const fs::path copy = configuration.cache_root / "reload"
                    / (std::string{name} + ".epochai");
                if (!write_bytes(copy, bytes)) return false;
                Orchestrator restarted{};
                const auto result = restarted.resume(configuration, copy, now);
                return accepted
                    ? result.accepted
                        && result.snapshot.campaign.previous_state_digest == before.campaign.state_digest
                        && result.snapshot.campaign.record_generation == before.campaign.record_generation + 1u
                        && result.snapshot.pending_operation_id.empty()
                        && result.snapshot.campaign.session.validation.empty()
                        && result.snapshot.campaign.counters == before.campaign.counters
                    : !result && read_bytes(copy) == bytes;
            };
            // A newer shared record (or orphan from interrupted publication)
            // must never replace the exact generation referenced by V2.
            if (!reload("exact", checkpoint, true)
                || read_bytes(exact) != exact_bytes) return false;
            const fs::path backup = exact.string() + ".previous";
            fs::rename(exact, backup, ec);
            if (ec) return false;
            const bool missing_refused = reload("missing", checkpoint, false)
                && !fs::exists(exact);
            const bool corrupt_refused = write_bytes(exact, "corrupt immutable record")
                && reload("corrupt", checkpoint, false)
                && read_bytes(exact) == "corrupt immutable record";
            fs::remove(exact, ec);
            fs::rename(backup, exact, ec);
            if (ec || !missing_refused || !corrupt_refused
                || read_bytes(exact) != exact_bytes) return false;

            // V1 used a shared campaign path, so compatibility is allowed only
            // for an equal digest AND generation, never merely a newer record.
            std::string legacy = checkpoint;
            const std::string current_header = "EPOCH_AI_SELF_ITERATION_ORCHESTRATOR_V2\n";
            if (!legacy.starts_with(current_header)) return false;
            legacy.replace(0u, current_header.size(), "EPOCH_AI_SELF_ITERATION_ORCHESTRATOR_V1\n");
            shared = iteration_campaign::save_report(configuration.cache_root, before.campaign);
            if (!shared || shared.state_digest != before.campaign.state_digest
                || !reload("legacy-exact", legacy, true)) return false;
            auto changed = before.campaign;
            changed.status = "Different bytes at the same campaign generation.";
            changed.state_digest.clear();
            shared = iteration_campaign::save_report(configuration.cache_root, changed);
            if (!shared || shared.state_digest == before.campaign.state_digest
                || !reload("legacy-wrong-digest", legacy, false)) return false;
            shared = iteration_campaign::save_report(configuration.cache_root, unpublished.report);
            return shared.accepted && reload("legacy-newer", legacy, false)
                && read_bytes(requested.state_path) == checkpoint
                && read_bytes(exact) == exact_bytes;
        }

        [[nodiscard]] bool capacity_contract(Configuration configuration, const std::uint64_t now)
        {
            configuration.cache_root /= "capacity";
            Limits limits{};
            limits.maximum_state_bytes = 64u * 1024u;
            limits.maximum_summary_bytes = 4096u;
            Orchestrator orchestrator{};
            Result step = orchestrator.begin(configuration, limits);
            if (!step) return false;
            step = orchestrator.request_plan(action(step.snapshot, "plan", now + 1u));
            if (!step || !step.pending_operation) return false;
            // Five bounded descriptions fit individually; percent escaping
            // makes their aggregate leave insufficient space for a long receipt.
            const std::string large(3072u, '%');
            step = orchestrator.record_plan(receipt(*step.pending_operation, now + 2u),
                "One bounded plan", large);
            if (!step) return false;
            step = orchestrator.share_curated_evidence(action(step.snapshot, "share", now + 3u),
                step.snapshot.campaign.session.scope_digest, std::string(64u, '2'), large);
            if (!step) return false;
            step = orchestrator.request_proposal(action(step.snapshot, "proposal", now + 4u));
            if (!step || !step.pending_operation) return false;
            step = orchestrator.record_proposal(receipt(*step.pending_operation, now + 5u),
                "One bounded candidate", large);
            if (!step) return false;
            step = orchestrator.review_proposal(action(step.snapshot, "review", now + 6u), true, large);
            if (!step) return false;
            step = orchestrator.decide_apply(action(step.snapshot, "apply", now + 7u), true, large);
            if (!step || !step.pending_operation) return false;
            step = orchestrator.record_apply(receipt(*step.pending_operation, now + 8u),
                std::string(64u, '3'), "Host applied exact candidate.");
            if (!step) return false;
            step = orchestrator.request_validation(action(step.snapshot, "validation", now + 9u));
            if (!step || !step.pending_operation) return false;
            const Snapshot before = orchestrator.snapshot();
            const std::string bytes = read_bytes(step.state_path);
            const auto operation_receipt = receipt(*step.pending_operation, now + 10u);
            const auto refused = orchestrator.record_validation(operation_receipt,
                std::string(64u, '4'), std::string(4096u, '%'), false);
            if (refused || refused.status != "Orchestration state exceeds its persistence budget."
                || !same_validation_state(before, refused.snapshot)
                || !same_validation_state(before, orchestrator.snapshot())
                || read_bytes(step.state_path) != bytes) return false;
            const auto retried = orchestrator.record_validation(operation_receipt,
                std::string(64u, '4'), "Compiler failed; request one bounded repair.", false);
            return retried.accepted && retried.snapshot.phase == Phase::awaiting_proposal_request
                && retried.snapshot.campaign.session.validation.size() == 1u
                && retried.snapshot.campaign.session.repair_attempt == 1u
                && !orchestrator.record_validation(operation_receipt,
                    std::string(64u, '4'), "Stale receipt after successful retry.", false);
        }

        [[nodiscard]] bool redirected_cache_contract(
            Configuration configuration, const std::uint64_t now)
        {
            namespace fs = std::filesystem;
            const auto fixture = configuration.cache_root / "redirect-fixture";
            const auto outside = fixture / "outside";
            std::error_code ec{};
            if (!write_bytes(outside / "canary", "outside cache canary")) return false;
            const auto unchanged = [&]
            {
                return read_bytes(outside / "canary") == "outside cache canary";
            };
            // Link creation is required proof, not a skipped passing branch.
            // Windows hosts must provide symlink privilege/Developer Mode.
            const auto link = [&](const fs::path& target, const fs::path& path)
            {
                fs::create_directories(path.parent_path(), ec);
                if (ec) return false;
                fs::create_directory_symlink(target, path, ec);
                return !ec;
            };
            for (const std::string prefix : {"orchestrator_records", "op"})
            {
                configuration.cache_root = fixture / ("write-" + prefix);
                if (!link(outside, configuration.cache_root / prefix)) return false;
                Orchestrator rejected{};
                const auto result = rejected.begin(configuration);
                if (result || !unchanged() || fs::exists(outside / "1")) return false;
                fs::remove(configuration.cache_root / prefix, ec);
                if (ec) return false;
            }

            const auto physical = fixture / "physical";
            const auto alias = fixture / "trusted-alias";
            fs::create_directories(physical, ec);
            if (ec || !link(physical, alias)) return false;
            configuration.cache_root = alias;
            Orchestrator first{};
            const auto begun = first.begin(configuration);
            if (!begun || !unchanged()) return false;
            Orchestrator alias_reload{};
            const auto resumed = alias_reload.resume(configuration, begun.state_path, now + 1u);
            if (!resumed || resumed.snapshot.campaign.previous_state_digest
                != begun.snapshot.campaign.state_digest) return false;
            configuration.cache_root = physical;
            const auto checkpoint = read_bytes(resumed.state_path);
            const auto exact = record_path(configuration, resumed.snapshot);
            const auto campaign_bytes = read_bytes(exact);
            const auto records = physical / "orchestrator_records";
            const auto saved_records = physical / "records-original";
            const auto generation = resumed.snapshot.campaign.record_generation;
            const auto outside_exact = outside / std::to_string(generation)
                / (resumed.snapshot.campaign.state_digest + ".epochai");
            if (checkpoint.empty() || campaign_bytes.empty()
                || !write_bytes(outside_exact, campaign_bytes)) return false;
            fs::rename(records, saved_records, ec);
            if (ec || !link(outside, records)) return false;
            Orchestrator redirected{};
            const auto denied = redirected.resume(configuration, resumed.state_path, now + 2u);
            const bool exact_refused = !denied && denied.snapshot.generation == 0u
                && read_bytes(resumed.state_path) == checkpoint
                && read_bytes(outside_exact) == campaign_bytes && unchanged()
                && !fs::exists(outside / std::to_string(generation + 1u));
            fs::remove(records, ec);
            if (ec) return false;
            fs::rename(saved_records, records, ec);
            if (ec || !exact_refused) return false;

            const auto shared = iteration_campaign::save_report(physical, resumed.snapshot.campaign);
            if (!shared) return false;
            const auto shared_relative = shared.state_path.lexically_relative(physical / "campaigns");
            if (!write_bytes(outside / shared_relative, read_bytes(shared.state_path))) return false;
            fs::rename(physical / "campaigns", physical / "campaigns-original", ec);
            if (ec || !link(outside, physical / "campaigns")) return false;
            std::string legacy = checkpoint;
            const std::string header = "EPOCH_AI_SELF_ITERATION_ORCHESTRATOR_V2\n";
            legacy.replace(0u, header.size(), "EPOCH_AI_SELF_ITERATION_ORCHESTRATOR_V1\n");
            const auto legacy_path = physical / "legacy-state.epochai";
            if (!write_bytes(legacy_path, legacy)) return false;
            Orchestrator legacy_reader{};
            const auto legacy_denied = legacy_reader.resume(configuration, legacy_path, now + 3u);
            const bool legacy_refused = !legacy_denied && legacy_denied.snapshot.generation == 0u
                && read_bytes(legacy_path) == legacy && unchanged();
            fs::remove(physical / "campaigns", ec);
            if (ec) return false;
            fs::rename(physical / "campaigns-original", physical / "campaigns", ec);
            if (ec || !legacy_refused) return false;

            const auto outer = physical / "outer-link";
            if (!write_bytes(outside / "state.epochai", checkpoint) || !link(outside, outer))
                return false;
            Orchestrator outer_reader{};
            const auto outer_denied = outer_reader.resume(configuration, outer / "state.epochai", now + 4u);
            const bool outer_refused = !outer_denied && outer_denied.snapshot.generation == 0u
                && read_bytes(outside / "state.epochai") == checkpoint && unchanged();
            fs::remove(outer, ec);
            if (ec) return false;
            fs::remove(alias, ec);
            return !ec && outer_refused;
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
                if (!failed_publication_contract(restarted, requested, now + 13u,
                        std::string(64u, '4'), "Compiler failed; preserve a retryable receipt.", false))
                {
                    fs::remove_all(root, ec);
                    return false;
                }
            }
            // Exercise the final persistence boundary for every trusted actor.
            // The exact retry must reuse the same immutable report successfully.
            if (!failed_publication_contract(restarted, requested,
                    now + 13u + index * 2u, std::string(64u, validation_hex[index]),
                    "Fake trusted host validated the exact candidate digest.", true)
                || (index == 0u && !durable_reference_contract(
                    configuration, requested, now + 13u)))
            {
                fs::remove_all(root, ec);
                return false;
            }
            current = restarted.record_validation(
                receipt(*requested.pending_operation, now + 13u + index * 2u),
                std::string(64u, validation_hex[index]),
                "Fake trusted host validated the exact candidate digest.", true);
            if (!current
                || current.snapshot.validation_index != index + 1u
                || current.snapshot.campaign.session.validation.size() != index + 1u
                || restarted.record_validation(
                    receipt(*requested.pending_operation, now + 13u + index * 2u),
                    std::string(64u, validation_hex[index]), "Consumed receipt replay.", true))
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

        const bool ok = capacity_contract(configuration, now + 60u)
            && redirected_cache_contract(configuration, now + 80u)
            && external_begun
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
