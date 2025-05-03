#pragma once
#include <grpcpp/grpcpp.h>
#include <grpcpp/support/server_callback.h>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <iostream> // Added for logging

#include "generated/file.grpc.pb.h"
#include "logger/AccessLogger.hpp"

class AsyncUploadCall : public grpc::ServerReadReactor<CCcloud::UploadChunk> {
public:
    AsyncUploadCall(grpc::CallbackServerContext* ctx,
                    CCcloud::UploadResponse* resp)
        : ctx_(ctx), resp_(resp)
    {
        uuid_ = AccessLogger::generate_uuid();
        AccessLogger::log_prepare(uuid_, ctx_, OperationType::UPLOAD, "upload started");
        StartRead(&chunk_);
        t0_ = std::chrono::steady_clock::now();
    }

    /* 读取分片 */
    void OnReadDone(bool ok) override
    {
        // Added log: Indicate OnReadDone call and status
        // std::cout << "Server [" << uuid_ << "]: OnReadDone called with ok = " << (ok ? "true" : "false") << std::endl;

        if (!ok) {                                // 收到 END_STREAM
            // std::cout << "Server [" << uuid_ << "]: Received END_STREAM from client." << std::endl;
            finish_ok();
            return;
        }

        if (!file_opened_) {                      // 第一次才建文件
            std::filesystem::path upload_dir = std::filesystem::current_path() / "uploads";

            try {
                std::filesystem::create_directories(upload_dir);
            } catch (const std::filesystem::filesystem_error& e) {
                finish_err("Failed to create upload directory");
                return;
            } catch (const std::exception& e) {
                finish_err("Failed to create upload directory (unknown)");
                return;
            }

            std::filesystem::path file_path = upload_dir / chunk_.filename();

            ofs_.open(file_path, std::ios::binary | std::ios::trunc);
            if (!ofs_) {
                std::cerr << "Server [" << uuid_ << "]: Failed to open file for writing: " << chunk_.filename() << std::endl;
                finish_err("open failed");
                return;
            }
            file_opened_ = true;
        }

        ofs_.write(chunk_.data().data(), chunk_.data().size());

        if (!ofs_) {
             std::cerr << "Server [" << uuid_ << "]: File write failed for chunk." << std::endl;
             finish_err("file write failed");
             return;
        }

        StartRead(&chunk_);
    }

    /* 结束回调 */
    void OnDone() override
    {
        if (ofs_.is_open()) {
            ofs_.close();
        }

        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0_).count();

        if (status_.ok()) {
            AccessLogger::log_commit(uuid_, ctx_, OperationType::UPLOAD, "upload committed", grpc::StatusCode::OK, ms);
        } else {
            AccessLogger::log_abort(uuid_, ctx_, OperationType::UPLOAD, status_.error_code(), status_.error_message(), ms);
        }

        delete this;
    }

private:
    void finish_ok()
    {
        resp_->set_message("upload complete");
        status_ = grpc::Status::OK;
        Finish(status_);
    }
    void finish_err(const std::string& msg)
    {
        status_ = grpc::Status(grpc::StatusCode::INTERNAL, msg);
        Finish(status_);
    }

    grpc::CallbackServerContext* ctx_;
    CCcloud::UploadResponse* resp_;
    CCcloud::UploadChunk chunk_;

    std::ofstream ofs_;
    bool file_opened_ = false;

    std::string uuid_;
    std::chrono::steady_clock::time_point t0_;
    grpc::Status status_;
};
