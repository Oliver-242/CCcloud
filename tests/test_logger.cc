#include "logger/async_logger.hpp"
#include <thread>
#include <vector>
#include <iostream>

int main() {
    constexpr int thread_count = 10;
    constexpr int logs_per_thread = 10000;

    // 拿到全局logger实例（构造时已经start了）
    AsyncLogger& logger = AsyncLogger::instance();

    std::vector<std::thread> threads;

    // 启动多个生产者线程
    for (int i = 0; i < thread_count; ++i) {
        threads.emplace_back([i, &logger]() {
            for (int j = 0; j < logs_per_thread; ++j) {
                logger.append(LogEntry(Level::INFO, "Thread " + std::to_string(i) + " log " + std::to_string(j)));
            }
        });
    }

    // 等待所有线程完成
    for (auto& t : threads) {
        t.join();
    }

    // 停止Logger（flush剩余日志）
    logger.stop();

    std::cout << "Test completed: " << (thread_count * logs_per_thread) << " logs written." << std::endl;
    return 0;
}
