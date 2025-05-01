#pragma once

#include <grpcpp/grpcpp.h>
#include <fstream>

#include "generated/file.grpc.pb.h"
#include "CallBase.hpp"

class AsyncDownloadCall : public CallBase {
public:
    enum class State { CREATE, WRITE, WRITING, FINISH };

    AsyncDownloadCall(CCcloud::FileService::AsyncService* service, grpc::ServerCompletionQueue* cq)
        : service_(service), cq_(cq), responder_(&ctx_), state_(State::CREATE) {
        Proceed(true);
    }

    void Proceed(bool ok) override {
        if (!ok) {
            Finish(grpc::Status::CANCELLED);
            return;
        }

        if (state_ == State::CREATE) {
            state_ = State::WRITE;
            service_->RequestDownload(&ctx_, &request_, &responder_, cq_, cq_, this);
        } else if (state_ == State::WRITE) {
            new AsyncDownloadCall(service_, cq_);

            file_.open("uploads/" + request_.filename(), std::ios::binary);
            if (!file_.is_open()) {
                Finish(grpc::Status(grpc::StatusCode::NOT_FOUND, "File not found"));
                return;
            }

            DoWrite();
        } else if (state_ == State::WRITING) {
            if (file_.eof()) {
                Finish(grpc::Status::OK);
                return;
            }
            DoWrite();
        }
    }

private:
    void DoWrite() {
        file_.read(buffer_, sizeof(buffer_));
        size_t size = file_.gcount();
        if (size == 0) {
            Finish(grpc::Status::OK);
            return;
        }

        chunk_.Clear();
        chunk_.set_data(buffer_, size);
        responder_.Write(chunk_, this);
        state_ = State::WRITING;
    }

    void Finish(const grpc::Status& status) {
        if (file_.is_open()) file_.close();
        responder_.Finish(status, this);
        delete this;
    }

    CCcloud::FileService::AsyncService* service_;
    grpc::ServerCompletionQueue* cq_;
    grpc::ServerContext ctx_;
    grpc::ServerAsyncWriter<CCcloud::DownloadChunk> responder_;
    CCcloud::DownloadRequest request_;
    CCcloud::DownloadChunk chunk_;
    std::ifstream file_;
    char buffer_[4096];
    State state_ = State::CREATE;
};
