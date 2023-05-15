
#include <grpc/grpc.h>
#include <grpc++/grpc++.h>
#include <grpcpp/grpcpp.h>
//#include "../../../../grpc_out/info_exchange_service.grpc.pb.h"
//#include "../../../../proto/info_exchange_service.grpc.pb.h"

#ifdef BAZEL_BUILD
#include "protos/info_exchange_service.grpc.pb.h"
#elif TF_BAZEL_BUILD
#include "tensorflow/core/platform/pastor/protos/info_exchange_service.grpc.pb.h"
#else
#include "info_exchange_service.grpc.pb.h"
#endif


using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
//using grpc::ClientAsyncResponseReader;
//using grpc::CompletionQueue;

using exchangeservice::Empty;
using exchangeservice::Exchanger;
using exchangeservice::ReadRequest;
using exchangeservice::ReadResponse;
using exchangeservice::WriteAnnounce;


class Client{
    std::unique_ptr<Exchanger::Stub> _stub;

    public:
        Client(std::shared_ptr<Channel> channel) : _stub{Exchanger::NewStub(channel)} {}   
        Client(std::string server_address) : _stub{Exchanger::NewStub(grpc::CreateChannel(server_address, grpc::InsecureChannelCredentials()))} {}   

        //Client(const std::string& c_server_addr) : _stub(Exchanger::NewStub(grpc::CreateChannel(c_server_addr,grpc::InsecureChannelCredentials()))) {}

        int Request(const std::string& file_name, char* result, float offset, int size) {
            // Prepare request
            ReadRequest request;
            request.set_file_name(file_name);
            request.set_offset(offset);
            request.set_size(size);

            // Send request
            ReadResponse response;
            ClientContext context;
            grpc::Status status = _stub->Request(&context, request, &response);

            // Handle response
            if (status.ok()) {
                result = (char*) response.content().data();
                return response.size();
            } else {
                std::cerr << status.error_code() << ": " << status.error_message() << std::endl;
                std::cout << "RPC failed" <<std::endl;
                return -1;
            }
        }

        void Announce(const std::string& file_name, const std::string& worker) {
            // Prepare request
            WriteAnnounce request;
            request.set_worker(worker);
            request.set_file(file_name);

            // Send request
            Empty response;
            ClientContext context;
            grpc::Status status = _stub->Announce(&context, request, &response);

            // Handle response
            if (status.ok()) {
                return ;
            } else {
                std::cerr << status.error_code() << ": " << status.error_message() << std::endl;
                std::cout << "RPC failed" <<std::endl;
                return;
            }
        }
};
