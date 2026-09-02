//When running for now simply takes in a line which is brought in, executes the command via executor and sends the result back, that is it
//For now only one connection possible!
#include "server.h"


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
        throw std::runtime_error("Bind failed");
        return;
    }

     if (listen(socket_, 3) < 0) {
        throw std::runtime_error("Listen failed");
        return;
    }
}

TCPServer::~TCPServer(){ 

    stop();
}

void TCPServer::stop(){
    
    is_running_=false;
    if (socket_ >= 0) {
        ::shutdown(socket_, SHUT_RDWR);
        ::close(socket_);
        socket_ = -1;
    }
    for(int client_fd : client_fds_) {
        close(client_fd); //unblocks recv
    }
    std::cout << "Waiting for " << client_threads_.size() << " clients to disconnect...\n";
    client_threads_.clear(); 
    std::cout << "Server completely stopped.\n";
}

void TCPServer::run(){
    is_running_=true;
    while(is_running_){

        std::erase_if(client_threads_, [](std::jthread& t) {
            // If a jthread cannot request a stop, it means it already finished
            return !t.get_stop_source().stop_possible();
        });

        socklen_t client_len = sizeof(client_addr);

        std::cout << "Waiting for a client to connect...\n";
        int new_client= accept(socket_, (struct sockaddr*)&client_addr, &client_len);
        if (new_client < 0) {
            if (!is_running_) {
                std::cout << "Server stopping, accept unblocked cleanly.\n";
                break; 
            }
            std::cerr << "Accept failed\n";
            continue; // Retry accepting next client
        }
        //Add new thread
        client_fds_.push_back(new_client); // Track it so stop() can close it!
    
        client_threads_.emplace_back([this](std::stop_token stoken, int client_fd) {
            this->handle_client(stoken, client_fd);
        }, new_client);
    }
   
}

void TCPServer::handle_client(std::stop_token stoken,int client){
    std::cout <<"Client connected" << std::endl;
    char buffer[1024];
    std::string msg;
    while (!stoken.stop_requested()) {
        std::memset(buffer, 0, sizeof(buffer));
        ssize_t bytes_read = recv(client, buffer, sizeof(buffer) - 1,0);

        // Client closed connection or error occurred
        if (bytes_read <= 0) {
            std::cout << "Client disconnected.\n";
            break; // Break inner loop to accept next client
        }
        msg+=buffer;
        if(buffer[bytes_read-1]!='\n'){
            continue;
        }            
        std::string reply = std::string(execute_command(db_, msg) + "\n");
        send(client, reply.c_str(), std::strlen(reply.c_str()),0);
        msg.clear();
    }
    // Cleanup only the client socket; 
    close(client);
}