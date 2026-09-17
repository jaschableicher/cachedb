#include "executor.h"
#include "logger/logger.h"
Value execute_command(Database& db, const Command& command, bool logging){

    CommandContext context{db};
    ExecuteReturn result = Registry::get_instance()->execute(context,command); //Move from this to returning struct with CommandReturn enum and Value result, for this there is no need to string conversions, saves a little time
    
    
    if (result.state!=EXECUTEERROR && logging) {
        //TODO: fix aol to use msgpack data
        //Logger::get_instance()->log_command(line);
    }
    return result.value;
}