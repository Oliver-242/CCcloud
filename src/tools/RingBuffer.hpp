#include <vector>
#include <atomic>
#include <optional>

#include "BaseQueue.hpp"


template <typename T>
class LockFreeMPMCQueue: public BaseQueue<T> {
    static_assert(std::is_move_constructible_v<T>, "T must be move constructible");
    static_assert(std::is_copy_constructible_v<T>, "T must be copy constructible");
public:
    static LockFreeMPMCQueue& instance(size_t capacity) {
        static LockFreeMPMCQueue queue(capacity);
        return queue;
    }

    explicit LockFreeMPMCQueue(size_t capacity)
        : capacity_(capacity),
        buffer_(capacity),
        head_(0),
        tail_(0) {}

    bool try_enqueue(const T& value) {
        size_t current_tail = tail_.load(std::memory_order_acquire);
        size_t next_tail = (current_tail + 1) % capacity_;

        size_t current_head = head_.load(std::memory_order_acquire);
        if (next_tail == current_head) return false; // 队列满

        if (!tail_.compare_exchange_strong(
            current_tail, next_tail))
        {
            return false; // 竞争失败
        }

        buffer_[current_tail] = std::move(value);
        return true;
    }

    // 阻塞式入队（新增实现）
    void enqueue(const T& value) override {
        while (true) {
            // 尝试非阻塞入队
            if (try_enqueue(value)) {
                return;
            }
            std::this_thread::yield();
        }
    }

    // 阻塞式出队（新增实现）
    bool dequeue(T& value) override {
        while (true) {
            // 尝试非阻塞出队
            if (auto res = try_dequeue()) {
                value = std::move(*res);
                return true;
            }
            std::this_thread::yield();
        }
        return false;
    }

    std::optional<T> try_dequeue() {
        size_t current_head = head_.load(std::memory_order_acquire);
        size_t current_tail = tail_.load(std::memory_order_acquire);

        if (current_head == current_tail) return std::nullopt; // 队列空

        size_t new_head = (current_head + 1) % capacity_;
        if (!head_.compare_exchange_strong(
            current_head, new_head))
        {
            return std::nullopt; // 竞争失败
        }

        T value = std::move(buffer_[current_head]);
        return value;
    }

    bool empty() const override {
        return head_.load(std::memory_order_acquire) ==
            tail_.load(std::memory_order_acquire);
    }

    size_t size() const {
        size_t head = head_.load(std::memory_order_acquire);
        size_t tail = tail_.load(std::memory_order_acquire);
        return (tail - head + capacity_) % capacity_;
    }

private:
    const size_t capacity_;
    std::vector<T> buffer_;
    alignas(std::hardware_destructive_interference_size) std::atomic<size_t> head_;
    alignas(std::hardware_destructive_interference_size) std::atomic<size_t> tail_;
};