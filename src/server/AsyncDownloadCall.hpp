#pragma once

#include <grpcpp/grpcpp.h>
#include <grpcpp/support/server_callback.h>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <chrono>

#include "generated/file.grpc.pb.h"
#include "logger/AccessLogger.hpp"


class AsyncDownloadCall : public grpc::ServerWriteReactor<CCcloud::DownloadChunk> {
public:
    AsyncDownloadCall(grpc::CallbackServerContext* ctx,
                      const CCcloud::DownloadRequest* request)
        : ctx_(ctx), req_(request)
    {
        uuid_ = AccessLogger::generate_uuid();
        AccessLogger::log_prepare(uuid_, ctx_, OperationType::DOWNLOAD, "download started");
        StartWrite(&dchunk_);
        t0_ = std::chrono::steady_clock::now();
    }

    ~AsyncDownloadCall() override
    {
        if (ifs_.is_open()) {
            ifs_.close();
        }
    }

    void OnWriteDone(bool ok) override
    {
        if (!ok) {
            finish_ok();
            return;
        }

        if (!ifs_.is_open()) {
            std::filesystem::path file_path("uploads/" + req_->filename());
            ifs_.open(file_path, std::ios::binary);

            if (!ifs_.is_open()) {
                finish_err(format_msg(uuid_, "Failed to open file " + req_->filename()));
                return;
            }
        }
    
        ifs_.read(buffer_, sizeof(buffer_));
        std::streamsize bytes_read = ifs_.gcount();
    
        if (bytes_read > 0) {
            dchunk_.set_data(buffer_, bytes_read);
            StartWrite(&dchunk_);
        } else {
            if (ifs_.eof()) {
                finish_ok();
            } else if (ifs_.fail() || ifs_.bad()) {
                finish_err(format_msg(uuid_, "Error reading from file " + req_->filename()));
            } else {
                 finish_err(format_msg(uuid_, "Unexpected state after reading 0 bytes from file " + req_->filename()));
            }
            ifs_.close();
        }
    }

    void OnDone() override
    {
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0_).count();

        if (status_.ok()) {
            AccessLogger::log_commit(uuid_, ctx_, OperationType::DOWNLOAD, "download completed", grpc::StatusCode::OK, ms);
        } else {
            AccessLogger::log_abort(uuid_, ctx_, OperationType::DOWNLOAD, status_.error_code(), status_.error_message(), ms);
        }

        delete this;
    }

private:
    void finish_ok()
    {
        status_ = grpc::Status::OK;
        Finish(status_);
    }

    void finish_err(const std::string& msg)
    {
        status_ = grpc::Status(grpc::StatusCode::INTERNAL, msg);
        Finish(status_);
    }

    template <typename ... _Args>
    std::string format_msg(std::string uuid, _Args&&... args)
    {
        std::ostringstream oss;

        oss << "Server [" << uuid << "]";

        if constexpr (sizeof...(_Args) > 0) {
            ((oss << " " << std::forward<_Args>(args)), ...);
        }

        return oss.str();
    }

private:
    grpc::CallbackServerContext* ctx_;
    const CCcloud::DownloadRequest* req_;
    CCcloud::DownloadChunk dchunk_;

    std::ifstream ifs_;

    std::string uuid_;
    std::chrono::steady_clock::time_point t0_;
    grpc::Status status_;
    char buffer_[409600]; 
};
