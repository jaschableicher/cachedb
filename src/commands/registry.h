#ifndef REGISTRY_H
#define REGISTRY_H

//command registry
#include <unordered_map>
#include <string>
#include <optional>
#include "command.h"

enum ExecuteState{
    EXECUTESUCCESS,
    EXECUTEERROR
};
struct ExecuteReturn{
    Value value;
    ExecuteState state;
};
class Registry{
public:
    static Registry* get_instance(){
        if(instance_==nullptr){
            instance_=new Registry();
        }
        return instance_;
    }
    void register_command(CommandDescriptor command_descriptor);
    const CommandDescriptor* lookup(std::string_view id) const;
    ExecuteReturn execute(CommandContext& context,const Command& command) const;

private:
    static Registry* instance_;
    Registry() = default;
    ~Registry() = default;
    std::unordered_map<std::string, CommandDescriptor> commands_;
    bool value_matches_type(const Value& value, ValueType expected) const;
    bool coerce_value(Value& value, ValueType expected) const;
};

#endif
