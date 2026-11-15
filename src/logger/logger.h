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
#include "registry.h"



class LogReplayer{
public:
    LogReplayer(std::string filename,CommandContext& context);
    ~LogReplayer();
    void replay_commands();
private:
    template<typename T>
    T read_raw(std::istream& is) {
        T val;
        is.read(reinterpret_cast<char*>(&val), sizeof(T));
        return val;
    }

    std::string read_string(std::istream& is);
    Bytes read_bytes(std::istream& is);
    Value deserialize_value(std::istream& is);

    std::ifstream file;
    std::string filename_;
    CommandContext context_;
};

class Logger{
public:
    static Logger* get_instance(){
        if(instance_==nullptr){
            instance_=new Logger();
        }
        return instance_;
    }
    void log_command(const Command& command);
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

    template<typename T>
    void append_raw(const T& val) {
        const char* ptr = reinterpret_cast<const char*>(&val);
        binary_commands.insert(binary_commands.end(), ptr, ptr + sizeof(T));
    }
    void append_string(const std::string& str);
    void serialize_value(const Value& val);
    void append_bytes(const Bytes& bytes);
    std::thread log_thread_;
    std::atomic<bool> running_;
    static Logger* instance_;
    int aol_fd_;
    std::mutex data_lock_;

    
    std::vector<Command> active_buffer_;
    std::vector<Command> background_buffer_;
    std::vector<char> binary_commands;
    bool replaying_=false;
};


#endif
