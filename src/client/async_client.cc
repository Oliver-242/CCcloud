#include <grpcpp/grpcpp.h>
#include <grpcpp/client_context.h>
#include <grpcpp/support/async_stream.h>
#include <iostream>
#include <thread>
#include <vector>
#include <memory>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <string>
#include <stdexcept>
#include <utility> // For std::move, std::min
#include <numeric> // For std::iota

#include "generated/file.grpc.pb.h"

using namespace std;
using grpc::Channel;
using grpc::ClientAsyncWriter;
using grpc::ClientContext;
using grpc::CompletionQueue;
using grpc::Status;
using grpc::ClientAsyncReader; // For server streaming client side (Download)
using grpc::ClientAsyncResponseReader; // For unary client side (Delete)


static const int concurrency_default = 128;
static const int chunks_default = 100;
static const long long chunk_size_default = 40960;

int concurrency = concurrency_default;
int chunks = chunks_default;
long long chunk_size = chunk_size_default;


struct TaskResource {
    int id; // 任务ID
    shared_ptr<Channel> channel;
    unique_ptr<CompletionQueue> cq; // 每个任务线程有自己的 CQ
    shared_ptr<TaskResource> self_ptr; // 用于作为 CQ 事件的 tag，指向自身
};


template <typename T>
T parseSingle(const std::string& value, const std::string& argName) {
    try {
        size_t pos = 0;
        T result;
        if constexpr (std::is_same_v<T, int>) {
            result = std::stoi(value, &pos);
        } else if constexpr (std::is_same_v<T, unsigned int>) {
            result = std::stoul(value, &pos);
        } else if constexpr (std::is_same_v<T, long long>) {
            result = std::stoll(value, &pos);
        } else if constexpr (std::is_same_v<T, unsigned long long>) {
            result = std::stoull(value, &pos);
        } else {
            throw std::runtime_error("Unsupported type for parsing.");
        }
        if (pos != value.length()) {
            throw std::invalid_argument("Invalid characters after numeric value");
        }
        return result;
    } catch (const std::invalid_argument& e) {
        throw std::invalid_argument("Invalid value for argument '" + argName + "': " + e.what());
    } catch (const std::out_of_range& e) {
        throw std::out_of_range("Value for argument '" + argName + "' is out of range: " + e.what());
    }
}

bool parseArgs(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg.rfind("--concurrency=", 0) == 0) {
            try {
                concurrency = parseSingle<int>(arg.substr(std::string("--concurrency=").length()), "--concurrency");
                if (concurrency <= 0) {
                    throw std::invalid_argument("Concurrency must be a positive integer.");
                }
            } catch (const std::exception& e) {
                std::cerr << "Error parsing argument '--concurrency': " << e.what() << std::endl;
                return false;
            }
        }
        else if (arg.rfind("--chunks=", 0) == 0) {
            try {
                chunks = parseSingle<int>(arg.substr(std::string("--chunks=").length()), "--chunks");
                if (chunks <= 0) {
                    throw std::invalid_argument("Chunks must be a positive integer.");
                }
            } catch (const std::exception& e) {
                std::cerr << "Error parsing argument '--chunks': " << e.what() << std::endl;
                return false;
            }
        }
        else if (arg.rfind("--chunk_size=", 0) == 0) {
            try {
                chunk_size = parseSingle<long long>(arg.substr(std::string("--chunk_size=").length()), "--chunk_size");
                if (chunk_size <= 0) {
                    throw std::invalid_argument("Chunk size must be a positive integer.");
                }
            } catch (const std::exception& e) {
                std::cerr << "Error parsing argument '--chunk_size': " << e.what() << std::endl;
                return false;
            }
        }
        else {
            std::cerr << "Error: Unknown command line argument '" << arg << "'" << std::endl;
            return false;
        }
    }

    std::cout << "Concurrency: " << concurrency << std::endl;
    std::cout << "Chunks: " << chunks << std::endl;
    std::cout << "Chunk Size: " << chunk_size << std::endl;

    return true;
}

std::string generate_dummy_data(size_t size) {
    static const char* pattern = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz~!@#$%^&*()_+-={}[]|;:',./?><";
    static const size_t pattern_len = strlen(pattern);

    std::string s;
    s.resize(size);

    size_t offset = 0;
    while (offset < size) {
        size_t copy_len = std::min(pattern_len, size - offset);
        std::memcpy(&s[offset], pattern, copy_len);
        offset += copy_len;
    }

    return s;
}

// Function to handle CQ shutdown and draining for a given TaskResource
void shutdown_and_drain_cq(shared_ptr<TaskResource> task) {
    // std::cout << "[Task " << task->id << "] Shutting down CQ..." << std::endl;
    if (task->cq) {
        task->cq->Shutdown(); // Initiate shutdown
        void* received_tag = nullptr;
        bool ok_cq = false;
        // Drain any remaining events. Next will return false when queue is empty after shutdown.
        while (task->cq->Next(&received_tag, &ok_cq)) {
            // Any tags received here after Shutdown() are events that were
            // enqueued before Shutdown() was called but processed after.
            // In this model, they should relate to this task's op.
            // assert(received_tag == (void*)task->self_ptr.get()); // Optional assert
            // std::cout << "[Task " << task->id << "] Drained a late tag: " << received_tag << std::endl; // Optional debug log
        }
        // std::cout << "[Task " << task->id << "] CQ drained." << std::endl; // Optional debug log
    } else {
        cerr << "[Task " << task->id << "] CQ was not initialized, skipping shutdown/drain." << endl;
    }
}


void run_upload_only(shared_ptr<TaskResource> task, int chunks, long long chunk_size) {
    auto stub = CCcloud::FileService::NewStub(task->channel);
    void* received_tag = nullptr;
    bool ok_cq = false;

    unique_ptr<ClientContext> upload_ctx = make_unique<ClientContext>();
    CCcloud::UploadResponse upload_response;
    Status upload_status;
    unique_ptr<ClientAsyncWriter<CCcloud::UploadChunk>> upload_writer;

    enum class UploadStep { START, WRITE, WRITES_DONE, FINISH };

    // Start Upload RPC
    upload_writer = stub->AsyncUpload(upload_ctx.get(), &upload_response, task->cq.get(), (void*)UploadStep::START);
    if (!task->cq->Next(&received_tag, &ok_cq) || !ok_cq || received_tag != (void*)UploadStep::START) {
        cerr << "[Task " << task->id << "] Upload failed to start." << endl;
        return;
    }

    // Write chunks one by one
    for (int i = 0; i < chunks; ++i) {
        CCcloud::UploadChunk chunk;
        if (i == 0) chunk.set_filename("test_" + to_string(task->id) + ".bin");
        chunk.set_data(generate_dummy_data(chunk_size));

        upload_writer->Write(chunk, (void*)UploadStep::WRITE);
        if (!task->cq->Next(&received_tag, &ok_cq) || !ok_cq || received_tag != (void*)UploadStep::WRITE) {
            cerr << "[Task " << task->id << "] Upload failed during write " << i << "." << endl;
            upload_writer->Finish(&upload_status, (void*)UploadStep::FINISH);
            task->cq->Next(&received_tag, &ok_cq);  // Try to finish cleanly
            return;
        }
    }
    // Finish writing
    upload_writer->WritesDone((void*)UploadStep::WRITES_DONE);

    if (!task->cq->Next(&received_tag, &ok_cq) || !ok_cq || received_tag != (void*)UploadStep::WRITES_DONE) {
        cerr << "[Task " << task->id << "] Upload failed: WritesDone error." << endl;
        upload_writer->Finish(&upload_status, (void*)UploadStep::FINISH);
        task->cq->Next(&received_tag, &ok_cq);
        return;
    }

    // Wait for RPC finish
    upload_writer->Finish(&upload_status, (void*)UploadStep::FINISH);
    if (!task->cq->Next(&received_tag, &ok_cq) || received_tag != (void*)UploadStep::FINISH) {
        cerr << "[Task " << task->id << "] Upload failed: Finish wait error." << endl;
        return;
    }

    if (upload_status.ok()) {
        cout << "[Task " << task->id << "] Upload success: " << upload_response.message() << endl;
    } else {
        cerr << "[Task " << task->id << "] Upload failed: " << upload_status.error_message() << endl;
    }

    shutdown_and_drain_cq(task);
}



// Function to perform only the Download RPC for a task thread
void run_download_only(shared_ptr<TaskResource> task) {
     auto stub = CCcloud::FileService::NewStub(task->channel);
     void* received_tag = nullptr;
     bool ok_cq = false;

     unique_ptr<ClientContext> download_ctx = make_unique<ClientContext>();
     CCcloud::DownloadRequest download_request;
     Status download_status; // Final status for the download RPC
     unique_ptr<ClientAsyncReader<CCcloud::DownloadChunk>> download_reader; // Client side reader for server streaming
     CCcloud::DownloadChunk download_chunk; // Reusable chunk message for receiving data

     download_request.set_filename("test_" + to_string(task->id) + ".bin");

     // Initiate download call. Use task->self_ptr.get() as the tag.
     download_reader = stub->AsyncDownload(download_ctx.get(), download_request, task->cq.get(), (void*)task->self_ptr.get());

     // Wait for AsyncDownload start
      if (!task->cq->Next(&received_tag, &ok_cq) || !ok_cq) {
         cerr << "[Task " << task->id << "] Download Failed: Failed to start download call or CQ shut down before start." << endl;
         download_status = Status(grpc::StatusCode::INTERNAL, "Failed to initiate call");
         goto download_cleanup; // Jump to cleanup
     }
     // assert(received_tag == (void*)task->self_ptr.get());

     // Read chunks
     while (true) {
         // Initiate async read for the next chunk. Use task->self_ptr.get() as the tag.
         download_reader->Read(&download_chunk, (void*)task->self_ptr.get());
         // Wait for Read completion
          if (!task->cq->Next(&received_tag, &ok_cq)) {
              cerr << "[Task " << task->id << "] Download Failed: Failed waiting for Read completion or CQ shut down." << endl;
              ok_cq = false; // CQ shutdown means fail
              break; // Exit read loop
          }
          // assert(received_tag == (void*)task->self_ptr.get());

         if (!ok_cq) {
             // Read operation itself failed (!ok_cq). This indicates end of stream after the last message
             // has been read, OR an error occurred during streaming.
             break; // Exit read loop - normal end or error
         }

         // ok_cq is true: Process received chunk data
         // std::cout << "[Task " << task->id << "] Received " << download_chunk.data().size() << " bytes for download." << std::endl; // Optional log
         // In a real app, you'd write this data to a file or process it.
     }

     // Finish download RPC (gets final status)
     // This MUST be called after the read loop finishes (!ok_cq from Read) to get the final status.
     // Use task->self_ptr.get() as the tag.
     if (download_reader) {
         download_reader->Finish(&download_status, (void*)task->self_ptr.get());
         // Wait for Finish completion
         if (!task->cq->Next(&received_tag, &ok_cq)) {
              cerr << "[Task " << task->id << "] Download Failed: Failed waiting for Finish completion or CQ shut down." << endl;
              // Status might not be set correctly if Finish wait failed.
         }
         // assert(received_tag == (void*)task->self_ptr.get());
     } else {
          download_status = Status(grpc::StatusCode::INTERNAL, "Download reader not initialized");
     }


download_cleanup: // Label for cleanup within this function
     if (download_status.ok()) {
         cout << "[Task " << task->id << "] Download success." << endl;
     } else {
         cerr << "[Task " << task->id << "] Download failed: " << download_status.error_message() << endl;
     }

     // --- CQ Shutdown and Drain ---
     shutdown_and_drain_cq(task);
}


// Function to perform only the Delete RPC for a task thread
void run_delete_only(shared_ptr<TaskResource> task) {
    auto stub = CCcloud::FileService::NewStub(task->channel);
    void* received_tag = nullptr;
    bool ok_cq = false;

    unique_ptr<ClientContext> delete_ctx = make_unique<ClientContext>();
    CCcloud::DeleteRequest delete_request;
    CCcloud::DeleteResponse delete_response; // Response message for unary RPC
    Status delete_status; // Final status for unary RPC
    unique_ptr<ClientAsyncResponseReader<CCcloud::DeleteResponse>> delete_response_reader; // Unary RPC client side uses ClientAsyncResponseReader

    delete_request.set_filename("test_" + to_string(task->id) + ".bin");

    // Initiate delete call (unary RPC)
    delete_response_reader = stub->AsyncDelete(delete_ctx.get(), delete_request, task->cq.get());
    // The Finish method for a unary client RPC includes sending the request
    // and waiting for the response/status. The tag is returned when it's all done.
    // Use task->self_ptr.get() as the tag.
    if (delete_response_reader) {
        delete_response_reader->Finish(&delete_response, &delete_status, (void*)task->self_ptr.get());
        // Wait for the entire unary RPC (Delete) to complete
        if (!task->cq->Next(&received_tag, &ok_cq)) {
             cerr << "[Task " << task->id << "] Delete Failed: Failed waiting for RPC completion or CQ shut down." << endl;
             // Status might not be set correctly.
        }
        // assert(received_tag == (void*)task->self_ptr.get());
    } else {
        delete_status = Status(grpc::StatusCode::INTERNAL, "Delete response reader not initialized");
    }


    if (delete_status.ok()) {
        cout << "[Task " << task->id << "] Delete success: " << delete_response.message() << endl;
    } else {
        cerr << "[Task " << task->id << "] Delete failed: " << delete_status.error_message() << endl;
    }

    // --- CQ Shutdown and Drain ---
    shutdown_and_drain_cq(task);
}


int main(int argc, char** argv) {
    std::cout << "Usage: " << argv[0] << " [--concurrency=<num>] [--chunks=<num>] [--chunk_size=<size>]" << std::endl;

    bool flag = parseArgs(argc, argv);
    if (!flag) {
        // Error message already printed in parseArgs
        return 1;
    }

    // Using localhost:9527 as the server address
    auto channel = grpc::CreateChannel("localhost:9527", grpc::InsecureChannelCredentials());

    auto total_program_start_time = chrono::steady_clock::now();

    // --- UPLOAD PHASE ---
    cout << "\n--- Starting Upload Phase ---" << endl;
    auto upload_phase_start_time = chrono::steady_clock::now();
    vector<thread> upload_workers;
    vector<shared_ptr<TaskResource>> upload_tasks; // Resources for upload phase

    for (int i = 0; i < concurrency; ++i) {
        auto task = make_shared<TaskResource>();
        task->id = i;
        task->channel = channel;
        task->cq = make_unique<CompletionQueue>(); // Create a unique CQ for this task thread
        task->self_ptr = task; // Set the self-reference shared_ptr for the tag
        upload_tasks.push_back(task); // Store the task resource

        // Create a thread and pass the shared_ptr<TaskResource> by value (copied).
        upload_workers.emplace_back(run_upload_only, task, chunks, chunk_size);
    }

    // Join all Upload threads
    for (auto& t : upload_workers) t.join();
    auto upload_phase_end_time = chrono::steady_clock::now();
    auto upload_phase_duration = chrono::duration_cast<chrono::milliseconds>(upload_phase_end_time - upload_phase_start_time).count();
    cout << "--- Upload Phase Finished in " << upload_phase_duration << " ms ---" << endl;
    // upload_tasks vector goes out of scope here, cleaning up Upload TaskResources and their CQs.
    // upload_workers vector goes out of scope here.


    // // --- DOWNLOAD PHASE ---
    // cout << "\n--- Starting Download Phase ---" << endl;
    // auto download_phase_start_time = chrono::steady_clock::now();
    // vector<thread> download_workers;
    // vector<shared_ptr<TaskResource>> download_tasks; // Resources for download phase

    // for (int i = 0; i < concurrency; ++i) {
    //     auto task = make_shared<TaskResource>();
    //     task->id = i;
    //     task->channel = channel;
    //     task->cq = make_unique<CompletionQueue>(); // Create a unique CQ for this task thread
    //     task->self_ptr = task; // Set the self-reference shared_ptr for the tag
    //     download_tasks.push_back(task); // Store the task resource

    //     // Create a thread and pass the shared_ptr<TaskResource> by value (copied).
    //     download_workers.emplace_back(run_download_only, task);
    // }

    // // Join all Download threads
    // for (auto& t : download_workers) t.join();
    // auto download_phase_end_time = chrono::steady_clock::now();
    // auto download_phase_duration = chrono::duration_cast<chrono::milliseconds>(download_phase_end_time - download_phase_start_time).count();
    // cout << "--- Download Phase Finished in " << download_phase_duration << " ms ---" << endl;
    // // download_tasks vector goes out of scope here.
    // // download_workers vector goes out of scope here.


    // // --- DELETE PHASE ---
    // cout << "\n--- Starting Delete Phase ---" << endl;
    // auto delete_phase_start_time = chrono::steady_clock::now();
    // vector<thread> delete_workers;
    // vector<shared_ptr<TaskResource>> delete_tasks; // Resources for delete phase

    // for (int i = 0; i < concurrency; ++i) {
    //     auto task = make_shared<TaskResource>();
    //     task->id = i;
    //     task->channel = channel;
    //     task->cq = make_unique<CompletionQueue>(); // Create a unique CQ for this task thread
    //     task->self_ptr = task; // Set the self-reference shared_ptr for the tag
    //     delete_tasks.push_back(task); // Store the task resource

    //     // Create a thread and pass the shared_ptr<TaskResource> by value (copied).
    //     delete_workers.emplace_back(run_delete_only, task);
    // }

    // // Join all Delete threads
    // for (auto& t : delete_workers) t.join();
    // auto delete_phase_end_time = chrono::steady_clock::now();
    // auto delete_phase_duration = chrono::duration_cast<chrono::milliseconds>(delete_phase_end_time - delete_phase_start_time).count();
    // cout << "--- Delete Phase Finished in " << delete_phase_duration << " ms ---" << endl;
    // // delete_tasks vector goes out of scope here.
    // // delete_workers vector goes out of scope here.


    auto total_program_execution_duration = chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - total_program_start_time).count();
    cout << "\n--- All Phases Completed ---" << endl;
    cout << "Total program execution time: " << total_program_execution_duration << " ms for " << concurrency << " tasks." << endl;


    return 0;
}
