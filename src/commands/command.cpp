#include "registry.h"

void register_commands() {
    Registry* registry = Registry::get_instance();

    registry->register_command({
        .id = 1,
        .name = "SET",
        .arguments = {
            { ValueType::String, false },
            { ValueType::String, false }
        },
        .handler = [](CommandContext& ctx, std::span<const Value> args) -> Value {
            const auto& key = std::get<std::string>(args[0]);
            const auto& value = std::get<std::string>(args[1]);

            ctx.db.set(key, value);
            return std::string("OK");
        }
    });

    registry->register_command({
        .id = 2,
        .name = "GET",
        .arguments = {
            { ValueType::String, false }
        },
        .handler = [](CommandContext& ctx, std::span<const Value> args) -> Value {
            const auto& key = std::get<std::string>(args[0]);
            auto value = ctx.db.get(key);

            if (!value) return Null{};
            return *value;
        }
    });

    registry->register_command({
        .id = 3,
        .name = "DEL",
        .arguments = {
            { ValueType::String, false }
        },
        .handler = [](CommandContext& ctx, std::span<const Value> args) -> Value {
            const auto& key = std::get<std::string>(args[0]);
            return int64_t{ctx.db.erase(key) ? 1 : 0};
        }
    });
}