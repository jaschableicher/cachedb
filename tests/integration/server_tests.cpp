//TODO: Server integration tests to check wether the server is running correctly and responds correctly
// Basically what the api/sdk must do later

#include <gtest/gtest.h>
#include <string>
#include <cstring>
#include <thread>
#include <chrono>
#include <atomic>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "database/database.h"
#include "commands/command.h"
#include "protocol/protocol.h"
#include "server/server.h"

class ServerIntegrationTest : public ::testing::Test {
protected:
    static constexpr int PORT = 5634;
    static constexpr const char* HOST = "127.0.0.1";

    static inline Database db{};
    static inline TCPServer server{db};
    static inline std::thread server_thread;

    static void SetUpTestSuite() {
        register_commands();

        // Run server on background thread
        server_thread = std::thread([]() {
            server.run();
        });

        // Give the server a brief moment to bind to 5634
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    static void TearDownTestSuite() {
        std::cout <<"Stopping test suite" << std::endl;
        server.stop();
        if (server_thread.joinable()) {
            server_thread.join();
        }
    }

    Value SendAndReceive(const Command& command) {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) return Null{};

        // 1-second timeout so the test never hangs
        struct timeval tv{.tv_sec = 1, .tv_usec = 0};
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(PORT);
        inet_pton(AF_INET, HOST, &addr.sin_addr);

        if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            close(sock);
            return Null{};
        }

        msgpack::sbuffer payload;
        msgpack::packer<msgpack::sbuffer> writer(payload);
        writer.pack_array(2);
        writer.pack(command.name);
        writer.pack_array(static_cast<uint32_t>(command.args.size()));
        for (const auto& arg : command.args) {
            auto encoded = protocol::encode_value(arg);
            auto value = msgpack::unpack(encoded.data(), encoded.size());
            writer.pack(value.get());
        }
        const auto frame = protocol::frame_payload(payload);

        std::size_t sent = 0;
        while (sent < frame.size()) {
            const ssize_t count = send(sock, frame.data() + sent, frame.size() - sent, 0);
            if (count <= 0) {
                close(sock);
                return Null{};
            }
            sent += static_cast<std::size_t>(count);
        }

        auto receive_exactly = [&](char* data, std::size_t size) {
            std::size_t received = 0;
            while (received < size) {
                const ssize_t count = recv(sock, data + received, size - received, 0);
                if (count <= 0) return false;
                received += static_cast<std::size_t>(count);
            }
            return true;
        };

        unsigned char header[4];
        if (!receive_exactly(reinterpret_cast<char*>(header), sizeof(header))) {
            close(sock);
            return Null{};
        }

        const uint32_t response_size =
            (static_cast<uint32_t>(header[0]) << 24) |
            (static_cast<uint32_t>(header[1]) << 16) |
            (static_cast<uint32_t>(header[2]) << 8) |
             static_cast<uint32_t>(header[3]);

        std::vector<char> response(response_size);
        if (!receive_exactly(response.data(), response.size())) {
            close(sock);
            return Null{};
        }

        close(sock);
        auto decoded = msgpack::unpack(response.data(), response.size());
        return protocol::decode_value(decoded.get());
    }
};

// --- Test Cases ---
TEST_F(ServerIntegrationTest, ServerAnswers) {
    // Verifies the server's outer accept loop correctly handles new connections
    Value response = SendAndReceive(Command{.name = "whatever", .args = {}});
    ASSERT_TRUE(std::holds_alternative<std::string>(response));
    EXPECT_FALSE(std::get<std::string>(response).empty());
}
TEST_F(ServerIntegrationTest, SetAndGetValue) {
    Value response = SendAndReceive(Command{.name = "SET", .args = {std::string("hello"), std::string("world")}});
    ASSERT_TRUE(std::holds_alternative<std::string>(response));
    EXPECT_EQ(std::get<std::string>(response), "OK");
    Value response2 = SendAndReceive(Command{.name = "GET", .args = {std::string("hello")}});
    ASSERT_TRUE(std::holds_alternative<std::string>(response2));
    EXPECT_EQ(std::get<std::string>(response2), "world");
}
