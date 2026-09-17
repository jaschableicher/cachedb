#ifndef VALUE_H
#define VALUE_H

#include <vector>
#include <variant>
#include <cstdint>
#include <cstddef>
#include <string>
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

enum class ValueType : uint8_t{
    Any=0,
    Null=1,
    Bool=2,
    Int64=3,
    UInt64=4,
    Double=5,
    String=6,
    Bytes=7,
    Array=8,
    Map=9,
    Extension=10
};

#endif