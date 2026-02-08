module; // REQUIRED global module fragment

// ============================================================
// Named module
// ============================================================

export module aengine.taskgraph.dotsystem;

// ------------------------------------------------------------
// Engine dependencies (header units / modules)
// ------------------------------------------------------------

import ampmcboundedqueue;   // provides almondnamespace::MPMCQueue
import aengine.systems;     // provides almondnamespace::Task

// ------------------------------------------------------------
// Standard library
// ------------------------------------------------------------
import <atomic>;
import <coroutine>;
import <cstdint>;
import <fstream>;
import <iostream>;
import <memory>;
import <string>;
import <thread>;
import <vector>;
import <semaphore>;
import <algorithm>;

// ============================================================
// Task graph system
// ============================================================

export namespace almondnamespace::taskgraph
{
    struct Node
    {
        Task Task_;
        std::atomic<int> PrereqCount{ 0 };
        std::vector<Node*> Dependents;
        std::string Label;

        explicit Node(Task&& t) : Task_(std::move(t)) {}
    };

    using NodePtr = std::unique_ptr<Node>;

    class TaskGraph
    {
    public:
        explicit TaskGraph(std::size_t workerCount)
            : Queue_(1024)
            , Running_(true)
            , WorkSem_(0)
        {
            for (std::size_t i = 0; i < workerCount; ++i)
                Workers_.emplace_back(&TaskGraph::WorkerLoop, this);
        }

        ~TaskGraph()
        {
            Running_ = false;
            for (std::size_t i = 0; i < Workers_.size(); ++i)
                WorkSem_.release();

            for (auto& t : Workers_)
                if (t.joinable())
                    t.join();
        }

        void AddNode(NodePtr node)
        {
            Nodes_.push_back(std::move(node));
        }

        void AddDependency(Node& a, Node& b)
        {
            b.PrereqCount.fetch_add(1, std::memory_order_relaxed);
            a.Dependents.push_back(&b);
        }

        void Execute()
        {
            for (auto& n : Nodes_) {
                if (n->PrereqCount.load(std::memory_order_acquire) == 0) {
                    EnqueueNode(n.get());
                }
            }
        }

        void WaitAll()
        {
            while (true) {
                bool busy = !Queue_.empty();
                for (auto& n : Nodes_) {
                    if (n->PrereqCount.load(std::memory_order_acquire) >= 0) {
                        busy = true;
                        break;
                    }
                }
                if (!busy)
                    break;
                std::this_thread::yield();
            }
        }

        void PruneFinished()
        {
            auto end = std::remove_if(
                Nodes_.begin(),
                Nodes_.end(),
                [](const NodePtr& node) {
                    return node->PrereqCount.load(std::memory_order_acquire) < 0;
                });

            Nodes_.erase(end, Nodes_.end());
        }

        std::size_t QueueDepth() const
        {
            return Queue_.approximate_size();
        }

        std::size_t CompletedCount() const
        {
            return Completed_.load(std::memory_order_relaxed);
        }

        std::size_t EnqueuedCount() const
        {
            return Enqueued_.load(std::memory_order_relaxed);
        }

        void DumpDot(const std::string& path = "graph.dot")
        {
            std::ofstream out(path);
            out << "digraph G{";

            for (auto& n : Nodes_) {
                auto id = reinterpret_cast<std::uintptr_t>(n.get());
                out << "N" << id << "[label=\"" << n->Label << "\"];";
            }

            for (auto& n : Nodes_) {
                auto s = reinterpret_cast<std::uintptr_t>(n.get());
                for (auto* d : n->Dependents) {
                    auto t = reinterpret_cast<std::uintptr_t>(d);
                    out << "N" << s << "->N" << t << ";";
                }
            }

            out << "}";
        }

    private:
        void EnqueueNode(Node* node)
        {
            while (!Queue_.enqueue(node)) {
                std::this_thread::yield();
            }

            Enqueued_.fetch_add(1, std::memory_order_relaxed);
            WorkSem_.release();
        }

        void MarkCompleted(Node* node)
        {
            node->PrereqCount.store(-1, std::memory_order_release);
            Completed_.fetch_add(1, std::memory_order_relaxed);
        }

        void WorkerLoop()
        {
            Node* n = nullptr;

            while (Running_) {
                WorkSem_.acquire();
                if (!Running_) break;
                if (!Queue_.dequeue(n)) continue;

                if (!n || !n->Task_.h) {
#ifndef NDEBUG
                    std::cerr << "[TaskGraph] WARNING: null coroutine handle, skipping\n";
#endif
                    continue;
                }

                n->Task_.h.resume();

                if (n->Task_.h && n->Task_.h.done()) {
                    n->Task_.h.destroy();
                    n->Task_.h = nullptr;
                }

                for (auto* d : n->Dependents) {
                    if (d->PrereqCount.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                        EnqueueNode(d);
                    }
                }

                MarkCompleted(n);
            }

            // Drain remaining work
            while (Queue_.dequeue(n)) {
                if (!n || !n->Task_.h) continue;

                n->Task_.h.resume();

                if (n->Task_.h && n->Task_.h.done()) {
                    n->Task_.h.destroy();
                    n->Task_.h = nullptr;
                }

                for (auto* d : n->Dependents) {
                    if (d->PrereqCount.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                        EnqueueNode(d);
                    }
                }

                MarkCompleted(n);
            }
        }

        MPMCQueue<Node*>            Queue_;
        std::vector<std::thread>    Workers_;
        std::atomic<bool>           Running_;
        std::counting_semaphore<>   WorkSem_;
        std::vector<NodePtr>        Nodes_;
        std::atomic<std::size_t>    Enqueued_{ 0 };
        std::atomic<std::size_t>    Completed_{ 0 };
    };
}
