#include <atomic>
#include <thread>
#include <mutex>
#include <list>

#include "BaseQueue.hpp"

static constexpr int RETIRE_THRESHOLD = 64;
static constexpr int EPOCH_ADVANCE_INTERVAL_MS = 100;

struct EBRNodeBase {
    virtual ~EBRNodeBase() = default;
};

class EBRManager {
public:
    struct alignas(64) ThreadControlBlock {
        std::atomic<bool> active{false};
        std::atomic<int> local_epoch{0};
        std::list<std::pair<EBRNodeBase*, int>> retired_nodes;
        int retire_count = 0;
    };

    static EBRManager& instance() {
        static EBRManager inst;
        return inst;
    }

    ThreadControlBlock* register_thread() {
        std::lock_guard<std::mutex> lock(mtx);
        auto* tcb = new ThreadControlBlock();
        tcbs.push_back(tcb);
        return tcb;
    }

    void enter_critical(ThreadControlBlock* tcb) {
        tcb->local_epoch.store(global_epoch.load(std::memory_order_relaxed), std::memory_order_relaxed);
        tcb->active.store(true, std::memory_order_release);
    }

    void exit_critical(ThreadControlBlock* tcb) {
        tcb->active.store(false, std::memory_order_release);
    }

    void retire(ThreadControlBlock* tcb, EBRNodeBase* node) {
        tcb->retired_nodes.emplace_back(node, global_epoch.load(std::memory_order_relaxed));
        if (++tcb->retire_count >= RETIRE_THRESHOLD) {
            // try_advance_epoch();
            try_reclaim(tcb);
            tcb->retire_count = 0;
        }
    }

    void start_reclaimer() {
        reclaimer = std::thread([this]() {
            while (!stop_flag.load(std::memory_order_acquire)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(EPOCH_ADVANCE_INTERVAL_MS));
                try_advance_epoch();
                std::lock_guard<std::mutex> lock(mtx);
                for (auto* tcb : tcbs) {
                    try_reclaim(tcb);
                }
            }
        });
    }

    void stop_reclaimer() {
        stop_flag.store(true, std::memory_order_release);
        if (reclaimer.joinable()) reclaimer.join();
    }

    int current_epoch() const { return global_epoch.load(std::memory_order_acquire); }

private:
    std::vector<ThreadControlBlock*> tcbs;
    std::mutex mtx;
    std::atomic<int> global_epoch{0};
    std::atomic<bool> stop_flag{false};
    std::thread reclaimer;

    void try_advance_epoch() {
        int cur = global_epoch.load(std::memory_order_acquire);
        for (auto* tcb : tcbs) {
            if (tcb->active.load(std::memory_order_acquire) && tcb->local_epoch.load(std::memory_order_acquire) <= cur) {
                return; // cannot advance yet
            }
        }
        global_epoch.compare_exchange_strong(cur, cur + 1, std::memory_order_acq_rel);
    }

    void try_reclaim(ThreadControlBlock* tcb) {
        int cur = global_epoch.load(std::memory_order_acquire);
        auto& rl = tcb->retired_nodes;
        auto it = rl.begin();
        while (it != rl.end()) {
            if (it->second + 2 <= cur) {
                delete it->first;
                it = rl.erase(it);
            } else {
                ++it;
            }
        }
    }

    EBRManager() = default;
    ~EBRManager() {
        stop_reclaimer();
        for (auto* tcb : tcbs) delete tcb;
    }
};

// MPMCQueue implementation

template<typename T>
class MPMCQueue: public BaseQueue {
private:
    struct Node : public EBRNodeBase {
        T data;
        std::atomic<Node*> next;
        explicit Node(const T& d) : data(d), next(nullptr) {}
    };

    alignas(std::hardware_destructive_interference_size) std::atomic<Node*> head;
    alignas(std::hardware_destructive_interference_size) std::atomic<Node*> tail;
    thread_local static EBRManager::ThreadControlBlock* local_tcb;
    std::atomic<long long> size{0};
    static constexpr int MAX_SIZE = 10000;

public:
    MPMCQueue() {
        Node* dummy = new Node(T());
        head.store(dummy, std::memory_order_relaxed);
        tail.store(dummy, std::memory_order_relaxed);
        EBRManager::instance().start_reclaimer();
    }

    static MPMCQueue<T>& instance() {
        static MPMCQueue<T> instance;
        return instance;
    }

    ~MPMCQueue() {
        T tmp;
        while (dequeue(tmp));
        delete head.load(std::memory_order_relaxed);
    }

    void enqueue(T&& item) override {
        while (size.load(std::memory_order_relaxed) >= MAX_SIZE) {
            // 简单版：如果满了，忙等；可以后面优化成条件变量等待
            std::this_thread::yield();
        }

        Node* node = new Node(item);
        while (true) {
            Node* last = tail.load(std::memory_order_acquire);
            Node* next = last->next.load(std::memory_order_acquire);
            if (next == nullptr) {
                if (last->next.compare_exchange_weak(next, node, std::memory_order_release, std::memory_order_relaxed)) {
                    size.fetch_add(1, std::memory_order_relaxed);
                    tail.compare_exchange_weak(last, node, std::memory_order_release, std::memory_order_relaxed);
                    return;
                }
            } else {
                tail.compare_exchange_weak(last, next, std::memory_order_release, std::memory_order_relaxed);
            }
        }
    }

    bool dequeue(T& result) override {
        if (!local_tcb) local_tcb = EBRManager::instance().register_thread();
        EBRManager::instance().enter_critical(local_tcb);

        while (true){
            Node* first = head.load(std::memory_order_acquire);
            Node* next = first->next.load(std::memory_order_acquire);
            if (next == nullptr) {
                return false;
            }
            if (head.compare_exchange_strong(first, next, std::memory_order_acq_rel, std::memory_order_relaxed)) {
                result = next->data;
                size.fetch_sub(1, std::memory_order_relaxed);
                EBRManager::instance().retire(local_tcb, first);
                EBRManager::instance().exit_critical(local_tcb);
                return true;
            }
        }

        EBRManager::instance().exit_critical(local_tcb);
        return false; // non-blocking: don't retry
    }

    bool empty() const override {
        return size.load(std::memory_order_relaxed) == 0;
    }
};

template<typename T>
thread_local typename EBRManager::ThreadControlBlock* MPMCQueue<T>::local_tcb = nullptr;