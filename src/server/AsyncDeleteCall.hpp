#pragma once

#include <grpcpp/grpcpp.h>
#include <grpcpp/support/server_callback.h>
#include <filesystem>
#include <system_error>
#include <chrono>

#include "generated/file.grpc.pb.h"
#include "logger/AccessLogger.hpp"

class AsyncDeleteCall : public grpc::ServerUnaryReactor {
public:
    AsyncDeleteCall(grpc::CallbackServerContext* ctx,
                    const CCcloud::DeleteRequest* req,
                    CCcloud::DeleteResponse* resp)
        : ctx_(ctx), req_(req), resp_(resp)
    {
        uuid_ = AccessLogger::generate_uuid();
        AccessLogger::log_prepare(uuid_, ctx_, OperationType::DELETE, "delete started");
        t0_ = std::chrono::steady_clock::now();
        perform_delete();
    }

    ~AsyncDeleteCall() override
    {
    }

    void OnDone() override
    {
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0_).count();

        if (status_.ok()) {
            AccessLogger::log_commit(uuid_, ctx_, OperationType::DELETE, "download completed", grpc::StatusCode::OK, ms);
        } else {
            AccessLogger::log_abort(uuid_, ctx_, OperationType::DELETE, status_.error_code(), status_.error_message(), ms);
        }

        delete this;
    }

    void OnCancel() override
    {
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0_).count();
        AccessLogger::log_abort(uuid_, ctx_, OperationType::DELETE, status_.error_code(), status_.error_message(), ms);
        delete this;
    }

private:
    void perform_delete() {
        std::string filename_to_delete = req_->filename();
        std::filesystem::path file_path("uploads/" + filename_to_delete);

        std::error_code ec;
        if (std::filesystem::remove(file_path, ec)) {
            finish_ok();
        } else {
            finish_err(format_msg(uuid_, "Failed to delete file: " + filename_to_delete + " - " + ec.message()));
        }
    }

    void finish_ok()
    {
        resp_->set_success(true);
        resp_->set_message("delete complete");
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
    const CCcloud::DeleteRequest* req_;
    CCcloud::DeleteResponse* resp_;

    std::string uuid_;
    std::chrono::steady_clock::time_point t0_;
    grpc::Status status_;
};
