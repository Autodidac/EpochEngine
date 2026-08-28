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

export module ai.iteration_supervisor_control;

export import ai.iteration_campaign_scheduler;

export namespace epochengine::ai::iteration_supervisor_control
{
    inline constexpr std::string_view command_schema{
        "epoch-ai-supervisor-command/v1"};
    inline constexpr std::string_view checkpoint_schema{
        "epoch-ai-supervisor-checkpoint/v1"};

    struct Limits final
    {
        std::uint32_t maximum_query_items{32u};
        std::uint32_t maximum_receipts{256u};
        std::uint32_t maximum_checkpoint_bytes{512u * 1024u};

        friend bool operator==(const Limits&, const Limits&) = default;
    };

    struct Configuration final
    {
        std::string control_id{};
        std::string session_id{};
        std::string actor_sha256{};
        std::string campaign_id{};
        std::uint64_t created_at_unix_seconds{};
        std::uint64_t expires_at_unix_seconds{};
        Limits limits{};

        friend bool operator==(const Configuration&, const Configuration&) = default;
    };

    enum class ControlPhase : std::uint8_t
    {
        active,
        paused,
        reviewed,
        cancelled
    };

    enum class CommandKind : std::uint8_t
    {
        pause,
        resume,
        cancel,
        retry,
        approve,
        reject,
        synchronize
    };

    struct Command final
    {
        std::string command_id{};
        CommandKind kind{CommandKind::pause};
        std::string control_id{};
        std::string session_id{};
        std::string actor_sha256{};
        std::string campaign_id{};
        std::string objective_id{};
        std::string operation_id{};
        std::uint64_t expected_control_generation{};
        std::string expected_control_state_sha256{};
        std::uint64_t expected_queue_generation{};
        std::string expected_queue_state_sha256{};
        std::uint64_t expected_scheduler_generation{};
        std::string expected_scheduler_state_sha256{};
        std::uint64_t now_unix_seconds{};
        bool operator_approved{};
        bool source_write_requested{};
        bool arbitrary_file_read_requested{};
        bool model_launch_requested{};
        bool promotion_requested{};
        bool release_requested{};
        bool server_requested{};
        bool network_listener_requested{};

        friend bool operator==(const Command&, const Command&) = default;
    };

    struct ItemSummary final
    {
        std::string objective_id{};
        std::string objective_sha256{};
        std::string campaign_id{};
        std::string campaign_state_sha256{};
        std::uint64_t campaign_generation{};
        iteration_campaign_queue::ItemPhase phase{
            iteration_campaign_queue::ItemPhase::queued};
        std::string request_id{};
        std::string request_sha256{};
        std::string response_receipt_id{};
        std::string response_sha256{};

        friend bool operator==(const ItemSummary&, const ItemSummary&) = default;
    };

    struct QuerySnapshot final
    {
        std::string control_id{};
        std::string session_id{};
        std::string campaign_id{};
        std::uint64_t control_generation{};
        std::string control_state_sha256{};
        ControlPhase control_phase{ControlPhase::active};
        std::uint64_t queue_generation{};
        std::string queue_state_sha256{};
        std::uint64_t scheduler_generation{};
        std::string scheduler_state_sha256{};
        iteration_campaign_scheduler::Phase scheduler_phase{
            iteration_campaign_scheduler::Phase::idle};
        std::string active_objective_id{};
        std::string active_operation_id{};
        std::uint64_t next_retry_at_unix_seconds{};
        std::vector<ItemSummary> items{};
        bool truncated{};
        bool source_write_permitted{};
        bool arbitrary_file_read_permitted{};
        bool model_launch_permitted{};
        bool promotion_permitted{};
        bool release_permitted{};
        bool server_permitted{};
        bool network_listener_permitted{};

        friend bool operator==(const QuerySnapshot&, const QuerySnapshot&) = default;
    };

    struct Snapshot final
    {
        Configuration configuration{};
        std::uint64_t generation{};
        std::string previous_state_sha256{};
        std::string state_sha256{};
        ControlPhase phase{ControlPhase::active};
        std::uint64_t queue_generation{};
        std::string queue_state_sha256{};
        std::uint64_t scheduler_generation{};
        std::string scheduler_state_sha256{};
        std::string active_objective_id{};
        std::string active_operation_id{};
        std::string last_receipt_sha256{};
        std::vector<std::string> seen_command_ids{};
        std::vector<std::string> seen_command_sha256{};
        bool source_write_permitted{};
        bool arbitrary_file_read_permitted{};
        bool model_launch_permitted{};
        bool promotion_permitted{};
        bool release_permitted{};
        bool server_permitted{};
        bool network_listener_permitted{};

        friend bool operator==(const Snapshot&, const Snapshot&) = default;
    };

    enum class Outcome : std::uint8_t
    {
        paused,
        resumed,
        cancelled,
        retry_authorized,
        approved,
        rejected,
        synchronized
    };

    struct Receipt final
    {
        std::string receipt_id{};
        std::string command_id{};
        std::string command_sha256{};
        std::string previous_receipt_sha256{};
        std::string control_state_before_sha256{};
        std::string queue_state_before_sha256{};
        std::string queue_state_after_sha256{};
        std::string scheduler_state_before_sha256{};
        std::string scheduler_state_after_sha256{};
        std::string campaign_id{};
        std::string objective_id{};
        std::string operation_id{};
        Outcome outcome{Outcome::paused};
        std::uint64_t completed_at_unix_seconds{};
        std::string transition_sha256{};
        std::string receipt_sha256{};
        bool source_write_permitted{};
        bool promotion_permitted{};
        bool release_permitted{};
        bool server_permitted{};
        bool network_listener_permitted{};

        friend bool operator==(const Receipt&, const Receipt&) = default;
    };

    enum class Code : std::uint8_t
    {
        ready,
        paused,
        resumed,
        cancelled,
        retry_authorized,
        approved,
        rejected,
        synchronized,
        invalid_configuration,
        malformed_command,
        noncanonical_command,
        invalid_authority,
        authority_broadening,
        stale_state,
        unknown_identifier,
        invalid_transition,
        replay_refused,
        receipt_limit_reached,
        invalid_checkpoint
    };

    struct Result final
    {
        Code code{Code::invalid_configuration};
        Snapshot snapshot{};
        Receipt receipt{};
        iteration_campaign_queue::Result queue{};
        iteration_campaign_scheduler::Result scheduler{};
        std::optional<iteration_campaign_scheduler::Authority> retry_authority{};
        std::string status{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == Code::ready || code == Code::paused
                || code == Code::resumed || code == Code::cancelled
                || code == Code::retry_authorized || code == Code::approved
                || code == Code::rejected || code == Code::synchronized;
        }
    };

    struct QueryResult final
    {
        Code code{Code::invalid_configuration};
        QuerySnapshot snapshot{};
        std::string status{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == Code::ready;
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

    class Surface final
    {
    public:
        [[nodiscard]] Result begin(
            const Configuration& configuration,
            const iteration_campaign_queue::QueueSnapshot& queue,
            const iteration_campaign_scheduler::Snapshot& scheduler);
        [[nodiscard]] QueryResult query(
            const iteration_campaign_queue::QueueSnapshot& queue,
            const iteration_campaign_scheduler::Snapshot& scheduler) const;
        [[nodiscard]] Result submit(
            std::span<const std::byte> canonical_command,
            iteration_campaign_queue::Queue& queue,
            iteration_campaign_scheduler::Scheduler& scheduler);
        [[nodiscard]] Result restore(
            std::span<const std::byte> checkpoint,
            std::uint64_t now_unix_seconds,
            const iteration_campaign_queue::QueueSnapshot& queue,
            const iteration_campaign_scheduler::Snapshot& scheduler);

        [[nodiscard]] Checkpoint checkpoint() const;
        [[nodiscard]] const Snapshot& snapshot() const noexcept;

    private:
        void observe(
            const iteration_campaign_queue::QueueSnapshot& queue,
            const iteration_campaign_scheduler::Snapshot& scheduler);
        [[nodiscard]] Receipt commit(
            const Command& command,
            std::string command_sha256,
            Outcome outcome,
            std::string queue_before,
            std::string scheduler_before);

        Snapshot snapshot_{};
        bool initialized_{};
    };

    [[nodiscard]] std::vector<std::byte> canonical_command(
        const Command& command);
    [[nodiscard]] bool run_contract();
}
