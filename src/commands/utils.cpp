#include "utils.h"
std::string value_to_string(const Value& value){
    
    if (std::holds_alternative<Null>(value)) {
        return "(nil)";
    }

    if (const auto* str = std::get_if<std::string>(&value))
    {
        return *str;
    }

    if (const auto* integer = std::get_if<int64_t>(&value))
    {
        return std::to_string(*integer);
    }

    if (const auto* integer = std::get_if<uint64_t>(&value))
    {
        return std::to_string(*integer);
    }

    if (const auto* number = std::get_if<double>(&value))
    {
        return std::to_string(*number);
    }

    if (const auto* boolean = std::get_if<bool>(&value))
    {
        return *boolean ? "true" : "false";
    }

    return "<unsupported value>";
}