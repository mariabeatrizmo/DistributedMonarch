
#include <grpc/grpc.h>
#include <grpc++/grpc++.h>
#include <grpcpp/grpcpp.h>

#include "server.h"
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
//using grpc::ClientAsyncResponseReader;
//using grpc::CompletionQueue;

using exchangeservice::Empty;
using exchangeservice::Exchanger;
using exchangeservice::ReadRequest;
using exchangeservice::ReadResponse;
using exchangeservice::WriteAnnounce;

HierarchicalDataPlane* _data_plane;

class ExchangerImpl final : public Exchanger::Service {
    grpc::Status Request(ServerContext* context, const ReadRequest* request, ReadResponse* response) override {
        char *buf; 
        int size = (*_data_plane).base_read_origin(request->file_name(), buf, request->size(), request->offset());
        response->set_content(buf);
        response->set_size(size);
        return grpc::Status::OK;
    }

    grpc::Status Announce(ServerContext* context, const WriteAnnounce* request, Empty* response) override {
        if ((*_data_plane).worker_files_map.find(request->worker()) == (*_data_plane).worker_files_map.end())
            (*_data_plane).worker_files_map.insert({request->worker(), std::vector<std::string>()});

        (*_data_plane).worker_files_map[request->worker()].push_back(request->file());

        return grpc::Status::OK;
    }
    
};

void RunServer(HierarchicalDataPlane* hdp) {
    _data_plane = hdp;
    std::string server_address{_data_plane->self_ip + ":2510"};
    ExchangerImpl service;

    // Build server
    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<Server> server{builder.BuildAndStart()};

    // Run server
    std::cout << "Server listening on " << server_address << std::endl;
    server->Wait();
}
