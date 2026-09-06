/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <chrono>
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

module ai.iteration_session;

namespace epochengine::ai::iteration_session
{
    namespace
    {
        struct Fixture final
        {
            std::filesystem::path root{};
            Fixture() = default;
            explicit Fixture(std::filesystem::path path)
                : root{std::move(path)} {}
            Fixture(const Fixture&) = delete;
            Fixture& operator=(const Fixture&) = delete;
            Fixture(Fixture&& other) noexcept
                : root{std::exchange(other.root, {})} {}
            Fixture& operator=(Fixture&&) = delete;
            ~Fixture()
            {
                std::error_code ec{};
                if (!root.empty())
                    std::filesystem::remove_all(root, ec);
            }
        };

        [[nodiscard]] Fixture make_fixture()
        {
            Fixture fixture{
                std::filesystem::temp_directory_path()
                / ("epoch_iteration_session_"
                    + std::to_string(std::chrono::steady_clock::now()
                        .time_since_epoch().count()))};
            std::error_code ec{};
            std::filesystem::create_directories(fixture.root / "Engine/src/ai", ec);
            std::ofstream{fixture.root / "Engine.sln"} << "solution\n";
            std::ofstream{fixture.root / "Engine/src/ai/ai.contract.cpp"}
                << "namespace epochengine::ai { int contract = 1; }\n";
            return fixture;
        }

        [[nodiscard]] bool complete_flow_contract()
        {
            auto fixture = make_fixture();
            SourceAuthority source{
                .kind = SourceAuthorityKind::explicit_checkout,
                .root = std::filesystem::absolute(fixture.root),
                .source_version = "0.89.32",
                .commit = "0123456789012345678901234567890123456789",
                .receipt_digest = std::string(64u, 'a'),
                .verified = true};
            const auto inspected = inspect_curated_files(
                source, {"Engine/src/ai/ai.contract.cpp"});
            if (!inspected.accepted || inspected.files.size() != 1u)
                return false;

            std::vector<std::string> twelvePaths{};
            for (std::size_t index = 0u; index < kMaximumCuratedFiles; ++index)
            {
                const auto path = "Engine/src/ai/ai.context_"
                    + std::to_string(index) + ".cpp";
                std::ofstream{fixture.root / path, std::ios::binary}
                    << "namespace epochengine::ai { int context = 1; }\n";
                twelvePaths.push_back(path);
            }
            const auto twelve = inspect_curated_files(source, twelvePaths);
            IterationSession twelveSession{};
            if (!twelve.accepted || twelve.files.size() != kMaximumCuratedFiles
                || !twelveSession.configure(SessionConfiguration{
                    .objective = "Inspect a model-selected twelve-file source context.",
                    .model_name = "operator/future-agentic-model",
                    .source = source,
                    .curated_files = twelve.files,
                    .policy = CandidatePolicy::manual_each_candidate,
                    .maximum_repair_attempts = 2u}))
                return false;
            twelvePaths.push_back("Engine/src/ai/ai.contract.cpp");
            if (inspect_curated_files(source, twelvePaths).accepted)
                return false;

            IterationSession session{};
            auto forged = inspected.files;
            forged.front().byte_count += 1u;
            if (session.configure(SessionConfiguration{
                    .objective = "Reject forged curated evidence.",
                    .model_name = "Qwen3.8-27B",
                    .source = source,
                    .curated_files = forged,
                    .policy = CandidatePolicy::manual_each_candidate,
                    .maximum_repair_attempts = 3u}))
            {
                return false;
            }
            if (!session.configure(SessionConfiguration{
                    .objective = "Strengthen the bounded AI contract.",
                    .model_name = "operator/future|agentic\nmodel",
                    .source = source,
                    .curated_files = inspected.files,
                    .policy = CandidatePolicy::manual_each_candidate,
                    .maximum_repair_attempts = 3u})
                || !session.context_shared())
            {
                return false;
            }
            const RequestIdentity identity = session.identity();
            if (!session.stage_candidate(identity, "exact candidate bytes")
                || session.approve_candidate({identity.session_id, identity.request_id + 1u})
                || !session.approve_candidate(identity)
                || !session.record_implementation(
                    identity, std::string(64u, '1'), "Sandbox transaction committed."))
            {
                return false;
            }

            const CandidateReport implemented = session.report();
            if (session.record_validation(
                    identity,
                    ValidationEvidence{
                        .actor = ValidationActor::debug_compiler,
                        .candidate_digest = std::string(64u, 'f'),
                        .evidence_digest = std::string(64u, '2'),
                        .summary = "Mismatched candidate evidence.",
                        .passed = true},
                    false))
            {
                return false;
            }

            const std::vector<ValidationActor> actors{
                ValidationActor::debug_compiler,
                ValidationActor::debug_contract,
                ValidationActor::release_compiler,
                ValidationActor::release_contract,
                ValidationActor::headless_compiler,
                ValidationActor::headless_contract,
                ValidationActor::full_validation};
            for (std::size_t index = 0u; index < actors.size(); ++index)
            {
                if (!session.record_validation(
                        identity,
                        ValidationEvidence{
                            .actor = actors[index],
                            .candidate_digest = implemented.candidate_digest,
                            .evidence_digest = std::string(64u,
                                static_cast<char>('2' + index)),
                            .summary = index == 0u
                                ? "Trusted|validation\nactor passed."
                                : "Trusted validation actor passed.",
                            .passed = true},
                        index + 1u == actors.size()))
                {
                    return false;
                }
            }
            const CandidateReport report = session.report();
            return report.phase == SessionPhase::candidate_verified
                && !report.candidate_digest.empty()
                && report.validation.size() == 7u
                && report.resume_requires_revalidation
                && session.serialize_report().find(
                    "schema=epoch.ai.iteration.report.v1") != std::string::npos
                && report.model_name == "operator/future|agentic\nmodel"
                && session.serialize_report().find("model=operator/future%7cagentic%0amodel")
                    != std::string::npos
                && session.serialize_report().find(
                    "Trusted%7cvalidation%0aactor") != std::string::npos;
        }

        [[nodiscard]] bool same_candidate_state(
            const CandidateReport& left, const CandidateReport& right)
        {
            // Refusals may update visible status, never candidate authority.
            return left.identity == right.identity && left.phase == right.phase
                && left.proposal_digest == right.proposal_digest
                && left.candidate_digest == right.candidate_digest
                && left.scope_digest == right.scope_digest
                && left.objective_digest == right.objective_digest
                && left.candidate_approved == right.candidate_approved
                && left.repair_attempt == right.repair_attempt
                && left.validation.size() == right.validation.size()
                && std::equal(left.validation.begin(), left.validation.end(), right.validation.begin(),
                    [](const ValidationEvidence& a, const ValidationEvidence& b)
                    {
                        return a.actor == b.actor && a.candidate_digest == b.candidate_digest
                            && a.evidence_digest == b.evidence_digest && a.summary == b.summary
                            && a.passed == b.passed;
                    });
        }

        [[nodiscard]] bool repair_and_resume_contract(const bool fail_after_partial_success)
        {
            auto fixture = make_fixture();
            SourceAuthority source{
                .kind = SourceAuthorityKind::verified_cache,
                .root = std::filesystem::absolute(fixture.root),
                .source_version = "0.89.32",
                .commit = "abcdefabcdefabcdefabcdefabcdefabcdefabcd",
                .receipt_digest = std::string(64u, 'b'),
                .verified = true};
            const auto inspected = inspect_curated_files(
                source, {"Engine/src/ai/ai.contract.cpp"});
            IterationSession session{};
            if (!inspected.accepted
                || !session.configure(SessionConfiguration{
                    .objective = "Repair one bounded AI defect.",
                    .model_name = "Qwen3.8",
                    .source = source,
                    .curated_files = inspected.files,
                    .maximum_repair_attempts = 2u})
                || !session.context_shared())
            {
                return false;
            }
            const RequestIdentity first = session.identity();
            if (!session.stage_candidate(first, "candidate one")
                || !session.approve_candidate(first)
                || !session.record_implementation(
                    first, std::string(64u, 'c'), "Sandbox candidate one."))
            {
                return false;
            }
            constexpr std::array actors{
                ValidationActor::debug_compiler,
                ValidationActor::debug_contract,
                ValidationActor::release_compiler,
                ValidationActor::release_contract,
                ValidationActor::headless_compiler,
                ValidationActor::headless_contract,
                ValidationActor::full_validation};
            const std::size_t failureIndex = fail_after_partial_success ? actors.size() - 1u : 0u;
            for (std::size_t index = 0u; index <= failureIndex; ++index)
            {
                if (!session.record_validation(first,
                        ValidationEvidence{
                            .actor = actors[index],
                            .candidate_digest = std::string(64u, 'c'),
                            .evidence_digest = std::string(64u, 'd'),
                            .summary = "Candidate one validation receipt.",
                            .passed = index != failureIndex},
                        false))
                    return false;
            }
            const CandidateReport failedCandidate = session.report();
            const RequestIdentity repair = session.identity();
            if (failedCandidate.validation.size() != failureIndex + 1u
                || failedCandidate.validation.back().passed
                || repair == first || session.stage_candidate(first, "stale repair")
                || !same_candidate_state(failedCandidate, session.report())
                || session.stage_candidate(repair, {})
                || !same_candidate_state(failedCandidate, session.report())
                || !session.stage_candidate(repair, "candidate two")
                || !session.report().validation.empty()
                || !session.report().candidate_digest.empty()
                || !session.approve_candidate(repair))
            {
                return false;
            }

            const auto approvedRepair = session.report();
            if (session.record_implementation(first, std::string(64u, 'e'), "Stale implementation.")
                || !same_candidate_state(approvedRepair, session.report())
                || session.record_implementation(repair, "invalid", "Malformed implementation.")
                || !same_candidate_state(approvedRepair, session.report())
                || !session.record_implementation(repair, std::string(64u, 'e'), "Sandbox candidate two."))
                return false;

            const auto emptyRepair = session.report();
            ValidationEvidence receipt{
                .actor = ValidationActor::debug_compiler,
                .candidate_digest = std::string(64u, 'e'),
                .evidence_digest = std::string(64u, '2'),
                .summary = "Candidate two validation receipt.",
                .passed = true};
            if (session.record_validation(first, receipt, false)
                || !same_candidate_state(emptyRepair, session.report()))
                return false;
            receipt.candidate_digest = failedCandidate.candidate_digest;
            if (session.record_validation(repair, receipt, false)
                || !same_candidate_state(emptyRepair, session.report()))
                return false;
            receipt.candidate_digest = emptyRepair.candidate_digest;
            receipt.actor = static_cast<ValidationActor>(255u);
            if (session.record_validation(repair, receipt, false)
                || !same_candidate_state(emptyRepair, session.report()))
                return false;
            receipt.actor = ValidationActor::debug_compiler;
            if (session.record_validation(repair, receipt, true)
                || !same_candidate_state(emptyRepair, session.report()))
                return false;

            for (std::size_t index = 0u; index < actors.size(); ++index)
            {
                receipt.actor = actors[index];
                receipt.evidence_digest = std::string(64u, static_cast<char>('2' + index));
                if (!session.record_validation(repair, receipt, index + 1u == actors.size()))
                    return false;
                const auto accepted = session.report();
                if (accepted.validation.size() != index + 1u
                    || session.record_validation(repair, receipt, false)
                    || !same_candidate_state(accepted, session.report()))
                    return false;
                if (index == 0u)
                {
                    // Six passed actors from the failed predecessor cannot
                    // supplement this candidate's single current receipt.
                    receipt.actor = ValidationActor::full_validation;
                    if (session.record_validation(repair, receipt, true)
                        || !same_candidate_state(accepted, session.report()))
                        return false;
                }
            }

            CandidateReport report = session.report();
            if (report.phase != SessionPhase::candidate_verified || report.validation.size() != actors.size()
                || report.repair_attempt != 1u
                || !std::all_of(report.validation.begin(), report.validation.end(),
                    [&](const ValidationEvidence& item)
                    { return item.passed && item.candidate_digest == report.candidate_digest; })
                || failedCandidate.validation.size() != failureIndex + 1u)
                return false;
            IterationSession sameScope{};
            if (!sameScope.resume_scope_fail_closed(
                    report, source, inspected.files)
                || sameScope.identity() == report.identity
                || sameScope.report().objective != "Repair one bounded AI defect."
                || sameScope.report().objective_digest != report.objective_digest
                || sameScope.report().candidate_approved
                || !sameScope.report().validation.empty())
                return false;
            SourceAuthority changed = source;
            changed.commit = "ffffffffffffffffffffffffffffffffffffffff";
            IterationSession resumed{};
            return !resumed.resume_scope_fail_closed(
                report, changed, inspected.files)
                && resumed.report().phase == SessionPhase::blocked;
        }
    }

    bool run_contract()
    {
        return complete_flow_contract() && repair_and_resume_contract(false)
            && repair_and_resume_contract(true);
    }
}
