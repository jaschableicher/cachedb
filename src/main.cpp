#include <iostream>
#include "database/database.h"
#include "executor.h"
#include "server/server.h"
#include "logger/logger.h"
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

        auto result = execute_command(db, line,true);

        std::cout << result << '\n';
    }
}


int main() {
    register_commands();
    Database db;
    TCPServer server(db);
    //replay database.aof cache
    //on a clean exit the last command on database.aof must be SAVE for the datadump
    //If no SAVE existed, the programm crashed so recover the latest datadump and then from the last SAVE replay the commands
    //Then if
    Logger::get_instance()->replay_commands(db);
    std::thread server_thread(&TCPServer::run, &server);


   cli(db);
   server.stop();
   server_thread.join();
   Logger::destroy_instance();
}
