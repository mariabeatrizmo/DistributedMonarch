//#include "server.hpp"


//HierarchicalDataPlane* _data_plane;


//class ExchangerImpl final : public Exchanger::Service {
//    grpc::Status Request(ServerContext* context, const ReadRequest* request, ReadResponse* response) override {
//        /*char *buf;
//        int size = (*_data_plane).base_read_origin(request->file_name(), buf, request->size(), request->offset());
//        response->set_content(buf);
//        response->set_size(size);*/
//        std::cout << "Server received Request" << std::endl <<std::flush;
//        //std::cout << "3) _data_plane->self_ip: " << _data_plane->self_ip  << std::endl <<std::flush;
//        return grpc::Status::OK;
//    }

//    grpc::Status Announce(ServerContext* context, const WriteAnnounce* request, Empty* response) override {
//        /*if ((*_data_plane).worker_files_map.find(request->worker()) == (*_data_plane).worker_files_map.end())
//            (*_data_plane).worker_files_map.insert({request->worker(), std::vector<std::string>()});
//
//        (*_data_plane).worker_files_map[request->worker()].push_back(request->file());*/
//        std::cout << "Server received Announce" << std::endl <<std::flush;
//
//        return grpc::Status::OK;
//    }

//};


/*void aux::run(){
  std::string server_address( "0.0.0.0:50051");
  ExchangerImpl service;
  static grpc::internal::GrpcLibraryInitializer g_gli_initializer;

  grpc::EnableDefaultHealthCheckService(true);
  grpc::reflection::InitProtoReflectionServerBuilderPlugin();
  ServerBuilder builder;

  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);

  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address  << std::endl;

  server->Wait();
}


void aux::RunServer(HierarchicalDataPlane* hdp) {
  _data_plane = hdp;
  std::thread t1(run);
  t1.detach();
}*/
