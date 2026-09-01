/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <algorithm>
#include <atomic>
#include <charconv>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <limits>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

module ai.iteration_campaign;

import core.sha256;
import platform.filesystem;

namespace epochengine::ai::iteration_campaign
{
    namespace
    {
        constexpr std::size_t kMaximumReportBytes = 512u * 1024u;
        constexpr std::uint64_t kMaximumCampaignSeconds = 24u * 60u * 60u;

        [[nodiscard]] std::string digest_text(const std::string_view text)
        {
            return core::sha256::hex(core::sha256::hash(text));
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

        [[nodiscard]] bool safe_identifier(const std::string_view text)
        {
            return !text.empty() && text.size() <= 128u
                && std::all_of(text.begin(), text.end(), [](const char value)
                {
                    return (value >= 'a' && value <= 'z')
                        || (value >= 'A' && value <= 'Z')
                        || (value >= '0' && value <= '9')
                        || value == '.' || value == '_' || value == '-';
                });
        }

        [[nodiscard]] bool valid_authority(
            const iteration_session::SourceAuthority& source)
        {
            using iteration_session::IterationTargetKind;
            using iteration_session::SourceAuthorityKind;
            if (!source.verified || source.root.empty() || !source.root.is_absolute())
                return false;
            if (source.target_kind == IterationTargetKind::engine_source)
            {
                return (source.kind == SourceAuthorityKind::explicit_checkout
                        || source.kind == SourceAuthorityKind::verified_cache)
                    && !source.source_version.empty()
                    && source.source_version.size() <= 64u
                    && lowercase_hex(source.commit, 40u)
                    && lowercase_hex(source.receipt_digest, 64u)
                    && source.project_id.empty()
                    && source.project_manifest_digest.empty()
                    && source.project_profile_digest.empty();
            }
            return source.kind == SourceAuthorityKind::verified_project
                && source.source_version.empty() && source.commit.empty()
                && source.receipt_digest.empty()
                && safe_identifier(source.project_id)
                && lowercase_hex(source.project_manifest_digest, 64u)
                && lowercase_hex(source.project_profile_digest, 64u);
        }

        [[nodiscard]] std::string authority_fingerprint(
            const iteration_session::SourceAuthority& source)
        {
            std::ostringstream out{};
            out << static_cast<unsigned>(source.target_kind) << '\n'
                << static_cast<unsigned>(source.kind) << '\n'
                << source.root.generic_string() << '\n'
                << source.source_version << '\n' << source.commit << '\n'
                << source.receipt_digest << '\n' << source.project_id << '\n'
                << source.project_manifest_digest << '\n'
                << source.project_profile_digest;
            return out.str();
        }

        [[nodiscard]] std::string target_kind_name(
            const iteration_session::IterationTargetKind kind)
        {
            return kind == iteration_session::IterationTargetKind::engine_source
                ? "engine" : "project";
        }

        [[nodiscard]] std::string escape_field(const std::string_view text)
        {
            constexpr char digits[] = "0123456789abcdef";
            std::string escaped{};
            escaped.reserve(text.size());
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

        [[nodiscard]] int hex_nibble(const char value) noexcept
        {
            if (value >= '0' && value <= '9')
                return value - '0';
            if (value >= 'a' && value <= 'f')
                return value - 'a' + 10;
            return -1;
        }

        [[nodiscard]] bool unescape_field(
            const std::string_view text,
            std::string& result)
        {
            result.clear();
            result.reserve(text.size());
            for (std::size_t index = 0u; index < text.size(); ++index)
            {
                if (text[index] != '%')
                {
                    if (text[index] == '\r' || text[index] == '\n')
                        return false;
                    result.push_back(text[index]);
                    continue;
                }
                if (index + 2u >= text.size())
                    return false;
                const int high = hex_nibble(text[index + 1u]);
                const int low = hex_nibble(text[index + 2u]);
                if (high < 0 || low < 0)
                    return false;
                result.push_back(static_cast<char>((high << 4) | low));
                index += 2u;
            }
            return result.find('\0') == std::string::npos;
        }

        template <typename Integer>
        [[nodiscard]] bool parse_unsigned(
            const std::string_view text,
            Integer& result)
        {
            if (text.empty())
                return false;
            Integer parsed{};
            const auto [end, error] = std::from_chars(
                text.data(), text.data() + text.size(), parsed);
            if (error != std::errc{} || end != text.data() + text.size())
                return false;
            result = parsed;
            return std::to_string(result) == text;
        }

        [[nodiscard]] bool valid_budgets(const CampaignBudgets& value)
        {
            const CampaignBudgets engine_max{};
            return value.maximum_candidates > 0u
                && value.maximum_candidates <= engine_max.maximum_candidates
                && value.maximum_model_calls > 0u
                && value.maximum_model_calls <= engine_max.maximum_model_calls
                && value.maximum_source_operations > 0u
                && value.maximum_source_operations <= engine_max.maximum_source_operations
                && value.maximum_repairs_per_candidate > 0u
                && value.maximum_repairs_per_candidate <= engine_max.maximum_repairs_per_candidate
                && value.maximum_changes_per_candidate > 0u
                && value.maximum_changes_per_candidate <= engine_max.maximum_changes_per_candidate
                && value.maximum_curated_files > 0u
                && value.maximum_curated_files <= engine_max.maximum_curated_files
                && value.maximum_context_bytes > 0u
                && value.maximum_context_bytes <= engine_max.maximum_context_bytes
                && value.maximum_candidate_bytes > 0u
                && value.maximum_candidate_bytes <= engine_max.maximum_candidate_bytes
                && value.maximum_file_bytes > 0u
                && value.maximum_file_bytes <= engine_max.maximum_file_bytes
                && value.maximum_transaction_bytes > 0u
                && value.maximum_transaction_bytes <= engine_max.maximum_transaction_bytes
                && value.maximum_workspace_files > 0u
                && value.maximum_workspace_files <= engine_max.maximum_workspace_files
                && value.maximum_workspace_bytes > 0u
                && value.maximum_workspace_bytes <= engine_max.maximum_workspace_bytes
                && value.maximum_validation_records > 0u
                && value.maximum_validation_records <= engine_max.maximum_validation_records;
        }

        [[nodiscard]] bool valid_report(const CampaignReport& report)
        {
            if (report.record_generation == 0u
                || (!report.previous_state_digest.empty()
                    && !lowercase_hex(report.previous_state_digest, 64u))
                || !lowercase_hex(report.campaign_id, 64u)
                || !lowercase_hex(report.target_key, 64u)
                || !valid_authority(report.session.source)
                || report.target_key != digest_text(authority_fingerprint(report.session.source))
                || report.created_at_unix_seconds == 0u
                || report.expires_at_unix_seconds <= report.created_at_unix_seconds
                || report.expires_at_unix_seconds - report.created_at_unix_seconds
                    > kMaximumCampaignSeconds
                || !valid_budgets(report.budgets)
                || !report.session.identity.valid()
                || report.session.objective.empty()
                || report.session.objective.size() > 4096u
                || report.session.objective_digest
                    != digest_text(report.session.objective)
                || report.session.model_name.empty()
                || report.session.model_name.size() > 256u
                || report.session.curated_files.empty()
                || report.session.curated_files.size()
                    > report.budgets.maximum_curated_files
                || report.session.validation.size()
                    > report.budgets.maximum_validation_records
                || report.counters.candidates > report.budgets.maximum_candidates
                || report.counters.model_calls > report.budgets.maximum_model_calls
                || report.counters.source_operations
                    > report.budgets.maximum_source_operations
                || report.counters.validation_records
                    > report.budgets.maximum_validation_records
                || report.status.size() > 4096u)
                return false;

            std::uint64_t context_bytes{};
            for (const auto& file : report.session.curated_files)
            {
                if (file.relative_path.empty() || file.relative_path.size() > 512u
                    || !lowercase_hex(file.sha256, 64u)
                    || file.byte_count > report.budgets.maximum_file_bytes
                    || file.byte_count > report.budgets.maximum_context_bytes
                    || context_bytes > report.budgets.maximum_context_bytes - file.byte_count)
                    return false;
                context_bytes += file.byte_count;
            }
            return true;
        }

        [[nodiscard]] std::string serialize_payload(const CampaignReport& report)
        {
            const auto& source = report.session.source;
            std::ostringstream out{};
            out << "schema=epoch.ai.iteration.campaign.v1\n"
                << "record_generation=" << report.record_generation << '\n'
                << "previous_state_sha256=" << report.previous_state_digest << '\n'
                << "campaign_id=" << report.campaign_id << '\n'
                << "target_key=" << report.target_key << '\n'
                << "created_at=" << report.created_at_unix_seconds << '\n'
                << "expires_at=" << report.expires_at_unix_seconds << '\n'
                << "budget_candidates=" << report.budgets.maximum_candidates << '\n'
                << "budget_model_calls=" << report.budgets.maximum_model_calls << '\n'
                << "budget_source_operations=" << report.budgets.maximum_source_operations << '\n'
                << "budget_repairs=" << report.budgets.maximum_repairs_per_candidate << '\n'
                << "budget_changes=" << report.budgets.maximum_changes_per_candidate << '\n'
                << "budget_curated_files=" << report.budgets.maximum_curated_files << '\n'
                << "budget_context_bytes=" << report.budgets.maximum_context_bytes << '\n'
                << "budget_candidate_bytes=" << report.budgets.maximum_candidate_bytes << '\n'
                << "budget_file_bytes=" << report.budgets.maximum_file_bytes << '\n'
                << "budget_transaction_bytes=" << report.budgets.maximum_transaction_bytes << '\n'
                << "budget_workspace_files=" << report.budgets.maximum_workspace_files << '\n'
                << "budget_workspace_bytes=" << report.budgets.maximum_workspace_bytes << '\n'
                << "budget_validation_records=" << report.budgets.maximum_validation_records << '\n'
                << "count_candidates=" << report.counters.candidates << '\n'
                << "count_model_calls=" << report.counters.model_calls << '\n'
                << "count_source_operations=" << report.counters.source_operations << '\n'
                << "count_validation_records=" << report.counters.validation_records << '\n'
                << "target_kind=" << static_cast<unsigned>(source.target_kind) << '\n'
                << "source_kind=" << static_cast<unsigned>(source.kind) << '\n'
                << "target_root=" << escape_field(source.root.generic_string()) << '\n'
                << "source_version=" << escape_field(source.source_version) << '\n'
                << "source_commit=" << source.commit << '\n'
                << "source_receipt=" << source.receipt_digest << '\n'
                << "project_id=" << escape_field(source.project_id) << '\n'
                << "project_manifest_sha256=" << source.project_manifest_digest << '\n'
                << "project_profile_sha256=" << source.project_profile_digest << '\n'
                << "objective=" << escape_field(report.session.objective) << '\n'
                << "objective_sha256=" << report.session.objective_digest << '\n'
                << "model=" << escape_field(report.session.model_name) << '\n'
                << "policy=" << static_cast<unsigned>(report.session.policy) << '\n'
                << "session_id=" << report.session.identity.session_id << '\n'
                << "request_id=" << report.session.identity.request_id << '\n'
                << "session_phase=" << static_cast<unsigned>(report.session.phase) << '\n'
                << "scope_sha256=" << report.session.scope_digest << '\n'
                << "proposal_sha256=" << report.session.proposal_digest << '\n'
                << "candidate_sha256=" << report.session.candidate_digest << '\n'
                << "repair_attempt=" << report.session.repair_attempt << '\n'
                << "candidate_approved=" << (report.session.candidate_approved ? 1 : 0) << '\n'
                << "resume_requires_revalidation="
                << (report.session.resume_requires_revalidation ? 1 : 0) << '\n'
                << "session_status=" << escape_field(report.session.status) << '\n'
                << "campaign_status=" << escape_field(report.status) << '\n'
                << "cancelled=" << (report.cancelled ? 1 : 0) << '\n'
                << "file_count=" << report.session.curated_files.size() << '\n';
            for (const auto& file : report.session.curated_files)
                out << "file=" << escape_field(file.relative_path) << '|'
                    << file.sha256 << '|' << file.byte_count << '\n';
            out << "validation_count=" << report.session.validation.size() << '\n';
            for (const auto& evidence : report.session.validation)
                out << "validation=" << static_cast<unsigned>(evidence.actor) << '|'
                    << (evidence.passed ? 1 : 0) << '|'
                    << evidence.candidate_digest << '|' << evidence.evidence_digest
                    << '|' << escape_field(evidence.summary) << '\n';
            return out.str();
        }

        class PayloadReader final
        {
        public:
            explicit PayloadReader(const std::string_view payload)
                : payload_{payload} {}

            [[nodiscard]] bool take(
                const std::string_view key,
                std::string_view& value)
            {
                if (offset_ >= payload_.size())
                    return false;
                const std::size_t end = payload_.find('\n', offset_);
                if (end == std::string_view::npos)
                    return false;
                const std::string_view line = payload_.substr(offset_, end - offset_);
                const std::string prefix = std::string{key} + '=';
                if (!line.starts_with(prefix))
                    return false;
                value = line.substr(prefix.size());
                offset_ = end + 1u;
                return true;
            }

            [[nodiscard]] bool complete() const noexcept
            {
                return offset_ == payload_.size();
            }

        private:
            std::string_view payload_{};
            std::size_t offset_{};
        };

        [[nodiscard]] bool parse_bool(const std::string_view text, bool& value)
        {
            if (text == "0")
            {
                value = false;
                return true;
            }
            if (text == "1")
            {
                value = true;
                return true;
            }
            return false;
        }

        [[nodiscard]] bool split_fields(
            const std::string_view text,
            const std::size_t expected,
            std::vector<std::string_view>& fields)
        {
            fields.clear();
            std::size_t offset{};
            while (offset <= text.size())
            {
                const std::size_t end = text.find('|', offset);
                fields.push_back(text.substr(offset,
                    end == std::string_view::npos ? text.size() - offset : end - offset));
                if (end == std::string_view::npos)
                    break;
                offset = end + 1u;
            }
            return fields.size() == expected;
        }

        [[nodiscard]] bool parse_payload(
            const std::string_view payload,
            CampaignReport& report)
        {
            PayloadReader reader{payload};
            std::string_view value{};
            auto raw = [&](const std::string_view key, std::string& destination)
            {
                if (!reader.take(key, value))
                    return false;
                destination.assign(value);
                return true;
            };
            auto escaped = [&](const std::string_view key, std::string& destination)
            {
                return reader.take(key, value) && unescape_field(value, destination);
            };
            auto number = [&](const std::string_view key, auto& destination)
            {
                return reader.take(key, value) && parse_unsigned(value, destination);
            };

            std::string schema{};
            unsigned target_kind{};
            unsigned source_kind{};
            unsigned policy{};
            unsigned phase{};
            bool candidate_approved{};
            bool resume_revalidation{};
            if (!raw("schema", schema) || schema != "epoch.ai.iteration.campaign.v1"
                || !number("record_generation", report.record_generation)
                || !raw("previous_state_sha256", report.previous_state_digest)
                || !raw("campaign_id", report.campaign_id)
                || !raw("target_key", report.target_key)
                || !number("created_at", report.created_at_unix_seconds)
                || !number("expires_at", report.expires_at_unix_seconds)
                || !number("budget_candidates", report.budgets.maximum_candidates)
                || !number("budget_model_calls", report.budgets.maximum_model_calls)
                || !number("budget_source_operations", report.budgets.maximum_source_operations)
                || !number("budget_repairs", report.budgets.maximum_repairs_per_candidate)
                || !number("budget_changes", report.budgets.maximum_changes_per_candidate)
                || !number("budget_curated_files", report.budgets.maximum_curated_files)
                || !number("budget_context_bytes", report.budgets.maximum_context_bytes)
                || !number("budget_candidate_bytes", report.budgets.maximum_candidate_bytes)
                || !number("budget_file_bytes", report.budgets.maximum_file_bytes)
                || !number("budget_transaction_bytes", report.budgets.maximum_transaction_bytes)
                || !number("budget_workspace_files", report.budgets.maximum_workspace_files)
                || !number("budget_workspace_bytes", report.budgets.maximum_workspace_bytes)
                || !number("budget_validation_records", report.budgets.maximum_validation_records)
                || !number("count_candidates", report.counters.candidates)
                || !number("count_model_calls", report.counters.model_calls)
                || !number("count_source_operations", report.counters.source_operations)
                || !number("count_validation_records", report.counters.validation_records)
                || !number("target_kind", target_kind) || target_kind > 1u
                || !number("source_kind", source_kind) || source_kind > 3u)
                return false;
            report.session.source.target_kind =
                static_cast<iteration_session::IterationTargetKind>(target_kind);
            report.session.source.kind =
                static_cast<iteration_session::SourceAuthorityKind>(source_kind);
            report.session.source.verified = true;
            std::string root{};
            if (!escaped("target_root", root)
                || !escaped("source_version", report.session.source.source_version)
                || !raw("source_commit", report.session.source.commit)
                || !raw("source_receipt", report.session.source.receipt_digest)
                || !escaped("project_id", report.session.source.project_id)
                || !raw("project_manifest_sha256", report.session.source.project_manifest_digest)
                || !raw("project_profile_sha256", report.session.source.project_profile_digest)
                || !escaped("objective", report.session.objective)
                || !raw("objective_sha256", report.session.objective_digest)
                || !escaped("model", report.session.model_name)
                || !number("policy", policy) || policy > 1u
                || !number("session_id", report.session.identity.session_id)
                || !number("request_id", report.session.identity.request_id)
                || !number("session_phase", phase) || phase > 10u
                || !raw("scope_sha256", report.session.scope_digest)
                || !raw("proposal_sha256", report.session.proposal_digest)
                || !raw("candidate_sha256", report.session.candidate_digest)
                || !number("repair_attempt", report.session.repair_attempt)
                || !reader.take("candidate_approved", value)
                || !parse_bool(value, candidate_approved)
                || !reader.take("resume_requires_revalidation", value)
                || !parse_bool(value, resume_revalidation)
                || !escaped("session_status", report.session.status)
                || !escaped("campaign_status", report.status)
                || !reader.take("cancelled", value)
                || !parse_bool(value, report.cancelled))
                return false;
            report.session.source.root = std::filesystem::path{root};
            report.session.policy = static_cast<iteration_session::CandidatePolicy>(policy);
            report.session.phase = static_cast<iteration_session::SessionPhase>(phase);
            report.session.candidate_approved = candidate_approved;
            report.session.resume_requires_revalidation = resume_revalidation;

            std::uint32_t file_count{};
            if (!number("file_count", file_count)
                || file_count == 0u
                || file_count > report.budgets.maximum_curated_files)
                return false;
            report.session.curated_files.clear();
            std::vector<std::string_view> parts{};
            for (std::uint32_t index = 0u; index < file_count; ++index)
            {
                if (!reader.take("file", value) || !split_fields(value, 3u, parts))
                    return false;
                iteration_session::CuratedFile file{};
                if (!unescape_field(parts[0], file.relative_path)
                    || !lowercase_hex(parts[1], 64u)
                    || !parse_unsigned(parts[2], file.byte_count))
                    return false;
                file.sha256.assign(parts[1]);
                report.session.curated_files.push_back(std::move(file));
            }

            std::uint32_t validation_count{};
            if (!number("validation_count", validation_count)
                || validation_count > report.budgets.maximum_validation_records)
                return false;
            report.session.validation.clear();
            for (std::uint32_t index = 0u; index < validation_count; ++index)
            {
                if (!reader.take("validation", value)
                    || !split_fields(value, 5u, parts))
                    return false;
                unsigned actor{};
                bool passed{};
                iteration_session::ValidationEvidence evidence{};
                if (!parse_unsigned(parts[0], actor) || actor > 6u
                    || !parse_bool(parts[1], passed)
                    || (!parts[2].empty() && !lowercase_hex(parts[2], 64u))
                    || !lowercase_hex(parts[3], 64u)
                    || !unescape_field(parts[4], evidence.summary))
                    return false;
                evidence.actor = static_cast<iteration_session::ValidationActor>(actor);
                evidence.passed = passed;
                evidence.candidate_digest.assign(parts[2]);
                evidence.evidence_digest.assign(parts[3]);
                report.session.validation.push_back(std::move(evidence));
            }
            return reader.complete() && valid_report(report)
                && serialize_payload(report) == payload;
        }

        [[nodiscard]] std::string make_envelope(
            const CampaignReport& report,
            std::string& digest)
        {
            const std::string payload = serialize_payload(report);
            digest = digest_text(payload);
            return "EPOCH_AI_CAMPAIGN_STATE_V1\npayload_bytes="
                + std::to_string(payload.size()) + "\npayload_sha256="
                + digest + "\n\n" + payload;
        }

        [[nodiscard]] bool parse_envelope(
            const std::string_view bytes,
            CampaignReport& report,
            std::string& digest)
        {
            if (bytes.empty() || bytes.size() > kMaximumReportBytes
                || !bytes.starts_with("EPOCH_AI_CAMPAIGN_STATE_V1\n"))
                return false;
            const std::size_t size_begin = std::string_view{
                "EPOCH_AI_CAMPAIGN_STATE_V1\n"}.size();
            const std::size_t size_end = bytes.find('\n', size_begin);
            const std::size_t digest_end = size_end == std::string_view::npos
                ? std::string_view::npos : bytes.find('\n', size_end + 1u);
            if (size_end == std::string_view::npos
                || digest_end == std::string_view::npos
                || digest_end + 1u >= bytes.size() || bytes[digest_end + 1u] != '\n')
                return false;
            constexpr std::string_view size_prefix = "payload_bytes=";
            constexpr std::string_view digest_prefix = "payload_sha256=";
            const std::string_view size_line = bytes.substr(
                size_begin, size_end - size_begin);
            const std::string_view digest_line = bytes.substr(
                size_end + 1u, digest_end - size_end - 1u);
            if (!size_line.starts_with(size_prefix)
                || !digest_line.starts_with(digest_prefix))
                return false;
            std::uint64_t payload_bytes{};
            if (!parse_unsigned(size_line.substr(size_prefix.size()), payload_bytes))
                return false;
            digest.assign(digest_line.substr(digest_prefix.size()));
            const std::string_view payload = bytes.substr(digest_end + 2u);
            return payload.size() == payload_bytes
                && lowercase_hex(digest, 64u)
                && digest_text(payload) == digest
                && parse_payload(payload, report);
        }

        [[nodiscard]] std::string read_small_file(
            const std::filesystem::path& path)
        {
            std::error_code ec{};
            const auto size = std::filesystem::file_size(path, ec);
            if (ec || size == 0u || size > kMaximumReportBytes)
                return {};
            std::ifstream input{path, std::ios::binary};
            if (!input)
                return {};
            std::string bytes(static_cast<std::size_t>(size), '\0');
            input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
            return input && input.peek() == std::char_traits<char>::eof()
                ? bytes : std::string{};
        }

        [[nodiscard]] CampaignResult reject(std::string status)
        {
            return CampaignResult{.status = std::move(status)};
        }
    }

    CampaignBudgets default_budgets(
        const iteration_session::IterationTargetKind target_kind) noexcept
    {
        CampaignBudgets budgets{};
        if (target_kind == iteration_session::IterationTargetKind::project_source)
        {
            budgets.maximum_workspace_files = 2048u;
            budgets.maximum_workspace_bytes = 256u * 1024u * 1024u;
        }
        return budgets;
    }

    std::vector<ValidationStep> validation_plan(
        const iteration_session::IterationTargetKind target_kind)
    {
        if (target_kind == iteration_session::IterationTargetKind::project_source)
            return {ValidationStep::project_compiler, ValidationStep::project_contract};
        return {
            ValidationStep::debug_compiler,
            ValidationStep::debug_contract,
            ValidationStep::release_compiler,
            ValidationStep::release_contract,
            ValidationStep::headless_compiler,
            ValidationStep::headless_contract,
            ValidationStep::full_validation};
    }

    CampaignResult begin_campaign(
        CampaignConfiguration configuration,
        iteration_session::IterationSession& session)
    {
        if (!valid_authority(configuration.authority)
            || configuration.created_at_unix_seconds == 0u
            || configuration.duration_seconds == 0u
            || configuration.duration_seconds > kMaximumCampaignSeconds
            || configuration.created_at_unix_seconds
                > (std::numeric_limits<std::uint64_t>::max)()
                    - configuration.duration_seconds
            || !valid_budgets(configuration.budgets)
            || (configuration.policy
                    == iteration_session::CandidatePolicy::auto_validate_within_approved_scope
                && !configuration.auto_validation_permit))
            return reject("Campaign configuration lacks exact authority, bounded time, budgets, or explicit auto-validation permission.");

        const auto configured = session.configure(
            iteration_session::SessionConfiguration{
                .objective = std::move(configuration.objective),
                .model_name = std::move(configuration.model_name),
                .source = std::move(configuration.authority),
                .curated_files = std::move(configuration.curated_files),
                .policy = configuration.policy,
                .maximum_repair_attempts =
                    configuration.budgets.maximum_repairs_per_candidate});
        if (!configured)
            return reject(configured.status);

        static std::atomic<std::uint64_t> next_campaign{0u};
        const std::uint64_t sequence = next_campaign.fetch_add(
            1u, std::memory_order_relaxed) + 1u;
        CampaignReport report{};
        report.created_at_unix_seconds = configuration.created_at_unix_seconds;
        report.expires_at_unix_seconds = configuration.created_at_unix_seconds
            + configuration.duration_seconds;
        report.budgets = configuration.budgets;
        report.session = session.report();
        report.target_key = digest_text(authority_fingerprint(report.session.source));
        report.campaign_id = digest_text(report.target_key + "\n"
            + report.session.objective_digest + "\n"
            + std::to_string(report.created_at_unix_seconds) + "\n"
            + std::to_string(sequence));
        report.status = "Durable campaign initialized; live source, Git, releases, updater, and network remain outside campaign authority.";
        if (!valid_report(report))
            return reject("Campaign initialization did not produce a valid bounded report.");
        return CampaignResult{
            .accepted = true,
            .report = std::move(report),
            .status = "Durable campaign initialized."};
    }

    CampaignResult consume_budget(
        CampaignReport report,
        const BudgetKind kind,
        const std::uint32_t amount)
    {
        if (!valid_report(report) || report.cancelled || amount == 0u)
            return reject("Campaign budget mutation rejected invalid state, cancellation, or zero work.");
        std::uint32_t* counter{};
        std::uint32_t maximum{};
        switch (kind)
        {
        case BudgetKind::candidate:
            counter = &report.counters.candidates;
            maximum = report.budgets.maximum_candidates;
            break;
        case BudgetKind::model_call:
            counter = &report.counters.model_calls;
            maximum = report.budgets.maximum_model_calls;
            break;
        case BudgetKind::source_operation:
            counter = &report.counters.source_operations;
            maximum = report.budgets.maximum_source_operations;
            break;
        case BudgetKind::validation_record:
            counter = &report.counters.validation_records;
            maximum = report.budgets.maximum_validation_records;
            break;
        }
        if (!counter || amount > maximum - *counter)
            return reject("Campaign budget is exhausted; no additional work was admitted.");
        *counter += amount;
        ++report.record_generation;
        report.previous_state_digest = report.state_digest;
        report.state_digest.clear();
        report.status = "Bounded campaign work admitted and counted.";
        return CampaignResult{
            .accepted = true,
            .report = std::move(report),
            .status = "Campaign budget consumed."};
    }

    std::filesystem::path state_path(
        const std::filesystem::path& cache_root,
        const CampaignReport& report)
    {
        if (!cache_root.is_absolute()
            || !lowercase_hex(report.target_key, 64u)
            || !lowercase_hex(report.campaign_id, 64u))
            return {};
        return cache_root / "campaigns"
            / target_kind_name(report.session.source.target_kind)
            / report.campaign_id / "state.epochai";
    }

    CampaignResult save_report(
        const std::filesystem::path& cache_root,
        const CampaignReport& report)
    {
        if (!valid_report(report) || !cache_root.is_absolute())
            return reject("Campaign report or cache root is invalid.");
        std::error_code ec{};
        std::filesystem::create_directories(cache_root, ec);
        const auto canonical_root = std::filesystem::weakly_canonical(cache_root, ec);
        if (ec || canonical_root.empty()
            || std::filesystem::is_symlink(canonical_root, ec))
            return reject("Campaign cache root could not be established safely.");
        const auto destination = state_path(canonical_root, report);
        std::filesystem::create_directories(destination.parent_path(), ec);
        if (ec)
            return reject("Campaign state directory could not be created.");
        const auto canonical_parent = std::filesystem::weakly_canonical(
            destination.parent_path(), ec);
        const auto relative_parent = canonical_parent.lexically_relative(canonical_root);
        if (ec || relative_parent.empty() || relative_parent.is_absolute()
            || std::any_of(relative_parent.begin(), relative_parent.end(),
                [](const auto& part) { return part == ".."; })
            || std::filesystem::is_symlink(canonical_parent, ec))
            return reject("Campaign state directory escaped or linked outside the cache root.");

        std::string digest{};
        const std::string bytes = make_envelope(report, digest);
        if (bytes.size() > kMaximumReportBytes)
            return reject("Campaign report exceeds the 512 KiB persistence bound.");
        static std::atomic<std::uint64_t> next_temp{0u};
        const auto temporary = std::filesystem::path{
            destination.string() + ".tmp."
                + std::to_string(report.record_generation) + "."
                + std::to_string(next_temp.fetch_add(1u, std::memory_order_relaxed) + 1u)};
        const auto data = std::span<const std::byte>{
            reinterpret_cast<const std::byte*>(bytes.data()), bytes.size()};
        if (!platform::filesystem::exclusive_create_and_write(temporary, data, ec))
            return reject("Campaign temporary state could not be exclusively written and flushed.");
        CampaignReport verified{};
        std::string verified_digest{};
        const std::string reread = read_small_file(temporary);
        if (reread != bytes || !parse_envelope(reread, verified, verified_digest)
            || serialize_payload(verified) != serialize_payload(report))
        {
            std::filesystem::remove(temporary, ec);
            return reject("Campaign temporary state failed exact reread verification.");
        }
        if (!platform::filesystem::atomic_replace_same_filesystem(
                temporary, destination, ec))
        {
            std::filesystem::remove(temporary, ec);
            return reject("Campaign state could not be atomically replaced; prior state remains authoritative.");
        }
        CampaignReport saved = report;
        saved.state_digest = digest;
        return CampaignResult{
            .accepted = true,
            .report = std::move(saved),
            .state_path = destination,
            .state_digest = digest,
            .status = "Campaign state was flushed, reread, and atomically published."};
    }

    CampaignResult load_report(const std::filesystem::path& report_path)
    {
        if (!report_path.is_absolute())
            return reject("Campaign state path is not absolute.");
        std::error_code ec{};
        const auto state = std::filesystem::symlink_status(report_path, ec);
        if (ec)
            return reject("Campaign state path metadata could not be inspected: "
                + ec.message());
        if (std::filesystem::is_symlink(state))
            return reject("Campaign state path is linked and was refused.");
        if (!std::filesystem::is_regular_file(state))
            return reject("Campaign state path is missing or non-regular.");
        const std::string bytes = read_small_file(report_path);
        CampaignReport report{};
        std::string digest{};
        if (!parse_envelope(bytes, report, digest))
            return reject("Campaign state envelope, payload hash, schema, or canonical fields are invalid.");
        report.state_digest = digest;
        return CampaignResult{
            .accepted = true,
            .report = std::move(report),
            .state_path = report_path,
            .state_digest = digest,
            .status = "Campaign state loaded with exact canonical integrity evidence."};
    }

    CampaignResult resume_campaign(
        CampaignReport report,
        const iteration_session::SourceAuthority& current_authority,
        std::vector<iteration_session::CuratedFile> current_files,
        const std::uint64_t now_unix_seconds,
        iteration_session::IterationSession& session,
        const bool auto_validation_permit)
    {
        if (!valid_report(report) || report.cancelled
            || now_unix_seconds < report.created_at_unix_seconds
            || now_unix_seconds >= report.expires_at_unix_seconds
            || !valid_authority(current_authority)
            || current_authority != report.session.source
            || current_files != report.session.curated_files)
            return reject("Campaign resume refused state, time, authority, profile, manifest, or curated-file drift.");
        if (report.counters.candidates >= report.budgets.maximum_candidates
            || report.counters.model_calls >= report.budgets.maximum_model_calls
            || report.counters.source_operations >= report.budgets.maximum_source_operations
            || report.counters.validation_records >= report.budgets.maximum_validation_records)
            return reject("Campaign resume refused an exhausted budget.");

        iteration_session::CandidateReport prior = report.session;
        prior.candidate_approved = false;
        prior.validation.clear();
        prior.proposal_digest.clear();
        prior.candidate_digest.clear();
        if (prior.policy
                == iteration_session::CandidatePolicy::auto_validate_within_approved_scope
            && !auto_validation_permit)
            prior.policy = iteration_session::CandidatePolicy::manual_each_candidate;
        const auto prior_identity = prior.identity;
        const auto resumed = session.resume_scope_fail_closed(
            prior, current_authority, std::move(current_files));
        if (!resumed || session.identity() == prior_identity
            || session.report().candidate_approved
            || !session.report().validation.empty())
            return reject("Campaign resume did not mint a clean, revalidated session identity.");

        ++report.record_generation;
        report.previous_state_digest = report.state_digest;
        report.state_digest.clear();
        report.session = session.report();
        report.status = "Campaign resumed after exact authority and curated-file revalidation; approvals and permits were cleared.";
        return CampaignResult{
            .accepted = true,
            .report = std::move(report),
            .status = "Campaign resumed fail-closed."};
    }
}
