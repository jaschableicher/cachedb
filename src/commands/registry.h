//command registry
#include <unordered_map>
#include <string>
#include <optional>
#include "command.h"


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
    Value execute(CommandContext& context,const Command& command) const;

private:
    static Registry* instance_;
    Registry() = default;
    ~Registry() = default;
    std::unordered_map<std::string, CommandDescriptor> commands_;
    bool value_matches_type(const Value& value, ValueType expected) const;
};