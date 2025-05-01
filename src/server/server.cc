#include <grpcpp/grpcpp.h>
#include <iostream>
#include <memory>
#include <thread>

#include "generated/file.grpc.pb.h"
#include "AsyncCall.hpp"

int main() {
    std::string server_address("0.0.0.0:9527");
    CCcloud::FileService::AsyncService service;

    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<grpc::ServerCompletionQueue> cq = builder.AddCompletionQueue();
    std::unique_ptr<grpc::Server> server = builder.BuildAndStart();

    std::cout << "✅ Async gRPC Server listening on " << server_address << std::endl;

    new AsyncUploadCall(&service, cq.get());
    // new AsyncDownloadCall(&service, cq.get());
    // new AsyncDeleteCall(&service, cq.get());

    const int kThreadCount = 24;
    std::vector<std::thread> workers;
    for (int i = 0; i < kThreadCount; ++i) {
        workers.emplace_back([&cq]() {
            void* tag;
            bool ok;
            while (cq->Next(&tag, &ok)) {
                static_cast<CallBase*>(tag)->Proceed(ok);
            }
        });
    }

    for (auto& t : workers) t.join();

    return 0;
}
