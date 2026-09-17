#include "protocol/messagepack.h"
#include <gtest/gtest.h>
#include <string>

TEST(MessagePackTest, RoundTripsStringWithNewlineAndNullByte) {
    const std::string original{"hello\nworld\0!", 13};
    msgpack::sbuffer buffer;
    msgpack::pack(buffer, original);

    auto decoded = msgpack::unpack(buffer.data(), buffer.size());
    EXPECT_EQ(decoded.get().as<std::string>(), original);
}