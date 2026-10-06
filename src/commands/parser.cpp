#include "parser.h"

Command parse_command(const char* data, std::size_t size){
    std::size_t offset = 0;
    auto handle = msgpack::unpack(data, size, offset);

    if (offset != size) {
        throw std::runtime_error("Trailing data after command");
    }

    const auto& root = handle.get();

    // ["SET", [[String, "key"], [Bool, true]]]
    if (root.type != msgpack::type::ARRAY ||
        root.via.array.size != 2) {
        throw std::runtime_error(
            "Expected [command, arguments]");
    }

    const auto& name = root.via.array.ptr[0];
    const auto& arguments = root.via.array.ptr[1];

    if (name.type != msgpack::type::STR) {
        throw std::runtime_error("Command must be a string");
    }

    if (arguments.type != msgpack::type::ARRAY) {
        throw std::runtime_error("Arguments must be an array");
    }

    Command command;
    command.name = name.as<std::string>();
    command.args.reserve(arguments.via.array.size);

    for (uint32_t i = 0; i < arguments.via.array.size; ++i) {
        command.args.push_back(
            protocol::decode_value(arguments.via.array.ptr[i]));
    }

    return command;

}
