#include "logger.h"
#include "commands/executor.h"
#include <cerrno>
#include <fstream>
#include <stdexcept>
Logger* Logger::instance_ = nullptr;
//Only need to log set and del commands(expired deletion counts as del!) as anything else is only look ups
Logger::Logger(){
    running_.store(true);
    // Preserve existing contents and append new entries; create only if missing.
    aol_fd_ = open("database.aof", O_WRONLY | O_CREAT | O_APPEND, 0644);

    if(aol_fd_==-1){
        throw std::runtime_error("Failed to open logger");
    }
    //reserver large enough strings to not need the programm to move around in memory
    //Not too large so they dont overtake the memory!
    //1MB of space each
    active_buffer_.reserve(1024 * 1024); 
    background_buffer_.reserve(1024 * 1024);
    log_thread_ = std::thread(&Logger::log_buffer_thread, this);
}
Logger::~Logger(){
    std::cout << "Deleting logger" <<std::endl;
    //Make sure no data is left in memory to log
    //close the logging thread and then the file
    running_.store(false);
    log_thread_.join();
    
    if(!active_buffer_.empty()){
        log_buffer();
    }
    close(aol_fd_);
}
 void Logger::append_bytes(const Bytes& bytes) {
    uint32_t len = static_cast<uint32_t>(bytes.size());
    append_raw(len);
    if (len > 0) {
        const char* ptr = reinterpret_cast<const char*>(bytes.data());
        active_buffer_.insert(active_buffer_.end(), ptr, ptr + len);
    }
}
void Logger::serialize_value(const Value& val) {
        // We use std::visit to handle the active type of the variant.
        // We map the active type to your exact ValueType enum.
        std::visit([this](const auto& arg) {
            using T = std::decay_t<decltype(arg)>;

            if constexpr (std::is_same_v<T, Null>) {
                append_raw(ValueType::Null);
                // Null/monostate has 0 bytes of payload
            }
            else if constexpr (std::is_same_v<T, bool>) {
                append_raw(ValueType::Bool);
                append_raw(arg);
            }
            else if constexpr (std::is_same_v<T, int64_t>) {
                append_raw(ValueType::Int64);
                append_raw(arg);
            }
            else if constexpr (std::is_same_v<T, uint64_t>) {
                append_raw(ValueType::UInt64);
                append_raw(arg);
            }
            else if constexpr (std::is_same_v<T, double>) {
                append_raw(ValueType::Double);
                append_raw(arg);
            }
            else if constexpr (std::is_same_v<T, std::string>) {
                append_raw(ValueType::String);
                append_string(arg);
            }
            else if constexpr (std::is_same_v<T, Bytes>) {
                append_raw(ValueType::Bytes);
                append_bytes(arg);
            }
            // Array, Map, and Extension are currently ignored as requested
        }, val);
    }


void Logger::append_string(const std::string& str) {
    uint32_t len = static_cast<uint32_t>(str.size());
    append_raw(len);
    if (len > 0) {
        active_buffer_.insert(active_buffer_.end(), str.begin(), str.end());
    }
}


void Logger::log_command(Command command){
    //Check here whether to log or not!
    //For now simply only del and set

    std::scoped_lock lock(data_lock_);
    if(replaying_) return; //Do not log replays!
    //Turn command into binary, append to buffer
    //Make the command

    append_string(command.name);
    uint32_t arg_count = static_cast<uint32_t>(command.args.size());
    append_raw(arg_count);

    for(const auto& arg:command.args){
        serialize_value(arg);
    }

}
void Logger::log_buffer(){
    
    {
        std::scoped_lock lock(data_lock_);
        std::swap(active_buffer_,background_buffer_);
    }
    if(background_buffer_.empty()) return;
    // Producers append to active_buffer_; write background_buffer_ outside the lock.
    std::size_t written = 0;
    while (written < background_buffer_.size()) {
        const ssize_t count = write(aol_fd_, background_buffer_.data() + written,
                                    background_buffer_.size() - written);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) {
            std::cerr << "Failed to write logger buffer" << std::endl;
            return;
        }
        written += static_cast<std::size_t>(count);
    }
    background_buffer_.clear();
    //Wait for line to be added to the disk
    if(fdatasync(aol_fd_)==-1){
        std::cerr << "Failed to write to disk" << std::endl;
    }
}

void Logger::log_buffer_thread(){
    //Copy data from buffer a to buffer b via pointers
    //as buffer b was empty, a is now empty as a is now b so data can be rewritten again
    while(running_.load()){
        log_buffer();
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}

void Logger::replay_commands(Database& db){
    //read in file line per line
    //then simply call executor for this
    // Read through a separate stream: the append descriptor is write-only.
    replaying_ = true;
    CommandContext context{db};

    LogReplayer replayer("database.aof",context);
  
    replayer.replay_commands();

    replaying_=false;
}