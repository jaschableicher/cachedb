#include <arpa/inet.h>
#include <atomic>
#include <barrier>
#include <chrono>
#include <cerrno>
#include <cstdlib>
#include <fcntl.h>
#include <iomanip>
#include <iostream>
#include <limits>
#include <netinet/in.h>
#include <poll.h>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace {

struct Options {
    std::string host = "127.0.0.1";
    unsigned short port = 5634;
    std::size_t clients = 100;
    int timeout_ms = 10'000;
    int ramp_up_ms = 1'000;   // Spread client starts across this duration
    int hold_sec = 5;         // Keep all connections open concurrently for X seconds
};

struct Results {
    std::atomic<std::size_t> successful_connections = 0;
    std::atomic<std::size_t> failed_connections = 0;
    std::atomic<long long> total_latency_us = 0;
    std::atomic<long long> longest_latency_us = 0;
};

void print_usage(const char* executable) {
    std::cout
        << "Usage: " << executable << " [options]\n"
        << "  --host <IPv4 address>   Server address (default: 127.0.0.1)\n"
        << "  --port <1-65535>        Server port (default: 5634)\n"
        << "  --clients <count>       Target concurrent connections (default: 100)\n"
        << "  --timeout-ms <count>    Per-connection timeout in ms (default: 15000)\n"
        << "  --ramp-up-ms <count>    Time to ramp up all connections in ms (default: 1000)\n"
        << "  --hold-sec <count>      Duration to hold connections open in sec (default: 5)\n";
}

bool parse_positive_number(std::string_view text, unsigned long long& value) {
    if (text.empty()) return false;
    char* end = nullptr;
    errno = 0;
    const auto parsed = std::strtoull(text.data(), &end, 10);
    if (errno != 0 || end != text.data() + text.size()) return false;
    value = parsed;
    return true;
}

bool parse_options(int argc, char* argv[], Options& options) {
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument = argv[index];
        if (argument == "--help") {
            print_usage(argv[0]);
            std::exit(EXIT_SUCCESS);
        }
        if (index + 1 == argc) return false;

        const std::string_view value = argv[++index];
        unsigned long long parsed = 0;
        if (argument == "--host") {
            options.host = value;
        } else if (argument == "--port" && parse_positive_number(value, parsed) && parsed <= 65535) {
            options.port = static_cast<unsigned short>(parsed);
        } else if (argument == "--clients" && parse_positive_number(value, parsed) && parsed > 0) {
            options.clients = static_cast<std::size_t>(parsed);
        } else if (argument == "--timeout-ms" && parse_positive_number(value, parsed)) {
            options.timeout_ms = static_cast<int>(parsed);
        } else if (argument == "--ramp-up-ms" && parse_positive_number(value, parsed)) {
            options.ramp_up_ms = static_cast<int>(parsed);
        } else if (argument == "--hold-sec" && parse_positive_number(value, parsed)) {
            options.hold_sec = static_cast<int>(parsed);
        } else {
            return false;
        }
    }
    return true;
}

// Returns the socket file descriptor if successful (>= 0), or -1 on failure
int connect_with_timeout(const Options& options, const sockaddr_in& address, long long& latency_us) {
    const auto started_at = std::chrono::steady_clock::now();
    const int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) return -1;

    const int current_flags = fcntl(socket_fd, F_GETFL, 0);
    if (current_flags < 0 || fcntl(socket_fd, F_SETFL, current_flags | O_NONBLOCK) < 0) {
        close(socket_fd);
        return -1;
    }

    bool connected = connect(socket_fd, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == 0;
    if (!connected && errno == EINPROGRESS) {
        pollfd descriptor{.fd = socket_fd, .events = POLLOUT, .revents = 0};
        const int poll_result = poll(&descriptor, 1, options.timeout_ms);
        if (poll_result > 0 && (descriptor.revents & POLLOUT) != 0) {
            int socket_error = 0;
            socklen_t error_length = sizeof(socket_error);
            connected = (getsockopt(socket_fd, SOL_SOCKET, SO_ERROR, &socket_error, &error_length) == 0)
                        && (socket_error == 0);
        }
    }

    if (!connected) {
        close(socket_fd);
        return -1;
    }

    latency_us = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - started_at).count();
    return socket_fd;
}

void record_longest_latency(std::atomic<long long>& longest_latency_us, long long latency_us) {
    long long current = longest_latency_us.load();
    while (current < latency_us && !longest_latency_us.compare_exchange_weak(current, latency_us)) {
    }
}

} // namespace

int main(int argc, char* argv[]) {
    Options options;
    if (!parse_options(argc, argv, options)) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(options.port);
    if (inet_pton(AF_INET, options.host.c_str(), &address.sin_addr) != 1) {
        std::cerr << "Invalid IPv4 address: " << options.host << '\n';
        return EXIT_FAILURE;
    }

    Results results;
    std::atomic<bool> hold_gate{true};
    std::vector<std::thread> clients;
    clients.reserve(options.clients);

    const auto ramp_interval = (options.clients > 1 && options.ramp_up_ms > 0)
        ? std::chrono::microseconds((options.ramp_up_ms * 1000) / options.clients)
        : std::chrono::microseconds(0);

    const auto benchmark_start = std::chrono::steady_clock::now();

    for (std::size_t index = 0; index < options.clients; ++index) {
        // Stagger thread connection start times
        if (ramp_interval.count() > 0 && index > 0) {
            std::this_thread::sleep_for(ramp_interval);
        }

        clients.emplace_back([&, index]() {
            long long latency_us = 0;
            int sock_fd = connect_with_timeout(options, address, latency_us);

            if (sock_fd >= 0) {
                results.successful_connections.fetch_add(1);
                results.total_latency_us.fetch_add(latency_us);
                record_longest_latency(results.longest_latency_us, latency_us);

                // Hold connection open until test finishes
                while (hold_gate.load(std::memory_order_relaxed)) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                }

                close(sock_fd);
            } else {
                results.failed_connections.fetch_add(1);
            }
        });
    }

    // Hold steady state
    std::cout << "All " << options.clients << " clients launched. Holding steady state for " 
              << options.hold_sec << "s...\n";
    std::this_thread::sleep_for(std::chrono::seconds(options.hold_sec));

    // Release all connections
    hold_gate.store(false, std::memory_order_relaxed);

    for (std::thread& client : clients) {
        client.join();
    }

    const auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - benchmark_start).count();
    const std::size_t successes = results.successful_connections.load();
    const std::size_t failures = results.failed_connections.load();
    const double average_latency_ms = successes == 0
        ? 0.0
        : static_cast<double>(results.total_latency_us.load()) / successes / 1'000.0;
    const double longest_latency_ms = static_cast<double>(results.longest_latency_us.load()) / 1'000.0;

    std::cout << "\n--- Results ---\n"
              << std::fixed << std::setprecision(2)
              << "Server: " << options.host << ':' << options.port << '\n'
              << "Max Simultaneous Sockets: " << successes << " / " << options.clients << '\n'
              << "Failed Connections: " << failures << '\n'
              << "Total Test Duration: " << elapsed << " s\n"
              << "Average Connect Latency: " << average_latency_ms << " ms\n"
              << "Longest Connect Latency: " << longest_latency_ms << " ms\n";

    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
