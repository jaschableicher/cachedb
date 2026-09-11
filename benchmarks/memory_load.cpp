#include <algorithm>
#include <arpa/inet.h>
#include <atomic>
#include <chrono>
#include <cerrno>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <netinet/in.h>
#include <sstream>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace {

constexpr const char* HOST = "127.0.0.1";
constexpr unsigned short PORT = 5634;
constexpr int TIMEOUT_SEC = 5;

enum class CommandType {
    SET,
    GET,
    MIXED
};

struct Options {
    std::size_t clients = 1000;
    std::size_t total_requests = 1000000;
    CommandType command = CommandType::GET;
    std::size_t value_size_bytes = 128;
};

struct WorkerResult {
    std::vector<uint32_t> latencies_us;
    std::size_t failed_requests = 0;
    bool connection_failed = false;
};

void print_usage(const char* executable) {
    std::cout
        << "Usage: " << executable << " [options]\n"
        << "  --clients <count>       Total concurrent connections (default: 1000)\n"
        << "  --requests <count>      Total requests to distribute across clients (default: 1000000)\n"
        << "  --command <SET|GET|MIX> Command to benchmark (default: GET)\n"
        << "  --value-size <bytes>    Payload size for SET (default: 128)\n";
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
        if (argument == "--clients" && parse_positive_number(value, parsed) && parsed > 0) {
            options.clients = static_cast<std::size_t>(parsed);
        } else if (argument == "--requests" && parse_positive_number(value, parsed) && parsed > 0) {
            options.total_requests = static_cast<std::size_t>(parsed);
        } else if (argument == "--value-size" && parse_positive_number(value, parsed)) {
            options.value_size_bytes = static_cast<std::size_t>(parsed);
        } else if (argument == "--command") {
            if (value == "SET" || value == "set") {
                options.command = CommandType::SET;
            } else if (value == "GET" || value == "get") {
                options.command = CommandType::GET;
            } else if (value == "MIXED" || value == "mix" || value == "MIX") {
                options.command = CommandType::MIXED;
            } else {
                return false;
            }
        } else {
            return false;
        }
    }
    return true;
}

int connect_to_server(const sockaddr_in& address) {
    const int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) return -1;

    struct timeval tv{};
    tv.tv_sec = TIMEOUT_SEC;
    tv.tv_usec = 0;
    setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(socket_fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    if (connect(socket_fd, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) < 0) {
        close(socket_fd);
        return -1;
    }
    return socket_fd;
}

bool send_command_and_read_response(int socket_fd, const std::string& command, std::string& response) {
    std::size_t total_sent = 0;
    while (total_sent < command.size()) {
        const ssize_t sent = send(socket_fd, command.data() + total_sent, command.size() - total_sent, 0);
        if (sent <= 0) return false;
        total_sent += sent;
    }

    char buffer[4096];
    response.clear();
    while (true) {
        const ssize_t received = recv(socket_fd, buffer, sizeof(buffer) - 1, 0);
        if (received <= 0) return false;
        buffer[received] = '\0';
        response.append(buffer, received);
        if (response.find('\n') != std::string::npos) {
            break;
        }
    }
    return true;
}

void execute_benchmark(std::size_t client_id, std::size_t num_requests, const Options& options,
                       const sockaddr_in& address, WorkerResult& result) {
    const int sock_fd = connect_to_server(address);
    if (sock_fd < 0) {
        result.connection_failed = true;
        result.failed_requests = num_requests;
        return;
    }

    result.latencies_us.reserve(num_requests);
    const std::string dummy_value(options.value_size_bytes, 'X');
    std::string response;
    response.reserve(4096);

    for (std::size_t i = 0; i < num_requests; ++i) {
        const std::string key = "k_" + std::to_string(client_id) + "_" + std::to_string(i);

        std::string cmd;
        if (options.command == CommandType::SET) {
            cmd = "SET " + key + " " + dummy_value + "\r\n";
        } else if (options.command == CommandType::GET) {
            cmd = "GET " + key + "\r\n";
        } else {
            cmd = (i % 2 == 0) ? ("SET " + key + " " + dummy_value + "\r\n")
                               : ("GET " + key + "\r\n");
        }

        const auto start_time = std::chrono::steady_clock::now();
        if (send_command_and_read_response(sock_fd, cmd, response)) {
            const auto latency = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - start_time).count();
            result.latencies_us.push_back(static_cast<uint32_t>(latency));
        } else {
            result.failed_requests += (num_requests - i);
            break;
        }
    }

    close(sock_fd);
}

std::string format_number(std::size_t value) {
    std::string s = std::to_string(value);
    int insert_pos = static_cast<int>(s.length()) - 3;
    while (insert_pos > 0) {
        s.insert(insert_pos, "'");
        insert_pos -= 3;
    }
    return s;
}

double get_percentile_ms(const std::vector<uint32_t>& sorted_latencies, double percentile) {
    if (sorted_latencies.empty()) return 0.0;
    const std::size_t idx = static_cast<std::size_t>(percentile * sorted_latencies.size() / 100.0);
    const std::size_t clamped_idx = std::min(idx, sorted_latencies.size() - 1);
    return sorted_latencies[clamped_idx] / 1000.0;
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
    address.sin_port = htons(PORT);
    if (inet_pton(AF_INET, HOST, &address.sin_addr) != 1) {
        std::cerr << "Invalid IPv4 address: " << HOST << '\n';
        return EXIT_FAILURE;
    }

    std::vector<WorkerResult> worker_results(options.clients);
    std::vector<std::thread> workers;
    workers.reserve(options.clients);

    const std::size_t base_requests = options.total_requests / options.clients;
    const std::size_t remainder = options.total_requests % options.clients;

    const auto start_time = std::chrono::steady_clock::now();

    for (std::size_t i = 0; i < options.clients; ++i) {
        const std::size_t reqs = base_requests + (i < remainder ? 1 : 0);
        workers.emplace_back(execute_benchmark, i, reqs, std::cref(options),
                             std::cref(address), std::ref(worker_results[i]));
    }

    for (auto& worker : workers) {
        worker.join();
    }

    const auto end_time = std::chrono::steady_clock::now();
    const auto duration_sec = std::chrono::duration<double>(end_time - start_time).count();

    std::vector<uint32_t> all_latencies;
    all_latencies.reserve(options.total_requests);

    std::size_t failed_requests = 0;
    std::size_t successful_connections = 0;

    for (const auto& res : worker_results) {
        if (!res.connection_failed) {
            successful_connections++;
        }
        failed_requests += res.failed_requests;
        all_latencies.insert(all_latencies.end(), res.latencies_us.begin(), res.latencies_us.end());
    }

    std::sort(all_latencies.begin(), all_latencies.end());

    const std::size_t total_successful_requests = all_latencies.size();
    const std::size_t rps = duration_sec > 0 ? static_cast<std::size_t>(total_successful_requests / duration_sec) : 0;

    const double p50 = get_percentile_ms(all_latencies, 50.0);
    const double p95 = get_percentile_ms(all_latencies, 95.0);
    const double p99 = get_percentile_ms(all_latencies, 99.0);
    const double p999 = get_percentile_ms(all_latencies, 99.9);

    std::cout << "Requests: " << format_number(options.total_requests)
              << " Connections: " << format_number(successful_connections)
              << " Throughput: " << format_number(rps) << " req/s"
              << std::fixed << std::setprecision(2)
              << " Latency: p50: " << p50 << " ms"
              << " p95: " << p95 << " ms"
              << " p99: " << p99 << " ms"
              << " p999: " << p999 << " ms\n";

    return (failed_requests == 0 && successful_connections == options.clients) ? EXIT_SUCCESS : EXIT_FAILURE;
}
