#include "protocol.h"
constexpr uint32_t MAX_MESSAGE_SIZE = 16 * 1024 * 1024; // Example: 16 MiB

namespace protocol{
std::vector<char> frame_payload(const msgpack::sbuffer& payload) {
    if (payload.size() > MAX_MESSAGE_SIZE) {
        throw std::length_error("Message too large");
    }

    const auto length = static_cast<uint32_t>(payload.size());
    std::vector<char> frame(4 + payload.size());

    // Network byte order: most significant byte first.
    auto* header = reinterpret_cast<unsigned char*>(frame.data());
    header[0] = static_cast<unsigned char>(length >> 24);
    header[1] = static_cast<unsigned char>(length >> 16);
    header[2] = static_cast<unsigned char>(length >> 8);
    header[3] = static_cast<unsigned char>(length);

    std::copy(payload.data(), payload.data() + payload.size(),
              frame.begin() + 4);

    return frame;
}
}