module;

export module ampmcboundedqueue;

import <atomic>;
import <cassert>;
import <cstddef>;
import <memory>;
import <type_traits>;

export namespace almondnamespace {
    template<typename T>
    class MPMCQueue {
    public:
        // capacity must be a power of two
        explicit MPMCQueue(size_t capacity)
            : capacity_(capacity),
              mask_(capacity - 1),
              buffer_(static_cast<Node*>(operator new[](sizeof(Node) * capacity)))
        {
            assert((capacity & mask_) == 0 && "capacity must be power of two");
            for (size_t i = 0; i < capacity_; ++i) {
                new(&buffer_[i]) Node{ i, T{} };
            }
            head_.store(0, std::memory_order_relaxed);
            tail_.store(0, std::memory_order_relaxed);
        }

        ~MPMCQueue() {
            for (size_t i = 0; i < capacity_; ++i)
                buffer_[i].~Node();
            operator delete[](buffer_);
        }

        bool enqueue(const T& item) {
            Node* node;
            size_t pos = tail_.load(std::memory_order_relaxed);
            for (;;) {
                node = &buffer_[pos & mask_];
                size_t seq = node->seq.load(std::memory_order_acquire);
                intptr_t dif = (intptr_t)seq - (intptr_t)pos;
                if (dif == 0) {
                    if (tail_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
                        break;
                }
                else if (dif < 0) {
                    return false; // queue full
                }
                else {
                    pos = tail_.load(std::memory_order_relaxed);
                }
            }
            node->data = item;
            node->seq.store(pos + 1, std::memory_order_release);
            return true;
        }

        bool dequeue(T& item) {
            Node* node;
            size_t pos = head_.load(std::memory_order_relaxed);
            for (;;) {
                node = &buffer_[pos & mask_];
                size_t seq = node->seq.load(std::memory_order_acquire);
                intptr_t dif = (intptr_t)seq - (intptr_t)(pos + 1);
                if (dif == 0) {
                    if (head_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
                        break;
                }
                else if (dif < 0) {
                    return false; // queue empty
                }
                else {
                    pos = head_.load(std::memory_order_relaxed);
                }
            }
            item = std::move(node->data);
            node->seq.store(pos + capacity_, std::memory_order_release);
            return true;
        }

        // Add this method to check if the queue is empty
        bool empty() const {
            return head_.load(std::memory_order_relaxed) == tail_.load(std::memory_order_relaxed);
        }

        size_t approximate_size() const {
            auto head = head_.load(std::memory_order_relaxed);
            auto tail = tail_.load(std::memory_order_relaxed);
            return tail - head;
        }

        size_t capacity() const {
            return capacity_;
        }

    private:
        struct Node {
            std::atomic<size_t> seq;
            T                    data;
        };

        const size_t     capacity_;
        const size_t     mask_;
        Node* buffer_;
        std::atomic<size_t> head_;
        std::atomic<size_t> tail_;

        // non‑copyable
        MPMCQueue(const MPMCQueue&) = delete;
        MPMCQueue& operator=(const MPMCQueue&) = delete;
    };
}
