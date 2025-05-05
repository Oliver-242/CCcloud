#pragma once

#include <grpcpp/grpcpp.h>
#include <string>
#include <fstream>
#include <iostream>
#include <memory>
#include <filesystem>
#include <vector> 
#include <functional> // 用于可能的 completion callback

#include "generated/file.grpc.pb.h" // 假设这是你的 Protobuf 生成的头文件


class UploadReactor;
class DownloadReactor;
class DeleteReactor;

// 一个简单的客户端封装类，使用 Reactor API
class CloudClientApi {
public:
    CloudClientApi(const std::string& server_address) {
        // 注意：这里使用了 InsecureChannelCredentials，生产环境请使用 SSL/TLS
        channel_ = grpc::CreateChannel(server_address, grpc::InsecureChannelCredentials());
        async_stub_ = CCcloud::FileService::NewAsyncStub(channel_);
    }

    ~CloudClientApi() {
    }

    // filePath 本地待上传的文件路径
    // remoteFileName: 文件在服务器上保存的名称
    // callback: 可选的完成后回调函数 (Status, Response) -> void
    void UploadFile(const std::string& filePath, const std::string& remoteFileName) {
        std::ifstream* file_stream = new std::ifstream(filePath, std::ios::binary);

        if (!file_stream->is_open()) {
            std::cerr << "Error: Could not open file for uploading: " << filePath << std::endl;
            delete file_stream;
            return;
        }

        new UploadReactor(async_stub_.get(), filePath, remoteFileName, file_stream);
    }

    // 下载文件
    // remoteFileName: 服务器上的文件名称
    // localPath: 保存到本地的文件路径
    // callback: 可选的完成后回调函数 (Status, Response) -> void (对于下载，response 可能是空的或只包含状态信息)
    void DownloadFile(const std::string& remoteFileName, const std::string& localPath) {
         std::ofstream* file_stream = new std::ofstream(localPath, std::ios::binary | std::ios::trunc);
         if (!file_stream->is_open()) {
             std::cerr << "Error: Could not open file for writing: " << localPath << std::endl;
             delete file_stream;
             return;
         }

         new DownloadReactor(async_stub_.get(), remoteFileName, localPath, file_stream);
    }

    // 删除文件
    // remoteFileName: 服务器上待删除的文件名称
    // callback: 可选的完成后回调函数 (Status, Response) -> void
    void DeleteFile(const std::string& remoteFileName) {
        new DeleteReactor(async_stub_.get(), remoteFileName);
    }


private:
    std::shared_ptr<grpc::Channel> channel_;
    std::unique_ptr<CCcloud::FileService::AsyncStub> async_stub_;
};


class UploadReactor final : public grpc::ClientWriteReactor<CCcloud::UploadChunk> {
public:
    UploadReactor(CCcloud::FileService::AsyncStub* async_stub,
                  const std::string& filePath,
                  const std::string& remoteFileName,
                  std::ifstream* file_stream)
        : file_stream_(file_stream), remote_file_name_(remoteFileName), is_first_chunk_(true) {
        async_stub->PrepareAsyncUpload(&context_, &response_, this);
        StartCall();
        StartWrite(&next_chunk_);
    }

    ~UploadReactor() override {
        if (file_stream_) {
            file_stream_->close();
            delete file_stream_;
            file_stream_ = nullptr;
        }
    }

    void OnWriteDone(bool ok) override {
        if (ok) {
            ReadNextChunkFromFileAndStartWrite();
        }
    }

    void OnDone(const grpc::Status& s) override {
        if (s.ok() && response_.success()) {
            std::cout << "Upload '" << remote_file_name_ << "' successful: " << response_.message() << std::endl;
        } else {
            std::cerr << "Upload '" << remote_file_name_ << "' failed: " << s.error_message();
            if (!s.ok()) {
                 std::cerr << " (gRPC code: " << s.error_code() << ")";
            }
             if (!response_.success()) {
                 std::cerr << " (Server message: " << response_.message() << ")";
             }
            std::cerr << std::endl;
        }

        delete this;
    }

private:
    void ReadNextChunkFromFileAndStartWrite() {
        next_chunk_.Clear();

        file_stream_->read(buffer_, sizeof(buffer_));
        size_t bytes_read = file_stream_->gcount();

        if (bytes_read > 0) {
            if (is_first_chunk_) {
                next_chunk_.set_filename(remote_file_name_);
                is_first_chunk_ = false;
            }
            next_chunk_.set_data(buffer_, bytes_read);
            StartWrite(&next_chunk_);
        } else {
            WritesDone();
        }
        if (file_stream_->bad()) {
            std::cerr << "UploadReactor: File stream error during read." << std::endl;
            Finish(grpc::Status(grpc::StatusCode::INTERNAL, "Local file read error."));
        }
    }


    grpc::ClientContext context_;
    your_service::UploadResponse response_;
    std::ifstream* file_stream_;
    std::string remote_file_name_;
    char buffer_[409600];
    CCcloud::UploadChunk next_chunk_;
    bool is_first_chunk_;
};


class DownloadReactor final : public grpc::ClientReadReactor<your_service::DownloadChunk> {
public:
    DownloadReactor(CCcloud::FileService::AsyncStub* async_stub,
                    const std::string& remoteFileName,
                    const std::string& localPath,
                    std::ofstream* file_stream)
        : local_path_(localPath), file_stream_(file_stream) {
        request_.set_filename(remoteFileName);

        async_stub->PrepareAsyncDownload(&context_, &request_, this);
        StartCall();
        StartRead(&next_chunk_);
    }

    ~DownloadReactor() override {
        if (file_stream_) {
            if (file_stream_->is_open()) {
                 file_stream_->close();
            }
            delete file_stream_;
            file_stream_ = nullptr;
        }
    }

    void OnReadDone(bool ok) override {
        if (ok) {
            file_stream_->write(next_chunk_.data().data(), next_chunk_.data().size());

            if (!(*file_stream_)) {
                std::cerr << "DownloadReactor: Failed to write chunk to local file." << std::endl;
                Finish(grpc::Status(grpc::StatusCode::INTERNAL, "Local file write error."));
                return;
            }

            // 成功写入文件，启动下一次读取服务器发送的数据块
            StartRead(&next_chunk_);
        } else {
            std::cout << "Server finished writing stream." << std::endl;
        }
    }

    void OnDone(const grpc::Status& s) override {
        bool file_write_success = file_stream_ && file_stream_->good();

        if (s.ok() && file_write_success) {
            std::cout << "Download successful: " << request_.filename() << " -> " << local_path_ << std::endl;
        } else {
            std::cerr << "Download failed: " << s.error_message();
            if (!s.ok()) {
                 std::cerr << " (gRPC code: " << s.error_code() << ")";
            }
             if (!file_write_success) {
                 std::cerr << " (Local file write error after receiving chunks)";
             }
            std::cerr << std::endl;

            if (std::filesystem::exists(local_path_)) {
                std::error_code ec;
                std::filesystem::remove(local_path_, ec);
                if (ec) {
                     std::cerr << "Warning: Failed to remove incomplete download file: " << local_path_ << " - " << ec.message() << std::endl;
                } else {
                     std::cerr << "Cleaned up incomplete download file: " << local_path_ << std::endl;
                }
            }
        }

        delete this;
    }

private:
    grpc::ClientContext context_;
    your_service::DownloadRequest request_;
    your_service::DownloadChunk next_chunk_;
    std::string local_path_;
    std::ofstream* file_stream_;
};


class DeleteReactor final : public grpc::ClientUnaryReactor {
public:
    DeleteReactor(CCcloud::FileService::AsyncStub* async_stub,
                  const std::string& remoteFileName) {
        request_.set_filename(remoteFileName);

        async_stub->PrepareAsyncDelete(&context_, &request_, &response_, this);
        StartCall();
    }

    ~DeleteReactor() override {
    }

    void OnDone(const grpc::Status& s) override {
        if (s.ok() && response_.success()) {
            std::cout << "Delete successful: " << request_.filename() << ". Message: " << response_.message() << std::endl;
        } else {
            std::cerr << "Delete failed: " << s.error_message();
            if (!s.ok()) {
                 std::cerr << " (gRPC code: " << s.error_code() << ")";
            }
             if (!response_.success()) {
                 std::cerr << " (Server message: " << response_.message() << ")";
             }
            std::cerr << std::endl;
        }

        delete this;
    }

private:
    grpc::ClientContext context_;
    your_service::DeleteRequest request_;
    your_service::DeleteResponse response_;
};