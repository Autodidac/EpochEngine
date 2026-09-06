/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
#include <algorithm>
#include <atomic>
#include <chrono>
#include <future>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <thread>

import editor.task_scheduler;
import taskgraph.dotsystem;

namespace epochengine::editor_tasks
{
    namespace
    {
        std::atomic<std::uint64_t> deterministicTime{100u};

        [[nodiscard]] std::uint64_t deterministic_clock() noexcept
        {
            return deterministicTime.fetch_add(10u, std::memory_order_relaxed);
        }

        [[nodiscard]] bool invalid_limits_contract()
        {
            try
            {
                Scheduler invalid{{
                    .worker_count = 0u,
                    .retained_finished_nodes = 1u}};
                (void)invalid;
                return false;
            }
            catch (const std::invalid_argument&)
            {
                return true;
            }
            catch (...)
            {
                return false;
            }
        }

        [[nodiscard]] bool submission_contract()
        {
            Scheduler scheduler{{
                .worker_count = 2u,
                .retained_finished_nodes = 1u}};

            auto first = scheduler.submit(
                "Compile project source",
                [] { return 41; });
            auto second = scheduler.submit(
                "Publish verified artifact",
                [] { return std::string{"ready"}; });
            if (first.get() != 41
                || second.get() != "ready")
            {
                return false;
            }

            scheduler.graph().WaitAll();
            const taskgraph::GraphSnapshot snapshot =
                scheduler.snapshot();
            if (snapshot.acceptedCount != 2u
                || snapshot.completedCount != 2u
                || snapshot.failedCount != 0u
                || snapshot.outstandingCount != 0u
                || snapshot.nodes.size() != 2u
                || snapshot.nodes[0u].label
                    != "Compile project source"
                || snapshot.nodes[1u].label
                    != "Publish verified artifact")
            {
                return false;
            }

            const MaintenanceReport maintenance =
                scheduler.maintain();
            const taskgraph::GraphSnapshot pruned =
                scheduler.snapshot();
            return maintenance.pruned
                && maintenance.nodes_before == 2u
                && maintenance.nodes_after == 0u
                && pruned.nodeCount == 0u
                && pruned.acceptedCount == 2u
                && pruned.completedCount == 2u;
        }

        [[nodiscard]] bool activity_accounting_contract()
        {
            Scheduler scheduler{{
                .worker_count = 1u,
                .retained_finished_nodes = 8u}};
            const auto idle = scheduler.activity();
            if (idle.worker_count != 1u || idle.idle_workers != 1u
                || idle.has_work() || idle.running_tasks != 0u
                || idle.queued_tasks != 0u || idle.waiting_tasks != 0u)
                return false;

            std::promise<void> startedPromise{};
            auto started = startedPromise.get_future();
            std::promise<void> releasePromise{};
            auto release = releasePromise.get_future().share();
            auto running = scheduler.submit(
                "Activity running worker", [&startedPromise, release]
                {
                    startedPromise.set_value();
                    release.wait();
                });
            started.wait();
            auto queued = scheduler.submit_cancellable(
                "Activity queued task", [](std::stop_token) {});
            const auto busy = scheduler.activity();
            const bool busyMatches = busy.worker_count == 1u
                && busy.idle_workers == 0u && busy.running_tasks == 1u
                && busy.queued_tasks == 1u && busy.waiting_tasks == 0u
                && busy.outstanding_tasks == 2u && busy.has_work();
            const auto cancellation = scheduler.cancel(queued.cancellation);
            const auto afterCancel = scheduler.activity();
            const bool cancellationMatches = cancellation == taskgraph::CancelCode::cancelled
                && afterCancel.running_tasks == 1u && afterCancel.queued_tasks == 0u
                && afterCancel.outstanding_tasks == 1u && afterCancel.idle_workers == 0u;
            // Always release the running task before an assertion can return:
            // scheduler destruction drains its owned jobs rather than abandoning them.
            releasePromise.set_value();
            running.get();
            scheduler.graph().WaitAll();
            const auto finished = scheduler.activity();
            return busyMatches && cancellationMatches
                && finished.worker_count == 1u && finished.idle_workers == 1u
                && finished.running_tasks == 0u && finished.queued_tasks == 0u
                && finished.waiting_tasks == 0u && !finished.has_work();
        }

        [[nodiscard]] bool future_exception_contract()
        {
            Scheduler scheduler{{
                .worker_count = 1u,
                .retained_finished_nodes = 4u}};
            auto future = scheduler.submit(
                "Failing project compiler",
                []() -> int
                {
                    throw std::runtime_error{"compile failed"};
                });
            try
            {
                (void)future.get();
                return false;
            }
            catch (const std::runtime_error& error)
            {
                if (std::string{error.what()} != "compile failed")
                    return false;
            }
            catch (...)
            {
                return false;
            }

            scheduler.graph().WaitAll();
            const taskgraph::GraphSnapshot snapshot =
                scheduler.snapshot();
            return snapshot.acceptedCount == 1u
                && snapshot.completedCount == 0u
                && snapshot.failedCount == 1u
                && snapshot.nodes.size() == 1u
                && snapshot.nodes.front().state
                    == taskgraph::NodeState::failed
                && snapshot.outstandingCount == 0u;
        }

        [[nodiscard]] bool timing_contract()
        {
            deterministicTime.store(100u, std::memory_order_relaxed);
            Scheduler scheduler{{
                .worker_count = 1u,
                .retained_finished_nodes = 1u},
                &deterministic_clock};
            auto future = scheduler.submit(
                "Timed source compilation",
                [] { return 7; });
            if (future.get() != 7)
                return false;
            scheduler.graph().WaitAll();

            const taskgraph::GraphSnapshot snapshot = scheduler.snapshot();
            if (snapshot.nodes.size() != 1u)
                return false;
            const taskgraph::NodeDiagnostic& node = snapshot.nodes.front();
            if (!node.wasQueued || !node.wasStarted || !node.wasFinished
                || node.acceptedNanoseconds != 100u
                || node.queuedNanoseconds != 110u
                || node.startedNanoseconds != 120u
                || node.finishedNanoseconds != 130u
                || node.queueLatencyNanoseconds != 10u
                || node.runDurationNanoseconds != 10u
                || snapshot.totalQueueLatencyNanoseconds != 10u
                || snapshot.peakQueueLatencyNanoseconds != 10u
                || snapshot.totalRunNanoseconds != 10u
                || snapshot.peakRunNanoseconds != 10u)
            {
                return false;
            }

            scheduler.graph().PruneFinished();
            const taskgraph::GraphSnapshot pruned = scheduler.snapshot();
            return pruned.nodes.empty()
                && pruned.totalQueueLatencyNanoseconds == 10u
                && pruned.peakQueueLatencyNanoseconds == 10u
                && pruned.totalRunNanoseconds == 10u
                && pruned.peakRunNanoseconds == 10u;
        }

        [[nodiscard]] bool cancellation_contract()
        {
            using namespace std::chrono_literals;
            Scheduler scheduler{{
                .worker_count = 1u,
                .retained_finished_nodes = 8u}};

            std::promise<void> blockerStartedPromise{};
            std::future<void> blockerStarted =
                blockerStartedPromise.get_future();
            std::promise<void> releaseBlockerPromise{};
            std::shared_future<void> releaseBlocker =
                releaseBlockerPromise.get_future().share();
            auto blocker = scheduler.submit_cancellable(
                "Cancellation queue blocker",
                [&blockerStartedPromise, releaseBlocker](std::stop_token)
                {
                    blockerStartedPromise.set_value();
                    releaseBlocker.wait();
                    return 7;
                });
            blockerStarted.wait();

            bool queuedInvoked = false;
            auto queued = scheduler.submit_cancellable(
                "Cancel before start",
                [&queuedInvoked](std::stop_token)
                {
                    queuedInvoked = true;
                    return 11;
                });
            if (scheduler.cancel(queued.cancellation)
                    != taskgraph::CancelCode::cancelled
                || scheduler.cancel(queued.cancellation)
                    != taskgraph::CancelCode::already_terminal)
            {
                releaseBlockerPromise.set_value();
                return false;
            }
            try
            {
                (void)queued.completion.get();
                releaseBlockerPromise.set_value();
                return false;
            }
            catch (const TaskCancelled&)
            {
            }
            catch (...)
            {
                releaseBlockerPromise.set_value();
                return false;
            }
            if (queuedInvoked)
            {
                releaseBlockerPromise.set_value();
                return false;
            }

            releaseBlockerPromise.set_value();
            if (blocker.completion.get() != 7)
                return false;

            std::promise<void> runningStartedPromise{};
            std::future<void> runningStarted =
                runningStartedPromise.get_future();
            auto running = scheduler.submit_cancellable(
                "Cooperative running cancellation",
                [&runningStartedPromise](std::stop_token cancellation)
                {
                    runningStartedPromise.set_value();
                    while (!cancellation.stop_requested())
                        std::this_thread::sleep_for(1ms);
                    return 13;
                });
            runningStarted.wait();
            if (scheduler.cancel(running.cancellation)
                != taskgraph::CancelCode::requested)
            {
                return false;
            }
            if (running.completion.get() != 13)
                return false;

            Scheduler foreign{{
                .worker_count = 1u,
                .retained_finished_nodes = 2u}};
            if (foreign.cancel(running.cancellation)
                != taskgraph::CancelCode::not_found)
            {
                return false;
            }

            scheduler.graph().WaitAll();
            const taskgraph::GraphSnapshot snapshot = scheduler.snapshot();
            if (snapshot.completedCount != 1u
                || snapshot.cancelledCount != 2u
                || snapshot.failedCount != 0u
                || snapshot.cancellationRequestCount != 2u
                || snapshot.outstandingCount != 0u
                || snapshot.nodes.size() != 3u)
            {
                return false;
            }
            const auto queuedNode = std::find_if(
                snapshot.nodes.begin(),
                snapshot.nodes.end(),
                [](const taskgraph::NodeDiagnostic& node)
                {
                    return node.label == "Cancel before start";
                });
            const auto runningNode = std::find_if(
                snapshot.nodes.begin(),
                snapshot.nodes.end(),
                [](const taskgraph::NodeDiagnostic& node)
                {
                    return node.label == "Cooperative running cancellation";
                });
            return queuedNode != snapshot.nodes.end()
                && queuedNode->state == taskgraph::NodeState::cancelled
                && queuedNode->cancellationRequested
                && !queuedNode->wasStarted
                && queuedNode->wasFinished
                && runningNode != snapshot.nodes.end()
                && runningNode->state == taskgraph::NodeState::cancelled
                && runningNode->cancellationRequested
                && runningNode->wasStarted
                && runningNode->wasFinished;
        }
        [[nodiscard]] bool label_contract()
        {
            Scheduler scheduler{{
                .worker_count = 1u,
                .retained_finished_nodes = 4u}};
            try
            {
                auto future = scheduler.submit("", [] { return 1; });
                (void)future;
                return false;
            }
            catch (const std::invalid_argument&)
            {
                return scheduler.snapshot().acceptedCount == 0u;
            }
            catch (...)
            {
                return false;
            }
        }
    }

    bool run_scheduler_contract()
    {
        return invalid_limits_contract()
            && submission_contract()
            && activity_accounting_contract()
            && future_exception_contract()
            && timing_contract()
            && cancellation_contract()
            && label_contract();
    }
}

int main()
{
    return epochengine::editor_tasks::run_scheduler_contract()
        ? 0
        : 1;
}
