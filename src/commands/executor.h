#ifndef EXECUTOR_H
#define EXECUTOR_H
#include "database/database.h"

std::string execute_command(Database& db, std::string line);

#endif