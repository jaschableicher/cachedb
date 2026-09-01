#include "registry.h"
#include <iostream>
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

Value Registry::execute( CommandContext& context,const Command& command) const{
    const CommandDescriptor* descriptor = lookup(command.name);

    if (!descriptor) {
        return std::string(
            "Error unknown command '" +
            command.name +
            "'"
        );
    }

    std::size_t required_arguments = 0;

    for (const auto& argument : descriptor->arguments)
    {
        if (!argument.optional)
            ++required_arguments;
    }

    if (command.args.size() < required_arguments) {
        return std::string(
            "Error too few arguments for '" + std::string(descriptor->name) +"'"
        );
    }

    if (command.args.size() >descriptor->arguments.size())
    {
        return std::string(
            "Error too many arguments for '" +
            std::string(descriptor->name) +
            "'"
        );
    }

    for ( std::size_t i = 0; i < command.args.size();++i ) {
        if (!value_matches_type(
                command.args[i],
                descriptor->arguments[i].type))
        {
            return std::string(
                "ERR invalid argument type at index " +
                std::to_string(i)
            );
        }
    }

    return descriptor->handler(
        context,
        command.args
    );
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
    }

    return false;
}