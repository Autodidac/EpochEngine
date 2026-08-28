/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <algorithm>
#include <array>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

module ai.iteration_session;

import core.sha256;

namespace epochengine::ai::iteration_session
{
    namespace
    {
        [[nodiscard]] std::string digest_text(const std::string_view text)
        {
            return core::sha256::hex(core::sha256::hash(text));
        }

        [[nodiscard]] iteration::EvidenceDigest loop_digest(
            const std::string_view hex)
        {
            const auto parsed = core::sha256::from_hex(hex);
            if (!parsed)
                return {};
            iteration::EvidenceDigest result{};
            for (std::size_t index = 0u; index < 8u; ++index)
            {
                result.high = (result.high << 8u) | parsed->bytes[index];
                result.low = (result.low << 8u) | parsed->bytes[index + 8u];
            }
            return result;
        }

        [[nodiscard]] bool path_is_within(
            const std::filesystem::path& root,
            const std::filesystem::path& candidate)
        {
            const auto relative = candidate.lexically_relative(root);
            return !relative.empty() && relative != "." && !relative.is_absolute()
                && std::none_of(relative.begin(), relative.end(),
                    [](const auto& component) { return component == ".."; });
        }

        [[nodiscard]] std::string hash_file(
            const std::filesystem::path& path,
            std::uint64_t& byte_count)
        {
            std::ifstream input{path, std::ios::binary};
            if (!input)
                return {};
            core::sha256::Hasher hasher{};
            std::array<char, 64u * 1024u> buffer{};
            byte_count = 0u;
            while (input)
            {
                input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
                const auto count = input.gcount();
                if (count > 0)
                {
                    hasher.update(std::string_view{
                        buffer.data(), static_cast<std::size_t>(count)});
                    byte_count += static_cast<std::uint64_t>(count);
                }
            }
            return input.eof() ? core::sha256::hex(hasher.finish()) : std::string{};
        }

        [[nodiscard]] bool valid_summary(const std::string_view summary)
        {
            return !summary.empty() && summary.size() <= 4096u;
        }

        [[nodiscard]] bool lowercase_hex(
            const std::string_view text,
            const std::size_t size)
        {
            return text.size() == size
                && std::all_of(text.begin(), text.end(), [](const char value)
                {
                    return (value >= '0' && value <= '9')
                        || (value >= 'a' && value <= 'f');
                });
        }

        [[nodiscard]] std::string escape_field(const std::string_view text)
        {
            constexpr char digits[] = "0123456789abcdef";
            std::string escaped{};
            escaped.reserve(std::min<std::size_t>(text.size() * 3u, 12288u));
            for (const unsigned char value : text)
            {
                const bool safe = (value >= 'a' && value <= 'z')
                    || (value >= 'A' && value <= 'Z')
                    || (value >= '0' && value <= '9')
                    || value == '.' || value == '_' || value == '-'
                    || value == '/' || value == ':' || value == '\\';
                if (safe)
                    escaped.push_back(static_cast<char>(value));
                else
                {
                    escaped.push_back('%');
                    escaped.push_back(digits[(value >> 4u) & 0x0fu]);
                    escaped.push_back(digits[value & 0x0fu]);
                }
            }
            return escaped;
        }

        [[nodiscard]] bool complete_validation_set(
            const std::vector<ValidationEvidence>& evidence)
        {
            std::set<ValidationActor> passed{};
            for (const auto& item : evidence)
            {
                if (item.passed)
                    passed.insert(item.actor);
            }
            return passed.size() == 7u;
        }

        [[nodiscard]] std::string source_kind_name(SourceAuthorityKind kind)
        {
            switch (kind)
            {
            case SourceAuthorityKind::explicit_checkout: return "explicit_checkout";
            case SourceAuthorityKind::verified_cache: return "verified_cache";
            case SourceAuthorityKind::unavailable: return "unavailable";
            }
            return "unavailable";
        }
    }

    CuratedInspection inspect_curated_files(
        const SourceAuthority& authority,
        const std::vector<std::string>& relative_paths)
    {
        CuratedInspection result{};
        if (!authority.verified || authority.root.empty()
            || !authority.root.is_absolute())
        {
            result.status = "Source inspection requires one verified absolute source authority.";
            return result;
        }
        if (relative_paths.empty() || relative_paths.size() > 6u)
        {
            result.status = "Curated source inspection requires one to six reviewed files.";
            return result;
        }

        std::error_code ec{};
        const auto root = std::filesystem::weakly_canonical(authority.root, ec);
        if (ec || root.empty())
        {
            result.status = "The verified source authority could not be canonicalized.";
            return result;
        }

        std::set<std::string> unique{};
        for (const std::string& relative_text : relative_paths)
        {
            const std::filesystem::path relative{relative_text};
            if (relative.empty() || relative.is_absolute()
                || std::any_of(relative.begin(), relative.end(),
                    [](const auto& component) { return component == ".."; }))
            {
                result.status = "A curated source path escaped the verified authority.";
                result.files.clear();
                return result;
            }
            const auto candidate = std::filesystem::weakly_canonical(root / relative, ec);
            if (ec || !path_is_within(root, candidate)
                || !std::filesystem::is_regular_file(candidate, ec)
                || std::filesystem::is_symlink(candidate, ec))
            {
                result.status = "A curated source path is missing, non-regular, or linked.";
                result.files.clear();
                return result;
            }
            const std::string normalized = relative.generic_string();
            if (!unique.insert(normalized).second)
            {
                result.status = "Curated source paths must be unique.";
                result.files.clear();
                return result;
            }
            std::uint64_t bytes{};
            const std::string sha = hash_file(candidate, bytes);
            if (sha.size() != 64u)
            {
                result.status = "A curated source file could not be hashed.";
                result.files.clear();
                return result;
            }
            result.files.push_back(CuratedFile{
                .relative_path = normalized,
                .sha256 = sha,
                .byte_count = bytes});
        }
        result.accepted = true;
        result.status = "Curated source files are bound to verified SHA-256 evidence.";
        return result;
    }

    SessionResult IterationSession::configure(SessionConfiguration configuration)
    {
        if (configuration.objective.empty() || configuration.objective.size() > 4096u
            || configuration.model_name.empty() || configuration.model_name.size() > 256u
            || !configuration.source.verified
            || configuration.source.kind == SourceAuthorityKind::unavailable
            || configuration.source.root.empty()
            || !configuration.source.root.is_absolute()
            || configuration.source.source_version.empty()
            || configuration.source.source_version.size() > 64u
            || !lowercase_hex(configuration.source.commit, 40u)
            || !lowercase_hex(configuration.source.receipt_digest, 64u)
            || configuration.curated_files.empty()
            || configuration.curated_files.size() > 6u
            || configuration.maximum_repair_attempts == 0u
            || configuration.maximum_repair_attempts > 10u)
        {
            return reject("Iteration session configuration is incomplete or outside policy bounds.");
        }
        std::vector<std::string> curated_paths{};
        curated_paths.reserve(configuration.curated_files.size());
        for (const auto& file : configuration.curated_files)
        {
            if (file.relative_path.empty() || !lowercase_hex(file.sha256, 64u))
                return reject("Iteration session requires SHA-256 evidence for every curated file.");
            curated_paths.push_back(file.relative_path);
        }
        const auto inspected = inspect_curated_files(
            configuration.source, curated_paths);
        if (!inspected.accepted || inspected.files != configuration.curated_files)
            return reject("Iteration session refused curated metadata not derived from verified live files.");

        static std::atomic<std::uint64_t> next_session_id{0u};
        std::uint64_t session_id = next_session_id.fetch_add(
            1u, std::memory_order_relaxed) + 1u;
        if (session_id == 0u)
            session_id = next_session_id.fetch_add(
                1u, std::memory_order_relaxed) + 1u;
        next_request_id_ = 1u;
        report_ = CandidateReport{
            .identity = {session_id, next_request_id_},
            .phase = SessionPhase::awaiting_context_share,
            .policy = configuration.policy,
            .source = std::move(configuration.source),
            .objective_digest = digest_text(configuration.objective),
            .model_name = std::move(configuration.model_name),
            .curated_files = std::move(configuration.curated_files),
            .status = "Awaiting explicit curated-context sharing."};

        std::string scope = report_.objective_digest + "\n"
            + report_.source.root.generic_string() + "\n"
            + report_.source.source_version + "\n" + report_.source.commit;
        for (const auto& file : report_.curated_files)
            scope += "\n" + file.relative_path + "\n" + file.sha256;
        report_.scope_digest = digest_text(scope);
        scope_digest_ = loop_digest(report_.scope_digest);
        const auto configured = loop_.configure(
            iteration::LoopPolicy{
                .risk = iteration::RiskClass::related_files,
                .scope_digest = scope_digest_,
                .maximum_repair_attempts = configuration.maximum_repair_attempts,
                .require_research = false,
                .require_static_analysis = false,
                .require_sanitizer = false,
                .require_local_self_review = false,
                .require_visual_validation = false},
            iteration::infer_model_capabilities(report_.model_name));
        if (!configured)
        {
            report_.phase = SessionPhase::blocked;
            report_.status = configured.status;
            return reject(configured.status);
        }
        const auto begun = loop_.begin();
        if (!begun)
        {
            report_.phase = SessionPhase::blocked;
            report_.status = begun.status;
            return reject(begun.status);
        }
        configured_ = true;
        compiler_milestone_recorded_ = false;
        return accept(report_.status);
    }

    SessionResult IterationSession::require_selection(std::string reason)
    {
        report_ = {};
        report_.phase = SessionPhase::selection_required;
        report_.status = reason.empty()
            ? "Host source curation requires a more specific objective or an explicit reviewed file selection. No source bytes were read or sent."
            : std::move(reason);
        configured_ = false;
        return reject(report_.status);
    }

    SessionResult IterationSession::context_shared()
    {
        if (!configured_ || report_.phase != SessionPhase::awaiting_context_share)
            return reject("Curated context sharing is stale or not expected by this session.");
        report_.phase = SessionPhase::awaiting_candidate;
        return accept("Curated SHA-256-bound source evidence was explicitly shared; awaiting one exact candidate.");
    }

    SessionResult IterationSession::stage_candidate(
        const RequestIdentity identity,
        const std::string_view proposal_bytes)
    {
        if (!identity_matches(identity)
            || (report_.phase != SessionPhase::awaiting_candidate
                && report_.phase != SessionPhase::awaiting_repair)
            || proposal_bytes.empty() || proposal_bytes.size() > 1024u * 1024u)
        {
            return reject("Candidate staging rejected stale identity, phase, or unbounded bytes.");
        }
        report_.proposal_digest = digest_text(proposal_bytes);
        proposal_digest_ = loop_digest(report_.proposal_digest);
        report_.candidate_digest.clear();
        report_.candidate_approved = false;
        report_.phase = SessionPhase::awaiting_candidate_approval;
        return accept("Exact candidate digest staged for manual operator review.");
    }

    SessionResult IterationSession::record_loop_model_milestone(
        const iteration::Milestone milestone,
        const iteration::EvidenceDigest digest,
        const iteration::EvidenceDigest authority,
        std::string summary)
    {
        const auto action = loop_.next_action();
        if (action.milestone != milestone)
            return reject("Iteration milestone order diverged from the guarded session.");
        const auto recorded = loop_.record(iteration::MilestoneEvidence{
            .milestone = milestone,
            .actor = iteration::EvidenceActor::local_model,
            .digest = digest,
            .authority_digest = authority,
            .dispatch_id = action.dispatch_id,
            .summary = std::move(summary),
            .verified = true,
            .passed = true,
            .operator_reviewed = true});
        return recorded ? accept(recorded.status) : reject(recorded.status);
    }

    SessionResult IterationSession::record_loop_host_milestone(
        const iteration::Milestone milestone,
        const iteration::EvidenceActor actor,
        const iteration::EvidenceDigest digest,
        std::string summary,
        const bool passed)
    {
        const auto action = loop_.next_action();
        if (action.milestone != milestone || action.actor != actor)
            return reject("Trusted evidence actor or milestone did not match the active dispatch.");
        const auto recorded = loop_.record(iteration::MilestoneEvidence{
            .milestone = milestone,
            .actor = actor,
            .digest = digest,
            .authority_digest = proposal_digest_,
            .dispatch_id = action.dispatch_id,
            .summary = std::move(summary),
            .verified = true,
            .passed = passed,
            .operator_reviewed = actor == iteration::EvidenceActor::operator_user});
        return recorded ? accept(recorded.status) : reject(recorded.status);
    }

    SessionResult IterationSession::approve_candidate(const RequestIdentity identity)
    {
        if (!identity_matches(identity)
            || report_.phase != SessionPhase::awaiting_candidate_approval
            || !proposal_digest_.valid())
        {
            return reject("Candidate approval rejected stale identity, phase, or digest.");
        }
        if (loop_.snapshot().milestone == iteration::Milestone::inspect_architecture)
        {
            if (!record_loop_model_milestone(iteration::Milestone::inspect_architecture,
                    proposal_digest_, scope_digest_, "Operator reviewed model inspection against curated architecture evidence."))
                return {false, report_.status};
            if (!record_loop_model_milestone(iteration::Milestone::state_invariants,
                    proposal_digest_, scope_digest_, "Operator reviewed candidate invariants against exact curated file hashes."))
                return {false, report_.status};
        }
        if (!record_loop_model_milestone(iteration::Milestone::source_proposal,
                proposal_digest_, proposal_digest_, "Operator reviewed one exact-content bounded source proposal."))
            return {false, report_.status};
        const auto approval = record_loop_host_milestone(
            iteration::Milestone::operator_approval,
            iteration::EvidenceActor::operator_user,
            proposal_digest_,
            "Operator approved this digest for disposable sandbox execution only.",
            true);
        if (!approval)
            return approval;
        report_.candidate_approved = true;
        report_.phase = SessionPhase::executing_candidate;
        return accept("Candidate approved for sandbox execution; live source remains read-only.");
    }

    SessionResult IterationSession::record_implementation(
        const RequestIdentity identity,
        std::string evidence_digest,
        std::string summary)
    {
        if (!identity_matches(identity) || report_.phase != SessionPhase::executing_candidate
            || evidence_digest.size() != 64u || !valid_summary(summary))
            return reject("Sandbox implementation evidence is stale or malformed.");
        const auto digest = loop_digest(evidence_digest);
        if (!digest.valid())
            return reject("Sandbox implementation evidence digest is invalid.");
        const auto recorded = record_loop_host_milestone(
            iteration::Milestone::implement,
            iteration::EvidenceActor::source_executor,
            digest,
            std::move(summary),
            true);
        if (!recorded)
            return recorded;
        report_.candidate_digest = std::move(evidence_digest);
        report_.phase = SessionPhase::validating_candidate;
        compiler_milestone_recorded_ = false;
        return accept("Sandbox transaction is digest-bound; trusted validation actors may run.");
    }

    SessionResult IterationSession::record_validation(
        const RequestIdentity identity,
        ValidationEvidence evidence,
        const bool validation_sequence_complete)
    {
        if (!identity_matches(identity) || report_.phase != SessionPhase::validating_candidate
            || evidence.candidate_digest != report_.candidate_digest
            || !lowercase_hex(evidence.evidence_digest, 64u)
            || !valid_summary(evidence.summary))
            return reject("Validation evidence is stale, malformed, or outside the active candidate.");
        const auto digest = loop_digest(evidence.evidence_digest);
        if (!digest.valid())
            return reject("Validation evidence digest is invalid.");
        report_.validation.push_back(evidence);

        if (!evidence.passed)
        {
            const auto milestone = compiler_milestone_recorded_
                ? iteration::Milestone::test : iteration::Milestone::compile;
            const auto actor = compiler_milestone_recorded_
                ? iteration::EvidenceActor::test_runner
                : iteration::EvidenceActor::compiler;
            const auto failed = record_loop_host_milestone(
                milestone, actor, digest, evidence.summary, false);
            if (!failed)
                return failed;
            report_.repair_attempt = loop_.snapshot().repair_attempts;
            report_.phase = SessionPhase::awaiting_repair;
            report_.candidate_approved = false;
            ++next_request_id_;
            report_.identity.request_id = next_request_id_;
            return accept("Trusted validation failed; one fresh digest-bound repair request is required.");
        }

        if (!compiler_milestone_recorded_
            && evidence.actor == ValidationActor::debug_compiler)
        {
            const auto compiled = record_loop_host_milestone(
                iteration::Milestone::compile,
                iteration::EvidenceActor::compiler,
                digest,
                evidence.summary,
                true);
            if (!compiled)
                return compiled;
            compiler_milestone_recorded_ = true;
        }

        if (validation_sequence_complete)
        {
            if (!compiler_milestone_recorded_ || !complete_validation_set(report_.validation))
                return reject("Candidate verification requires all seven trusted validation actors.");
            const auto tested = record_loop_host_milestone(
                iteration::Milestone::test,
                iteration::EvidenceActor::test_runner,
                digest,
                "Debug, Release, HeadlessCI, and explicit full validation passed for the exact sandbox candidate.",
                true);
            if (!tested || !loop_.snapshot().complete)
                return reject("The bounded iteration loop did not accept the complete validation set.");
            report_.phase = SessionPhase::candidate_verified;
            report_.resume_requires_revalidation = true;
            return accept("Exact candidate passed the bounded validation session; manual live promotion remains separate.");
        }
        return accept("Trusted validation evidence recorded for the active candidate.");
    }

    SessionResult IterationSession::cancel(
        const RequestIdentity identity,
        std::string reason)
    {
        if (!identity_matches(identity) || !valid_summary(reason))
            return reject("Cancellation rejected stale identity or invalid reason.");
        const auto cancelled = loop_.cancel(reason);
        report_.phase = SessionPhase::cancelled;
        report_.status = cancelled.status;
        return {true, report_.status};
    }

    SessionResult IterationSession::resume_scope_fail_closed(
        const CandidateReport& report,
        const SourceAuthority& current_source,
        std::vector<CuratedFile> current_files)
    {
        if (!report.identity.valid() || !current_source.verified
            || report.source.root != current_source.root
            || report.source.source_version != current_source.source_version
            || report.source.commit != current_source.commit
            || report.curated_files != current_files)
        {
            report_ = report;
            report_.phase = SessionPhase::blocked;
            report_.candidate_approved = false;
            report_.status = "Resume refused because source authority or curated file hashes changed.";
            configured_ = false;
            return {false, report_.status};
        }
        SessionConfiguration configuration{
            .objective = "resumed-scope:" + report.objective_digest,
            .model_name = report.model_name,
            .source = current_source,
            .curated_files = std::move(current_files),
            .policy = report.policy,
            .maximum_repair_attempts = 3u};
        const auto resumed = configure(std::move(configuration));
        if (!resumed)
            return resumed;
        report_.status = "Scope resumed fail-closed; candidate, approvals, permits, and validation must be recreated.";
        report_.resume_requires_revalidation = true;
        return {true, report_.status};
    }

    RequestIdentity IterationSession::identity() const noexcept
    {
        return report_.identity;
    }

    CandidateReport IterationSession::report() const
    {
        CandidateReport result = report_;
        result.repair_attempt = loop_.snapshot().repair_attempts;
        return result;
    }

    std::string IterationSession::serialize_report() const
    {
        std::ostringstream out{};
        out << "schema=epoch.ai.iteration.report.v1\n"
            << "session_id=" << report_.identity.session_id << '\n'
            << "request_id=" << report_.identity.request_id << '\n'
            << "phase=" << static_cast<unsigned>(report_.phase) << '\n'
            << "policy=" << static_cast<unsigned>(report_.policy) << '\n'
            << "source_kind=" << source_kind_name(report_.source.kind) << '\n'
            << "source_root=" << escape_field(report_.source.root.generic_string()) << '\n'
            << "source_version=" << escape_field(report_.source.source_version) << '\n'
            << "source_commit=" << escape_field(report_.source.commit) << '\n'
            << "source_receipt=" << escape_field(report_.source.receipt_digest) << '\n'
            << "objective_sha256=" << report_.objective_digest << '\n'
            << "scope_sha256=" << report_.scope_digest << '\n'
            << "proposal_sha256=" << report_.proposal_digest << '\n'
            << "candidate_sha256=" << report_.candidate_digest << '\n'
            << "model=" << escape_field(report_.model_name) << '\n'
            << "repair_attempt=" << loop_.snapshot().repair_attempts << '\n'
            << "candidate_approved=" << (report_.candidate_approved ? 1 : 0) << '\n'
            << "resume_requires_revalidation=1\n";
        for (const auto& file : report_.curated_files)
            out << "file=" << escape_field(file.relative_path) << '|' << file.sha256 << '|'
                << file.byte_count << '\n';
        for (const auto& evidence : report_.validation)
            out << "validation=" << static_cast<unsigned>(evidence.actor) << '|'
                << (evidence.passed ? 1 : 0) << '|'
                << evidence.candidate_digest << '|'
                << evidence.evidence_digest << '|'
                << escape_field(evidence.summary) << '\n';
        out << "status=" << escape_field(report_.status) << '\n';
        return out.str();
    }

    bool IterationSession::identity_matches(const RequestIdentity identity) const noexcept
    {
        return configured_ && identity == report_.identity;
    }

    SessionResult IterationSession::reject(std::string status)
    {
        report_.status = std::move(status);
        return {false, report_.status};
    }

    SessionResult IterationSession::accept(std::string status)
    {
        report_.status = std::move(status);
        return {true, report_.status};
    }
}
