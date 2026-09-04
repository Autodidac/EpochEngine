/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <initializer_list>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

module ai.mcp_campaign;

import core.sha256;

namespace epochengine::ai::mcp_campaign
{
    namespace
    {
        constexpr std::uint64_t kCampaignDurationSeconds = 8u * 60u * 60u;

        [[nodiscard]] bool lowercase_hex(
            const std::string_view value,
            const std::size_t size) noexcept
        {
            return value.size() == size
                && std::all_of(value.begin(), value.end(), [](const char c)
                {
                    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
                });
        }

        [[nodiscard]] bool identifier(
            const std::string_view value,
            const std::size_t maximum = 96u) noexcept
        {
            return !value.empty() && value.size() <= maximum
                && std::all_of(value.begin(), value.end(), [](const char c)
                {
                    const auto byte = static_cast<unsigned char>(c);
                    return std::isalnum(byte) != 0
                        || c == '-' || c == '_' || c == '.' || c == ':';
                });
        }

        [[nodiscard]] bool bounded_text(
            const std::string_view value,
            const std::size_t maximum,
            const bool allow_empty = false) noexcept
        {
            if ((!allow_empty && value.empty()) || value.size() > maximum)
                return false;
            for (const unsigned char byte : value)
            {
                if (byte == 0u || byte == 0x7fu
                    || (byte < 0x20u && byte != '\t' && byte != '\n'))
                    return false;
            }
            return true;
        }

        [[nodiscard]] std::string json_string(const std::string_view value)
        {
            constexpr char digits[] = "0123456789abcdef";
            std::string output{"\""};
            output.reserve(value.size() + 2u);
            for (const unsigned char byte : value)
            {
                switch (byte)
                {
                case '\"': output += "\\\""; break;
                case '\\': output += "\\\\"; break;
                case '\b': output += "\\b"; break;
                case '\f': output += "\\f"; break;
                case '\n': output += "\\n"; break;
                case '\r': output += "\\r"; break;
                case '\t': output += "\\t"; break;
                default:
                    if (byte < 0x20u)
                    {
                        output += "\\u00";
                        output.push_back(digits[(byte >> 4u) & 0x0fu]);
                        output.push_back(digits[byte & 0x0fu]);
                    }
                    else
                    {
                        output.push_back(static_cast<char>(byte));
                    }
                    break;
                }
            }
            output.push_back('\"');
            return output;
        }

        [[nodiscard]] std::string digest_text(const std::string_view value)
        {
            return core::sha256::hex(core::sha256::hash(value));
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

        [[nodiscard]] bool safe_relative_path(
            const std::string_view relative_path)
        {
            if (relative_path.empty() || relative_path.size() > 512u
                || relative_path.find('\\') != std::string_view::npos)
                return false;
            const std::filesystem::path path{std::string{relative_path}};
            if (path.is_absolute() || path.has_root_path()
                || path.lexically_normal().generic_string() != relative_path)
                return false;
            return std::none_of(path.begin(), path.end(), [](const auto& part)
            {
                return part == ".." || part == ".";
            });
        }

        [[nodiscard]] bool valid_authority(
            const iteration_session::SourceAuthority& source)
        {
            if (!source.verified || !source.root.is_absolute()
                || source.root.lexically_normal() != source.root)
                return false;
            if (source.target_kind
                == iteration_session::IterationTargetKind::engine_source)
            {
                return (source.kind
                        == iteration_session::SourceAuthorityKind::explicit_checkout
                    || source.kind
                        == iteration_session::SourceAuthorityKind::verified_cache)
                    && bounded_text(source.source_version, 32u)
                    && lowercase_hex(source.commit, 40u)
                    && lowercase_hex(source.receipt_digest, 64u)
                    && source.project_id.empty()
                    && source.project_manifest_digest.empty()
                    && source.project_profile_digest.empty();
            }
            return source.kind
                    == iteration_session::SourceAuthorityKind::verified_project
                && source.source_version.empty()
                && source.commit.empty()
                && source.receipt_digest.empty()
                && identifier(source.project_id)
                && lowercase_hex(source.project_manifest_digest, 64u)
                && lowercase_hex(source.project_profile_digest, 64u);
        }

        [[nodiscard]] bool valid_curated_files(
            const std::vector<iteration_session::CuratedFile>& files)
        {
            if (files.empty() || files.size() > iteration_session::kMaximumCuratedFiles)
                return false;
            std::vector<std::string> paths{};
            for (const auto& file : files)
            {
                if (!safe_relative_path(file.relative_path)
                    || !lowercase_hex(file.sha256, 64u)
                    || file.byte_count > iteration_session::kMaximumCuratedFileBytes
                    || std::find(paths.begin(), paths.end(), file.relative_path)
                        != paths.end())
                    return false;
                paths.push_back(file.relative_path);
            }
            return true;
        }

        [[nodiscard]] bool valid_target(
            const ResolvedTargetHandle& target,
            const std::uint64_t host_generation)
        {
            return identifier(target.handle_id)
                && target.available
                && target.host_generation == host_generation
                && valid_authority(target.authority)
                && valid_curated_files(target.curated_files)
                && target.cache_root.is_absolute()
                && target.cache_root.lexically_normal() == target.cache_root;
        }

        [[nodiscard]] bool valid_budgets(
            const iteration_campaign::CampaignBudgets& value,
            const iteration_session::IterationTargetKind target_kind)
        {
            const auto maximum = iteration_campaign::default_budgets(target_kind);
            return value.maximum_candidates > 0u
                && value.maximum_candidates <= maximum.maximum_candidates
                && value.maximum_model_calls > 0u
                && value.maximum_model_calls <= maximum.maximum_model_calls
                && value.maximum_source_operations > 0u
                && value.maximum_source_operations
                    <= maximum.maximum_source_operations
                && value.maximum_repairs_per_candidate > 0u
                && value.maximum_repairs_per_candidate
                    <= maximum.maximum_repairs_per_candidate
                && value.maximum_changes_per_candidate > 0u
                && value.maximum_changes_per_candidate
                    <= maximum.maximum_changes_per_candidate
                && value.maximum_curated_files > 0u
                && value.maximum_curated_files <= maximum.maximum_curated_files
                && value.maximum_context_bytes > 0u
                && value.maximum_context_bytes <= maximum.maximum_context_bytes
                && value.maximum_candidate_bytes > 0u
                && value.maximum_candidate_bytes
                    <= maximum.maximum_candidate_bytes
                && value.maximum_file_bytes > 0u
                && value.maximum_file_bytes <= maximum.maximum_file_bytes
                && value.maximum_transaction_bytes > 0u
                && value.maximum_transaction_bytes
                    <= maximum.maximum_transaction_bytes
                && value.maximum_workspace_files > 0u
                && value.maximum_workspace_files
                    <= maximum.maximum_workspace_files
                && value.maximum_workspace_bytes > 0u
                && value.maximum_workspace_bytes
                    <= maximum.maximum_workspace_bytes
                && value.maximum_validation_records > 0u
                && value.maximum_validation_records
                    <= maximum.maximum_validation_records;
        }

        [[nodiscard]] bool valid_campaign(
            const CampaignSnapshot& campaign,
            const ResolvedTargetHandle& target,
            const HostSnapshot& host)
        {
            const auto& report = campaign.report;
            return campaign.available
                && campaign.host_generation == host.generation
                && campaign.target_handle_id == target.handle_id
                && lowercase_hex(report.campaign_id, 64u)
                && lowercase_hex(report.target_key, 64u)
                && lowercase_hex(report.state_digest, 64u)
                && report.target_key
                    == digest_text(authority_fingerprint(report.session.source))
                && report.session.source == target.authority
                && report.session.curated_files == target.curated_files
                && report.session.identity.valid()
                && bounded_text(report.session.objective, 4096u)
                && lowercase_hex(report.session.objective_digest, 64u)
                && bounded_text(report.session.model_name, 256u)
                && report.created_at_unix_seconds > 0u
                && report.expires_at_unix_seconds
                    > report.created_at_unix_seconds
                && valid_budgets(
                    report.budgets, report.session.source.target_kind)
                && report.counters.candidates
                    <= report.budgets.maximum_candidates
                && report.counters.model_calls
                    <= report.budgets.maximum_model_calls
                && report.counters.source_operations
                    <= report.budgets.maximum_source_operations
                && report.counters.validation_records
                    <= report.budgets.maximum_validation_records
                && report.session.validation.size()
                    <= report.budgets.maximum_validation_records
                && bounded_text(report.status, 4096u, true);
        }

        [[nodiscard]] const ResolvedTargetHandle* find_target(
            const HostSnapshot& host,
            const std::string_view id)
        {
            const auto found = std::find_if(
                host.targets.begin(), host.targets.end(),
                [id](const auto& value) { return value.handle_id == id; });
            return found == host.targets.end() ? nullptr : &*found;
        }

        [[nodiscard]] const CampaignSnapshot* find_campaign(
            const HostSnapshot& host,
            const std::string_view id)
        {
            const auto found = std::find_if(
                host.campaigns.begin(), host.campaigns.end(),
                [id](const auto& value)
                {
                    return value.report.campaign_id == id;
                });
            return found == host.campaigns.end() ? nullptr : &*found;
        }

        [[nodiscard]] bool valid_host_shape(
            const HostSnapshot& host,
            const AdapterLimits& limits)
        {
            if (!identifier(host.snapshot_id)
                || host.generation == 0u || host.now_unix_seconds == 0u
                || host.targets.size() > limits.maximum_target_handles
                || host.campaigns.size() > limits.maximum_campaigns)
                return false;
            for (std::size_t i = 0u; i < host.targets.size(); ++i)
            {
                if (!identifier(host.targets[i].handle_id))
                    return false;
                for (std::size_t j = i + 1u; j < host.targets.size(); ++j)
                    if (host.targets[i].handle_id == host.targets[j].handle_id)
                        return false;
            }
            for (std::size_t i = 0u; i < host.campaigns.size(); ++i)
            {
                const auto& id = host.campaigns[i].report.campaign_id;
                if (!lowercase_hex(id, 64u))
                    return false;
                for (std::size_t j = i + 1u; j < host.campaigns.size(); ++j)
                    if (id == host.campaigns[j].report.campaign_id)
                        return false;
            }
            return true;
        }

        [[nodiscard]] bool exact_arguments(
            const McpToolCall& call,
            const std::initializer_list<std::string_view> names)
        {
            if (call.arguments.size() != names.size())
                return false;
            for (const auto name : names)
                if (std::count_if(
                        call.arguments.begin(), call.arguments.end(),
                        [name](const McpArgument& value)
                        {
                            return value.name == name;
                        }) != 1)
                    return false;
            return std::all_of(
                call.arguments.begin(), call.arguments.end(),
                [names](const McpArgument& value)
                {
                    return std::find(names.begin(), names.end(), value.name)
                        != names.end();
                });
        }

        [[nodiscard]] std::string_view argument(
            const McpToolCall& call,
            const std::string_view name)
        {
            const auto found = std::find_if(
                call.arguments.begin(), call.arguments.end(),
                [name](const McpArgument& value) { return value.name == name; });
            return found == call.arguments.end()
                ? std::string_view{} : std::string_view{found->value};
        }

        [[nodiscard]] bool engine_capability_allowed(
            const McpSessionAuthority& authority,
            const iteration_session::SourceAuthority& source) noexcept
        {
            return source.target_kind
                    != iteration_session::IterationTargetKind::engine_source
                || has_capability(
                    authority.granted_capabilities,
                    McpToolCapability::engine_source);
        }

        [[nodiscard]] AdapterCode validation_code(
            const McpErrorCode error) noexcept
        {
            switch (error)
            {
            case McpErrorCode::none: return AdapterCode::ready;
            case McpErrorCode::unknown_tool: return AdapterCode::unknown_tool;
            case McpErrorCode::capability_denied:
                return AdapterCode::capability_denied;
            case McpErrorCode::approval_required:
                return AdapterCode::approval_required;
            case McpErrorCode::budget_exhausted:
                return AdapterCode::budget_exhausted;
            case McpErrorCode::cancelled: return AdapterCode::cancelled;
            default: return AdapterCode::invalid_request;
            }
        }

        [[nodiscard]] McpErrorCode adapter_error(
            const AdapterCode code) noexcept
        {
            switch (code)
            {
            case AdapterCode::ready:
            case AdapterCode::pending_host_action:
                return McpErrorCode::none;
            case AdapterCode::unknown_tool: return McpErrorCode::unknown_tool;
            case AdapterCode::capability_denied:
                return McpErrorCode::capability_denied;
            case AdapterCode::approval_required:
                return McpErrorCode::approval_required;
            case AdapterCode::cancelled: return McpErrorCode::cancelled;
            case AdapterCode::budget_exhausted:
                return McpErrorCode::budget_exhausted;
            case AdapterCode::target_not_found:
            case AdapterCode::campaign_not_found:
            case AdapterCode::stale_host_snapshot:
            case AdapterCode::evidence_missing:
                return McpErrorCode::evidence_missing;
            default: return McpErrorCode::invalid_request;
            }
        }

        [[nodiscard]] McpToolResult tool_result(
            const McpToolCall& call,
            const McpCallState state,
            const McpErrorCode error,
            std::string output,
            const AdapterLimits& limits,
            std::vector<McpEvidenceAttachment> evidence = {})
        {
            bool truncated{};
            if (output.size() > limits.maximum_response_bytes)
            {
                output = "{\"schema\":\"epoch.ai.mcp_campaign.v1\","
                    "\"status\":\"bounded_response_exceeded\"}";
                evidence.clear();
                truncated = true;
            }
            return McpToolResult{
                .session_id = call.session_id,
                .call_id = call.call_id,
                .state = state,
                .error = error,
                .output = std::move(output),
                .evidence = std::move(evidence),
                .output_truncated = truncated};
        }

        [[nodiscard]] std::string phase_name(
            const iteration_session::SessionPhase phase)
        {
            switch (phase)
            {
            case iteration_session::SessionPhase::idle: return "idle";
            case iteration_session::SessionPhase::selection_required:
                return "selection_required";
            case iteration_session::SessionPhase::awaiting_context_share:
                return "awaiting_context_share";
            case iteration_session::SessionPhase::awaiting_candidate:
                return "awaiting_candidate";
            case iteration_session::SessionPhase::awaiting_candidate_approval:
                return "awaiting_candidate_approval";
            case iteration_session::SessionPhase::executing_candidate:
                return "executing_candidate";
            case iteration_session::SessionPhase::validating_candidate:
                return "validating_candidate";
            case iteration_session::SessionPhase::awaiting_repair:
                return "awaiting_repair";
            case iteration_session::SessionPhase::candidate_verified:
                return "candidate_verified";
            case iteration_session::SessionPhase::cancelled: return "cancelled";
            case iteration_session::SessionPhase::blocked: return "blocked";
            }
            return "blocked";
        }

        [[nodiscard]] std::string validation_step_name(
            const iteration_campaign::ValidationStep step)
        {
            switch (step)
            {
            case iteration_campaign::ValidationStep::debug_compiler:
                return "debug_compiler";
            case iteration_campaign::ValidationStep::debug_contract:
                return "debug_contract";
            case iteration_campaign::ValidationStep::release_compiler:
                return "release_compiler";
            case iteration_campaign::ValidationStep::release_contract:
                return "release_contract";
            case iteration_campaign::ValidationStep::headless_compiler:
                return "headless_compiler";
            case iteration_campaign::ValidationStep::headless_contract:
                return "headless_contract";
            case iteration_campaign::ValidationStep::full_validation:
                return "full_validation";
            case iteration_campaign::ValidationStep::project_compiler:
                return "project_compiler";
            case iteration_campaign::ValidationStep::project_contract:
                return "project_contract";
            }
            return "debug_compiler";
        }
    }

    HostActionKind PendingHostAction::kind() const noexcept { return kind_; }
    const std::string& PendingHostAction::action_id() const noexcept
    {
        return action_id_;
    }
    const std::string& PendingHostAction::request_key() const noexcept
    {
        return request_key_;
    }
    const std::string& PendingHostAction::transport_session_id() const noexcept
    {
        return transport_session_id_;
    }
    const std::string& PendingHostAction::call_id() const noexcept
    {
        return call_id_;
    }
    const std::string& PendingHostAction::host_snapshot_id() const noexcept
    {
        return host_snapshot_id_;
    }
    std::uint64_t PendingHostAction::host_generation() const noexcept
    {
        return host_generation_;
    }
    const std::string& PendingHostAction::target_handle_id() const noexcept
    {
        return target_handle_id_;
    }
    const std::string& PendingHostAction::campaign_id() const noexcept
    {
        return campaign_id_;
    }
    iteration_session::RequestIdentity
    PendingHostAction::campaign_session_identity() const noexcept
    {
        return campaign_session_identity_;
    }
    const std::filesystem::path& PendingHostAction::cache_root() const noexcept
    {
        return cache_root_;
    }
    const iteration_campaign::CampaignConfiguration&
    PendingHostAction::start_configuration() const noexcept
    {
        return start_configuration_;
    }
    const iteration_campaign::CampaignReport&
    PendingHostAction::campaign_report() const noexcept
    {
        return campaign_report_;
    }
    const iteration_session::SourceAuthority&
    PendingHostAction::current_authority() const noexcept
    {
        return current_authority_;
    }
    const std::vector<iteration_session::CuratedFile>&
    PendingHostAction::current_curated_files() const noexcept
    {
        return current_curated_files_;
    }
    std::uint64_t PendingHostAction::now_unix_seconds() const noexcept
    {
        return now_unix_seconds_;
    }
    const std::string& PendingHostAction::reason() const noexcept
    {
        return reason_;
    }
    iteration_campaign::ValidationStep
    PendingHostAction::validation_step() const noexcept
    {
        return validation_step_;
    }
    const std::string& PendingHostAction::candidate_digest() const noexcept
    {
        return candidate_digest_;
    }
    std::uint32_t PendingHostAction::expected_validation_record_count()
        const noexcept
    {
        return expected_validation_record_count_;
    }

    McpToolRegistry make_tool_registry()
    {
        McpToolRegistry registry{};
        const auto add = [&registry](
            std::string name,
            std::string title,
            std::string description,
            std::string schema,
            const McpToolCapability capability,
            const McpToolRisk risk)
        {
            (void)registry.register_tool(McpToolDescriptor{
                .name = std::move(name),
                .title = std::move(title),
                .description = std::move(description),
                .input_schema_json = std::move(schema),
                .capabilities = capability,
                .risk = risk});
        };
        add(std::string{kInspectTargetTool}, "Inspect Campaign Target",
            "Inspect bounded authority evidence for one host-resolved target handle.",
            "{\"type\":\"object\",\"properties\":{\"target_handle\":{\"type\":\"string\"}},\"required\":[\"target_handle\"],\"additionalProperties\":false}",
            McpToolCapability::inspect, McpToolRisk::read_only);
        add(std::string{kStartTool}, "Start Iteration Campaign",
            "Request a bounded manual campaign against one host-resolved target.",
            "{\"type\":\"object\",\"properties\":{\"target_handle\":{\"type\":\"string\"},\"objective\":{\"type\":\"string\"},\"model\":{\"type\":\"string\"}},\"required\":[\"target_handle\",\"objective\",\"model\"],\"additionalProperties\":false}",
            McpToolCapability::author, McpToolRisk::workspace_write);
        add(std::string{kResumeTool}, "Resume Iteration Campaign",
            "Request fail-closed restoration of one host-resolved campaign.",
            "{\"type\":\"object\",\"properties\":{\"campaign_id\":{\"type\":\"string\"}},\"required\":[\"campaign_id\"],\"additionalProperties\":false}",
            McpToolCapability::author, McpToolRisk::workspace_write);
        add(std::string{kCancelTool}, "Cancel Iteration Campaign",
            "Request cancellation of one exact campaign identity.",
            "{\"type\":\"object\",\"properties\":{\"campaign_id\":{\"type\":\"string\"},\"reason\":{\"type\":\"string\"}},\"required\":[\"campaign_id\",\"reason\"],\"additionalProperties\":false}",
            McpToolCapability::author, McpToolRisk::workspace_write);
        add(std::string{kStatusTool}, "Inspect Campaign Status",
            "Read bounded campaign counters and digest-bound status evidence.",
            "{\"type\":\"object\",\"properties\":{\"campaign_id\":{\"type\":\"string\"}},\"required\":[\"campaign_id\"],\"additionalProperties\":false}",
            McpToolCapability::inspect, McpToolRisk::read_only);
        add(std::string{kValidateCandidateTool}, "Validate Campaign Candidate",
            "Request the next fixed host validation step for the active candidate.",
            "{\"type\":\"object\",\"properties\":{\"campaign_id\":{\"type\":\"string\"}},\"required\":[\"campaign_id\"],\"additionalProperties\":false}",
            McpToolCapability::build | McpToolCapability::capture,
            McpToolRisk::child_process);
        return registry;
    }

    CampaignAdapter::CampaignAdapter(
        std::string transport_session_id,
        const AdapterLimits limits)
        : transport_session_id_{std::move(transport_session_id)},
          limits_{limits}
    {
    }

    AdapterResult CampaignAdapter::dispatch(
        const mcp_stdio::RequestId& request_id,
        const McpToolCall& call,
        const McpSessionAuthority& authority,
        const HostSnapshot& host)
    {
        const auto reject = [&](const AdapterCode code, std::string status)
        {
            const McpCallState state = code == AdapterCode::cancelled
                ? McpCallState::cancelled : McpCallState::rejected;
            const std::string output =
                "{\"schema\":\"epoch.ai.mcp_campaign.v1\",\"code\":"
                + json_string(adapter_code_name(code)) + ",\"status\":"
                + json_string(status) + "}";
            return AdapterResult{
                .code = code,
                .result = tool_result(
                    call, state, adapter_error(code), output, limits_),
                .status = std::move(status)};
        };

        if (!limits_.valid() || !identifier(transport_session_id_)
            || !request_id.valid()
            || request_id.kind == mcp_stdio::RequestIdKind::absent
            || call.session_id != transport_session_id_
            || call.call_id != "rpc:" + request_id.key())
            return reject(AdapterCode::invalid_request,
                "Transport session, request, and call identities are not exactly bound.");

        const std::string call_key = call.session_id + "\n" + call.call_id;
        if (std::find(
                seen_call_ids_.begin(), seen_call_ids_.end(), call_key)
            != seen_call_ids_.end())
            return reject(AdapterCode::replay_denied,
                "This exact transport call identity was already consumed.");
        if (seen_call_ids_.size() >= limits_.maximum_seen_calls)
            return reject(AdapterCode::budget_exhausted,
                "The adapter call budget is exhausted.");
        seen_call_ids_.push_back(call_key);

        if (host.cancellation_requested || call.cancellation_requested
            || authority.cancellation_requested)
            return reject(AdapterCode::cancelled,
                "The transport, call, or host snapshot requested cancellation.");
        if (!valid_host_shape(host, limits_))
            return reject(AdapterCode::stale_host_snapshot,
                "The host snapshot is missing bounded identity or generation evidence.");

        McpToolRegistry registry = make_tool_registry();
        const McpValidation validation = registry.validate(call, authority);
        if (!validation)
            return reject(validation_code(validation.error), validation.message);

        const auto pending = [&](PendingHostAction action, std::string resource_key)
        {
            if (pending_bindings_.size() >= limits_.maximum_pending_actions)
                return reject(AdapterCode::budget_exhausted,
                    "The adapter pending-action budget is exhausted.");
            const auto duplicate = std::find_if(
                pending_bindings_.begin(), pending_bindings_.end(),
                [&](const PendingBinding& binding)
                {
                    return binding.resource_key == resource_key
                        && binding.host_generation == host.generation;
                });
            if (duplicate != pending_bindings_.end())
                return reject(AdapterCode::replay_denied,
                    "This host resource already has a pending action for the same snapshot generation.");
            pending_bindings_.push_back(
                {std::move(resource_key), host.generation});
            const std::string output =
                "{\"schema\":\"epoch.ai.mcp_campaign.v1\","
                "\"state\":\"pending_host_action\",\"action_id\":"
                + json_string(action.action_id()) + ",\"request_key\":"
                + json_string(action.request_key()) + ",\"call_id\":"
                + json_string(action.call_id()) + ",\"campaign_id\":"
                + json_string(action.campaign_id()) + ",\"campaign_session_id\":"
                + std::to_string(action.campaign_session_identity().session_id)
                + "}";
            return AdapterResult{
                .code = AdapterCode::pending_host_action,
                .result = tool_result(
                    call, McpCallState::proposed, McpErrorCode::none,
                    output, limits_),
                .pending_action = std::move(action),
                .status =
                    "The exact bounded operation awaits host ownership; the adapter executed nothing."};
        };

        const auto make_action = [&](const HostActionKind kind)
        {
            PendingHostAction action{};
            action.kind_ = kind;
            action.action_id_ = digest_text(
                host.snapshot_id + "\n" + std::to_string(host.generation)
                + "\n" + request_id.key() + "\n" + call.call_id);
            action.request_key_ = request_id.key();
            action.transport_session_id_ = call.session_id;
            action.call_id_ = call.call_id;
            action.host_snapshot_id_ = host.snapshot_id;
            action.host_generation_ = host.generation;
            action.now_unix_seconds_ = host.now_unix_seconds;
            return action;
        };

        if (call.tool == kInspectTargetTool)
        {
            if (!exact_arguments(call, {"target_handle"}))
                return reject(AdapterCode::invalid_request,
                    "Target inspection accepts only one host handle identifier.");
            const std::string_view target_id = argument(call, "target_handle");
            const auto* target = identifier(target_id)
                ? find_target(host, target_id) : nullptr;
            if (!target || !valid_target(*target, host.generation))
                return reject(AdapterCode::target_not_found,
                    "No current verified target matches that opaque host handle.");
            if (!engine_capability_allowed(authority, target->authority))
                return reject(AdapterCode::capability_denied,
                    "Engine-source target inspection requires explicit engine-source capability.");
            std::uint64_t context_bytes{};
            for (const auto& file : target->curated_files)
                context_bytes += file.byte_count;
            const std::string target_digest = digest_text(
                target->handle_id + "\n"
                + authority_fingerprint(target->authority));
            const std::string output =
                "{\"schema\":\"epoch.ai.mcp_campaign.v1\","
                "\"target_handle\":" + json_string(target->handle_id)
                + ",\"target_kind\":"
                + json_string(target->authority.target_kind
                    == iteration_session::IterationTargetKind::engine_source
                    ? "engine_source" : "project_source")
                + ",\"authority_sha256\":" + json_string(target_digest)
                + ",\"curated_files\":"
                + std::to_string(target->curated_files.size())
                + ",\"curated_bytes\":" + std::to_string(context_bytes)
                + ",\"host_generation\":"
                + std::to_string(host.generation) + "}";
            return AdapterResult{
                .code = AdapterCode::ready,
                .result = tool_result(
                    call, McpCallState::succeeded, McpErrorCode::none,
                    output, limits_,
                    {McpEvidenceAttachment{
                        .kind = "resolved_target_authority",
                        .content_hash = target_digest,
                        .summary =
                            "Host-resolved target authority and curated evidence metadata; no source bytes or paths disclosed."}}),
                .status = "Bounded target evidence returned."};
        }

        if (call.tool == kStartTool)
        {
            if (!exact_arguments(call, {"target_handle", "objective", "model"}))
                return reject(AdapterCode::invalid_request,
                    "Campaign start accepts only target_handle, objective, and model.");
            const std::string_view target_id = argument(call, "target_handle");
            const std::string_view objective = argument(call, "objective");
            const std::string_view model = argument(call, "model");
            const auto* target = identifier(target_id)
                ? find_target(host, target_id) : nullptr;
            if (!target || !valid_target(*target, host.generation))
                return reject(AdapterCode::target_not_found,
                    "No current verified target matches that opaque host handle.");
            if (!engine_capability_allowed(authority, target->authority))
                return reject(AdapterCode::capability_denied,
                    "Engine-source campaign start requires explicit engine-source capability.");
            if (!bounded_text(objective, limits_.maximum_objective_bytes)
                || !bounded_text(model, limits_.maximum_model_name_bytes))
                return reject(AdapterCode::invalid_request,
                    "Campaign objective or model binding exceeds its bounded text contract.");
            PendingHostAction action =
                make_action(HostActionKind::start_campaign);
            action.target_handle_id_ = target->handle_id;
            action.cache_root_ = target->cache_root;
            action.current_authority_ = target->authority;
            action.current_curated_files_ = target->curated_files;
            action.start_configuration_ =
                iteration_campaign::CampaignConfiguration{
                    .authority = target->authority,
                    .objective = std::string{objective},
                    .model_name = std::string{model},
                    .curated_files = target->curated_files,
                    .policy = iteration_session::CandidatePolicy::
                        manual_each_candidate,
                    .budgets = iteration_campaign::default_budgets(
                        target->authority.target_kind),
                    .created_at_unix_seconds = host.now_unix_seconds,
                    .duration_seconds = kCampaignDurationSeconds,
                    .auto_validation_permit = false};
            return pending(
                std::move(action), "target:" + target->handle_id);
        }

        if (call.tool != kResumeTool && call.tool != kCancelTool
            && call.tool != kStatusTool
            && call.tool != kValidateCandidateTool)
            return reject(AdapterCode::unknown_tool,
                "The MCP campaign adapter exposes no such tool.");

        const bool cancellation = call.tool == kCancelTool;
        if (!exact_arguments(
                call, cancellation
                    ? std::initializer_list<std::string_view>{
                        "campaign_id", "reason"}
                    : std::initializer_list<std::string_view>{"campaign_id"}))
            return reject(AdapterCode::invalid_request,
                "The campaign operation received unknown, duplicate, or missing arguments.");
        const std::string_view campaign_id = argument(call, "campaign_id");
        const auto* campaign = lowercase_hex(campaign_id, 64u)
            ? find_campaign(host, campaign_id) : nullptr;
        if (!campaign)
            return reject(AdapterCode::campaign_not_found,
                "No current host campaign matches that exact campaign identity.");
        const auto* target = find_target(host, campaign->target_handle_id);
        if (!target || !valid_target(*target, host.generation)
            || !valid_campaign(*campaign, *target, host))
            return reject(AdapterCode::evidence_missing,
                "Campaign state is stale or no longer matches its resolved target authority.");
        if (!engine_capability_allowed(authority, target->authority))
            return reject(AdapterCode::capability_denied,
                "Engine-source campaign access requires explicit engine-source capability.");

        const auto& report = campaign->report;
        if (call.tool == kStatusTool)
        {
            const std::string output =
                "{\"schema\":\"epoch.ai.mcp_campaign.v1\","
                "\"campaign_id\":" + json_string(report.campaign_id)
                + ",\"state_sha256\":" + json_string(report.state_digest)
                + ",\"target_key\":" + json_string(report.target_key)
                + ",\"campaign_session_id\":"
                + std::to_string(report.session.identity.session_id)
                + ",\"campaign_request_id\":"
                + std::to_string(report.session.identity.request_id)
                + ",\"phase\":" + json_string(phase_name(report.session.phase))
                + ",\"candidate_sha256\":"
                + json_string(report.session.candidate_digest)
                + ",\"candidates\":"
                + std::to_string(report.counters.candidates)
                + ",\"model_calls\":"
                + std::to_string(report.counters.model_calls)
                + ",\"source_operations\":"
                + std::to_string(report.counters.source_operations)
                + ",\"validation_records\":"
                + std::to_string(report.counters.validation_records)
                + ",\"cancelled\":"
                + (report.cancelled ? "true" : "false") + "}";
            return AdapterResult{
                .code = AdapterCode::ready,
                .result = tool_result(
                    call, McpCallState::succeeded, McpErrorCode::none,
                    output, limits_,
                    {McpEvidenceAttachment{
                        .kind = "campaign_state",
                        .content_hash = report.state_digest,
                        .summary =
                            "Bounded durable campaign status; no source, candidate, command, token, or path bytes disclosed."}}),
                .status = "Bounded campaign status returned."};
        }

        if (report.cancelled
            || report.session.phase == iteration_session::SessionPhase::cancelled)
            return reject(AdapterCode::cancelled,
                "The exact campaign is already cancelled.");
        if (host.now_unix_seconds < report.created_at_unix_seconds
            || host.now_unix_seconds > report.expires_at_unix_seconds)
            return reject(AdapterCode::stale_host_snapshot,
                "The campaign time authority is expired or moved backwards.");

        if (call.tool == kResumeTool)
        {
            if (report.counters.candidates >= report.budgets.maximum_candidates
                || report.counters.model_calls
                    >= report.budgets.maximum_model_calls
                || report.counters.source_operations
                    >= report.budgets.maximum_source_operations)
                return reject(AdapterCode::budget_exhausted,
                    "The campaign cannot resume after exhausting a work budget.");
            PendingHostAction action =
                make_action(HostActionKind::resume_campaign);
            action.target_handle_id_ = target->handle_id;
            action.campaign_id_ = report.campaign_id;
            action.campaign_session_identity_ = report.session.identity;
            action.cache_root_ = target->cache_root;
            action.campaign_report_ = report;
            action.current_authority_ = target->authority;
            action.current_curated_files_ = target->curated_files;
            return pending(
                std::move(action), "campaign:" + report.campaign_id);
        }

        if (call.tool == kCancelTool)
        {
            const std::string_view reason = argument(call, "reason");
            if (!bounded_text(reason, limits_.maximum_reason_bytes))
                return reject(AdapterCode::invalid_request,
                    "Campaign cancellation requires one bounded visible reason.");
            PendingHostAction action =
                make_action(HostActionKind::cancel_campaign);
            action.target_handle_id_ = target->handle_id;
            action.campaign_id_ = report.campaign_id;
            action.campaign_session_identity_ = report.session.identity;
            action.cache_root_ = target->cache_root;
            action.campaign_report_ = report;
            action.current_authority_ = target->authority;
            action.current_curated_files_ = target->curated_files;
            action.reason_ = std::string{reason};
            return pending(
                std::move(action), "campaign:" + report.campaign_id);
        }

        if (report.session.phase
                != iteration_session::SessionPhase::validating_candidate
            || !lowercase_hex(report.session.candidate_digest, 64u)
            || report.counters.validation_records
                != report.session.validation.size())
            return reject(AdapterCode::evidence_missing,
                "Candidate validation requires one digest-bound validating phase with synchronized evidence counters.");
        const auto plan = iteration_campaign::validation_plan(
            report.session.source.target_kind);
        const std::size_t next = report.session.validation.size();
        if (next >= plan.size()
            || report.counters.validation_records
                >= report.budgets.maximum_validation_records)
            return reject(AdapterCode::budget_exhausted,
                "The candidate validation plan or evidence budget is complete.");
        PendingHostAction action =
            make_action(HostActionKind::validate_candidate);
        action.target_handle_id_ = target->handle_id;
        action.campaign_id_ = report.campaign_id;
        action.campaign_session_identity_ = report.session.identity;
        action.cache_root_ = target->cache_root;
        action.campaign_report_ = report;
        action.current_authority_ = target->authority;
        action.current_curated_files_ = target->curated_files;
        action.validation_step_ = plan[next];
        action.candidate_digest_ = report.session.candidate_digest;
        action.expected_validation_record_count_ =
            report.counters.validation_records + 1u;
        AdapterResult result = pending(
            std::move(action), "campaign:" + report.campaign_id);
        if (result.pending_action)
        {
            result.result.output =
                "{\"schema\":\"epoch.ai.mcp_campaign.v1\","
                "\"state\":\"pending_host_action\",\"action_id\":"
                + json_string(result.pending_action->action_id())
                + ",\"campaign_id\":"
                + json_string(report.campaign_id)
                + ",\"campaign_session_id\":"
                + std::to_string(report.session.identity.session_id)
                + ",\"campaign_request_id\":"
                + std::to_string(report.session.identity.request_id)
                + ",\"candidate_sha256\":"
                + json_string(report.session.candidate_digest)
                + ",\"validation_step\":"
                + json_string(validation_step_name(
                    result.pending_action->validation_step()))
                + "}";
        }
        return result;
    }

    std::size_t CampaignAdapter::seen_call_count() const noexcept
    {
        return seen_call_ids_.size();
    }

    std::size_t CampaignAdapter::pending_action_count() const noexcept
    {
        return pending_bindings_.size();
    }
}
