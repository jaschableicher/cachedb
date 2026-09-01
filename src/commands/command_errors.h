#ifndef COMMAND_ERRORS_H
#define COMMAND_ERRORS_H
#include <string>
#include <cstdint>

enum class ErrorCode : uint32_t {
    UnknownCommand,
    InvalidArgument,
    WrongType,
    NotFound,
    AlreadyExists,
    InternalError,
    ProtocolError
};

struct Error {
    ErrorCode code;
    std::string message;
};
#endif