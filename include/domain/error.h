/*
 * error.h - Reporte de errores del dominio.
 *
 * Decision de diseno: en C no hay excepciones, por lo que todas las
 * operaciones que pueden fallar reciben un `Error *` de salida y devuelven
 * `bool`. Esto obliga al llamador a decidir que hacer con el fallo y evita
 * los "fallos silenciosos" (exit() escondido en una capa interna o valores
 * centinela sin explicacion).
 */
#ifndef DOMAIN_ERROR_H
#define DOMAIN_ERROR_H

#include <stdbool.h>

typedef enum {
    ERROR_NONE = 0,
    ERROR_INVALID_ARGUMENT,
    ERROR_INVALID_STATE_TRANSITION,
    ERROR_DUPLICATED_PID,
    ERROR_PARSE,
    ERROR_IO,
    ERROR_OUT_OF_MEMORY,
    ERROR_SIMULATION_LIMIT
} ErrorCode;

#define ERROR_MESSAGE_CAPACITY 320

typedef struct {
    ErrorCode code;
    char message[ERROR_MESSAGE_CAPACITY];
} Error;

void error_clear(Error *error);
bool error_is_set(const Error *error);

/*
 * Registra el error y SIEMPRE devuelve false, de modo que el llamador pueda
 * escribir `return error_set(error, ...);` en una sola linea.
 * Acepta error == NULL (el llamador declara que no le interesa el detalle).
 */
bool error_set(Error *error, ErrorCode code, const char *format, ...);

const char *error_code_name(ErrorCode code);
const char *error_message(const Error *error);

#endif /* DOMAIN_ERROR_H */
