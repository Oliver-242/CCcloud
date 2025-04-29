#pragma once

#include <grpcpp/grpcpp.h>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include "generated/file.grpc.pb.h"

static std::string generate_uuid() {
    static thread_local boost::uuids::random_generator generator;
    return boost::uuids::to_string(generator());
}

class FileServiceImpl final : public CCcloud::FileService::Service {
public:
    grpc::Status Upload(grpc::ServerContext*,
                        grpc::ServerReader<CCcloud::UploadChunk>* reader,
                        CCcloud::UploadResponse* response) override {
        namespace fs = std::filesystem;

        CCcloud::UploadChunk chunk;
        std::ofstream ofs;
        std::string filename;

        // 确保 data 目录存在
        fs::create_directories("data");

        while (reader->Read(&chunk)) {
            if (!ofs.is_open()) {
                if (chunk.filename().empty()) {
                    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Filename required in first chunk");
                }
                filename = chunk.filename();
                ofs.open("data/" + filename, std::ios::binary);
                if (!ofs.is_open()) {
                    return grpc::Status(grpc::StatusCode::INTERNAL, "Failed to open output file");
                }
            }

            ofs.write(chunk.data().data(), chunk.data().size());
        }

        ofs.close();
        response->set_success(true);
        response->set_message("Upload successful");
        return grpc::Status::OK;
    }

    grpc::Status Download(grpc::ServerContext*,
                          const CCcloud::DownloadRequest* request,
                          grpc::ServerWriter<CCcloud::DownloadChunk>* writer) override {
        namespace fs = std::filesystem;

        std::string filename = request->filename();
        std::string full_path = "data/" + filename;

        if (!fs::exists(full_path)) {
            return grpc::Status(grpc::StatusCode::NOT_FOUND, "File not found");
        }

        std::ifstream ifs(full_path, std::ios::binary);
        if (!ifs.is_open()) {
            return grpc::Status(grpc::StatusCode::INTERNAL, "Failed to open file for reading");
        }

        const size_t buffer_size = 4096;
        std::vector<char> buffer(buffer_size);

        while (ifs.read(buffer.data(), buffer_size) || ifs.gcount() > 0) {
            CCcloud::DownloadChunk chunk;
            chunk.set_data(buffer.data(), ifs.gcount());
            if (!writer->Write(chunk)) {
                break;  // 客户端关闭连接
            }
        }

        ifs.close();
        return grpc::Status::OK;
    }

    grpc::Status Delete(grpc::ServerContext*,
                        const CCcloud::DeleteRequest* request,
                        CCcloud::DeleteResponse* response) override {
        namespace fs = std::filesystem;

        std::string filename = request->filename();
        std::string full_path = "data/" + filename;

        if (!fs::exists(full_path)) {
            response->set_success(false);
            response->set_message("File not found");
            return grpc::Status::OK;
        }

        std::error_code ec;
        bool removed = fs::remove(full_path, ec);
        if (!removed || ec) {
            response->set_success(false);
            response->set_message("Failed to delete file: " + ec.message());
            return grpc::Status::OK;
        }

        response->set_success(true);
        response->set_message("File deleted");
        return grpc::Status::OK;
    }
};
