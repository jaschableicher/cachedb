#include "server.h"

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