/************************************************
 * Epoch Engine - Modular C++ Framework
 *
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 * Provided "AS IS", without warranty of any kind.
 * See LICENSE for full terms.
 ***********************************************/
module; // REQUIRED global module fragment

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <condition_variable>
#include <deque>
#include <fstream>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <string>
#include <stop_token>
#include <thread>
#include <utility>
#include <vector>

// ============================================================
// Named module
// ============================================================

export module taskgraph.dotsystem;

// ------------------------------------------------------------
// Engine dependencies (header units / modules)
// ------------------------------------------------------------

import systems.task;     // provides epochengine::Task
import systems.registry;
import core.logger;

// ------------------------------------------------------------
// Standard library
// ------------------------------------------------------------

// ============================================================
// Task graph system
// ============================================================

export namespace epochengine::taskgraph
{
    using MonotonicClock = std::uint64_t (*)() noexcept;
    enum class NodeState : std::uint8_t
    {
        pending,
        queued,
        running,
        completed,
        failed,
        cancelled
    };

    struct NodeHandle final
    {
        std::uint64_t id{};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return id != 0u;
        }

        [[nodiscard]] friend constexpr bool operator==(
            const NodeHandle&,
            const NodeHandle&) noexcept = default;
    };

    enum class CancelCode : std::uint8_t
    {
        cancelled,
        requested,
        already_terminal,
        not_found
    };

    struct NodeDiagnostic
    {
        std::uint64_t id{};
        NodeState state{ NodeState::pending };
        std::string label;
        std::size_t remainingPrerequisites{};
        std::size_t dependentCount{};
        std::uint64_t acceptedNanoseconds{};
        std::uint64_t queuedNanoseconds{};
        std::uint64_t startedNanoseconds{};
        std::uint64_t finishedNanoseconds{};
        std::uint64_t queueLatencyNanoseconds{};
        std::uint64_t runDurationNanoseconds{};
        bool wasQueued{};
        bool wasStarted{};
        bool wasFinished{};
        bool cancellationRequested{};
    };

    struct GraphEdgeDiagnostic final
    {
        std::uint64_t prerequisite_id{};
        std::uint64_t dependent_id{};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return prerequisite_id != 0u
                && dependent_id != 0u
                && prerequisite_id != dependent_id;
        }
    };

    struct GraphSnapshot
    {
        std::vector<NodeDiagnostic> nodes;
        std::vector<GraphEdgeDiagnostic> edges;
        std::size_t edgeCount{};
        std::size_t omittedEdgeCount{};
        std::size_t nodeCount{};
        std::size_t omittedNodeCount{};
        std::size_t workerCount{};
        std::size_t pendingCount{};
        std::size_t queueCount{};
        std::size_t runningCount{};
        std::size_t outstandingCount{};
        std::size_t acceptedCount{};
        std::size_t enqueuedCount{};
        std::size_t completedCount{};
        std::size_t failedCount{};
        std::size_t cancelledCount{};
        std::size_t rejectedCount{};
        std::size_t cancellationRequestCount{};
        std::uint64_t totalQueueLatencyNanoseconds{};
        std::uint64_t peakQueueLatencyNanoseconds{};
        std::uint64_t totalRunNanoseconds{};
        std::uint64_t peakRunNanoseconds{};
        std::uint64_t revision{};
        bool accepting{};
        bool draining{};
        bool stopped{};
    };

    class TaskGraph;

    struct Node
    {
        Task Task_;
        std::atomic<int> PrereqCount{ 0 };
        std::vector<Node*> Dependents;
        std::string Label;

        explicit Node(
            Task&& task,
            std::stop_source cancellation = {},
            std::function<void()> cancelledBeforeStart = {})
            : Task_(std::move(task)),
              CancellationSource_(std::move(cancellation)),
              CancelledBeforeStart_(std::move(cancelledBeforeStart))
        {
        }

    private:
        friend class TaskGraph;

        std::atomic<NodeState> State_{ NodeState::pending };
        std::atomic<TaskGraph*> Owner_{ nullptr };
        std::uint64_t DiagnosticId_{};
        std::string DiagnosticLabel_;
        std::uint64_t AcceptedNanoseconds_{};
        std::uint64_t QueuedNanoseconds_{};
        std::uint64_t StartedNanoseconds_{};
        std::uint64_t FinishedNanoseconds_{};
        bool WasQueued_{};
        bool WasStarted_{};
        bool WasFinished_{};
        bool CancellationRequested_{};
        std::stop_source CancellationSource_{};
        std::function<void()> CancelledBeforeStart_{};
    };

    using NodePtr = std::unique_ptr<Node>;

    class TaskGraph
    {
    public:
        static constexpr std::size_t MaxNodeCapacity = 65'536;
        static constexpr std::size_t MaxWorkerCount = 256;
        static constexpr std::size_t MaxSnapshotNodes = 512;
        static constexpr std::size_t MaxSnapshotEdges = 4'096;
        static constexpr std::size_t MaxSnapshotLabelBytes = 192;
        static constexpr std::size_t MaxDiagnosticLabelBytes = 1'024;

        explicit TaskGraph(
            std::size_t workerCount,
            MonotonicClock clock = nullptr)
            : Clock_(clock ? clock : &DefaultNowNanoseconds)
        {
            const auto boundedWorkerCount = (std::min)(workerCount, MaxWorkerCount);
            Workers_.reserve(boundedWorkerCount);

            try
            {
                for (std::size_t index = 0; index < boundedWorkerCount; ++index)
                    Workers_.emplace_back(&TaskGraph::WorkerLoop, this);
            }
            catch (...)
            {
                {
                    std::lock_guard lock(Mutex_);
                    StopRequested_ = true;
                    Lifecycle_ = Lifecycle::stopped;
                }
                WorkAvailable_.notify_all();

                for (auto& worker : Workers_)
                {
                    if (worker.joinable())
                        worker.join();
                }
                throw;
            }

            if (workerCount > MaxWorkerCount)
                logger::warn("TaskGraph", "Worker count exceeded the bounded task-graph limit and was clamped.");
        }

        TaskGraph(const TaskGraph&) = delete;
        TaskGraph& operator=(const TaskGraph&) = delete;

        ~TaskGraph() noexcept
        {
            Shutdown();
        }

        [[nodiscard]] NodeHandle AddNode(NodePtr node)
        {
            if (!node)
            {
                RecordRejectedNode("Null node rejected.");
                return {};
            }

            std::unique_lock lock(Mutex_);
            if (Lifecycle_ != Lifecycle::accepting)
            {
                RejectNodeLocked();
                lock.unlock();
                CompletionChanged_.notify_all();
                logger::warn("TaskGraph", "Node rejected because the task graph is shutting down.");
                return {};
            }

            if (Nodes_.size() >= MaxNodeCapacity ||
                NextNodeId_ == (std::numeric_limits<std::uint64_t>::max)())
            {
                RejectNodeLocked();
                lock.unlock();
                CompletionChanged_.notify_all();
                logger::warn("TaskGraph", "Node rejected because the bounded task-graph capacity was reached.");
                return {};
            }

            TaskGraph* expectedOwner = nullptr;
            if (!node->Owner_.compare_exchange_strong(
                    expectedOwner,
                    this,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire))
            {
                RejectNodeLocked();
                lock.unlock();
                CompletionChanged_.notify_all();
                logger::warn("TaskGraph", "Node rejected because it already belongs to a task graph.");
                return {};
            }

            node->DiagnosticId_ = NextNodeId_++;
            const NodeHandle handle{node->DiagnosticId_};
            node->DiagnosticLabel_ = node->Label.substr(0, MaxDiagnosticLabelBytes);
            node->State_.store(NodeState::pending, std::memory_order_release);
            node->AcceptedNanoseconds_ = NowNanosecondsLocked();
            node->QueuedNanoseconds_ = 0u;
            node->StartedNanoseconds_ = 0u;
            node->FinishedNanoseconds_ = 0u;
            node->WasQueued_ = false;
            node->WasStarted_ = false;
            node->WasFinished_ = false;
            node->CancellationRequested_ = false;
            Nodes_.push_back(std::move(node));
            SaturatingIncrement(Accepted_);
            SaturatingIncrement(Outstanding_);
            BumpRevisionLocked();

            lock.unlock();
            CompletionChanged_.notify_all();
            return handle;
        }
        void AddDependency(Node& prerequisite, Node& dependent)
        {
            bool rejected = false;
            {
                std::lock_guard lock(Mutex_);

                const auto prerequisiteOwner = prerequisite.Owner_.load(std::memory_order_acquire);
                const auto dependentOwner = dependent.Owner_.load(std::memory_order_acquire);
                const bool foreignOwner =
                    (prerequisiteOwner && prerequisiteOwner != this) ||
                    (dependentOwner && dependentOwner != this);
                const bool duplicate =
                    std::find(
                        prerequisite.Dependents.begin(),
                        prerequisite.Dependents.end(),
                        &dependent) != prerequisite.Dependents.end();
                const bool overflow =
                    dependent.PrereqCount.load(std::memory_order_acquire) ==
                    (std::numeric_limits<int>::max)();

                if (&prerequisite == &dependent ||
                    foreignOwner ||
                    duplicate ||
                    dependent.State_.load(std::memory_order_acquire) != NodeState::pending ||
                    overflow)
                {
                    rejected = true;
                }
                else
                {
                    prerequisite.Dependents.push_back(&dependent);
                    const auto state = prerequisite.State_.load(std::memory_order_acquire);
                    if (!IsTerminal(state))
                        dependent.PrereqCount.fetch_add(1, std::memory_order_release);
                    BumpRevisionLocked();
                }
            }

            CompletionChanged_.notify_all();
            if (rejected)
                logger::warn("TaskGraph", "Invalid, duplicate, late, or cross-graph dependency rejected.");
        }

        [[nodiscard]] CancelCode Cancel(NodeHandle handle)
        {
            std::function<void()> cancelledBeforeStart{};
            CancelCode result = CancelCode::not_found;
            {
                std::lock_guard lock(Mutex_);
                Node* node = ResolveNodeLocked(handle);
                if (node == nullptr)
                    return CancelCode::not_found;

                const NodeState state = node->State_.load(std::memory_order_acquire);
                if (IsTerminal(state))
                    return CancelCode::already_terminal;
                if (node->CancellationRequested_)
                    return state == NodeState::running
                        ? CancelCode::requested
                        : CancelCode::cancelled;

                node->CancellationRequested_ = true;
                (void)node->CancellationSource_.request_stop();
                SaturatingIncrement(CancellationRequests_);
                BumpRevisionLocked();
                if (state == NodeState::running)
                {
                    result = CancelCode::requested;
                }
                else
                {
                    if (state == NodeState::queued)
                    {
                        ReadyQueue_.erase(
                            std::remove(ReadyQueue_.begin(), ReadyQueue_.end(), node),
                            ReadyQueue_.end());
                    }
                    node->State_.store(NodeState::cancelled, std::memory_order_release);
                    node->PrereqCount.store(-3, std::memory_order_release);
                    FinishTimingLocked(*node);
                    SaturatingIncrement(Cancelled_);
                    if (Outstanding_ > 0u)
                        --Outstanding_;

                    for (Node* dependent : node->Dependents)
                    {
                        if (!dependent || !ContainsNodeLocked(dependent))
                            continue;
                        if (dependent->State_.load(std::memory_order_acquire)
                            != NodeState::pending)
                        {
                            continue;
                        }
                        const int remaining = dependent->PrereqCount.load(
                            std::memory_order_acquire);
                        if (remaining <= 0)
                            continue;
                        dependent->PrereqCount.store(
                            remaining - 1,
                            std::memory_order_release);
                        if (remaining == 1)
                            QueueNodeLocked(*dependent);
                    }
                    cancelledBeforeStart = node->CancelledBeforeStart_;
                    result = CancelCode::cancelled;
                    BumpRevisionLocked();
                }
            }

            if (cancelledBeforeStart)
            {
                try
                {
                    cancelledBeforeStart();
                }
                catch (...)
                {
                    logger::error(
                        "TaskGraph",
                        "A pre-start cancellation completion callback failed.");
                }
            }
            WorkAvailable_.notify_all();
            CompletionChanged_.notify_all();
            return result;
        }
        void Execute()
        {
            bool runInline = false;
            {
                std::lock_guard lock(Mutex_);
                if (Lifecycle_ != Lifecycle::accepting)
                    return;

                ScheduleReadyNodesLocked();
                runInline = Workers_.empty();
            }

            WorkAvailable_.notify_all();
            CompletionChanged_.notify_all();
            if (runInline)
                DrainInline(false);
        }

        void WaitAll()
        {
            if (CurrentWorkerGraph_ == this)
            {
                logger::warn("TaskGraph", "WaitAll cannot be called from one of the graph's own workers.");
                return;
            }

            WaitForDrain();
        }

        void PruneFinished()
        {
            std::lock_guard lock(Mutex_);
            const auto previousSize = Nodes_.size();
            auto end = std::remove_if(Nodes_.begin(), Nodes_.end(), [](const NodePtr& node) {
                const auto state = node->State_.load(std::memory_order_acquire);
                if (!IsTerminal(state))
                    return false;

                node->Owner_.store(nullptr, std::memory_order_release);
                return true;
            });

            Nodes_.erase(end, Nodes_.end());

            if (Nodes_.size() != previousSize)
                BumpRevisionLocked();
        }

        [[nodiscard]] std::size_t QueueDepth() const
        {
            std::lock_guard lock(Mutex_);
            return ReadyQueue_.size();
        }

        [[nodiscard]] std::size_t CompletedCount() const
        {
            std::lock_guard lock(Mutex_);
            return Completed_;
        }

        [[nodiscard]] std::size_t EnqueuedCount() const
        {
            std::lock_guard lock(Mutex_);
            return Enqueued_;
        }

        [[nodiscard]] std::size_t FailedCount() const
        {
            std::lock_guard lock(Mutex_);
            return Failed_;
        }

        [[nodiscard]] std::size_t CancelledCount() const
        {
            std::lock_guard lock(Mutex_);
            return Cancelled_;
        }

        [[nodiscard]] std::size_t WorkerCount() const noexcept
        {
            return Workers_.size();
        }

        [[nodiscard]] std::uint64_t Revision() const
        {
            std::lock_guard lock(Mutex_);
            return Revision_;
        }

        [[nodiscard]] GraphSnapshot Snapshot(std::size_t maxNodes = MaxSnapshotNodes) const
        {
            GraphSnapshot result;
            maxNodes = (std::min)(maxNodes, MaxSnapshotNodes);

            std::lock_guard lock(Mutex_);
            result.nodeCount = Nodes_.size();
            result.omittedNodeCount = Nodes_.size() > maxNodes ? Nodes_.size() - maxNodes : 0;
            result.workerCount = Workers_.size();
            result.queueCount = ReadyQueue_.size();
            result.runningCount = RunningCount_;
            result.outstandingCount = Outstanding_;
            result.acceptedCount = Accepted_;
            result.enqueuedCount = Enqueued_;
            result.completedCount = Completed_;
            result.failedCount = Failed_;
            result.cancelledCount = Cancelled_;
            result.rejectedCount = Rejected_;
            result.cancellationRequestCount = CancellationRequests_;
            result.totalQueueLatencyNanoseconds = TotalQueueLatencyNanoseconds_;
            result.peakQueueLatencyNanoseconds = PeakQueueLatencyNanoseconds_;
            result.totalRunNanoseconds = TotalRunNanoseconds_;
            result.peakRunNanoseconds = PeakRunNanoseconds_;
            result.revision = Revision_;
            result.accepting = Lifecycle_ == Lifecycle::accepting;
            result.draining = Lifecycle_ == Lifecycle::draining;
            result.stopped = Lifecycle_ == Lifecycle::stopped;

            result.nodes.reserve((std::min)(Nodes_.size(), maxNodes));
            for (const auto& node : Nodes_)
            {
                const auto state = node->State_.load(std::memory_order_acquire);
                if (state == NodeState::pending)
                    SaturatingIncrement(result.pendingCount);

                if (result.nodes.size() >= maxNodes)
                    continue;

                NodeDiagnostic diagnostic;
                diagnostic.id = node->DiagnosticId_;
                diagnostic.state = state;
                diagnostic.label = node->DiagnosticLabel_.substr(0, MaxSnapshotLabelBytes);
                const auto prerequisiteCount = node->PrereqCount.load(std::memory_order_acquire);
                diagnostic.remainingPrerequisites = prerequisiteCount > 0
                    ? static_cast<std::size_t>(prerequisiteCount)
                    : 0;
                diagnostic.dependentCount = (std::min)(node->Dependents.size(), MaxNodeCapacity);
                diagnostic.acceptedNanoseconds = node->AcceptedNanoseconds_;
                diagnostic.queuedNanoseconds = node->QueuedNanoseconds_;
                diagnostic.startedNanoseconds = node->StartedNanoseconds_;
                diagnostic.finishedNanoseconds = node->FinishedNanoseconds_;
                diagnostic.wasQueued = node->WasQueued_;
                diagnostic.wasStarted = node->WasStarted_;
                diagnostic.wasFinished = node->WasFinished_;
                diagnostic.cancellationRequested = node->CancellationRequested_;
                diagnostic.queueLatencyNanoseconds =
                    node->WasQueued_ && node->WasStarted_
                        ? ElapsedNanoseconds(
                            node->QueuedNanoseconds_,
                            node->StartedNanoseconds_)
                        : 0u;
                diagnostic.runDurationNanoseconds =
                    node->WasStarted_ && node->WasFinished_
                        ? ElapsedNanoseconds(
                            node->StartedNanoseconds_,
                            node->FinishedNanoseconds_)
                        : 0u;
                result.nodes.push_back(std::move(diagnostic));
            }

            const std::size_t includedNodeCount = result.nodes.size();
            const auto includedEnd = Nodes_.begin()
                + static_cast<std::ptrdiff_t>(includedNodeCount);
            result.edges.reserve((std::min)(
                MaxSnapshotEdges,
                includedNodeCount * 2u));
            for (auto source = Nodes_.begin(); source != Nodes_.end(); ++source)
            {
                for (const Node* dependent : (*source)->Dependents)
                {
                    ++result.edgeCount;
                    if (source >= includedEnd
                        || result.edges.size() >= MaxSnapshotEdges)
                    {
                        continue;
                    }

                    const auto target = std::find_if(
                        Nodes_.begin(),
                        includedEnd,
                        [dependent](const NodePtr& candidate)
                        {
                            return candidate.get() == dependent;
                        });
                    if (target == includedEnd)
                        continue;

                    result.edges.push_back({
                        .prerequisite_id = (*source)->DiagnosticId_,
                        .dependent_id = (*target)->DiagnosticId_
                    });
                }
            }
            result.omittedEdgeCount =
                result.edgeCount - result.edges.size();
            return result;
        }

        void DumpDot(const std::string& path = "graph.dot") const
        {
            struct DotNode
            {
                std::uint64_t id{};
                std::string label;
                std::vector<std::uint64_t> dependents;
            };

            std::vector<DotNode> graph;
            {
                std::lock_guard lock(Mutex_);
                graph.reserve(Nodes_.size());

                for (const auto& node : Nodes_)
                {
                    DotNode copy;
                    copy.id = node->DiagnosticId_;
                    copy.label = node->DiagnosticLabel_;
                    copy.dependents.reserve(node->Dependents.size());

                    for (const auto* dependent : node->Dependents)
                    {
                        const auto found = std::find_if(
                            Nodes_.begin(),
                            Nodes_.end(),
                            [dependent](const NodePtr& candidate) {
                                return candidate.get() == dependent;
                            });
                        if (found != Nodes_.end())
                            copy.dependents.push_back((*found)->DiagnosticId_);
                    }
                    graph.push_back(std::move(copy));
                }
            }

            std::ofstream out(path, std::ios::out | std::ios::trunc);
            if (!out)
            {
                logger::error("TaskGraph", "Failed to open the requested DOT output path.");
                return;
            }

            out << "digraph G{";
            for (const auto& node : graph)
                out << "N" << node.id << "[label=\"" << EscapeDotLabel(node.label) << "\"];";

            for (const auto& node : graph)
            {
                for (const auto dependent : node.dependents)
                    out << "N" << node.id << "->N" << dependent << ";";
            }
            out << "}";

            if (!out)
                logger::error("TaskGraph", "Failed while writing the requested DOT output.");
        }

        void Shutdown() noexcept
        {
            if (CurrentWorkerGraph_ == this)
            {
                logger::error("TaskGraph", "Shutdown cannot be initiated from one of the graph's own workers.");
                return;
            }

            {
                std::unique_lock lock(Mutex_);
                if (Lifecycle_ == Lifecycle::stopped)
                    return;

                if (Lifecycle_ == Lifecycle::draining)
                {
                    CompletionChanged_.wait(lock, [this] {
                        return Lifecycle_ == Lifecycle::stopped;
                    });
                    return;
                }

                Lifecycle_ = Lifecycle::draining;
                BumpRevisionLocked();
                ScheduleReadyNodesLocked();
            }

            WorkAvailable_.notify_all();
            WaitForDrain();

            {
                std::lock_guard lock(Mutex_);
                StopRequested_ = true;
                BumpRevisionLocked();
            }
            WorkAvailable_.notify_all();

            for (auto& worker : Workers_)
            {
                if (worker.joinable())
                    worker.join();
            }

            {
                std::lock_guard lock(Mutex_);
                Lifecycle_ = Lifecycle::stopped;
                BumpRevisionLocked();
            }
            CompletionChanged_.notify_all();
        }

    private:
        enum class Lifecycle : std::uint8_t
        {
            accepting,
            draining,
            stopped
        };

        template <class T>
        static void SaturatingIncrement(T& value) noexcept
        {
            if (value != (std::numeric_limits<T>::max)())
                ++value;
        }

        void BumpRevisionLocked() noexcept
        {
            SaturatingIncrement(Revision_);
        }

        [[nodiscard]] static std::uint64_t DefaultNowNanoseconds() noexcept
        {
            const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
            return elapsed > 0 ? static_cast<std::uint64_t>(elapsed) : 0u;
        }

        [[nodiscard]] std::uint64_t NowNanosecondsLocked() const noexcept
        {
            return Clock_();
        }

        [[nodiscard]] static constexpr std::uint64_t ElapsedNanoseconds(
            std::uint64_t start,
            std::uint64_t finish) noexcept
        {
            return finish >= start ? finish - start : 0u;
        }

        static void SaturatingAdd(
            std::uint64_t& value,
            std::uint64_t increment) noexcept
        {
            const auto remaining = (std::numeric_limits<std::uint64_t>::max)()
                - value;
            value += (std::min)(remaining, increment);
        }

        void FinishTimingLocked(Node& node) noexcept
        {
            node.FinishedNanoseconds_ = NowNanosecondsLocked();
            node.WasFinished_ = true;
            if (!node.WasStarted_)
                return;

            const std::uint64_t runDuration = ElapsedNanoseconds(
                node.StartedNanoseconds_, node.FinishedNanoseconds_);
            SaturatingAdd(TotalRunNanoseconds_, runDuration);
            PeakRunNanoseconds_ = (std::max)(PeakRunNanoseconds_, runDuration);

            if (!node.WasQueued_)
                return;
            const std::uint64_t queueLatency = ElapsedNanoseconds(
                node.QueuedNanoseconds_, node.StartedNanoseconds_);
            SaturatingAdd(TotalQueueLatencyNanoseconds_, queueLatency);
            PeakQueueLatencyNanoseconds_ = (std::max)(
                PeakQueueLatencyNanoseconds_, queueLatency);
        }

        void RejectNodeLocked() noexcept
        {
            SaturatingIncrement(Failed_);
            SaturatingIncrement(Rejected_);
            BumpRevisionLocked();
        }

        void RecordRejectedNode(const char* reason)
        {
            {
                std::lock_guard lock(Mutex_);
                RejectNodeLocked();
            }
            CompletionChanged_.notify_all();
            logger::warn("TaskGraph", reason);
        }

        [[nodiscard]] static constexpr bool IsTerminal(
            NodeState state) noexcept
        {
            return state == NodeState::completed
                || state == NodeState::failed
                || state == NodeState::cancelled;
        }

        [[nodiscard]] Node* ResolveNodeLocked(NodeHandle handle) noexcept
        {
            if (!handle.valid())
                return nullptr;
            const auto found = std::find_if(
                Nodes_.begin(),
                Nodes_.end(),
                [handle](const NodePtr& candidate)
                {
                    return candidate->DiagnosticId_ == handle.id;
                });
            return found == Nodes_.end() ? nullptr : found->get();
        }
        [[nodiscard]] bool ContainsNodeLocked(const Node* node) const noexcept
        {
            return std::find_if(
                Nodes_.begin(),
                Nodes_.end(),
                [node](const NodePtr& candidate) {
                    return candidate.get() == node;
                }) != Nodes_.end();
        }

        bool QueueNodeLocked(Node& node) noexcept
        {
            if (node.State_.load(std::memory_order_acquire) != NodeState::pending)
                return false;
            if (ReadyQueue_.size() >= MaxNodeCapacity)
                return false;

            try
            {
                ReadyQueue_.push_back(&node);
            }
            catch (...)
            {
                node.State_.store(NodeState::failed, std::memory_order_release);
                node.PrereqCount.store(-2, std::memory_order_release);
                FinishTimingLocked(node);
                SaturatingIncrement(Failed_);
                if (Outstanding_ > 0)
                    --Outstanding_;
                BumpRevisionLocked();
                return false;
            }

            node.State_.store(NodeState::queued, std::memory_order_release);
            node.QueuedNanoseconds_ = NowNanosecondsLocked();
            node.WasQueued_ = true;
            SaturatingIncrement(Enqueued_);
            BumpRevisionLocked();
            return true;
        }

        [[nodiscard]] Node* PopNodeLocked() noexcept
        {
            if (ReadyQueue_.empty())
                return nullptr;

            Node* node = ReadyQueue_.front();
            ReadyQueue_.pop_front();
            if (!node || node->State_.load(std::memory_order_acquire) != NodeState::queued)
                return nullptr;

            node->State_.store(NodeState::running, std::memory_order_release);
            node->StartedNanoseconds_ = NowNanosecondsLocked();
            node->WasStarted_ = true;
            SaturatingIncrement(RunningCount_);
            BumpRevisionLocked();
            return node;
        }

        void ScheduleReadyNodesLocked()
        {
            for (auto& node : Nodes_)
            {
                if (node->State_.load(std::memory_order_acquire) == NodeState::pending &&
                    node->PrereqCount.load(std::memory_order_acquire) == 0)
                {
                    QueueNodeLocked(*node);
                }
            }
        }

        [[nodiscard]] std::size_t FailQuiescentNodesLocked() noexcept
        {
            if (Outstanding_ == 0 || !ReadyQueue_.empty() || RunningCount_ != 0)
                return 0;

            std::size_t failedNow = 0;
            for (auto& node : Nodes_)
            {
                if (node->State_.load(std::memory_order_acquire) != NodeState::pending)
                    continue;

                node->State_.store(NodeState::failed, std::memory_order_release);
                node->PrereqCount.store(-2, std::memory_order_release);
                FinishTimingLocked(*node);
                SaturatingIncrement(Failed_);
                if (Outstanding_ > 0)
                    --Outstanding_;
                ++failedNow;
                BumpRevisionLocked();
            }
            return failedNow;
        }

        [[nodiscard]] bool InvokeNode(Node& node) noexcept
        {
            if (!node.Task_.h)
            {
                logger::warn("TaskGraph", "Task node has a null coroutine handle and was failed.");
                return false;
            }

            try
            {
                node.Task_.h.resume();
            }
            catch (...)
            {
                logger::error("TaskGraph", "Task coroutine resume raised an exception and was failed.");
                node.Task_.h.destroy();
                node.Task_.h = nullptr;
                return false;
            }

            if (!node.Task_.h.done())
            {
                logger::error("TaskGraph", "Task coroutine suspended after dispatch; one-shot graph nodes must finish on resume.");
                node.Task_.h.destroy();
                node.Task_.h = nullptr;
                return false;
            }

            const bool succeeded = !node.Task_.h.promise().failed;
            node.Task_.h.destroy();
            node.Task_.h = nullptr;
            return succeeded;
        }

        void FinalizeNode(Node& node, bool succeeded) noexcept
        {
            {
                std::lock_guard lock(Mutex_);
                if (node.State_.load(std::memory_order_acquire) != NodeState::running)
                    return;

                if (RunningCount_ > 0u)
                    --RunningCount_;

                const bool cancelled = node.CancellationRequested_
                    || node.CancellationSource_.stop_requested();
                node.State_.store(
                    cancelled
                        ? NodeState::cancelled
                        : (succeeded ? NodeState::completed : NodeState::failed),
                    std::memory_order_release);
                node.PrereqCount.store(
                    cancelled ? -3 : (succeeded ? -1 : -2),
                    std::memory_order_release);
                FinishTimingLocked(node);
                if (cancelled)
                    SaturatingIncrement(Cancelled_);
                else if (succeeded)
                    SaturatingIncrement(Completed_);
                else
                    SaturatingIncrement(Failed_);
                if (Outstanding_ > 0u)
                    --Outstanding_;

                for (Node* dependent : node.Dependents)
                {
                    if (!dependent || !ContainsNodeLocked(dependent))
                        continue;
                    if (dependent->State_.load(std::memory_order_acquire)
                        != NodeState::pending)
                    {
                        continue;
                    }

                    const int remaining = dependent->PrereqCount.load(
                        std::memory_order_acquire);
                    if (remaining <= 0)
                        continue;

                    dependent->PrereqCount.store(
                        remaining - 1,
                        std::memory_order_release);
                    if (remaining == 1)
                        QueueNodeLocked(*dependent);
                }
                BumpRevisionLocked();
            }

            WorkAvailable_.notify_all();
            CompletionChanged_.notify_all();
        }
        void DrainInline(bool failQuiescent) noexcept
        {
            for (;;)
            {
                Node* node = nullptr;
                std::size_t failedNow = 0;
                {
                    std::lock_guard lock(Mutex_);
                    ScheduleReadyNodesLocked();
                    node = PopNodeLocked();
                    if (!node && failQuiescent)
                        failedNow = FailQuiescentNodesLocked();
                }

                if (failedNow > 0)
                {
                    logger::error("TaskGraph", "Unresolvable task dependencies were failed during drain.");
                    CompletionChanged_.notify_all();
                }

                if (!node)
                    return;

                TaskGraph* previousGraph = CurrentWorkerGraph_;
                CurrentWorkerGraph_ = this;
                const bool succeeded = InvokeNode(*node);
                CurrentWorkerGraph_ = previousGraph;
                FinalizeNode(*node, succeeded);
            }
        }

        void WaitForDrain() noexcept
        {
            if (Workers_.empty())
            {
                DrainInline(true);
                return;
            }

            std::unique_lock lock(Mutex_);
            for (;;)
            {
                ScheduleReadyNodesLocked();
                WorkAvailable_.notify_all();

                if (Outstanding_ == 0)
                    return;

                const auto failedNow = FailQuiescentNodesLocked();
                if (failedNow > 0)
                {
                    lock.unlock();
                    logger::error("TaskGraph", "Unresolvable task dependencies were failed during drain.");
                    CompletionChanged_.notify_all();
                    lock.lock();
                    continue;
                }

                CompletionChanged_.wait(lock);
            }
        }

        void WorkerLoop() noexcept
        {
            epochengine::systems::threading::ScopedThreadActivity threadActivity{};
            CurrentWorkerGraph_ = this;

            for (;;)
            {
                Node* node = nullptr;
                {
                    std::unique_lock lock(Mutex_);
                    WorkAvailable_.wait(lock, [this] {
                        return StopRequested_ || !ReadyQueue_.empty();
                    });

                    if (StopRequested_ && ReadyQueue_.empty())
                        break;

                    node = PopNodeLocked();
                }

                if (!node)
                    continue;

                const bool succeeded = InvokeNode(*node);
                FinalizeNode(*node, succeeded);
            }

            CurrentWorkerGraph_ = nullptr;
        }

        [[nodiscard]] static std::string EscapeDotLabel(const std::string& label)
        {
            std::string escaped;
            escaped.reserve(label.size());
            for (const char character : label)
            {
                switch (character)
                {
                case '\\': escaped += "\\\\"; break;
                case '"': escaped += "\\\""; break;
                case '\n': escaped += "\\n"; break;
                case '\r': break;
                default: escaped.push_back(character); break;
                }
            }
            return escaped;
        }

        inline static thread_local TaskGraph* CurrentWorkerGraph_ = nullptr;

        mutable std::mutex Mutex_;
        std::condition_variable WorkAvailable_;
        std::condition_variable CompletionChanged_;
        std::deque<Node*> ReadyQueue_;
        std::vector<std::thread> Workers_;
        std::vector<NodePtr> Nodes_;
        Lifecycle Lifecycle_{ Lifecycle::accepting };
        bool StopRequested_{};
        std::uint64_t NextNodeId_{ 1 };
        std::uint64_t Revision_{ 1 };
        std::size_t RunningCount_{};
        std::size_t Outstanding_{};
        std::size_t Accepted_{};
        std::size_t Enqueued_{};
        std::size_t Completed_{};
        std::size_t Failed_{};
        std::size_t Cancelled_{};
        std::size_t Rejected_{};
        std::size_t CancellationRequests_{};
        std::uint64_t TotalQueueLatencyNanoseconds_{};
        std::uint64_t PeakQueueLatencyNanoseconds_{};
        std::uint64_t TotalRunNanoseconds_{};
        std::uint64_t PeakRunNanoseconds_{};
        MonotonicClock Clock_{};
    };
}
