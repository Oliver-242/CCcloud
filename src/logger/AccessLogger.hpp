#pragma once

#include <chrono>
#include <string>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include "async_logger.hpp"

class AccessLogger {
public:
    static std::string generate_uuid() {
        static thread_local boost::uuids::random_generator gen;
        return boost::uuids::to_string(gen());
    }

    static void log_prepare(const std::string& uuid,
                            grpc::ServerContext* context,
                            OperationType op,
                            const std::string& params) {
        LogEntry log;
        log.uuid = uuid;
        log.timestamp = std::chrono::system_clock::now();
        log.log_type = LogType::PREPARE;
        log.level = Level::INFO;
        log.operation = op;
        parse_context_info(context, log);
        log.params = params;
        log.status_code = grpc::StatusCode::OK;
        static_assert(std::is_class_v<AsyncLogger<>>, "AsyncLogger not visible");

        AsyncLogger<>::instance().append(std::move(log));
    }

    static void log_commit(const std::string& uuid,
                           grpc::ServerContext* context,
                           OperationType op,
                           const std::string& params,
                           grpc::StatusCode code,
                           long long duration_ms,
                           const std::string& error_msg = "") {
        LogEntry log;
        log.uuid = uuid;
        log.timestamp = std::chrono::system_clock::now();
        log.log_type = LogType::COMMIT;
        log.level = code == grpc::StatusCode::OK ? Level::INFO : Level::ERROR;
        log.operation = op;
        parse_context_info(context, log);
        log.params = params;
        log.status_code = code;
        log.duration_ms = duration_ms;
        log.error_message = error_msg;

        AsyncLogger<>::instance().append(std::move(log));
    }

    static void log_abort(const std::string& uuid,
                          grpc::ServerContext* context,
                          OperationType op,
                          grpc::StatusCode code,
                          const std::string& reason,
                          long long duration_ms) {
        log_commit(uuid, context, op, "", code, duration_ms, reason);
    }

private:
    static void parse_context_info(grpc::ServerContext* context, LogEntry& log) {
        std::string peer = context->peer();  // ipv4:192.168.1.5:5000
        log.client_ip = peer;
        log.client_port = 0;  // 可补充解析
        log.server_ip = "127.0.0.1"; // 如部署时可配置真实 IP
        log.server_port = 0;
    }
};
