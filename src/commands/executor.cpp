#include "executor.h"
#include "parser.h"
#include "registry.h"
#include "utils.h"
std::string execute_command(Database& db, std::string line){
    Command command = parse_command(line);
    CommandContext context{db};
    Value result = Registry::get_instance()->execute(context,command);
    return value_to_string(result);
}