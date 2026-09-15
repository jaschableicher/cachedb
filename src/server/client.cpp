#include "server.h"
bool is_http_request(const char* buffer, size_t bytes_read) {
    if (bytes_read < 4) return false;
    
    // Check for common HTTP verbs
    return (std::strncmp(buffer, "GET ", 4) == 0 ||
            std::strncmp(buffer, "POST ", 5) == 0 ||
            std::strncmp(buffer, "HEAD ", 5) == 0 ||
            std::strncmp(buffer, "OPTIONS ", 8) == 0);
}
bool TCPServer::handle_new_client(){
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    // Accept the connection (non-blocking is highly recommended here)
    int new_client = accept(socket_, (struct sockaddr*)&client_addr, &client_len);
    if (new_client == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return true; //pending connections
        perror("accept failed");
        return false;
    }

    // Register this NEW client socket with epoll to monitor it for data
    set_nonblocking(new_client);
    struct epoll_event client_ev;
    client_ev.events = EPOLLIN;         // Trigger when client sends data
    client_ev.data.fd = new_client;     // Save the client FD

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, new_client, &client_ev) < 0) {
        close(new_client);
        return false;
    }
    // Track idle clients too, so destruction closes every accepted socket.
    message_pool.try_emplace(new_client);
    return false;
}

void TCPServer::handle_data(int client_fd){
    char buffer[1024];
    
    std::memset(buffer, 0, sizeof(buffer));
    ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1,0);

    // Client closed connection or error occurred
    if (bytes_read <= 0) {
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
        close(client_fd);
        message_pool.erase(client_fd);
        return;
    }
    if (is_http_request(buffer, bytes_read)) {
        const std::string http_response =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plain; charset=utf-8\r\n"
            "Content-Length: 2\r\n"
            "Connection: close\r\n"
            "\r\n"
            "Cache OK";

        send(client_fd, http_response.c_str(), http_response.length(), 0);
        close(client_fd); // Close immediately so browser finishes loading
        return;
    }
    message_pool[client_fd].append(buffer, bytes_read);
    if(buffer[bytes_read-1]!='\n'){
        return;
    }          
    
    std::string msg = message_pool[client_fd];
    //Worker thread made p50 quite a bit worse however p999 was way way better
    //FROM:  Requests: 50'000 Connections: 1'000 Throughput: 64'963 req/s Latency: p50: 4.52 ms p95: 10.26 ms p99: 177.51 ms p999: 362.86 ms
    //  TO:  Requests: 50'000 Connections: 1'000 Throughput: 69'242 req/s Latency: p50: 11.96 ms p95: 15.47 ms p99: 17.01 ms p999: 22.45 ms
    //In general still WAAY to slow, p50 MUST be far below 1ms
    worker.push(Task{db_, client_fd, std::move(msg)});
    message_pool[client_fd].clear();

}
