// Generated by the gRPC C++ plugin.
// If you make any local change, they will be lost.
// source: agent_com.proto

#include "agent_com.pb.h"
#include "agent_com.grpc.pb.h"

#include <functional>
#include <grpcpp/impl/codegen/async_stream.h>
#include <grpcpp/impl/codegen/async_unary_call.h>
#include <grpcpp/impl/codegen/channel_interface.h>
#include <grpcpp/impl/codegen/client_unary_call.h>
#include <grpcpp/impl/codegen/client_callback.h>
#include <grpcpp/impl/codegen/method_handler_impl.h>
#include <grpcpp/impl/codegen/rpc_service_method.h>
#include <grpcpp/impl/codegen/server_callback.h>
#include <grpcpp/impl/codegen/service_type.h>
#include <grpcpp/impl/codegen/sync_stream.h>
namespace bbque {

static const char* RemoteAgent_method_names[] = {
  "/bbque.RemoteAgent/Discover",
  "/bbque.RemoteAgent/Ping",
  "/bbque.RemoteAgent/GetResourceStatus",
  "/bbque.RemoteAgent/GetWorkloadStatus",
  "/bbque.RemoteAgent/GetChannelStatus",
  "/bbque.RemoteAgent/SetNodeManagementAction",
};

std::unique_ptr< RemoteAgent::Stub> RemoteAgent::NewStub(const std::shared_ptr< ::grpc::ChannelInterface>& channel, const ::grpc::StubOptions& options) {
  (void)options;
  std::unique_ptr< RemoteAgent::Stub> stub(new RemoteAgent::Stub(channel));
  return stub;
}

RemoteAgent::Stub::Stub(const std::shared_ptr< ::grpc::ChannelInterface>& channel)
  : channel_(channel), rpcmethod_Discover_(RemoteAgent_method_names[0], ::grpc::internal::RpcMethod::NORMAL_RPC, channel)
  , rpcmethod_Ping_(RemoteAgent_method_names[1], ::grpc::internal::RpcMethod::NORMAL_RPC, channel)
  , rpcmethod_GetResourceStatus_(RemoteAgent_method_names[2], ::grpc::internal::RpcMethod::NORMAL_RPC, channel)
  , rpcmethod_GetWorkloadStatus_(RemoteAgent_method_names[3], ::grpc::internal::RpcMethod::NORMAL_RPC, channel)
  , rpcmethod_GetChannelStatus_(RemoteAgent_method_names[4], ::grpc::internal::RpcMethod::NORMAL_RPC, channel)
  , rpcmethod_SetNodeManagementAction_(RemoteAgent_method_names[5], ::grpc::internal::RpcMethod::NORMAL_RPC, channel)
  {}

::grpc::Status RemoteAgent::Stub::Discover(::grpc::ClientContext* context, const ::bbque::GenericRequest& request, ::bbque::GenericReply* response) {
  return ::grpc::internal::BlockingUnaryCall(channel_.get(), rpcmethod_Discover_, context, request, response);
}

void RemoteAgent::Stub::experimental_async::Discover(::grpc::ClientContext* context, const ::bbque::GenericRequest* request, ::bbque::GenericReply* response, std::function<void(::grpc::Status)> f) {
  return ::grpc::internal::CallbackUnaryCall(stub_->channel_.get(), stub_->rpcmethod_Discover_, context, request, response, std::move(f));
}

::grpc::ClientAsyncResponseReader< ::bbque::GenericReply>* RemoteAgent::Stub::AsyncDiscoverRaw(::grpc::ClientContext* context, const ::bbque::GenericRequest& request, ::grpc::CompletionQueue* cq) {
  return ::grpc::internal::ClientAsyncResponseReaderFactory< ::bbque::GenericReply>::Create(channel_.get(), cq, rpcmethod_Discover_, context, request, true);
}

::grpc::ClientAsyncResponseReader< ::bbque::GenericReply>* RemoteAgent::Stub::PrepareAsyncDiscoverRaw(::grpc::ClientContext* context, const ::bbque::GenericRequest& request, ::grpc::CompletionQueue* cq) {
  return ::grpc::internal::ClientAsyncResponseReaderFactory< ::bbque::GenericReply>::Create(channel_.get(), cq, rpcmethod_Discover_, context, request, false);
}

::grpc::Status RemoteAgent::Stub::Ping(::grpc::ClientContext* context, const ::bbque::GenericRequest& request, ::bbque::GenericReply* response) {
  return ::grpc::internal::BlockingUnaryCall(channel_.get(), rpcmethod_Ping_, context, request, response);
}

void RemoteAgent::Stub::experimental_async::Ping(::grpc::ClientContext* context, const ::bbque::GenericRequest* request, ::bbque::GenericReply* response, std::function<void(::grpc::Status)> f) {
  return ::grpc::internal::CallbackUnaryCall(stub_->channel_.get(), stub_->rpcmethod_Ping_, context, request, response, std::move(f));
}

::grpc::ClientAsyncResponseReader< ::bbque::GenericReply>* RemoteAgent::Stub::AsyncPingRaw(::grpc::ClientContext* context, const ::bbque::GenericRequest& request, ::grpc::CompletionQueue* cq) {
  return ::grpc::internal::ClientAsyncResponseReaderFactory< ::bbque::GenericReply>::Create(channel_.get(), cq, rpcmethod_Ping_, context, request, true);
}

::grpc::ClientAsyncResponseReader< ::bbque::GenericReply>* RemoteAgent::Stub::PrepareAsyncPingRaw(::grpc::ClientContext* context, const ::bbque::GenericRequest& request, ::grpc::CompletionQueue* cq) {
  return ::grpc::internal::ClientAsyncResponseReaderFactory< ::bbque::GenericReply>::Create(channel_.get(), cq, rpcmethod_Ping_, context, request, false);
}

::grpc::Status RemoteAgent::Stub::GetResourceStatus(::grpc::ClientContext* context, const ::bbque::ResourceStatusRequest& request, ::bbque::ResourceStatusReply* response) {
  return ::grpc::internal::BlockingUnaryCall(channel_.get(), rpcmethod_GetResourceStatus_, context, request, response);
}

void RemoteAgent::Stub::experimental_async::GetResourceStatus(::grpc::ClientContext* context, const ::bbque::ResourceStatusRequest* request, ::bbque::ResourceStatusReply* response, std::function<void(::grpc::Status)> f) {
  return ::grpc::internal::CallbackUnaryCall(stub_->channel_.get(), stub_->rpcmethod_GetResourceStatus_, context, request, response, std::move(f));
}

::grpc::ClientAsyncResponseReader< ::bbque::ResourceStatusReply>* RemoteAgent::Stub::AsyncGetResourceStatusRaw(::grpc::ClientContext* context, const ::bbque::ResourceStatusRequest& request, ::grpc::CompletionQueue* cq) {
  return ::grpc::internal::ClientAsyncResponseReaderFactory< ::bbque::ResourceStatusReply>::Create(channel_.get(), cq, rpcmethod_GetResourceStatus_, context, request, true);
}

::grpc::ClientAsyncResponseReader< ::bbque::ResourceStatusReply>* RemoteAgent::Stub::PrepareAsyncGetResourceStatusRaw(::grpc::ClientContext* context, const ::bbque::ResourceStatusRequest& request, ::grpc::CompletionQueue* cq) {
  return ::grpc::internal::ClientAsyncResponseReaderFactory< ::bbque::ResourceStatusReply>::Create(channel_.get(), cq, rpcmethod_GetResourceStatus_, context, request, false);
}

::grpc::Status RemoteAgent::Stub::GetWorkloadStatus(::grpc::ClientContext* context, const ::bbque::GenericRequest& request, ::bbque::WorkloadStatusReply* response) {
  return ::grpc::internal::BlockingUnaryCall(channel_.get(), rpcmethod_GetWorkloadStatus_, context, request, response);
}

void RemoteAgent::Stub::experimental_async::GetWorkloadStatus(::grpc::ClientContext* context, const ::bbque::GenericRequest* request, ::bbque::WorkloadStatusReply* response, std::function<void(::grpc::Status)> f) {
  return ::grpc::internal::CallbackUnaryCall(stub_->channel_.get(), stub_->rpcmethod_GetWorkloadStatus_, context, request, response, std::move(f));
}

::grpc::ClientAsyncResponseReader< ::bbque::WorkloadStatusReply>* RemoteAgent::Stub::AsyncGetWorkloadStatusRaw(::grpc::ClientContext* context, const ::bbque::GenericRequest& request, ::grpc::CompletionQueue* cq) {
  return ::grpc::internal::ClientAsyncResponseReaderFactory< ::bbque::WorkloadStatusReply>::Create(channel_.get(), cq, rpcmethod_GetWorkloadStatus_, context, request, true);
}

::grpc::ClientAsyncResponseReader< ::bbque::WorkloadStatusReply>* RemoteAgent::Stub::PrepareAsyncGetWorkloadStatusRaw(::grpc::ClientContext* context, const ::bbque::GenericRequest& request, ::grpc::CompletionQueue* cq) {
  return ::grpc::internal::ClientAsyncResponseReaderFactory< ::bbque::WorkloadStatusReply>::Create(channel_.get(), cq, rpcmethod_GetWorkloadStatus_, context, request, false);
}

::grpc::Status RemoteAgent::Stub::GetChannelStatus(::grpc::ClientContext* context, const ::bbque::GenericRequest& request, ::bbque::ChannelStatusReply* response) {
  return ::grpc::internal::BlockingUnaryCall(channel_.get(), rpcmethod_GetChannelStatus_, context, request, response);
}

void RemoteAgent::Stub::experimental_async::GetChannelStatus(::grpc::ClientContext* context, const ::bbque::GenericRequest* request, ::bbque::ChannelStatusReply* response, std::function<void(::grpc::Status)> f) {
  return ::grpc::internal::CallbackUnaryCall(stub_->channel_.get(), stub_->rpcmethod_GetChannelStatus_, context, request, response, std::move(f));
}

::grpc::ClientAsyncResponseReader< ::bbque::ChannelStatusReply>* RemoteAgent::Stub::AsyncGetChannelStatusRaw(::grpc::ClientContext* context, const ::bbque::GenericRequest& request, ::grpc::CompletionQueue* cq) {
  return ::grpc::internal::ClientAsyncResponseReaderFactory< ::bbque::ChannelStatusReply>::Create(channel_.get(), cq, rpcmethod_GetChannelStatus_, context, request, true);
}

::grpc::ClientAsyncResponseReader< ::bbque::ChannelStatusReply>* RemoteAgent::Stub::PrepareAsyncGetChannelStatusRaw(::grpc::ClientContext* context, const ::bbque::GenericRequest& request, ::grpc::CompletionQueue* cq) {
  return ::grpc::internal::ClientAsyncResponseReaderFactory< ::bbque::ChannelStatusReply>::Create(channel_.get(), cq, rpcmethod_GetChannelStatus_, context, request, false);
}

::grpc::Status RemoteAgent::Stub::SetNodeManagementAction(::grpc::ClientContext* context, const ::bbque::NodeManagementRequest& request, ::bbque::GenericReply* response) {
  return ::grpc::internal::BlockingUnaryCall(channel_.get(), rpcmethod_SetNodeManagementAction_, context, request, response);
}

void RemoteAgent::Stub::experimental_async::SetNodeManagementAction(::grpc::ClientContext* context, const ::bbque::NodeManagementRequest* request, ::bbque::GenericReply* response, std::function<void(::grpc::Status)> f) {
  return ::grpc::internal::CallbackUnaryCall(stub_->channel_.get(), stub_->rpcmethod_SetNodeManagementAction_, context, request, response, std::move(f));
}

::grpc::ClientAsyncResponseReader< ::bbque::GenericReply>* RemoteAgent::Stub::AsyncSetNodeManagementActionRaw(::grpc::ClientContext* context, const ::bbque::NodeManagementRequest& request, ::grpc::CompletionQueue* cq) {
  return ::grpc::internal::ClientAsyncResponseReaderFactory< ::bbque::GenericReply>::Create(channel_.get(), cq, rpcmethod_SetNodeManagementAction_, context, request, true);
}

::grpc::ClientAsyncResponseReader< ::bbque::GenericReply>* RemoteAgent::Stub::PrepareAsyncSetNodeManagementActionRaw(::grpc::ClientContext* context, const ::bbque::NodeManagementRequest& request, ::grpc::CompletionQueue* cq) {
  return ::grpc::internal::ClientAsyncResponseReaderFactory< ::bbque::GenericReply>::Create(channel_.get(), cq, rpcmethod_SetNodeManagementAction_, context, request, false);
}

RemoteAgent::Service::Service() {
  AddMethod(new ::grpc::internal::RpcServiceMethod(
      RemoteAgent_method_names[0],
      ::grpc::internal::RpcMethod::NORMAL_RPC,
      new ::grpc::internal::RpcMethodHandler< RemoteAgent::Service, ::bbque::GenericRequest, ::bbque::GenericReply>(
          std::mem_fn(&RemoteAgent::Service::Discover), this)));
  AddMethod(new ::grpc::internal::RpcServiceMethod(
      RemoteAgent_method_names[1],
      ::grpc::internal::RpcMethod::NORMAL_RPC,
      new ::grpc::internal::RpcMethodHandler< RemoteAgent::Service, ::bbque::GenericRequest, ::bbque::GenericReply>(
          std::mem_fn(&RemoteAgent::Service::Ping), this)));
  AddMethod(new ::grpc::internal::RpcServiceMethod(
      RemoteAgent_method_names[2],
      ::grpc::internal::RpcMethod::NORMAL_RPC,
      new ::grpc::internal::RpcMethodHandler< RemoteAgent::Service, ::bbque::ResourceStatusRequest, ::bbque::ResourceStatusReply>(
          std::mem_fn(&RemoteAgent::Service::GetResourceStatus), this)));
  AddMethod(new ::grpc::internal::RpcServiceMethod(
      RemoteAgent_method_names[3],
      ::grpc::internal::RpcMethod::NORMAL_RPC,
      new ::grpc::internal::RpcMethodHandler< RemoteAgent::Service, ::bbque::GenericRequest, ::bbque::WorkloadStatusReply>(
          std::mem_fn(&RemoteAgent::Service::GetWorkloadStatus), this)));
  AddMethod(new ::grpc::internal::RpcServiceMethod(
      RemoteAgent_method_names[4],
      ::grpc::internal::RpcMethod::NORMAL_RPC,
      new ::grpc::internal::RpcMethodHandler< RemoteAgent::Service, ::bbque::GenericRequest, ::bbque::ChannelStatusReply>(
          std::mem_fn(&RemoteAgent::Service::GetChannelStatus), this)));
  AddMethod(new ::grpc::internal::RpcServiceMethod(
      RemoteAgent_method_names[5],
      ::grpc::internal::RpcMethod::NORMAL_RPC,
      new ::grpc::internal::RpcMethodHandler< RemoteAgent::Service, ::bbque::NodeManagementRequest, ::bbque::GenericReply>(
          std::mem_fn(&RemoteAgent::Service::SetNodeManagementAction), this)));
}

RemoteAgent::Service::~Service() {
}

::grpc::Status RemoteAgent::Service::Discover(::grpc::ServerContext* context, const ::bbque::GenericRequest* request, ::bbque::GenericReply* response) {
  (void) context;
  (void) request;
  (void) response;
  return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
}

::grpc::Status RemoteAgent::Service::Ping(::grpc::ServerContext* context, const ::bbque::GenericRequest* request, ::bbque::GenericReply* response) {
  (void) context;
  (void) request;
  (void) response;
  return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
}

::grpc::Status RemoteAgent::Service::GetResourceStatus(::grpc::ServerContext* context, const ::bbque::ResourceStatusRequest* request, ::bbque::ResourceStatusReply* response) {
  (void) context;
  (void) request;
  (void) response;
  return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
}

::grpc::Status RemoteAgent::Service::GetWorkloadStatus(::grpc::ServerContext* context, const ::bbque::GenericRequest* request, ::bbque::WorkloadStatusReply* response) {
  (void) context;
  (void) request;
  (void) response;
  return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
}

::grpc::Status RemoteAgent::Service::GetChannelStatus(::grpc::ServerContext* context, const ::bbque::GenericRequest* request, ::bbque::ChannelStatusReply* response) {
  (void) context;
  (void) request;
  (void) response;
  return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
}

::grpc::Status RemoteAgent::Service::SetNodeManagementAction(::grpc::ServerContext* context, const ::bbque::NodeManagementRequest* request, ::bbque::GenericReply* response) {
  (void) context;
  (void) request;
  (void) response;
  return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
}


}  // namespace bbque
