//When running for now simply takes in a line which is brought in, executes the command via executor and sends the result back, that is it
//For now only one connection possible!
#include "server.h"
int MAX_CLIENTS=6000;
///4095 is a hard limit at least on the wsl company device
//TODO: Test on home device with different configuration!

#include <fcntl.h>

void set_nonblocking(int fd) {
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

bool TCPServer::handle_new_client(){
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    // Accept the connection (non-blocking is highly recommended here)
    int new_client = accept(socket_, (struct sockaddr*)&client_addr, &client_len);
    if (new_client == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return true; //pending connections
        perror("accept failed");
        return false;
    }

    // Register this NEW client socket with epoll to monitor it for data
    set_nonblocking(new_client);
    struct epoll_event client_ev;
    client_ev.events = EPOLLIN;         // Trigger when client sends data
    client_ev.data.fd = new_client;     // Save the client FD

    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, new_client, &client_ev);
    return false;
}

void TCPServer::handle_data(int client_fd){
    char buffer[1024];
    
    std::memset(buffer, 0, sizeof(buffer));
    ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1,0);

    // Client closed connection or error occurred
    if (bytes_read <= 0) {
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
        close(client_fd);
        message_pool.erase(client_fd);
        return;
    }
    message_pool[client_fd].append(buffer, bytes_read);
    if(buffer[bytes_read-1]!='\n'){
        return;
    }            
    std::string reply = std::string(execute_command(db_, message_pool[client_fd]) + "\n");
    send(client_fd, reply.c_str(), std::strlen(reply.c_str()),0);
    message_pool[client_fd].clear();

}