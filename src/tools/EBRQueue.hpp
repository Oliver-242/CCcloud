#pragma once
#include <iostream>
#include <atomic>
#include <thread>
#include <vector>
#include <optional>
#include <memory>
#include <functional>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <vector>
#include <numeric> // for std::iota
#include <chrono> // for sleep_for, etc.

#if __cplusplus >= 202002L
#include <concepts>
#endif

#include "BaseQueue.hpp"

#ifdef __cpp_lib_hardware_interference_size
    inline constexpr size_t hardware_destructive_interference_size = std::hardware_destructive_interference_size;
#else
    inline constexpr size_t hardware_destructive_interference_size = 64;
#endif

class EBRManager {
private:
    struct RetiredNode {
        void* ptr;
        std::function<void(void*)> deleter;
        uint64_t epoch;
    };

    struct alignas(hardware_destructive_interference_size) ThreadData {
        std::atomic<uint64_t> local_epoch{UINT64_MAX}; // UINT64_MAX 表示不活跃
        std::vector<RetiredNode> retired_list;
        std::mutex list_mutex; // 保护 retired_list
        bool registered = false;

        ThreadData() = default;
        ThreadData(const ThreadData&) = delete;
        ThreadData& operator=(const ThreadData&) = delete;
        ThreadData(ThreadData&&) = delete;
        ThreadData& operator=(ThreadData&&) = delete;
    };

    alignas(hardware_destructive_interference_size) std::atomic<uint64_t> global_epoch{0};
    std::shared_mutex registry_mutex;
    std::unordered_map<std::thread::id, std::unique_ptr<ThreadData>> thread_registry;

    static thread_local ThreadData* current_thread_data;
    static thread_local bool needs_registration;


    void register_thread_if_needed() {
        if (needs_registration) {
            std::thread::id tid = std::this_thread::get_id();
            std::unique_lock lock(registry_mutex);
            if (thread_registry.find(tid) == thread_registry.end()) {
                auto data = std::make_unique<ThreadData>();
                current_thread_data = data.get();
                thread_registry[tid] = std::move(data);
                current_thread_data->registered = true;
            } else {
                 current_thread_data = thread_registry[tid].get();
            }
            needs_registration = false;
        }
        if (!current_thread_data) {
             std::thread::id tid = std::this_thread::get_id();
             std::shared_lock lock(registry_mutex);
             auto it = thread_registry.find(tid);
             if (it != thread_registry.end()) {
                 current_thread_data = it->second.get();
             } else {
                 lock.unlock();
                 std::unique_lock write_lock(registry_mutex);
                 if (thread_registry.find(tid) == thread_registry.end()) {
                     auto data = std::make_unique<ThreadData>();
                     current_thread_data = data.get();
                     thread_registry[tid] = std::move(data);
                     current_thread_data->registered = true;
                 } else {
                    current_thread_data = thread_registry[tid].get();
                 }
             }
        }
    }

    void try_reclaim(ThreadData* data) {
        uint64_t current_global_epoch = global_epoch.load(std::memory_order_acquire);
        if (current_global_epoch < 2) return;
        uint64_t safe_epoch = current_global_epoch - 2;

        std::vector<RetiredNode> still_retired;
        std::vector<RetiredNode> to_delete;
        {
            std::lock_guard list_lock(data->list_mutex);
             for (auto& node : data->retired_list) {
                 if (node.epoch <= safe_epoch) {
                     to_delete.push_back(std::move(node));
                 } else {
                     still_retired.push_back(std::move(node));
                 }
             }
             data->retired_list = std::move(still_retired);
        }

        for (const auto& node : to_delete) {
            node.deleter(node.ptr);
        }
    }

    EBRManager() = default;

public:
    static EBRManager& instance() {
        static EBRManager instance;
        return instance;
    }    

    ~EBRManager() {
         bump_epoch_often();
         bump_epoch_often();
         bump_epoch_often();

         std::unique_lock lock(registry_mutex);
         for (auto& pair : thread_registry) {
             try_reclaim(pair.second.get());
             std::lock_guard list_lock(pair.second->list_mutex);
             for (const auto& node : pair.second->retired_list) {
                 node.deleter(node.ptr);
             }
             pair.second->retired_list.clear();
         }
         thread_registry.clear();
    }

    EBRManager(const EBRManager&) = delete;
    EBRManager& operator=(const EBRManager&) = delete;
    EBRManager(EBRManager&&) = delete;
    EBRManager& operator=(EBRManager&&) = delete;

    void enter() {
        register_thread_if_needed();
        if (current_thread_data) {
            uint64_t current_epoch = global_epoch.load(std::memory_order_relaxed);
            current_thread_data->local_epoch.store(current_epoch, std::memory_order_relaxed);
        } else {
             std::cerr << "Error: Thread data not available in enter()" << std::endl;
        }
    }

    void leave() {
         if (current_thread_data) {
            current_thread_data->local_epoch.store(UINT64_MAX, std::memory_order_relaxed);
            try_reclaim(current_thread_data);
        } else {
             std::cerr << "Error: Thread data not available in leave()" << std::endl;
        }
    }

    template <typename T>
    void retire(T* ptr) {
         if (!ptr) return;
         register_thread_if_needed();
         if (current_thread_data) {
            uint64_t current_epoch = global_epoch.load(std::memory_order_relaxed);
            std::function<void(void*)> deleter = [](void* p){ delete static_cast<T*>(p); };
            RetiredNode node{static_cast<void*>(ptr), deleter, current_epoch};
            std::lock_guard lock(current_thread_data->list_mutex);
            current_thread_data->retired_list.push_back(std::move(node));
        } else {
            std::cerr << "Error: Thread data not available in retire(). Leaking memory." << std::endl;
        }
    }

    void bump_epoch() {
        global_epoch.fetch_add(1, std::memory_order_release);
    }

    void bump_epoch_often() {
        for(int i = 0; i < 5; ++i) {
            bump_epoch();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

     void unregister_thread() {
         std::thread::id tid = std::this_thread::get_id();
         std::unique_lock lock(registry_mutex);
         auto it = thread_registry.find(tid);
         if (it != thread_registry.end()) {
             ThreadData* data = it->second.get();
             try_reclaim(data);
             {
                 std::lock_guard list_lock(data->list_mutex);
                 for (const auto& node : data->retired_list) {
                     node.deleter(node.ptr);
                 }
                 data->retired_list.clear();
             }
             thread_registry.erase(it);
             current_thread_data = nullptr;
             needs_registration = true;
         }
    }
};

thread_local EBRManager::ThreadData* EBRManager::current_thread_data = nullptr;
thread_local bool EBRManager::needs_registration = true;

template <typename T>
#if __cplusplus >= 202002L
    requires std::movable<T> // 假设 T 是可移动的
#endif
class MPMCQueue final : public BaseQueue<T> { // 使用 final 明确无后续继承，继承 BaseQueue
private:
    struct Node {
        std::optional<T> data;
        std::atomic<Node*> next{nullptr};

        Node() = default;
        explicit Node(const T& val) : data(val) {}
        Node(T&& val) noexcept : data(move(val)) {}
    };

    alignas(hardware_destructive_interference_size) std::atomic<Node*> head;
    alignas(hardware_destructive_interference_size) std::atomic<Node*> tail;

    MPMCQueue() {
        Node* dummy = new Node();
        head.store(dummy, std::memory_order_relaxed);
        tail.store(dummy, std::memory_order_relaxed);
    }

    ~MPMCQueue() override {
        T temp_val;
        while (dequeue(temp_val));
        Node* dummy = head.load(std::memory_order_relaxed);
        if (dummy) {
            delete dummy;
        }
    }

public:
    static MPMCQueue<T>& instance() {
        static MPMCQueue<T> instance;
        return instance;
    }

    MPMCQueue(const MPMCQueue&) = delete;
    MPMCQueue& operator=(const MPMCQueue&) = delete;

    /**
     * @brief Enqueues a value into the queue (thread-safe).
     * Implements BaseQueue::enqueue.
     * @param value The value to enqueue (passed by const reference).
     */
    void enqueue(const T& value) override {
        Node* new_node = new Node(value);
        EBRManager::instance().enter();
        Node* current_tail = nullptr;
        while (true) {
            current_tail = tail.load(std::memory_order_acquire);
            Node* next_node = current_tail->next.load(std::memory_order_acquire);

            if (current_tail == tail.load(std::memory_order_acquire)) {
                if (next_node == nullptr) {
                    if (current_tail->next.compare_exchange_weak(
                            next_node, new_node,
                            std::memory_order_release, std::memory_order_relaxed))
                    {
                        tail.compare_exchange_weak(
                            current_tail, new_node,
                            std::memory_order_release, std::memory_order_relaxed);
                            EBRManager::instance().leave();
                        return;
                    }
                } else {
                    tail.compare_exchange_weak(
                        current_tail, next_node,
                        std::memory_order_release, std::memory_order_relaxed);
                }
            }
        }
    }

    /**
     * @brief Dequeues a value from the queue (thread-safe).
     * Implements BaseQueue::dequeue.
     * @param value Reference to store the dequeued value.
     * @return true if a value was successfully dequeued, false otherwise.
     */
    bool dequeue(T& value) override { // 重命名并使用 override
        EBRManager::instance().enter();
        Node* current_head = nullptr;
        Node* current_tail = nullptr;
        Node* next_node = nullptr;

        while (true) {
            current_head = head.load(std::memory_order_acquire);
            current_tail = tail.load(std::memory_order_acquire);
            next_node = current_head->next.load(std::memory_order_acquire);

            if (current_head == head.load(std::memory_order_acquire)) {
                if (current_head == current_tail) {
                    if (next_node == nullptr) {
                        EBRManager::instance().leave();
                        return false; // Queue is empty
                    }
                    tail.compare_exchange_weak(
                        current_tail, next_node,
                        std::memory_order_release, std::memory_order_relaxed);
                } else {
                    if (next_node == nullptr) {
                         continue; // Should not happen if head != tail, retry
                    }
                    // Try to move head
                    if (head.compare_exchange_weak(
                            current_head, next_node,
                            std::memory_order_acq_rel, std::memory_order_relaxed))
                    {
                        // Dequeue successful
                        if (next_node->data.has_value()) {
                            value = std::move(*(next_node->data)); // Move data out
                            next_node->data.reset();
                        } else {
                             // Problem: Dequeued node has no data (should be impossible unless it's the dummy node?)
                             EBRManager::instance().leave();
                             // This indicates a potential logic error or race we didn't handle.
                             // For robustness, maybe return false or throw. Let's return false here.
                             return false;
                        }
                        EBRManager::instance().retire(current_head); // Retire the old head
                        EBRManager::instance().leave();
                        return true;
                    }
                }
            }
        }
    }

    /**
     * @brief Checks if the queue is likely empty (thread-safe, snapshot check).
     * Implements BaseQueue::empty.
     * @return true if the queue appeared empty at the moment of check, false otherwise.
     * @warning Due to concurrency, the state might change immediately after this call.
     */
    bool empty() const override {
        // 需要 const_cast 来调用非 const 的 ebr.enter/leave
        // 或者提供 const 版本的 EBR enter/leave (如果只读访问不需要更新本地纪元，但仍需保护指针读取)
        // 这里简单用 const_cast，假设 EBR enter/leave 本身线程安全且逻辑上允许来自 const 方法
        // 一个更好的 EBR 设计会区分读/写临界区。
        const_cast<EBRManager&>(EBRManager::instance()).enter();

        Node* current_head = head.load(std::memory_order_acquire);
        // We also need tail to differentiate between empty and tail lagging
        Node* current_tail = tail.load(std::memory_order_acquire);
        Node* next_node = current_head->next.load(std::memory_order_acquire);

        bool is_empty = (current_head == current_tail) && (next_node == nullptr);

        const_cast<EBRManager&>(EBRManager::instance()).leave();
        return is_empty;
    }
};
