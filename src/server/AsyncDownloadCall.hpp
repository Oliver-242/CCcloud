#pragma once

#include <grpcpp/grpcpp.h>
#include <fstream>
#include <filesystem>
#include <memory>
#include <utility>

#include "generated/file.grpc.pb.h"
#include "CallBase.hpp"

class AsyncDownloadCall : public CallBase {
public:
    enum class State {
        REQUEST_QUEUED, // 构造函数中调用 RequestDownload。在此状态等待 RequestDownload 完成。
        READING_FILE,   // RequestDownload 完成。在此状态打开文件，发起第一个 Write。
        WRITING,        // 处理 Write 完成事件，发起下一个 Write 或 Finish。
        DONE            // RPC 结束 (Finish 完成 或 初始 Request 失败)。在此状态清理对象。
    };

    AsyncDownloadCall(CCcloud::FileService::AsyncService* service, grpc::ServerCompletionQueue* cq)
        : service_(service), cq_(cq), responder_(&ctx_), state_(State::REQUEST_QUEUED) { // 从 REQUEST_QUEUED 状态开始
        // 这个对象是为了处理 *一个* 潜在的传入 RPC 而创建的。
        // 发起异步调用，请求将 *这个特定的* RPC 与 *这个对象* 匹配。
        service_->RequestDownload(&ctx_, &request_, &responder_, cq_, cq_, this);
        // 当 RequestDownload 完成时，会向 CQ 投递一个 tag 为 this 的事件，
        // 状态为 REQUEST_QUEUED。
    }

    // 析构函数处理资源清理
    ~AsyncDownloadCall() {
        if (file_.is_open()) file_.close(); // 确保文件关闭
        // std::cout << "[Server] AsyncDownloadCall object " << this << " destroyed." << std::endl; // Optional debug log
    }

    void Proceed(bool ok) override {
        // DONE 状态特殊处理 - 意味着对象可以被删除了
        if (state_ == State::DONE) {
             // 这个 Proceed 调用是由最终的 Finish 操作完成 或 初始 RequestDownload 失败(!ok) 触发的。
             // 现在可以安全地删除对象了。
             // std::cout << "[Server] Deleting AsyncDownloadCall object " << this << std::endl; // Optional debug log
             delete this; // 清理这个对象。
             return;      // 删除后安全退出。
        }

        switch (state_) {
            case State::REQUEST_QUEUED: { // 这个状态由 service_->RequestDownload 完成时触发 (RPC 被接受 或 setup 失败)
                // ok 表明 RequestDownload 操作是否成功。
                if (!ok) {
                    // RequestDownload 失败。清理。
                    state_ = State::DONE; // 转移到清理状态
                    // std::cerr << "[Server] RequestDownload failed for object " << this << std::endl; // Optional debug log
                    return; // 退出当前的 Proceed 调用。
                }

                // ok 为 true: RequestDownload 成功。这个对象绑定到一个真实的传入 RPC。
                // !!! 这是关键部分 !!! : 请求 *下一个* 同类型 RPC。
                new AsyncDownloadCall(service_, cq_); // <--- 请求下一个传入 Download 调用！

                // 现在，处理当前这个 RPC (刚刚接受的)。
                state_ = State::READING_FILE; // 转移到打开文件和发送第一个 chunk 的状态

                // 打开文件
                std::string path = "uploads/" + request_.filename();
                if (!std::filesystem::exists(path)) {
                    // 文件不存在
                    HandleError(grpc::StatusCode::NOT_FOUND, "File not found");
                    return; // HandleError 发起 Finish 并设置状态为 DONE。
                }

                file_.open(path, std::ios::binary);
                if (!file_.is_open()) {
                    // 打开文件失败
                    HandleError(grpc::StatusCode::INTERNAL, "Failed to open file for reading");
                    return; // HandleError 发起 Finish 并设置状态为 DONE。
                }

                // 文件打开成功，开始发送第一个 chunk
                DoWrite(); // DoWrite 发起第一个 responder_.Write
                // DoWrite 会将状态设置为 WRITING
                break; // 退出 switch, 返回当前的 Proceed 调用。
            }
            case State::READING_FILE: { // 这个状态应该只在 REQUEST_QUEUED 之后触发一次
                 // 如果 Proceed 在 READING_FILE 状态被调用，意味着 DoWrite() 内部的 Write 操作完成了。
                 // 现在进入正常的 Write 完成处理流程。
                 state_ = State::WRITING; // 转移到 WRITING 状态，处理后续 Write 完成事件
                 // Fallthrough to WRITING case to process the completion of the first Write.
            }
            case State::WRITING: { // 这个状态处理 responder_.Write 完成事件
                // ok 表明上一个 Write 操作是否成功。
                if (!ok) {
                    // Write 失败 (客户端可能断开连接)。处理错误。
                    // std::cerr << "[Server] Write !ok for object " << this << ". Client disconnected?" << std::endl; // Optional debug log
                    HandleError(grpc::StatusCode::CANCELLED, "Client disconnected during download");
                    return; // HandleError 发起 Finish 并设置状态为 DONE。
                }

                // ok 为 true: 上一个 Write 成功。检查文件是否已读完。
                // 尝试读取下一个 chunk
                file_.read(buffer_, sizeof(buffer_));
                size_t size = file_.gcount(); // 实际读取的字节数

                if (size > 0) {
                    // 成功读取到数据，发送下一个 chunk。
                    chunk_.Clear(); // 清空上一个 chunk 的数据
                    chunk_.set_data(buffer_, size);
                    responder_.Write(chunk_, this); // 发起异步 Write，tag 仍然是 this，状态保持 WRITING
                    // 当 Write 完成时，Proceed 会以 state == WRITING 被再次调用。
                } else if (file_.eof()) {
                    // 读取返回 0 字节且文件结束。结束流式传输。
                    // std::cout << "[Server] File finished reading for object " << this << ". Calling Finish." << std::endl; // Optional debug log
                    responder_.Finish(grpc::Status::OK, this); // 发起异步 Finish
                    state_ = State::DONE; // 转移到等待 Finish 完成的状态。
                    // Finish 完成后，Proceed 会以 state == DONE 被再次调用。
                } else {
                    // 读取返回 0 字节但文件未结束。可能是读取错误。
                    // std::cerr << "[Server] File read returned 0 bytes but not eof for object " << this << std::endl; // Optional debug log
                    HandleError(grpc::StatusCode::INTERNAL, "File read error");
                    // HandleError 发起 Finish 并设置状态为 DONE。
                }
                break; // 退出 switch, 返回当前的 Proceed 调用。
            }
            // DONE 状态的 case 在函数开头处理了
        }
    }

private:
    // 助手函数，读取文件并发起 Write 操作 (仅用于第一次 Write)
    void DoWrite() {
        file_.read(buffer_, sizeof(buffer_));
        size_t size = file_.gcount(); // 实际读取的字节数

        if (size > 0) {
            chunk_.Clear(); // 清空上一个 chunk 的数据
            chunk_.set_data(buffer_, size);
            responder_.Write(chunk_, this); // 发起异步 Write，tag 仍然是 this
            // 状态将在 READING_FILE case 中设置为 WRITING
            // 当 Write 完成时，Proceed 会以 state == READING_FILE 或 WRITING 被再次调用。
        } else if (file_.eof()) {
            // 文件为空或只有文件名，直接结束。
             responder_.Finish(grpc::Status::OK, this); // 发起异步 Finish
             state_ = State::DONE; // 转移到等待 Finish 完成的状态。
             // Finish 完成后，Proceed 会以 state == DONE 被再次调用。
        } else {
            // 文件读取错误或为空且不是 eof
            HandleError(grpc::StatusCode::INTERNAL, "Initial file read error or empty file");
            // HandleError 发起 Finish 并设置状态为 DONE。
        }
    }

    // 助手函数，处理错误情况，发起 Finish
    void HandleError(grpc::StatusCode code, const std::string& msg) {
        // std::cerr << "[Server] Download Error for object " << this << ": " << msg << std::endl; // Optional debug log
        if (file_.is_open()) file_.close(); // 关闭文件
        // 发起异步的 Finish 操作，以错误状态结束 RPC。
        responder_.Finish(grpc::Status(code, msg), this);
        state_ = State::DONE; // 转移到等待 Finish 完成的状态。
        // Finish 完成后，Proceed 会以 state == DONE 被调用，该调用将删除对象。
    }


    CCcloud::FileService::AsyncService* service_;
    grpc::ServerCompletionQueue* cq_;
    grpc::ServerContext ctx_;
    grpc::ServerAsyncWriter<CCcloud::DownloadChunk> responder_; // ServerAsyncWriter for server streaming
    CCcloud::DownloadRequest request_; // Request message for Download
    CCcloud::DownloadChunk chunk_;     // Reusable chunk message for sending data

    std::ifstream file_; // File stream for reading
    char buffer_[409600];  // Buffer to read file chunks
    State state_;        // Current state of the RPC handler
};
