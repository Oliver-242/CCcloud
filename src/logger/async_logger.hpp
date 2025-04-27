#pragma once
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <fstream>
#include <string>
#include <atomic>
#include <chrono>
#include <sstream>
#include <iomanip>

#include "tools/lockfreequeue.hpp"

static const int LOGENTRY_BATCH_THRESHOLD = 256; // 批量写入日志的阈值
static const int LOGENTRY_BATCH_TIMEOUT_MS = 256; // 批量写入日志的超时时间

enum class Level {
    INFO,
    WARN,
    ERROR
};

struct LogEntry {
    std::chrono::system_clock::time_point timestamp;
    Level level;    // INFO, WARN, ERROR
    std::string message;  

    LogEntry(const Level& lvl, const std::string& msg)
        : timestamp(std::chrono::system_clock::now()), level(lvl), message(msg) {}

    LogEntry() : timestamp(std::chrono::system_clock::now()), level(Level::INFO), message("") {}

    LogEntry(const LogEntry&) = default;
    LogEntry(LogEntry&&) = default;
    LogEntry& operator=(const LogEntry&) = default;
    LogEntry& operator=(LogEntry&&) = default;
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
    AsyncLogger(AsyncLogger&&) = delete;
    AsyncLogger& operator=(AsyncLogger&&) = delete;

    // 提交日志（生产者接口）
    void append(LogEntry&& entry) {
        MPMCQueue<LogEntry>::instance().enqueue(std::move(entry));
        if (++unflushed_count_ >= LOGENTRY_BATCH_THRESHOLD) {
            unflushed_count_ = 0;
            cv_.notify_one();
        }
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
        : file_path_(file_path), running_(false) {
            start();
        }

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
            cv_.wait_for(
                lock, 
                std::chrono::milliseconds(LOGENTRY_BATCH_TIMEOUT_MS), 
                [this]() { return !running_ || !MPMCQueue<LogEntry>::instance().empty(); }
            );
            lock.unlock();

            std::vector<LogEntry> entries;
            int cnt = 0;
            while (!MPMCQueue<LogEntry>::instance().empty() && cnt < LOGENTRY_BATCH_THRESHOLD) {
                LogEntry entry;
                if (MPMCQueue<LogEntry>::instance().dequeue(entry)) {
                    entries.emplace_back(std::move(entry));
                    cnt++;
                }
            }
            ofs << format_log_entry(entries);
            ofs.flush();

            if (!running_ && MPMCQueue<LogEntry>::instance().empty()) {
                break;
            }
        }
    }

    std::string format_log_entry(const std::vector<LogEntry>& entries) {
        std::ostringstream oss;
        for (const auto& entry : entries) {
            std::time_t t = std::chrono::system_clock::to_time_t(entry.timestamp);
            std::tm tm_time;
#if defined(_WIN32) || defined(_WIN64)
            localtime_s(&tm_time, &t);
#else
            localtime_r(&t, &tm_time);
#endif
            oss << "[" << std::put_time(&tm_time, "%Y-%m-%d %H:%M:%S") << "]";
            oss << " [" << loglevel_to_string(entry.level) << "] ";
            oss << entry.message << "\n";
        }
        return oss.str();
    }

    std::string loglevel_to_string(const Level& level) {
        switch (level) {
            case Level::INFO: return "INFO";
            case Level::WARN: return "WARN";
            case Level::ERROR: return "ERROR";
            default: return "UNKNOWN";
        }
    }

private:
    std::string file_path_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::atomic<bool> running_;
    std::atomic<int> unflushed_count_{0};
    std::thread log_thread_;
};
