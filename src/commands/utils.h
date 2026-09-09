#ifndef UTILS_H
#define UTILS_H
#include "command.h"
#include <charconv>
std::string value_to_string(const Value& value);
Value parse_scalar_value(std::string_view raw_str);
#endif