#ifndef COMMAND_H
#define COMMAND_H

#include <vector>
#include <string>

enum class CommandType {
    Set,
    Get,
    Del,
    Unknown
};

struct Command {
    CommandType type;
    std::vector<std::string> args;
};

#endif