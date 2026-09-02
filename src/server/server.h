#ifndef SERVER_H
#define SERVER_H
#include "database/database.h"
#include "commands/executor.h"
#include <unistd.h>
#include <cstdint>
#include <arpa/inet.h>   // htons, htonl, inet_pton
#include <cerrno>        // errno, EINTR
#include <cstring>       // std::strerror
#include <sys/socket.h>  // socket, bind, sendto, recvfrom, setsockopt
#include <utility>
#include <stdexcept>
#include <iostream>
#include <cerrno>
#include <thread>
#include <vector>
#include <sys/epoll.h>


//Switch to singleton? as usually only one server or not?
//Running TCP server which simply accepts commands
class TCPServer{
public:
    explicit TCPServer(Database& db);
    ~TCPServer();
    void run();
    void stop();
private:
    Database& db_;
    bool is_running_;

    int socket_=-1;
   
    uint16_t tcp_port_ = 5634; //standard port
    struct sockaddr_in server_addr;
    std::vector<std::jthread> client_threads_;
    std::vector<int> client_fds_;

};


#endif