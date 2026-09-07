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
//How to do this as the direct logging could slow down the returns of set command significantly. 
// Idea is to have a simple list to append to which is read out for then to be writen to file asap
// but as a thread
void Logger::log_command(std::string& line){
    //Check here whether to log or not!
    //For now simply only del and set
    
    
    //TODO: later maybe via registry to have a simple log bool to check the command there via e.g. Registry::get_instance()->should_log(std::string& command);
    if(!line.starts_with("DEL") && !line.starts_with("SET")) return;

    std::scoped_lock lock(data_lock_);
    active_buffer_.append(line);
    if (line.empty() || line.back() != '\n') active_buffer_.append("\n");

}
void Logger::log_buffer(){
     {
        std::scoped_lock lock(data_lock_);
        std::swap(active_buffer_,background_buffer_);
    }
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
    std::ifstream file("database.aof");
    if (!file) {
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        execute_command(db, line, false);
    }
}
