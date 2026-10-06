#include "executor.h"
#include "logger/logger.h"
Value execute_command(Database& db, const Command& command){

    CommandContext context{db};
    ExecuteReturn result = Registry::get_instance()->execute(context,command); 
    return result.value;
}