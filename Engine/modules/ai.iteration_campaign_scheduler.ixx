/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

export module ai.iteration_campaign_scheduler;

export import ai.iteration_campaign_queue;
export import ai.mcp_orchestrator_bridge;

export namespace epochengine::ai::iteration_campaign_scheduler
{
    inline constexpr std::string_view checkpoint_schema{
        "epoch-ai-campaign-scheduler/v1"};

    struct RetryPolicy final
    {
        std::uint32_t maximum_attempts{4u};
        std::uint64_t initial_backoff_seconds{2u};
        std::uint64_t maximum_backoff_seconds{30u};

        friend bool operator==(const RetryPolicy&, const RetryPolicy&) = default;
    };

    struct Configuration final
    {
        std::string scheduler_id{};
        std::string human_authority_sha256{};
        std::string queue_id{};
        std::string orchestrator_id{};
        std::string host_configuration_sha256{};
        self_iteration_orchestrator::TransportKind transport{
            self_iteration_orchestrator::TransportKind::guarded_local_mcp_child};
        std::uint64_t created_at_unix_seconds{};
        std::uint64_t expires_at_unix_seconds{};
        RetryPolicy retry{};

        friend bool operator==(const Configuration&, const Configuration&) = default;
    };

    enum class Phase : std::uint8_t
    {
        idle,
        backoff,
        awaiting_transport_approval,
        awaiting_transport_response,
        awaiting_human_review,
        blocked,
        cancelled
    };

    struct Snapshot final
    {
        Configuration configuration{};
        std::uint64_t generation{};
        std::string previous_state_sha256{};
        std::string state_sha256{};
        Phase phase{Phase::idle};
        std::uint64_t queue_generation{};
        std::string queue_state_sha256{};
        std::uint64_t orchestrator_generation{};
        std::string orchestrator_state_sha256{};
        std::string objective_id{};
        std::string objective_sha256{};
        std::string campaign_state_sha256{};
        iteration_campaign_queue::RequestReceipt request{};
        std::string pending_bridge_receipt_id{};
        std::string pending_operation_id{};
        std::string transport_binding_sha256{};
        std::uint32_t attempts{};
        std::uint64_t next_retry_at_unix_seconds{};
        std::string last_failure_sha256{};
        std::vector<std::string> seen_bridge_receipt_ids{};
        std::vector<std::string> seen_operation_ids{};
        bool source_write_permitted{};
        bool promotion_permitted{};
        bool release_permitted{};
        bool server_permitted{};
        bool network_listener_permitted{};

        friend bool operator==(const Snapshot&, const Snapshot&) = default;
    };

    struct Authority final
    {
        std::string actor_sha256{};
        std::string expected_state_sha256{};
        std::string expected_queue_state_sha256{};
        std::string expected_orchestrator_state_sha256{};
        std::uint64_t expected_generation{};
        std::uint64_t now_unix_seconds{};
        bool operator_permitted{};
    };

    enum class Code : std::uint8_t
    {
        ready,
        pending_human_approval,
        awaiting_response,
        awaiting_human_review,
        invalid_configuration,
        invalid_authority,
        stale_state,
        invalid_transition,
        transport_backoff,
        transport_blocked,
        replay_refused,
        cancelled,
        invalid_checkpoint
    };

    struct Result final
    {
        Code code{Code::invalid_configuration};
        Snapshot snapshot{};
        mcp_orchestrator_bridge::Result bridge{};
        std::optional<iteration_campaign_queue::RequestReceipt> request{};
        std::string status{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == Code::ready
                || code == Code::pending_human_approval
                || code == Code::awaiting_response
                || code == Code::awaiting_human_review;
        }
    };

    struct Checkpoint final
    {
        std::vector<std::byte> bytes{};
        std::string sha256{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return !bytes.empty() && sha256.size() == 64u;
        }
    };

    class Scheduler final
    {
    public:
        [[nodiscard]] Result begin(
            const Configuration& configuration,
            const iteration_campaign_queue::QueueSnapshot& queue,
            const self_iteration_orchestrator::Snapshot& orchestrator);
        [[nodiscard]] Result restore(
            std::span<const std::byte> bytes,
            std::uint64_t now_unix_seconds,
            const iteration_campaign_queue::QueueSnapshot& queue,
            const self_iteration_orchestrator::Snapshot& orchestrator);
        [[nodiscard]] Result dispatch_next(
            const Authority& authority,
            iteration_campaign_queue::Queue& queue,
            mcp_orchestrator_bridge::Bridge& bridge,
            self_iteration_orchestrator::Orchestrator& orchestrator);
        [[nodiscard]] Result approve_dispatch(
            const Authority& authority,
            const mcp_orchestrator_bridge::HostApproval& approval,
            iteration_campaign_queue::Queue& queue,
            mcp_orchestrator_bridge::Bridge& bridge,
            self_iteration_orchestrator::Orchestrator& orchestrator);
        [[nodiscard]] Result complete_response(
            const Authority& authority,
            const self_iteration_orchestrator::PendingOperation& operation,
            mcp_orchestrator_bridge::HostOperationResult response,
            iteration_campaign_queue::Queue& queue,
            mcp_orchestrator_bridge::Bridge& bridge,
            self_iteration_orchestrator::Orchestrator& orchestrator);
        [[nodiscard]] Result cancel(
            const Authority& authority,
            iteration_campaign_queue::Queue& queue);

        [[nodiscard]] Checkpoint checkpoint() const;
        [[nodiscard]] const Snapshot& snapshot() const noexcept;

    private:
        [[nodiscard]] Result validate(
            const Authority& authority,
            const iteration_campaign_queue::QueueSnapshot& queue,
            const self_iteration_orchestrator::Snapshot& orchestrator) const;
        [[nodiscard]] Result failure(
            Code code,
            std::string status,
            std::uint64_t now_unix_seconds,
            const iteration_campaign_queue::QueueSnapshot& queue,
            const self_iteration_orchestrator::Snapshot& orchestrator);
        void synchronize(
            const iteration_campaign_queue::QueueSnapshot& queue,
            const self_iteration_orchestrator::Snapshot& orchestrator);
        void commit_mutation();

        Snapshot snapshot_{};
        bool initialized_{};
    };

    [[nodiscard]] bool run_contract();
}
