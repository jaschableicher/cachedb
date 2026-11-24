#include "server.h"



void Server::run(){
    // Preserve a stop request made before this thread started.
    if (!is_running_.load()) return;


    
    while(is_running_.load()){
        pollfd listener{socket_, POLLIN, 0};
        int ready = poll(&listener, 1, 100);
        if (ready == 0) continue; // Periodically check for a stop request.
        if (ready < 0) {
            if (errno == EINTR) continue;
            std::cerr << "Poll failed: " << std::strerror(errno) << '\n';
            break;
        }
        if (!is_running_.load()) break;
        if (!(listener.revents & POLLIN)) break;

         int client_fd = accept(socket_, nullptr, nullptr);
        if (client_fd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) continue;
            std::cerr << "Accept failed: " << std::strerror(errno) << '\n';
            continue;
        }

        // 4. Assign the new client to the next worker in a round-robin fashion
        workers[next_worker]->add_client(client_fd);
        next_worker = (next_worker + 1) % NUM_WORKERS;
    }
    for (auto& worker : workers) {
        worker->join();
    }
}
