#ifndef SERVER_H
#define SERVER_H
#include "database/database.h"
#include "commands/executor.h"
#include <cstdint>
#include <arpa/inet.h>   // htons, htonl, inet_pton

#include <cerrno>
#include <sys/epoll.h>
#include "utils.h"
#include "client_pool.h"
#include "server_interface.h"

class TCP: public Server{
public:
    explicit TCP(Database& db);
    ~TCP();
private:
    uint16_t tcp_port_ = 5634; //standard port
    struct sockaddr_in server_addr;
};


#endif
