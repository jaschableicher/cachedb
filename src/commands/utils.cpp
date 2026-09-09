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

Value parse_scalar_value(std::string_view sv) {
     // 1. Check for Null
    if (sv == "null" || sv == "Null" || sv == "NULL" || sv=="nil" || sv=="(nil)") {
        return Null{};
    }

    // 2. Check for Boolean
    if (sv == "true" || sv == "True") {
        return true;
    }
    if (sv == "false" || sv == "False") {
        return false;
    }
    const char* first = sv.data();
    const char* last  = sv.data() + sv.size();
    // Unsigned integer 
    if (sv.front() >= '0' && sv.front() <= '9') {
        uint64_t u_val = 0;
        auto [ptr, ec] = std::from_chars(first, last, u_val);
        if (ec == std::errc{} && ptr == last) {
            return u_val;
        }
    }

    // Signed integer 
    int64_t i_val = 0;
    auto [ptr, ec] = std::from_chars(first, last, i_val);
    if (ec == std::errc{} && ptr == last) {
        return i_val;
    }

    
    double d_val = 0.0;
    auto [d_ptr, d_ec] = std::from_chars(first, last, d_val);
    if (d_ec == std::errc{} && d_ptr == last) {
        return d_val;
    }

    return std::string(sv);
}