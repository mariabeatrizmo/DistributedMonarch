//
// Created by dantas on 19/10/20.
//
//#include <libexplain/open.h>
#include <iostream>
#include <sstream>
#include <cstring>
#include <cmath>
#include <cstdlib>
#include <chrono>
#include <fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <mutex>
//#include <shared_mutex>
#include <boost/thread/locks.hpp>
#include <boost/thread/shared_mutex.hpp>
//#include <boost/version.hpp>

#include "absl/strings/strip.h"
#include "hierarchical_data_plane.h"
#include "hierarchical_data_plane_builder.h"
#include "../handlers/control_handler.h"

/*#if defined(INCLUDE_GRPC_DHT)
#include "client.cpp"
#include "server.hpp"
#endif*/

#define MAX_MEM_COPY 1024 * 1024 * 8 //1MB
//std::mutex emplace_mutex;
//std::mutex access_mutex;
//std::shared_mutex erase_mutex;
boost::shared_mutex erase_mutex;
std::mutex map_mutex;
//std::map<std::string, std::mutex*> erase_mutex_map;

HierarchicalDataPlaneBuilder* HierarchicalDataPlane::create(int instance_id_, int world_size_, int number_of_workers, int hierarchy_size){
    return new HierarchicalDataPlaneBuilder{instance_id_, world_size_, number_of_workers, hierarchy_size};
};

HierarchicalDataPlane::HierarchicalDataPlane(int id, int ws, int nw, int hierarchy_size) {
    neighbours_index = 0;
    placed_samples = 0;
    instance_id = id;
    rank = -1;
    world_size = ws;
    worker_id = -1;
    num_workers = nw;
    storage_sync_timeout = 15;
    reached_stability = false;
    synchronization_thread_pool = new ctpl::thread_pool(1);
    storage_hierarchy_size = hierarchy_size;
    total_used_threads = 0;
    shared_thread_pool = false;
    debug_logger = new Logger();
    rate_limiter = new ClientWatchRateLimiter(-1);
    control_handler = nullptr;
    profiler = nullptr;
    profiling_service = nullptr;
    transparent_api = false;
    uses_large_seq_reads = true;
    epoch_size_signal = 0;
    std::cout << "HP" << std::endl << std::flush;
    /*std::cout << "Using Boost "     
          << BOOST_VERSION / 100000     << "."  // major version
          << BOOST_VERSION / 100 % 1000 << "."  // minor version
          << BOOST_VERSION % 100                // patch level
          << std::endl;*/

    workers_ip = std::vector<std::string>();
    char * val_temp = std::getenv( "WRKS_ADDRS" );
    std::cout << "WRKS_ADDRS: " << val_temp << std::endl;
    std::string s = std::string(val_temp);
    char* val=s.c_str();
    std::cout << "WRKS_ADDRS: " << val << std::endl;

    char * p = strtok(val, ",");
    while (p != NULL) {
        workers_ip.push_back(std::string(p));
        p = strtok(NULL, ",");
    }

    self_ip = workers_ip[atoi(std::getenv( "TASK_ID" ))];
    std::cout << "self_ip: " << self_ip << std::endl;

    worker_files_map = std::map<std::string, std::vector<std::string>>();
    for(auto worker : workers_ip)
        worker_files_map.insert({worker, std::vector<std::string>()});

    //aux::RunServer(this);

    if(!dht && ! grpc){
        std::thread updater(&HierarchicalDataPlane::updater, this);
        updater.detach();
        std::thread exchager(&HierarchicalDataPlane::exchager, this);
        exchager.detach();
        //std::thread teste(&HierarchicalDataPlane::exchager, this);
        //teste.detach();
    }
}



/*void HierarchicalDataPlane::teste(){
    for (std::string worker_ip : workers_ip){
        int client = socket(AF_INET, SOCK_STREAM, 0);
        if (client < 0){
            std::cout << "Error creating socket" << std::endl;
            exit(1);
        }
        int port_server = 50051;
        struct sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = inet_addr(worker_ip.c_str());
        server_addr.sin_port = htons(port_server);
        

        if (connect(client, (struct sockaddr*)&server_addr, sizeof(server_addr)) == 0) {
            std::string fi = "train-00001-01024";
            int size=strlen(fi.c_str());
            uint32_t network_msg_size = htonl(size);
            send(client, (const char*)&network_msg_size, sizeof(uint32_t), 0);
            send(client, fi.c_str(), size, 0);

            size=strlen(self_ip.c_str());
            network_msg_size = htonl(size);
            send(client, (const char*)&network_msg_size, sizeof(uint32_t), 0);
            send(client, self_ip.c_str(), size, 0);
            std::cout << "Sender " << self_ip << " anuncia que tem o ficheiro" << fi << "." << std::endl << std::flush;
        
        close(client);
        }
    }
}*/




void HierarchicalDataPlane::updater(){
    int num_workers = workers_ip.size();
    int server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (server_socket == -1){
        std::cout << "Failed to create socket descriptor. " << strerror(errno) << "\n";
        return ;}

    int port_server = 50051;
    struct sockaddr_in client_address;
    int csize = sizeof(client_address);
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr(self_ip.c_str());
    address.sin_port = htons(port_server);

    if (bind(server_socket, (struct sockaddr *)&address, sizeof(address)) < 0){
        std::cout << "Failed to bind socket! " << strerror(errno) << "\n";
        return ;}
    
    if (listen(server_socket, num_workers) < 0) { 
        std::cout << "Failed to listen on socket! " << strerror(errno) << "\n";
        exit(EXIT_FAILURE);
    }
    while(1) {
        int client = accept(server_socket, (struct sockaddr*) &client_address, (socklen_t*) &csize);

	{
        std::unique_lock<std::mutex> lock(map_mutex);
        uint32_t ntw_size;
        int received_size = recv(client, &ntw_size, sizeof(uint32_t), 0);
        int sz = ntohl(ntw_size);
        char fl[sz];
        received_size = recv(client, fl, sz, 0);
        std::string file (fl);
        file = file.substr(0, received_size);

        received_size = recv(client, &ntw_size, sizeof(uint32_t), 0);
        sz = ntohl(ntw_size);
        char wrkr[sz];
        received_size = recv(client, wrkr, sz, 0);
        std::string worker (wrkr);
        worker = worker.substr(0, received_size);

        //std::thread upd(&HierarchicalDataPlane::update, this, worker, file);
        //upd.detach();
                
        if (worker_files_map.find(worker) == worker_files_map.end())
            worker_files_map.insert({worker, std::vector<std::string>()});

        worker_files_map[worker].push_back(file);
	}

	
        //std::cout << "Worker " << self_ip << " knows that " << worker << " has " << file << " saved." << std::endl << std::flush;

        close(client);
    }
}


void HierarchicalDataPlane::update(std::string worker, std::string file){
    if (worker_files_map.find(worker) == worker_files_map.end())
        worker_files_map.insert({worker, std::vector<std::string>()});
          
    worker_files_map[worker].push_back(file);
}



void HierarchicalDataPlane::exchager(){
    int num_workers = workers_ip.size();
    int server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (server_socket == -1){
        std::cout << "Failed to create socket descriptor. " << strerror(errno) << "\n";
        return ;}

    int port_server = 50052;
    struct sockaddr_in client_address;
    int csize = sizeof(client_address);
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr(self_ip.c_str());
    address.sin_port = htons(port_server);

    if (bind(server_socket, (struct sockaddr *)&address, sizeof(address)) < 0){
        std::cout << "Failed to bind socket! " << strerror(errno) << "\n";
        return ;}
    
    if (listen(server_socket, num_workers) < 0) { 
        std::cout << "Failed to listen on socket! " << strerror(errno) << "\n";
        exit(EXIT_FAILURE);
    }
    while(1) {
        int client = accept(server_socket, (struct sockaddr*) &client_address, (socklen_t*) &csize);
        struct linger so_linger;
	so_linger.l_onoff = 1;
	so_linger.l_linger = 30;
	/*int z = setsockopt(client,
               SOL_SOCKET,
               SO_LINGER,
               &so_linger,
               sizeof so_linger);
	if ( z ) perror("setsockopt(2)");*/

        uint32_t ntw_offset, ntw_size, ntw_msg_size;
        int offset, size, msg_size;

        int received_size = recv(client, &ntw_offset, sizeof(uint32_t), 0);
        offset = ntohl(ntw_offset);

        received_size = recv(client, &ntw_size, sizeof(uint32_t), 0);
        size = ntohl(ntw_size);

        received_size = recv(client, &ntw_msg_size, sizeof(uint32_t), 0);
        msg_size = ntohl(ntw_msg_size);

        char buff[msg_size];
        received_size = recv(client, buff, msg_size, 0);
	
        char buf[size];
	std::string file = std::string(buff);
        file = file.substr(0, received_size);
        auto* fi = metadata_container->get_metadata(file);
        //std::cout << "buff is: " << std::string(buff) << std::endl << std::flush;
        //std::cout << "fi_name is: " << fi->get_name() << " -------------- " << fi->get_file_descriptor(fi->get_storage_level()) << "   size is: " << size << "     offset is: " << offset  << std::endl << std::flush;
	
        //received_size = base_read_aux(fi, buf, offset, size, false);
        //Status<ssize_t> read_status = read_from_storage(true, fi, buf, offset, size, fi->get_storage_level(), true);
        
        //ssize_t r = pread(fi->get_file_descriptor(fi->get_storage_level()), buf, size, static_cast<off_t>(offset));
	//fcntl(fi->get_file_descriptor(fi->get_storage_level()), F_GETFD);
	std::thread sender(&HierarchicalDataPlane::sender, this, client, file, offset, size);
	sender.detach();
	
	}
}

void HierarchicalDataPlane::sender(int client, std::string file, int offset, int size){
	auto* fi = metadata_container->get_metadata(file);
        auto m_pread = (libc_pread_t) dlsym(RTLD_NEXT, "pread");
	ssize_t r;
        int fd; char buf[size];

        if(fcntl(fi->get_file_descriptor(fi->get_storage_level()), F_GETFD)==0){
		fd = fi->get_file_descriptor(fi->get_storage_level());
	        r = m_pread(fi->get_file_descriptor(fi->get_storage_level()), buf, size, static_cast<off_t>(offset));
	}else{ 

		auto level = fi->get_storage_level(); 
		/*std::string path = get_fs_driver(level)->get_full_path(file); 
		auto m_open_var = (libc_open_variadic_t) dlsym(RTLD_NEXT, "open");
		fd = m_open_var(path.c_str(), O_RDONLY);
		r = m_pread(fd, buf, size, static_cast<off_t>(offset));
                auto m_close = (libc_close_t) dlsym(RTLD_NEXT, "close");
                m_close(fd;*/
		get_fs_driver(level)->open_descriptor(fi, O_RDONLY, false, true);
		fd = fi->get_file_descriptor(level);
		r = m_pread(fd, buf, size, static_cast<off_t>(offset));
		get_fs_driver(level)->close_descriptor(fi, true);
		
		
		/*std::string path = "/tmp/100g_tfrecords/"; 
		path += file;
		const char *pathname = path.c_str(); //((std::string) "/scratch1/09111/mbbm/100g_tfrecords/" + fi->get_name()).c_str();
                //std::cout << pathname << std::endl << std::flush;
		//(static_cast<FileSystemDriver*>(storage_hierarchical_matrix[matrix_index][fi->get_storage_level()]))->get_full_path(fi->get_name()).c_str();
		auto m_open_var = (libc_open_variadic_t) dlsym(RTLD_NEXT, "open");
		auto m_close = (libc_close_t) dlsym(RTLD_NEXT, "close");
		//m_close(fi->get_file_descriptor(fi->get_storage_level()));
		fd = m_open_var(pathname, O_RDONLY);
		std::cout << "fd: " << fd << std::endl << std::flush;
		if(fcntl(fd)==0){      //if(fd==-1){
			perror("open /tmp falha");
			path = "/scratch1/09111/mbbm/100g_tfrecords/";
	                path += file;
        	        pathname = path.c_str();
			fd = m_open_var(pathname, O_RDONLY);
		}
		std::cout << pathname << std::endl << std::flush;
		r = m_pread(fi->get_file_descriptor(fi->get_storage_level()), buf, size, static_cast<off_t>(offset));
		m_close(fd);*/
	}
        //std::cout << "r is: " << r << std::endl << std::flush;
        //received_size = read_status.return_value;
        //std::cout << "buff is: " << std::string(buff) << std::endl << std::flush;


        ssize_t sent=-1;
        if(r > 0){
           //uint32_t network_r=htonl(r);
           //send(client, &network_r, sizeof(uint32_t), 0);
           sent = send(client, buf, r, 0);
        } else std::cout << self_ip << " tried reading " << size << " from " << fi->get_name() <<
               " no offset " << offset <<  " and read " << r << " characters." << std::endl << std::flush;

	/*uint32_t ntw_tot; int total;
        recv(client, &ntw_tot, sizeof(uint32_t), 0);
        total = ntohl(ntw_tot);
	while(total < size){
                std::cout << "r = " << r << std::endl << std::flush;
                total=0;
                send(client, buf, r, 0);
        	if(total<size){ received_size = recv(client, &ntw_tot, sizeof(uint32_t), 0); total = ntohl(ntw_tot); }
        }*/



        shutdown(client, SHUT_WR);
        for(;;) {
           ssize_t res=recv(client, buf, 4000, 0);
           if(res < 0) {
               perror("reading");
               exit(1);
           }
           if(!res) break;
        }
        //if(size != r) std::cout << "Worker " << self_ip << " sent " << file << " - " << size << " -> " << r << " enviou " << sent << std::endl << std::flush;
        //else std::cout << self_ip << " sent " << file << " - " << size  << " enviou " << sent << std::endl << std::flush;
	//std::cout << "Worker " << self_ip << " sent " << file << " - " << size << std::endl << std::flush;
        close(client);
}





HierarchicalDataPlane::HierarchicalDataPlane(HierarchicalDataPlane* hdp){
    std::cout << "HP 2" << std::endl << std::flush;
    neighbours_index = 0;
    placed_samples = 0;
    storage_sync_timeout = hdp->storage_sync_timeout;
    instance_id = hdp->instance_id;
    matrix_index = hdp->matrix_index;
    rank = hdp->rank;
    world_size = hdp->world_size;
    worker_id = hdp->worker_id;
    num_workers = hdp->num_workers;
    reached_stability = false;
    synchronization_thread_pool = hdp->synchronization_thread_pool;
    storage_hierarchical_matrix = hdp->storage_hierarchical_matrix;
    storage_hierarchy_size = hdp->storage_hierarchy_size;
    metadata_container = hdp->metadata_container;
    storage_hierarchy_thread_pools = hdp->storage_hierarchy_thread_pools;
    total_used_threads = hdp->total_used_threads;
    shared_thread_pool = hdp->shared_thread_pool;
    debug_logger = hdp->debug_logger;
    rate_limiter = hdp->rate_limiter;
    profiler = hdp->profiler;
    profiling_service = hdp->profiling_service;
    control_handler = hdp->control_handler;
    type = hdp->type;
    uses_large_seq_reads = hdp->uses_large_seq_reads;
    epoch_size_signal = hdp->epoch_size_signal;

    workers_ip = hdp->workers_ip;
    self_ip = hdp->self_ip;
    worker_files_map = hdp->worker_files_map;
}

HierarchicalDataPlane::~HierarchicalDataPlane(){
    /*for(std::map<std::string, std::vector<std::string>>::const_iterator it = worker_files_map.begin(); it != worker_files_map.end(); ++it){
        std::cout << it->first << " : ";
        for (auto i: it->second)
	    std::cout << i << ' ';
        std::cout << "\n" <<std::flush;
    }*/

    delete profiling_service;
}

absl::string_view HierarchicalDataPlane::decode_filename(absl::string_view full_path){
    return absl::StripPrefix(full_path, get_driver(storage_hierarchy_size - 1)->prefix());
}

//Init is only called on open. This assumes that the pread and close always precede the open syscall and
//are executed by the same calling thread.

int HierarchicalDataPlane::open(const char *pathname, int flags, mode_t mode, bool _64_option){
    std::string s_pathname(pathname);
    auto filename = decode_filename(s_pathname);

    //open is not directed to a file in source dir
    if(filename.length() == s_pathname.length()){
        int fd = get_fs_driver(storage_hierarchy_size - 1)->passthrough_lib_open(pathname, flags, mode, _64_option);
        if(debug_logger->is_activated()) {
            if (_64_option) {
                debug_write("Stray open64_variadic(" + std::string(pathname) + ") = " + std::to_string(fd)
                + ". PID= " + std::to_string(getpid()));
            }
            else {
                debug_write("Stray open_variadic(" + std::string(pathname) + ") = " + std::to_string(fd)
                + ". PID= " + std::to_string(getpid()));
            }
        }
        //always assumes that the last level is file system
        return fd;
    }else{
        absl::call_once(once_, [this](){
            metadata_init();
        });
        auto* fi = metadata_container->get_metadata(filename.data());
        int storage_level = fi->get_storage_level();
        int fildes;
        bool has_opened = true;
        if(fi->has_shareable_file_descriptors()){
            has_opened = get_fs_driver(storage_level)->open_descriptor(fi, flags, mode, _64_option, true) == 1;
            fildes = fi->get_file_descriptor(storage_level);
        }else{
            fildes = get_fs_driver(storage_level)->passthrough_lib_open(pathname, flags, mode, _64_option);
        }
        metadata_container->store_fildes(fildes, fi);
        if(debug_logger->is_activated()) {
            if (_64_option) {
                debug_write("open64_variadic(" + std::string(pathname) + ") = " + std::to_string(fildes)
                + ". PID= " + std::to_string(getpid()));
            }else{
                debug_write("open_variadic(" + std::string(pathname) + ") = " + std::to_string(fildes)
                + ". PID= " + std::to_string(getpid()));
            }
        }

        if(profiler && has_opened){
            profiler->submit_client_metadata_request("open", storage_level);
        }

        return fildes;
    }
}

int HierarchicalDataPlane::open(const char *pathname, int flags, bool _64_option) {
    std::string s_pathname(pathname);
    auto filename = decode_filename(s_pathname);
    //open is not directed to a file in source dir
    if(filename.length() == s_pathname.length()){
        int fd = get_fs_driver(storage_hierarchy_size - 1)->passthrough_lib_open(pathname, flags, _64_option);
        if(debug_logger->is_activated()) {
            if (_64_option) {
                debug_write("Stray open64(" + std::string(pathname) + ") = " + std::to_string(fd)
                + ". PID= " + std::to_string(getpid()));
            }
            else {
                debug_write("Stray open(" + std::string(pathname) + ") = " + std::to_string(fd)
                + ". PID= " + std::to_string(getpid()));
            }
        }
        //always assumes that the last level is file system
        return fd;
    }else {
        absl::call_once(once_, [this](){
            metadata_init();
        });
        auto *fi = metadata_container->get_metadata(filename.data());
        int storage_level = fi->get_storage_level();
        int fildes;
        bool has_opened = true;
        if(fi->has_shareable_file_descriptors()){
            has_opened = get_fs_driver(storage_level)->open_descriptor(fi,  flags, _64_option, true) == 1;
            fildes = fi->get_file_descriptor(storage_level);
        }else{
            fildes = get_fs_driver(storage_level)->passthrough_lib_open(pathname, flags, _64_option);
        }

        metadata_container->store_fildes(fildes, fi);
        if(debug_logger->is_activated()) {
            if (_64_option) {
                debug_write("open64(" + std::string(pathname) + ") = " + std::to_string(fildes)
                + ". PID= " + std::to_string(getpid()));
            }else{
                debug_write("open(" + std::string(pathname) + ") = " + std::to_string(fildes)
                + ". PID= " + std::to_string(getpid()));
            }
        }

        if(profiler && has_opened){
            profiler->submit_client_metadata_request("open", storage_level);
	}

        return fildes;
    }
}

int HierarchicalDataPlane::close(int fildes){
    //close to a file not in source dir
    FileInfo* fi = nullptr;
    if (is_training){
        fi = metadata_container->remove_fildes(fildes);
        //std::cout << "oioio" << std::endl;
    }
    if(!fi){
        int res = get_fs_driver(storage_hierarchy_size - 1)->passthrough_lib_close(fildes);
        if(debug_logger->is_activated())
            debug_write("Stray close(" + std::to_string(fildes) + ") = " + std::to_string(res)
                        + ". PID= " + std::to_string(getpid()));
        return res;
    }
    if(debug_logger->is_activated())
        debug_write("close(" + std::to_string(fildes) + ") = " + std::to_string(0)
                    + ". PID= " + std::to_string(getpid()));

    //Close all open fd for this file.
    if(fi->has_shareable_file_descriptors()){
        for(int i = 0; i < storage_hierarchy_size; i++){
            if(is_file_system(i)){
                int has_closed = get_fs_driver(i)->close_descriptor(fi, true) == 1;
                if(profiler && has_closed){
                    profiler->submit_client_metadata_request("close", i);
                }
            }
        }
    }else{
        //current storage level has to be the same as the one in the open (small files).
        get_fs_driver(fi->get_storage_level())->passthrough_lib_close(fildes);
        if(profiler){
            profiler->submit_client_metadata_request("close", fi->get_storage_level());
        }
    }

    {
    //std::unique_lock<std::mutex> lock_map(erase_mutex);
    boost::unique_lock<boost::shared_mutex> lock_map(erase_mutex);
    //{
    //if(erase_mutex_map.find(fi->get_name()) != erase_mutex_map.end()) std::unique_lock<std::mutex> lock_file( *(erase_mutex_map[fi->get_name()]) );
    if((files_caching.find(fi->get_name()) != files_caching.end())) {
	delete[] files_caching[fi->get_name()].second;
        int x = files_caching.erase(fi->get_name());
    } 
    //}
    }

    return 0;
}

ssize_t HierarchicalDataPlane::read(const std::string& filename, char* result, uint64_t offset, size_t n) {
    auto* fi = metadata_container->get_metadata(filename);
    return read(fi, result, offset, n, false);
}

ssize_t HierarchicalDataPlane::read_from_id(int file_id, char* result, uint64_t offset, size_t n) {
    auto* fi = metadata_container->get_metadata(file_id);
    return read(fi, result, offset, n, false);
}

ssize_t HierarchicalDataPlane::read_from_id(int file_id, char* result){
    auto* fi = metadata_container->get_metadata(file_id);
    return read(fi, result, 0, fi->_get_size(), false);
}

ssize_t HierarchicalDataPlane::pread(int fildes, char *result, size_t nbyte, uint64_t offset, bool _64_option){
    auto* fi = metadata_container->get_metadata_from_fildes(fildes);
    if(!fi){
        if(debug_logger->is_activated()){
            if(_64_option){
                debug_write("Stray pread64(" + std::to_string(fildes) + ", ..., " + std::to_string(nbyte) + ", "
                + std::to_string(offset) + ")" + "From Lustre . PID= " + std::to_string(getpid()));
            } else{
                debug_write("Stray pread(" + std::to_string(fildes) + ", ..., " + std::to_string(nbyte) + ", "
                + std::to_string(offset) + ")" + "From Lustre . PID= " + std::to_string(getpid()));
            }
        }
        return get_fs_driver(storage_hierarchy_size - 1)->passthrough_lib_pread(fildes, result, nbyte, offset, _64_option);
    }
    if(debug_logger->is_activated()){
        int storage_level = fi->get_storage_level();
        if(_64_option){
            debug_write("pread64(" + std::to_string(fildes) + ", ..., " + std::to_string(nbyte) + ", "
            + std::to_string(offset) + ")" + " From: " + std::to_string(storage_level) + " . PID= " + std::to_string(getpid()));
        }else{
            debug_write("pread(" + std::to_string(fildes) + ", ..., " + std::to_string(nbyte) + ", "
            + std::to_string(offset) + ")" + " From: " + std::to_string(storage_level) + " . PID= " + std::to_string(getpid()));
        }
    }
    return read(fi, result, offset, nbyte, _64_option);
}

ssize_t HierarchicalDataPlane::read(FileInfo* fi, char* result, uint64_t offset, size_t n, bool _64_option){
    ssize_t bytes_size = 0;
    if(profiler) {
        TimeSubmission ts;
        ts.set_start();
        bytes_size = base_read(fi, result, offset, n, _64_option);
        ts.set_end();
        profiler->submit_client_read_time(ts);
        if(metadata_container->is_train_file(fi->get_name())) {
            check_epoch_finish(bytes_size);
        }
    }else{
        bytes_size = base_read(fi, result, offset, n, _64_option);
    }
    return bytes_size;
}



ssize_t HierarchicalDataPlane::send_exchange_request(std::string worker, FileInfo* fi, char* result, uint64_t offset, size_t requested_size){
    int client = socket(AF_INET, SOCK_STREAM, 0);
    if (client < 0){
        std::cout << "Error creating socket - send_exchange_request\n\n" << std::endl;
        //exit(1);
        return -1;
    }else{
    int port_server = 50052;
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(worker.c_str());
    server_addr.sin_port = htons(port_server);

    if (connect(client, (struct sockaddr*)&server_addr, sizeof(server_addr)) == 0) {
        int path_size = fi->get_name().size();
        uint32_t network_path_size=htonl(fi->get_name().size());
        uint32_t network_size=htonl(requested_size);
        uint32_t network_offset=htonl(offset);
        send(client, &network_offset, sizeof(uint32_t), 0);
        send(client, &network_size, sizeof(uint32_t), 0);
        send(client, &network_path_size, sizeof(uint32_t), 0);
        send(client, fi->get_name().c_str(), path_size, 0);

        //uint32_t network_bs;
        //recv(client, &network_bs, sizeof(uint32_t), 0);
        //int bs = ntohl(network_bs);

        int total = 0;
        int count;
        while (total < requested_size && (count = recv(client, result+total, requested_size-total, MSG_WAITALL)) > 0){
                total += count;
                //std::cout << total << ", " << std::flush;
        }
        close(client);
        if(total == requested_size) {
	    debug_write("[HierarchicalDataPlane] " + self_ip + " reading from " + worker + " file " + fi->get_name() +
                " with offset:" + std::to_string(offset) + " and size:" + std::to_string(requested_size));
	    return requested_size;
	}
        else return -2;
    }}   
    return -1;
}



void HierarchicalDataPlane::mem_prefetcher(std::string worker, char* result, FileInfo* fi, ssize_t requested_size){
    char* file = new char[125072862];
    memcpy(file, result, requested_size);
    bool b = false;
    
    {
    //std::unique_lock<std::mutex> lock(erase_mutex);
    boost::unique_lock<boost::shared_mutex> lock(erase_mutex);
    if (files_caching.find(fi->get_name()) == files_caching.end()){
        while(!b) b = (files_caching.emplace(fi->get_name(), std::pair<int, char*> (requested_size, file) ) ).second;
        //b = !(erase_mutex_map.find(fi->get_name()) == erase_mutex_map.end());
        //while(!b) b = (erase_mutex_map.emplace(fi->get_name(),new std::mutex)).second;
    }
    }

    int offset = requested_size; int i=0;
    requested_size = requested_size + offset > fi->_get_size() ? fi->_get_size() - offset : requested_size;
    while(requested_size>0){
        char buf[requested_size];
        ssize_t r = -3;

        while(r!=requested_size) {
		r = send_exchange_request(worker,fi,buf,offset,requested_size); 
		if(r==-1) {
		    /* {
	 	    std::unique_lock<std::mutex> lock(map_mutex);
		    worker_files_map.erase(fi->get_name());
		    } */
		    return;
		}
	}
        {
	//std::unique_lock<std::mutex> lock( *(erase_mutex_map[fi->get_name()]) );
        //std::shared_lock<std::mutex> lock(erase_mutex);
        boost::shared_lock<boost::shared_mutex> lock(erase_mutex);
        b = files_caching.find(fi->get_name()) == files_caching.end();
        if(!b){
            memcpy(files_caching[fi->get_name()].second + offset, buf, requested_size);
            //files_caching[fi->get_name()].first += requested_size;
            i = files_caching[fi->get_name()].first.load() + requested_size;
            files_caching[fi->get_name()].first.exchange( i );
	}}
        
        if(b) break;
	offset += requested_size;
        requested_size = requested_size + offset > fi->_get_size() ? fi->_get_size() - offset : requested_size;
    }
}



//TODO needs to call a new read from fs to deal with !has_shared_file_descriptor that accepts the fd.
//TODO break this function in two (transparent and non-transparent)
ssize_t HierarchicalDataPlane::base_read(FileInfo* fi, char* result, uint64_t offset, size_t n, bool _64_option){
    //std::cout << "--------------------------" << fi->get_name() << ":" << n << "-------------------------------------- offset: " << offset <<std::endl << std::flush;
    if(offset >= fi->_get_size()) {
        if(debug_logger->is_activated())
            debug_write("Tried to read from offset: " + std::to_string(offset) + " which goes beyond file size: " +
                std::to_string(fi->_get_size()) + " name: " + fi->get_name());
        return 0;
    }

    //it's crucial that we always use this storage_level "snapshot" since it can be changes in parallel
    int storage_level = fi->get_storage_level();
    if(storage_level == storage_hierarchical_matrix[matrix_index].size()-1){
        /*if(dht){
            const char* entry;
            
            auto info_key = dht::InfoHash::get(fi->get_name());
            auto vals = dht_node.get(info_key).get();
            if(!vals.empty()){
                auto v = (char*) vals.front().get()->data.data();
                v=&v[1];
                std::cout << " - value - " << v << " - " << fi->get_name() << std::flush;

                Client client(v);
                int content_size = client.Request(fi->get_name(), result, offset, n);
                return (ssize_t) content_size;
            }
            else{
                std::cout << "------------------------------- nao entrou : " << fi->get_name() <<std::endl;
            }
            
        }
        else if(grpc){
            for(auto worker_saved_data : worker_files_map){
                if(std::find(worker_saved_data.second.begin(), worker_saved_data.second.end(),fi->get_name())!=worker_saved_data.second.end()){
                    Client client(worker_saved_data.first);
                    int content_size = client.Request(fi->get_name(), result, offset, n);
                    return (ssize_t) content_size;
                    break;
                }
            }
        }else{*/
            bool found = false;
            {
            //if(erase_mutex_map.find(fi->get_name()) != erase_mutex_map.end()) std::unique_lock<std::mutex> lock( *(erase_mutex_map[fi->get_name()]) );
            //std::shared_lock<std::mutex> lock(erase_mutex);
            boost::shared_lock<boost::shared_mutex> lock(erase_mutex);
            
            found=files_caching.find(fi->get_name()) != files_caching.end()  &&  files_caching[fi->get_name()].first > offset;
	    if(found){
		//std::cout << "já tinha " << fi->get_name() << " - " << offset << std::endl << std::flush;
                ssize_t requested_size = n + offset > fi->_get_size() ? fi->_get_size() - offset : n;
                memcpy(result, files_caching[fi->get_name()].second + offset, requested_size);
		return requested_size;
            }
	    }

            if(!found) 
	    for(auto worker : workers_ip){
		if(worker != self_ip){
		    if(!worker_files_map[worker].empty() &&
		       std::find(worker_files_map[worker].begin(), worker_files_map[worker].end(), fi->get_name()) != worker_files_map[worker].end()){
			size_t requested_size = n + offset > fi->_get_size() ? fi->_get_size() - offset : n;

			ssize_t r = -3; 
                        int count=0;
			while(!(r>0) && count<1) {r = send_exchange_request(worker,fi,result,offset,requested_size); count++;}

			if( offset == 0 && r == requested_size) {
                            std::thread mem_prefetcher(&HierarchicalDataPlane::mem_prefetcher, this, worker, result, fi, requested_size);
                            mem_prefetcher.detach();
                        }

                        if(r>0) return r;
		    }
		}
	    }
    }


    size_t requested_size = n + offset > fi->_get_size() ? fi->_get_size() - offset : n;

    if(debug_logger->is_activated())
        debug_write("client reading from level " +
                    std::to_string(storage_level) + 
                    " file " + fi->get_name() + 
                    " with offset: " +
                    std::to_string(offset) + " and size: " + std::to_string(requested_size));

    //Open check phase
    if(!transparent_api && fi->has_shareable_file_descriptors() && is_file_system(storage_level)){
        check_open_phase(fi, storage_level);
    }
    Status<ssize_t> read_status = read_from_storage(true, fi, result, offset, requested_size, storage_level, _64_option);
    //Started reading must be called after read so that the first read opens de file if fs driver.
    if(read_status.state == SUCCESS) {
        if(control_handler->check_placement_validity(dynamic_cast<PlacedFileInfo*>(fi))){
            auto* f = new File(fi, offset, requested_size);
            if(f->is_full_read()){
                memcpy(f->get_content(), result, requested_size);
            }
            get_fs_driver(storage_level)->open_descriptor(fi, O_RDWR, false, false);
            //std::cout << "ssize_t HierarchicalDataPlane::base_read(FileInfo* fi, char* result, uint64_t offset, size_t n, bool _64_option) - control_handler->place(f);" << std::endl << std::flush;
            control_handler->place(f);
        }
    }
    //TODO this mechanism needs to be added with prefetch + relaxed read
    //Close check phase. Transparent uses the close method for this.
    if(!transparent_api && fi->has_shareable_file_descriptors() && is_file_system(storage_level)){
        check_close_phase(fi, requested_size, offset, storage_level);
    }
    return read_status.return_value;
}




ssize_t HierarchicalDataPlane::base_read_aux(FileInfo* fi, char* result, uint64_t offset, size_t n, bool _64_option){
    if(offset >= fi->_get_size()) {
        if(debug_logger->is_activated())
            debug_write("Tried to read from offset: " + std::to_string(offset) + " which goes beyond file size: " +
                std::to_string(fi->_get_size()) + " name: " + fi->get_name());
        return 0;
    }

    //it's crucial that we always use this storage_level "snapshot" since it can be changes in parallel
    int storage_level = fi->get_storage_level();
    std::cout << storage_level << std::endl;
    size_t requested_size = n + offset > fi->_get_size() ? fi->_get_size() - offset : n;

    if(debug_logger->is_activated())
        debug_write("client reading from level " +
                    std::to_string(storage_level) + 
                    " file " + fi->get_name() + 
                    " with offset: " +
                    std::to_string(offset) + " and size: " + std::to_string(requested_size));

    //Open check phase
    if(!transparent_api && fi->has_shareable_file_descriptors() && is_file_system(storage_level)){
        check_open_phase(fi, storage_level);
    }
    Status<ssize_t> read_status = read_from_storage(true, fi, result, offset, requested_size, storage_level, _64_option);
    //Started reading must be called after read so that the first read opens de file if fs driver.
    if(read_status.state == SUCCESS) {
        if(control_handler->check_placement_validity(dynamic_cast<PlacedFileInfo*>(fi))){
            auto* f = new File(fi, offset, requested_size);
            if(f->is_full_read()){
                memcpy(f->get_content(), result, requested_size);
            }
            get_fs_driver(storage_level)->open_descriptor(fi, O_RDWR, false, false);
            control_handler->place(f);
        }
    }
    //TODO this mechanism needs to be added with prefetch + relaxed read
    //Close check phase. Transparent uses the close method for this.
    if(!transparent_api && fi->has_shareable_file_descriptors() && is_file_system(storage_level)){
        check_close_phase(fi, requested_size, offset, storage_level);
    }
    return read_status.return_value;
}

void HierarchicalDataPlane::check_open_phase(FileInfo* fi, int storage_level){
    //when using large sequential reads on intrusive open is called once.
    bool has_opened = true;
    if(uses_large_seq_reads){
        if(fi->get_file_descriptor(storage_level) == -1){
            has_opened = get_fs_driver(storage_level)->open_descriptor(fi,  O_RDONLY, false, true) == 1; 
            if(profiler && has_opened){
                profiler->submit_client_metadata_request("open", storage_level);
            }
        }
    }
    //When large sequential is not present each client needs to signal the open to avoid close.
    //TODO if this mode is true then eviction cannot be made only when we reach EOF
    else {
        has_opened = get_fs_driver(storage_level)->open_descriptor(fi,  O_RDONLY, false, true) == 1;
        if(profiler && has_opened){
            profiler->submit_client_metadata_request("open", storage_level);
        }
    }
}

//TODO check this!!
void HierarchicalDataPlane::check_close_phase(FileInfo* fi, size_t requested_size, uint64_t offset, int storage_level){
    bool has_closed = true;
    if(offset + requested_size >= fi->_get_size() && is_file_system(storage_level)){
        //fi->client_ended_read(storage_level);
        has_closed = get_fs_driver(storage_level)->close_descriptor(fi, true) == 1;

        if(profiler && has_closed){
            profiler->submit_client_metadata_request("close", storage_level);
        }
    }
    if(fi->storage_changed()){
        //To prevent close without open
        if(fi->get_file_descriptor(fi->get_last_storage_read()) != -1) {
            //fi->client_ended_read(fi->get_last_storage_read());
            has_closed = get_fs_driver(fi->get_last_storage_read())->close_descriptor(fi, true) == 1;

            if(profiler && has_closed){
                profiler->submit_client_metadata_request("close", fi->get_last_storage_read());
            }
        }
        fi->update_last_storage_read();
    }
}

Status<ssize_t> HierarchicalDataPlane::read_from_storage(bool client_req, File* f, int level){
    return read_from_storage(client_req, f->get_info(), f->get_content(), f->get_offset(), f->get_requested_size(), level, false);
}

Status<ssize_t> HierarchicalDataPlane::read_from_storage(bool client_req, FileInfo* fi, char* result, uint64_t offset, size_t n, int level, bool _64_option){
    Status<ssize_t> s = NOT_FOUND;
    if(profiler){
        StatSubmission ss;
        ss.set_start();
        s = base_read_from_storage(fi, result, offset, n, level, _64_option);
        ss.set_end();
        ss.set_n_bytes(n);
        if(client_req){
            profiler->submit_storage_client_read(ss, level);
        }else{
            profiler->submit_storage_background_read(ss, level);
        }
    }else{
        s = base_read_from_storage(fi, result, offset, n, level, _64_option);
    }
    return s;
}

Status<ssize_t> HierarchicalDataPlane::base_read_from_storage(FileInfo* fi, char* result, uint64_t offset, size_t n, int level, bool _64_option){
    Status<ssize_t> s = NOT_FOUND;
    int found_level = level;
    while(s.state == NOT_FOUND && found_level < storage_hierarchy_size){
        s = storage_hierarchical_matrix[matrix_index][found_level]->read(fi, result, offset, n, _64_option);
        if(s.state == NOT_FOUND)
            found_level += 1;
    }
    if(s.state == NOT_FOUND){
        std::cerr << "File : " << fi->get_name() << " could not be found anywhere. Requested level: " + std::to_string(level) + " max level permitted: " +
        std::to_string(found_level - 1) + ". Level 0 descriptor: " + std::to_string(fi->get_file_descriptor(0)) + ". Level 1 descriptor: " + std::to_string(fi->get_file_descriptor(1)) +  ". Exiting...\n";
        exit(1);
    }

    if (found_level != level) {
        s = MISS;
        //f->get_info()->set_storage_level(found_level);
        if(debug_logger->is_activated())
            debug_write("File : " + fi->get_name() + " cache miss. Target level: " + std::to_string(level) + " Actual level: " + std::to_string(found_level));
    }
    return s;
}

void* HierarchicalDataPlane::mmap (void *addr, size_t length, int prot, int flags, int fd, off_t offset){
    auto* fi = metadata_container->get_metadata_from_fildes(fd);
    if(!fi){
        if(debug_logger->is_activated()){
            std::ostringstream address;
            address << addr;
            std::string addr_name = address.str();
            debug_write("Stray mmap(" + addr_name + ", " + std::to_string(length) + ", ..., " + std::to_string(prot) + ", "
            + std::to_string(flags) + ", " + std::to_string(fd) + ", " + std::to_string(offset) +
            "). PID= " + std::to_string(getpid()));
        }
        return get_fs_driver(storage_hierarchy_size - 1)->passthrough_lib_mmap(addr, length, prot, flags, fd, offset);
    }
    if(debug_logger->is_activated()){
        std::ostringstream address;
        address << addr;
        std::string addr_name = address.str();
        debug_write("mmap(" + addr_name + ", "  + std::to_string(length) + ", ..., " + std::to_string(prot) + ", "
        + std::to_string(flags) + ", " + std::to_string(fd) + ", " + std::to_string(offset) +
        "). PID= " + std::to_string(getpid()));
    }

    return mmap (addr, length, prot, flags, fi, offset);
}


void* HierarchicalDataPlane::mmap(void *addr, size_t length, int prot, int flags, FileInfo* fi, off_t offset){
    void* res;
    if(profiler) {
        TimeSubmission ts;
        ts.set_start();
        res = base_mmap(addr, length, prot, flags, fi, offset);
        ts.set_end();
        profiler->submit_client_read_time(ts);
        if(metadata_container->is_train_file(fi->get_name())) {
            check_epoch_finish(length);
        }
    }else{
        res = base_mmap(addr, length, prot, flags, fi, offset);
    }
    return res;
}

void* HierarchicalDataPlane::base_mmap(void *addr, size_t length, int prot, int flags, FileInfo* fi, off_t offset){
    if(offset >= fi->_get_size()) {
        if(debug_logger->is_activated())
            debug_write("Tried to mmap from offset: " + std::to_string(offset) + " which goes beyond file size: " +
            std::to_string(fi->_get_size()) + " name: " + fi->get_name());
        //TODO careful with this return !!!!
        return (void *) -1;
    }

    //it's crucial that we always use this storage_level "snapshot" since it can be changes in parallel
    int storage_level = fi->get_storage_level();
    size_t requested_size = length + offset > fi->_get_size() ? fi->_get_size() - offset : length;

    if(debug_logger->is_activated())
        debug_write("client reading from level " +
        std::to_string(storage_level) +
        " file " + fi->get_name() +
        " with offset: " +
        std::to_string(offset) + " and size: " + std::to_string(requested_size));

    //Open check phase
    if(!transparent_api && fi->has_shareable_file_descriptors() && is_file_system(storage_level)){
        check_open_phase(fi, storage_level);
    }
    Status<void*> read_status = nullptr;
    if(control_handler->uses_async_calls()){
        read_status = mmap_from_storage(addr, length, prot, flags, fi, offset, storage_level);
        //Started reading must be called after read so that the first read opens de file if fs driver.
        if(read_status.state == SUCCESS) {
            if(control_handler->check_placement_validity(dynamic_cast<PlacedFileInfo*>(fi))){
                //Since mmap does not do any I/O so Monarch brings the entire sample
                auto* f = new File(fi, offset, 1);
                get_fs_driver(storage_level)->open_descriptor(fi, O_RDWR, false, false);
                control_handler->place(f);
            }
        }
    }else{
        if(control_handler->check_placement_validity(dynamic_cast<PlacedFileInfo*>(fi))){
            //Monarch brings the entire sample synchronously
            auto* f = new File(fi, offset, 1);
            control_handler->place(f);
            //update variable with new storage
            storage_level = fi->get_storage_level();
        }
        read_status = mmap_from_storage(addr, length, prot, flags, fi, offset, storage_level);
    }
    //TODO this mechanism needs to be added with prefetch + relaxed read
    //Close check phase. Transparent uses the close method for this.
    if(fi->has_shareable_file_descriptors() && !transparent_api && is_file_system(storage_level)){
        check_close_phase(fi, requested_size, offset, storage_level);
    }
    return read_status.return_value;
}

Status<void*> HierarchicalDataPlane::mmap_from_storage(void *addr, size_t length, int prot, int flags, FileInfo* fi, off_t offset, int level){
    Status<void*> s = NOT_FOUND;
    if(profiler){
        StatSubmission ss;
        ss.set_start();
        s = base_mmap_from_storage(addr, length, prot, flags, fi, offset, level);
        ss.set_end();
        ss.set_n_bytes(length);
        profiler->submit_storage_client_read(ss, level);
    }else{
        s = base_mmap_from_storage(addr, length, prot, flags, fi, offset, level);
    }
    return s;
}

Status<void*> HierarchicalDataPlane::base_mmap_from_storage(void *addr, size_t length, int prot, int flags, FileInfo* fi, off_t offset, int level){
    Status<void*> s = NOT_FOUND;
    int found_level = level;
    while(s.state == NOT_FOUND && found_level < storage_hierarchy_size){
        if(is_file_system(found_level)){
            s = get_fs_driver(found_level)->passthrough_lib_mmap(addr, length, prot, flags, fi->get_file_descriptor(level), offset);
            if(s.return_value == MAP_FAILED){
                s.state = NOT_FOUND;
                if(debug_logger->is_activated()){
                    debug_write("MAP_FAILED. errno: "  + std::string(std::strerror(errno)) +  ". on file: " + fi->get_name()
                    + " fd: " + std::to_string(fi->get_file_descriptor(level)));
                }
            }
        }else{
            //TODO dont know if this works
            char* ptr;
            get_driver(found_level)->read(fi, ptr, offset, length, false);
            s = {ptr};
        }
        if(s.state == NOT_FOUND)
            found_level += 1;
    }
    if(s.state == NOT_FOUND){
        std::cerr << "File : " << fi->get_name() << " could not be found anywhere. Requested level: " + std::to_string(level) + " max level permitted: " +
        std::to_string(found_level - 1) + ". Level 0 descriptor: " + std::to_string(fi->get_file_descriptor(0)) + ". Level 1 descriptor: " + std::to_string(fi->get_file_descriptor(1)) +  ". Exiting...\n";
        exit(1);
    }

    if (found_level != level) {
        s = MISS;
        //f->get_info()->set_storage_level(found_level);
        if(debug_logger->is_activated())
            debug_write("Cache miss. Target level: " + std::to_string(level) + " Actual level: " + std::to_string(found_level));
    }
    return s;
}

Status<ssize_t> HierarchicalDataPlane::write(File* f, int level){
    Status<ssize_t> s = SUCCESS;
    if(profiler){
        StatSubmission write_submission;
        write_submission.set_start();
        s = storage_hierarchical_matrix[matrix_index][level]->write(f);
        write_submission.set_end();
        write_submission.set_n_bytes(f->get_requested_size());
        profiler->submit_storage_write(write_submission, level);
    }else{
        s = storage_hierarchical_matrix[matrix_index][level]->write(f);
    }
    return s;
}

Status<ssize_t> HierarchicalDataPlane::remove(FileInfo* fi, int level){
    return storage_hierarchical_matrix[matrix_index][level]->remove(fi);
}

File* HierarchicalDataPlane::remove_for_copy(FileInfo* fi, int level){
    return storage_hierarchical_matrix[matrix_index][level]->remove_for_copy(fi);
}

int HierarchicalDataPlane::get_target_class_from_id(int id) {
    return metadata_container->get_metadata(id)->get_target();
}

size_t HierarchicalDataPlane::get_file_size(const std::string &filename){
    return metadata_container->get_metadata(filename)->_get_size();
}

size_t HierarchicalDataPlane::get_file_size_from_id(int id){
    return metadata_container->get_metadata(id)->_get_size();
}

int HierarchicalDataPlane::get_target_class(const std::string &filename){
    return metadata_container->get_metadata(filename)->get_target();
}

void HierarchicalDataPlane::check_epoch_finish(size_t n){
    std::unique_lock<std::mutex> ul(epoch_change_mutex);
    epoch_size_signal += n;
    //std::cout << "EPOCH " << epoch_size_signal << " , " << metadata_container->get_train_full_size() << std::endl;
    if(epoch_size_signal == metadata_container->get_train_full_size()){
        profiling_service->signal_finished_epoch();
        epoch_size_signal = 0;
    }
}

ctpl::thread_pool* HierarchicalDataPlane::t_pool(int storage_index){
    if (!shared_thread_pool)
        return storage_hierarchy_thread_pools[storage_index];
    return storage_hierarchy_thread_pools[0];
}

int HierarchicalDataPlane::get_storage_hierarchical_matrix_index(int rank_, int worker_id_){
    return worker_id_ + rank_ * num_workers;
}

//ignores worker_id for now
//TODO stability should be reached when all storage level are 99.99% full and not related to index.
//The if is kind of strange, but for now does no harm
int HierarchicalDataPlane::get_list_index(int rank_, int worker_id_, int local_index){
    if(local_index == metadata_container->get_file_count() - 1)
        reached_stability = true;
    return metadata_container->get_id(rank_, local_index);
}

bool HierarchicalDataPlane::is_file_system(int level){
    return storage_hierarchical_matrix[matrix_index][level]->file_system_type();
}

FileSystemDriver* HierarchicalDataPlane::get_fs_driver(int level){
    auto* driver = storage_hierarchical_matrix[matrix_index][level];
    if(driver->alloc_type()){
        return (FileSystemDriver*)(((AllocableDataStorageDriver*)driver)->get_base_storage_driver());
    }
    return (FileSystemDriver*)driver;
}

bool HierarchicalDataPlane::is_allocable(int rank_, int worker_id_, int level){
    int line = get_storage_hierarchical_matrix_index(rank_, worker_id_);
    return storage_hierarchical_matrix[line][level]->alloc_type();
}

bool HierarchicalDataPlane::is_allocable(int level){
    return storage_hierarchical_matrix[matrix_index][level]->alloc_type();
}

bool HierarchicalDataPlane::is_blocking(int level){
    auto* driver = storage_hierarchical_matrix[matrix_index][level];
    if(driver->alloc_type()){
        return ((AllocableDataStorageDriver*)driver)->blocking_type();
    }
    return false;
}

bool HierarchicalDataPlane::is_eventual(int level){
    auto* driver = storage_hierarchical_matrix[matrix_index][level];
    if(driver->alloc_type()){
        return ((AllocableDataStorageDriver*)driver)->eventual_type();
    }
    return false;
}

AllocableDataStorageDriver* HierarchicalDataPlane::get_alloc_driver(int rank_, int worker_id_, int level){
    int line = get_storage_hierarchical_matrix_index(rank_, worker_id_);
    return (AllocableDataStorageDriver*)storage_hierarchical_matrix[line][level];
}

AllocableDataStorageDriver* HierarchicalDataPlane::get_alloc_driver(int level){
    return (AllocableDataStorageDriver*)storage_hierarchical_matrix[matrix_index][level];
}

BlockingAllocableDataStorageDriver* HierarchicalDataPlane::get_blocking_alloc_driver(int level){
    return (BlockingAllocableDataStorageDriver*)storage_hierarchical_matrix[matrix_index][level];
}

DataStorageDriver* HierarchicalDataPlane::get_driver(int rank_, int worker_id_, int level){
    int line = get_storage_hierarchical_matrix_index(rank_, worker_id_);
    return storage_hierarchical_matrix[line][level];
}

DataStorageDriver* HierarchicalDataPlane::get_driver(int level){
    return storage_hierarchical_matrix[matrix_index][level];
}

//TODO does not count level 0 if it's in-memory type with prefetch
int HierarchicalDataPlane::free_level(int rank_, int worker_id_, FileInfo* fi, int offset){
    for(int i = offset; i < storage_hierarchy_size - 1; i++)
        if(is_allocable(rank_, worker_id_, i)){
            auto* driver = get_alloc_driver(rank_, worker_id_, i);
            if((type != "prefetch_enabled" || !driver->in_memory_type()) && !driver->becomesFull(fi))
                return i;
        }
    return -1;
}

//TODO more generic
int HierarchicalDataPlane::alloc_free_level(FileInfo* fi, int offset){
    for(int i = offset; i < storage_hierarchy_size - 1; i++) {
        if (get_alloc_driver(i)->conditional_allocate_storage(fi).state != STORAGE_FULL) {
            return i;
        }
    }
    return -1;
}

int HierarchicalDataPlane::free_level(FileInfo* fi, int offset){
    for(int i = offset; i < storage_hierarchy_size - 1; i++) {
        if (!get_alloc_driver(i)->becomesFull(fi)) {
            return i;
        }
    }
    return -1;
}

EventualAllocableDataStorageDriver* HierarchicalDataPlane::get_eventual_alloc_driver(int level){
    return (EventualAllocableDataStorageDriver*)storage_hierarchical_matrix[matrix_index][level];
}

int HierarchicalDataPlane::eventual_free_level(FileInfo* fi, int offset){
    for(int i = offset; i < storage_hierarchy_size - 1; i++) {
        if (!get_eventual_alloc_driver(i)->eventual_becomesFull(fi)) {
            return i;
        }
    }
    return -1;
}

bool HierarchicalDataPlane::enforce_rate_limit(){
    if(rate_limiter != nullptr)
        return rate_limiter->rate_limit();
    return false;
}

void HierarchicalDataPlane::enforce_rate_brake(int new_brake_id){
    std::cerr << "Method not available" << std::endl;
    exit(1);
    //if(rate_limiter != nullptr) {
     //   rate_limiter->pull_brake(new_brake_id);
    ///}
}

void HierarchicalDataPlane::enforce_rate_continuation(int new_brake_release_id){
    std::cerr << "Method not available" << std::endl;
    exit(1);
    //if(rate_limiter != nullptr)
      //  rate_limiter->release_brake(new_brake_release_id);
}

void HierarchicalDataPlane::apply_job_termination() {
    if(rate_limiter != nullptr)
        rate_limiter->apply_job_termination();
}

void HierarchicalDataPlane::apply_job_start(){
    if(rate_limiter != nullptr)
        rate_limiter->apply_job_start();
}

void HierarchicalDataPlane::await_termination(){
    if(rate_limiter != nullptr)
        rate_limiter->await_termination();
}

void HierarchicalDataPlane::set_total_jobs(int iter_size){
    if(rate_limiter != nullptr)
        rate_limiter->set_total_jobs(iter_size);
}

void HierarchicalDataPlane::init(bool transparent_api_){
    transparent_api = transparent_api_;
    worker_id = 0;
    rank = 0;
    instance_id = 0;
    matrix_index = 0;
    if(debug_logger->is_activated()){
        debug_write("Init called by PID: " + std::to_string(getpid()));
    }
    if(transparent_api){
        for(int i = 0; i < storage_hierarchy_size; i++){
            if(is_file_system(i)){
                get_fs_driver(i)->enable_transparent_api();
            }
        }
    }else{
        metadata_init();
    }
}

void HierarchicalDataPlane::metadata_init(){
    metadata_container->init();
    if(debug_logger->is_activated())
        debug_write("Metadata container initialized!");

    if(world_size > 1 || num_workers > 1) {
        metadata_container->make_partition(rank, world_size, worker_id, num_workers);
        if(debug_logger->is_activated())
            debug_write("Metadatada container has partitioned file count set to: " + std::to_string(metadata_container->get_file_count()));

        for(int w = 0; w < num_workers; w++){
            for(int r = 0; r < world_size; r++){
                for(int i = 0; i < storage_hierarchy_size - 1; i++){
                    if (is_allocable(r, w, i)) {
                        auto *driver = get_alloc_driver(r, w, i);
                        size_t total_size = driver->get_max_storage_size();
                        size_t new_size = std::floor(total_size / (num_workers * world_size));
                        driver->resize(new_size);
                    }
                }
            }
        }
        if(debug_logger->is_activated())
            debug_write("Allocable drivers max_storage_size have been resized according to the given partition");
    }

    if(instance_id == 0){
        auto dirs = metadata_container->get_dirs_for_environment();
        //ignore source level
        for(int i = 0; i < storage_hierarchy_size - 1; i++){
            (storage_hierarchical_matrix[matrix_index][i])->create_environment(dirs, true);
        }
        (storage_hierarchical_matrix[matrix_index][storage_hierarchy_size - 1])->create_environment(dirs, false);

        if(debug_logger->is_activated())
            debug_write("Instance 0 created storage environment");
    }

    control_handler->prepare_environment();

    //TODO take this out of here. logic in the wrong place
    if(storage_hierarchy_size > 1) {
        if (control_handler->get_placement_policy() == PUSH_DOWN) {
            size_t full_capacity = 0;
            for (int i = 0; i < storage_hierarchy_size - 1; i++) {
                if (is_allocable(i)) {
                    full_capacity += get_alloc_driver(i)->get_max_storage_size();
                }
            }
            becomes_full = metadata_container->get_full_size() > full_capacity;
        } else {
            becomes_full = metadata_container->get_full_size() > get_alloc_driver(0)->get_max_storage_size();
        }
        std::string b_f = becomes_full ? "true" : "false";
        if(debug_logger->is_activated())
            debug_write("First storage level becomes full: " + b_f );
    }

    is_training = true;
}

void HierarchicalDataPlane::start(){
    if(rank == -1) {
        debug_write("Adjusting rank...");
        rank = 0;
    }

    if(worker_id == -1){
        debug_write("Adjusting worker_id");
        worker_id = 0;
    }

    if(profiling_service){
        profiling_service->start();
    }

    /* TODO
    synchronization_thread_pool->push([this](int id){start_sync_loop(0);});
     */
}

//TODO use decent method -> print to file in home_dir
std::vector<std::string> HierarchicalDataPlane::configs() {
    std::vector<std::string> res;
    res.push_back("- HierarchicalModule\n");
    res.push_back("\t- hierarchy " + std::to_string(storage_hierarchy_size) + "\n");
    res.push_back("\t\thierarchy_size: " + std::to_string(storage_hierarchy_size) + "\n");
    std::string b = shared_thread_pool ? "true" : "false";
    res.push_back("\t\tshared_pool: " + b + "\n");
    res.push_back("\t\ttotal_used_threads: " + std::to_string(total_used_threads) + "\n");

    for(int i = 0; i < storage_hierarchy_size; i++){
        res.push_back("\t\t- storage_driver_" + std::to_string(i) + ":\n");
        if(!shared_thread_pool){
            res.push_back("\t\t\tthread_pool_size: " + std::to_string(storage_hierarchy_thread_pools[i]->size()) + "\n");
        }

        auto* driver = storage_hierarchical_matrix[matrix_index][i];
        for(const auto& str : driver->configs())
            res.push_back("\t\t\t" + str);
    }

    //for(const auto& str : metadata_container->configs())
      //  res.push_back("\t" + str);

    return res;
}

void HierarchicalDataPlane::print(){
    if(instance_id == 0)
        for (const auto &str : configs())
            std::cout << str;
}

int HierarchicalDataPlane::get_file_count(){
    return metadata_container->get_file_count();
}

void HierarchicalDataPlane::make_blocking_drivers(){
    for(int i = 0; i < storage_hierarchy_size - 1; i++){
        if(is_allocable(i)){
            storage_hierarchical_matrix[matrix_index][i] = new BlockingAllocableDataStorageDriver(get_alloc_driver(i));
        }
    }
}

void HierarchicalDataPlane::make_eventual_drivers(){
    for(int i = 0; i < storage_hierarchy_size - 1; i++){
        if(is_allocable(i)){
            storage_hierarchical_matrix[matrix_index][i] = new EventualAllocableDataStorageDriver(get_alloc_driver(i));
        }
    }
}

void HierarchicalDataPlane::debug_write(const std::string& msg){
    debug_logger->_write("[HierarchicalDataPlane] " + msg);
}

void HierarchicalDataPlane::set_distributed_params(int rank_, int worker_id_){
    HierarchicalDataPlane::rank = rank_;
    HierarchicalDataPlane::worker_id = worker_id_;
    HierarchicalDataPlane::matrix_index = get_storage_hierarchical_matrix_index(rank, worker_id);
}

void HierarchicalDataPlane::start_sync_loop(int offset){
    if (world_size == 1 && num_workers == 1)
        return;
    while(placed_samples != metadata_container->get_iter_size()) {
        sleep(storage_sync_timeout);
        synchronize_storages(offset);
    }
}

//TODO doesn't support in-memory type due to eviction policy
//Its thread since the housekeeper does not interact with the neighbours storages
void HierarchicalDataPlane::synchronize_storages(int offset){
    if(!reached_stability){
        for(int i = 0; i < placed_samples; i++){
            for(int r = 0; r < world_size; r++){
                for(int w = 0; w < num_workers; w++){
                    //dont update your own storage
                    if(get_storage_hierarchical_matrix_index(r, w) != matrix_index) {
                        int index = get_list_index(r, w, neighbours_index);
                        auto *fi = metadata_container->get_metadata_from_ordered_id(r, index);
                        if (fi->get_storage_level() == storage_hierarchy_size - 1) {
                            int level = free_level(r, w, fi, offset);
                            if (level > 0) {
                                get_alloc_driver(r, w, level)->allocate_storage(fi);
                                fi->loaded_to(level);
                            }
                        }
                    }
                }
            }
            neighbours_index++;
        }
        debug_write("During synchronization neighbours_index was updated to:" + std::to_string(neighbours_index));
    }
}

/*//TODO only possible if char** is passed as argument
 * if(read_status.state == SUCCESS) {
        int level = free_level(f->get_info());
        if (level >= 0){
            memcpy(result, f->get_content(), f->get_requested_size());
            //result is in buffer, writter thread will manage the created file deletion
            housekeeper_thread_pool->push([this, f](int id) { handle_placement(f); });
        }else
            result = f->get_content();
    }
 */
