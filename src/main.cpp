#include <iostream>
#include "database/database.h"
#include "executor.h"
#include "server/server.h"
#include <thread>
void register_commands();

void cli(Database& db){
    
    

    std::string line;

    while (true) {
        std::cout << "> ";

        if (!std::getline(std::cin, line))
            break;

        if (line == "EXIT")
            break;

        auto result = execute_command(db, line);

        std::cout << result << '\n';
    }
}


int main() {
    register_commands();
    Database db;
    TCPServer server(db);
    std::thread server_thread(&TCPServer::run, &server);


   cli(db);
   server_thread.join();
}