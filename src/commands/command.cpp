#include "registry.h"
#include "../database/database.h"

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

    registry->register_command({
        .id=4,
        .name="EXPIRE",
        .arguments = {
            { ValueType::String, false },
            { ValueType::Int64, false }
        },
        .handler=[](CommandContext& ctx, std::span<const Value> args) -> Value{
            const auto& key = std::get<std::string>(args[0]);
            const auto& expires_in_seconds = std::get<int64_t>(args[1]);
            if(ctx.db.set_expiry(key, expires_in_seconds).has_value()){
                return expires_in_seconds;
            }
            return -1;
        }
    });

    registry->register_command({
        .id=5,
        .name="TTL",
        .arguments = {
            { ValueType::String, false }
        },
        .handler=[](CommandContext& ctx, std::span<const Value> args) -> Value{
            const auto& key = std::get<std::string>(args[0]);
            
            auto value = ctx.db.get_expiry(key);

            if (value==-1) return Null{};
            //calculate sedconds from now
            const auto epoch_now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch());
            const std::int64_t expires_in_seconds = value- epoch_now.count() ;
            return expires_in_seconds;
        }
    });
}
