#include "uds.h"


//TODO: Test on home device with different configuration!

#include <fcntl.h>
#include <poll.h>



UDS::UDS(Database& db): Server(db, true){
    unlink(path); // Remove stale socket file if it exists

    //initialize tcp server with a port to listen to
    socket_=socket(AF_UNIX, SOCK_STREAM, 0);

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
    unlink(path);
    std::cout << "Server completely stopped.\n";
}

