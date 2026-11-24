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
#include "server_interface.h"
//Switch to singleton? as usually only one server or not?
//Running TCP server which simply accepts commands
class UDS : public Server{
public:
    explicit UDS(Database& db);
    ~UDS();

private:
    struct sockaddr_un server_addr;
    const char* path = "/tmp/cachedb.sock";

};


#endif
