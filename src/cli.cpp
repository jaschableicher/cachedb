#include "commands/utils.h"
#include "protocol/protocol.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cctype>
#include <charconv>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

namespace {

bool iequals(std::string_view left, std::string_view right) noexcept {
    if (left.size() != right.size()) return false;
    for (std::size_t index = 0; index < left.size(); ++index) {
        if ((left[index] | 0x20) != (right[index] | 0x20)) return false;
    }
    return true;
}

Value parse_scalar_value(std::string_view input) {
    if (input.empty()) return std::string{};

    const char first_character = input.front();
    const char* const first = input.data();
    const char* const last = first + input.size();
    if ((first_character >= '0' && first_character <= '9') ||
        first_character == '-' || first_character == '+') {
        const char* start = first_character == '+' ? first + 1 : first;
        int64_t integer = 0;
        const auto integer_result = std::from_chars(start, last, integer);
        if (integer_result.ec == std::errc{} && integer_result.ptr == last) return integer;

        double number = 0.0;
        const auto number_result = std::from_chars(first, last, number);
        if (number_result.ec == std::errc{} && number_result.ptr == last) return number;
        return std::string(input);
    }
    if (iequals(input, "nil") || iequals(input, "null") || input == "(nil)") return Null{};
    if (iequals(input, "true")) return true;
    if (iequals(input, "false")) return false;
    return std::string(input);
}

std::vector<Value> parse_arguments(std::string_view input) {
    std::vector<Value> arguments;
    for (std::size_t index = 0; index < input.size();) {
        while (index < input.size() && std::isspace(static_cast<unsigned char>(input[index]))) ++index;
        if (index >= input.size()) break;

        if (input[index] == '"' || input[index] == '\'') {
            const char quotation = input[index++];
            std::string argument;
            while (index < input.size() && input[index] != quotation) {
                if (input[index] == '\\' && index + 1 < input.size()) {
                    ++index;
                    switch (input[index]) {
                        case 'n': argument += '\n'; break;
                        case 't': argument += '\t'; break;
                        default: argument += input[index]; break;
                    }
                } else {
                    argument += input[index];
                }
                ++index;
            }
            if (index < input.size() && input[index] == quotation) ++index;
            arguments.push_back(argument);
        } else {
            const std::size_t start = index;
            while (index < input.size() && !std::isspace(static_cast<unsigned char>(input[index]))) ++index;
            arguments.push_back(parse_scalar_value(input.substr(start, index - start)));
        }
    }
    return arguments;
}

void print_usage(const char* executable) {
    std::cout << "Usage: " << executable << " [--host <IPv4>] [--port <1-65535>]\n"
              << "Defaults: 127.0.0.1:5634. Enter one command per line.\n"
              << "Quote strings containing spaces or numeric strings.\n"
              << "Use quit, exit, or EOF to disconnect. Piped input is supported.\n";
}

class Connection {
public:
    Connection() : socket_fd_(socket(AF_INET, SOCK_STREAM, 0)) {
        if (socket_fd_ < 0) fail("socket");
    }

    ~Connection() { close(socket_fd_); }
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;

    void connect_to(const std::string& host, unsigned short port) {
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(port);
        if (inet_pton(AF_INET, host.c_str(), &address.sin_addr) != 1) {
            throw std::runtime_error("Invalid IPv4 address: " + host);
        }

        timeval timeout{};
        timeout.tv_sec = 5;
        if (setsockopt(socket_fd_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0 ||
            setsockopt(socket_fd_, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
            fail("setsockopt");
        }
        if (connect(socket_fd_, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) < 0) {
            fail("connect");
        }
    }

    Value request(const std::vector<char>& frame) {
        std::size_t offset = 0;
        while (offset < frame.size()) {
            const auto sent = send(socket_fd_, frame.data() + offset, frame.size() - offset, MSG_NOSIGNAL);
            if (sent < 0 && errno == EINTR) continue;
            if (sent < 0) fail("send");
            if (sent == 0) throw std::runtime_error("Connection closed while sending");
            offset += static_cast<std::size_t>(sent);
        }

        uint32_t header = 0;
        receive_exactly(reinterpret_cast<char*>(&header), sizeof(header));
        const uint32_t size = ntohl(header);
        if (size == 0 || size > 16 * 1024 * 1024) {
            throw std::runtime_error("Invalid response length");
        }
        std::vector<char> payload(size);
        receive_exactly(payload.data(), payload.size());
        offset = 0;
        auto response = msgpack::unpack(payload.data(), payload.size(), offset);
        if (offset != payload.size()) throw std::runtime_error("Trailing data after response");
        return protocol::decode_value(response.get());
    }

private:
    [[noreturn]] static void fail(const char* operation) {
        throw std::runtime_error(std::string(operation) + ": " + std::strerror(errno));
    }

    void receive_exactly(char* data, std::size_t size) {
        std::size_t offset = 0;
        while (offset < size) {
            const auto received = recv(socket_fd_, data + offset, size - offset, 0);
            if (received < 0 && errno == EINTR) continue;
            if (received < 0) fail("receive");
            if (received == 0) throw std::runtime_error("Server disconnected before replying");
            offset += static_cast<std::size_t>(received);
        }
    }

    int socket_fd_;
};

std::vector<char> make_request(const std::string& name, std::string_view input) {
    const auto arguments = parse_arguments(input);
    msgpack::sbuffer payload;
    msgpack::packer<msgpack::sbuffer> writer(payload);
    writer.pack_array(2);
    writer.pack(name);
    writer.pack_array(static_cast<uint32_t>(arguments.size()));
    for (const auto& argument : arguments) {
        const auto encoded = protocol::encode_value(argument);
        payload.write(encoded.data(), encoded.size());
    }
    return protocol::frame_payload(payload);
}

void print_reply(const Value& reply) {
    if (const auto* number = std::get_if<uint64_t>(&reply)) {
        std::cout << *number;
    } else if (const auto* bytes = std::get_if<Bytes>(&reply)) {
        std::cout << "0x" << std::hex << std::setfill('0');
        for (const auto byte : *bytes) {
            std::cout << std::setw(2) << std::to_integer<unsigned int>(byte);
        }
        std::cout << std::dec << std::setfill(' ');
    } else {
        std::cout << value_to_string(reply);
    }
    std::cout << '\n';
}

}

int main(int argc, char* argv[]) {
    std::string host = "127.0.0.1";
    unsigned short port = 5634;
    for (int index = 1; index < argc; ++index) {
        const std::string_view option = argv[index];
        if (option == "--help") {
            print_usage(argv[0]);
            return EXIT_SUCCESS;
        }
        if ((option != "--host" && option != "--port") || index + 1 == argc) {
            print_usage(argv[0]);
            return EXIT_FAILURE;
        }
        const std::string_view value = argv[++index];
        if (option == "--host") {
            host = value;
        } else {
            unsigned int parsed = 0;
            const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
            if (result.ec != std::errc{} || result.ptr != value.data() + value.size() ||
                parsed == 0 || parsed > 65535) {
                std::cerr << "Invalid port: " << value << '\n';
                return EXIT_FAILURE;
            }
            port = static_cast<unsigned short>(parsed);
        }
    }

    try {
        Connection connection;
        connection.connect_to(host, port);
        const bool interactive = isatty(STDIN_FILENO) != 0;
        std::string line;
        while (true) {
            if (interactive) std::cout << host << ':' << port << "> " << std::flush;
            if (!std::getline(std::cin, line)) break;
            std::istringstream input(line);
            std::string name;
            if (!(input >> name)) continue;
            for (char& character : name) {
                character = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
            }
            if (name == "QUIT" || name == "EXIT") break;
            std::string arguments;
            std::getline(input, arguments);
            std::vector<char> frame;
            try {
                frame = make_request(name, arguments);
            } catch (const std::exception& error) {
                std::cerr << "Invalid command: " << error.what() << '\n';
                if (!interactive) return EXIT_FAILURE;
                continue;
            }
            print_reply(connection.request(frame));
        }
        if (std::cin.bad()) throw std::runtime_error("Failed to read input");
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}