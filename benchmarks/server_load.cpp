#include "load_common.h"

#include <array>
#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <memory>
#include <msgpack.hpp>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>

namespace {
constexpr std::size_t max_frame = 16 * 1024 * 1024;
enum class Expected { Ok, Value, Missing, Deleted };
struct Request {
    std::vector<char> frame;
    Expected expected;
    load::Clock::time_point started{};
};
Request request(const std::string& command, const std::string& key,
                const std::string* value, Expected expected) {
    msgpack::sbuffer payload;
    msgpack::packer<msgpack::sbuffer> writer(payload);
    writer.pack_array(2);
    writer.pack(command);
    writer.pack_array(value ? 2 : 1);
    auto string_arg = [&](const std::string& text) {
        writer.pack_array(2);
        writer.pack_uint8(6);
        writer.pack(text);
    };
    string_arg(key);
    if (value) string_arg(*value);
    if (payload.size() > max_frame) throw std::runtime_error("Request exceeds server frame limit");
    Request result{{}, expected, {}};
    result.frame.resize(4 + payload.size());
    const auto size = static_cast<uint32_t>(payload.size());
    result.frame[0] = static_cast<char>(size >> 24);
    result.frame[1] = static_cast<char>(size >> 16);
    result.frame[2] = static_cast<char>(size >> 8);
    result.frame[3] = static_cast<char>(size);
    std::copy(payload.data(), payload.data() + payload.size(), result.frame.begin() + 4);
    return result;
}
void validate(const std::vector<char>& response, Expected expected, const std::string& value) {
    std::size_t offset = 0;
    const auto handle = msgpack::unpack(response.data(), response.size(), offset);
    const auto& root = handle.get();
    if (offset != response.size() || root.type != msgpack::type::ARRAY || root.via.array.size != 2)
        throw std::runtime_error("Malformed MessagePack reply");
    const auto tag = root.via.array.ptr[0].as<uint8_t>();
    const auto& payload = root.via.array.ptr[1];
    if (expected == Expected::Missing && tag == 1 && payload.type == msgpack::type::NIL) return;
    if (expected == Expected::Deleted && tag == 3 && payload.as<int64_t>() == 1) return;
    if (tag == 6 && payload.type == msgpack::type::STR) {
        const std::string_view text(payload.via.str.ptr, payload.via.str.size);
        if (expected == Expected::Ok && text == "OK") return;
        if (expected == Expected::Value && text == value) return;
        throw std::runtime_error("Unexpected string/error reply: " + std::string(text.substr(0, 160)));
    }
    throw std::runtime_error("Unexpected reply type or value");
}

class Connection {
    int fd_ = -1;
    int timeout_ms_;
    static void fail(const char* action) {
        throw std::runtime_error(std::string(action) + ": " + std::strerror(errno));
    }
    short wait(short events, load::Clock::time_point progress) const {
        for (;;) {
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                load::Clock::now() - progress).count();
            if (elapsed >= timeout_ms_) throw std::runtime_error("Socket inactivity timeout");
            pollfd descriptor{fd_, events, 0};
            const int ready = poll(&descriptor, 1, timeout_ms_ - static_cast<int>(elapsed));
            if (ready < 0 && errno == EINTR) continue;
            if (ready < 0) fail("poll");
            if (ready == 0) throw std::runtime_error("Socket inactivity timeout");
            if (descriptor.revents & POLLNVAL) throw std::runtime_error("Invalid socket");
            return descriptor.revents;
        }
    }
public:
    explicit Connection(const load::Options& options) : timeout_ms_(static_cast<int>(options.timeout_ms)) {
        fd_ = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
        if (fd_ < 0) fail("socket");
        try {
            const int enabled = 1;
            if (setsockopt(fd_, IPPROTO_TCP, TCP_NODELAY, &enabled, sizeof(enabled)) < 0) fail("TCP_NODELAY");
            sockaddr_in address{};
            address.sin_family = AF_INET;
            address.sin_port = htons(static_cast<uint16_t>(options.port));
            if (inet_pton(AF_INET, options.host.c_str(), &address.sin_addr) != 1)
                throw std::runtime_error("--host must be an IPv4 address");
            if (connect(fd_, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) < 0) {
                if (errno != EINPROGRESS) fail("connect");
                wait(POLLOUT, load::Clock::now());
                int error = 0;
                socklen_t size = sizeof(error);
                if (getsockopt(fd_, SOL_SOCKET, SO_ERROR, &error, &size) < 0) fail("getsockopt");
                if (error) { errno = error; fail("connect"); }
            }
        } catch (...) { close(fd_); fd_ = -1; throw; }
    }
    ~Connection() { if (fd_ >= 0) close(fd_); }
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;

    // Read and write concurrently so a large pipeline cannot deadlock on full
    // socket buffers. Each request is timed from its first successful send to
    // receipt of its entire reply. Validation is outside that latency interval.
    void exchange(std::vector<Request>& batch, const std::string& value, load::Result* result = nullptr) {
        std::size_t sending = 0, sent = 0, receiving = 0, header_used = 0, payload_used = 0;
        std::array<unsigned char, 4> header{};
        std::vector<char> payload;
        auto progress = load::Clock::now();
        while (receiving < batch.size()) {
            const short events = wait(static_cast<short>(POLLIN | (sending < batch.size() ? POLLOUT : 0)), progress);
            if ((events & POLLOUT) && sending < batch.size()) {
                auto& current = batch[sending];
                const auto began = load::Clock::now();
                const auto count = send(fd_, current.frame.data() + sent, current.frame.size() - sent, MSG_NOSIGNAL);
                if (count > 0) {
                    if (sent == 0) current.started = began;
                    sent += static_cast<std::size_t>(count);
                    progress = load::Clock::now();
                    if (sent == current.frame.size()) { ++sending; sent = 0; }
                } else if (count < 0 && errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) fail("send");
                else if (count == 0) throw std::runtime_error("Socket closed while sending");
            }
            if (events & (POLLIN | POLLHUP | POLLERR)) {
                const bool reading_header = header_used < header.size();
                char* destination = reading_header ? reinterpret_cast<char*>(header.data()) + header_used :
                    payload.data() + payload_used;
                const auto remaining = reading_header ? header.size() - header_used : payload.size() - payload_used;
                const auto count = recv(fd_, destination, remaining, 0);
                if (count == 0) throw std::runtime_error("Server closed the connection");
                if (count < 0) {
                    if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
                        if (events & (POLLHUP | POLLERR)) throw std::runtime_error("Socket hangup/error");
                        continue;
                    }
                    fail("recv");
                }
                progress = load::Clock::now();
                if (reading_header) {
                    header_used += static_cast<std::size_t>(count);
                    if (header_used == header.size()) {
                        const uint32_t size = (static_cast<uint32_t>(header[0]) << 24) |
                            (static_cast<uint32_t>(header[1]) << 16) |
                            (static_cast<uint32_t>(header[2]) << 8) | header[3];
                        if (size == 0 || size > max_frame) throw std::runtime_error("Invalid reply frame length");
                        payload.resize(size);
                    }
                } else {
                    payload_used += static_cast<std::size_t>(count);
                    if (payload_used == payload.size()) {
                        const auto ended = load::Clock::now();
                        const auto& current = batch[receiving];
                        if (current.started == load::Clock::time_point{}) throw std::runtime_error("Unsolicited reply");
                        validate(payload, current.expected, value);
                        if (result) {
                            result->latency_ns.push_back(static_cast<uint64_t>(
                                std::chrono::duration_cast<std::chrono::nanoseconds>(ended - current.started).count()));
                            if (current.expected == Expected::Ok) ++result->writes; else ++result->reads;
                        }
                        ++receiving;
                        header_used = 0;
                        payload_used = 0;
                    }
                }
            }
        }
    }
};
} // namespace

int main(int argc, char** argv) {
    try {
        const auto options = load::parse(argc, argv, true);
        const load::Workload workload(options);
        load::describe(options, workload, true);
        if (workload.preload()) {
            Connection setup(options);
            for (std::size_t offset = 0; offset < workload.keys.size();) {
                const auto count = std::min(options.pipeline, workload.keys.size() - offset);
                std::vector<Request> batch;
                batch.reserve(count);
                for (std::size_t i = 0; i < count; ++i)
                    batch.push_back(request("SET", workload.keys[offset + i], &workload.value, Expected::Ok));
                setup.exchange(batch, workload.value);
                offset += count;
            }
        }
        std::vector<std::unique_ptr<Connection>> connections(options.clients);
        auto execute = [&](std::size_t id, std::size_t offset, std::size_t count,
                           load::Result* result, const std::string* warm_key) {
            std::string scratch;
            for (std::size_t done = 0; done < count;) {
                const auto length = std::min(options.pipeline, count - done);
                std::vector<Request> batch;
                batch.reserve(length);
                for (std::size_t i = 0; i < length; ++i) {
                    const auto index = offset + done + i;
                    const bool write = workload.write(index);
                    const auto& key = warm_key ? *warm_key : workload.key(index, scratch);
                    batch.push_back(request(write ? "SET" : "GET", key, write ? &workload.value : nullptr,
                        write ? Expected::Ok : options.mode == load::Mode::Miss ? Expected::Missing : Expected::Value));
                }
                connections[id]->exchange(batch, workload.value, result);
                done += length;
            }
        };
        auto prepare = [&](std::size_t id, std::size_t offset) {
            connections[id] = std::make_unique<Connection>(options);
            const auto warm_key = workload.prefix + "warm:" + std::to_string(id);
            const bool inserting = options.mode == load::Mode::Set;
            execute(id, offset, options.warmup, nullptr, inserting ? &warm_key : nullptr);
            if (inserting && options.warmup != 0) {
                std::vector<Request> cleanup;
                cleanup.push_back(request("DEL", warm_key, nullptr, Expected::Deleted));
                connections[id]->exchange(cleanup, workload.value);
            }
        };
        return load::run(options, prepare, [&](std::size_t id, std::size_t offset,
                                               std::size_t count, load::Result& result) {
            execute(id, offset, count, &result, nullptr);
        });
    } catch (const std::exception& error) {
        std::cerr << "Benchmark failed: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
