/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

export module ai.mcp_orchestrator_bridge;

export import ai.mcp_stdio;
export import ai.mcp_campaign;
export import ai.mcp_child_host;
export import ai.self_iteration_orchestrator;

export namespace epochengine::ai::mcp_orchestrator_bridge
{
    struct Limits final
    {
        std::size_t maximum_seen_requests{128u};
        std::size_t maximum_pending_receipts{32u};
        std::size_t maximum_notifications{128u};
        std::size_t maximum_summary_bytes{2048u};
        std::size_t maximum_content_bytes{1024u * 1024u};
        std::size_t maximum_response_bytes{32u * 1024u};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return maximum_seen_requests >= 16u
                && maximum_seen_requests <= 256u
                && maximum_pending_receipts > 0u
                && maximum_pending_receipts <= 64u
                && maximum_notifications >= maximum_pending_receipts
                && maximum_notifications <= 256u
                && maximum_summary_bytes >= 64u
                && maximum_summary_bytes <= 4096u
                && maximum_content_bytes >= 4096u
                && maximum_content_bytes <= 2u * 1024u * 1024u
                && maximum_response_bytes >= 1024u
                && maximum_response_bytes <= 128u * 1024u;
        }
    };

    enum class ConnectionState : std::uint8_t
    {
        disconnected,
        connected,
        cancelled
    };

    enum class Code : std::uint8_t
    {
        ready,
        pending_host_approval,
        disconnected,
        invalid_request,
        capability_denied,
        approval_required,
        stale_state,
        replay_denied,
        out_of_order,
        cancelled,
        budget_exhausted,
        rejected
    };

    enum class MutationKind : std::uint8_t
    {
        request_plan,
        share_curated_evidence,
        request_proposal,
        review_proposal,
        decide_apply,
        request_validation,
        checkpoint,
        cancel
    };

    enum class NotificationKind : std::uint8_t
    {
        connected,
        disconnected,
        pending,
        progress,
        completed,
        rejected,
        cancelled
    };

    struct MutationIntent final
    {
        MutationKind kind{MutationKind::request_plan};
        std::string scope_sha256{};
        std::string evidence_sha256{};
        std::string summary{};
        bool approve{};
    };

    struct HostOperationResult final
    {
        std::string operation_id{};
        std::uint64_t expected_generation{};
        std::string expected_state_sha256{};
        std::string transition_id{};
        std::uint64_t now_unix_seconds{};
        std::string content{};
        std::string evidence_sha256{};
        std::string summary{};
        bool passed{};
        bool operator_approved{};
    };

    struct HostApproval final
    {
        std::string receipt_id{};
        std::uint64_t connection_generation{};
        std::uint64_t expected_orchestrator_generation{};
        std::string expected_state_sha256{};
        std::uint64_t now_unix_seconds{};
        bool operator_approved{};
    };

    struct Notification final
    {
        std::uint64_t sequence{};
        NotificationKind kind{NotificationKind::progress};
        std::string request_key{};
        std::string receipt_id{};
        std::string operation_id{};
        std::string campaign_id{};
        std::uint64_t campaign_generation{};
        std::string state_sha256{};
        std::string evidence_sha256{};
        std::string message{};
    };

    class PendingReceipt final
    {
    public:
        [[nodiscard]] const std::string& receipt_id() const noexcept;
        [[nodiscard]] MutationKind kind() const noexcept;
        [[nodiscard]] const std::string& transport_session_id() const noexcept;
        [[nodiscard]] const std::string& connection_id() const noexcept;
        [[nodiscard]] std::uint64_t connection_generation() const noexcept;
        [[nodiscard]] const std::string& request_key() const noexcept;
        [[nodiscard]] const std::string& call_id() const noexcept;
        [[nodiscard]] const std::string& orchestrator_id() const noexcept;
        [[nodiscard]] const std::string& campaign_id() const noexcept;
        [[nodiscard]] std::uint64_t campaign_generation() const noexcept;
        [[nodiscard]] iteration_session::RequestIdentity session_identity() const noexcept;
        [[nodiscard]] std::uint64_t expected_orchestrator_generation() const noexcept;
        [[nodiscard]] const std::string& expected_state_sha256() const noexcept;
        [[nodiscard]] const MutationIntent& intent() const noexcept;

    private:
        friend class Bridge;
        std::string receipt_id_{};
        MutationKind kind_{MutationKind::request_plan};
        std::string transport_session_id_{};
        std::string connection_id_{};
        std::uint64_t connection_generation_{};
        std::string request_key_{};
        std::string call_id_{};
        std::string orchestrator_id_{};
        std::string campaign_id_{};
        std::uint64_t campaign_generation_{};
        iteration_session::RequestIdentity session_identity_{};
        std::uint64_t expected_orchestrator_generation_{};
        std::string expected_state_sha256_{};
        MutationIntent intent_{};
    };

    struct Result final
    {
        Code code{Code::invalid_request};
        McpToolResult tool_result{};
        self_iteration_orchestrator::Snapshot snapshot{};
        std::optional<PendingReceipt> pending_receipt{};
        std::optional<self_iteration_orchestrator::PendingOperation> pending_operation{};
        std::string status{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == Code::ready || code == Code::pending_host_approval;
        }
    };

    class Bridge final
    {
    public:
        explicit Bridge(std::string transport_session_id, Limits limits = {});

        [[nodiscard]] Result connect(
            std::string connection_id,
            std::uint64_t connection_generation,
            const self_iteration_orchestrator::Snapshot& snapshot);
        [[nodiscard]] Result disconnect(
            std::string reason,
            const self_iteration_orchestrator::Snapshot& snapshot);
        [[nodiscard]] Result dispatch(
            const mcp_stdio::RequestId& request_id,
            const McpToolCall& call,
            const McpSessionAuthority& authority,
            self_iteration_orchestrator::Orchestrator& orchestrator,
            std::uint64_t now_unix_seconds);
        [[nodiscard]] Result propose_mutation(
            const mcp_stdio::RequestId& request_id,
            std::string call_id,
            MutationIntent intent,
            self_iteration_orchestrator::Orchestrator& orchestrator);
        [[nodiscard]] Result approve_receipt(
            const HostApproval& approval,
            self_iteration_orchestrator::Orchestrator& orchestrator);
        [[nodiscard]] Result complete_operation(
            const self_iteration_orchestrator::PendingOperation& operation,
            HostOperationResult result,
            self_iteration_orchestrator::Orchestrator& orchestrator);
        [[nodiscard]] std::vector<Notification> drain_notifications();
        [[nodiscard]] ConnectionState connection_state() const noexcept;
        [[nodiscard]] std::uint64_t connection_generation() const noexcept;

    private:
        struct PendingState final
        {
            PendingReceipt receipt{};
            bool consumed{};
        };

        [[nodiscard]] Result reject(
            Code code,
            const McpToolCall* call,
            self_iteration_orchestrator::Snapshot snapshot,
            std::string status);
        [[nodiscard]] Result make_pending(
            const mcp_stdio::RequestId& request_id,
            std::string call_id,
            MutationIntent intent,
            self_iteration_orchestrator::Orchestrator& orchestrator);
        void notify(
            NotificationKind kind,
            std::string request_key,
            std::string receipt_id,
            std::string operation_id,
            const self_iteration_orchestrator::Snapshot& snapshot,
            std::string evidence_sha256,
            std::string message);

        std::string transport_session_id_{};
        std::string connection_id_{};
        std::string bound_orchestrator_id_{};
        std::string last_state_sha256_{};
        std::uint64_t connection_generation_{};
        ConnectionState connection_state_{ConnectionState::disconnected};
        Limits limits_{};
        std::vector<std::string> seen_request_keys_{};
        std::vector<PendingState> pending_{};
        std::vector<std::string> completed_operation_ids_{};
        std::vector<Notification> notifications_{};
        std::uint64_t next_notification_sequence_{};
    };

    [[nodiscard]] bool run_contract();
}
