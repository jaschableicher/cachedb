#include "executor.h"
#include "logger/logger.h"
std::string execute_command(Database& db, std::string line, bool logging){
    Command command = parse_command(line);
    CommandContext context{db};
    ExecuteReturn result = Registry::get_instance()->execute(context,command); //Move from this to returning struct with CommandReturn enum and Value result, for this there is no need to string conversions, saves a little time
    
    
    if (result.state!=EXECUTEERROR && logging) {
        Logger::get_instance()->log_command(line);
    }
    std::string result_string = value_to_string(result.value);
    return result_string;
}