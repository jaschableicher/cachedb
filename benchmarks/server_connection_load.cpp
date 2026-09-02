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
    int timeout_ms = 1'000;
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
        << "  --host <IPv4 address>  Server address (default: 127.0.0.1)\n"
        << "  --port <1-65535>       Server port (default: 5634)\n"
        << "  --clients <count>      Simultaneous connections (default: 100)\n"
        << "  --timeout-ms <count>   Per-connection timeout (default: 1000)\n";
}

bool parse_positive_number(std::string_view text, unsigned long long& value) {
    if (text.empty()) {
        return false;
    }

    char* end = nullptr;
    errno = 0;
    const auto parsed = std::strtoull(text.data(), &end, 10);
    if (errno != 0 || end != text.data() + text.size() || parsed == 0) {
        return false;
    }

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

        if (index + 1 == argc) {
            return false;
        }

        const std::string_view value = argv[++index];
        unsigned long long parsed = 0;
        if (argument == "--host") {
            options.host = value;
        } else if (argument == "--port" && parse_positive_number(value, parsed) && parsed <= std::numeric_limits<unsigned short>::max()) {
            options.port = static_cast<unsigned short>(parsed);
        } else if (argument == "--clients" && parse_positive_number(value, parsed)&& parsed <= std::numeric_limits<std::size_t>::max()) {
            options.clients = static_cast<std::size_t>(parsed);
        } else if (argument == "--timeout-ms" && parse_positive_number(value, parsed) && parsed <= static_cast<unsigned long long>(std::numeric_limits<int>::max())) {
            options.timeout_ms = static_cast<int>(parsed);
        } else {
            return false;
        }
    }

    return true;
}

bool connect_with_timeout(const Options& options, const sockaddr_in& address, long long& latency_us) {
    const auto started_at = std::chrono::steady_clock::now();
    const int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        return false;
    }

    const int current_flags = fcntl(socket_fd, F_GETFL, 0);
    if (current_flags < 0 || fcntl(socket_fd, F_SETFL, current_flags | O_NONBLOCK) < 0) {
        close(socket_fd);
        return false;
    }

    bool connected = connect(socket_fd, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == 0;
    if (!connected && errno == EINPROGRESS) {
        pollfd descriptor{.fd = socket_fd, .events = POLLOUT, .revents = 0};
        const int poll_result = poll(&descriptor, 1, options.timeout_ms);
        if (poll_result > 0 && (descriptor.revents & POLLOUT) != 0) {
            int socket_error = 0;
            socklen_t error_length = sizeof(socket_error);
            connected = getsockopt(socket_fd, SOL_SOCKET, SO_ERROR, &socket_error, &error_length) == 0
                && socket_error == 0;
        }
    }

    close(socket_fd);
    if (!connected) {
        return false;
    }

    latency_us = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - started_at).count();
    return true;
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
    std::barrier start_gate(static_cast<std::ptrdiff_t>(options.clients + 1));
    std::vector<std::thread> clients;
    clients.reserve(options.clients);

    for (std::size_t index = 0; index < options.clients; ++index) {
        clients.emplace_back([&]() {
            start_gate.arrive_and_wait();

            long long latency_us = 0;
            if (connect_with_timeout(options, address, latency_us)) {
                results.successful_connections.fetch_add(1);
                results.total_latency_us.fetch_add(latency_us);
                record_longest_latency(results.longest_latency_us, latency_us);
            } else {
                results.failed_connections.fetch_add(1);
            }
        });
    }

    const auto started_at = std::chrono::steady_clock::now();
    start_gate.arrive_and_wait();
    for (std::thread& client : clients) {
        client.join();
    }
    const auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - started_at).count();

    const std::size_t successes = results.successful_connections.load();
    const std::size_t failures = results.failed_connections.load();
    const double average_latency_ms = successes == 0
        ? 0.0
        : static_cast<double>(results.total_latency_us.load()) / successes / 1'000.0;
    const double longest_latency_ms = static_cast<double>(results.longest_latency_us.load()) / 1'000.0;

    std::cout << std::fixed << std::setprecision(2)
              << "Server: " << options.host << ':' << options.port << '\n'
              << "Requested clients: " << options.clients << '\n'
              << "Successful connections: " << successes << '\n'
              << "Failed connections: " << failures << '\n'
              << "Elapsed: " << elapsed << " s\n"
              << "Connection rate: " << (elapsed == 0.0 ? 0.0 : successes / elapsed) << " connections/s\n"
              << "Average connection latency: " << average_latency_ms << " ms\n"
              << "Longest connection latency: " << longest_latency_ms << " ms\n";

    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}