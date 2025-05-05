# CCcloud - Cloud Storage System (In Progress)

Personal Project: High-performance cloud storage service with asynchronous logging and scalable multi-protocol interfaces.

---

## 📖 Project Overview

This project aims to build a lightweight, **high-performance cloud storage system**, providing robust support for **large-scale concurrent file uploads** over both **gRPC** and **HTTP** protocols.

The current repository contains the first foundational component:
a **high-throughput asynchronous logger**, designed to support internal system logging, access auditing, and fault diagnosis without impacting service performance.

The asynchronous logger module has been thoroughly stress-tested and will be integrated into the cloud storage server as a critical infrastructure piece.

---

## ✨ Features

* **Asynchronous Logging Subsystem**

  * Single background consumer thread.
  * Producer/Consumer separation with minimal impact on application threads.
  * Structured log entries with automatic timestamping.
  * Batch write + timeout-triggered flush (threshold or interval).
  * Rolling log files when file size exceeds configurable limits.
  * Thread-safe global singleton design.

* **Two Lock-Free Queue Implementations**

  * `EBRQueue`: Michael-Scott Queue + Epoch-Based Reclamation (safe memory management).
  * `LockFreeMPMCQueue`: Bounded ring buffer, fully lock-free for predictable memory usage.

* **Independent stress testing under high concurrency**

---

## 🔭 Future Development

* **File Upload Service**:

  * Support **RPC API** for high-efficiency streaming uploads.
  * Support **HTTP API** for wide client compatibility.

* **Cloud Storage Core**

  * Distributed storage node architecture.
  * Persistent metadata management.
  * File sharding, replication, and recovery.

* **Enhanced Observability**

  * Integrate asynchronous logger into all subsystems.
  * Support dynamic log filtering and log levels at runtime.

* **Performance and Reliability**

  * Load balancing for uploads.
  * Rate limiting, backpressure, and overload protection.

---

## 📀 Architecture (Current Module: Async Logger)

```plaintext
+----------------+
| Application    |
| (Logger.append)|
+--------+-------+
         |
         v
+------------------------+
| Lock-free Message Queue |
| (EBRQueue / RingBuffer) |
+-----------+------------+
            |
            v
+-------------------------------+
| Background Flush Thread       |
| - Batch dequeue and format    |
| - Flush to log file           |
| - Roll log files when oversized |
+-------------------------------+
```

---

## Building Instruction

### 1. Clone the repository

```bash
git clone <repo_url>
```

### 2. Create and enter the build directory

```bash
mkdir build
cd build
```

### 3. Configure the project with CMake and build the project

```bash
cmake .. && make
```

### 4. Run the logger test

```bash
./bin/tests/test_logger
```

All executable files are organized under build/bin/.

---

## ⚡ Performance Testing

### Asynchronous Logger Performance

The asynchronous logger subsystem has been independently stress-tested under high concurrency:

#### Test Configuration

* **128 concurrent threads**, each producing **100,000 simple log entries**
* **Total**: **12.8 million log entries** generated.

```C++
constexpr int thread_count = 128;
constexpr int logs_per_thread = 100000;

for (int i = 0; i < thread_count; ++i) {
    threads.emplace_back([i, &logger]() {
        for (int j = 0; j < logs_per_thread; ++j) {
            std::ostringstream oss;
            oss << "Hahaha! *** Thread " << i << " log " << j << " ***";
            logger.append(LogEntry(Level::INFO, oss.str()));
        }
    });
}
```

#### Results

* The logger system successfully completed writing all **12.8 million logs** in approximately **25 seconds**.
* No data loss, no thread contention issues observed.
* **Average sustained logging throughput**: approximately **512,000 logs per second**.

#### Test Environment

* **CPU**: 12-core processor (24 logic cores)
* **Memory**: 32GB RAM (7500MT/s)
* **Operating System**: WSL2
* **Compiler**: g++ 14.2
* **C++ Standard**: C++20

#### Queue Backends

* Both **`EBRQueue`** (Michael-Scott + EBR) and **`LockFreeMPMCQueue`** (RingBuffer) achieved comparable performance.

#### Conclusion

The logger demonstrates strong scalability and high throughput, meeting production-level concurrency requirements.

---

### Preliminary File Operation Benchmark (Local Loopback)

All tests are conducted on a **local loopback setup**, with both server and client running on the same host machine.

| File Size | File Count | Operation | Time (ms)   |
| --------- | ---------- | --------- | ----------- |
| 4 MB      | 128        | Upload    | ~400        |
|           |            | Download  | ~400        |
|           |            | Delete    | ~20         |
| 400 MB    | 12         | Upload    | ~2500       |
|           |            | Download  | ~2500       |
|           |            | Delete    | ~60         |
| 4 GB      | 1          | Upload    | ~2500       |
|           |            | Download  | ~3000       |
|           |            | Dlete     | ~200        |

> These numbers serve as an early baseline and will be re-evaluated under more realistic environments and external network settings.

---

### Full System Benchmark (Planned)

Once the full cloud storage server is completed, additional benchmarks will be conducted to measure:

* Full pipeline upload throughput (**RPC / HTTP**)
* Metadata storage latency
* Node replication and sharding performance
