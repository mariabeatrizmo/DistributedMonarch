/*#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>
#include <grpcpp/impl/grpc_library.h>

#include "hierarchical_data_plane.h"

#ifdef BAZEL_BUILD
#include "protos/info_exchange_service.grpc.pb.h"
#elif TF_BAZEL_BUILD
#include "tensorflow/core/platform/pastor/protos/info_exchange_service.grpc.pb.h"
#else
#include "info_exchange_service.grpc.pb.h"
#endif


using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

using exchangeservice::Empty;
using exchangeservice::Exchanger;
using exchangeservice::ReadRequest;
using exchangeservice::ReadResponse;
using exchangeservice::WriteAnnounce;

namespace aux{
void run();
void RunServer(HierarchicalDataPlane* hdp);
}
*/