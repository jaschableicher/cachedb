#include "registry.h"
#include "../database/database.h"

void register_commands() {
    Registry* registry = Registry::get_instance();

    registry->register_command({
        .id = 1,
        .name = "SET",
        .arguments = {
            { ValueType::String, false },
            { ValueType::Any, false }
        },
        .handler = [](CommandContext& ctx, std::span<const Value> args) -> Value {
            const auto& key = std::get<std::string>(args[0]);
            

            ctx.db.set(key, args[1]);
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

    registry->register_command({
        .id=6,
        .name="SAVE",
        .arguments = {
            { ValueType::String, true } // Can provide a custom .db location , must include filename with .db!
        },
        .handler=[](CommandContext& ctx, std::span<const Value> args) -> Value{
            //Calls logger saving and dumps the memory into a .db file
            if(!args.empty()){
                //check if it is a valid location to dump to
                //If a file of that .db name exists simply overwrite it
                const auto& dump_location = std::get<std::string>(args[0]);
                if(!dump_location.ends_with(".db")) 
                    return "ERR: dump file must be a .db file";
                //If an absolute path is given, save the db there
                //If only relative, save in subpath of standard dump location
                

            }
            std::string filename = "dump.db";
            //Dump simply in dump.db for now to test
            return ctx.db.create_memory_snapshot(filename) ? "Successfully dumped memory" : "ERR: Dumping memory boom";
            
        }
    });

    registry->register_command({
        .id=7,
        .name="LOAD_SNAPSHOT",
        .arguments = {
            { ValueType::String, true } // optional file to load in, if file is selected, wal log will not be checked to load back newer values into cache!
        },
        .handler = [](CommandContext& ctx, std::span<const Value> args) -> Value{
            if(!args.empty()){

            }
            std::string filename = "dump.db";
            //Simply read in dump.db for now to test!
            switch(ctx.db.read_memory_snapshot(filename)){
                case SnapshotReturn::Success:
                    return "Success";
                default:
                    return "ERR: Unknown error";
            };
        }
    });
}
