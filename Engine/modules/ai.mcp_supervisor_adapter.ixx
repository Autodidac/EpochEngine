/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

export module ai.mcp_supervisor_adapter;

export import ai.iteration_supervisor_control;
export import ai.curated_context_bundle;

export namespace epochengine::ai::mcp_supervisor_adapter
{
    inline constexpr std::uint32_t format_version{1u};
    inline constexpr std::string_view request_schema{
        "epoch.ai.mcp_supervisor_adapter.request/v1"};
    inline constexpr std::string_view response_schema{
        "epoch.ai.mcp_supervisor_adapter.response/v1"};

    enum class Method : std::uint8_t
    {
        campaign_snapshot,
        campaign_query,
        supervisor_pause,
        supervisor_resume,
        supervisor_cancel,
        supervisor_retry,
        supervisor_approve,
        supervisor_reject,
        curated_context_query,
        curated_context_evidence
    };

    [[nodiscard]] constexpr std::string_view method_name(Method value) noexcept
    {
        switch (value)
        {
        case Method::campaign_snapshot: return "epoch.campaign.snapshot";
        case Method::campaign_query: return "epoch.campaign.query";
        case Method::supervisor_pause: return "epoch.supervisor.pause";
        case Method::supervisor_resume: return "epoch.supervisor.resume";
        case Method::supervisor_cancel: return "epoch.supervisor.cancel";
        case Method::supervisor_retry: return "epoch.supervisor.retry";
        case Method::supervisor_approve: return "epoch.supervisor.approve";
        case Method::supervisor_reject: return "epoch.supervisor.reject";
        case Method::curated_context_query:
            return "epoch.curated_context.query";
        case Method::curated_context_evidence:
            return "epoch.curated_context.evidence";
        }
        return "unknown";
    }

    enum class Code : std::uint8_t
    {
        ready,
        parse_error,
        invalid_request,
        noncanonical_request,
        method_not_found,
        invalid_params,
        invalid_authority,
        stale_adapter_state,
        stale_host_state,
        replay_refused,
        budget_exceeded,
        host_rejected,
        curated_context_unavailable,
        checkpoint_malformed,
        checkpoint_integrity,
        checkpoint_cross_session,
        checkpoint_stale,
        checkpoint_noncanonical
    };

    [[nodiscard]] constexpr std::string_view code_name(Code value) noexcept
    {
        switch (value)
        {
        case Code::ready: return "ready";
        case Code::parse_error: return "parse_error";
        case Code::invalid_request: return "invalid_request";
        case Code::noncanonical_request: return "noncanonical_request";
        case Code::method_not_found: return "method_not_found";
        case Code::invalid_params: return "invalid_params";
        case Code::invalid_authority: return "invalid_authority";
        case Code::stale_adapter_state: return "stale_adapter_state";
        case Code::stale_host_state: return "stale_host_state";
        case Code::replay_refused: return "replay_refused";
        case Code::budget_exceeded: return "budget_exceeded";
        case Code::host_rejected: return "host_rejected";
        case Code::curated_context_unavailable:
            return "curated_context_unavailable";
        case Code::checkpoint_malformed: return "checkpoint_malformed";
        case Code::checkpoint_integrity: return "checkpoint_integrity";
        case Code::checkpoint_cross_session:
            return "checkpoint_cross_session";
        case Code::checkpoint_stale: return "checkpoint_stale";
        case Code::checkpoint_noncanonical: return "checkpoint_noncanonical";
        }
        return "unknown";
    }

    struct Limits final
    {
        std::uint32_t maximum_request_bytes{32u * 1024u};
        std::uint32_t maximum_response_bytes{128u * 1024u};
        std::uint32_t maximum_seen_requests{256u};
        std::uint32_t maximum_summary_bytes{4096u};
        std::uint32_t maximum_checkpoint_bytes{512u * 1024u};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return maximum_request_bytes >= 1024u
                && maximum_request_bytes <= 1024u * 1024u
                && maximum_response_bytes >= 4096u
                && maximum_response_bytes <= 4u * 1024u * 1024u
                && maximum_seen_requests > 0u
                && maximum_seen_requests <= 4096u
                && maximum_summary_bytes >= 256u
                && maximum_summary_bytes <= 64u * 1024u
                && maximum_checkpoint_bytes >= 4096u
                && maximum_checkpoint_bytes <= 8u * 1024u * 1024u;
        }

        friend constexpr bool operator==(const Limits&, const Limits&) = default;
    };

    struct Configuration final
    {
        std::string adapter_id{};
        std::string control_id{};
        std::string session_id{};
        std::string actor_sha256{};
        std::string campaign_id{};
        std::string project_id{};
        std::uint64_t curated_session_id{};
        std::uint64_t created_at_unix_seconds{};
        std::uint64_t expires_at_unix_seconds{};
        Limits limits{};

        friend bool operator==(
            const Configuration&,
            const Configuration&) = default;
    };

    struct Request final
    {
        std::string id{};
        Method method{Method::campaign_query};
        std::string session_id{};
        std::string actor_sha256{};
        std::string campaign_id{};
        std::string project_id{};
        std::string objective_id{};
        std::string operation_id{};
        std::uint64_t curated_session_id{};
        std::uint64_t expected_adapter_generation{};
        std::string expected_adapter_state_sha256{};
        std::uint64_t expected_control_generation{};
        std::string expected_control_state_sha256{};
        std::uint64_t expected_queue_generation{};
        std::string expected_queue_state_sha256{};
        std::uint64_t expected_scheduler_generation{};
        std::string expected_scheduler_state_sha256{};
        std::string curated_bundle_sha256{};
        std::uint64_t now_unix_seconds{};
        bool operator_approved{};

        friend bool operator==(const Request&, const Request&) = default;
    };

    using QueryFunction =
        iteration_supervisor_control::QueryResult (*)(void*);
    using SubmitFunction = iteration_supervisor_control::Result (*)(
        void*,
        std::span<const std::byte>);

    struct HostGateway final
    {
        void* context{};
        QueryFunction query{};
        SubmitFunction submit{};
        const curated_context_bundle::Bundle* curated_context{};
    };

    struct Snapshot final
    {
        Configuration configuration{};
        std::uint64_t generation{};
        std::string previous_state_sha256{};
        std::string state_sha256{};
        std::string last_receipt_sha256{};
        std::vector<std::string> seen_request_ids{};
        std::vector<std::string> seen_request_sha256{};
        bool source_bytes_exposed{};
        bool filesystem_read_permitted{};
        bool transport_permitted{};
        bool model_launch_permitted{};
        bool source_apply_permitted{};
        bool promotion_permitted{};
        bool release_permitted{};
        bool server_permitted{};
        bool network_listener_permitted{};

        friend bool operator==(const Snapshot&, const Snapshot&) = default;
    };

    struct Result final
    {
        Code code{Code::invalid_request};
        std::vector<std::uint8_t> canonical_response{};
        std::string request_sha256{};
        std::string response_sha256{};
        std::string receipt_sha256{};
        std::uint64_t adapter_generation{};
        std::string adapter_state_sha256{};
        std::string host_state_sha256{};
        std::string status{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == Code::ready;
        }
    };

    struct Checkpoint final
    {
        Code code{Code::checkpoint_malformed};
        std::vector<std::uint8_t> canonical_bytes{};
        std::string sha256{};
        std::string status{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == Code::ready;
        }
    };

    struct RestoreExpectation final
    {
        Configuration configuration{};
        std::uint64_t current_generation{};
        std::string checkpoint_sha256{};
    };

    struct RestoreResult final
    {
        Code code{Code::checkpoint_malformed};
        Snapshot snapshot{};
        std::string status{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == Code::ready;
        }
    };

    class Adapter final
    {
    public:
        [[nodiscard]] Result begin(const Configuration& configuration) noexcept;
        [[nodiscard]] Result dispatch(
            std::span<const std::uint8_t> canonical_request,
            const HostGateway& gateway) noexcept;
        [[nodiscard]] Checkpoint checkpoint() const noexcept;
        [[nodiscard]] RestoreResult restore(
            std::span<const std::uint8_t> canonical_checkpoint,
            const RestoreExpectation& expectation) noexcept;
        [[nodiscard]] const Snapshot& snapshot() const noexcept;

    private:
        Snapshot snapshot_{};
        bool initialized_{};
    };

    [[nodiscard]] std::vector<std::uint8_t> canonical_request(
        const Request& request);

    enum class ContractFailure : std::uint8_t
    {
        none,
        begin,
        campaign_snapshot,
        campaign_query,
        pause,
        resume,
        cancel,
        retry,
        approve,
        reject,
        curated_query,
        curated_evidence,
        malformed,
        noncanonical,
        unknown_method,
        invalid_authority,
        stale_adapter,
        stale_host,
        replay,
        request_budget,
        response_budget,
        host_rejection,
        curated_missing,
        curated_tamper,
        source_boundary,
        checkpoint_roundtrip,
        checkpoint_tamper,
        checkpoint_truncation,
        checkpoint_trailing,
        checkpoint_cross_session,
        checkpoint_stale,
        checkpoint_noncanonical
    };

    [[nodiscard]] constexpr std::string_view contract_failure_name(
        ContractFailure value) noexcept
    {
        switch (value)
        {
        case ContractFailure::none: return "none";
        case ContractFailure::begin: return "begin";
        case ContractFailure::campaign_snapshot: return "campaign_snapshot";
        case ContractFailure::campaign_query: return "campaign_query";
        case ContractFailure::pause: return "pause";
        case ContractFailure::resume: return "resume";
        case ContractFailure::cancel: return "cancel";
        case ContractFailure::retry: return "retry";
        case ContractFailure::approve: return "approve";
        case ContractFailure::reject: return "reject";
        case ContractFailure::curated_query: return "curated_query";
        case ContractFailure::curated_evidence: return "curated_evidence";
        case ContractFailure::malformed: return "malformed";
        case ContractFailure::noncanonical: return "noncanonical";
        case ContractFailure::unknown_method: return "unknown_method";
        case ContractFailure::invalid_authority: return "invalid_authority";
        case ContractFailure::stale_adapter: return "stale_adapter";
        case ContractFailure::stale_host: return "stale_host";
        case ContractFailure::replay: return "replay";
        case ContractFailure::request_budget: return "request_budget";
        case ContractFailure::response_budget: return "response_budget";
        case ContractFailure::host_rejection: return "host_rejection";
        case ContractFailure::curated_missing: return "curated_missing";
        case ContractFailure::curated_tamper: return "curated_tamper";
        case ContractFailure::source_boundary: return "source_boundary";
        case ContractFailure::checkpoint_roundtrip: return "checkpoint_roundtrip";
        case ContractFailure::checkpoint_tamper: return "checkpoint_tamper";
        case ContractFailure::checkpoint_truncation: return "checkpoint_truncation";
        case ContractFailure::checkpoint_trailing: return "checkpoint_trailing";
        case ContractFailure::checkpoint_cross_session: return "checkpoint_cross_session";
        case ContractFailure::checkpoint_stale: return "checkpoint_stale";
        case ContractFailure::checkpoint_noncanonical: return "checkpoint_noncanonical";
        }
        return "unknown";
    }

    [[nodiscard]] ContractFailure run_contract() noexcept;
}
