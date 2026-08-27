/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>

module editor.ai_development_controller;

namespace epochengine::editor_ai_development
{
    namespace
    {
        [[nodiscard]] SessionConfiguration configuration(
            std::uint64_t session_value = 77u)
        {
            return {
                .session_value = session_value,
                .session_generation = 1u,
                .workspace_id = "epoch.editor.contract",
                .engine_source_root = "Engine",
                .project_source_root = "Projects",
                .build_output_root = "x64",
                .evidence_root = "logs",
                .operator_id = "operator.primary",
                .opened_at = {1'000u},
                .expires_at = {20'000u},
                .clock_policy = ClockPolicy::externally_driven_contract};
        }

        [[nodiscard]] int evidence_digest_failure_code() noexcept
        {
            constexpr std::array<std::uint8_t, 32u> empty_expected{
                0xe3u, 0xb0u, 0xc4u, 0x42u, 0x98u, 0xfcu, 0x1cu, 0x14u,
                0x9au, 0xfbu, 0xf4u, 0xc8u, 0x99u, 0x6fu, 0xb9u, 0x24u,
                0x27u, 0xaeu, 0x41u, 0xe4u, 0x64u, 0x9bu, 0x93u, 0x4cu,
                0xa4u, 0x95u, 0x99u, 0x1bu, 0x78u, 0x52u, 0xb8u, 0x55u};
            constexpr std::array<std::uint8_t, 32u> abc_expected{
                0xbau, 0x78u, 0x16u, 0xbfu, 0x8fu, 0x01u, 0xcfu, 0xeau,
                0x41u, 0x41u, 0x40u, 0xdeu, 0x5du, 0xaeu, 0x22u, 0x23u,
                0xb0u, 0x03u, 0x61u, 0xa3u, 0x96u, 0x17u, 0x7au, 0x9cu,
                0xb4u, 0x10u, 0xffu, 0x61u, 0xf2u, 0x00u, 0x15u, 0xadu};
            if (evidence_digest("").bytes != empty_expected)
                return 11;
            if (evidence_digest("abc").bytes != abc_expected)
                return 12;
            if (evidence_digest("abc") != evidence_digest("abc"))
                return 13;
            if (evidence_digest("abc") == evidence_digest("abd"))
                return 14;
            return 0;
        }
        [[nodiscard]] ProposalRequest build_run_proposal()
        {
            return {
                .title = "Build and run reviewed project",
                .rationale = "Compile and launch only after exact operator approval.",
                .operations = {
                    {OperationKind::build,
                        "Build the reviewed workspace revision.",
                        "x64/Debug"},
                    {OperationKind::run,
                        "Run the reviewed project binary.",
                        "x64/Debug/EpochEditor.exe"}},
                .expires_at = {10'000u}};
        }

        [[nodiscard]] bool phase_order_contract()
        {
            DevelopmentController controller{configuration()};
            if (controller.snapshot().phase != ControllerPhase::ready)
                return false;
            if (controller.review("review.host", "Reviewed.", {1'100u}).code
                != ControllerCode::invalid_phase)
            {
                return false;
            }
            if (controller.approve(
                    "operator.primary", "Approved.", {1'100u}, {2'000u}).code
                != ControllerCode::invalid_phase)
            {
                return false;
            }
            if (controller.authorize({1'100u}, {100u}).code
                != ControllerCode::invalid_phase)
            {
                return false;
            }
            if (!controller.propose(build_run_proposal(), {1'100u}))
                return false;
            if (controller.review(
                    "review.host", "Backdated review.", {1'099u}).code
                != ControllerCode::invalid_request
                || controller.snapshot().phase != ControllerPhase::proposed)
            {
                return false;
            }

            const ControllerSnapshot proposed = controller.snapshot();
            if (proposed.phase != ControllerPhase::proposed
                || proposed.digest_hex.size() != 64u
                || proposed.operations.size() != 2u
                || !proposed.operator_approval_required
                || proposed.exact_operator_approval_recorded)
            {
                return false;
            }
            if (controller.authorize({1'150u}, {100u}).code
                != ControllerCode::invalid_phase)
            {
                return false;
            }
            if (!controller.review(
                    "review.host", "Paths and operations reviewed.", {1'200u}))
            {
                return false;
            }
            if (controller.authorize({1'250u}, {100u}).code
                != ControllerCode::invalid_phase)
            {
                return false;
            }
            if (controller.approve(
                    "operator.other", "Wrong operator.", {1'250u}, {2'000u}).code
                != ControllerCode::invalid_request)
            {
                return false;
            }
            if (!controller.approve(
                    "operator.primary", "Exact digest approved.", {1'300u}, {2'000u}))
            {
                return false;
            }
            if (!controller.snapshot().exact_operator_approval_recorded)
                return false;
            if (!controller.authorize({1'350u}, {100u}))
                return false;
            if (controller.authorize({1'351u}, {100u}).code
                != ControllerCode::invalid_phase)
            {
                return false;
            }
            return controller.snapshot().phase == ControllerPhase::authorized
                && controller.snapshot().single_use_permit_issued;
        }

        [[nodiscard]] bool completion_contract()
        {
            DevelopmentController controller{configuration(78u)};
            if (!controller.propose(build_run_proposal(), {1'100u})
                || !controller.review("review.host", "Reviewed.", {1'200u})
                || !controller.approve(
                    "operator.primary", "Approved.", {1'300u}, {2'000u})
                || !controller.authorize({1'350u}, {100u}))
            {
                return false;
            }

            CompletionRequest incomplete{
                .actor_id = "epoch.host",
                .outcome = CompletionOutcome::succeeded,
                .summary = "Build-only evidence is insufficient for build and run.",
                .evidence = {{
                    EvidenceKind::build_log,
                    "logs/build.log",
                    evidence_digest("verified build log 1"),
                    "Verified build log.",
                    true}}};
            if (controller.complete(std::move(incomplete), {1'400u}).code
                != ControllerCode::guard_rejected)
            {
                return false;
            }

            CompletionRequest complete{
                .actor_id = "epoch.host",
                .outcome = CompletionOutcome::succeeded,
                .summary = "Build and runtime evidence verified.",
                .evidence = {
                    {EvidenceKind::build_log,
                        "logs/build.log",
                        evidence_digest("verified build log 2"),
                        "Verified build log.",
                        true},
                    {EvidenceKind::runtime_report,
                        "logs/runtime.json",
                        evidence_digest("verified runtime report"),
                        "Verified bounded runtime report.",
                        true}}};
            if (!controller.complete(std::move(complete), {1'401u}))
                return false;
            const ControllerSnapshot snapshot = controller.snapshot();
            if (snapshot.phase != ControllerPhase::completed
                || !snapshot.permit_consumed
                || snapshot.evidence_record_count != 1u)
            {
                return false;
            }

            CompletionRequest duplicate{
                .actor_id = "epoch.host",
                .outcome = CompletionOutcome::failed,
                .summary = "A consumed permit cannot be reused.",
                .evidence = {{
                    EvidenceKind::diagnostic,
                    "logs/duplicate.json",
                    evidence_digest("duplicate attempt"),
                    "Duplicate attempt.",
                    true}}};
            return controller.complete(std::move(duplicate), {1'402u}).code
                == ControllerCode::invalid_phase;
        }

        [[nodiscard]] bool source_edit_and_cancel_contract()
        {
            DevelopmentController controller{configuration(79u)};
            ProposalRequest proposal{
                .title = "Apply reviewed source patch",
                .rationale = "Edit one engine file and one project file.",
                .operations = {
                    {.kind = OperationKind::engine_source_edit,
                        .summary = "Apply reviewed engine patch.",
                        .relative_path = "Engine/src/editor/editor.scene.cpp",
                        .before = {
                            .exists = true,
                            .digest = evidence_digest("engine-before"),
                            .byte_count = 13u},
                        .after = {
                            .exists = true,
                            .digest = evidence_digest("engine-after"),
                            .byte_count = 12u}},
                    {.kind = OperationKind::project_source_edit,
                        .summary = "Apply reviewed project patch.",
                        .relative_path = "Projects/example/main.cpp",
                        .before = {
                            .exists = true,
                            .digest = evidence_digest("project-before"),
                            .byte_count = 14u},
                        .after = {
                            .exists = true,
                            .digest = evidence_digest("project-after"),
                            .byte_count = 13u}}},
                .expires_at = {10'000u}};
            if (!controller.propose(std::move(proposal), {1'100u}))
                return false;
            const ControllerSnapshot snapshot = controller.snapshot();
            if (snapshot.operations.size() != 2u
                || !snapshot.operations[0u].writes_source
                || !snapshot.operations[1u].writes_source)
            {
                return false;
            }
            if (!controller.cancel(
                    "operator.primary", "Operator cancelled before review.", {1'200u}))
            {
                return false;
            }
            return controller.snapshot().phase == ControllerPhase::cancelled
                && !controller.snapshot().permit_consumed
                && controller.review("review.host", "Too late.", {1'300u}).code
                    == ControllerCode::invalid_phase;
        }

        [[nodiscard]] bool source_execution_boundary_contract()
        {
            SessionConfiguration config = configuration(81u);
            config.workspace_root = "relative-root-is-rejected";
            DevelopmentController controller{std::move(config)};
            ProposalRequest proposal{
                .title = "Apply one exact source postimage",
                .rationale = "The transaction must remain bound to reviewed bytes.",
                .operations = {{
                    .kind = OperationKind::engine_source_edit,
                    .summary = "Replace one reviewed engine source file.",
                    .relative_path = "Engine/src/ai/example.cpp",
                    .before = {
                        .exists = true,
                        .digest = evidence_digest("before"),
                        .byte_count = 6u},
                    .after = {
                        .exists = true,
                        .digest = evidence_digest("after"),
                        .byte_count = 5u}}},
                .expires_at = {10'000u}};
            if (!controller.propose(std::move(proposal), {1'100u})
                || !controller.review(
                    "review.host", "Exact bytes reviewed.", {1'200u})
                || !controller.approve(
                    "operator.primary",
                    "Exact digest approved.",
                    {1'300u},
                    {2'000u})
                || !controller.authorize({1'400u}, {200u}))
            {
                return false;
            }

            CompletionRequest forged_completion{
                .actor_id = "untrusted.model",
                .outcome = CompletionOutcome::succeeded,
                .summary = "Self-attested source completion.",
                .evidence = {{
                    EvidenceKind::patch,
                    "logs/forged.patch",
                    evidence_digest("forged patch"),
                    "Untrusted source evidence.",
                    true}}};
            if (controller.complete(
                    std::move(forged_completion), {1'450u}).code
                    != ControllerCode::invalid_request
                || controller.snapshot().phase
                    != ControllerPhase::authorized)
            {
                return false;
            }

            const SourceExecutionReport mismatch =
                controller.execute_authorized_source_changes({{
                    .stable_operation_id = 99u,
                    .relative_path = "Engine/src/ai/example.cpp",
                    .replacement_bytes = "after"}}, {1'500u});
            if (mismatch.controller_result.code
                    != ControllerCode::invalid_request
                || mismatch.attempted
                || controller.snapshot().phase
                    != ControllerPhase::authorized)
            {
                return false;
            }

            const SourceExecutionReport rejected =
                controller.execute_authorized_source_changes({{
                    .stable_operation_id = 1u,
                    .relative_path = "Engine/src/ai/example.cpp",
                    .replacement_bytes = "after"}}, {1'501u});
            const ControllerSnapshot snapshot = controller.snapshot();
            return rejected.attempted
                && !rejected.committed
                && rejected.transaction_code == "invalid_root"
                && rejected.evidence_digest.valid()
                && rejected.controller_result
                && snapshot.phase == ControllerPhase::failed
                && snapshot.permit_consumed
                && snapshot.evidence_record_count == 1u;
        }
        struct TemporaryWorkspace final
        {
            std::filesystem::path root{};

            ~TemporaryWorkspace()
            {
                std::error_code error{};
                std::filesystem::remove_all(root, error);
            }
        };

        int model_proposal_failure_detail{};
        [[nodiscard]] std::string model_source_packet(
            std::string_view area,
            std::string_view path,
            std::string_view replacementLine)
        {
            std::string packet =
                "EPOCH_SOURCE_PROPOSAL_V1\n"
                "title: Replace one reviewed source file\n"
                "rationale: Verify powerless model ingestion and host evidence\n"
                "lifetime_seconds: 900\n"
                "operation_count: 1\n"
                "begin_operation\n"
                "area: ";
            packet += area;
            packet += "\npath: ";
            packet += path;
            packet +=
                "\nsummary: Replace the exact reviewed source postimage\n"
                "final_newline: true\n"
                "begin_content\n|";
            packet += replacementLine;
            packet +=
                "\nend_content\n"
                "end_operation\n"
                "end_proposal\n";
            return packet;
        }

        [[nodiscard]] std::string model_exact_patch_packet(
            std::string_view path,
            std::string_view searchLine,
            std::string_view replacementLine)
        {
            std::string packet =
                "EPOCH_SOURCE_PATCH_PROPOSAL_V1\n"
                "title: Replace one exact reviewed source block\n"
                "rationale: Verify exact-block ingestion and host postimage reconstruction\n"
                "lifetime_seconds: 900\n"
                "operation_count: 1\n"
                "begin_operation\n"
                "area: engine\n"
                "path: ";
            packet += path;
            packet +=
                "\nsummary: Replace the exact reviewed source block\n"
                "search_final_newline: true\n"
                "begin_search\n|";
            packet += searchLine;
            packet +=
                "\nend_search\n"
                "replacement_final_newline: true\n"
                "begin_replacement\n|";
            packet += replacementLine;
            packet +=
                "\nend_replacement\n"
                "end_operation\n"
                "end_proposal\n";
            return packet;
        }

        [[nodiscard]] bool model_proposal_ingestion_contract()
        {
            model_proposal_failure_detail = 0;
            const auto unique = std::chrono::steady_clock::now()
                .time_since_epoch().count();
            TemporaryWorkspace workspace{
                std::filesystem::temp_directory_path()
                    / ("epoch_model_proposal_contract_"
                        + std::to_string(unique))};
            std::error_code error{};
            const std::filesystem::path sourceRoot = workspace.root / "source";
            const std::filesystem::path sandboxRoot = workspace.root / "sandbox";
            if (!std::filesystem::create_directories(
                    sourceRoot / "Engine/src/ai", error)
                || error)
            {
                return false;
            }
            if (!std::filesystem::create_directories(sandboxRoot, error)
                || error)
            {
                return false;
            }

            const std::filesystem::path source =
                sourceRoot / "Engine/src/ai/ai.example.cpp";
            {
                std::ofstream output{source, std::ios::binary};
                output << "before\n";
                if (!output)
                    return false;
            }

            SessionConfiguration config = configuration(82u);
            config.workspace_root = sandboxRoot.generic_string();
            config.source_snapshot_root = sourceRoot.generic_string();
            DevelopmentController controller{config};
            const std::string packet = model_source_packet(
                "engine",
                "Engine/src/ai/ai.example.cpp",
                "after");
            if (!controller.propose_model_reply(
                    packet,
                    OperationKind::engine_source_edit,
                    {1'100u}))
            {
                return false;
            }

            const ControllerSnapshot staged = controller.snapshot();
            const ContentState expectedBefore{
                .exists = true,
                .digest = evidence_digest("before\n"),
                .byte_count = 7u};
            const ContentState expectedAfter{
                .exists = true,
                .digest = evidence_digest("after\n"),
                .byte_count = 6u};
            if (staged.phase != ControllerPhase::proposed
                || staged.operations.size() != 1u
                || !staged.model_source_payloads_ready
                || staged.exact_operator_approval_recorded
                || staged.single_use_permit_issued
                || staged.evidence_record_count != 0u
                || staged.operations.front().before != expectedBefore
                || staged.operations.front().after != expectedAfter)
            {
                return false;
            }

            DevelopmentController exactBlock{config};
            if (!exactBlock.propose_model_reply(
                    model_exact_patch_packet(
                        "Engine/src/ai/ai.example.cpp",
                        "before",
                        "after"),
                    OperationKind::engine_source_edit,
                    {1'100u}))
            {
                return false;
            }
            const ControllerSnapshot exactStaged = exactBlock.snapshot();
            if (exactStaged.phase != ControllerPhase::proposed
                || exactStaged.operations.size() != 1u
                || exactStaged.operations.front().before != expectedBefore
                || exactStaged.operations.front().after != expectedAfter)
            {
                return false;
            }

            if (!controller.review(
                    "review.host", "Exact source bytes reviewed.", {1'200u})
                || !controller.approve(
                    "operator.primary",
                    "Approve this digest only.",
                    {1'300u},
                    {2'000u})
                || !controller.authorize({1'400u}, {200u}))
            {
                return false;
            }
            const SourceExecutionReport report =
                controller.execute_authorized_model_source_changes({1'450u});
            if (!report || !report.evidence_digest.valid()
                || controller.snapshot().phase != ControllerPhase::completed
                || controller.snapshot().model_source_payloads_ready)
            {
                return false;
            }

            std::ifstream liveInput{source, std::ios::binary};
            const std::string liveActual{
                std::istreambuf_iterator<char>{liveInput},
                std::istreambuf_iterator<char>{}};
            if (liveActual != "before\n")
                return false;

            std::ifstream sandboxInput{
                sandboxRoot / "Engine/src/ai/ai.example.cpp",
                std::ios::binary};
            const std::string sandboxActual{
                std::istreambuf_iterator<char>{sandboxInput},
                std::istreambuf_iterator<char>{}};
            liveInput.close();
            sandboxInput.close();
            if (sandboxActual != "after\n")
                return false;

            const std::vector<OperationSummary> reviewedOperations =
                staged.operations;
            if (!verify_source_promotion_candidate(
                    sourceRoot.generic_string(),
                    sandboxRoot.generic_string(),
                    reviewedOperations))
            {
                model_proposal_failure_detail = 1;
                return false;
            }
            {
                std::ofstream output{source, std::ios::binary | std::ios::trunc};
                output << "stale\n";
            }
            if (verify_source_promotion_candidate(
                    sourceRoot.generic_string(),
                    sandboxRoot.generic_string(),
                    reviewedOperations).code
                != PromotionCandidateCode::live_preimage_mismatch)
            {
                model_proposal_failure_detail = 2;
                return false;
            }
            {
                std::ofstream output{source, std::ios::binary | std::ios::trunc};
                output << "before\n";
            }
            {
                std::ofstream output{
                    sandboxRoot / "Engine/src/ai/ai.example.cpp",
                    std::ios::binary | std::ios::trunc};
                output << "tampered\n";
            }
            if (verify_source_promotion_candidate(
                    sourceRoot.generic_string(),
                    sandboxRoot.generic_string(),
                    reviewedOperations).code
                != PromotionCandidateCode::sandbox_postimage_mismatch)
            {
                model_proposal_failure_detail = 3;
                return false;
            }
            {
                std::ofstream output{
                    sandboxRoot / "Engine/src/ai/ai.example.cpp",
                    std::ios::binary | std::ios::trunc};
                output << "after\n";
            }

            SessionConfiguration promotionConfig = configuration(83u);
            promotionConfig.workspace_root = sourceRoot.generic_string();
            promotionConfig.source_snapshot_root = sourceRoot.generic_string();
            DevelopmentController promotion{promotionConfig};
            if (!promotion.propose_model_reply(
                    packet,
                    OperationKind::engine_source_edit,
                    {1'100u}))
            {
                model_proposal_failure_detail = 4;
                return false;
            }
            const ControllerSnapshot promotionSnapshot = promotion.snapshot();
            if (promotionSnapshot.operations != reviewedOperations)
            {
                model_proposal_failure_detail = 51;
                return false;
            }
            if (!promotion.review(
                    "review.host",
                    "Verified sandbox and live source digests reviewed.",
                    {1'200u}))
            {
                model_proposal_failure_detail = 52;
                return false;
            }
            if (!promotion.approve(
                    "operator.primary",
                    "Approve exact verified live promotion.",
                    {1'300u},
                    {2'000u}))
            {
                model_proposal_failure_detail = 53;
                return false;
            }
            if (!promotion.authorize({1'400u}, {200u}))
            {
                model_proposal_failure_detail = 54;
                return false;
            }
            if (!promotion.execute_authorized_model_source_changes({1'450u}))
            {
                model_proposal_failure_detail = 55;
                return false;
            }
            std::ifstream promotedInput{source, std::ios::binary};
            const std::string promotedActual{
                std::istreambuf_iterator<char>{promotedInput},
                std::istreambuf_iterator<char>{}};
            if (promotedActual != "after\n"
                || verify_source_promotion_candidate(
                    sourceRoot.generic_string(),
                    sandboxRoot.generic_string(),
                    reviewedOperations).code
                    != PromotionCandidateCode::live_preimage_mismatch)
            {
                model_proposal_failure_detail = 6;
                return false;
            }

            model_proposal_failure_detail = 40;
            DevelopmentController wrongArea{config};
            const ControllerResult rejected =
                wrongArea.propose_model_reply(
                    model_source_packet(
                        "project",
                        "Projects/demo/main.cpp",
                        "int main() {}"),
                    OperationKind::engine_source_edit,
                    {1'100u});
            if (rejected.code != ControllerCode::invalid_request
                || wrongArea.snapshot().phase != ControllerPhase::ready
                || wrongArea.snapshot().model_source_payloads_ready)
            {
                return false;
            }

            DevelopmentController legacyPath{config};
            const ControllerResult legacyRejected =
                legacyPath.propose_model_reply(
                    model_source_packet(
                        "engine",
                        "Engine/Source/Core/Logger.h",
                        "#pragma once"),
                    OperationKind::engine_source_edit,
                    {1'100u});
            DevelopmentController badName{config};
            const ControllerResult badNameRejected =
                badName.propose_model_reply(
                    model_source_packet(
                        "engine",
                        "Engine/src/ai/Logger.cpp",
                        "int value = 1;"),
                    OperationKind::engine_source_edit,
                    {1'100u});
            return legacyRejected.code == ControllerCode::invalid_request
                && legacyRejected.status.find("legacy capitalized")
                    != std::string::npos
                && badNameRejected.code == ControllerCode::invalid_request
                && badNameRejected.status.find("<owner>.<subject_role>")
                    != std::string::npos;
        }
        [[nodiscard]] bool invalid_configuration_contract()
        {
            SessionConfiguration invalid = configuration(80u);
            invalid.project_source_root = "../Projects";
            DevelopmentController controller{std::move(invalid)};
            return controller.snapshot().phase == ControllerPhase::unavailable
                && controller.propose(build_run_proposal(), {1'100u}).code
                    == ControllerCode::invalid_phase;
        }

        [[nodiscard]] int controller_contract_failure_code()
        {
            const int evidenceDigestFailure =
                evidence_digest_failure_code();
            if (evidenceDigestFailure != 0) return evidenceDigestFailure;
            if (!phase_order_contract()) return 2;
            if (!completion_contract()) return 3;
            if (!source_edit_and_cancel_contract()) return 4;
            if (!source_execution_boundary_contract()) return 5;
            if (!invalid_configuration_contract()) return 6;
            if (!model_proposal_ingestion_contract())
            {
                return model_proposal_failure_detail == 0
                    ? 7
                    : 70 + model_proposal_failure_detail;
            }
            return 0;
        }
    }

    bool run_contract()
    {
        return controller_contract_failure_code() == 0;
    }
}

#if defined(EPOCH_EDITOR_AI_DEVELOPMENT_CONTROLLER_CONTRACT_MAIN)
int main()
{
    return epochengine::editor_ai_development::
        controller_contract_failure_code();
}
#endif