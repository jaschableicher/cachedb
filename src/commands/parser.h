#ifndef PARSER_H
#define PARSER_H
#include "command.h"

Command parse_command(std::string_view input);

#endif