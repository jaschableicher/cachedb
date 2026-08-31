#include <iostream>
#include "database/database.h"
#include "executor.h"
#include "server/server.h"

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
    Database db;
    TCPServer server(db);
    server.run();
}