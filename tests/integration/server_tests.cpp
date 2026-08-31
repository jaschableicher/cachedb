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
#include "server/server.h"

class ServerIntegrationTest : public ::testing::Test {
protected:
    static constexpr int PORT = 5634;
    static constexpr const char* HOST = "127.0.0.1";

    static inline Database db{};
    static inline TCPServer server{db};
    static inline std::thread server_thread;

    static void SetUpTestSuite() {
        // Run server on background thread
        server_thread = std::thread([]() {
            server.run();
        });

        // Give the server a brief moment to bind to 5634
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    static void TearDownTestSuite() {
        server.stop();
        if (server_thread.joinable()) {
            server_thread.join();
        }
    }

    // Helper client to send a message and read the response
    std::string SendAndReceive(const std::string& msg) {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) return "";

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
            return "";
        }

        send(sock, msg.c_str(), msg.size(), 0);

        char buffer[1024] = {0};
        ssize_t bytes = recv(sock, buffer, sizeof(buffer) - 1, 0);
        close(sock);

        return (bytes > 0) ? std::string(buffer, bytes) : "";
    }
};

// --- Test Cases ---
TEST_F(ServerIntegrationTest, ServerAnswers) {
    // Verifies the server's outer accept loop correctly handles new connections
    std::string response = SendAndReceive("whatever\n");
    EXPECT_FALSE(
        response.empty()
    );
}
TEST_F(ServerIntegrationTest, SetAndGetValue) {
    std::string response = SendAndReceive("SET hello world\n");
    EXPECT_EQ(response, "OK\n");
    std::string response2 = SendAndReceive("GET hello\n");
    EXPECT_EQ(response2, "world\n");
}

