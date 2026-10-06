#include "utils.h"

template <typename T>
std::string num_to_string(T val) {
    std::array<char, 32> buffer;
    auto [ptr, ec] = std::to_chars(buffer.data(), buffer.data() + buffer.size(), val);
    return std::string(buffer.data(), ptr);
}

template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;


std::string value_to_string(const Value& value) {
    return std::visit(overloaded {
        [](const Null&) -> std::string { return "(nil)"; },
        [](const std::string& str) -> std::string { return str; },
        [](int64_t v) -> std::string { return num_to_string(v); },
        [](double v) -> std::string { return num_to_string(v); },
        [](bool b) -> std::string { return b ? "true" : "false"; },
        [](const auto&) -> std::string { return "<unsupported value>"; }
    }, value);
}



