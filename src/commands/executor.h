#ifndef EXECUTOR_H
#define EXECUTOR_H
#include "database/database.h"
#include "parser.h"
#include "registry.h"
#include "utils.h"
#include "command.h"

Value execute_command(Database& db, const Command& command, bool logging = true);

#endif