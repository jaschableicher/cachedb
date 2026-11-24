#pragma once
#include <atomic>
#include <poll.h>

#include "database/database.h"
#include "client_pool.h"
constexpr int NUM_WORKERS = 4;

class Server{
public:
    Server(Database& db, bool is_running = true) 
        : db_(db), is_running_(is_running) {}
    void run();
    void stop(){
        is_running_.store(false);
    };

protected:
    Database& db_;
    std::atomic<bool> is_running_;
    int socket_=-1;
    int epoll_fd = -1;
    std::vector<std::unique_ptr<ClientWorker>> workers;
    size_t next_worker = 0;
};