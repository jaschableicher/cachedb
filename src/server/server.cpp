//When running for now simply takes in a line which is brought in, executes the command via executor and sends the result back, that is it
//For now only one connection possible!
#include "server.h"
int MAX_CLIENTS=6000;
constexpr int NUM_WORKERS = 4;
///4095 is a hard limit at least on the wsl company device
//TODO: Test on home device with different configuration!

#include <fcntl.h>
#include <poll.h>



TCPServer::TCPServer(Database& db): db_(db), is_running_(true){
    //initialize tcp server with a port to listen to
    socket_=socket(AF_INET, SOCK_STREAM, 0);//SOCK_DGRAM for udp
    //TODO: Error Handling
    if(socket_ <0){
        throw std::runtime_error("TCP Server not able to be started");
        return;
    }
    //Configure tcp_server
    int opt = 1;
    setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(tcp_port_);
    if (bind(socket_, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        //TODO: Error Handling
        throw std::runtime_error("Cannot bind to Port: " +  std::to_string(tcp_port_));
        return;
    }

     if (listen(socket_, 65535) < 0) {
        throw std::runtime_error("Listening to port " + std::to_string(tcp_port_) + " failed");
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

TCPServer::~TCPServer(){ 
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

void TCPServer::stop(){
    is_running_.store(false);
}

void TCPServer::run(){
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
