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



void ClientWorker::handle_client_data(int client_fd){
    char incoming[4096];

    auto disconnect = [&]{
        epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, client_fd, nullptr);
        message_pool_.erase(client_fd);
        close(client_fd);
    };
     
    const ssize_t received = recv(client_fd, incoming, sizeof(incoming), 0);

    if (received == 0) {
        disconnect();
        return;
    }

    if (received < 0) {
        if (errno == EINTR || errno == EAGAIN ||errno == EWOULDBLOCK) {
            return;
        }
        disconnect();
        return;
    }
    auto& input = message_pool_[client_fd];
    input.append(incoming, static_cast<std::size_t>(received));
    while (input.size() >= 4) {
        const auto* header = reinterpret_cast<const unsigned char*>(input.data());
        const uint32_t payload_size =
            (static_cast<uint32_t>(header[0]) << 24) |
            (static_cast<uint32_t>(header[1]) << 16) |
            (static_cast<uint32_t>(header[2]) << 8)  |
             static_cast<uint32_t>(header[3]);

        if (payload_size > MAX_MESSAGE_SIZE) {
            disconnect();
            return;
        }
        const std::size_t frame_size = 4 + payload_size;

        // The remaining payload will arrive in a later recv().
        if (input.size() < frame_size){
            return;
        }
        Value reply;
        try{
            Command command = parse_command(input.data()+4, payload_size);

            reply = execute_command(db_,command);
        }catch(const std::exception& error){
            reply= std::string("ERR protocol: ") + error.what();
        }
        input.erase(0, frame_size);//remove the frame

        msgpack::v1::sbuffer response_payload = protocol::encode_value(reply);
        std::vector<char> response_frame = protocol::frame_payload(response_payload);
        std::size_t sent_total = 0;
        while (sent_total < response_frame.size()) {
            const ssize_t sent = send(client_fd, response_frame.data() + sent_total,response_frame.size() - sent_total, MSG_NOSIGNAL);

            if (sent < 0 && errno == EINTR)
                continue;

            if (sent <= 0) {
                disconnect();
                return;
            }

            sent_total += static_cast<std::size_t>(sent);
        }
    }
} 