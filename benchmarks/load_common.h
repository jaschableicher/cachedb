#pragma once

#include <algorithm>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <stdexcept>
#include <cctype>
#include <cstdint>
#include <cstddef>
#include <exception>
#include <iomanip>
#include <iostream>
#include <latch>
#include <limits>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
#include <unistd.h>

namespace load {
using Clock = std::chrono::steady_clock;
enum class Mode { Get, Miss, Set, Overwrite, Mixed };
struct Options {
    std::size_t clients = 4;
    std::size_t requests = 1'000'000;
    std::size_t keys = 10'000;
    std::size_t value_size = 128;
    std::size_t warmup = 1'000; // Per worker; excluded from results.
    std::size_t pipeline = 1;
    std::size_t read_percent = 50;
    std::size_t port = 5634;
    std::size_t timeout_ms = 5'000;
    std::string host = "127.0.0.1";
    Mode mode = Mode::Get;
};
inline const char* mode_name(Mode mode) {
    switch (mode) {
        case Mode::Get: return "GET";
        case Mode::Miss: return "GET_MISS";
        case Mode::Set: return "SET";
        case Mode::Overwrite: return "OVERWRITE";
        case Mode::Mixed: return "MIX";
    }
    return "unknown";
}
inline void usage(const char* executable, bool network) {
    std::cout << "Usage: " << executable << " [options]\n"
        << "  --command GET|GET_MISS|SET|OVERWRITE|MIX (default: GET)\n"
        << "  --clients N       Concurrent workers/connections (default: 4)\n"
        << "  --requests N      Total measured operations (default: 1000000)\n"
        << "  --keys N          Shared keyspace; use 1 for a hot key (default: 10000)\n"
        << "  --value-size N    String payload bytes (default: 128)\n"
        << "  --warmup N        Unmeasured operations PER worker (default: 1000)\n"
        << "  --read-percent N  GET percentage in MIX, 0..100 (default: 50)\n";
    if (network) {
        std::cout << "  --pipeline N      Requests per pipeline batch (default: 1)\n"
            << "  --host IPv4       Server address (default: 127.0.0.1)\n"
            << "  --port N          Server port (default: 5634)\n"
            << "  --timeout-ms N    Socket inactivity timeout (default: 5000)\n";
    }
    std::cout << "GET/OVERWRITE/MIX preload --keys entries. GET_MISS expects nil.\n"
        << "SET inserts unique keys; --keys does not limit its dataset.\n"
        << "MIX uses deterministic uniform keys and a deterministic read/write mix.\n"
        << "Setup, warmup and thread creation are excluded from measurement.\n";
}
inline bool number(std::string_view text, std::size_t& value) {
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    return !text.empty() && error == std::errc{} && end == text.data() + text.size();
}
inline Options parse(int argc, char** argv, bool network) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string_view flag = argv[i];
        if (flag == "--help") { usage(argv[0], network); std::exit(EXIT_SUCCESS); }
        if (++i == argc) throw std::runtime_error("Missing value for " + std::string(flag));
        const std::string_view value = argv[i];
        if (flag == "--command") {
            std::string name(value);
            for (char& c : name) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            if (name == "GET") options.mode = Mode::Get;
            else if (name == "GET_MISS") options.mode = Mode::Miss;
            else if (name == "SET") options.mode = Mode::Set;
            else if (name == "OVERWRITE") options.mode = Mode::Overwrite;
            else if (name == "MIX" || name == "MIXED") options.mode = Mode::Mixed;
            else throw std::runtime_error("Unknown command: " + name);
            continue;
        }
        if (network && flag == "--host") { options.host = value; continue; }
        std::size_t parsed = 0;
        if (!number(value, parsed)) throw std::runtime_error("Invalid number for " + std::string(flag));
        if (flag == "--clients") options.clients = parsed;
        else if (flag == "--requests") options.requests = parsed;
        else if (flag == "--keys") options.keys = parsed;
        else if (flag == "--value-size") options.value_size = parsed;
        else if (flag == "--warmup") options.warmup = parsed;
        else if (flag == "--read-percent") options.read_percent = parsed;
        else if (network && flag == "--pipeline") options.pipeline = parsed;
        else if (network && flag == "--port") options.port = parsed;
        else if (network && flag == "--timeout-ms") options.timeout_ms = parsed;
        else throw std::runtime_error("Unknown option: " + std::string(flag));
    }
    if (options.clients == 0 || options.requests < options.clients || options.keys == 0 ||
        options.pipeline == 0 || options.read_percent > 100 || options.port == 0 ||
        options.port > 65535 || options.timeout_ms == 0 ||
        options.timeout_ms > static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
        options.value_size > 16 * 1024 * 1024 - 1024) {
        throw std::runtime_error("Invalid limits: requests >= clients > 0, keys/pipeline > 0, "
                                 "read-percent <= 100, port 1..65535, payload <= 16 MiB - 1 KiB");
    }
    return options;
}
inline uint64_t mix(uint64_t n) {
    n += 0x9e3779b97f4a7c15ULL;
    n = (n ^ (n >> 30)) * 0xbf58476d1ce4e5b9ULL;
    n = (n ^ (n >> 27)) * 0x94d049bb133111ebULL;
    return n ^ (n >> 31);
}
struct Workload {
    const Options& options;
    std::string prefix = "bench:" + std::to_string(getpid()) + ":" +
        std::to_string(Clock::now().time_since_epoch().count()) + ":";
    std::string value;
    std::vector<std::string> keys;
    explicit Workload(const Options& opts) : options(opts), value(opts.value_size, 'X') {
        if (opts.mode != Mode::Set) {
            keys.reserve(opts.keys);
            for (std::size_t i = 0; i < opts.keys; ++i)
                keys.push_back(prefix + "key:" + std::to_string(i));
        }
    }
    bool preload() const {
        return options.mode == Mode::Get || options.mode == Mode::Overwrite || options.mode == Mode::Mixed;
    }
    bool write(std::size_t index) const {
        return options.mode == Mode::Set || options.mode == Mode::Overwrite ||
            (options.mode == Mode::Mixed && mix(index ^ 0xa0761d6478bd642fULL) % 100 >= options.read_percent);
    }
    const std::string& key(std::size_t index, std::string& scratch) const {
        if (options.mode == Mode::Set) {
            scratch = prefix + "insert:" + std::to_string(index);
            return scratch;
        }
        return keys[mix(index) % keys.size()];
    }
};
struct Result {
    std::vector<uint64_t> latency_ns;
    std::size_t reads = 0;
    std::size_t writes = 0;
    std::string error;
    Clock::time_point finished;
};
inline double percentile(const std::vector<uint64_t>& sorted, double fraction) {
    if (sorted.empty()) return 0;
    const auto rank = static_cast<std::size_t>(std::ceil(fraction * static_cast<double>(sorted.size())));
    return static_cast<double>(sorted[std::max<std::size_t>(1, rank) - 1]) / 1000.0;
}

// Every worker completes setup/warmup before the main thread releases the gate.
// Failures and thread-creation errors release waiting workers rather than deadlocking.
template <typename Prepare, typename Execute>
int run(const Options& options, Prepare prepare, Execute execute) {
    std::vector<Result> results(options.clients);
    std::vector<std::thread> threads;
    threads.reserve(options.clients);
    std::latch ready(static_cast<std::ptrdiff_t>(options.clients));
    std::latch start(1);
    std::atomic<bool> abort{false};
    const auto base = options.requests / options.clients;
    const auto remainder = options.requests % options.clients;
    try {
        for (std::size_t id = 0; id < options.clients; ++id) {
            threads.emplace_back([&, id] {
                auto& result = results[id];
                const auto count = base + (id < remainder ? 1 : 0);
                const auto offset = id * base + std::min(id, remainder);
                try {
                    result.latency_ns.reserve(count);
                    prepare(id, offset);
                } catch (const std::exception& error) {
                    result.error = error.what();
                    abort.store(true);
                }
                ready.count_down();
                start.wait();
                if (!abort.load()) {
                    try { execute(id, offset, count, result); }
                    catch (const std::exception& error) { result.error = error.what(); }
                }
                result.finished = Clock::now();
            });
        }
    } catch (...) {
        abort.store(true);
        start.count_down();
        for (auto& thread : threads) thread.join();
        throw;
    }
    ready.wait();
    const auto began = Clock::now();
    start.count_down();
    for (auto& thread : threads) thread.join();
    auto ended = began;
    std::vector<uint64_t> latency;
    latency.reserve(options.requests);
    std::size_t reads = 0, writes = 0;
    bool errors = abort.load();
    for (std::size_t id = 0; id < results.size(); ++id) {
        auto& result = results[id];
        ended = std::max(ended, result.finished);
        reads += result.reads;
        writes += result.writes;
        latency.insert(latency.end(), result.latency_ns.begin(), result.latency_ns.end());
        if (!result.error.empty()) {
            errors = true;
            std::cerr << "Worker " << id << ": " << result.error << '\n';
        }
    }
    std::sort(latency.begin(), latency.end());
    const double seconds = std::chrono::duration<double>(ended - began).count();
    const auto failed = options.requests - latency.size();
    const bool valid = !errors && failed == 0;
    std::cout << "Status: " << (valid ? "OK" : "FAILED (not a valid performance baseline)")
        << " Command: " << mode_name(options.mode) << " Clients: " << options.clients
        << " Requests: " << options.requests << " Successful: " << latency.size()
        << " Failed/uncompleted: " << failed << " Reads: " << reads << " Writes: " << writes
        << std::fixed << std::setprecision(3) << " Duration: " << seconds << " s"
        << " Throughput: " << (seconds > 0 ? static_cast<double>(latency.size()) / seconds : 0)
        << " req/s\nLatency (successful requests): p50: " << percentile(latency, .50)
        << " us p95: " << percentile(latency, .95) << " us p99: " << percentile(latency, .99)
        << " us p999: " << percentile(latency, .999) << " us\n";
    return valid ? EXIT_SUCCESS : EXIT_FAILURE;
}
inline void describe(const Options& options, const Workload& workload, bool network) {
#ifndef NDEBUG
    std::cerr << "WARNING: Debug build; use the linux-prod Release preset for performance measurements.\n";
#endif
    std::cout << "Mode: " << mode_name(options.mode) << " Keys: " << (options.mode == Mode::Set ? std::string("unique inserts") : std::to_string(options.keys))
        << " Value bytes: " << options.value_size << " Warmup/worker: " << options.warmup
        << " Prefix: " << workload.prefix;
    if (network) std::cout << " Server: " << options.host << ':' << options.port << " Pipeline: " << options.pipeline;
    std::cout << "\nPreparing workload...\n" << std::flush;
}
} // namespace load
