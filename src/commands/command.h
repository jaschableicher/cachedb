#ifndef COMMAND_H
#define COMMAND_H

#include <vector>
#include <variant>
#include <cstdint>
#include <span>
#include <functional>
#include <string>
#include "database/database.h"
#include "value.h"




enum class CommandType {
    Set,
    Get,
    Del,
    Unknown
};

struct Command {
    std::string name;
    std::vector<Value> args;
};



struct CommandContext {
    Database& db;
};
using CommandHandler =
    std::function<Value(CommandContext&, std::span<const Value>)>;



struct ArgumentSpec {
    ValueType type; // E.g. ValueType::String
    bool optional = false;
};

struct CommandDescriptor {
    uint32_t id;
    std::string name;

    std::vector<ArgumentSpec> arguments;

    CommandHandler handler;
};


void register_commands();
#endif