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
#include <filesystem>
#define DEBUG
#ifdef DEBUG
#include <iostream>
#endif

#include "tools/lockfreequeue.hpp"

#define DEFAULT_LOG_PATH "/home/personal/CCcloud/logs" // 默认日志文件路径
static constexpr int LOGENTRY_BATCH_THRESHOLD = 512; // 批量写入日志的阈值
static constexpr int LOGENTRY_BATCH_TIMEOUT_MS = 256; // 批量写入日志的超时时间
static constexpr int MAX_LOG_FILE_SIZE = 10 * 1024 * 1024; // 每个日志文件最大大小 10MB


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
    static AsyncLogger& instance(const std::string& file_path = DEFAULT_LOG_PATH) {
        static AsyncLogger logger(file_path);
        return logger;
    }

    AsyncLogger(const AsyncLogger&) = delete;
    AsyncLogger& operator=(const AsyncLogger&) = delete;
    AsyncLogger(AsyncLogger&&) = delete;
    AsyncLogger& operator=(AsyncLogger&&) = delete;

    // 提交日志
    void append(LogEntry&& entry) {
        log_queue_.enqueue(std::move(entry));
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
        : file_path_(file_path), running_(false), current_file_index_(1), log_queue_(MPMCQueue<LogEntry>::instance()) {
        namespace fs = std::filesystem;
        if (!fs::exists(file_path) || !fs::is_directory(file_path)) {
            if (!fs::create_directories(file_path)) {
                throw std::runtime_error("Failed to create log root directory: " + file_path);
            }
        }
            start();
        }

    ~AsyncLogger() {
        stop();
    }

    std::string get_current_log_file_path() const {
        namespace fs = std::filesystem;
        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        std::tm tm_time;
#if defined(_WIN32) || defined(_WIN64)
        localtime_s(&tm_time, &t);
#else
        localtime_r(&t, &tm_time);
#endif
        std::stringstream ss;
        ss << std::put_time(&tm_time, "%Y-%m-%d");
        fs::path date_dir = fs::path(file_path_) / ss.str();
        if (!fs::exists(date_dir)) {
            if (!fs::create_directories(date_dir)) {
                throw std::runtime_error("Failed to create date directory: " + date_dir.string());
            }
            return (date_dir / "1.txt").string();
        } else {
            std::error_code ec;
            uintmax_t f_size = fs::file_size(date_dir / (std::to_string(current_file_index_) + ".txt"), ec);
#ifdef DEBUG
            std::cout << "Current file index: " << current_file_index_ << ", File size: " << f_size << std::endl;
#endif
            if (ec) {
                throw std::runtime_error("Failed to get file size: " + (date_dir / (std::to_string(current_file_index_) + ".txt")).string());
            }
            if (f_size >= MAX_LOG_FILE_SIZE) {
                std::cout << "File size exceeded, creating new file." << std::endl;
                ++current_file_index_;
                std::ofstream ofs((date_dir / (std::to_string(current_file_index_) + ".txt")).string(), std::ios::app);
                if (!ofs.is_open()) {
                    throw std::runtime_error("Failed to create log file: " + (date_dir / (std::to_string(current_file_index_) + ".txt")).string());
                }
                ofs.close();
                return (date_dir / (std::to_string(current_file_index_) + ".txt")).string();
            }
            return (date_dir / (std::to_string(current_file_index_) + ".txt")).string();
        }
    }

    void background_flush() {
        std::ofstream ofs;

        auto open_log_file = [&ofs, this]() {
            std::string current_log_file = get_current_log_file_path();
            ofs.open(current_log_file, std::ios::app);
            if (!ofs.is_open()) {
                throw std::runtime_error("Failed to open log file: " + current_log_file);
            }
        };

        while (true) {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait_for(
                lock, 
                std::chrono::milliseconds(LOGENTRY_BATCH_TIMEOUT_MS), 
                [this]() { return !running_ || !log_queue_.empty(); }
            );
            lock.unlock();

            open_log_file();
            std::vector<LogEntry> entries;
            int cnt = 0;
            while (!log_queue_.empty() && cnt < LOGENTRY_BATCH_THRESHOLD) {
                LogEntry entry;
                if (log_queue_.dequeue(entry)) {
                    entries.emplace_back(std::move(entry));
                    cnt++;
                }
            }

            ofs << format_log_entry(entries);
            ofs.flush();

            if (!running_ && log_queue_.empty()) {
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
    MPMCQueue<LogEntry>& log_queue_;
    std::condition_variable cv_;
    std::atomic<bool> running_;
    std::atomic<int> unflushed_count_{0};
    std::thread log_thread_;
    mutable int current_file_index_; // 用于记录当前日志文件序号
};
