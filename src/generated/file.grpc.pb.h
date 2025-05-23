// Generated by the gRPC C++ plugin.
// If you make any local change, they will be lost.
// source: file.proto
#ifndef GRPC_file_2eproto__INCLUDED
#define GRPC_file_2eproto__INCLUDED

#include "file.pb.h"

#include <functional>
#include <grpcpp/generic/async_generic_service.h>
#include <grpcpp/support/async_stream.h>
#include <grpcpp/support/async_unary_call.h>
#include <grpcpp/support/client_callback.h>
#include <grpcpp/client_context.h>
#include <grpcpp/completion_queue.h>
#include <grpcpp/support/message_allocator.h>
#include <grpcpp/support/method_handler.h>
#include <grpcpp/impl/proto_utils.h>
#include <grpcpp/impl/rpc_method.h>
#include <grpcpp/support/server_callback.h>
#include <grpcpp/impl/server_callback_handlers.h>
#include <grpcpp/server_context.h>
#include <grpcpp/impl/service_type.h>
#include <grpcpp/support/status.h>
#include <grpcpp/support/stub_options.h>
#include <grpcpp/support/sync_stream.h>
#include <grpcpp/ports_def.inc>

namespace CCcloud {

class FileService final {
 public:
  static constexpr char const* service_full_name() {
    return "CCcloud.FileService";
  }
  class StubInterface {
   public:
    virtual ~StubInterface() {}
    std::unique_ptr< ::grpc::ClientWriterInterface< ::CCcloud::UploadChunk>> Upload(::grpc::ClientContext* context, ::CCcloud::UploadResponse* response) {
      return std::unique_ptr< ::grpc::ClientWriterInterface< ::CCcloud::UploadChunk>>(UploadRaw(context, response));
    }
    std::unique_ptr< ::grpc::ClientAsyncWriterInterface< ::CCcloud::UploadChunk>> AsyncUpload(::grpc::ClientContext* context, ::CCcloud::UploadResponse* response, ::grpc::CompletionQueue* cq, void* tag) {
      return std::unique_ptr< ::grpc::ClientAsyncWriterInterface< ::CCcloud::UploadChunk>>(AsyncUploadRaw(context, response, cq, tag));
    }
    std::unique_ptr< ::grpc::ClientAsyncWriterInterface< ::CCcloud::UploadChunk>> PrepareAsyncUpload(::grpc::ClientContext* context, ::CCcloud::UploadResponse* response, ::grpc::CompletionQueue* cq) {
      return std::unique_ptr< ::grpc::ClientAsyncWriterInterface< ::CCcloud::UploadChunk>>(PrepareAsyncUploadRaw(context, response, cq));
    }
    std::unique_ptr< ::grpc::ClientReaderInterface< ::CCcloud::DownloadChunk>> Download(::grpc::ClientContext* context, const ::CCcloud::DownloadRequest& request) {
      return std::unique_ptr< ::grpc::ClientReaderInterface< ::CCcloud::DownloadChunk>>(DownloadRaw(context, request));
    }
    std::unique_ptr< ::grpc::ClientAsyncReaderInterface< ::CCcloud::DownloadChunk>> AsyncDownload(::grpc::ClientContext* context, const ::CCcloud::DownloadRequest& request, ::grpc::CompletionQueue* cq, void* tag) {
      return std::unique_ptr< ::grpc::ClientAsyncReaderInterface< ::CCcloud::DownloadChunk>>(AsyncDownloadRaw(context, request, cq, tag));
    }
    std::unique_ptr< ::grpc::ClientAsyncReaderInterface< ::CCcloud::DownloadChunk>> PrepareAsyncDownload(::grpc::ClientContext* context, const ::CCcloud::DownloadRequest& request, ::grpc::CompletionQueue* cq) {
      return std::unique_ptr< ::grpc::ClientAsyncReaderInterface< ::CCcloud::DownloadChunk>>(PrepareAsyncDownloadRaw(context, request, cq));
    }
    virtual ::grpc::Status Delete(::grpc::ClientContext* context, const ::CCcloud::DeleteRequest& request, ::CCcloud::DeleteResponse* response) = 0;
    std::unique_ptr< ::grpc::ClientAsyncResponseReaderInterface< ::CCcloud::DeleteResponse>> AsyncDelete(::grpc::ClientContext* context, const ::CCcloud::DeleteRequest& request, ::grpc::CompletionQueue* cq) {
      return std::unique_ptr< ::grpc::ClientAsyncResponseReaderInterface< ::CCcloud::DeleteResponse>>(AsyncDeleteRaw(context, request, cq));
    }
    std::unique_ptr< ::grpc::ClientAsyncResponseReaderInterface< ::CCcloud::DeleteResponse>> PrepareAsyncDelete(::grpc::ClientContext* context, const ::CCcloud::DeleteRequest& request, ::grpc::CompletionQueue* cq) {
      return std::unique_ptr< ::grpc::ClientAsyncResponseReaderInterface< ::CCcloud::DeleteResponse>>(PrepareAsyncDeleteRaw(context, request, cq));
    }
    class async_interface {
     public:
      virtual ~async_interface() {}
      virtual void Upload(::grpc::ClientContext* context, ::CCcloud::UploadResponse* response, ::grpc::ClientWriteReactor< ::CCcloud::UploadChunk>* reactor) = 0;
      virtual void Download(::grpc::ClientContext* context, const ::CCcloud::DownloadRequest* request, ::grpc::ClientReadReactor< ::CCcloud::DownloadChunk>* reactor) = 0;
      virtual void Delete(::grpc::ClientContext* context, const ::CCcloud::DeleteRequest* request, ::CCcloud::DeleteResponse* response, std::function<void(::grpc::Status)>) = 0;
      virtual void Delete(::grpc::ClientContext* context, const ::CCcloud::DeleteRequest* request, ::CCcloud::DeleteResponse* response, ::grpc::ClientUnaryReactor* reactor) = 0;
    };
    typedef class async_interface experimental_async_interface;
    virtual class async_interface* async() { return nullptr; }
    class async_interface* experimental_async() { return async(); }
   private:
    virtual ::grpc::ClientWriterInterface< ::CCcloud::UploadChunk>* UploadRaw(::grpc::ClientContext* context, ::CCcloud::UploadResponse* response) = 0;
    virtual ::grpc::ClientAsyncWriterInterface< ::CCcloud::UploadChunk>* AsyncUploadRaw(::grpc::ClientContext* context, ::CCcloud::UploadResponse* response, ::grpc::CompletionQueue* cq, void* tag) = 0;
    virtual ::grpc::ClientAsyncWriterInterface< ::CCcloud::UploadChunk>* PrepareAsyncUploadRaw(::grpc::ClientContext* context, ::CCcloud::UploadResponse* response, ::grpc::CompletionQueue* cq) = 0;
    virtual ::grpc::ClientReaderInterface< ::CCcloud::DownloadChunk>* DownloadRaw(::grpc::ClientContext* context, const ::CCcloud::DownloadRequest& request) = 0;
    virtual ::grpc::ClientAsyncReaderInterface< ::CCcloud::DownloadChunk>* AsyncDownloadRaw(::grpc::ClientContext* context, const ::CCcloud::DownloadRequest& request, ::grpc::CompletionQueue* cq, void* tag) = 0;
    virtual ::grpc::ClientAsyncReaderInterface< ::CCcloud::DownloadChunk>* PrepareAsyncDownloadRaw(::grpc::ClientContext* context, const ::CCcloud::DownloadRequest& request, ::grpc::CompletionQueue* cq) = 0;
    virtual ::grpc::ClientAsyncResponseReaderInterface< ::CCcloud::DeleteResponse>* AsyncDeleteRaw(::grpc::ClientContext* context, const ::CCcloud::DeleteRequest& request, ::grpc::CompletionQueue* cq) = 0;
    virtual ::grpc::ClientAsyncResponseReaderInterface< ::CCcloud::DeleteResponse>* PrepareAsyncDeleteRaw(::grpc::ClientContext* context, const ::CCcloud::DeleteRequest& request, ::grpc::CompletionQueue* cq) = 0;
  };
  class Stub final : public StubInterface {
   public:
    Stub(const std::shared_ptr< ::grpc::ChannelInterface>& channel, const ::grpc::StubOptions& options = ::grpc::StubOptions());
    std::unique_ptr< ::grpc::ClientWriter< ::CCcloud::UploadChunk>> Upload(::grpc::ClientContext* context, ::CCcloud::UploadResponse* response) {
      return std::unique_ptr< ::grpc::ClientWriter< ::CCcloud::UploadChunk>>(UploadRaw(context, response));
    }
    std::unique_ptr< ::grpc::ClientAsyncWriter< ::CCcloud::UploadChunk>> AsyncUpload(::grpc::ClientContext* context, ::CCcloud::UploadResponse* response, ::grpc::CompletionQueue* cq, void* tag) {
      return std::unique_ptr< ::grpc::ClientAsyncWriter< ::CCcloud::UploadChunk>>(AsyncUploadRaw(context, response, cq, tag));
    }
    std::unique_ptr< ::grpc::ClientAsyncWriter< ::CCcloud::UploadChunk>> PrepareAsyncUpload(::grpc::ClientContext* context, ::CCcloud::UploadResponse* response, ::grpc::CompletionQueue* cq) {
      return std::unique_ptr< ::grpc::ClientAsyncWriter< ::CCcloud::UploadChunk>>(PrepareAsyncUploadRaw(context, response, cq));
    }
    std::unique_ptr< ::grpc::ClientReader< ::CCcloud::DownloadChunk>> Download(::grpc::ClientContext* context, const ::CCcloud::DownloadRequest& request) {
      return std::unique_ptr< ::grpc::ClientReader< ::CCcloud::DownloadChunk>>(DownloadRaw(context, request));
    }
    std::unique_ptr< ::grpc::ClientAsyncReader< ::CCcloud::DownloadChunk>> AsyncDownload(::grpc::ClientContext* context, const ::CCcloud::DownloadRequest& request, ::grpc::CompletionQueue* cq, void* tag) {
      return std::unique_ptr< ::grpc::ClientAsyncReader< ::CCcloud::DownloadChunk>>(AsyncDownloadRaw(context, request, cq, tag));
    }
    std::unique_ptr< ::grpc::ClientAsyncReader< ::CCcloud::DownloadChunk>> PrepareAsyncDownload(::grpc::ClientContext* context, const ::CCcloud::DownloadRequest& request, ::grpc::CompletionQueue* cq) {
      return std::unique_ptr< ::grpc::ClientAsyncReader< ::CCcloud::DownloadChunk>>(PrepareAsyncDownloadRaw(context, request, cq));
    }
    ::grpc::Status Delete(::grpc::ClientContext* context, const ::CCcloud::DeleteRequest& request, ::CCcloud::DeleteResponse* response) override;
    std::unique_ptr< ::grpc::ClientAsyncResponseReader< ::CCcloud::DeleteResponse>> AsyncDelete(::grpc::ClientContext* context, const ::CCcloud::DeleteRequest& request, ::grpc::CompletionQueue* cq) {
      return std::unique_ptr< ::grpc::ClientAsyncResponseReader< ::CCcloud::DeleteResponse>>(AsyncDeleteRaw(context, request, cq));
    }
    std::unique_ptr< ::grpc::ClientAsyncResponseReader< ::CCcloud::DeleteResponse>> PrepareAsyncDelete(::grpc::ClientContext* context, const ::CCcloud::DeleteRequest& request, ::grpc::CompletionQueue* cq) {
      return std::unique_ptr< ::grpc::ClientAsyncResponseReader< ::CCcloud::DeleteResponse>>(PrepareAsyncDeleteRaw(context, request, cq));
    }
    class async final :
      public StubInterface::async_interface {
     public:
      void Upload(::grpc::ClientContext* context, ::CCcloud::UploadResponse* response, ::grpc::ClientWriteReactor< ::CCcloud::UploadChunk>* reactor) override;
      void Download(::grpc::ClientContext* context, const ::CCcloud::DownloadRequest* request, ::grpc::ClientReadReactor< ::CCcloud::DownloadChunk>* reactor) override;
      void Delete(::grpc::ClientContext* context, const ::CCcloud::DeleteRequest* request, ::CCcloud::DeleteResponse* response, std::function<void(::grpc::Status)>) override;
      void Delete(::grpc::ClientContext* context, const ::CCcloud::DeleteRequest* request, ::CCcloud::DeleteResponse* response, ::grpc::ClientUnaryReactor* reactor) override;
     private:
      friend class Stub;
      explicit async(Stub* stub): stub_(stub) { }
      Stub* stub() { return stub_; }
      Stub* stub_;
    };
    class async* async() override { return &async_stub_; }

   private:
    std::shared_ptr< ::grpc::ChannelInterface> channel_;
    class async async_stub_{this};
    ::grpc::ClientWriter< ::CCcloud::UploadChunk>* UploadRaw(::grpc::ClientContext* context, ::CCcloud::UploadResponse* response) override;
    ::grpc::ClientAsyncWriter< ::CCcloud::UploadChunk>* AsyncUploadRaw(::grpc::ClientContext* context, ::CCcloud::UploadResponse* response, ::grpc::CompletionQueue* cq, void* tag) override;
    ::grpc::ClientAsyncWriter< ::CCcloud::UploadChunk>* PrepareAsyncUploadRaw(::grpc::ClientContext* context, ::CCcloud::UploadResponse* response, ::grpc::CompletionQueue* cq) override;
    ::grpc::ClientReader< ::CCcloud::DownloadChunk>* DownloadRaw(::grpc::ClientContext* context, const ::CCcloud::DownloadRequest& request) override;
    ::grpc::ClientAsyncReader< ::CCcloud::DownloadChunk>* AsyncDownloadRaw(::grpc::ClientContext* context, const ::CCcloud::DownloadRequest& request, ::grpc::CompletionQueue* cq, void* tag) override;
    ::grpc::ClientAsyncReader< ::CCcloud::DownloadChunk>* PrepareAsyncDownloadRaw(::grpc::ClientContext* context, const ::CCcloud::DownloadRequest& request, ::grpc::CompletionQueue* cq) override;
    ::grpc::ClientAsyncResponseReader< ::CCcloud::DeleteResponse>* AsyncDeleteRaw(::grpc::ClientContext* context, const ::CCcloud::DeleteRequest& request, ::grpc::CompletionQueue* cq) override;
    ::grpc::ClientAsyncResponseReader< ::CCcloud::DeleteResponse>* PrepareAsyncDeleteRaw(::grpc::ClientContext* context, const ::CCcloud::DeleteRequest& request, ::grpc::CompletionQueue* cq) override;
    const ::grpc::internal::RpcMethod rpcmethod_Upload_;
    const ::grpc::internal::RpcMethod rpcmethod_Download_;
    const ::grpc::internal::RpcMethod rpcmethod_Delete_;
  };
  static std::unique_ptr<Stub> NewStub(const std::shared_ptr< ::grpc::ChannelInterface>& channel, const ::grpc::StubOptions& options = ::grpc::StubOptions());

  class Service : public ::grpc::Service {
   public:
    Service();
    virtual ~Service();
    virtual ::grpc::Status Upload(::grpc::ServerContext* context, ::grpc::ServerReader< ::CCcloud::UploadChunk>* reader, ::CCcloud::UploadResponse* response);
    virtual ::grpc::Status Download(::grpc::ServerContext* context, const ::CCcloud::DownloadRequest* request, ::grpc::ServerWriter< ::CCcloud::DownloadChunk>* writer);
    virtual ::grpc::Status Delete(::grpc::ServerContext* context, const ::CCcloud::DeleteRequest* request, ::CCcloud::DeleteResponse* response);
  };
  template <class BaseClass>
  class WithAsyncMethod_Upload : public BaseClass {
   private:
    void BaseClassMustBeDerivedFromService(const Service* /*service*/) {}
   public:
    WithAsyncMethod_Upload() {
      ::grpc::Service::MarkMethodAsync(0);
    }
    ~WithAsyncMethod_Upload() override {
      BaseClassMustBeDerivedFromService(this);
    }
    // disable synchronous version of this method
    ::grpc::Status Upload(::grpc::ServerContext* /*context*/, ::grpc::ServerReader< ::CCcloud::UploadChunk>* /*reader*/, ::CCcloud::UploadResponse* /*response*/) override {
      abort();
      return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
    }
    void RequestUpload(::grpc::ServerContext* context, ::grpc::ServerAsyncReader< ::CCcloud::UploadResponse, ::CCcloud::UploadChunk>* reader, ::grpc::CompletionQueue* new_call_cq, ::grpc::ServerCompletionQueue* notification_cq, void *tag) {
      ::grpc::Service::RequestAsyncClientStreaming(0, context, reader, new_call_cq, notification_cq, tag);
    }
  };
  template <class BaseClass>
  class WithAsyncMethod_Download : public BaseClass {
   private:
    void BaseClassMustBeDerivedFromService(const Service* /*service*/) {}
   public:
    WithAsyncMethod_Download() {
      ::grpc::Service::MarkMethodAsync(1);
    }
    ~WithAsyncMethod_Download() override {
      BaseClassMustBeDerivedFromService(this);
    }
    // disable synchronous version of this method
    ::grpc::Status Download(::grpc::ServerContext* /*context*/, const ::CCcloud::DownloadRequest* /*request*/, ::grpc::ServerWriter< ::CCcloud::DownloadChunk>* /*writer*/) override {
      abort();
      return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
    }
    void RequestDownload(::grpc::ServerContext* context, ::CCcloud::DownloadRequest* request, ::grpc::ServerAsyncWriter< ::CCcloud::DownloadChunk>* writer, ::grpc::CompletionQueue* new_call_cq, ::grpc::ServerCompletionQueue* notification_cq, void *tag) {
      ::grpc::Service::RequestAsyncServerStreaming(1, context, request, writer, new_call_cq, notification_cq, tag);
    }
  };
  template <class BaseClass>
  class WithAsyncMethod_Delete : public BaseClass {
   private:
    void BaseClassMustBeDerivedFromService(const Service* /*service*/) {}
   public:
    WithAsyncMethod_Delete() {
      ::grpc::Service::MarkMethodAsync(2);
    }
    ~WithAsyncMethod_Delete() override {
      BaseClassMustBeDerivedFromService(this);
    }
    // disable synchronous version of this method
    ::grpc::Status Delete(::grpc::ServerContext* /*context*/, const ::CCcloud::DeleteRequest* /*request*/, ::CCcloud::DeleteResponse* /*response*/) override {
      abort();
      return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
    }
    void RequestDelete(::grpc::ServerContext* context, ::CCcloud::DeleteRequest* request, ::grpc::ServerAsyncResponseWriter< ::CCcloud::DeleteResponse>* response, ::grpc::CompletionQueue* new_call_cq, ::grpc::ServerCompletionQueue* notification_cq, void *tag) {
      ::grpc::Service::RequestAsyncUnary(2, context, request, response, new_call_cq, notification_cq, tag);
    }
  };
  typedef WithAsyncMethod_Upload<WithAsyncMethod_Download<WithAsyncMethod_Delete<Service > > > AsyncService;
  template <class BaseClass>
  class WithCallbackMethod_Upload : public BaseClass {
   private:
    void BaseClassMustBeDerivedFromService(const Service* /*service*/) {}
   public:
    WithCallbackMethod_Upload() {
      ::grpc::Service::MarkMethodCallback(0,
          new ::grpc::internal::CallbackClientStreamingHandler< ::CCcloud::UploadChunk, ::CCcloud::UploadResponse>(
            [this](
                   ::grpc::CallbackServerContext* context, ::CCcloud::UploadResponse* response) { return this->Upload(context, response); }));
    }
    ~WithCallbackMethod_Upload() override {
      BaseClassMustBeDerivedFromService(this);
    }
    // disable synchronous version of this method
    ::grpc::Status Upload(::grpc::ServerContext* /*context*/, ::grpc::ServerReader< ::CCcloud::UploadChunk>* /*reader*/, ::CCcloud::UploadResponse* /*response*/) override {
      abort();
      return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
    }
    virtual ::grpc::ServerReadReactor< ::CCcloud::UploadChunk>* Upload(
      ::grpc::CallbackServerContext* /*context*/, ::CCcloud::UploadResponse* /*response*/)  { return nullptr; }
  };
  template <class BaseClass>
  class WithCallbackMethod_Download : public BaseClass {
   private:
    void BaseClassMustBeDerivedFromService(const Service* /*service*/) {}
   public:
    WithCallbackMethod_Download() {
      ::grpc::Service::MarkMethodCallback(1,
          new ::grpc::internal::CallbackServerStreamingHandler< ::CCcloud::DownloadRequest, ::CCcloud::DownloadChunk>(
            [this](
                   ::grpc::CallbackServerContext* context, const ::CCcloud::DownloadRequest* request) { return this->Download(context, request); }));
    }
    ~WithCallbackMethod_Download() override {
      BaseClassMustBeDerivedFromService(this);
    }
    // disable synchronous version of this method
    ::grpc::Status Download(::grpc::ServerContext* /*context*/, const ::CCcloud::DownloadRequest* /*request*/, ::grpc::ServerWriter< ::CCcloud::DownloadChunk>* /*writer*/) override {
      abort();
      return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
    }
    virtual ::grpc::ServerWriteReactor< ::CCcloud::DownloadChunk>* Download(
      ::grpc::CallbackServerContext* /*context*/, const ::CCcloud::DownloadRequest* /*request*/)  { return nullptr; }
  };
  template <class BaseClass>
  class WithCallbackMethod_Delete : public BaseClass {
   private:
    void BaseClassMustBeDerivedFromService(const Service* /*service*/) {}
   public:
    WithCallbackMethod_Delete() {
      ::grpc::Service::MarkMethodCallback(2,
          new ::grpc::internal::CallbackUnaryHandler< ::CCcloud::DeleteRequest, ::CCcloud::DeleteResponse>(
            [this](
                   ::grpc::CallbackServerContext* context, const ::CCcloud::DeleteRequest* request, ::CCcloud::DeleteResponse* response) { return this->Delete(context, request, response); }));}
    void SetMessageAllocatorFor_Delete(
        ::grpc::MessageAllocator< ::CCcloud::DeleteRequest, ::CCcloud::DeleteResponse>* allocator) {
      ::grpc::internal::MethodHandler* const handler = ::grpc::Service::GetHandler(2);
      static_cast<::grpc::internal::CallbackUnaryHandler< ::CCcloud::DeleteRequest, ::CCcloud::DeleteResponse>*>(handler)
              ->SetMessageAllocator(allocator);
    }
    ~WithCallbackMethod_Delete() override {
      BaseClassMustBeDerivedFromService(this);
    }
    // disable synchronous version of this method
    ::grpc::Status Delete(::grpc::ServerContext* /*context*/, const ::CCcloud::DeleteRequest* /*request*/, ::CCcloud::DeleteResponse* /*response*/) override {
      abort();
      return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
    }
    virtual ::grpc::ServerUnaryReactor* Delete(
      ::grpc::CallbackServerContext* /*context*/, const ::CCcloud::DeleteRequest* /*request*/, ::CCcloud::DeleteResponse* /*response*/)  { return nullptr; }
  };
  typedef WithCallbackMethod_Upload<WithCallbackMethod_Download<WithCallbackMethod_Delete<Service > > > CallbackService;
  typedef CallbackService ExperimentalCallbackService;
  template <class BaseClass>
  class WithGenericMethod_Upload : public BaseClass {
   private:
    void BaseClassMustBeDerivedFromService(const Service* /*service*/) {}
   public:
    WithGenericMethod_Upload() {
      ::grpc::Service::MarkMethodGeneric(0);
    }
    ~WithGenericMethod_Upload() override {
      BaseClassMustBeDerivedFromService(this);
    }
    // disable synchronous version of this method
    ::grpc::Status Upload(::grpc::ServerContext* /*context*/, ::grpc::ServerReader< ::CCcloud::UploadChunk>* /*reader*/, ::CCcloud::UploadResponse* /*response*/) override {
      abort();
      return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
    }
  };
  template <class BaseClass>
  class WithGenericMethod_Download : public BaseClass {
   private:
    void BaseClassMustBeDerivedFromService(const Service* /*service*/) {}
   public:
    WithGenericMethod_Download() {
      ::grpc::Service::MarkMethodGeneric(1);
    }
    ~WithGenericMethod_Download() override {
      BaseClassMustBeDerivedFromService(this);
    }
    // disable synchronous version of this method
    ::grpc::Status Download(::grpc::ServerContext* /*context*/, const ::CCcloud::DownloadRequest* /*request*/, ::grpc::ServerWriter< ::CCcloud::DownloadChunk>* /*writer*/) override {
      abort();
      return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
    }
  };
  template <class BaseClass>
  class WithGenericMethod_Delete : public BaseClass {
   private:
    void BaseClassMustBeDerivedFromService(const Service* /*service*/) {}
   public:
    WithGenericMethod_Delete() {
      ::grpc::Service::MarkMethodGeneric(2);
    }
    ~WithGenericMethod_Delete() override {
      BaseClassMustBeDerivedFromService(this);
    }
    // disable synchronous version of this method
    ::grpc::Status Delete(::grpc::ServerContext* /*context*/, const ::CCcloud::DeleteRequest* /*request*/, ::CCcloud::DeleteResponse* /*response*/) override {
      abort();
      return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
    }
  };
  template <class BaseClass>
  class WithRawMethod_Upload : public BaseClass {
   private:
    void BaseClassMustBeDerivedFromService(const Service* /*service*/) {}
   public:
    WithRawMethod_Upload() {
      ::grpc::Service::MarkMethodRaw(0);
    }
    ~WithRawMethod_Upload() override {
      BaseClassMustBeDerivedFromService(this);
    }
    // disable synchronous version of this method
    ::grpc::Status Upload(::grpc::ServerContext* /*context*/, ::grpc::ServerReader< ::CCcloud::UploadChunk>* /*reader*/, ::CCcloud::UploadResponse* /*response*/) override {
      abort();
      return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
    }
    void RequestUpload(::grpc::ServerContext* context, ::grpc::ServerAsyncReader< ::grpc::ByteBuffer, ::grpc::ByteBuffer>* reader, ::grpc::CompletionQueue* new_call_cq, ::grpc::ServerCompletionQueue* notification_cq, void *tag) {
      ::grpc::Service::RequestAsyncClientStreaming(0, context, reader, new_call_cq, notification_cq, tag);
    }
  };
  template <class BaseClass>
  class WithRawMethod_Download : public BaseClass {
   private:
    void BaseClassMustBeDerivedFromService(const Service* /*service*/) {}
   public:
    WithRawMethod_Download() {
      ::grpc::Service::MarkMethodRaw(1);
    }
    ~WithRawMethod_Download() override {
      BaseClassMustBeDerivedFromService(this);
    }
    // disable synchronous version of this method
    ::grpc::Status Download(::grpc::ServerContext* /*context*/, const ::CCcloud::DownloadRequest* /*request*/, ::grpc::ServerWriter< ::CCcloud::DownloadChunk>* /*writer*/) override {
      abort();
      return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
    }
    void RequestDownload(::grpc::ServerContext* context, ::grpc::ByteBuffer* request, ::grpc::ServerAsyncWriter< ::grpc::ByteBuffer>* writer, ::grpc::CompletionQueue* new_call_cq, ::grpc::ServerCompletionQueue* notification_cq, void *tag) {
      ::grpc::Service::RequestAsyncServerStreaming(1, context, request, writer, new_call_cq, notification_cq, tag);
    }
  };
  template <class BaseClass>
  class WithRawMethod_Delete : public BaseClass {
   private:
    void BaseClassMustBeDerivedFromService(const Service* /*service*/) {}
   public:
    WithRawMethod_Delete() {
      ::grpc::Service::MarkMethodRaw(2);
    }
    ~WithRawMethod_Delete() override {
      BaseClassMustBeDerivedFromService(this);
    }
    // disable synchronous version of this method
    ::grpc::Status Delete(::grpc::ServerContext* /*context*/, const ::CCcloud::DeleteRequest* /*request*/, ::CCcloud::DeleteResponse* /*response*/) override {
      abort();
      return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
    }
    void RequestDelete(::grpc::ServerContext* context, ::grpc::ByteBuffer* request, ::grpc::ServerAsyncResponseWriter< ::grpc::ByteBuffer>* response, ::grpc::CompletionQueue* new_call_cq, ::grpc::ServerCompletionQueue* notification_cq, void *tag) {
      ::grpc::Service::RequestAsyncUnary(2, context, request, response, new_call_cq, notification_cq, tag);
    }
  };
  template <class BaseClass>
  class WithRawCallbackMethod_Upload : public BaseClass {
   private:
    void BaseClassMustBeDerivedFromService(const Service* /*service*/) {}
   public:
    WithRawCallbackMethod_Upload() {
      ::grpc::Service::MarkMethodRawCallback(0,
          new ::grpc::internal::CallbackClientStreamingHandler< ::grpc::ByteBuffer, ::grpc::ByteBuffer>(
            [this](
                   ::grpc::CallbackServerContext* context, ::grpc::ByteBuffer* response) { return this->Upload(context, response); }));
    }
    ~WithRawCallbackMethod_Upload() override {
      BaseClassMustBeDerivedFromService(this);
    }
    // disable synchronous version of this method
    ::grpc::Status Upload(::grpc::ServerContext* /*context*/, ::grpc::ServerReader< ::CCcloud::UploadChunk>* /*reader*/, ::CCcloud::UploadResponse* /*response*/) override {
      abort();
      return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
    }
    virtual ::grpc::ServerReadReactor< ::grpc::ByteBuffer>* Upload(
      ::grpc::CallbackServerContext* /*context*/, ::grpc::ByteBuffer* /*response*/)  { return nullptr; }
  };
  template <class BaseClass>
  class WithRawCallbackMethod_Download : public BaseClass {
   private:
    void BaseClassMustBeDerivedFromService(const Service* /*service*/) {}
   public:
    WithRawCallbackMethod_Download() {
      ::grpc::Service::MarkMethodRawCallback(1,
          new ::grpc::internal::CallbackServerStreamingHandler< ::grpc::ByteBuffer, ::grpc::ByteBuffer>(
            [this](
                   ::grpc::CallbackServerContext* context, const::grpc::ByteBuffer* request) { return this->Download(context, request); }));
    }
    ~WithRawCallbackMethod_Download() override {
      BaseClassMustBeDerivedFromService(this);
    }
    // disable synchronous version of this method
    ::grpc::Status Download(::grpc::ServerContext* /*context*/, const ::CCcloud::DownloadRequest* /*request*/, ::grpc::ServerWriter< ::CCcloud::DownloadChunk>* /*writer*/) override {
      abort();
      return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
    }
    virtual ::grpc::ServerWriteReactor< ::grpc::ByteBuffer>* Download(
      ::grpc::CallbackServerContext* /*context*/, const ::grpc::ByteBuffer* /*request*/)  { return nullptr; }
  };
  template <class BaseClass>
  class WithRawCallbackMethod_Delete : public BaseClass {
   private:
    void BaseClassMustBeDerivedFromService(const Service* /*service*/) {}
   public:
    WithRawCallbackMethod_Delete() {
      ::grpc::Service::MarkMethodRawCallback(2,
          new ::grpc::internal::CallbackUnaryHandler< ::grpc::ByteBuffer, ::grpc::ByteBuffer>(
            [this](
                   ::grpc::CallbackServerContext* context, const ::grpc::ByteBuffer* request, ::grpc::ByteBuffer* response) { return this->Delete(context, request, response); }));
    }
    ~WithRawCallbackMethod_Delete() override {
      BaseClassMustBeDerivedFromService(this);
    }
    // disable synchronous version of this method
    ::grpc::Status Delete(::grpc::ServerContext* /*context*/, const ::CCcloud::DeleteRequest* /*request*/, ::CCcloud::DeleteResponse* /*response*/) override {
      abort();
      return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
    }
    virtual ::grpc::ServerUnaryReactor* Delete(
      ::grpc::CallbackServerContext* /*context*/, const ::grpc::ByteBuffer* /*request*/, ::grpc::ByteBuffer* /*response*/)  { return nullptr; }
  };
  template <class BaseClass>
  class WithStreamedUnaryMethod_Delete : public BaseClass {
   private:
    void BaseClassMustBeDerivedFromService(const Service* /*service*/) {}
   public:
    WithStreamedUnaryMethod_Delete() {
      ::grpc::Service::MarkMethodStreamed(2,
        new ::grpc::internal::StreamedUnaryHandler<
          ::CCcloud::DeleteRequest, ::CCcloud::DeleteResponse>(
            [this](::grpc::ServerContext* context,
                   ::grpc::ServerUnaryStreamer<
                     ::CCcloud::DeleteRequest, ::CCcloud::DeleteResponse>* streamer) {
                       return this->StreamedDelete(context,
                         streamer);
                  }));
    }
    ~WithStreamedUnaryMethod_Delete() override {
      BaseClassMustBeDerivedFromService(this);
    }
    // disable regular version of this method
    ::grpc::Status Delete(::grpc::ServerContext* /*context*/, const ::CCcloud::DeleteRequest* /*request*/, ::CCcloud::DeleteResponse* /*response*/) override {
      abort();
      return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
    }
    // replace default version of method with streamed unary
    virtual ::grpc::Status StreamedDelete(::grpc::ServerContext* context, ::grpc::ServerUnaryStreamer< ::CCcloud::DeleteRequest,::CCcloud::DeleteResponse>* server_unary_streamer) = 0;
  };
  typedef WithStreamedUnaryMethod_Delete<Service > StreamedUnaryService;
  template <class BaseClass>
  class WithSplitStreamingMethod_Download : public BaseClass {
   private:
    void BaseClassMustBeDerivedFromService(const Service* /*service*/) {}
   public:
    WithSplitStreamingMethod_Download() {
      ::grpc::Service::MarkMethodStreamed(1,
        new ::grpc::internal::SplitServerStreamingHandler<
          ::CCcloud::DownloadRequest, ::CCcloud::DownloadChunk>(
            [this](::grpc::ServerContext* context,
                   ::grpc::ServerSplitStreamer<
                     ::CCcloud::DownloadRequest, ::CCcloud::DownloadChunk>* streamer) {
                       return this->StreamedDownload(context,
                         streamer);
                  }));
    }
    ~WithSplitStreamingMethod_Download() override {
      BaseClassMustBeDerivedFromService(this);
    }
    // disable regular version of this method
    ::grpc::Status Download(::grpc::ServerContext* /*context*/, const ::CCcloud::DownloadRequest* /*request*/, ::grpc::ServerWriter< ::CCcloud::DownloadChunk>* /*writer*/) override {
      abort();
      return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
    }
    // replace default version of method with split streamed
    virtual ::grpc::Status StreamedDownload(::grpc::ServerContext* context, ::grpc::ServerSplitStreamer< ::CCcloud::DownloadRequest,::CCcloud::DownloadChunk>* server_split_streamer) = 0;
  };
  typedef WithSplitStreamingMethod_Download<Service > SplitStreamedService;
  typedef WithSplitStreamingMethod_Download<WithStreamedUnaryMethod_Delete<Service > > StreamedService;
};

}  // namespace CCcloud


#include <grpcpp/ports_undef.inc>
#endif  // GRPC_file_2eproto__INCLUDED
