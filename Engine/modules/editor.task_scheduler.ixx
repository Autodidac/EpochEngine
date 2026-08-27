/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <coroutine>
#include <exception>
#include <future>
#include <functional>
#include <memory>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

export module editor.task_scheduler;

import systems.task;
export import taskgraph.dotsystem;

export namespace epochengine::editor_tasks
{
    struct SchedulerLimits final
    {
        std::size_t worker_count{2u};
        std::size_t retained_finished_nodes{256u};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return worker_count > 0u
                && worker_count <= taskgraph::TaskGraph::MaxWorkerCount
                && retained_finished_nodes > 0u
                && retained_finished_nodes
                    <= taskgraph::TaskGraph::MaxNodeCapacity;
        }
    };

    struct MaintenanceReport final
    {
        std::size_t nodes_before{};
        std::size_t nodes_after{};
        std::size_t outstanding{};
        std::uint64_t revision_before{};
        std::uint64_t revision_after{};
        bool pruned{};
    };

    class TaskCancelled final : public std::runtime_error
    {
    public:
        TaskCancelled()
            : std::runtime_error{"Editor task was cancelled."}
        {
        }
    };

    struct CancellationTicket final
    {
        std::uint64_t scheduler_id{};
        taskgraph::NodeHandle node{};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return scheduler_id != 0u && node.valid();
        }
    };

    template <class Result>
    struct Submission final
    {
        std::future<Result> completion{};
        CancellationTicket cancellation{};
    };

    namespace detail
    {
        template <class Result>
        struct CompletionState final
        {
            std::promise<Result> promise{};
            std::atomic_flag settled = ATOMIC_FLAG_INIT;
        };

        template <class Result>
        [[nodiscard]] bool claim_completion(
            const std::shared_ptr<CompletionState<Result>>& state) noexcept
        {
            return state
                && !state->settled.test_and_set(std::memory_order_acq_rel);
        }

        template <class Result>
        void settle_exception(
            const std::shared_ptr<CompletionState<Result>>& state,
            std::exception_ptr error) noexcept
        {
            if (!claim_completion(state))
                return;
            try
            {
                state->promise.set_exception(std::move(error));
            }
            catch (...)
            {
            }
        }

        template <class Result>
        void settle_cancelled(
            const std::shared_ptr<CompletionState<Result>>& state) noexcept
        {
            settle_exception<Result>(
                state,
                std::make_exception_ptr(TaskCancelled{}));
        }
    }

    class Scheduler final
    {
    public:
        explicit Scheduler(
            SchedulerLimits limits,
            taskgraph::MonotonicClock clock = nullptr)
            : limits_(limits),
              scheduler_id_(next_scheduler_id()),
              graph_(validated_worker_count(limits), clock)
        {
        }

        Scheduler(const Scheduler&) = delete;
        Scheduler& operator=(const Scheduler&) = delete;
        Scheduler(Scheduler&&) = delete;
        Scheduler& operator=(Scheduler&&) = delete;

        template <class Operation>
        [[nodiscard]] auto submit(
            std::string label,
            Operation&& operation)
            -> std::future<std::invoke_result_t<Operation>>
        {
            using Result = std::invoke_result_t<Operation>;
            auto submitted = submit_cancellable(
                std::move(label),
                [operation = std::forward<Operation>(operation)](
                    std::stop_token) mutable -> Result
                {
                    if constexpr (std::is_void_v<Result>)
                    {
                        std::invoke(operation);
                        return;
                    }
                    else
                    {
                        return std::invoke(operation);
                    }
                });
            return std::move(submitted.completion);
        }

        template <class Operation>
        [[nodiscard]] auto submit_cancellable(
            std::string label,
            Operation&& operation)
            -> Submission<std::invoke_result_t<Operation, std::stop_token>>
        {
            using Result = std::invoke_result_t<Operation, std::stop_token>;
            if (label.empty()
                || label.size() > taskgraph::TaskGraph::MaxDiagnosticLabelBytes)
            {
                throw std::invalid_argument{
                    "Editor task labels must be non-empty and bounded."};
            }

            const taskgraph::GraphSnapshot before = graph_.Snapshot(0u);
            if (!before.accepting)
            {
                throw std::runtime_error{
                    "Editor task scheduler is not accepting work."};
            }

            auto completion =
                std::make_shared<detail::CompletionState<Result>>();
            std::future<Result> future = completion->promise.get_future();
            auto operationState = std::make_shared<std::decay_t<Operation>>(
                std::forward<Operation>(operation));
            std::stop_source cancellation{};
            const std::stop_token token = cancellation.get_token();
            std::function<void()> invocation =
                [completion, operationState, token]() mutable
                {
                    if (token.stop_requested())
                    {
                        detail::settle_cancelled(completion);
                        return;
                    }
                    try
                    {
                        if constexpr (std::is_void_v<Result>)
                        {
                            std::invoke(*operationState, token);
                            if (detail::claim_completion(completion))
                                completion->promise.set_value();
                        }
                        else
                        {
                            Result result = std::invoke(*operationState, token);
                            if (detail::claim_completion(completion))
                                completion->promise.set_value(std::move(result));
                        }
                    }
                    catch (...)
                    {
                        detail::settle_exception<Result>(
                            completion,
                            std::current_exception());
                        throw;
                    }
                };
            std::function<void()> cancelledBeforeStart =
                [completion]
                {
                    detail::settle_cancelled(completion);
                };
            auto node = std::make_unique<taskgraph::Node>(
                run_operation(std::move(invocation)),
                cancellation,
                std::move(cancelledBeforeStart));
            node->Label = std::move(label);
            const taskgraph::NodeHandle handle = graph_.AddNode(std::move(node));
            if (!handle.valid())
            {
                detail::settle_exception<Result>(
                    completion,
                    std::make_exception_ptr(std::runtime_error{
                        "Editor task scheduler rejected the submitted task."}));
                throw std::runtime_error{
                    "Editor task scheduler rejected the submitted task."};
            }

            graph_.Execute();
            return {
                .completion = std::move(future),
                .cancellation = {
                    .scheduler_id = scheduler_id_,
                    .node = handle}};
        }

        [[nodiscard]] taskgraph::CancelCode cancel(
            CancellationTicket ticket)
        {
            if (!ticket.valid() || ticket.scheduler_id != scheduler_id_)
                return taskgraph::CancelCode::not_found;
            return graph_.Cancel(ticket.node);
        }

        [[nodiscard]] MaintenanceReport maintain()
        {
            const taskgraph::GraphSnapshot before = graph_.Snapshot(0u);
            MaintenanceReport report{
                .nodes_before = before.nodeCount,
                .nodes_after = before.nodeCount,
                .outstanding = before.outstandingCount,
                .revision_before = before.revision,
                .revision_after = before.revision};

            if (before.nodeCount > limits_.retained_finished_nodes
                && before.outstandingCount == 0u)
            {
                graph_.PruneFinished();
                const taskgraph::GraphSnapshot after = graph_.Snapshot(0u);
                report.nodes_after = after.nodeCount;
                report.revision_after = after.revision;
                report.pruned = after.nodeCount < before.nodeCount;
            }
            return report;
        }

        [[nodiscard]] taskgraph::GraphSnapshot snapshot(
            std::size_t maximum_nodes =
                taskgraph::TaskGraph::MaxSnapshotNodes) const
        {
            return graph_.Snapshot(maximum_nodes);
        }

        [[nodiscard]] taskgraph::TaskGraph& graph() noexcept
        {
            return graph_;
        }

        [[nodiscard]] const taskgraph::TaskGraph& graph() const noexcept
        {
            return graph_;
        }

        [[nodiscard]] const SchedulerLimits& limits() const noexcept
        {
            return limits_;
        }

    private:
        [[nodiscard]] static std::size_t validated_worker_count(
            const SchedulerLimits& limits)
        {
            if (!limits.valid())
            {
                throw std::invalid_argument{
                    "Editor task scheduler limits are invalid."};
            }
            return limits.worker_count;
        }

        [[nodiscard]] static std::uint64_t next_scheduler_id()
        {
            const std::uint64_t id = NextSchedulerId_.fetch_add(
                1u,
                std::memory_order_relaxed);
            if (id == 0u)
            {
                throw std::overflow_error{
                    "Editor task scheduler identity space was exhausted."};
            }
            return id;
        }

        static Task run_operation(std::function<void()> operation)
        {
            operation();
            co_return;
        }

        inline static std::atomic<std::uint64_t> NextSchedulerId_{1u};
        SchedulerLimits limits_{};
        std::uint64_t scheduler_id_{};
        taskgraph::TaskGraph graph_;
    };
}
