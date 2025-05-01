#include "client/CCcloud_client.hpp"

int main(int argc, char** argv) {
    if (argc < 4) {
        std::cerr << "Usage:\n"
                  << "  upload <local_file> <remote_file>\n"
                  << "  download <remote_file> <local_file>\n"
                  << "  delete <remote_file>\n";
        return 1;
    }

    auto channel = grpc::CreateChannel("localhost:50051", grpc::InsecureChannelCredentials());
    CCCloudClient client(channel);

    std::string cmd = argv[1];
    if (cmd == "upload" && argc == 4) {
        client.UploadFile(argv[2], argv[3]);
    } else if (cmd == "download" && argc == 4) {
        client.DownloadFile(argv[2], argv[3]);
    } else if (cmd == "delete" && argc == 3) {
        client.DeleteFile(argv[2]);
    } else {
        std::cerr << "Invalid command or argument count.\n";
        return 1;
    }

    return 0;
}
