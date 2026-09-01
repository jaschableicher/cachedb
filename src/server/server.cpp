//When running for now simply takes in a line which is brought in, executes the command via executor and sends the result back, that is it
//For now only one connection possible!
#include "server.h"


TCPServer::TCPServer(Database& db): db_(db){ 
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
    close(client_);//If a client exists 
    close(socket_);
}

void TCPServer::stop(){
    isRunning=false;
}

void TCPServer::run(){
    isRunning=true;
    while(isRunning){
        socklen_t client_len = sizeof(client_addr);

        std::cout << "Waiting for a client to connect...\n";
        client_= accept(socket_, (struct sockaddr*)&client_addr, &client_len);
        if (client_ < 0) {
            std::cerr << "Accept failed\n";
            continue; // Retry accepting next client
        }
        std::cout <<"Client connected" << std::endl;
        char buffer[1024];
        std::string msg;
        while (isRunning) {
            std::memset(buffer, 0, sizeof(buffer));
            ssize_t bytes_read = recv(client_, buffer, sizeof(buffer) - 1,0);

            // Client closed connection or error occurred
            if (bytes_read <= 0) {
                std::cout << "Client disconnected.\n\n";
                break; // Break inner loop to accept next client
            }
            msg+=buffer;
            if(buffer[bytes_read-1]!='\n'){
                continue;
            }            
            std::string reply = std::string(execute_command(db_, msg) + "\n");
            send(client_, reply.c_str(), std::strlen(reply.c_str()),0);
            msg.clear();
        }

        // Cleanup only the client socket; keep server_fd open
        close(client_);
    }
}