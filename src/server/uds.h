#ifndef UDS_H
#define UDS_H
#include "database/database.h"
#include "commands/executor.h"
#include <unistd.h>
#include <cstdint>
#include <arpa/inet.h>   // htons, htonl, inet_pton
#include <cerrno>        // errno, EINTR
#include <cstring>       // std::strerror
#include <sys/socket.h>  // socket, bind, sendto, recvfrom, setsockopt
#include <sys/un.h>      // sockaddr_un
#include <utility>
#include <stdexcept>
#include <iostream>
#include <cerrno>
#include <thread>
#include <vector>
#include <sys/epoll.h>
#include "utils.h"
#include "client_pool.h"
//Switch to singleton? as usually only one server or not?
//Running TCP server which simply accepts commands
class UDS{
public:
    explicit UDS(Database& db);
    ~UDS();
    // Run once. Stop and join the calling thread before destroying the server.
    void run();
    void stop();
private:
    Database& db_;
    std::atomic<bool> is_running_;

    int socket_=-1;
    int epoll_fd = -1;
    struct sockaddr_un server_addr;
    const char* path = "/tmp/cachedb.sock";

    std::vector<std::unique_ptr<ClientWorker>> workers;
    size_t next_worker = 0;
};


#endif
