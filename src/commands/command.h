#ifndef COMMAND_H
#define COMMAND_H

#include <vector>
#include <variant>
#include <cstdint>
#include <span>
#include <functional>
#include <string>
#include "database/database.h"
using Bytes = std::vector<std::byte>;
using Null = std::monostate;

struct Array {};
struct Map {};
struct Extension {};

using Value = std::variant<
    Null,
    bool,
    int64_t,
    uint64_t,
    double,
    std::string,
    Bytes,
    Array,
    Map,
    Extension
>;




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

enum class ValueType {
    Null,
    Bool,
    Int64,
    UInt64,
    Double,
    String,
    Bytes,
    Array,
    Map,
    Extension
};

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