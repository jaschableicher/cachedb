#pragma once
#include "commands/value.h"
#include "messagepack.h"
namespace protocol{
    

    msgpack::sbuffer encode_value(const Value& value);
    Value decode_value(const char* data, size_t size);
    std::vector<char> frame_payload(const msgpack::sbuffer& payload);
}