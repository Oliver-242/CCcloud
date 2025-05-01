#include <grpcpp/grpcpp.h>

#include "server/FileServiceImpl.hpp"

int main() {
    const std::string server_address = "0.0.0.0:50051";
    FileServiceImpl service;

    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    auto server = builder.BuildAndStart();
    std::cout << "Server listening on " << server_address << std::endl;
    server->Wait();
    return 0;
}
