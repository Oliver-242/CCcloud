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
#include <stdexcept>

#include "generated/file.grpc.pb.h"

using namespace std;
using grpc::Channel;
using grpc::ClientAsyncWriter;
using grpc::ClientContext;
using grpc::CompletionQueue;
using grpc::Status;

// Use const int instead of static constinit if C++20 is not strictly required or for broader compatibility
static const int concurrency_default = 128;
static const int chunks_default = 100;
static const long long chunk_size_default = 9128;

int concurrency = concurrency_default;
int chunks = chunks_default;
long long chunk_size = chunk_size_default;


struct UploadTask {
    unique_ptr<ClientContext> context;
    unique_ptr<ClientAsyncWriter<CCcloud::UploadChunk>> writer;
    CCcloud::UploadResponse response;
    Status status;
    // Removed: void* tag; // tag will be received locally in run_upload
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
        // Re-throw with argName context
        throw std::invalid_argument("Invalid value for argument '" + argName + "': " + e.what());
    } catch (const std::out_of_range& e) {
        // Re-throw with argName context
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
    static const char* pattern = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
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

// run_upload 函数接收该线程专属的 CompletionQueue 的 unique_ptr 所有权
void run_upload(int id, int chunks, int chunk_size, shared_ptr<Channel> channel, unique_ptr<CompletionQueue> cq) {
    auto stub = CCcloud::FileService::NewStub(channel);
    // shared_ptr ensures UploadTask lives until no tags or smart pointers reference it
    auto task = make_shared<UploadTask>();

    task->context = make_unique<ClientContext>();

    // Initiate the asynchronous upload call. Pass task.get() as the tag.
    task->writer = stub->AsyncUpload(task->context.get(), &(task->response), cq.get(), (void*)task.get());

    void* received_tag = nullptr; // Local variable to receive the tag
    bool ok_cq = false; // Local variable to receive the ok status of the operation

    // --- Wait for AsyncUpload start ---
    // Blocking wait on *this thread's* dedicated CQ for the initial event.
    // cq->Next returns true if an event was dequeued, false if the CQ is shutting down.
    // ok_cq indicates if the operation associated with the tag was successful.
    if (!cq->Next(&received_tag, &ok_cq) || !ok_cq) {
         cerr << "[Task " << id << "] Failed to start upload call or CQ shut down before start." << endl;
         // No cleanup needed for task pointer here, shared_ptr handles it.
         // CQ will be shut down and drained later implicitly when unique_ptr goes out of scope.
         // Explicit shutdown/drain is safer though.
         // For simplicity in this test, we just return if initial call fails.
         return;
    }
    // Optional safety check: Assert that the received tag is the one we expect.
    // assert(received_tag == (void*)task.get());


    // --- Send chunks ---
    for (int i = 0; i < chunks; ++i) {
        CCcloud::UploadChunk chunk;
        // Set filename only in the first chunk
        if (i == 0) {
            chunk.set_filename("test_" + to_string(id) + ".txt");
        }
        chunk.set_data(generate_dummy_data(chunk_size));

        // Initiate async write. Pass task.get() as the tag.
        task->writer->Write(chunk, (void*)task.get());

        // Wait for this specific Write to complete on *this thread's* CQ.
        // We must wait for the previous write to complete before initiating the next one
        // in this per-op blocking model.
        if (!cq->Next(&received_tag, &ok_cq) || !ok_cq) {
            cerr << "[Task " << id << "] Failed waiting for Write " << i << " completion or CQ shut down." << endl;
            // If Write failed (!ok_cq), the RPC is likely broken. Break loop and try to finish.
            // assert(received_tag == (void*)task.get()); // Check which tag was returned
            break; // Exit chunk loop
        }
        // Optional safety check: assert(received_tag == (void*)task.get());
    }

    // --- Signal end of writes ---
    // Only proceed if the last operation was ok (or if loop finished normally).
    // ok_cq reflects the status of the last cq->Next call.
    if (ok_cq) {
        task->writer->WritesDone((void*)task.get());
        // Wait for WritesDone to complete on *this thread's* CQ.
        if (!cq->Next(&received_tag, &ok_cq)) { // ok_cq here indicates op success
             cerr << "[Task " << id << "] Failed waiting for WritesDone completion or CQ shut down." << endl;
             // If Next returns false, it means CQ is shutting down, ok_cq is also false.
             ok_cq = false; // Explicitly set ok_cq to false if Next returned false
        }
        // Optional safety check: assert(received_tag == (void*)task.get());
    } else {
        // If we failed earlier (e.g., write failed), the RPC might be in a bad state.
        // The subsequent Finish might fail, or we might need to Cancel it.
        // task->context->TryCancel(); // Consider cancelling the RPC asynchronously
        // For this test, let's proceed to Finish regardless, it should handle cleanup.
        // assert(received_tag == (void*)task.get()); // Check which tag was returned
    }


    // --- Finish the RPC ---
    // Initiate Finish operation. Pass task.get() as the tag.
    // This MUST be called to get the final status and complete the RPC.
    task->writer->Finish(&task->status, (void*)task.get());
    // Wait for Finish to complete on *this thread's* CQ.
    if (!cq->Next(&received_tag, &ok_cq)) {
         cerr << "[Task " << id << "] Failed waiting for RPC Finish completion or CQ shut down." << endl;
         // ok_cq might be false, task->status might be non-OK.
         // assert(received_tag == (void*)task.get()); // Check which tag was returned
    }
    // Optional safety check: assert(received_tag == (void*)task.get());


    // Check final status
    if (task->status.ok()) {
        cout << "[Task " << id << "] Upload success: " << task->response.message() << endl;
    } else {
        cerr << "[Task " << id << "] Upload failed: " << task->status.error_message() << endl;
    }

    // --- CQ Shutdown and Drain ---
    // This thread's CQ has completed its work for this RPC.
    // Correctly shut down the CQ and drain any pending events before destroying it.
    cq->Shutdown(); // Initiate shutdown
    // Drain any remaining events. Next will return false when queue is empty after shutdown.
    while (cq->Next(&received_tag, &ok_cq)) {
        // Any tags received here after Shutdown() are events that were
        // enqueued before Shutdown() was called but processed after.
        // For this simple blocking model, they should still be task.get(),
        // but in more complex scenarios, you might need to handle them.
        // assert(received_tag == (void*)task.get()); // Should still be task.get() for this task's CQ
    }
    // The unique_ptr<CompletionQueue> 'cq' goes out of scope here, destroying the CQ.
    // This is now safe after the CQ has been properly shut down and drained.

    // The shared_ptr<UploadTask> 'task' goes out of scope here, cleaning up the task object.
    // This ensures the task object is alive while its tags might be processed by cq->Next,
    // and the CQ is properly managed.
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

    // Removed: CompletionQueue cq; // Don't use a single shared CQ for blocking per-op threads

    vector<thread> workers;
    // Vector to hold unique_ptrs to CompletionQueues, managing their lifetime
    vector<unique_ptr<CompletionQueue>> cqs;

    auto start = chrono::steady_clock::now();

    for (int i = 0; i < concurrency; ++i) {
        // Create a unique CompletionQueue for each thread/RPC
        auto thread_cq = make_unique<CompletionQueue>();
        // Store the unique_ptr in the vector to manage its lifetime
        cqs.push_back(std::move(thread_cq));
        // Pass the unique_ptr by moving it into the thread's constructor arguments.
        // The thread now owns this specific CQ.
        workers.emplace_back(run_upload, i, chunks, chunk_size, channel, std::move(cqs.back()));
    }

    // Join all the threads. Main thread waits here until all workers finish.
    for (auto& t : workers) t.join();

    // The 'cqs' vector goes out of scope here after all threads have joined.
    // Each unique_ptr in the vector will be destroyed. Since each CQ was properly
    // shut down and drained within its thread (in run_upload), destroying the unique_ptr
    // (and thus the CQ) is now safe.

    auto duration = chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - start).count();
    cout << "\nTotal time: " << duration << " ms for " << concurrency << " uploads." << endl;
    cout << "Avg: " << duration / concurrency << " ms/upload." << endl;

    return 0;
}