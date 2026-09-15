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


// Helper for case-insensitive 3, 4, 5-char keyword checks without heap alloc
inline bool iequals(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if ((a[i] | 0x20) != (b[i] | 0x20)) return false; // ASCII case-folding
    }
    return true;
}

Value parse_scalar_value(std::string_view sv) {
     // 1. Check for Null
    if (sv.empty()) {
        return std::string{};
    }

    const char first_char = sv.front();
    const char* const first = sv.data();
    const char* const last  = first + sv.size();

     if ((first_char >= '0' && first_char <= '9') || first_char == '-' || first_char == '+') {

        const char* start = (first_char == '+') ? first + 1 : first;
        int64_t i_val = 0;
        auto [ptr, ec] = std::from_chars(start, last, i_val);
        if (ec == std::errc{} && ptr == last) {
            return i_val;
        }


        // Floating Point (fallback if integer parsing didn't consume the whole string)
        double d_val = 0.0;
        auto [d_ptr, d_ec] = std::from_chars(first, last, d_val);
        if (d_ec == std::errc{} && d_ptr == last) {
            return d_val;
        }
        return std::string(sv);
    }
    switch (sv.size()) {
        case 3:
            if (iequals(sv, "nil")) return Null{};
            break;
        case 4:
            if (iequals(sv, "null")) return Null{};
            if (iequals(sv, "true")) return true;
            break;
        case 5:
            if (sv == "(nil)") return Null{};
            if (iequals(sv, "false")) return false;
            break;
        default:
            break;
    }
    return std::string(sv);
}

