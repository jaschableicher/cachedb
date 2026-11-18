#include "uds.h"

constexpr int NUM_WORKERS = 4;

//TODO: Test on home device with different configuration!

#include <fcntl.h>
#include <poll.h>



UDS::UDS(Database& db): db_(db), is_running_(true){
    const char* path = "/tmp/cachedb.sock";
    unlink(path); // Remove stale socket file if it exists

    //initialize tcp server with a port to listen to
    socket_=socket(AF_UNIX, SOCK_STREAM, 0);//SOCK_DGRAM for udp

    if(socket_ <0){
        throw std::runtime_error("UDS Server not able to be started");
        return;
    }

    
    server_addr.sun_family = AF_UNIX;
    std::strncpy(server_addr.sun_path, path, sizeof(server_addr.sun_path) - 1);

    if (bind(socket_, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        throw std::runtime_error("Cannot bind to UDS socket " + std::string(path));
        return;
    }

     if (listen(socket_, SOMAXCONN) < 0) {
        throw std::runtime_error("Listening to UDS " + std::string(path) + " failed");
        return;
    }
    set_nonblocking(socket_);

    for (int i = 0; i < NUM_WORKERS; ++i) {
        auto worker = std::make_unique<ClientWorker>(db_);
        worker->start();
        workers.push_back(std::move(worker));
        std::cout << "Started worker " << i << std::endl;
    }

}

UDS::~UDS(){ 
    stop();
   
    if (epoll_fd >= 0) {
        ::close(epoll_fd);
    }
    if (socket_ >= 0) {
        ::shutdown(socket_, SHUT_RDWR);
        ::close(socket_);
        socket_ = -1;
    }

    std::cout << "Server completely stopped.\n";
}

void UDS::stop(){
    is_running_.store(false);
}

void UDS::run(){
    // Preserve a stop request made before this thread started.
    if (!is_running_.load()) return;


    
    while(is_running_.load()){
        pollfd listener{socket_, POLLIN, 0};
        int ready = poll(&listener, 1, 100);
        if (ready == 0) continue; // Periodically check for a stop request.
        if (ready < 0) {
            if (errno == EINTR) continue;
            std::cerr << "Poll failed: " << std::strerror(errno) << '\n';
            break;
        }
        if (!is_running_.load()) break;
        if (!(listener.revents & POLLIN)) break;

         int client_fd = accept(socket_, nullptr, nullptr);
        if (client_fd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) continue;
            std::cerr << "Accept failed: " << std::strerror(errno) << '\n';
            continue;
        }

        // 4. Assign the new client to the next worker in a round-robin fashion
        workers[next_worker]->add_client(client_fd);
        next_worker = (next_worker + 1) % NUM_WORKERS;
    }
    for (auto& worker : workers) {
        worker->join();
    }
}
