#include "domain/error.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

void error_clear(Error *error)
{
    if (error == NULL) {
        return;
    }
    error->code = ERROR_NONE;
    error->message[0] = '\0';
}

bool error_is_set(const Error *error)
{
    return error != NULL && error->code != ERROR_NONE;
}

bool error_set(Error *error, ErrorCode code, const char *format, ...)
{
    if (error == NULL) {
        return false;
    }
    error->code = code;

    va_list args;
    va_start(args, format);
    vsnprintf(error->message, sizeof(error->message), format, args);
    va_end(args);
    return false;
}

const char *error_code_name(ErrorCode code)
{
    switch (code) {
    case ERROR_NONE:                     return "NONE";
    case ERROR_INVALID_ARGUMENT:         return "INVALID_ARGUMENT";
    case ERROR_INVALID_STATE_TRANSITION: return "INVALID_STATE_TRANSITION";
    case ERROR_DUPLICATED_PID:           return "DUPLICATED_PID";
    case ERROR_PARSE:                    return "PARSE";
    case ERROR_IO:                       return "IO";
    case ERROR_OUT_OF_MEMORY:            return "OUT_OF_MEMORY";
    case ERROR_SIMULATION_LIMIT:         return "SIMULATION_LIMIT";
    }
    return "UNKNOWN";
}

const char *error_message(const Error *error)
{
    if (error == NULL || error->message[0] == '\0') {
        return "(sin detalle)";
    }
    return error->message;
}
