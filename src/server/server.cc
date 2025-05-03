#include <grpcpp/grpcpp.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/server_context.h>
#include <grpcpp/support/server_callback.h>

#include <iostream>
#include <memory>
#include <thread>
#include <chrono>

#include "generated/file.grpc.pb.h"
#include "AsyncUploadCall.hpp"
#include "logger/AccessLogger.hpp"
#include "logger/async_logger.hpp"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::CallbackServerContext;

// 服务实现类，使用 callback API
class CCcloudServiceImplCallback final : public CCcloud::FileService::ExperimentalCallbackService {
public:
    CCcloudServiceImplCallback() {
    }

    ~CCcloudServiceImplCallback() {
    }

    grpc::ServerReadReactor<CCcloud::UploadChunk>* Upload(
        CallbackServerContext* context,
        CCcloud::UploadResponse* response) override {
        return new AsyncUploadCall(context, response);
    }

    // 如果你还有 Download/Delete 等方法也在 callback 模式中实现，应当添加它们的 override
};

int main() {
    std::string server_address("0.0.0.0:9527");
    CCcloudServiceImplCallback service;

    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);  // 注册 callback service

    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "✅ Callback-based gRPC Server listening on " << server_address << std::endl;

    server->Wait();
    return 0;
}
