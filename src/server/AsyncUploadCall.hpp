#pragma once

#include <grpcpp/grpcpp.h>
#include <fstream>
#include <filesystem>
#include <memory>
#include <iostream> // Added for potential logging/debugging
#include <utility> // Added for std::move

#include "generated/file.grpc.pb.h"
#include "CallBase.hpp"

class AsyncUploadCall : public CallBase {
public:
    // 定义清晰的状态，基于异步操作的流程
    enum class State {
        REQUEST_QUEUED, // 构造函数中调用 RequestUpload。在此状态等待 RequestUpload 完成。
        READING,        // RequestUpload 完成。在此状态处理 Read 完成事件，发起下一个 Read 或 Finish。
        DONE            // RPC 结束 (Finish 完成 或 初始 Request 失败)。在此状态清理对象。
    };

    AsyncUploadCall(CCcloud::FileService::AsyncService* service, grpc::ServerCompletionQueue* cq)
        : service_(service), cq_(cq), state_(State::REQUEST_QUEUED) { // 从 REQUEST_QUEUED 状态开始
        // 这个对象是为了处理 *一个* 潜在的传入 RPC 而创建的。
        // 发起异步调用，请求将 *这个特定的* RPC 与 *这个对象* 匹配。
        reader_ = std::make_unique<grpc::ServerAsyncReader<CCcloud::UploadResponse, CCcloud::UploadChunk>>(&ctx_);
        // 调用 RequestUpload。当 RequestUpload 完成时，会向 CQ 投递一个 tag 为 this 的事件，
        // 状态为 REQUEST_QUEUED。
        service_->RequestUpload(&ctx_, reader_.get(), cq_, cq_, this);
        // Proceed 函数将在 CQ 轮询到这个事件时被调用。
    }

    // 析构函数处理资源清理
    ~AsyncUploadCall() {
        if (ofs_.is_open()) ofs_.close(); // 确保文件关闭
        // reader_ 和 context_ 的 unique_ptr 会自动释放内存
        // final_response_ 和 final_status_ 是轻量级成员
        // std::cout << "[Server] AsyncUploadCall object " << this << " destroyed." << std::endl; // Optional debug log
    }

    void Proceed(bool ok) override {
        // DONE 状态特殊处理 - 意味着对象可以被删除了
        if (state_ == State::DONE) {
             // 这个 Proceed 调用是由最终的 Finish 操作完成 或 初始 RequestUpload 失败(!ok) 触发的。
             // 现在可以安全地删除对象了。
             // std::cout << "[Server] Deleting AsyncUploadCall object " << this << std::endl; // Optional debug log
             delete this; // 清理这个对象。
             return;      // 删除后安全退出。
        }

        switch (state_) {
            case State::REQUEST_QUEUED: { // 这个状态由 service_->RequestUpload 完成时触发 (RPC 被接受 或 setup 失败)
                // 这里的 ok 表明 RequestUpload 操作是否成功 (即客户端发起的调用是否被匹配上)。
                if (!ok) {
                    // RequestUpload 操作本身失败 (例如 CQ 被关闭, 服务器正在关机, 客户端在 setup 阶段取消)。
                    // 这个对象无法处理一个有效的 RPC，需要清理。
                    state_ = State::DONE; // 转移到清理状态
                    // 注意：这里不能直接 delete this。当前正在处理 CQ 事件的 Proceed 调用需要先完成。
                    // CQ 会在后续某个时间点再次为这个 tag (this) 调用 Proceed，那时状态是 DONE。
                    // std::cerr << "[Server] RequestUpload failed for object " << this << std::endl; // Optional debug log
                    return; // 退出当前的 Proceed 调用。
                }

                // ok 为 true: RequestUpload 成功。这个对象现在绑定到一个真实的传入 RPC。
                // !!! 这是关键部分 !!! : 通知服务器准备好接受 *下一个* 同类型 RPC。
                // 创建一个新的 AsyncUploadCall 对象。这个新对象的构造函数会调用 RequestUpload
                // 请求下一个 Upload 调用。
                // std::cout << "[Server] Accepted new Upload RPC, object " << this << ". Requesting next." << std::endl; // Optional debug log
                new AsyncUploadCall(service_, cq_); // <--- 请求下一个传入调用！

                // 现在，处理当前这个 RPC (刚刚接受的)。
                state_ = State::READING; // 转移到处理读操作的状态
                // 发起当前 RPC 的第一个 Read 操作。
                reader_->Read(&chunk_, this);
                // 当第一个 Read 完成时，Proceed 会以 state == READING 被再次调用。
                break; // 退出 switch, 返回当前的 Proceed 调用。
            }
            case State::READING: { // 这个状态处理 Read 操作的完成事件
                if (!ok) {
                    // Read 操作完成，但 !ok。意味着客户端流已关闭 或 读取出错。
                    // std::cout << "[Server] Read !ok for object " << this << ". Client closed stream?" << std::endl; // Optional debug log
                    // 处理流结束 / 错误。关闭文件。
                    if (ofs_.is_open()) ofs_.close(); // 确保文件关闭

                    // 准备最终的状态和响应 (使用成员变量)
                    // 客户端正常关闭流通常认为是成功完成 (如果之前的数据都收到了)
                    final_status_ = grpc::Status::OK;
                    final_response_.set_message("Upload complete");

                    // 发起异步的 Finish 操作。这将结束服务器端的 RPC，并发送响应头/状态/消息。
                    reader_->Finish(final_response_, final_status_, this);
                    state_ = State::DONE; // 转移到等待 Finish 完成的状态。
                    // Finish 完成后，Proceed 会以 state == DONE 被再次调用。
                    return; // 退出当前的 Proceed 调用。
                }

                // ok 为 true: Read 成功。处理接收到的 chunk。
                if (!filename_received_) {
                     filename_ = chunk_.filename();
                     if (filename_.empty()) {
                        // 第一个 chunk 文件名为空。视为无效参数错误。
                        // std::cerr << "[Server] Filename empty in first chunk for object " << this << std::endl; // Optional debug log
                        HandleReadError(grpc::StatusCode::INVALID_ARGUMENT, "Filename required in first chunk");
                        return; // HandleReadError 发起 Finish 并设置状态为 DONE。
                     }
                     // 只在第一个 chunk 时创建目录和打开文件
                     std::filesystem::create_directories("uploads/"); // 确保目录存在
                     ofs_.open("uploads/" + filename_, std::ios::binary); // 打开文件写入
                     if (!ofs_.is_open()) {
                        // 打开文件失败。视为内部错误。
                        // std::cerr << "[Server] Failed to open file '" << filename_ << "' for object " << this << std::endl; // Optional debug log
                        HandleReadError(grpc::StatusCode::INTERNAL, "Failed to open file for writing");
                        return; // HandleReadError 发起 Finish 并设置状态为 DONE。
                     }
                     filename_received_ = true; // 标记文件名已处理
                }
                // 写入数据 chunk (适用于第一个 chunk 和后续非空 chunk)
                if (!chunk_.data().empty()) {
                     ofs_.write(chunk_.data().data(), chunk_.data().size());
                     // TODO: 检查写入错误 (ofs_.good() 或检查底层 IO 函数返回值)。
                     // 如果写入失败，调用 HandleReadError 并使用 grpc::StatusCode::INTERNAL。
                     // std::cout << "[Server] Wrote " << chunk_.data().size() << " bytes for object " << this << std::endl; // Optional debug log
                }

                // 发起下一个 Read 操作。保持状态为 READING。
                // 这个 Read 完成后，Proceed 会以 state == READING 被再次调用。
                reader_->Read(&chunk_, this);
                break; // 退出 switch, 返回当前的 Proceed 调用。
            }
            // DONE 状态的 case 被函数开头的检查处理了
        }
    }

private:
    // 助手函数，处理在 READING 状态中检测到的错误 (如数据问题, 文件系统错误)
    void HandleReadError(grpc::StatusCode code, const std::string& msg) {
         // std::cerr << "[Server] Handling Read Error for object " << this << ": " << msg << std::endl; // Optional debug log
         if (ofs_.is_open()) ofs_.close(); // 关闭文件 (如果打开了)
         final_status_ = grpc::Status(code, msg); // 设置错误状态
         final_response_.set_message("Upload failed: " + msg); // 设置错误消息

         // 发起异步的 Finish 操作，以错误状态结束 RPC。
         reader_->Finish(final_response_, final_status_, this);
         state_ = State::DONE; // 转移到等待 Finish 完成的状态。
         // Finish 完成后，Proceed 会以 state == DONE 被调用，该调用将删除对象。
    }

    CCcloud::FileService::AsyncService* service_;
    grpc::ServerCompletionQueue* cq_;
    grpc::ServerContext ctx_;
    std::unique_ptr<grpc::ServerAsyncReader<CCcloud::UploadResponse, CCcloud::UploadChunk>> reader_;
    CCcloud::UploadChunk chunk_; // 可重用的 chunk 消息缓冲区

    std::ofstream ofs_; // 用于写入文件的文件流
    std::string filename_; // 存储第一个 chunk 中的文件名
    bool filename_received_ = false; // 标记文件名是否已处理
    State state_; // 当前 RPC 处理对象的状态

    // 成员变量，用于在发起 Finish 操作前存储最终的响应和状态
    CCcloud::UploadResponse final_response_;
    grpc::Status final_status_;
};