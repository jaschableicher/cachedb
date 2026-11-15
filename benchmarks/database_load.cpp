#include "load_common.h"
#include "database/database.h"

int main(int argc, char** argv) {
    try {
        const auto options = load::parse(argc, argv, false);
        const load::Workload workload(options);
        load::describe(options, workload, false);
        Database database;
        const Value value = workload.value;
        if (workload.preload())
            for (const auto& key : workload.keys) database.set(key, value);

        auto operation = [&](const std::string& key, bool write) {
            if (write) {
                database.set(key, value);
                return true;
            }
            const auto reply = database.get(key);
            if (options.mode == load::Mode::Miss) return !reply.has_value();
            return reply && std::holds_alternative<std::string>(*reply) &&
                std::get<std::string>(*reply) == workload.value;
        };
        auto prepare = [&](std::size_t id, std::size_t offset) {
            std::string scratch;
            const auto warm_key = workload.prefix + "warm:" + std::to_string(id);
            for (std::size_t i = 0; i < options.warmup; ++i) {
                const auto& key = options.mode == load::Mode::Set ? warm_key : workload.key(offset + i, scratch);
                if (!operation(key, workload.write(offset + i)))
                    throw std::runtime_error("Warmup returned an unexpected value");
            }
            if (options.mode == load::Mode::Set && options.warmup != 0) database.erase(warm_key);
        };
        auto execute = [&](std::size_t, std::size_t offset, std::size_t count, load::Result& result) {
            std::string scratch;
            for (std::size_t i = 0; i < count; ++i) {
                const auto index = offset + i;
                const auto& key = workload.key(index, scratch);
                const bool write = workload.write(index);
                const auto started = load::Clock::now();
                // Stop timing before validation; throughput includes harness overhead.
                std::optional<Value> reply;
                if (write) database.set(key, value);
                else reply = database.get(key);
                const auto ended = load::Clock::now();
                if (!write) {
                    const bool valid = options.mode == load::Mode::Miss ? !reply.has_value() :
                        reply && std::holds_alternative<std::string>(*reply) &&
                        std::get<std::string>(*reply) == workload.value;
                    if (!valid) throw std::runtime_error("GET returned an unexpected value for " + key);
                }
                result.latency_ns.push_back(static_cast<uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(ended - started).count()));
                if (write) ++result.writes; else ++result.reads;
            }
        };
        return load::run(options, prepare, execute);
    } catch (const std::exception& error) {
        std::cerr << "Benchmark failed: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
