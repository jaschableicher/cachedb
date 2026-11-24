#include "tcp_server.h"
#include <fcntl.h>
#include <netinet/tcp.h>
#include <poll.h>



TCP::TCP(Database& db): Server(db, true){
    //initialize tcp server with a port to listen to
    socket_=socket(AF_INET, SOCK_STREAM, 0);//SOCK_DGRAM for udp

    if(socket_ <0){
        throw std::runtime_error("TCP Server not able to be started");
        return;
    }
    //Configure tcp_server
    int opt = 1;
    setsockopt(socket_, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));
    
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(tcp_port_);
    if (bind(socket_, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        throw std::runtime_error("Cannot bind to Port: " +  std::to_string(tcp_port_));
        return;
    }

     if (listen(socket_, SOMAXCONN) < 0) {
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

TCP::~TCP(){ 
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

