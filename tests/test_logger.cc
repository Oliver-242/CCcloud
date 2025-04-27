#include "logger/async_logger.hpp"
#include <thread>
#include <vector>
#include <iostream>

int main() {
    constexpr int thread_count = 24;
    constexpr int logs_per_thread = 100000;

    // 拿到全局logger实例（构造时已经start了）
    AsyncLogger& logger = AsyncLogger::instance("/home/olivercai/personal/CCcloud/logs/");

    std::vector<std::thread> threads;

    // 启动多个生产者线程
    for (int i = 0; i < thread_count; ++i) {
        threads.emplace_back([i, &logger]() {
            for (int j = 0; j < logs_per_thread; ++j) {
                std::ostringstream oss;
                oss << "Hahaha! *** Thread " << i << " log " << j << " ***";
                logger.append(LogEntry(Level::INFO, oss.str()));
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
