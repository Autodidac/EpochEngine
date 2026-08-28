/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

export module ai.mcp_campaign;

export import ai.mcp_stdio;
export import ai.iteration_campaign;

export namespace epochengine::ai::mcp_campaign
{
    inline constexpr std::string_view kInspectTargetTool =
        "campaign.inspect_target";
    inline constexpr std::string_view kStartTool = "campaign.start";
    inline constexpr std::string_view kResumeTool = "campaign.resume";
    inline constexpr std::string_view kCancelTool = "campaign.cancel";
    inline constexpr std::string_view kStatusTool = "campaign.status";
    inline constexpr std::string_view kValidateCandidateTool =
        "campaign.validate_candidate";

    struct AdapterLimits final
    {
        std::size_t maximum_seen_calls{128u};
        std::size_t maximum_pending_actions{32u};
        std::size_t maximum_target_handles{16u};
        std::size_t maximum_campaigns{16u};
        std::size_t maximum_objective_bytes{4096u};
        std::size_t maximum_model_name_bytes{256u};
        std::size_t maximum_reason_bytes{1024u};
        std::size_t maximum_response_bytes{32u * 1024u};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return maximum_seen_calls > 0u
                && maximum_pending_actions > 0u
                && maximum_pending_actions <= maximum_seen_calls
                && maximum_target_handles > 0u
                && maximum_campaigns > 0u
                && maximum_objective_bytes >= 32u
                && maximum_objective_bytes <= 4096u
                && maximum_model_name_bytes >= 16u
                && maximum_model_name_bytes <= 256u
                && maximum_reason_bytes >= 16u
                && maximum_reason_bytes <= 4096u
                && maximum_response_bytes >= 1024u
                && maximum_response_bytes <= 256u * 1024u;
        }
    };

    struct ResolvedTargetHandle final
    {
        std::string handle_id{};
        iteration_session::SourceAuthority authority{};
        std::vector<iteration_session::CuratedFile> curated_files{};
        std::filesystem::path cache_root{};
        std::uint64_t host_generation{};
        bool available{};
    };

    struct CampaignSnapshot final
    {
        std::string target_handle_id{};
        iteration_campaign::CampaignReport report{};
        std::uint64_t host_generation{};
        bool available{};
    };

    struct HostSnapshot final
    {
        std::string snapshot_id{};
        std::uint64_t generation{};
        std::uint64_t now_unix_seconds{};
        std::vector<ResolvedTargetHandle> targets{};
        std::vector<CampaignSnapshot> campaigns{};
        bool cancellation_requested{};
    };

    enum class AdapterCode : std::uint8_t
    {
        ready,
        pending_host_action,
        invalid_request,
        unknown_tool,
        capability_denied,
        approval_required,
        replay_denied,
        cancelled,
        budget_exhausted,
        target_not_found,
        campaign_not_found,
        stale_host_snapshot,
        evidence_missing
    };

    [[nodiscard]] constexpr std::string_view adapter_code_name(
        AdapterCode code) noexcept
    {
        switch (code)
        {
        case AdapterCode::ready: return "ready";
        case AdapterCode::pending_host_action: return "pending_host_action";
        case AdapterCode::invalid_request: return "invalid_request";
        case AdapterCode::unknown_tool: return "unknown_tool";
        case AdapterCode::capability_denied: return "capability_denied";
        case AdapterCode::approval_required: return "approval_required";
        case AdapterCode::replay_denied: return "replay_denied";
        case AdapterCode::cancelled: return "cancelled";
        case AdapterCode::budget_exhausted: return "budget_exhausted";
        case AdapterCode::target_not_found: return "target_not_found";
        case AdapterCode::campaign_not_found: return "campaign_not_found";
        case AdapterCode::stale_host_snapshot: return "stale_host_snapshot";
        case AdapterCode::evidence_missing: return "evidence_missing";
        }
        return "invalid_request";
    }

    enum class HostActionKind : std::uint8_t
    {
        start_campaign,
        resume_campaign,
        cancel_campaign,
        validate_candidate
    };

    class PendingHostAction final
    {
    public:
        [[nodiscard]] HostActionKind kind() const noexcept;
        [[nodiscard]] const std::string& action_id() const noexcept;
        [[nodiscard]] const std::string& request_key() const noexcept;
        [[nodiscard]] const std::string& transport_session_id() const noexcept;
        [[nodiscard]] const std::string& call_id() const noexcept;
        [[nodiscard]] const std::string& host_snapshot_id() const noexcept;
        [[nodiscard]] std::uint64_t host_generation() const noexcept;
        [[nodiscard]] const std::string& target_handle_id() const noexcept;
        [[nodiscard]] const std::string& campaign_id() const noexcept;
        [[nodiscard]] iteration_session::RequestIdentity
            campaign_session_identity() const noexcept;
        [[nodiscard]] const std::filesystem::path& cache_root() const noexcept;
        [[nodiscard]] const iteration_campaign::CampaignConfiguration&
            start_configuration() const noexcept;
        [[nodiscard]] const iteration_campaign::CampaignReport&
            campaign_report() const noexcept;
        [[nodiscard]] const iteration_session::SourceAuthority&
            current_authority() const noexcept;
        [[nodiscard]] const std::vector<iteration_session::CuratedFile>&
            current_curated_files() const noexcept;
        [[nodiscard]] std::uint64_t now_unix_seconds() const noexcept;
        [[nodiscard]] const std::string& reason() const noexcept;
        [[nodiscard]] iteration_campaign::ValidationStep
            validation_step() const noexcept;
        [[nodiscard]] const std::string& candidate_digest() const noexcept;
        [[nodiscard]] std::uint32_t expected_validation_record_count()
            const noexcept;

    private:
        friend class CampaignAdapter;

        HostActionKind kind_{HostActionKind::start_campaign};
        std::string action_id_{};
        std::string request_key_{};
        std::string transport_session_id_{};
        std::string call_id_{};
        std::string host_snapshot_id_{};
        std::uint64_t host_generation_{};
        std::string target_handle_id_{};
        std::string campaign_id_{};
        iteration_session::RequestIdentity campaign_session_identity_{};
        std::filesystem::path cache_root_{};
        iteration_campaign::CampaignConfiguration start_configuration_{};
        iteration_campaign::CampaignReport campaign_report_{};
        iteration_session::SourceAuthority current_authority_{};
        std::vector<iteration_session::CuratedFile> current_curated_files_{};
        std::uint64_t now_unix_seconds_{};
        std::string reason_{};
        iteration_campaign::ValidationStep validation_step_{
            iteration_campaign::ValidationStep::debug_compiler};
        std::string candidate_digest_{};
        std::uint32_t expected_validation_record_count_{};
    };

    struct AdapterResult final
    {
        AdapterCode code{AdapterCode::invalid_request};
        McpToolResult result{};
        std::optional<PendingHostAction> pending_action{};
        std::string status{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == AdapterCode::ready
                || code == AdapterCode::pending_host_action;
        }
    };

    [[nodiscard]] McpToolRegistry make_tool_registry();

    class CampaignAdapter final
    {
    public:
        explicit CampaignAdapter(
            std::string transport_session_id,
            AdapterLimits limits = {});

        [[nodiscard]] AdapterResult dispatch(
            const mcp_stdio::RequestId& request_id,
            const McpToolCall& call,
            const McpSessionAuthority& authority,
            const HostSnapshot& host);

        [[nodiscard]] std::size_t seen_call_count() const noexcept;
        [[nodiscard]] std::size_t pending_action_count() const noexcept;

    private:
        struct PendingBinding final
        {
            std::string resource_key{};
            std::uint64_t host_generation{};
        };

        std::string transport_session_id_{};
        AdapterLimits limits_{};
        std::vector<std::string> seen_call_ids_{};
        std::vector<PendingBinding> pending_bindings_{};
    };

    [[nodiscard]] bool run_contract();
}
