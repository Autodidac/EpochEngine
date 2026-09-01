/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <algorithm>
#include <array>
#include <atomic>
#include <charconv>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

module ai.self_iteration_orchestrator;

import core.sha256;
import platform.filesystem;

namespace epochengine::ai::self_iteration_orchestrator
{
    namespace
    {
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

        [[nodiscard]] bool valid_summary(
            const std::string_view text,
            const Limits limits)
        {
            return !text.empty() && text.size() <= limits.maximum_summary_bytes
                && text.find('\0') == std::string_view::npos;
        }

        [[nodiscard]] std::string escape_field(const std::string_view text)
        {
            constexpr char digits[] = "0123456789abcdef";
            std::string result{};
            result.reserve(text.size());
            for (const unsigned char value : text)
            {
                const bool safe = (value >= 'a' && value <= 'z')
                    || (value >= 'A' && value <= 'Z')
                    || (value >= '0' && value <= '9')
                    || value == '.' || value == '_' || value == '-'
                    || value == '/' || value == ':' || value == '\\';
                if (safe)
                    result.push_back(static_cast<char>(value));
                else
                {
                    result.push_back('%');
                    result.push_back(digits[(value >> 4u) & 0x0fu]);
                    result.push_back(digits[value & 0x0fu]);
                }
            }
            return result;
        }

        [[nodiscard]] int nibble(const char value) noexcept
        {
            if (value >= '0' && value <= '9') return value - '0';
            if (value >= 'a' && value <= 'f') return value - 'a' + 10;
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
                    if (text[index] == '\r' || text[index] == '\n') return false;
                    result.push_back(text[index]);
                    continue;
                }
                if (index + 2u >= text.size()) return false;
                const int high = nibble(text[index + 1u]);
                const int low = nibble(text[index + 2u]);
                if (high < 0 || low < 0) return false;
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
            if (text.empty()) return false;
            Integer value{};
            const auto [end, error] = std::from_chars(
                text.data(), text.data() + text.size(), value);
            if (error != std::errc{} || end != text.data() + text.size()
                || std::to_string(value) != text)
                return false;
            result = value;
            return true;
        }

        [[nodiscard]] std::string target_name(
            const iteration_session::IterationTargetKind kind)
        {
            return kind == iteration_session::IterationTargetKind::engine_source
                ? "engine" : "project";
        }

        [[nodiscard]] std::array<iteration_session::ValidationActor, 7u>
            validation_actors()
        {
            using iteration_session::ValidationActor;
            return {ValidationActor::debug_compiler,
                ValidationActor::debug_contract,
                ValidationActor::release_compiler,
                ValidationActor::release_contract,
                ValidationActor::headless_compiler,
                ValidationActor::headless_contract,
                ValidationActor::full_validation};
        }

        [[nodiscard]] std::string host_fingerprint(const HostBinding& host)
        {
            std::ostringstream out{};
            out << host.binding_id << '\n' << host.configuration_sha256 << '\n'
                << host.generation << '\n' << host.stdio_only << '\n'
                << host.automatic_launch << '\n' << host.network_enabled << '\n'
                << host.listener_enabled << '\n' << host.server_enabled;
            return out.str();
        }

        [[nodiscard]] std::string serialize_payload(const Snapshot& snapshot)
        {
            std::ostringstream out{};
            out << "schema=epoch.ai.self_iteration_orchestrator.v1\n"
                << "generation=" << snapshot.generation << '\n'
                << "previous_state_sha256=" << snapshot.previous_state_sha256 << '\n'
                << "orchestrator_id=" << snapshot.orchestrator_id << '\n'
                << "target_key=" << snapshot.target_key << '\n'
                << "target_kind=" << static_cast<unsigned>(snapshot.campaign.session.source.target_kind) << '\n'
                << "phase=" << static_cast<unsigned>(snapshot.phase) << '\n'
                << "transport=" << static_cast<unsigned>(snapshot.transport) << '\n'
                << "provider=" << static_cast<unsigned>(snapshot.provider) << '\n'
                << "host_id=" << escape_field(snapshot.host.binding_id) << '\n'
                << "host_sha256=" << snapshot.host.configuration_sha256 << '\n'
                << "host_generation=" << snapshot.host.generation << '\n'
                << "profile_sha256=" << snapshot.profile_sha256 << '\n'
                << "campaign_id=" << snapshot.campaign.campaign_id << '\n'
                << "campaign_sha256=" << snapshot.campaign.state_digest << '\n'
                << "campaign_generation=" << snapshot.campaign.record_generation << '\n'
                << "plan_sha256=" << snapshot.plan_sha256 << '\n'
                << "proposal_sha256=" << snapshot.proposal_sha256 << '\n'
                << "candidate_sha256=" << snapshot.candidate_sha256 << '\n'
                << "pending_operation_id=" << snapshot.pending_operation_id << '\n'
                << "pending_operation_kind=" << static_cast<unsigned>(snapshot.pending_operation_kind) << '\n'
                << "last_transition_id=" << escape_field(snapshot.last_transition_id) << '\n'
                << "validation_index=" << snapshot.validation_index << '\n'
                << "status=" << escape_field(snapshot.status) << '\n'
                << "evidence_count=" << snapshot.evidence.size() << '\n';
            for (const auto& evidence : snapshot.evidence)
                out << "evidence=" << evidence.sequence << '|'
                    << static_cast<unsigned>(evidence.kind) << '|'
                    << (evidence.passed ? 1 : 0) << '|'
                    << escape_field(evidence.transition_id) << '|'
                    << evidence.evidence_sha256 << '|'
                    << escape_field(evidence.summary) << '\n';
            return out.str();
        }

        struct PersistedState final
        {
            Snapshot snapshot{};
            iteration_session::IterationTargetKind target_kind{
                iteration_session::IterationTargetKind::engine_source};
            std::string campaign_sha256{};
            std::uint64_t campaign_generation{};
        };

        [[nodiscard]] bool parse_payload(
            const std::string_view payload,
            PersistedState& persisted,
            const Limits limits)
        {
            std::vector<std::string_view> lines{};
            for (std::size_t start = 0u; start < payload.size();)
            {
                const auto end = payload.find('\n', start);
                if (end == std::string_view::npos) return false;
                lines.push_back(payload.substr(start, end - start));
                start = end + 1u;
            }
            std::size_t index{};
            auto take = [&](const std::string_view key, std::string_view& value)
            {
                if (index >= lines.size()) return false;
                const std::string prefix = std::string{key} + '=';
                if (!lines[index].starts_with(prefix)) return false;
                value = lines[index++].substr(prefix.size());
                return true;
            };
            auto number = [&](const std::string_view key, auto& value)
            {
                std::string_view text{};
                return take(key, text) && parse_unsigned(text, value);
            };
            std::string_view value{};
            Snapshot result{};
            unsigned raw{};
            if (!take("schema", value)
                || value != "epoch.ai.self_iteration_orchestrator.v1"
                || !number("generation", result.generation)
                || !take("previous_state_sha256", value)) return false;
            result.previous_state_sha256.assign(value);
            if ((!result.previous_state_sha256.empty()
                    && !lowercase_hex(result.previous_state_sha256, 64u))
                || !take("orchestrator_id", value)) return false;
            result.orchestrator_id.assign(value);
            if (!lowercase_hex(result.orchestrator_id, 64u)
                || !take("target_key", value)) return false;
            result.target_key.assign(value);
            if (!lowercase_hex(result.target_key, 64u)
                || !number("target_kind", raw) || raw > 1u) return false;
            result.campaign.target_key = result.target_key;
            persisted.target_kind = static_cast<iteration_session::IterationTargetKind>(raw);
            result.campaign.session.source.target_kind = persisted.target_kind;
            if (!number("phase", raw) || raw > static_cast<unsigned>(Phase::blocked)) return false;
            result.phase = static_cast<Phase>(raw);
            if (!number("transport", raw) || raw > 1u) return false;
            result.transport = static_cast<TransportKind>(raw);
            if (!number("provider", raw)
                || raw > static_cast<unsigned>(project_profile::Provider::engine_selected)) return false;
            result.provider = static_cast<project_profile::Provider>(raw);
            std::string decoded{};
            if (!take("host_id", value) || !unescape_field(value, decoded)) return false;
            result.host.binding_id = std::move(decoded);
            if (!take("host_sha256", value)) return false;
            result.host.configuration_sha256.assign(value);
            if (!number("host_generation", result.host.generation)
                || !take("profile_sha256", value)) return false;
            result.profile_sha256.assign(value);
            if (!take("campaign_id", value)) return false;
            result.campaign.campaign_id.assign(value);
            if (!take("campaign_sha256", value)) return false;
            persisted.campaign_sha256.assign(value);
            result.campaign.state_digest = persisted.campaign_sha256;
            if (!number("campaign_generation", persisted.campaign_generation)
                || !take("plan_sha256", value)) return false;
            result.campaign.record_generation = persisted.campaign_generation;
            result.plan_sha256.assign(value);
            if (!take("proposal_sha256", value)) return false;
            result.proposal_sha256.assign(value);
            if (!take("candidate_sha256", value)) return false;
            result.candidate_sha256.assign(value);
            if (!take("pending_operation_id", value)) return false;
            result.pending_operation_id.assign(value);
            if (!number("pending_operation_kind", raw)
                || raw > static_cast<unsigned>(OperationKind::trusted_validation)) return false;
            result.pending_operation_kind = static_cast<OperationKind>(raw);
            if (!take("last_transition_id", value)
                || !unescape_field(value, result.last_transition_id)
                || !number("validation_index", result.validation_index)
                || result.validation_index > validation_actors().size()
                || !take("status", value)
                || !unescape_field(value, result.status)) return false;
            std::size_t count{};
            if (!number("evidence_count", count)
                || count > limits.maximum_evidence_records) return false;
            result.evidence.reserve(count);
            for (std::size_t entry = 0u; entry < count; ++entry)
            {
                if (!take("evidence", value)) return false;
                std::array<std::string_view, 6u> fields{};
                std::size_t begin{};
                for (std::size_t part = 0u; part < fields.size(); ++part)
                {
                    const auto end = part + 1u == fields.size()
                        ? value.size() : value.find('|', begin);
                    if (end == std::string_view::npos) return false;
                    fields[part] = value.substr(begin, end - begin);
                    begin = end + 1u;
                }
                EvidenceRecord evidence{};
                unsigned kind{};
                unsigned passed{};
                if (!parse_unsigned(fields[0], evidence.sequence)
                    || evidence.sequence != entry + 1u
                    || !parse_unsigned(fields[1], kind)
                    || kind > static_cast<unsigned>(EvidenceKind::cancelled)
                    || !parse_unsigned(fields[2], passed) || passed > 1u
                    || !unescape_field(fields[3], evidence.transition_id)
                    || !lowercase_hex(fields[4], 64u)
                    || !unescape_field(fields[5], evidence.summary)
                    || !valid_summary(evidence.summary, limits)) return false;
                evidence.kind = static_cast<EvidenceKind>(kind);
                evidence.passed = passed != 0u;
                evidence.evidence_sha256.assign(fields[4]);
                result.evidence.push_back(std::move(evidence));
            }
            if (index != lines.size() || result.generation == 0u
                || !safe_identifier(result.host.binding_id)
                || !lowercase_hex(result.host.configuration_sha256, 64u)
                || result.host.generation == 0u
                || !lowercase_hex(result.campaign.campaign_id, 64u)
                || !lowercase_hex(persisted.campaign_sha256, 64u)
                || persisted.campaign_generation == 0u) return false;
            for (const auto& digest : {result.profile_sha256, result.plan_sha256,
                    result.proposal_sha256, result.candidate_sha256,
                    result.pending_operation_id})
                if (!digest.empty() && !lowercase_hex(digest, 64u)) return false;
            persisted.snapshot = std::move(result);
            return true;
        }

        [[nodiscard]] bool parse_envelope(
            const std::string_view bytes,
            PersistedState& state,
            std::string& digest,
            const Limits limits)
        {
            constexpr std::string_view header =
                "EPOCH_AI_SELF_ITERATION_ORCHESTRATOR_V1\n";
            if (bytes.size() > limits.maximum_state_bytes
                || !bytes.starts_with(header)) return false;
            const auto size_end = bytes.find('\n', header.size());
            const auto digest_end = size_end == std::string_view::npos
                ? std::string_view::npos : bytes.find('\n', size_end + 1u);
            if (size_end == std::string_view::npos
                || digest_end == std::string_view::npos
                || digest_end + 1u >= bytes.size()
                || bytes[digest_end + 1u] != '\n') return false;
            constexpr std::string_view size_prefix = "payload_bytes=";
            constexpr std::string_view digest_prefix = "payload_sha256=";
            const auto size_line = bytes.substr(header.size(), size_end - header.size());
            const auto digest_line = bytes.substr(size_end + 1u, digest_end - size_end - 1u);
            if (!size_line.starts_with(size_prefix)
                || !digest_line.starts_with(digest_prefix)) return false;
            std::uint64_t size{};
            if (!parse_unsigned(size_line.substr(size_prefix.size()), size)) return false;
            digest.assign(digest_line.substr(digest_prefix.size()));
            const auto payload = bytes.substr(digest_end + 2u);
            return payload.size() == size && lowercase_hex(digest, 64u)
                && digest_text(payload) == digest
                && parse_payload(payload, state, limits);
        }

        [[nodiscard]] std::string read_bounded(
            const std::filesystem::path& path,
            const std::size_t maximum)
        {
            std::error_code ec{};
            const auto size = std::filesystem::file_size(path, ec);
            if (ec || size == 0u || size > maximum) return {};
            std::ifstream input{path, std::ios::binary};
            if (!input) return {};
            return std::string{
                std::istreambuf_iterator<char>{input},
                std::istreambuf_iterator<char>{}};
        }
    }

    OperationKind PendingOperation::kind() const noexcept { return kind_; }
    const std::string& PendingOperation::operation_id() const noexcept { return operation_id_; }
    const std::string& PendingOperation::orchestrator_id() const noexcept { return orchestrator_id_; }
    const std::string& PendingOperation::campaign_id() const noexcept { return campaign_id_; }
    iteration_session::RequestIdentity PendingOperation::session_identity() const noexcept { return session_identity_; }
    iteration_session::IterationTargetKind PendingOperation::target_kind() const noexcept { return target_kind_; }
    TransportKind PendingOperation::transport() const noexcept { return transport_; }
    const HostBinding& PendingOperation::host_binding() const noexcept { return host_binding_; }
    std::uint64_t PendingOperation::expected_generation() const noexcept { return expected_generation_; }
    const std::string& PendingOperation::expected_state_sha256() const noexcept { return expected_state_sha256_; }
    const std::string& PendingOperation::transition_id() const noexcept { return transition_id_; }
    const std::string& PendingOperation::objective_sha256() const noexcept { return objective_sha256_; }
    const std::string& PendingOperation::scope_sha256() const noexcept { return scope_sha256_; }
    const std::string& PendingOperation::plan_sha256() const noexcept { return plan_sha256_; }
    const std::string& PendingOperation::proposal_sha256() const noexcept { return proposal_sha256_; }
    const std::string& PendingOperation::candidate_sha256() const noexcept { return candidate_sha256_; }
    std::uint32_t PendingOperation::validation_index() const noexcept { return validation_index_; }
    iteration_session::ValidationActor PendingOperation::validation_actor() const noexcept { return validation_actor_; }

    bool Orchestrator::validate_configuration(
        const Configuration& configuration,
        std::string& model_name,
        std::string& profile_sha256,
        TransportKind& transport,
        std::string& status) const
    {
        using iteration_session::IterationTargetKind;
        if (!limits_.valid() || !configuration.cache_root.is_absolute()
            || configuration.created_at_unix_seconds == 0u
            || configuration.objective.empty()
            || configuration.objective.size() > 4096u
            || !safe_identifier(configuration.host.binding_id)
            || !lowercase_hex(configuration.host.configuration_sha256, 64u)
            || configuration.host.generation == 0u || !configuration.host.stdio_only
            || configuration.host.automatic_launch || configuration.host.network_enabled
            || configuration.host.listener_enabled || configuration.host.server_enabled
            || !configuration.sandbox_apply_permitted)
        {
            status = "Orchestration requires bounded authority, explicit sandbox apply, and one inert stdio-only host binding.";
            return false;
        }
        if (configuration.provider == project_profile::Provider::disabled)
        {
            status = "Disabled AI provider cannot start a self-iteration campaign.";
            return false;
        }
        transport = configuration.provider == project_profile::Provider::external_mcp
            ? TransportKind::external_mcp : TransportKind::guarded_local_mcp_child;
        if (configuration.authority.target_kind == IterationTargetKind::project_source)
        {
            if (!configuration.project_source_campaign_permitted
                || configuration.engine_source_campaign_permitted
                || configuration.project_profile_bytes.empty())
            {
                status = "Generated-project orchestration requires only project-source authority.";
                return false;
            }
            const auto decoded = project_profile::parse_profile(
                configuration.project_profile_bytes);
            const bool providerMatches = decoded
                && (decoded.profile.provider == configuration.provider
                    || (decoded.profile.provider
                            == project_profile::Provider::engine_selected
                        && (configuration.provider
                                == project_profile::Provider::epoch_local_qwen38
                            || configuration.provider
                                == project_profile::Provider::external_mcp)));
            if (!providerMatches
                || !decoded.profile.enabled || !decoded.profile.project_source_write
                || decoded.profile.engine_source_write
                || !decoded.profile.operator_approval_per_iteration
                || decoded.profile.auto_start || decoded.profile.server_or_listener
                || decoded.sha256 != configuration.authority.project_profile_digest)
            {
                status = "Project profile failed strict canonical authority validation.";
                return false;
            }
            profile_sha256 = decoded.sha256;
            model_name = configuration.provider == project_profile::Provider::external_mcp
                    || decoded.profile.provider
                        == project_profile::Provider::engine_selected
                ? configuration.operator_model_binding
                : decoded.profile.model_binding;
            if (model_name.empty() || model_name.size() > 256u)
            {
                status = "External MCP project orchestration requires one explicit operator-selected model binding.";
                return false;
            }
        }
        else
        {
            if (!configuration.engine_source_campaign_permitted
                || configuration.project_source_campaign_permitted
                || !configuration.project_profile_bytes.empty()
                || configuration.engine_model_binding.empty()
                || configuration.engine_model_binding.size() > 256u)
            {
                status = "Engine orchestration requires a separate explicit engine-source permit and model binding.";
                return false;
            }
            profile_sha256 = digest_text("engine-source\n"
                + std::to_string(static_cast<unsigned>(configuration.provider))
                + "\n" + configuration.engine_model_binding);
            model_name = configuration.engine_model_binding;
        }
        return true;
    }

    Result Orchestrator::begin(Configuration configuration, const Limits limits)
    {
        if (configured_) return reject("Orchestrator is already configured.");
        limits_ = limits;
        std::string model{};
        std::string profile_digest{};
        std::string status{};
        TransportKind transport{};
        if (!validate_configuration(configuration, model, profile_digest, transport, status))
            return reject(std::move(status));
        iteration_campaign::CampaignResult begun = iteration_campaign::begin_campaign(
            iteration_campaign::CampaignConfiguration{
                .authority = configuration.authority,
                .objective = configuration.objective,
                .model_name = std::move(model),
                .curated_files = configuration.curated_files,
                .policy = iteration_session::CandidatePolicy::manual_each_candidate,
                .budgets = configuration.budgets,
                .created_at_unix_seconds = configuration.created_at_unix_seconds,
                .duration_seconds = configuration.duration_seconds,
                .auto_validation_permit = false}, session_);
        if (!begun) return reject(begun.status);
        cache_root_ = configuration.cache_root;
        snapshot_ = {};
        snapshot_.generation = 1u;
        snapshot_.campaign = std::move(begun.report);
        snapshot_.target_key = snapshot_.campaign.target_key;
        snapshot_.profile_sha256 = std::move(profile_digest);
        snapshot_.provider = configuration.provider;
        snapshot_.transport = transport;
        snapshot_.host = std::move(configuration.host);
        snapshot_.orchestrator_id = digest_text(snapshot_.campaign.campaign_id + "\n"
            + snapshot_.profile_sha256 + "\n" + host_fingerprint(snapshot_.host));
        snapshot_.phase = Phase::awaiting_plan_request;
        snapshot_.status = "Self-iteration campaign configured; live source remains read-only and every mutation requires host evidence plus manual approval.";
        snapshot_.manual_approval_required = true;
        snapshot_.live_source_read_only = true;
        state_path_ = cache_root_ / "orchestrations"
            / target_name(snapshot_.campaign.session.source.target_kind)
            / snapshot_.orchestrator_id / "state.epochai";
        snapshot_.evidence.push_back(EvidenceRecord{
            .sequence = 1u,
            .kind = EvidenceKind::configured,
            .transition_id = "configured",
            .evidence_sha256 = digest_text(snapshot_.campaign.campaign_id + "\n"
                + snapshot_.profile_sha256 + "\n" + snapshot_.host.configuration_sha256),
            .summary = "Strict target, provider, host binding, budgets, and manual approval policy admitted.",
            .passed = true});
        auto persisted = persist(snapshot_.status);
        if (!persisted)
        {
            const std::string failure = persisted.status;
            cache_root_.clear();
            state_path_.clear();
            snapshot_ = {};
            session_ = {};
            return reject("Campaign start failed closed: " + failure);
        }
        configured_ = true;
        return persisted;
    }

    Result Orchestrator::resume(
        Configuration configuration,
        const std::filesystem::path& path,
        const std::uint64_t now,
        const Limits limits)
    {
        if (configured_ || !path.is_absolute() || !limits.valid())
            return reject("Resume requires one fresh orchestrator and an absolute state path.");
        limits_ = limits;
        std::error_code ec{};
        if (!std::filesystem::is_regular_file(path, ec)
            || std::filesystem::is_symlink(path, ec))
            return reject("Orchestration state is missing, linked, or not regular.");
        PersistedState persisted{};
        std::string state_digest{};
        const std::string bytes = read_bounded(path, limits.maximum_state_bytes);
        if (!parse_envelope(bytes, persisted, state_digest, limits))
            return reject("Orchestration state envelope or digest is invalid.");
        cache_root_ = configuration.cache_root;
        std::string model{};
        std::string profile_digest{};
        std::string status{};
        TransportKind transport{};
        if (!validate_configuration(configuration, model, profile_digest, transport, status)
            || profile_digest != persisted.snapshot.profile_sha256
            || transport != persisted.snapshot.transport
            || configuration.provider != persisted.snapshot.provider
            || configuration.host.binding_id != persisted.snapshot.host.binding_id
            || configuration.host.configuration_sha256
                != persisted.snapshot.host.configuration_sha256
            || configuration.host.generation != persisted.snapshot.host.generation
            || configuration.authority.target_kind != persisted.target_kind)
            return reject("Resume refused provider, profile, target, or host-binding drift.");
        const auto campaign_path = iteration_campaign::state_path(
            cache_root_, persisted.snapshot.campaign);
        auto loaded = iteration_campaign::load_report(campaign_path);
        std::error_code campaign_path_error{};
        const bool compact_campaign_state_exists =
            std::filesystem::is_regular_file(
                campaign_path, campaign_path_error)
            && !campaign_path_error;
        if (!loaded && !compact_campaign_state_exists)
        {
            const auto legacy_campaign_path = cache_root_ / "campaigns"
                / target_name(persisted.target_kind)
                / persisted.snapshot.target_key
                / persisted.snapshot.campaign.campaign_id / "state.epochai";
            loaded = iteration_campaign::load_report(legacy_campaign_path);
        }
        if (!loaded)
            return reject("Resume refused missing durable campaign state: "
                + loaded.status);
        if (loaded.report.campaign_id
                != persisted.snapshot.campaign.campaign_id
            || loaded.report.target_key != persisted.snapshot.target_key)
            return reject("Resume refused mismatched durable campaign identity.");
        if (loaded.report.record_generation < persisted.campaign_generation)
            return reject("Resume refused stale durable campaign generation.");
        auto resumed = iteration_campaign::resume_campaign(
            loaded.report, configuration.authority, configuration.curated_files,
            now, session_, false);
        if (!resumed) return reject(resumed.status);
        snapshot_ = std::move(persisted.snapshot);
        snapshot_.state_sha256 = state_digest;
        snapshot_.previous_state_sha256 = state_digest;
        snapshot_.campaign = std::move(resumed.report);
        snapshot_.provider = configuration.provider;
        snapshot_.transport = transport;
        snapshot_.host = std::move(configuration.host);
        snapshot_.phase = Phase::awaiting_plan_request;
        snapshot_.plan_sha256.clear();
        snapshot_.proposal_sha256.clear();
        snapshot_.candidate_sha256.clear();
        snapshot_.pending_operation_id.clear();
        snapshot_.validation_index = 0u;
        ++snapshot_.generation;
        snapshot_.last_transition_id = "resume-" + std::to_string(snapshot_.generation);
        snapshot_.evidence.push_back(EvidenceRecord{
            .sequence = snapshot_.evidence.size() + 1u,
            .kind = EvidenceKind::resumed,
            .transition_id = snapshot_.last_transition_id,
            .evidence_sha256 = digest_text(state_digest + "\n"
                + loaded.state_digest + "\n" + snapshot_.campaign.session.scope_digest),
            .summary = "Crash/restart resume revalidated source authority and curated hashes; pending work and approvals were cleared.",
            .passed = true});
        if (snapshot_.evidence.size() > limits_.maximum_evidence_records)
            return reject("Resume evidence exceeds the orchestration record budget.");
        state_path_ = path;
        configured_ = true;
        return persist("Campaign resumed fail-closed; a fresh plan request is required.");
    }

    bool Orchestrator::action_matches(const ActionToken& action) const
    {
        return configured_ && action.expected_generation == snapshot_.generation
            && action.expected_state_sha256 == snapshot_.state_sha256
            && lowercase_hex(action.expected_state_sha256, 64u)
            && safe_identifier(action.transition_id)
            && action.transition_id != snapshot_.last_transition_id
            && action.now_unix_seconds >= snapshot_.campaign.created_at_unix_seconds
            && action.now_unix_seconds < snapshot_.campaign.expires_at_unix_seconds;
    }

    bool Orchestrator::receipt_matches(const OperationReceipt& receipt) const
    {
        return configured_ && !snapshot_.pending_operation_id.empty()
            && receipt.operation_id == snapshot_.pending_operation_id
            && receipt.expected_generation == snapshot_.generation
            && receipt.expected_state_sha256 == snapshot_.state_sha256
            && receipt.transition_id == snapshot_.last_transition_id
            && receipt.now_unix_seconds >= snapshot_.campaign.created_at_unix_seconds
            && receipt.now_unix_seconds < snapshot_.campaign.expires_at_unix_seconds;
    }

    PendingOperation Orchestrator::make_pending_operation() const
    {
        PendingOperation result{};
        result.kind_ = snapshot_.pending_operation_kind;
        result.operation_id_ = snapshot_.pending_operation_id;
        result.orchestrator_id_ = snapshot_.orchestrator_id;
        result.campaign_id_ = snapshot_.campaign.campaign_id;
        result.session_identity_ = snapshot_.campaign.session.identity;
        result.target_kind_ = snapshot_.campaign.session.source.target_kind;
        result.transport_ = snapshot_.transport;
        result.host_binding_ = snapshot_.host;
        result.expected_generation_ = snapshot_.generation;
        result.expected_state_sha256_ = snapshot_.state_sha256;
        result.transition_id_ = snapshot_.last_transition_id;
        result.objective_sha256_ = snapshot_.campaign.session.objective_digest;
        result.scope_sha256_ = snapshot_.campaign.session.scope_digest;
        result.plan_sha256_ = snapshot_.plan_sha256;
        result.proposal_sha256_ = snapshot_.proposal_sha256;
        result.candidate_sha256_ = snapshot_.candidate_sha256;
        result.validation_index_ = snapshot_.validation_index;
        if (snapshot_.validation_index < validation_actors().size())
            result.validation_actor_ = validation_actors()[snapshot_.validation_index];
        return result;
    }

    Result Orchestrator::reject(std::string status) const
    {
        return Result{.accepted = false, .snapshot = snapshot_,
            .state_path = state_path_, .status = std::move(status)};
    }

    Result Orchestrator::persist(std::string status)
    {
        snapshot_.campaign.session = session_.report();
        snapshot_.campaign.status = status;
        auto saved_campaign = iteration_campaign::save_report(cache_root_, snapshot_.campaign);
        if (!saved_campaign) return reject(saved_campaign.status);
        snapshot_.campaign = std::move(saved_campaign.report);
        snapshot_.status = std::move(status);
        const std::string payload = serialize_payload(snapshot_);
        const std::string digest = digest_text(payload);
        const std::string bytes = "EPOCH_AI_SELF_ITERATION_ORCHESTRATOR_V1\npayload_bytes="
            + std::to_string(payload.size()) + "\npayload_sha256=" + digest
            + "\n\n" + payload;
        if (bytes.size() > limits_.maximum_state_bytes)
            return reject("Orchestration state exceeds its persistence budget.");
        std::error_code ec{};
        std::filesystem::create_directories(state_path_.parent_path(), ec);
        if (ec) return reject("Orchestration state directory could not be created.");
        const auto root = std::filesystem::weakly_canonical(cache_root_, ec);
        const auto parent = std::filesystem::weakly_canonical(state_path_.parent_path(), ec);
        const auto relative = parent.lexically_relative(root);
        if (ec || root.empty() || parent.empty() || relative.empty()
            || relative.is_absolute()
            || std::any_of(relative.begin(), relative.end(),
                [](const auto& part) { return part == ".."; })
            || std::filesystem::is_symlink(parent, ec))
            return reject("Orchestration state directory escaped the canonical cache root.");
        static std::atomic<std::uint64_t> next_temp{0u};
        const auto temporary = std::filesystem::path{state_path_.string()
            + ".tmp." + std::to_string(snapshot_.generation) + "."
            + std::to_string(next_temp.fetch_add(1u) + 1u)};
        const auto data = std::span<const std::byte>{
            reinterpret_cast<const std::byte*>(bytes.data()), bytes.size()};
        if (!platform::filesystem::exclusive_create_and_write(temporary, data, ec))
            return reject("Orchestration temporary state could not be flushed.");
        PersistedState verified{};
        std::string verified_digest{};
        const std::string reread = read_bounded(temporary, limits_.maximum_state_bytes);
        if (reread != bytes
            || !parse_envelope(reread, verified, verified_digest, limits_)
            || verified_digest != digest
            || serialize_payload(verified.snapshot) != payload)
        {
            std::filesystem::remove(temporary, ec);
            return reject("Orchestration temporary state failed exact reread verification.");
        }
        if (!platform::filesystem::atomic_replace_same_filesystem(
                temporary, state_path_, ec))
        {
            std::filesystem::remove(temporary, ec);
            return reject("Orchestration state could not be atomically replaced.");
        }
        snapshot_.state_sha256 = digest;
        Result result{.accepted = true, .snapshot = snapshot_,
            .state_path = state_path_, .status = snapshot_.status};
        if (!snapshot_.pending_operation_id.empty())
            result.pending_operation = make_pending_operation();
        return result;
    }

    Result Orchestrator::commit(
        const Phase phase,
        const EvidenceKind evidence_kind,
        std::string transition_id,
        std::string evidence_sha256,
        std::string summary,
        const bool passed,
        const bool campaign_already_advanced)
    {
        if (!safe_identifier(transition_id) || !lowercase_hex(evidence_sha256, 64u)
            || !valid_summary(summary, limits_)
            || snapshot_.evidence.size() >= limits_.maximum_evidence_records)
            return reject("Transition evidence is malformed or exceeds its budget.");
        if (!campaign_already_advanced)
        {
            ++snapshot_.campaign.record_generation;
            snapshot_.campaign.previous_state_digest = snapshot_.campaign.state_digest;
            snapshot_.campaign.state_digest.clear();
        }
        snapshot_.previous_state_sha256 = snapshot_.state_sha256;
        snapshot_.state_sha256.clear();
        ++snapshot_.generation;
        snapshot_.phase = phase;
        snapshot_.last_transition_id = transition_id;
        snapshot_.evidence.push_back(EvidenceRecord{
            .sequence = snapshot_.evidence.size() + 1u,
            .kind = evidence_kind,
            .transition_id = std::move(transition_id),
            .evidence_sha256 = std::move(evidence_sha256),
            .summary = summary,
            .passed = passed});
        return persist(std::move(summary));
    }

    Result Orchestrator::create_operation(
        ActionToken action,
        const OperationKind kind,
        const Phase waiting_phase,
        const EvidenceKind evidence_kind,
        const iteration_campaign::BudgetKind budget_kind,
        std::string summary)
    {
        if (!action_matches(action) || !snapshot_.pending_operation_id.empty())
            return reject("Operation request is stale, replayed, expired, or overlaps pending work.");
        auto counted = iteration_campaign::consume_budget(snapshot_.campaign, budget_kind);
        if (!counted) return reject(counted.status);
        snapshot_.campaign = std::move(counted.report);
        snapshot_.pending_operation_kind = kind;
        snapshot_.pending_operation_id = digest_text(snapshot_.orchestrator_id + "\n"
            + snapshot_.campaign.campaign_id + "\n" + action.transition_id + "\n"
            + std::to_string(snapshot_.generation + 1u) + "\n"
            + std::to_string(static_cast<unsigned>(kind)));
        return commit(waiting_phase, evidence_kind, std::move(action.transition_id),
            digest_text(snapshot_.pending_operation_id + "\n"
                + snapshot_.campaign.session.scope_digest),
            std::move(summary), true, true);
    }

    Result Orchestrator::request_plan(ActionToken action)
    {
        if (snapshot_.phase != Phase::awaiting_plan_request)
            return reject("Plan request is not valid in the current state.");
        return create_operation(std::move(action), OperationKind::model_plan,
            Phase::awaiting_plan_result, EvidenceKind::plan_requested,
            iteration_campaign::BudgetKind::model_call,
            "Bounded plan request admitted through the selected operator-owned MCP transport.");
    }

    Result Orchestrator::record_plan(
        OperationReceipt receipt,
        const std::string_view plan_bytes,
        std::string summary)
    {
        if (snapshot_.phase != Phase::awaiting_plan_result
            || snapshot_.pending_operation_kind != OperationKind::model_plan
            || !receipt_matches(receipt) || plan_bytes.empty()
            || plan_bytes.size() > limits_.maximum_plan_bytes)
            return reject("Plan result is stale, malformed, or outside the plan budget.");
        snapshot_.plan_sha256 = digest_text(plan_bytes);
        snapshot_.pending_operation_id.clear();
        return commit(Phase::awaiting_curated_evidence,
            EvidenceKind::plan_received, receipt.transition_id,
            snapshot_.plan_sha256, std::move(summary), true);
    }

    Result Orchestrator::share_curated_evidence(
        ActionToken action,
        std::string scope_sha256,
        std::string evidence_sha256,
        std::string summary)
    {
        if (snapshot_.phase != Phase::awaiting_curated_evidence
            || !action_matches(action) || !action.operator_approved
            || scope_sha256 != snapshot_.campaign.session.scope_digest
            || !lowercase_hex(evidence_sha256, 64u))
            return reject("Curated evidence requires exact current scope and explicit operator approval.");
        const auto shared = session_.context_shared();
        if (!shared) return reject(shared.status);
        snapshot_.campaign.session = session_.report();
        return commit(Phase::awaiting_proposal_request,
            EvidenceKind::curated_context, std::move(action.transition_id),
            std::move(evidence_sha256), std::move(summary), true);
    }

    Result Orchestrator::request_proposal(ActionToken action)
    {
        if (snapshot_.phase != Phase::awaiting_proposal_request)
            return reject("Proposal request is not valid in the current state.");
        return create_operation(std::move(action), OperationKind::model_proposal,
            Phase::awaiting_proposal_result, EvidenceKind::proposal_requested,
            iteration_campaign::BudgetKind::model_call,
            "Digest-bound proposal request admitted through the selected MCP transport.");
    }

    Result Orchestrator::record_proposal(
        OperationReceipt receipt,
        const std::string_view proposal_bytes,
        std::string summary)
    {
        if (snapshot_.phase != Phase::awaiting_proposal_result
            || snapshot_.pending_operation_kind != OperationKind::model_proposal
            || !receipt_matches(receipt) || proposal_bytes.empty()
            || proposal_bytes.size() > snapshot_.campaign.budgets.maximum_candidate_bytes)
            return reject("Proposal result is stale, malformed, or outside the candidate budget.");
        const auto staged = session_.stage_candidate(
            snapshot_.campaign.session.identity, proposal_bytes);
        if (!staged) return reject(staged.status);
        auto counted = iteration_campaign::consume_budget(
            snapshot_.campaign, iteration_campaign::BudgetKind::candidate);
        if (!counted) return reject(counted.status);
        snapshot_.campaign = std::move(counted.report);
        snapshot_.campaign.session = session_.report();
        snapshot_.proposal_sha256 = snapshot_.campaign.session.proposal_digest;
        snapshot_.candidate_sha256 = snapshot_.campaign.session.candidate_digest;
        snapshot_.pending_operation_id.clear();
        return commit(Phase::awaiting_manual_review,
            EvidenceKind::proposal_received, receipt.transition_id,
            snapshot_.proposal_sha256, std::move(summary), true, true);
    }

    Result Orchestrator::review_proposal(
        ActionToken action,
        const bool approve,
        std::string reason)
    {
        if (snapshot_.phase != Phase::awaiting_manual_review
            || !action_matches(action) || !action.operator_approved
            || !valid_summary(reason, limits_))
            return reject("Proposal review requires one current explicit operator decision.");
        if (!approve)
        {
            const auto cancelled = session_.cancel(
                snapshot_.campaign.session.identity, reason);
            if (!cancelled) return reject(cancelled.status);
            snapshot_.campaign.cancelled = true;
            snapshot_.campaign.session = session_.report();
            return commit(Phase::rejected, EvidenceKind::manual_review,
                std::move(action.transition_id), digest_text("reject\n" + reason),
                std::move(reason), false);
        }
        const auto approved = session_.approve_candidate(
            snapshot_.campaign.session.identity);
        if (!approved) return reject(approved.status);
        snapshot_.campaign.session = session_.report();
        return commit(Phase::awaiting_apply_decision,
            EvidenceKind::manual_review, std::move(action.transition_id),
            digest_text("approve\n" + snapshot_.candidate_sha256),
            std::move(reason), true);
    }

    Result Orchestrator::decide_apply(
        ActionToken action,
        const bool approve,
        std::string reason)
    {
        if (snapshot_.phase != Phase::awaiting_apply_decision
            || !action_matches(action) || !action.operator_approved
            || !valid_summary(reason, limits_))
            return reject("Sandbox apply decision requires one current explicit operator approval.");
        if (!approve)
        {
            const auto cancelled = session_.cancel(
                snapshot_.campaign.session.identity, reason);
            if (!cancelled) return reject(cancelled.status);
            snapshot_.campaign.cancelled = true;
            snapshot_.campaign.session = session_.report();
            return commit(Phase::rejected, EvidenceKind::apply_decision,
                std::move(action.transition_id), digest_text("reject-apply\n" + reason),
                std::move(reason), false);
        }
        return create_operation(std::move(action), OperationKind::sandbox_apply,
            Phase::awaiting_apply_result, EvidenceKind::apply_decision,
            iteration_campaign::BudgetKind::source_operation, std::move(reason));
    }

    Result Orchestrator::record_apply(
        OperationReceipt receipt,
        std::string implementation_evidence_sha256,
        std::string summary)
    {
        if (snapshot_.phase != Phase::awaiting_apply_result
            || snapshot_.pending_operation_kind != OperationKind::sandbox_apply
            || !receipt_matches(receipt)
            || !lowercase_hex(implementation_evidence_sha256, 64u))
            return reject("Sandbox implementation evidence is stale or malformed.");
        const auto applied = session_.record_implementation(
            snapshot_.campaign.session.identity,
            implementation_evidence_sha256, summary);
        if (!applied) return reject(applied.status);
        snapshot_.campaign.session = session_.report();
        snapshot_.candidate_sha256 = snapshot_.campaign.session.candidate_digest;
        snapshot_.pending_operation_id.clear();
        snapshot_.validation_index = 0u;
        return commit(Phase::awaiting_validation_request,
            EvidenceKind::implementation, receipt.transition_id,
            std::move(implementation_evidence_sha256), std::move(summary), true);
    }

    Result Orchestrator::request_validation(ActionToken action)
    {
        if (snapshot_.phase != Phase::awaiting_validation_request
            || snapshot_.validation_index >= validation_actors().size())
            return reject("No trusted validation step is currently requestable.");
        return create_operation(std::move(action), OperationKind::trusted_validation,
            Phase::awaiting_validation_result, EvidenceKind::validation,
            iteration_campaign::BudgetKind::validation_record,
            "Trusted digest-bound validation requested; execution remains host-owned.");
    }

    Result Orchestrator::record_validation(
        OperationReceipt receipt,
        std::string evidence_sha256,
        std::string summary,
        const bool passed)
    {
        if (snapshot_.phase != Phase::awaiting_validation_result
            || snapshot_.pending_operation_kind != OperationKind::trusted_validation
            || !receipt_matches(receipt)
            || !lowercase_hex(evidence_sha256, 64u)
            || snapshot_.validation_index >= validation_actors().size())
            return reject("Validation evidence is stale, malformed, or outside the fixed sequence.");
        const std::uint32_t prior_index = snapshot_.validation_index;
        const bool complete = passed && prior_index + 1u == validation_actors().size();
        const auto recorded = session_.record_validation(
            snapshot_.campaign.session.identity,
            iteration_session::ValidationEvidence{
                .actor = validation_actors()[prior_index],
                .candidate_digest = snapshot_.candidate_sha256,
                .evidence_digest = evidence_sha256,
                .summary = summary,
                .passed = passed}, complete);
        if (!recorded) return reject(recorded.status);
        snapshot_.campaign.session = session_.report();
        snapshot_.pending_operation_id.clear();
        if (!passed)
        {
            snapshot_.candidate_sha256.clear();
            snapshot_.proposal_sha256.clear();
            snapshot_.validation_index = 0u;
            return commit(Phase::awaiting_proposal_request,
                EvidenceKind::validation, receipt.transition_id,
                std::move(evidence_sha256), std::move(summary), false);
        }
        ++snapshot_.validation_index;
        return commit(complete ? Phase::checkpoint_ready
                : Phase::awaiting_validation_request,
            EvidenceKind::validation, receipt.transition_id,
            std::move(evidence_sha256), std::move(summary), true);
    }

    Result Orchestrator::checkpoint(ActionToken action, std::string summary)
    {
        if (snapshot_.phase != Phase::checkpoint_ready
            || !action_matches(action) || !action.operator_approved
            || snapshot_.campaign.session.phase
                != iteration_session::SessionPhase::candidate_verified)
            return reject("Checkpoint requires exact verified candidate state and manual operator approval.");
        return commit(Phase::checkpointed, EvidenceKind::checkpoint,
            std::move(action.transition_id),
            digest_text(snapshot_.candidate_sha256 + "\n"
                + snapshot_.campaign.state_digest),
            std::move(summary), true);
    }

    Result Orchestrator::cancel(ActionToken action, std::string reason)
    {
        if (!action_matches(action) || !action.operator_approved
            || snapshot_.phase == Phase::checkpointed
            || snapshot_.phase == Phase::cancelled
            || snapshot_.phase == Phase::rejected)
            return reject("Cancellation requires one current explicit operator decision.");
        const auto cancelled = session_.cancel(
            snapshot_.campaign.session.identity, reason);
        if (!cancelled) return reject(cancelled.status);
        snapshot_.campaign.cancelled = true;
        snapshot_.campaign.session = session_.report();
        snapshot_.pending_operation_id.clear();
        return commit(Phase::cancelled, EvidenceKind::cancelled,
            std::move(action.transition_id), digest_text("cancel\n" + reason),
            std::move(reason), true);
    }

    Snapshot Orchestrator::snapshot() const { return snapshot_; }
    std::filesystem::path Orchestrator::state_path() const { return state_path_; }
}
