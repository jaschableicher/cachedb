#include "executor.h"
#include "parser.h"
std::string execute_command(Database& db, std::string line){
    Command command = parse_command(line);
    switch(command.type){
        case CommandType::Set: {
            if(command.args.size()<2){
                return "Error setting key. Too few values";
            }
            db.set(command.args[0],command.args[1]);
            return "OK";
        }
        case CommandType::Del: {
            if(command.args.size()<1){
                return "Error setting key. Too few values";
            }
            bool hasDeleted = db.erase(command.args[0]);
            return std::string(hasDeleted ? "1" : "0");//For later incase more values are deleted
        }
        case CommandType::Get: {
            if(command.args.size()<1){
                return "Error setting key. Too few values";
            }
            auto getter = db.get(command.args[0]);
            return getter.has_value()?getter.value() : "(nil)";
        }
        case CommandType::Unknown: {
            return "Unknown command";
        }
    }
    return "Unknown error";
}