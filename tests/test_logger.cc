#include <thread>
#include <vector>
#include <iostream>
#include <chrono>

#include "logger/async_logger.hpp"
#include "tools/RingBuffer.hpp"
#include "tools/EBRQueue.hpp"

constinit int thread_count = 12;
constinit int logs_per_thread = 100000;

int main(int argc, char* argv[]) {
    {
        if (argc == 2) {
            thread_count = std::stoi(argv[1]);
        } else if (argc == 3) {
            thread_count = std::stoi(argv[1]);
            logs_per_thread = std::stoi(argv[2]);
        }

        AsyncLogger<MPMCQueue<LogEntry>>& logger = AsyncLogger<MPMCQueue<LogEntry>>::instance("/home/olivercai/personal/CCcloud/logs/");

        std::vector<std::thread> threads;
        std::cout << "================ Logger Test ================" << std::endl;
        std::cout << "Start logger test..." << std::endl;
        std::cout << "Thread count: " << thread_count << ", Logs per thread: " << logs_per_thread << std::endl;
        auto start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < thread_count; ++i) {
            threads.emplace_back([i, &logger]() {
                for (int j = 0; j < logs_per_thread; ++j) {
                    std::ostringstream oss;
                    oss << "Hahaha! *** Thread " << i << " log " << j << " ***";
                    LogEntry le;
                    le.params = oss.str();
                    logger.append(std::move(le));
                }
            });
        }

        for (auto& t : threads) {
            t.join();
        }

        logger.stop();
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "Total: " << (thread_count * logs_per_thread) << " logs written." << std::endl;
        std::cout << "Time taken: " << duration.count() << " ms" << std::endl;
    }
    std::cout << "Test completed." << std::endl;
    std::cout << "=============================================" << std::endl;
    return 0;
}
