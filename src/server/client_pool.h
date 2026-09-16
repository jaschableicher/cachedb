#pragma once
#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <queue>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <cstring>
#include <thread>
#include "commands/command_worker.h"
#include "commands/utils.h"

class ClientWorker{
public:
    ClientWorker(Database& db);
    ~ClientWorker();
    void start();
    void join();
    void add_client(int client_fd);
private:
    void run();
    void handle_new_clients();
    void handle_client_data(int client_fd);

    CommandWorker worker;
    int epoll_fd_ = -1;
    int event_fd_ = -1;
    std::thread thread_;
    std::queue<int> client_queue_;
    std::mutex queue_mutex_;
    std::atomic<bool> running_;
    std::unordered_map<int,std::string> message_pool_;
    Database& db_;
};