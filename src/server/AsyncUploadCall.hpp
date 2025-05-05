#pragma once

#include <grpcpp/grpcpp.h>
#include <grpcpp/support/server_callback.h>
#include <filesystem>
#include <fstream>
#include <chrono>

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

    ~AsyncUploadCall() override
    {
        if (ofs_.is_open()) {
            ofs_.close();
        }
    }

    void OnReadDone(bool ok) override
    {
        if (!ok) {
            finish_ok();
            return;
        }

        if (!file_opened_) { // 第一次才建文件
            std::filesystem::path upload_dir = std::filesystem::current_path() / "uploads";

            try {
                std::filesystem::create_directories(upload_dir);
            } catch (const std::filesystem::filesystem_error& e) {
                finish_err(format_msg(uuid_, "Failed to create upload directory"));
                return;
            } catch (const std::exception& e) {
                finish_err(format_msg(uuid_, "Failed to create upload directory (unknown)"));
                return;
            }

            std::filesystem::path file_path = upload_dir / chunk_.filename();

            ofs_.open(file_path, std::ios::binary | std::ios::trunc);
            if (!ofs_) {
                finish_err(format_msg(uuid_, "Failed to open file for writing: " + chunk_.filename()));
                return;
            }
            file_opened_ = true;
        }

        ofs_.write(chunk_.data().data(), chunk_.data().size());

        if (!ofs_) {
            finish_err(format_msg(uuid_, chunk_.filename() + " writes failed for chunk."));
            return;
        }

        StartRead(&chunk_);
    }

    void OnDone() override
    {
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
        resp_->set_success(true);
        resp_->set_message("upload complete");
        status_ = grpc::Status::OK;
        Finish(status_);
    }

    void finish_err(const std::string& msg)
    {
        resp_->set_success(false);
        resp_->set_message(msg);
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
    CCcloud::UploadResponse* resp_;
    CCcloud::UploadChunk chunk_;

    std::ofstream ofs_;
    bool file_opened_ = false;

    std::string uuid_;
    std::chrono::steady_clock::time_point t0_;
    grpc::Status status_;
};
