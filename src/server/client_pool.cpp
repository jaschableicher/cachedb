#include "client_pool.h"
#include <cerrno>
bool is_http_request(const char* buffer, size_t bytes_read) {
    if (bytes_read < 4) return false;
    
    // Check for common HTTP verbs
    return (std::strncmp(buffer, "GET ", 4) == 0 ||
            std::strncmp(buffer, "POST ", 5) == 0 ||
            std::strncmp(buffer, "HEAD ", 5) == 0 ||
            std::strncmp(buffer, "OPTIONS ", 8) == 0);
}
ClientWorker::ClientWorker(Database& db):db_(db){
    epoll_fd_ = epoll_create1(0);
    if(epoll_fd_==-1){
        throw std::runtime_error("Failed to create epoll_fd_ in worker");
    }

    event_fd_ = eventfd(0, EFD_NONBLOCK);
    if (event_fd_ == -1) {
        throw std::runtime_error("Failed to create eventfd in worker");
    }
    epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = event_fd_;
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, event_fd_, &event)) {
            throw std::runtime_error("Failed to add event_fd to epoll");
    }
    running_=true;
}
ClientWorker::~ClientWorker(){
    close(epoll_fd_);
    close(event_fd_);
}

void ClientWorker::start(){
    thread_=std::thread(&ClientWorker::run, this);
}

void ClientWorker::join(){
    running_.store(false);
    if(thread_.joinable()){
        // Wake epoll_wait so the worker can observe the stop flag.
        uint64_t val = 1;
        while (write(event_fd_, &val, sizeof(val)) == -1 && errno == EINTR) {}
        thread_.join();
    }
}

void ClientWorker::add_client(int client_fd){
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        client_queue_.push(client_fd);
    }
    uint64_t val = 1;
    write(event_fd_, &val, sizeof(val));
}

void ClientWorker::run(){
    epoll_event events[128];
    while(running_.load()){
        int event_count = epoll_wait(epoll_fd_,events,128,-1);
        if (!running_.load()) break;
         for (int i = 0; i < event_count; ++i) {
                if (events[i].data.fd == event_fd_) {
                    // This is the signal that we have a new client in the queue
                    uint64_t val;
                    read(event_fd_, &val, sizeof(val)); // Clear the eventfd
                    handle_new_clients();
                } else {
                    // This is data from an existing client
                    handle_client_data(events[i].data.fd);
                }
            }
    }
}

void ClientWorker::handle_new_clients() {
    int client_fd;
    std::lock_guard<std::mutex> lock(queue_mutex_);
    while(!client_queue_.empty()){
        client_fd = client_queue_.front();
        client_queue_.pop();

        epoll_event event;
        event.events = EPOLLIN;
        event.data.fd = client_fd;
        if(epoll_ctl(epoll_fd_,EPOLL_CTL_ADD,client_fd,&event)){
            std::cerr << "Failed to add client" << client_fd << "to epoll\n";
           
            close(client_fd);
        }
        message_pool_.try_emplace(client_fd);
    }
}


std::vector<std::string> split_keep_newline(std::string_view str) {
    std::vector<std::string> result;
    size_t start = 0;
    
    while (start < str.size()) {
        // Find the next newline character
        size_t pos = str.find('\n', start);
        
        if (pos == std::string_view::npos) {
            // No more newlines, grab the remaining chunk
            result.emplace_back(str.substr(start));
            break;
        }
        
        // Extract substring *including* the newline character (+ 1)
        size_t length = (pos - start) + 1;
        result.emplace_back(str.substr(start, length));
        
        // Move start pointer past the newline
        start = pos + 1;
    }
    
    return result;
}

void ClientWorker::handle_client_data(int client_fd){
    char buffer[1024];
    ssize_t bytes_read =  recv(client_fd, buffer, sizeof(buffer) - 1,  0);
    if(bytes_read <=0){
        //client disconnected
        epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, client_fd, nullptr);
        message_pool_.erase(client_fd);
        close(client_fd);
    }
    else{
        //split buffer at \n
        message_pool_[client_fd].append(buffer,bytes_read);
        std::vector<std::string> commands = split_keep_newline(message_pool_[client_fd]);
        message_pool_[client_fd].clear();
        for(int i = 0; i<commands.size();i++){
            if(i==commands.size()-1){
                if(commands[i][commands[i].size()-1]!='\n'){
                    message_pool_[client_fd].append(commands[i]);
                    return;
                }
            }
            std::string reply = execute_command(db_, commands[i], true) + "\n";

            ssize_t bytes_sent = send(client_fd, reply.c_str(), reply.length(), 0);
            if (bytes_sent < 0) {
                message_pool_.erase(client_fd);
                close(client_fd);
                return;
            }
        }

   
    
         
        message_pool_[client_fd].clear();      
    }
} 