#include <iostream>
#include "database/database.h"
#include "executor.h"
int main() {
    Database db;

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