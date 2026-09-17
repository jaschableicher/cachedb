#include "parser.h"

std::vector<Value> parse_arguments(std::string_view input) {
    std::vector<Value> args;

    for (std::size_t i = 0; i < input.size();) {
        while (i < input.size() && std::isspace(static_cast<unsigned char>(input[i]))) ++i;
        if (i >= input.size()) break;

        std::string arg;
        bool is_quoted = false;
        if (std::string_view("\"'").find(input[i]) != std::string_view::npos) {
            char quotation = input[i];
            is_quoted=true;
            ++i;

            while (i < input.size() && input[i] != quotation) {
                if (input[i] == '\\' && i + 1 < input.size()) {
                    i++;

                    switch (input[i]) {
                        case 'n': arg += '\n'; break;
                        case 't': arg += '\t'; break;
                        case '"': arg += '"'; break;
                        case '\'':   arg += '\''; break;
                        case '\\': arg += '\\'; break;
                        default: arg += input[i]; break;
                    }
                } else {
                    arg += input[i];
                }

                i++;
            }

            if (i < input.size() && input[i] == quotation) ++i;
            args.push_back(arg);
        } else {
            std::size_t start = i;
            while (i < input.size() && !std::isspace(static_cast<unsigned char>(input[i]))) {
                ++i;
            }
            std::string_view token = input.substr(start, i - start);
            args.push_back(parse_scalar_value(token));
        }
    }

    return args;
}

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
