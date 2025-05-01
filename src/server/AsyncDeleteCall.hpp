#pragma once

#include <grpcpp/grpcpp.h>
#include <filesystem>

#include "generated/file.grpc.pb.h"
#include "CallBase.hpp"

class AsyncDeleteCall : public CallBase {
public:
    enum class State { CREATE, PROCESS, FINISH };

    AsyncDeleteCall(CCcloud::FileService::AsyncService* service, grpc::ServerCompletionQueue* cq)
        : service_(service), cq_(cq), responder_(&ctx_), state_(State::CREATE) {
        Proceed(true);
    }

    void Proceed(bool ok) override {
        if (!ok) {
            Finish(grpc::Status::CANCELLED);
            return;
        }

        if (state_ == State::CREATE) {
            state_ = State::PROCESS;
            service_->RequestDelete(&ctx_, &request_, &responder_, cq_, cq_, this);
        } else if (state_ == State::PROCESS) {
            new AsyncDeleteCall(service_, cq_);

            std::string path = "uploads/" + request_.filename();
            CCcloud::DeleteResponse response;
            grpc::Status status;

            if (std::filesystem::remove(path)) {
                response.set_message("Delete success");
                status = grpc::Status::OK;
            } else {
                response.set_message("Delete failed or not found");
                status = grpc::Status(grpc::StatusCode::NOT_FOUND, "Delete failed");
            }

            responder_.Finish(response, status, this);
            state_ = State::FINISH;
        } else {
            delete this;
        }
    }

private:
    void Finish(const grpc::Status& status, const std::string& message = "") {
        CCcloud::DeleteResponse response;
        response.set_message(message.empty() ? "Delete operation complete" : message);
        responder_.Finish(response, status, this);
        state_ = State::FINISH;
    }

    CCcloud::FileService::AsyncService* service_;
    grpc::ServerCompletionQueue* cq_;
    grpc::ServerContext ctx_;
    grpc::ServerAsyncResponseWriter<CCcloud::DeleteResponse> responder_;
    CCcloud::DeleteRequest request_;
    State state_;
};
