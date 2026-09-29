#include <algorithm>
#include <arpa/inet.h>
#include <array>
#include <chrono>
#include <cerrno>
#include <charconv>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <limits>
#include <msgpack.hpp>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;
constexpr std::size_t MAX_FRAME_SIZE = 16 * 1024 * 1024;
constexpr std::size_t BATCH_BYTES = 64 * 1024;
constexpr std::size_t PIPELINE_DEPTH = 16;
constexpr const char* HOST = "127.0.0.1";
constexpr unsigned short PORT = 5634;
constexpr auto TIMEOUT = std::chrono::seconds(5);

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

struct Result {
    std::vector<uint64_t> latencies_us;
    std::vector<uint64_t> queue_latencies_us;
    std::size_t failed_requests = 0;
    std::size_t connection_errors = 0;
    std::size_t server_errors = 0;
    std::size_t successful_connections = 0;
    double setup_seconds = 0;
    double duration_seconds = 0;
    double cpu_seconds = 0;
};

void print_usage(const char* executable) {
    std::cout
        << "Usage: " << executable << " [options]\n"
        << "  --clients <count>       Default: 1000\n"
        << "  --requests <count>      Default: 1000000\n"
        << "  --command <SET|GET|MIX> Default: GET\n"
        << "  --value-size <bytes>    Default: 128\n";
}

bool parse_positive_number(std::string_view text, unsigned long long& value) {
    if (text.empty()) return false;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size() &&
        value <= std::numeric_limits<std::size_t>::max();
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
        } else if (argument == "--value-size" && parse_positive_number(value, parsed) && parsed <= MAX_FRAME_SIZE - 1024) {
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

std::vector<char> make_command_frame(const std::string& name, const std::string& key,
                                     const std::string* value = nullptr) {
    msgpack::sbuffer payload;
    msgpack::packer<msgpack::sbuffer> writer(payload);
    writer.pack_array(2);
    writer.pack(name);
    writer.pack_array(value == nullptr ? 1 : 2);

    auto pack_string = [&](const std::string& text) {
        writer.pack_array(2);
        writer.pack_uint8(6);
        writer.pack(text);
    };

    pack_string(key);
    if (value != nullptr) pack_string(*value);

    const auto size = static_cast<uint32_t>(payload.size());
    std::vector<char> frame(4 + payload.size());
    frame[0] = static_cast<char>(size >> 24);
    frame[1] = static_cast<char>(size >> 16);
    frame[2] = static_cast<char>(size >> 8);
    frame[3] = static_cast<char>(size);
    std::copy(payload.data(), payload.data() + payload.size(), frame.begin() + 4);
    return frame;
}

class LoadGenerator {
public:
    LoadGenerator(const Options& options, Result& result)
        : options_(options), result_(result),
          clients_(std::min(options.clients, options.total_requests)),
          dummy_value_(options.value_size_bytes, 'X') {
        epoll_fd_ = epoll_create1(EPOLL_CLOEXEC);
        if (epoll_fd_ < 0) throw std::runtime_error("Cannot create epoll instance");
    }

    ~LoadGenerator() {
        for (auto& client : clients_) {
            if (client.socket_fd >= 0) close(client.socket_fd);
        }
        close(epoll_fd_);
    }

    LoadGenerator(const LoadGenerator&) = delete;
    LoadGenerator& operator=(const LoadGenerator&) = delete;

    void run(const sockaddr_in& address) {
        result_.latencies_us.reserve(options_.total_requests);
        result_.queue_latencies_us.reserve(options_.total_requests);
        const auto setup_start = Clock::now();
        for (std::size_t index = 0; index < clients_.size(); ++index) {
            auto& client = clients_[index];
            client.target = options_.total_requests / clients_.size() +
                (index < options_.total_requests % clients_.size() ? 1 : 0);
            client.queued_at = Clock::now();
            client.socket_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
            if (client.socket_fd >= 0) ++active_;
            const int enabled = 1;
            if (client.socket_fd < 0 ||
                setsockopt(client.socket_fd, IPPROTO_TCP, TCP_NODELAY, &enabled, sizeof(enabled)) < 0) {
                fail(client);
                continue;
            }
            const int connected = connect(client.socket_fd,
                reinterpret_cast<const sockaddr*>(&address), sizeof(address));
            if (connected < 0 && errno != EINPROGRESS) {
                fail(client);
                continue;
            }
            epoll_event event{};
            event.events = EPOLLOUT;
            event.data.u64 = index;
            if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, client.socket_fd, &event) < 0) {
                fail(client);
                continue;
            }
            client.connecting = true;
            ++connecting_;
        }

        std::array<epoll_event, 256> events{};
        while (connecting_ != 0) {
            const int count = wait(events);
            for (int index = 0; index < count; ++index) {
                const auto client_index = static_cast<std::size_t>(events[index].data.u64);
                auto& client = clients_[client_index];
                if (client.socket_fd < 0 || !client.connecting) continue;
                int error = 0;
                socklen_t length = sizeof(error);
                if (getsockopt(client.socket_fd, SOL_SOCKET, SO_ERROR, &error, &length) < 0 || error != 0) {
                    fail(client);
                    continue;
                }
                client.connecting = false;
                --connecting_;
                if (!watch(client_index, EPOLLIN)) continue;
                ++result_.successful_connections;
            }
            expire(true);
        }
        for (std::size_t index = 0; index < clients_.size(); ++index) {
            if (clients_[index].socket_fd >= 0) prepare(index);
        }
        result_.setup_seconds = std::chrono::duration<double>(Clock::now() - setup_start).count();
        const auto started = Clock::now();
        const auto cpu_started = std::clock();
        for (auto& client : clients_) client.queued_at = started;

        while (active_ != 0) {
            const int count = wait(events);
            for (int index = 0; index < count; ++index) {
                const auto client_index = static_cast<std::size_t>(events[index].data.u64);
                auto& client = clients_[client_index];
                if (client.socket_fd < 0) continue;
                const auto ready = events[index].events;
                if ((ready & EPOLLOUT) != 0) send_request(client_index);
                if (client.socket_fd >= 0 && (ready & (EPOLLIN | EPOLLHUP | EPOLLERR)) != 0) {
                    receive(client_index);
                }
            }
            expire(false);
        }
        result_.duration_seconds = std::chrono::duration<double>(Clock::now() - started).count();
        result_.cpu_seconds = static_cast<double>(std::clock() - cpu_started) / CLOCKS_PER_SEC;
    }

private:
    struct RequestTiming {
        std::size_t begin = 0;
        std::size_t end = 0;
        Clock::time_point sent_at{};
        bool send_started = false;
    };

    struct Client {
        int socket_fd = -1;
        bool connecting = false;
        std::size_t target = 0;
        std::size_t completed = 0;
        std::array<RequestTiming, PIPELINE_DEPTH> requests{};
        std::size_t request_count = 0;
        std::size_t replies_received = 0;
        std::size_t sent = 0;
        std::vector<char> output;
        std::string input;
        Clock::time_point queued_at;
    };

    int wait(std::array<epoll_event, 256>& events) {
        const int count = epoll_wait(epoll_fd_, events.data(), static_cast<int>(events.size()), 10);
        if (count < 0 && errno != EINTR) throw std::runtime_error("epoll_wait failed");
        return std::max(count, 0);
    }

    void finish(Client& client) {
        if (client.connecting) {
            --connecting_;
            client.connecting = false;
        }
        if (client.socket_fd >= 0) {
            epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, client.socket_fd, nullptr);
            close(client.socket_fd);
            client.socket_fd = -1;
            --active_;
        }
    }

    void fail(Client& client) {
        result_.failed_requests += client.target - client.completed;
        ++result_.connection_errors;
        finish(client);
    }

    bool watch(std::size_t index, uint32_t events) {
        epoll_event event{};
        event.events = events;
        event.data.u64 = index;
        if (epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, clients_[index].socket_fd, &event) < 0) {
            fail(clients_[index]);
            return false;
        }
        return true;
    }

    void expire(bool connecting) {
        const auto now = Clock::now();
        if (now < next_timeout_check_) return;
        next_timeout_check_ = now + std::chrono::milliseconds(10);
        for (auto& client : clients_) {
            if (client.socket_fd >= 0 && client.connecting == connecting &&
                now - client.queued_at >= TIMEOUT) {
                fail(client);
            }
        }
    }

    void prepare(std::size_t index) {
        auto& client = clients_[index];
        client.queued_at = Clock::now();
        client.sent = 0;
        client.output.clear();
        client.request_count = 0;
        client.replies_received = 0;
        while (client.request_count < PIPELINE_DEPTH &&
               client.request_count < client.target - client.completed && client.output.size() < BATCH_BYTES) {
            const auto sequence = client.completed + client.request_count;
            const std::string key = "k_" + std::to_string(index) + "_" + std::to_string(sequence);
            const bool is_set = options_.command == CommandType::SET ||
                (options_.command == CommandType::MIXED && sequence % 2 == 0);
            const auto frame = make_command_frame(is_set ? "SET" : "GET", key, is_set ? &dummy_value_ : nullptr);
            auto& request = client.requests[client.request_count++];
            request = RequestTiming{};
            request.begin = client.output.size();
            client.output.insert(client.output.end(), frame.begin(), frame.end());
            request.end = client.output.size();
        }
        watch(index, EPOLLIN | EPOLLOUT);
    }

    void send_request(std::size_t index) {
        auto& client = clients_[index];
        const auto remaining = client.output.size() - client.sent;
        const auto send_size = std::min(remaining, BATCH_BYTES);
        const auto attempted_at = Clock::now();
        for (std::size_t index = 0; index < client.request_count; ++index) {
            auto& request = client.requests[index];
            if (!request.send_started && request.begin < client.sent + send_size && request.end > client.sent) {
                request.sent_at = attempted_at;
                request.send_started = true;
            }
        }
        const auto count = send(client.socket_fd, client.output.data() + client.sent,
            send_size, MSG_NOSIGNAL);
        if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) return;
        if (count <= 0) {
            fail(client);
            return;
        }
        client.sent += static_cast<std::size_t>(count);
        if (client.sent == client.output.size()) watch(index, EPOLLIN);
    }

    void receive(std::size_t index) {
        auto& client = clients_[index];
        std::array<char, 64 * 1024> buffer;
        const auto count = recv(client.socket_fd, buffer.data(), buffer.size(), 0);
        const auto received_at = Clock::now();
        if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) return;
        if (count <= 0) {
            fail(client);
            return;
        }
        client.input.append(buffer.data(), static_cast<std::size_t>(count));
        std::size_t offset = 0;
        while (client.input.size() - offset >= 4) {
            uint32_t header = 0;
            std::memcpy(&header, client.input.data() + offset, sizeof(header));
            const std::size_t size = ntohl(header);
            if (size == 0 || size > MAX_FRAME_SIZE || client.replies_received == client.request_count) {
                fail(client);
                return;
            }
            if (client.input.size() - offset < size + 4) break;
            const auto& request = client.requests[client.replies_received];
            if (!request.send_started || client.sent < request.end) {
                fail(client);
                return;
            }
            bool server_error = false;
            try {
                std::size_t unpacked = 0;
                const auto response = msgpack::unpack(client.input.data() + offset + 4, size, unpacked);
                const auto& root = response.get();
                if (unpacked != size || root.type != msgpack::type::ARRAY || root.via.array.size != 2) {
                    throw std::runtime_error("Invalid reply");
                }
                const auto type = root.via.array.ptr[0].as<uint8_t>();
                const auto& value = root.via.array.ptr[1];
                if (type < 1 || type > 7) throw std::runtime_error("Invalid reply type");
                if (type == 6 && value.type == msgpack::type::STR) {
                    const std::string_view text(value.via.str.ptr, value.via.str.size);
                    server_error = text.starts_with("ERR");
                }
            } catch (const std::exception&) {
                fail(client);
                return;
            }
            if (server_error) {
                ++result_.server_errors;
            } else {
                const auto latency = std::chrono::duration_cast<std::chrono::microseconds>(received_at - request.sent_at).count();
                const auto queue_latency = std::chrono::duration_cast<std::chrono::microseconds>(request.sent_at - client.queued_at).count();
                result_.latencies_us.push_back(static_cast<uint64_t>(latency));
                result_.queue_latencies_us.push_back(static_cast<uint64_t>(queue_latency));
            }
            ++client.completed;
            ++client.replies_received;
            offset += size + 4;
        }
        client.input.erase(0, offset);
        if (client.replies_received == client.request_count) {
            if (!client.input.empty() || client.sent != client.output.size()) {
                fail(client);
            } else if (client.completed == client.target) {
                finish(client);
            } else {
                prepare(index);
            }
        }
    }

    const Options& options_;
    Result& result_;
    std::vector<Client> clients_;
    std::string dummy_value_;
    int epoll_fd_ = -1;
    std::size_t connecting_ = 0;
    std::size_t active_ = 0;
    Clock::time_point next_timeout_check_{};
};

std::string format_number(std::size_t value) {
    std::string s = std::to_string(value);
    int insert_pos = static_cast<int>(s.length()) - 3;
    while (insert_pos > 0) {
        s.insert(insert_pos, "'");
        insert_pos -= 3;
    }
    return s;
}

double get_percentile_ms(const std::vector<uint64_t>& sorted_latencies, double percentile) {
    if (sorted_latencies.empty()) return 0.0;
    const std::size_t idx = static_cast<std::size_t>(percentile * static_cast<double>(sorted_latencies.size()) / 100.0);
    const std::size_t clamped_idx = std::min(idx, sorted_latencies.size() - 1);
    return static_cast<double>(sorted_latencies[clamped_idx]) / 1000.0;
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

    Result result;
    try {
        LoadGenerator generator(options, result);
        generator.run(address);
    } catch (const std::exception& error) {
        std::cerr << "Benchmark failed: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
    auto& all_latencies = result.latencies_us;
    std::sort(all_latencies.begin(), all_latencies.end());
    std::sort(result.queue_latencies_us.begin(), result.queue_latencies_us.end());

    const std::size_t total_successful_requests = all_latencies.size();
    const double rps = result.duration_seconds > 0
        ? static_cast<double>(total_successful_requests) / result.duration_seconds : 0;
    const double cpu_percent = result.duration_seconds > 0
        ? result.cpu_seconds / result.duration_seconds * 100.0 : 0;

    const double p50 = get_percentile_ms(all_latencies, 50.0);
    const double p95 = get_percentile_ms(all_latencies, 95.0);
    const double p99 = get_percentile_ms(all_latencies, 99.0);
    const double p999 = get_percentile_ms(all_latencies, 99.9);

    std::cout << "Requests: " << format_number(options.total_requests)
              << " Connections: " << format_number(result.successful_connections)
              << '/' << format_number(std::min(options.clients, options.total_requests))
              << std::fixed << std::setprecision(2)
              << " Throughput: " << rps << " req/s"
              << " Request latency: p50: " << p50 << " ms"
              << " p95: " << p95 << " ms"
              << " p99: " << p99 << " ms"
              << " p999: " << p999 << " ms\n"
              << "Client queueing: p50: " << get_percentile_ms(result.queue_latencies_us, 50.0)
              << " ms p99: " << get_percentile_ms(result.queue_latencies_us, 99.0) << " ms\n"
              << "Successful: " << total_successful_requests
              << " Transport/protocol failed or skipped: " << result.failed_requests
              << " Server errors: " << result.server_errors
              << " Connection/protocol errors: " << result.connection_errors << '\n'
              << "Setup: " << result.setup_seconds << " s"
              << " Measured: " << result.duration_seconds << " s"
              << " Generator CPU: " << cpu_percent << "% of one core"
              << " Max outstanding per connection: " << PIPELINE_DEPTH << '\n'
              << "Latency: per-request first send attempt to complete reply read; includes post-send queueing.\n";
    if (cpu_percent >= 85.0) {
        std::cerr << "Warning: generator CPU is near saturation; throughput may be client-limited. "
                  << "Use multiple generator processes on a separate machine.\n";
    }
    return (result.failed_requests == 0 && result.server_errors == 0 && result.connection_errors == 0)
        ? EXIT_SUCCESS : EXIT_FAILURE;
}
