#include <iostream>
#include "database/database.h"
#include "executor.h"
#include "server/server.h"
#include "logger/logger.h"
#include <thread>
#include <csignal>
#include <atomic>

void register_commands();

void cli(Database& db){
    
    

    std::string line;

    while (true) {
        std::cout << "> ";
        if (!std::getline(std::cin, line))
            break;

        if (line == "EXIT")
            break;

        auto result = execute_command(db, line,true);

        std::cout << result << '\n';
    }
}
std::atomic<bool> g_running{true};

void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        g_running = false;
    }
}

int main() {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    register_commands();
    Database db;
    TCPServer server(db);
    //replay database.aof cache
    //on a clean exit the last command on database.aof must be SAVE for the datadump
    //If no SAVE existed, the programm crashed so recover the latest datadump and then from the last SAVE replay the commands
    //Then if
   // Logger::get_instance()->replay_commands(db);
    std::thread server_thread(&TCPServer::run, &server);

    while (g_running.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
  
   server.stop();
   if (server_thread.joinable()) {
        server_thread.join();  // Wait for the worker thread to finish
    }
   Logger::destroy_instance();
   return 0;
}
