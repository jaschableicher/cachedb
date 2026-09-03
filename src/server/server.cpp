//When running for now simply takes in a line which is brought in, executes the command via executor and sends the result back, that is it
//For now only one connection possible!
#include "server.h"
int MAX_CLIENTS=6000;
///4095 is a hard limit at least on the wsl company device
//TODO: Test on home device with different configuration!

#include <fcntl.h>

void TCPServer::set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

TCPServer::TCPServer(Database& db): db_(db), is_running_(false){ 
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
}

TCPServer::~TCPServer(){ 
    stop();
}

void TCPServer::stop(){
    
    is_running_.store(false);
    if (socket_ >= 0) {
        ::shutdown(socket_, SHUT_RDWR);
        ::close(socket_);
        socket_ = -1;
    }

    std::cout << "Server completely stopped.\n";
}

void TCPServer::run(){
    is_running_.store(true);

    struct epoll_event event, events[MAX_CLIENTS];
    event.events = EPOLLIN;
	event.data.fd = socket_;
	epoll_fd = epoll_create1(0);

	if (epoll_fd == -1) {
		std::cerr << "Failed to create epoll file descriptor\n";
		return;
	}
    if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, socket_, &event))
	{
		close(epoll_fd);
        std::cerr << "Failed to add file descriptor to epoll\n";
		return;
	}
	

    while(is_running_.load()){
        int event_count = epoll_wait(epoll_fd, events, MAX_CLIENTS, -1);//No timeout for now as it can be running without requests for a while!
        for (int i = 0; i < event_count; i++) {
            if (events[i].data.fd == socket_) {
                if(handle_new_client()) break;
                
            }
            else{
               handle_data(events[i].data.fd);
            }
        }
    }
   
}

