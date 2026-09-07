#include "executor.h"
#include "logger/logger.h"
std::string execute_command(Database& db, std::string line, bool logging){
    Command command = parse_command(line);
    CommandContext context{db};
    Value result = Registry::get_instance()->execute(context,command);
    std::string result_string = value_to_string(result);
    
    if (result_string.rfind("ERR", 0) != 0 && logging) {
        Logger::get_instance()->log_command(line);
    }
    return result_string;
}