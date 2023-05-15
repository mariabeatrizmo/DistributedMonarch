// Generated by the gRPC C++ plugin.
// If you make any local change, they will be lost.
// source: info_exchange_service.proto

#include "info_exchange_service.pb.h"
#include "info_exchange_service.grpc.pb.h"

#include <functional>
#include <grpcpp/support/async_stream.h>
#include <grpcpp/support/async_unary_call.h>
#include <grpcpp/impl/channel_interface.h>
#include <grpcpp/impl/client_unary_call.h>
#include <grpcpp/support/client_callback.h>
#include <grpcpp/support/message_allocator.h>
#include <grpcpp/support/method_handler.h>
#include <grpcpp/impl/rpc_service_method.h>
#include <grpcpp/support/server_callback.h>
#include <grpcpp/impl/server_callback_handlers.h>
#include <grpcpp/server_context.h>
#include <grpcpp/impl/service_type.h>
#include <grpcpp/support/sync_stream.h>
namespace exchangeservice {

static const char* Exchanger_method_names[] = {
  "/exchangeservice.Exchanger/Request",
  "/exchangeservice.Exchanger/Announce",
};

std::unique_ptr< Exchanger::Stub> Exchanger::NewStub(const std::shared_ptr< ::grpc::ChannelInterface>& channel, const ::grpc::StubOptions& options) {
  (void)options;
  std::unique_ptr< Exchanger::Stub> stub(new Exchanger::Stub(channel, options));
  return stub;
}

Exchanger::Stub::Stub(const std::shared_ptr< ::grpc::ChannelInterface>& channel, const ::grpc::StubOptions& options)
  : channel_(channel), rpcmethod_Request_(Exchanger_method_names[0], options.suffix_for_stats(),::grpc::internal::RpcMethod::NORMAL_RPC, channel)
  , rpcmethod_Announce_(Exchanger_method_names[1], options.suffix_for_stats(),::grpc::internal::RpcMethod::NORMAL_RPC, channel)
  {}

::grpc::Status Exchanger::Stub::Request(::grpc::ClientContext* context, const ::exchangeservice::ReadRequest& request, ::exchangeservice::ReadResponse* response) {
  return ::grpc::internal::BlockingUnaryCall< ::exchangeservice::ReadRequest, ::exchangeservice::ReadResponse, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(channel_.get(), rpcmethod_Request_, context, request, response);
}

void Exchanger::Stub::async::Request(::grpc::ClientContext* context, const ::exchangeservice::ReadRequest* request, ::exchangeservice::ReadResponse* response, std::function<void(::grpc::Status)> f) {
  ::grpc::internal::CallbackUnaryCall< ::exchangeservice::ReadRequest, ::exchangeservice::ReadResponse, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(stub_->channel_.get(), stub_->rpcmethod_Request_, context, request, response, std::move(f));
}

void Exchanger::Stub::async::Request(::grpc::ClientContext* context, const ::exchangeservice::ReadRequest* request, ::exchangeservice::ReadResponse* response, ::grpc::ClientUnaryReactor* reactor) {
  ::grpc::internal::ClientCallbackUnaryFactory::Create< ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(stub_->channel_.get(), stub_->rpcmethod_Request_, context, request, response, reactor);
}

::grpc::ClientAsyncResponseReader< ::exchangeservice::ReadResponse>* Exchanger::Stub::PrepareAsyncRequestRaw(::grpc::ClientContext* context, const ::exchangeservice::ReadRequest& request, ::grpc::CompletionQueue* cq) {
  return ::grpc::internal::ClientAsyncResponseReaderHelper::Create< ::exchangeservice::ReadResponse, ::exchangeservice::ReadRequest, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(channel_.get(), cq, rpcmethod_Request_, context, request);
}

::grpc::ClientAsyncResponseReader< ::exchangeservice::ReadResponse>* Exchanger::Stub::AsyncRequestRaw(::grpc::ClientContext* context, const ::exchangeservice::ReadRequest& request, ::grpc::CompletionQueue* cq) {
  auto* result =
    this->PrepareAsyncRequestRaw(context, request, cq);
  result->StartCall();
  return result;
}

::grpc::Status Exchanger::Stub::Announce(::grpc::ClientContext* context, const ::exchangeservice::WriteAnnounce& request, ::exchangeservice::Empty* response) {
  return ::grpc::internal::BlockingUnaryCall< ::exchangeservice::WriteAnnounce, ::exchangeservice::Empty, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(channel_.get(), rpcmethod_Announce_, context, request, response);
}

void Exchanger::Stub::async::Announce(::grpc::ClientContext* context, const ::exchangeservice::WriteAnnounce* request, ::exchangeservice::Empty* response, std::function<void(::grpc::Status)> f) {
  ::grpc::internal::CallbackUnaryCall< ::exchangeservice::WriteAnnounce, ::exchangeservice::Empty, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(stub_->channel_.get(), stub_->rpcmethod_Announce_, context, request, response, std::move(f));
}

void Exchanger::Stub::async::Announce(::grpc::ClientContext* context, const ::exchangeservice::WriteAnnounce* request, ::exchangeservice::Empty* response, ::grpc::ClientUnaryReactor* reactor) {
  ::grpc::internal::ClientCallbackUnaryFactory::Create< ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(stub_->channel_.get(), stub_->rpcmethod_Announce_, context, request, response, reactor);
}

::grpc::ClientAsyncResponseReader< ::exchangeservice::Empty>* Exchanger::Stub::PrepareAsyncAnnounceRaw(::grpc::ClientContext* context, const ::exchangeservice::WriteAnnounce& request, ::grpc::CompletionQueue* cq) {
  return ::grpc::internal::ClientAsyncResponseReaderHelper::Create< ::exchangeservice::Empty, ::exchangeservice::WriteAnnounce, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(channel_.get(), cq, rpcmethod_Announce_, context, request);
}

::grpc::ClientAsyncResponseReader< ::exchangeservice::Empty>* Exchanger::Stub::AsyncAnnounceRaw(::grpc::ClientContext* context, const ::exchangeservice::WriteAnnounce& request, ::grpc::CompletionQueue* cq) {
  auto* result =
    this->PrepareAsyncAnnounceRaw(context, request, cq);
  result->StartCall();
  return result;
}

Exchanger::Service::Service() {
  AddMethod(new ::grpc::internal::RpcServiceMethod(
      Exchanger_method_names[0],
      ::grpc::internal::RpcMethod::NORMAL_RPC,
      new ::grpc::internal::RpcMethodHandler< Exchanger::Service, ::exchangeservice::ReadRequest, ::exchangeservice::ReadResponse, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(
          [](Exchanger::Service* service,
             ::grpc::ServerContext* ctx,
             const ::exchangeservice::ReadRequest* req,
             ::exchangeservice::ReadResponse* resp) {
               return service->Request(ctx, req, resp);
             }, this)));
  AddMethod(new ::grpc::internal::RpcServiceMethod(
      Exchanger_method_names[1],
      ::grpc::internal::RpcMethod::NORMAL_RPC,
      new ::grpc::internal::RpcMethodHandler< Exchanger::Service, ::exchangeservice::WriteAnnounce, ::exchangeservice::Empty, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(
          [](Exchanger::Service* service,
             ::grpc::ServerContext* ctx,
             const ::exchangeservice::WriteAnnounce* req,
             ::exchangeservice::Empty* resp) {
               return service->Announce(ctx, req, resp);
             }, this)));
}

Exchanger::Service::~Service() {
}

::grpc::Status Exchanger::Service::Request(::grpc::ServerContext* context, const ::exchangeservice::ReadRequest* request, ::exchangeservice::ReadResponse* response) {
  (void) context;
  (void) request;
  (void) response;
  return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
}

::grpc::Status Exchanger::Service::Announce(::grpc::ServerContext* context, const ::exchangeservice::WriteAnnounce* request, ::exchangeservice::Empty* response) {
  (void) context;
  (void) request;
  (void) response;
  return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
}


}  // namespace exchangeservice
