#pragma once

#include <grpcpp/grpcpp.h>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "generated/file.grpc.pb.h"

class CCCloudClient {
public:
    explicit CCCloudClient(std::shared_ptr<grpc::Channel> channel)
        : stub_(CCcloud::FileService::NewStub(channel)) {}

    bool UploadFile(const std::string& local_path, const std::string& remote_filename) {
        grpc::ClientContext context;
        CCcloud::UploadResponse response;

        auto writer = stub_->Upload(&context, &response);
        std::ifstream ifs(local_path, std::ios::binary);
        if (!ifs) {
            std::cerr << "Failed to open local file: " << local_path << "\n";
            return false;
        }

        const size_t chunk_size = 409600;
        std::vector<char> buffer(chunk_size);
        bool first = true;

        while (ifs.read(buffer.data(), chunk_size) || ifs.gcount() > 0) {
            CCcloud::UploadChunk chunk;
            if (first) {
                chunk.set_filename(remote_filename);
                first = false;
            }
            chunk.set_data(buffer.data(), ifs.gcount());
            if (!writer->Write(chunk)) {
                std::cerr << "Upload stream broken.\n";
                return false;
            }
        }

        writer->WritesDone();
        auto status = writer->Finish();
        std::cout << "[Upload] " << response.message() << "\n";
        return status.ok();
    }

    bool DownloadFile(const std::string& remote_filename, const std::string& local_path) {
        grpc::ClientContext context;
        CCcloud::DownloadRequest request;
        request.set_filename(remote_filename);

        auto reader = stub_->Download(&context, request);
        std::ofstream ofs(local_path, std::ios::binary);
        if (!ofs) {
            std::cerr << "Failed to create local file: " << local_path << "\n";
            return false;
        }

        CCcloud::DownloadChunk chunk;
        while (reader->Read(&chunk)) {
            ofs.write(chunk.data().data(), chunk.data().size());
        }

        auto status = reader->Finish();
        std::cout << "[Download] status: " << (status.ok() ? "OK" : "FAILED") << "\n";
        return status.ok();
    }

    bool DeleteFile(const std::string& remote_filename) {
        grpc::ClientContext context;
        CCcloud::DeleteRequest req;
        CCcloud::DeleteResponse resp;
        req.set_filename(remote_filename);
        auto status = stub_->Delete(&context, req, &resp);
        std::cout << "[Delete] " << resp.message() << "\n";
        return status.ok() && resp.success();
    }

private:
    std::unique_ptr<CCcloud::FileService::Stub> stub_;
};
