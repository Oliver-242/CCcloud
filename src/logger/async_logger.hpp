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
#include <format>
#include <grpcpp/grpcpp.h>
// #define DEBUG
#ifdef DEBUG
#include <iostream>
#endif

#include "tools/BaseQueue.hpp"

#define DEFAULT_LOG_PATH "/home/olivercai/personal/CCcloud/logs" // 默认日志文件路径
static constexpr int LOGENTRY_BATCH_THRESHOLD = 256; // 批量写入日志的阈值
static constexpr int LOGENTRY_BATCH_TIMEOUT_MS = 256; // 批量写入日志的超时时间
static constexpr int MAX_LOG_FILE_SIZE = 32 * 1024 * 1024; // 每个日志文件最大大小 32MB


enum class Level {
    INFO,
    WARN,
    ERROR
};

enum class LogType {
    PREPARE,
    COMMIT,
    ABORT
};

enum class OperationType {
    UPLOAD,
    DOWNLOAD,
    DELETE
};

struct LogEntry {
    std::chrono::system_clock::time_point timestamp;
    std::string uuid;
    Level level;    // INFO, WARN, ERROR
    std::string client_ip;
    std::string server_ip;
    uint32_t client_port;
    uint32_t server_port;
    OperationType operation;           // Upload / Download / Delete
    std::string params;              // filename=abc.jpg size=2048
    LogType log_type;
    grpc::StatusCode status_code;
    std::string error_message;
    long long duration_ms;

    LogEntry()
    : timestamp(std::chrono::system_clock::now()),
      level(Level::INFO),
      client_port(0),
      server_port(0),
      operation(OperationType::UPLOAD),
      log_type(LogType::PREPARE),
      status_code(grpc::StatusCode::OK),
      duration_ms(0) {}

    LogEntry(Level lvl,
            const std::string& uuid,
            const std::string& cip,
            const std::string& sip,
            uint32_t cport,
            uint32_t sport,
            OperationType op,
            const std::string& param,
            LogType ltype,
            grpc::StatusCode code,
            const std::string& errmsg,
            long long dur)
        : timestamp(std::chrono::system_clock::now()),
          uuid(uuid),
          level(lvl),
          client_ip(cip),
          server_ip(sip),
          client_port(cport),
          server_port(sport),
          operation(op),
          params(param),
          log_type(ltype),
          status_code(code),
          error_message(errmsg),
          duration_ms(dur) {}

    LogEntry(const LogEntry&) = default;
    LogEntry(LogEntry&&) = default;
    LogEntry& operator=(const LogEntry&) = default;
    LogEntry& operator=(LogEntry&&) = default;
};

template <typename T>
concept HasValueType = requires {
    typename T::value_type;
};

template <typename T>
concept DerivedFromBaseQueue = HasValueType<T> && std::is_base_of_v<BaseQueue<typename T::value_type>, T>;

template <DerivedFromBaseQueue Q>
class AsyncLogger {
public:
    template <typename... Args>
    static AsyncLogger& instance(const std::string& file_path = DEFAULT_LOG_PATH, Args&&... args) {
        static AsyncLogger logger(file_path, std::forward<Args>(args)...);
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
    template <typename... Args>
    AsyncLogger(const std::string& file_path, Args&&... args)
        : file_path_(file_path), running_(false), current_file_index_(1), log_queue_(Q::instance(std::forward<Args>(args)...)) {
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
                std::chrono::milliseconds(LOGENTRY_BATCH_TIMEOUT_MS),    // 超时后即便没满足批量更新的日志数量也会落盘
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
#ifdef DEBUG
            std::cout << "Flushing " << entries.size() << " log entries." << std::endl;
#endif
            ofs << format_log_entry(entries);
            ofs.flush();
            ofs.close();
            if (!running_ && log_queue_.empty()) {
                break;
            }
        }
    }

    std::string format_log_entry(const std::vector<LogEntry>& entries) {  //支持到微秒
        std::ostringstream oss;

    for (const auto& entry : entries) {
        auto in_time_t = std::chrono::system_clock::to_time_t(entry.timestamp);
        auto micros = std::chrono::duration_cast<std::chrono::microseconds>(
            entry.timestamp.time_since_epoch()).count() % 1000000;

        std::tm buf;
    #if defined(_WIN32) || defined(_WIN64)
        localtime_s(&buf, &in_time_t);
    #else
        localtime_r(&in_time_t, &buf);
    #endif

        oss << "[" << std::put_time(&buf, "%Y-%m-%d_%H:%M:%S");
        oss << "." << std::setfill('0') << std::setw(6) << micros << "]";
        oss << " [" << entry.uuid << "]";
        oss << " [" << loglevel_to_string(entry.level) << "]";
        oss << " [" << logtype_to_string(entry.log_type) << "]";
        oss << " [" << operation_to_string(entry.operation) << "]";

        oss << " client=" << entry.client_ip << ":" << entry.client_port;
        oss << " server=" << entry.server_ip << ":" << entry.server_port;

        if (entry.log_type == LogType::PREPARE) {
            oss << " params=" << entry.params;
        } else if (entry.log_type == LogType::COMMIT || entry.log_type == LogType::ABORT) {
            oss << " result=" << (entry.status_code == grpc::StatusCode::OK ? "OK" : "FAILED");
            oss << " code=" << static_cast<int>(entry.status_code);
            if (!entry.error_message.empty()) {
                oss << " error=" << entry.error_message;
            }
            oss << " time=" << entry.duration_ms << "ms";
        }

        oss << "\n";
    }

    return oss.str();
    }

    std::string loglevel_to_string(const Level& level) const {
        switch (level) {
            case Level::INFO: return "INFO";
            case Level::WARN: return "WARN";
            case Level::ERROR: return "ERROR";
            default: return "UNKNOWN";
        }
    }

    std::string logtype_to_string(const LogType& type) const {
        switch (type) {
            case LogType::PREPARE: return "PREPARE";
            case LogType::COMMIT: return "COMMIT";
            case LogType::ABORT: return "ABORT";
            default: return "UNKNOWN";
        }
    }

    std::string operation_to_string(OperationType op) {
        switch (op) {
            case OperationType::UPLOAD: return "UPLOAD";
            case OperationType::DOWNLOAD: return "DOWNLOAD";
            case OperationType::DELETE: return "DELETE";
            default: return "UNKNOWN";
        }
    }

private:
    std::string file_path_;
    std::mutex mutex_;
    Q& log_queue_;
    std::condition_variable cv_;
    std::atomic<bool> running_;
    std::atomic<int> unflushed_count_{0};
    std::thread log_thread_;
    mutable int current_file_index_; // 用于记录当前日志文件序号
};
