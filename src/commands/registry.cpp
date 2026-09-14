#include "registry.h"

#include <charconv>
#include <iostream>
#include <limits>
Registry* Registry::instance_=nullptr;

const CommandDescriptor* Registry::lookup(std::string_view id) const
{
    auto it = commands_.find(std::string(id));

    if (it == commands_.end())
        return nullptr;

    return &it->second;
}

void Registry::register_command(CommandDescriptor command_descriptor){
    std::string name(command_descriptor.name);

    if(commands_.contains(std::string(command_descriptor.name))){
        throw std::runtime_error("Command already exists: " + name);
    }
    commands_.emplace(
        std::move(name),
        std::move(command_descriptor)
    );
}

ExecuteReturn Registry::execute( CommandContext& context,const Command& command) const{
    const CommandDescriptor* descriptor = lookup(command.name);

    if (!descriptor) {
        return {
            std::string(
            "ERR unknown command '" +
            command.name +
            "'"),
            EXECUTEERROR
        };
    }

    std::size_t required_arguments = 0;

    for (const auto& argument : descriptor->arguments)
    {
        if (!argument.optional)
            ++required_arguments;
    }

    if (command.args.size() < required_arguments) {
        return {
            std::string(
            "ERR too few arguments for '" + std::string(descriptor->name) +"'"),
            EXECUTEERROR
        };
    }

    if (command.args.size() >descriptor->arguments.size())
    {
        return {std::string(
            "ERR too many arguments for '" +
            std::string(descriptor->name) +
            "'"),
            EXECUTEERROR
        };
    }

    auto args = command.args;
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (!coerce_value(args[i], descriptor->arguments[i].type))
        {
            return {
                 std::string(
                "ERR invalid argument type at index " +
                std::to_string(i)),
                EXECUTEERROR
            };
        }
    }
    Value value = descriptor->handler(
            context,
            args
        );

    return {
        value,
        ExecuteState::EXECUTESUCCESS
    };
}

bool Registry::coerce_value(Value& value, ValueType expected) const
{
    if (value_matches_type(value, expected)) {
        return true;
    }

    if (expected == ValueType::Int64) {
        if (const auto* unsigned_integer = std::get_if<uint64_t>(&value)) {
            if (*unsigned_integer > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
                return false;
            }

            value = static_cast<int64_t>(*unsigned_integer);
            return true;
        }
    }

    const auto* text = std::get_if<std::string>(&value);
    if (text == nullptr || expected != ValueType::Int64) {
        return false;
    }

    std::int64_t parsed = 0;
    const char* begin = text->data();
    const char* end = begin + text->size();
    const auto [position, error] = std::from_chars(begin, end, parsed);
    if (error != std::errc{} || position != end) {
        return false;
    }

    value = parsed;
    return true;
}

bool Registry::value_matches_type(const Value& value, ValueType expected) const
{
    switch (expected) {
        case ValueType::Null:
            return std::holds_alternative<Null>(value);

        case ValueType::Bool:
            return std::holds_alternative<bool>(value);

        case ValueType::Int64:
            return std::holds_alternative<int64_t>(value);

        case ValueType::UInt64:
            return std::holds_alternative<uint64_t>(value);

        case ValueType::Double:
            return std::holds_alternative<double>(value);

        case ValueType::String:
            return std::holds_alternative<std::string>(value);

        case ValueType::Bytes:
            return std::holds_alternative<Bytes>(value);

        case ValueType::Array:
            return std::holds_alternative<Array>(value);

        case ValueType::Map:
            return std::holds_alternative<Map>(value);

        case ValueType::Extension:
            return std::holds_alternative<Extension>(value);
        case ValueType::Any:
            return true;
    }

    return false;
}
