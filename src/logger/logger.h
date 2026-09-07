#ifndef LOGGER_H
#define LOGGER_H
#include <string>
#include <fcntl.h> 
#include <unistd.h>
#include <iostream>
#include <cstring>
#include <mutex>
#include <thread>
#include <atomic>
#include "database/database.h"
#include "value.h"
#include "command.h"

class Logger{
public:
    static Logger* get_instance(){
        if(instance_==nullptr){
            instance_=new Logger();
        }
        return instance_;
    }
    void log_command(std::string& line);
    static void destroy_instance(){
        delete instance_;
        instance_ = nullptr;
    }
    void replay_commands(Database& db);
private:
    Logger();
    ~Logger();
    void log_buffer();
    void log_buffer_thread();
    std::thread log_thread_;
    std::atomic<bool> running_;
    static Logger* instance_;
    int aol_fd_;
    std::mutex data_lock_;

    std::string active_buffer_;
    std::string background_buffer_;
};
#endif
