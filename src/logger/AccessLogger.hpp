#pragma once

#include <chrono>
#include <string>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <grpcpp/server_context.h>

#include "async_logger.hpp"


template <typename _Tp>
concept CTX = is_base_of_v<grpc::ServerContextBase, _Tp>;

class AccessLogger {
public:
    static std::string generate_uuid() {
        static thread_local boost::uuids::random_generator gen;
        return boost::uuids::to_string(gen());
    }

    template <typename _CT>
    static void log_prepare(const std::string& uuid,
                            _CT* context,
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

        AsyncLogger<>::instance().append(std::move(log));
    }

    template <typename _CT>
    static void log_commit(const std::string& uuid,
                           _CT* context,
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

    template <typename _CT>
    static void log_abort(const std::string& uuid,
                          _CT* context,
                          OperationType op,
                          grpc::StatusCode code,
                          const std::string& reason,
                          long long duration_ms) {
        log_commit(uuid, context, op, "", code, duration_ms, reason);
    }

private:
    template <typename _CT>
    static void parse_context_info(_CT* context, LogEntry& log) {
        std::string peer = context->peer();  // 例如: "ipv4:192.168.1.5:5000"

        log.client_ip = "0.0.0.0"; // 解析失败时设为默认IP
        log.client_port = 0;       // 解析失败时设为默认端口

        size_t first_colon_pos = peer.find(':');

        if (first_colon_pos != std::string::npos) {
            size_t last_colon_pos = peer.rfind(':');

            if (last_colon_pos != std::string::npos && last_colon_pos > first_colon_pos) {
                try {
                    log.client_ip = peer.substr(first_colon_pos + 1, last_colon_pos - (first_colon_pos + 1));

                    std::string port_str = peer.substr(last_colon_pos + 1);
                    log.client_port = std::stoi(port_str);

                } catch (const std::invalid_argument& ia) {
                    std::cerr << "端口转换错误 (peer: " << peer << "): " << ia.what() << std::endl;
                } catch (const std::exception& e) {
                    std::cerr << "解析 peer 字符串时发生未知错误 (peer: " << peer << "): " << e.what() << std::endl;
                }
            } else {
                std::cerr << "Peer 字符串格式不正确，找不到端口分隔符或格式错误: " << peer << std::endl;
            }
        } else {
            std::cerr << "Peer 字符串格式不正确，找不到协议分隔符: " << peer << std::endl;
        }

        log.server_ip = "0.0.0.0";
        log.server_port = 9527;
    }
};
