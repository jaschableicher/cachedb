#ifndef PARSER_H
#define PARSER_H
#include "command.h"
#include "utils.h"
#include "protocol/protocol.h"
Command parse_command(const char* data, std::size_t size);
#endif