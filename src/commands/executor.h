#ifndef EXECUTOR_H
#define EXECUTOR_H
#include "database/database.h"
#include "parser.h"
#include "registry.h"
#include "utils.h"
#include "command.h"

std::string execute_command(Database& db, std::string line, bool logging = true);

#endif