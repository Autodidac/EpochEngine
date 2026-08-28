/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

module ai.iteration_campaign_queue;

namespace epochengine::ai::iteration_campaign_queue
{
    namespace
    {
        [[nodiscard]] HumanActionToken human_action(
            const QueueSnapshot& snapshot,
            std::uint64_t now,
            bool approved = true)
        {
            return HumanActionToken{
                snapshot.state_sha256,
                snapshot.configuration.human_authority_sha256,
                snapshot.generation,
                now,
                approved};
        }

        [[nodiscard]] ModelActionToken model_action(
            const QueueSnapshot& snapshot,
            std::uint64_t now)
        {
            return ModelActionToken{
                snapshot.state_sha256,
                snapshot.configuration.human_authority_sha256,
                snapshot.generation,
                now,
                true};
        }

        [[nodiscard]] CampaignReference campaign(
            char state_digit,
            std::uint64_t generation)
        {
            return CampaignReference{
                std::string(64u, 'a'),
                std::string(64u, 'b'),
                "cache/ai/iterations/campaign.epochai",
                std::string(64u, state_digit),
                generation};
        }

        [[nodiscard]] const WorkItem* item(
            const QueueSnapshot& snapshot,
            std::string_view objective_id)
        {
            const auto found = std::ranges::find_if(snapshot.items,
                [objective_id](const WorkItem& candidate)
                {
                    return candidate.request.objective_id == objective_id;
                });
            return found == snapshot.items.end() ? nullptr : &*found;
        }
    }

    bool run_contract()
    {
        constexpr std::uint64_t created = 2'000'000'000u;
        const QueueConfiguration configuration{
            "nightly-editor-campaign",
            std::string(64u, '1'),
            created,
            created + 6u * 60u * 60u,
            {8u, 32u, 32u, 2048u, 512u}};

        Queue queue{};
        if (!queue.begin(configuration)
            || queue.snapshot().generation != 1u
            || queue.snapshot().source_write_permitted
            || queue.snapshot().promotion_permitted
            || queue.snapshot().release_permitted
            || queue.snapshot().server_permitted
            || queue.snapshot().network_listener_permitted)
            return false;

        const auto initial_authority = human_action(queue.snapshot(), created + 1u);
        ObjectiveRequest low{
            "renderer-parity",
            "Close one bounded renderer capability gap with exact native evidence.",
            2u,
            campaign('c', 4u)};
        ObjectiveRequest high{
            "mcp-receipts",
            "Bind one transport-neutral MCP response to its approved objective.",
            6u,
            campaign('d', 7u)};
        if (!queue.enqueue(initial_authority, low))
            return false;

        const auto stale_authority = initial_authority;
        if (queue.enqueue(stale_authority, high).status != Status::stale_state
            || !queue.enqueue(human_action(queue.snapshot(), created + 2u), high)
            || queue.enqueue(human_action(queue.snapshot(), created + 3u), high).status
                != Status::duplicate_objective)
            return false;

        const Result issued = queue.issue_next(
            model_action(queue.snapshot(), created + 3u),
            "request-0001",
            std::string(64u, '2'),
            std::string(64u, '3'));
        if (!issued || issued.objective_id != high.objective_id
            || issued.request.request_sha256.size() != 64u
            || queue.issue_next(model_action(queue.snapshot(), created + 4u),
                    "request-0001", std::string(64u, '2'),
                    std::string(64u, '3')).status != Status::replay_refused)
            return false;

        ResponseReceipt response = make_response_receipt(
            issued.request,
            "receipt-0001",
            std::string(64u, '4'),
            created + 5u);
        ResponseReceipt altered = response;
        altered.request_sha256 = std::string(64u, '5');
        if (queue.record_response(altered).status != Status::invalid_receipt
            || !queue.record_response(response)
            || queue.record_response(response).status != Status::replay_refused)
            return false;

        auto wrong_actor = human_action(queue.snapshot(), created + 6u);
        wrong_actor.actor_sha256 = std::string(64u, '9');
        if (queue.review_response(wrong_actor, high.objective_id,
                HumanDisposition::approve_for_campaign).status
                != Status::invalid_authority
            || !queue.review_response(
                human_action(queue.snapshot(), created + 6u), high.objective_id,
                HumanDisposition::approve_for_campaign))
            return false;

        CampaignReference stale_campaign = high.campaign;
        stale_campaign.state_sha256 = std::string(64u, 'e');
        if (queue.advance_campaign_checkpoint(
                human_action(queue.snapshot(), created + 7u), high.objective_id,
                stale_campaign, false).status != Status::invalid_checkpoint)
            return false;
        CampaignReference advanced_campaign = campaign('e', 8u);
        if (!queue.advance_campaign_checkpoint(
                human_action(queue.snapshot(), created + 7u), high.objective_id,
                advanced_campaign, false))
            return false;

        const Checkpoint saved = queue.checkpoint();
        if (!saved || saved.bytes.size() < 256u)
            return false;
        Queue restored{};
        if (!restored.restore(saved.bytes, created + 8u)
            || restored.snapshot() != queue.snapshot()
            || restored.issue_next(model_action(restored.snapshot(), created + 8u),
                    "request-0001", std::string(64u, '2'),
                    std::string(64u, '3')).status != Status::replay_refused)
            return false;

        std::vector<std::byte> tampered = saved.bytes;
        tampered[tampered.size() / 2u] ^= std::byte{0x01u};
        Queue refused{};
        if (refused.restore(tampered, created + 8u).status
                != Status::invalid_checkpoint
            || !refused.snapshot().state_sha256.empty())
            return false;

        const Result reissued = restored.issue_next(
            model_action(restored.snapshot(), created + 9u),
            "request-0002",
            std::string(64u, '6'),
            std::string(64u, '7'));
        if (!reissued || reissued.objective_id != high.objective_id)
            return false;
        const ResponseReceipt final_response = make_response_receipt(
            reissued.request,
            "receipt-0002",
            std::string(64u, '8'),
            created + 10u);
        if (!restored.record_response(final_response)
            || !restored.review_response(
                human_action(restored.snapshot(), created + 11u),
                high.objective_id,
                HumanDisposition::approve_for_campaign)
            || !restored.advance_campaign_checkpoint(
                human_action(restored.snapshot(), created + 12u),
                high.objective_id, campaign('f', 9u), true))
            return false;

        const WorkItem* finished = item(restored.snapshot(), high.objective_id);
        const WorkItem* pending = item(restored.snapshot(), low.objective_id);
        return finished && finished->phase == ItemPhase::completed
            && finished->model_request.request_id.empty()
            && finished->model_response.receipt_id.empty()
            && pending && pending->phase == ItemPhase::queued
            && restored.snapshot().seen_request_ids.size() == 2u
            && restored.snapshot().seen_response_ids.size() == 2u
            && objective_digest(finished->request) == finished->objective_sha256
            && !restored.snapshot().source_write_permitted
            && !restored.snapshot().promotion_permitted
            && !restored.snapshot().release_permitted
            && !restored.snapshot().server_permitted
            && !restored.snapshot().network_listener_permitted;
    }
}

#if defined(EPOCH_AI_ITERATION_CAMPAIGN_QUEUE_CONTRACT_MAIN)
int main()
{
    return epochengine::ai::iteration_campaign_queue::run_contract() ? 0 : 1;
}
#endif
