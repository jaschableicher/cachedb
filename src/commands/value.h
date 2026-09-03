#ifndef VALUE_H
#define VALUE_H

#include <vector>
#include <variant>
#include <cstdint>
#include <span>
#include <functional>

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

#endif