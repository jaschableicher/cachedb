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
    // Run once. Stop and join the calling thread before destroying the server.
    void run();
    void stop();
private:
    Database& db_;
    std::atomic<bool> is_running_;

    int socket_=-1;
    int epoll_fd = -1;
    uint16_t tcp_port_ = 5634; //standard port
    struct sockaddr_in server_addr;


    std::unordered_map<int,std::string> message_pool;//<client_fd,msg>
    bool handle_new_client();
    void handle_data(int client_fd);
    void set_nonblocking(int fd);
};


#endif
