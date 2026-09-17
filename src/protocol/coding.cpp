#include "messagepack.h"
#include "protocol.h"
namespace protocol{
msgpack::sbuffer encode_value(const Value& value){
    msgpack::sbuffer buffer;
    msgpack::packer<msgpack::sbuffer> writer(buffer);
    writer.pack_array(2); // Type ID, payload

    auto write_type = [&](ValueType type) {
        writer.pack_uint8(static_cast<uint8_t>(type));
    };

    std::visit([&](const auto& data) {
        using T = std::decay_t<decltype(data)>;

        if constexpr (std::is_same_v<T, Null>) {
            write_type(ValueType::Null);
            writer.pack_nil();
        } else if constexpr (std::is_same_v<T, bool>) {
            write_type(ValueType::Bool);
            writer.pack(data);
        } else if constexpr (std::is_same_v<T, int64_t>) {
            write_type(ValueType::Int64);
            writer.pack_int64(data);
        } else if constexpr (std::is_same_v<T, uint64_t>) {
            write_type(ValueType::UInt64);
            writer.pack_uint64(data);
        } else if constexpr (std::is_same_v<T, double>) {
            write_type(ValueType::Double);
            writer.pack_double(data);
        } else if constexpr (std::is_same_v<T, std::string>) {
            if (data.size() > std::numeric_limits<uint32_t>::max()) {
                throw std::length_error("String too large");
            }

            write_type(ValueType::String);

            const auto size = static_cast<uint32_t>(data.size());
            writer.pack_str(size);
            if (size != 0) {
                writer.pack_str_body(data.data(), size);
            }
        } else if constexpr (std::is_same_v<T, Bytes>) {
            if (data.size() > std::numeric_limits<uint32_t>::max()) {
                throw std::length_error("Bytes too large");
            }

            write_type(ValueType::Bytes);

            const auto size = static_cast<uint32_t>(data.size());
            writer.pack_bin(size);
            if (size != 0) {
                writer.pack_bin_body(
                    reinterpret_cast<const char*>(data.data()), size);
            }
        } else {
            throw std::invalid_argument(
                "Array, Map and Extension are not supported yet");
        }
    }, value);
    return buffer;
}

Value decode_value(const msgpack::object& root) {
    if (root.type != msgpack::type::ARRAY ||
        root.via.array.size != 2) {
        throw std::runtime_error("Expected [type ID, payload]");
    }

    const auto type =
        static_cast<ValueType>(root.via.array.ptr[0].as<uint8_t>());
    const auto& payload = root.via.array.ptr[1];

    switch (type) {
        case ValueType::Null:
            if (payload.type != msgpack::type::NIL)
                throw std::runtime_error("Expected nil");
            return Null{};

        case ValueType::Bool:
            return payload.as<bool>();

        case ValueType::Int64:
            return payload.as<int64_t>();

        case ValueType::UInt64:
            return payload.as<uint64_t>();

        case ValueType::Double:
            return payload.as<double>();

        case ValueType::String:
            return payload.as<std::string>();

        case ValueType::Bytes: {
            if (payload.type != msgpack::type::BIN)
                throw std::runtime_error("Expected binary");

            Bytes bytes(payload.via.bin.size);
            for (std::size_t i = 0; i < bytes.size(); ++i) {
                bytes[i] = static_cast<std::byte>(
                    static_cast<unsigned char>(payload.via.bin.ptr[i]));
            }
            return bytes;
        }

        default:
            throw std::runtime_error("Unsupported value type");
    }
}
}