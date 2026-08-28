/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

export module ai.iteration_campaign_queue;

export namespace epochengine::ai::iteration_campaign_queue
{
    inline constexpr std::string_view checkpoint_schema{
        "epoch-ai-campaign-queue/v1"};

    struct QueueLimits final
    {
        std::uint32_t maximum_items{32u};
        std::uint32_t maximum_seen_requests{128u};
        std::uint32_t maximum_seen_responses{128u};
        std::uint32_t maximum_objective_bytes{4096u};
        std::uint32_t maximum_locator_bytes{1024u};

        friend bool operator==(const QueueLimits&, const QueueLimits&) = default;
    };

    struct QueueConfiguration final
    {
        std::string queue_id{};
        std::string human_authority_sha256{};
        std::uint64_t created_at_unix_seconds{};
        std::uint64_t expires_at_unix_seconds{};
        QueueLimits limits{};

        friend bool operator==(
            const QueueConfiguration&,
            const QueueConfiguration&) = default;
    };

    struct CampaignReference final
    {
        std::string campaign_id{};
        std::string target_key{};
        std::string state_locator{};
        std::string state_sha256{};
        std::uint64_t record_generation{};

        friend bool operator==(
            const CampaignReference&,
            const CampaignReference&) = default;
    };

    struct ObjectiveRequest final
    {
        std::string objective_id{};
        std::string objective{};
        std::uint8_t priority{};
        CampaignReference campaign{};

        friend bool operator==(
            const ObjectiveRequest&,
            const ObjectiveRequest&) = default;
    };

    enum class ItemPhase : std::uint8_t
    {
        queued,
        awaiting_response,
        awaiting_human_review,
        approved_for_campaign,
        completed,
        rejected,
        cancelled
    };

    struct RequestReceipt final
    {
        std::string request_id{};
        std::string request_sha256{};
        std::string binding_sha256{};
        std::string payload_sha256{};
        std::uint64_t issued_at_unix_seconds{};

        friend bool operator==(
            const RequestReceipt&,
            const RequestReceipt&) = default;
    };

    struct ResponseReceipt final
    {
        std::string receipt_id{};
        std::string request_id{};
        std::string request_sha256{};
        std::string binding_sha256{};
        std::string payload_sha256{};
        std::string response_sha256{};
        std::uint64_t received_at_unix_seconds{};

        friend bool operator==(
            const ResponseReceipt&,
            const ResponseReceipt&) = default;
    };

    struct WorkItem final
    {
        ObjectiveRequest request{};
        std::string objective_sha256{};
        ItemPhase phase{ItemPhase::queued};
        std::uint64_t insertion_sequence{};
        RequestReceipt model_request{};
        ResponseReceipt model_response{};

        friend bool operator==(const WorkItem&, const WorkItem&) = default;
    };

    struct HumanActionToken final
    {
        std::string expected_state_sha256{};
        std::string actor_sha256{};
        std::uint64_t expected_generation{};
        std::uint64_t now_unix_seconds{};
        bool operator_approved{};
    };

    struct ModelActionToken final
    {
        std::string expected_state_sha256{};
        std::string actor_sha256{};
        std::uint64_t expected_generation{};
        std::uint64_t now_unix_seconds{};
        bool model_call_permitted{};
    };

    enum class HumanDisposition : std::uint8_t
    {
        approve_for_campaign,
        reject,
        cancel
    };

    struct QueueSnapshot final
    {
        QueueConfiguration configuration{};
        std::uint64_t generation{};
        std::uint64_t next_insertion_sequence{};
        std::string previous_state_sha256{};
        std::string state_sha256{};
        std::vector<WorkItem> items{};
        std::vector<std::string> seen_request_ids{};
        std::vector<std::string> seen_request_sha256{};
        std::vector<std::string> seen_response_ids{};
        std::vector<std::string> seen_response_sha256{};
        bool source_write_permitted{};
        bool promotion_permitted{};
        bool release_permitted{};
        bool server_permitted{};
        bool network_listener_permitted{};

        friend bool operator==(
            const QueueSnapshot&,
            const QueueSnapshot&) = default;
    };

    enum class Status : std::uint8_t
    {
        ready,
        invalid_configuration,
        invalid_authority,
        stale_state,
        expired,
        limit_reached,
        invalid_objective,
        duplicate_objective,
        no_work,
        invalid_transition,
        invalid_receipt,
        replay_refused,
        invalid_checkpoint
    };

    struct Result final
    {
        Status status{Status::invalid_configuration};
        std::string objective_id{};
        std::string state_sha256{};
        RequestReceipt request{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return status == Status::ready;
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

    class Queue final
    {
    public:
        [[nodiscard]] Result begin(const QueueConfiguration& configuration);
        [[nodiscard]] Result restore(
            std::span<const std::byte> checkpoint_bytes,
            std::uint64_t now_unix_seconds);
        [[nodiscard]] Result enqueue(
            const HumanActionToken& authority,
            const ObjectiveRequest& request);
        [[nodiscard]] Result issue_next(
            const ModelActionToken& authority,
            std::string request_id,
            std::string transport_binding_sha256,
            std::string payload_sha256);
        [[nodiscard]] Result record_response(const ResponseReceipt& response);
        [[nodiscard]] Result review_response(
            const HumanActionToken& authority,
            std::string_view objective_id,
            HumanDisposition disposition);
        [[nodiscard]] Result advance_campaign_checkpoint(
            const HumanActionToken& authority,
            std::string_view objective_id,
            const CampaignReference& campaign,
            bool completed);
        [[nodiscard]] Result cancel(
            const HumanActionToken& authority,
            std::string_view objective_id);

        [[nodiscard]] Checkpoint checkpoint() const;
        [[nodiscard]] const QueueSnapshot& snapshot() const noexcept;

    private:
        [[nodiscard]] Result validate_human(
            const HumanActionToken& authority) const;
        [[nodiscard]] Result validate_model(
            const ModelActionToken& authority) const;
        [[nodiscard]] WorkItem* find_item(std::string_view objective_id);
        [[nodiscard]] const WorkItem* find_item(
            std::string_view objective_id) const;
        void commit_mutation();

        QueueSnapshot snapshot_{};
        bool initialized_{};
    };

    [[nodiscard]] std::string objective_digest(
        const ObjectiveRequest& request);
    [[nodiscard]] ResponseReceipt make_response_receipt(
        const RequestReceipt& request,
        std::string receipt_id,
        std::string payload_sha256,
        std::uint64_t received_at_unix_seconds);
    [[nodiscard]] bool run_contract();
}
