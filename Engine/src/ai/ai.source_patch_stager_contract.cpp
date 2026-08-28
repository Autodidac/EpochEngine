/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

module ai.source_patch_stager;

import core.sha256;

namespace epochengine::ai::source_patch_stager
{
    namespace
    {
        class ContractWriter final
        {
        public:
            void u8(const std::uint8_t value)
            {
                bytes_.push_back(static_cast<std::byte>(value));
            }
            void u32(const std::uint32_t value)
            {
                for (std::uint32_t shift{}; shift != 32u; shift += 8u)
                    u8(static_cast<std::uint8_t>(value >> shift));
            }
            void u64(const std::uint64_t value)
            {
                for (std::uint32_t shift{}; shift != 64u; shift += 8u)
                    u8(static_cast<std::uint8_t>(value >> shift));
            }
            void text(const std::string_view value)
            {
                u32(static_cast<std::uint32_t>(value.size()));
                bytes_.insert(bytes_.end(),
                    reinterpret_cast<const std::byte*>(value.data()),
                    reinterpret_cast<const std::byte*>(value.data() + value.size()));
            }
            [[nodiscard]] std::string digest() &&
            {
                return core::sha256::hex(core::sha256::hash(
                    std::span<const std::uint8_t>{
                        reinterpret_cast<const std::uint8_t*>(bytes_.data()),
                        bytes_.size()}));
            }
        private:
            std::vector<std::byte> bytes_{};
        };

        [[nodiscard]] std::string digest(const std::string_view value)
        {
            return core::sha256::hex(core::sha256::hash(value));
        }

        [[nodiscard]] Configuration configuration()
        {
            Configuration value{};
            value.stager_id = "stager-1";
            value.project_id = "epoch-project";
            value.session_id = "review-session-1";
            value.curated_session_id = 17u;
            value.campaign_id = "campaign-9";
            value.objective_id = "objective-1";
            value.operation_id = "operation-1";
#if defined(_WIN32)
            value.sandbox_root = "C:/epoch/contracts/sandbox";
#else
            value.sandbox_root = "/epoch/contracts/sandbox";
#endif
            value.sandbox_root_sha256 =
                sha256_hex(value.sandbox_root.generic_string());
            value.source_commit =
                "0123456789abcdef0123456789abcdef01234567";
            value.source_version = {0u, 89u, 32u};
            value.build_policy.require_renderer_smoke = true;
            value.build_policy.permit_explicit_renderer_skip = false;
            value.created_at_unix_seconds = 100u;
            value.expires_at_unix_seconds = 1000u;
            return value;
        }

        [[nodiscard]] source_patch_proposal::SealedProposal proposal()
        {
            source_patch_proposal::Configuration config{};
            config.engine_id = "proposal-engine-1";
            config.project_id = "epoch-project";
            config.workspace_root_sha256 =
                source_patch_proposal::sha256_hex("workspace-root");
            config.human_authority_sha256 =
                source_patch_proposal::sha256_hex("human-authority");
            config.source_revision = 42u;
            config.created_at_unix_seconds = 100u;
            config.expires_at_unix_seconds = 1000u;

            source_patch_proposal::Engine engine{};
            if (!engine.begin(config)) return {};

            const std::string before{"one\ntwo\n"};
            source_patch_proposal::ReviewedSource source{};
            source.project_id = config.project_id;
            source.source_revision = config.source_revision;
            source.relative_path = "Engine/src/ai/test.cpp";
            source.exists = true;
            source.content_sha256 =
                source_patch_proposal::sha256_hex(before);
            source.utf8_bytes = before;
            source.human_reviewed = true;
            source.update_permitted = true;

            source_patch_proposal::RangeEdit edit{};
            edit.edit_id = "edit-1";
            edit.start_line = 2u;
            edit.line_count = 1u;
            edit.expected_range_sha256 =
                source_patch_proposal::sha256_hex("two\n");
            edit.replacement_utf8 = "three\n";

            source_patch_proposal::FileOperation operation{};
            operation.operation_id = "file-operation-1";
            operation.kind = source_patch_proposal::OperationKind::update;
            operation.relative_path = source.relative_path;
            operation.base_source_revision = config.source_revision;
            operation.base_content_sha256 = source.content_sha256;
            operation.edits = {edit};

            source_patch_proposal::Authority authority{};
            authority.authority_id = "review-authority-1";
            authority.session_id = "review-session-1";
            authority.actor_sha256 = config.human_authority_sha256;
            authority.project_id = config.project_id;
            authority.workspace_root_sha256 = config.workspace_root_sha256;
            authority.review_receipt_sha256 =
                source_patch_proposal::sha256_hex("review-receipt");
            authority.source_snapshot_reviewed = true;
            authority.proposal_permitted = true;

            source_patch_proposal::Request request{};
            request.request_id = "proposal-request-1";
            request.title = "Bounded reviewed source change";
            request.rationale = "Simulate exact source changes for review.";
            request.expected_engine_generation = engine.snapshot().generation;
            request.expected_engine_state_sha256 =
                engine.snapshot().state_sha256;
            request.now_unix_seconds = 200u;
            request.authority = authority;
            request.operations = {operation};
            const auto packet = source_patch_proposal::encode_request(request);
            const source_patch_proposal::ReviewedSource sources[]{source};
            const auto result = engine.propose(packet, sources);
            return result.proposal.value_or(
                source_patch_proposal::SealedProposal{});
        }

        [[nodiscard]] std::string selection_digest(
            const curated_context_bundle::ReviewedEntry& entry)
        {
            return digest(entry.project_relative_path + "\n"
                + entry.declared_symbol + "\n"
                + std::to_string(entry.first_line) + "\n"
                + std::to_string(entry.last_line) + "\n"
                + std::to_string(entry.source_revision) + "\n"
                + digest(entry.exact_bytes));
        }

        [[nodiscard]] curated_context_bundle::Bundle curated()
        {
            curated_context_bundle::ReviewedEntry entry{};
            entry.project_relative_path = "Engine/src/ai/test.cpp";
            entry.declared_symbol = "test";
            entry.first_line = 1u;
            entry.last_line = 2u;
            entry.source_revision = 42u;
            entry.exact_bytes = "one\ntwo\n";
            entry.provenance.review_id = "review-42";
            entry.provenance.reviewer_binding = "editor-reviewed-source";
            entry.provenance.reviewed_at_unix_seconds = 150u;
            entry.provenance.selection_sha256 = selection_digest(entry);

            curated_context_bundle::Request request{};
            request.binding.project_id = "epoch-project";
#if defined(_WIN32)
            request.binding.reviewed_root = "C:/epoch/contracts/project";
#else
            request.binding.reviewed_root = "/epoch/contracts/project";
#endif
            request.binding.project_manifest_sha256 =
                std::string(64u, 'a');
            request.binding.project_profile_sha256 =
                std::string(64u, 'b');
            request.binding.reviewed_revision = 42u;
            request.binding.audience =
                curated_context_bundle::Audience::local_model;
            request.binding.provider =
                project_profile::Provider::epoch_local_qwen38;
            request.binding.model_binding = "qwen3.8-project-review";
            request.binding.endpoint_binding = "epoch-local-runtime";
            request.binding.session_id = 17u;
            request.binding.request_id = 29u;
            request.binding.campaign_id = "campaign-9";
            request.binding.campaign_generation = 3u;
            request.binding.operator_shared = true;
            request.reviewed_entries = {entry};
            return curated_context_bundle::build(request).bundle;
        }

        [[nodiscard]] std::string supervisor_receipt_digest(
            const iteration_supervisor_control::Receipt& receipt)
        {
            ContractWriter writer{};
            writer.text("epoch-ai-supervisor-receipt/v1");
            writer.text(receipt.receipt_id); writer.text(receipt.command_id);
            writer.text(receipt.command_sha256);
            writer.text(receipt.previous_receipt_sha256);
            writer.text(receipt.control_state_before_sha256);
            writer.text(receipt.queue_state_before_sha256);
            writer.text(receipt.queue_state_after_sha256);
            writer.text(receipt.scheduler_state_before_sha256);
            writer.text(receipt.scheduler_state_after_sha256);
            writer.text(receipt.campaign_id); writer.text(receipt.objective_id);
            writer.text(receipt.operation_id);
            writer.u8(static_cast<std::uint8_t>(receipt.outcome));
            writer.u64(receipt.completed_at_unix_seconds);
            writer.text(receipt.transition_sha256);
            return std::move(writer).digest();
        }

        struct ApprovalFixture final
        {
            iteration_supervisor_control::Snapshot snapshot{};
            iteration_supervisor_control::Receipt receipt{};
        };

        [[nodiscard]] ApprovalFixture approval()
        {
            ApprovalFixture value{};
            auto& receipt = value.receipt;
            receipt.receipt_id = "approval-receipt-1";
            receipt.command_id = "approve-command-1";
            receipt.command_sha256 = digest("approve-command");
            receipt.previous_receipt_sha256 = digest("previous-receipt");
            receipt.control_state_before_sha256 = digest("control-before");
            receipt.queue_state_before_sha256 = digest("queue-before");
            receipt.queue_state_after_sha256 = digest("queue-after");
            receipt.scheduler_state_before_sha256 = digest("scheduler-before");
            receipt.scheduler_state_after_sha256 = digest("scheduler-after");
            receipt.campaign_id = "campaign-9";
            receipt.objective_id = "objective-1";
            receipt.operation_id = "operation-1";
            receipt.outcome =
                iteration_supervisor_control::Outcome::approved;
            receipt.completed_at_unix_seconds = 220u;
            receipt.transition_sha256 = digest("approve-transition");
            receipt.receipt_sha256 = supervisor_receipt_digest(receipt);

            auto& snapshot = value.snapshot;
            snapshot.configuration.control_id = "control-1";
            snapshot.configuration.session_id = "review-session-1";
            snapshot.configuration.actor_sha256 = digest("actor");
            snapshot.configuration.campaign_id = "campaign-9";
            snapshot.configuration.created_at_unix_seconds = 100u;
            snapshot.configuration.expires_at_unix_seconds = 1000u;
            snapshot.generation = 7u;
            snapshot.previous_state_sha256 = digest("supervisor-previous");
            snapshot.state_sha256 = digest("supervisor-state");
            snapshot.phase =
                iteration_supervisor_control::ControlPhase::reviewed;
            snapshot.queue_generation = 3u;
            snapshot.queue_state_sha256 = receipt.queue_state_after_sha256;
            snapshot.scheduler_generation = 5u;
            snapshot.scheduler_state_sha256 =
                receipt.scheduler_state_after_sha256;
            snapshot.active_objective_id = receipt.objective_id;
            snapshot.active_operation_id = receipt.operation_id;
            snapshot.last_receipt_sha256 = receipt.receipt_sha256;
            return value;
        }

        [[nodiscard]] std::string destination_digest(
            const Configuration& config,
            const std::string_view path)
        {
            ContractWriter writer{};
            writer.text("epoch-ai-source-patch-destination/v1");
            writer.text(config.sandbox_root_sha256);
            writer.text(path);
            writer.text(config.project_id);
            writer.text(config.session_id);
            writer.text(config.operation_id);
            return std::move(writer).digest();
        }

        [[nodiscard]] Engine ready(const Configuration& config)
        {
            Engine engine{};
            (void)engine.begin(config);
            return engine;
        }

        [[nodiscard]] PrepareRequest prepare_request(
            const Engine& engine,
            const Configuration& config,
            const source_patch_proposal::SealedProposal& proposed,
            const curated_context_bundle::Bundle& bundle,
            const ApprovalFixture& approved)
        {
            PrepareRequest request{};
            request.request_id = "stage-request-1";
            request.expected_generation = engine.snapshot().generation;
            request.expected_state_sha256 = engine.snapshot().state_sha256;
            request.expected_supervisor_generation =
                approved.snapshot.generation;
            request.expected_supervisor_state_sha256 =
                approved.snapshot.state_sha256;
            request.expected_proposal_sha256 =
                proposed.canonical_proposal_sha256;
            request.expected_bundle_sha256 = bundle.bundle_sha256;
            request.expected_approval_receipt_sha256 =
                approved.receipt.receipt_sha256;
            request.now_unix_seconds = 250u;
            request.proposal = proposed;
            request.curated = bundle;
            request.supervisor = approved.snapshot;
            request.approval = approved.receipt;
            request.operator_approved = true;

            const auto& file = proposed.files.front();
            HostPreimage preimage{};
            preimage.relative_path = file.relative_path;
            preimage.kind = PathKind::regular;
            preimage.exact_bytes = "one\ntwo\n";
            preimage.content_sha256 = file.before_sha256;
            preimage.byte_count = preimage.exact_bytes.size();
            preimage.sandbox_root_sha256 = config.sandbox_root_sha256;
            preimage.destination_binding_sha256 =
                destination_digest(config, file.relative_path);
            preimage.beneath_sandbox = true;
            preimage.path_components_link_free = true;
            preimage.path_components_reparse_free = true;
            request.preimages = {preimage};
            return request;
        }

        [[nodiscard]] StagedReport staged_report(
            const Engine& engine,
            const StagePlan& plan)
        {
            StagedReport report{};
            report.report_id = "staged-report-1";
            report.expected_generation = engine.snapshot().generation;
            report.expected_state_sha256 = engine.snapshot().state_sha256;
            report.plan_sha256 = plan.plan_sha256;
            report.staging_manifest_sha256 =
                plan.staging_manifest_sha256;
            report.completed_at_unix_seconds = 300u;
            report.host_completed = true;
            report.atomic_transaction = true;
            report.rollback_available = true;
            for (const auto& operation : plan.operations)
            {
                StagedObservation observation{};
                observation.operation_id = operation.operation_id;
                observation.relative_path = operation.relative_path;
                observation.kind =
                    operation.kind == source_patch_bundle::Operation::remove
                    ? PathKind::missing : PathKind::regular;
                observation.exact_bytes = operation.postimage_utf8;
                observation.content_sha256 = operation.after_sha256;
                observation.byte_count = operation.after_byte_count;
                observation.sandbox_root_sha256 =
                    engine.snapshot().configuration.sandbox_root_sha256;
                observation.destination_binding_sha256 =
                    operation.destination_binding_sha256;
                observation.beneath_sandbox = true;
                observation.path_components_link_free = true;
                observation.path_components_reparse_free = true;
                report.observations.push_back(std::move(observation));
            }
            return report;
        }

        [[nodiscard]] build_validation::CheckEvidence passed(
            const build_validation::CheckLane lane)
        {
            return {.lane = lane,
                .status = build_validation::CheckStatus::passed,
                .duration_milliseconds = 1u,
                .evidence_sha256 = digest("check-" + std::to_string(
                    static_cast<std::uint8_t>(lane))),
                .diagnostic = "proven"};
        }

        [[nodiscard]] build_validation::ValidationReceipt validation(
            const Configuration& config,
            const std::string_view manifest)
        {
            build_validation::ValidationReceipt receipt{};
            receipt.source_version = config.source_version;
            receipt.packaged_version = {0u, 89u, 30u};
            receipt.source_commit = config.source_commit;
            receipt.source_tree_sha256 = manifest;
            receipt.platform = build_validation::Platform::windows_x64;
            receipt.compiler = build_validation::Compiler::msvc;
            receipt.configuration =
                build_validation::Configuration::release;
            receipt.target = "EpochEditor";
            receipt.toolchain = "msvc-19.44-vcpkg-static";
            receipt.artifact = {
                "epoch-contract.zip", digest("artifact"), 4096u};
            receipt.checks = {
                passed(build_validation::CheckLane::source_names),
                passed(build_validation::CheckLane::compile),
                passed(build_validation::CheckLane::engine_contract),
                passed(build_validation::CheckLane::headless_ci),
                passed(build_validation::CheckLane::package_inventory),
                passed(build_validation::CheckLane::dependency_resolution),
                passed(build_validation::CheckLane::renderer_smoke)};
            return receipt;
        }

        [[nodiscard]] BuildReport build_report(
            const Engine& engine,
            const StagedEvidence& staged)
        {
            BuildReport report{};
            report.report_id = "build-report-1";
            report.expected_generation = engine.snapshot().generation;
            report.expected_state_sha256 = engine.snapshot().state_sha256;
            report.staging_evidence_sha256 = staged.evidence_sha256;
            report.staging_manifest_sha256 =
                staged.staging_manifest_sha256;
            report.receipt = validation(engine.snapshot().configuration,
                staged.staging_manifest_sha256);
            report.receipt_sha256 =
                sha256_hex(build_validation::canonical_json(report.receipt));
            report.completed_at_unix_seconds = 350u;
            report.local_host = true;
            report.trusted_host = true;
            report.host_completed = true;
            return report;
        }

        template <typename Change>
        [[nodiscard]] bool prepare_refused(
            const Configuration& config,
            const source_patch_proposal::SealedProposal& proposed,
            const curated_context_bundle::Bundle& bundle,
            const ApprovalFixture& approved,
            Change change,
            const Code expected)
        {
            Engine engine = ready(config);
            PrepareRequest request =
                prepare_request(engine, config, proposed, bundle, approved);
            change(request);
            return engine.prepare(request).code == expected;
        }

        template <typename Change>
        [[nodiscard]] bool staged_refused(
            const Configuration& config,
            const source_patch_proposal::SealedProposal& proposed,
            const curated_context_bundle::Bundle& bundle,
            const ApprovalFixture& approved,
            Change change,
            const Code expected)
        {
            Engine engine = ready(config);
            const auto prepared = engine.prepare(
                prepare_request(engine, config, proposed, bundle, approved));
            if (!prepared || !prepared.plan) return false;
            StagedReport report = staged_report(engine, *prepared.plan);
            change(report);
            return engine.verify_staged(*prepared.plan, report).code == expected;
        }

        template <typename Change>
        [[nodiscard]] bool build_refused(
            const Configuration& config,
            const source_patch_proposal::SealedProposal& proposed,
            const curated_context_bundle::Bundle& bundle,
            const ApprovalFixture& approved,
            Change change,
            const Code expected)
        {
            Engine engine = ready(config);
            const auto prepared = engine.prepare(
                prepare_request(engine, config, proposed, bundle, approved));
            if (!prepared || !prepared.plan) return false;
            const auto staged = engine.verify_staged(
                *prepared.plan, staged_report(engine, *prepared.plan));
            if (!staged || !staged.staged) return false;
            BuildReport report = build_report(engine, *staged.staged);
            change(report);
            return engine.admit_build(*staged.staged, report).code == expected;
        }
    }

    bool run_contract()
    {
        const Configuration config = configuration();
        const auto proposed = proposal();
        const auto bundle = curated();
        const auto approved = approval();
        if (proposed.files.size() != 1u || bundle.entries.size() != 1u
            || proposed.canonical_proposal_sha256.empty()
            || bundle.bundle_sha256.empty())
            return false;

        Configuration unsafe = config;
        unsafe.live_source_write_permitted = true;
        Engine invalid{};
        if (invalid.begin(unsafe).code != Code::invalid_configuration)
            return false;

        Engine first = ready(config);
        Engine second = ready(config);
        const PrepareRequest first_request =
            prepare_request(first, config, proposed, bundle, approved);
        const PrepareRequest second_request =
            prepare_request(second, config, proposed, bundle, approved);
        const Result first_prepared = first.prepare(first_request);
        const Result second_prepared = second.prepare(second_request);
        if (!first_prepared || !second_prepared
            || !first_prepared.plan || !second_prepared.plan
            || *first_prepared.plan != *second_prepared.plan
            || first.snapshot().live_source_write_permitted
            || first.snapshot().promotion_permitted
            || first.snapshot().commit_permitted
            || first.snapshot().release_permitted)
            return false;
        if (first.prepare(first_request).code != Code::replay_refused)
            return false;

        if (!prepare_refused(config, proposed, bundle, approved,
                [](PrepareRequest& request)
                {
                    request.expected_generation++;
                }, Code::stale_state)
            || !prepare_refused(config, proposed, bundle, approved,
                [](PrepareRequest& request)
                {
                    request.live_source_write_requested = true;
                }, Code::authority_broadening)
            || !prepare_refused(config, proposed, bundle, approved,
                [](PrepareRequest& request)
                {
                    request.proposal.files.front().postimage_utf8 += "tamper";
                }, Code::proposal_tampered)
            || !prepare_refused(config, proposed, bundle, approved,
                [](PrepareRequest& request)
                {
                    request.curated.entries.front().exact_bytes += "tamper";
                }, Code::bundle_mismatch)
            || !prepare_refused(config, proposed, bundle, approved,
                [](PrepareRequest& request)
                {
                    request.operator_approved = false;
                }, Code::approval_required)
            || !prepare_refused(config, proposed, bundle, approved,
                [](PrepareRequest& request)
                {
                    request.approval.operation_id = "other-operation";
                }, Code::invalid_authority)
            || !prepare_refused(config, proposed, bundle, approved,
                [](PrepareRequest& request)
                {
                    request.preimages.clear();
                }, Code::partial_operations)
            || !prepare_refused(config, proposed, bundle, approved,
                [](PrepareRequest& request)
                {
                    request.preimages.push_back(request.preimages.front());
                }, Code::extra_operations)
            || !prepare_refused(config, proposed, bundle, approved,
                [](PrepareRequest& request)
                {
                    request.preimages.front().beneath_sandbox = false;
                }, Code::path_escape)
            || !prepare_refused(config, proposed, bundle, approved,
                [](PrepareRequest& request)
                {
                    request.preimages.front().kind = PathKind::symlink;
                }, Code::link_ambiguity)
            || !prepare_refused(config, proposed, bundle, approved,
                [](PrepareRequest& request)
                {
                    request.preimages.front().exact_bytes = "stale\n";
                }, Code::preimage_mismatch))
            return false;

        if (!staged_refused(config, proposed, bundle, approved,
                [](StagedReport& report)
                {
                    report.observations.clear();
                }, Code::partial_operations)
            || !staged_refused(config, proposed, bundle, approved,
                [](StagedReport& report)
                {
                    report.observations.push_back(report.observations.front());
                }, Code::extra_operations)
            || !staged_refused(config, proposed, bundle, approved,
                [](StagedReport& report)
                {
                    report.observations.front().kind = PathKind::reparse_point;
                }, Code::link_ambiguity)
            || !staged_refused(config, proposed, bundle, approved,
                [](StagedReport& report)
                {
                    report.observations.front().exact_bytes = "wrong\n";
                }, Code::wrong_postimage)
            || !staged_refused(config, proposed, bundle, approved,
                [](StagedReport& report)
                {
                    report.live_source_touched = true;
                }, Code::authority_broadening))
            return false;

        StagedReport accepted_report =
            staged_report(first, *first_prepared.plan);
        const Result accepted_staged =
            first.verify_staged(*first_prepared.plan, accepted_report);
        if (!accepted_staged || !accepted_staged.staged
            || !accepted_staged.staged->verified
            || accepted_staged.staged->live_source_write_permitted)
            return false;
        if (first.verify_staged(
                *first_prepared.plan, accepted_report).code
            != Code::replay_refused)
            return false;

        if (!build_refused(config, proposed, bundle, approved,
                [](BuildReport& report)
                {
                    report.trusted_host = false;
                }, Code::untrusted_build)
            || !build_refused(config, proposed, bundle, approved,
                [](BuildReport& report)
                {
                    report.receipt.checks.front().status =
                        build_validation::CheckStatus::failed;
                    report.receipt_sha256 = sha256_hex(
                        build_validation::canonical_json(report.receipt));
                }, Code::failed_build)
            || !build_refused(config, proposed, bundle, approved,
                [](BuildReport& report)
                {
                    report.receipt.checks.front().status =
                        build_validation::CheckStatus::skipped;
                    report.receipt_sha256 = sha256_hex(
                        build_validation::canonical_json(report.receipt));
                }, Code::failed_build)
            || !build_refused(config, proposed, bundle, approved,
                [](BuildReport& report)
                {
                    report.receipt.source_tree_sha256 = digest("wrong-tree");
                    report.receipt_sha256 = sha256_hex(
                        build_validation::canonical_json(report.receipt));
                }, Code::build_mismatch)
            || !build_refused(config, proposed, bundle, approved,
                [](BuildReport& report)
                {
                    report.promotion_performed = true;
                }, Code::authority_broadening))
            return false;

        const BuildReport accepted_build =
            build_report(first, *accepted_staged.staged);
        const Result admitted =
            first.admit_build(*accepted_staged.staged, accepted_build);
        if (!admitted || !admitted.build || !admitted.build->admitted)
            return false;
        if (first.admit_build(
                *accepted_staged.staged, accepted_build).code
            != Code::replay_refused)
            return false;
        return !admitted.build->upload_permitted
            && !admitted.build->promotion_permitted
            && !admitted.build->commit_permitted
            && !admitted.build->release_permitted
            && first.snapshot().phase == Phase::build_admitted
            && !first.snapshot().live_source_write_permitted
            && !first.snapshot().arbitrary_file_read_permitted
            && !first.snapshot().compiler_invocation_permitted
            && !first.snapshot().model_launch_permitted
            && !first.snapshot().promotion_permitted
            && !first.snapshot().commit_permitted
            && !first.snapshot().release_permitted
            && !first.snapshot().server_permitted
            && !first.snapshot().network_listener_permitted;
    }
}

#if defined(EPOCH_AI_SOURCE_PATCH_STAGER_CONTRACT_MAIN)
int main()
{
    return epochengine::ai::source_patch_stager::run_contract() ? 0 : 1;
}
#endif
