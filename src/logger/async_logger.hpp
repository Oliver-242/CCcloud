#pragma once
#include <thread>
#include <mutex>
#include <condition_variable>
#include <fstream>
#include <string>
#include <atomic>
#include <chrono>
#include <sstream>
#include <iomanip>

#include "tools/lockfreequeue.hpp"


struct LogEntry {
    std::chrono::system_clock::time_point timestamp;
    std::string level;    // INFO, WARN, ERROR
    std::string message;  // 日志内容

    LogEntry(const std::string& lvl, const std::string& msg)
        : timestamp(std::chrono::system_clock::now()), level(lvl), message(msg) {}
};

class AsyncLogger {
public:
    static AsyncLogger& instance() {
        static AsyncLogger logger("async_log.txt");
        return logger;
    }

    // 禁止拷贝和赋值
    AsyncLogger(const AsyncLogger&) = delete;
    AsyncLogger& operator=(const AsyncLogger&) = delete;

    // 提交日志（生产者接口）
    void append(LogEntry&& entry) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            log_queue_.enqueue(std::move(entry));
        }
        cv_.notify_one();
    }

    // 启动后台线程
    void start() {
        running_ = true;
        log_thread_ = std::thread(&AsyncLogger::background_flush, this);
    }

    // 停止后台线程，flush所有日志
    void stop() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            running_ = false;
        }
        cv_.notify_one();
        if (log_thread_.joinable()) {
            log_thread_.join();
        }
    }

private:
    AsyncLogger(const std::string& file_path)
        : file_path_(file_path), running_(false) {}

    ~AsyncLogger() {
        stop();
    }

    void background_flush() {
        std::ofstream ofs(file_path_, std::ios::app);
        if (!ofs.is_open()) {
            throw std::runtime_error("Failed to open log file: " + file_path_);
        }

        while (true) {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this]() { return !running_ || !log_queue_.empty(); });

            while (!log_queue_.empty()) {
                const LogEntry& entry = log_queue_.front();
                ofs << format_log_entry(entry) << "\n";
                log_queue_.pop();
            }

            ofs.flush();

            if (!running_ && log_queue_.empty()) {
                break;
            }
        }
    }

    std::string format_log_entry(const LogEntry& entry) {
        std::time_t t = std::chrono::system_clock::to_time_t(entry.timestamp);
        std::tm tm_time;
#if defined(_WIN32) || defined(_WIN64)
        localtime_s(&tm_time, &t);
#else
        localtime_r(&t, &tm_time);
#endif
        std::ostringstream oss;
        oss << "[" << std::put_time(&tm_time, "%Y-%m-%d %H:%M:%S") << "]";
        oss << " [" << entry.level << "] ";
        oss << entry.message;
        return oss.str();
    }

private:
    std::string file_path_;
    MPMCQueue<LogEntry> log_queue_;   //TODO: 使用lock-free队列MPMCQueue
    std::mutex mutex_;
    std::condition_variable cv_;
    std::atomic<bool> running_;
    std::thread log_thread_;
};
