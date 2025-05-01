#pragma once

#include <grpcpp/grpcpp.h>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include "generated/file.grpc.pb.h"
#include "logger/AccessLogger.hpp"


class FileServiceImpl final : public CCcloud::FileService::Service {
    public:
        grpc::Status Upload(grpc::ServerContext* context,
                            grpc::ServerReader<CCcloud::UploadChunk>* reader,
                            CCcloud::UploadResponse* response) override {
            using namespace std::chrono;
    
            auto uuid = AccessLogger::generate_uuid();
            auto start = steady_clock::now();
    
            std::string filename = "[unknown]";
            std::ofstream ofs;
            bool opened = false;
            size_t total_bytes = 0;
    
            AccessLogger::log_prepare(uuid, context, OperationType::UPLOAD, "upload started");
    
            CCcloud::UploadChunk chunk;
            while (reader->Read(&chunk)) {
                if (!opened) {
                    filename = chunk.filename();
                    std::filesystem::create_directories("uploads/");
                    ofs.open("uploads/" + filename, std::ios::binary);
                    if (!ofs.is_open()) {
                        auto duration = duration_cast<milliseconds>(steady_clock::now() - start).count();
                        AccessLogger::log_abort(uuid, context, OperationType::UPLOAD,
                                                grpc::StatusCode::INTERNAL,
                                                "failed to open file",
                                                duration);
                        return grpc::Status(grpc::StatusCode::INTERNAL, "Failed to open file");
                    }
                    opened = true;
                }
                ofs.write(chunk.data().data(), chunk.data().size());
                total_bytes += chunk.data().size();
            }
    
            ofs.close();
            response->set_message("Upload successful");
    
            auto duration = duration_cast<milliseconds>(steady_clock::now() - start).count();
            AccessLogger::log_commit(uuid, context, OperationType::UPLOAD,
                                     "filename=" + filename + " size=" + std::to_string(total_bytes),
                                     grpc::StatusCode::OK, duration);
    
            return grpc::Status::OK;
        }
    
        grpc::Status Download(grpc::ServerContext* context,
                              const CCcloud::DownloadRequest* request,
                              grpc::ServerWriter<CCcloud::DownloadChunk>* writer) override {
            using namespace std::chrono;
    
            auto uuid = AccessLogger::generate_uuid();
            auto start = steady_clock::now();
    
            std::string filename = request->filename();
            AccessLogger::log_prepare(uuid, context, OperationType::DOWNLOAD,
                                      "filename=" + filename);
    
            std::ifstream ifs("uploads/" + filename, std::ios::binary);
            if (!ifs.is_open()) {
                auto duration = duration_cast<milliseconds>(steady_clock::now() - start).count();
                AccessLogger::log_abort(uuid, context, OperationType::DOWNLOAD,
                                        grpc::StatusCode::NOT_FOUND,
                                        "file not found",
                                        duration);
                return grpc::Status(grpc::StatusCode::NOT_FOUND, "File not found");
            }
    
            constexpr size_t BUF_SIZE = 4096;
            char buffer[BUF_SIZE];
            size_t total_bytes = 0;
    
            while (ifs.read(buffer, BUF_SIZE) || ifs.gcount()) {
                CCcloud::DownloadChunk chunk;
                chunk.set_data(buffer, ifs.gcount());
                writer->Write(chunk);
                total_bytes += ifs.gcount();
            }
    
            ifs.close();
            auto duration = duration_cast<milliseconds>(steady_clock::now() - start).count();
            AccessLogger::log_commit(uuid, context, OperationType::DOWNLOAD,
                                     "filename=" + filename + " size=" + std::to_string(total_bytes),
                                     grpc::StatusCode::OK, duration);
    
            return grpc::Status::OK;
        }
    
        grpc::Status Delete(grpc::ServerContext* context,
                            const CCcloud::DeleteRequest* request,
                            CCcloud::DeleteResponse* response) override {
            using namespace std::chrono;
    
            auto uuid = AccessLogger::generate_uuid();
            auto start = steady_clock::now();
    
            std::string filename = request->filename();
            AccessLogger::log_prepare(uuid, context, OperationType::DELETE,
                                      "filename=" + filename);
    
            std::string filepath = "uploads/" + filename;
            grpc::StatusCode status;
            std::string message;
    
            if (std::filesystem::remove(filepath)) {
                response->set_message("Deleted successfully");
                status = grpc::StatusCode::OK;
            } else {
                response->set_message("Delete failed or file not found");
                status = grpc::StatusCode::NOT_FOUND;
            }
    
            auto duration = duration_cast<milliseconds>(steady_clock::now() - start).count();
            AccessLogger::log_commit(uuid, context, OperationType::DELETE,
                                     "filename=" + filename,
                                     status, duration);
    
            return grpc::Status(status, response->message());
        }
    };