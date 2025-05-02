#pragma once

#include <grpcpp/grpcpp.h>
#include <filesystem> // For file system operations
#include <system_error> // Added for std::error_code

#include "generated/file.grpc.pb.h"
#include "CallBase.hpp"

class AsyncDeleteCall : public CallBase {
public:
    // 定义清晰的状态，基于异步操作的流程
    enum class State {
        REQUEST_QUEUED, // 构造函数中调用 RequestDelete。在此状态等待 RequestDelete 完成。
        PROCESSING,     // RequestDelete 完成。在此状态执行删除操作并发起 Finish。
        DONE            // RPC 结束 (Finish 完成 或 初始 Request 失败)。在此状态清理对象。
    };

    AsyncDeleteCall(CCcloud::FileService::AsyncService* service, grpc::ServerCompletionQueue* cq)
        : service_(service), cq_(cq), responder_(&ctx_), state_(State::REQUEST_QUEUED) { // 从 REQUEST_QUEUED 状态开始
        // 这个对象是为了处理 *一个* 潜在的传入 RPC 而创建的。
        // 发起异步调用，请求将 *这个特定的* RPC 与 *这个对象* 匹配。
        service_->RequestDelete(&ctx_, &request_, &responder_, cq_, cq_, this);
        // 当 RequestDelete 完成时，会向 CQ 投递一个 tag 为 this 的事件，
        // 状态为 REQUEST_QUEUED。
    }

    // 析构函数处理资源清理
    ~AsyncDeleteCall() {
        // std::cout << "[Server] AsyncDeleteCall object " << this << " destroyed." << std::endl; // Optional debug log
        // No file streams or other resources to explicitly close in destructor for Delete
    }

    void Proceed(bool ok) override {
        // DONE 状态特殊处理 - 意味着对象可以被删除了
        if (state_ == State::DONE) {
             // 这个 Proceed 调用是由最终的 Finish 操作完成 或 初始 RequestDelete 失败(!ok) 触发的。
             delete this; // 清理这个对象。
             return;      // 删除后安全退出。
        }

        switch (state_) {
            case State::REQUEST_QUEUED: { // 这个状态由 service_->RequestDelete 完成时触发 (RPC 被接受 或 setup 失败)
                // ok 表明 RequestDelete 操作是否成功。
                if (!ok) {
                    // RequestDelete 失败。清理。
                    state_ = State::DONE; // 转移到清理状态
                    // std::cerr << "[Server] RequestDelete failed for object " << this << std::endl; // Optional debug log
                    return; // 退出当前的 Proceed 调用。
                }

                // ok 为 true: RequestDelete 成功。这个对象绑定到一个真实的传入 RPC。
                // !!! 这是关键部分 !!! : 请求 *下一个* 同类型 RPC。
                new AsyncDeleteCall(service_, cq_); // <--- 请求下一个传入 Delete 调用！

                // 现在，处理当前这个 RPC (刚刚接受的)。
                state_ = State::PROCESSING; // 转移到执行删除操作的状态

                // 执行文件删除逻辑
                std::string filename = request_.filename();
                std::string path = "uploads/" + filename;
                CCcloud::DeleteResponse response; // 准备响应消息
                grpc::Status status;             // 准备最终状态

                // std::cout << "[Server] Attempting to delete file: " << path << " for object " << this << std::endl; // Optional debug log

                std::error_code ec; // 用于接收 remove 错误码
                bool removed = std::filesystem::remove(path, ec);

                if (removed) {
                    response.set_message("Delete success");
                    status = grpc::Status::OK;
                    // std::cout << "[Server] File deleted successfully: " << path << std::endl; // Optional debug log
                } else {
                    // 删除失败。根据错误码判断是文件不存在还是其他错误。
                    // std::cerr << "[Server] File deletion failed for path: " << path << ", error: " << ec.message() << std::endl; // Optional debug log
                    response.set_message("Delete failed");
                    if (ec == std::errc::no_such_file_or_directory) {
                        response.set_message("Delete failed: File not found");
                        status = grpc::Status(grpc::StatusCode::NOT_FOUND, "File not found");
                    } else {
                         // 其他删除错误 (权限, 文件正在使用等)
                        status = grpc::Status(grpc::StatusCode::INTERNAL, "Delete failed: " + ec.message());
                    }
                }

                // 发起异步的 Finish 操作，结束 RPC 并发送最终响应和状态。
                responder_.Finish(response, status, this);
                state_ = State::DONE; // 转移到等待 Finish 完成的状态。
                // Finish 完成后，Proceed 会以 state == DONE 被调用，该调用将删除对象。
                break; // 退出 switch, 返回当前的 Proceed 调用。
            }
            case State::PROCESSING: { // 这个状态处理 responder_.Finish 完成事件
                 // 在 Unary RPC 中，PROCESSING 状态执行删除和发起 Finish。
                 // Proceed(ok) 在 PROCESSING 状态被调用，意味着 responder_.Finish 完成了。
                 // 这里 ok 指示 Finish 操作是否成功。通常我们不太关心 Finish 本身是否 ok，
                 // 关键是它已经完成了，可以清理对象了。
                 // std::cout << "[Server] Responder.Finish completed for object " << this << ", ok=" << ok << std::endl; // Optional debug log
                 state_ = State::DONE; // 转移到清理状态。虽然已经在上面设置了，这里再次明确。
                 // 下一个 CQ 事件将触发开头的 DONE 检查和 delete this。
                 return; // 退出当前的 Proceed 调用。
            }
            // DONE 状态的 case 在函数开头处理了
        }
    }

private:
    CCcloud::FileService::AsyncService* service_;
    grpc::ServerCompletionQueue* cq_;
    grpc::ServerContext ctx_;
    grpc::ServerAsyncResponseWriter<CCcloud::DeleteResponse> responder_; // ServerAsyncResponseWriter for unary RPC
    CCcloud::DeleteRequest request_; // Request message for Delete
    State state_; // Current state of the RPC handler
};
