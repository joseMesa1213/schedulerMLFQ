/*
 * csv_process_source.c - Adaptador de entrada: procesos desde un archivo CSV.
 *
 * Toda la fragilidad del mundo exterior (archivo inexistente, columnas de
 * menos, texto donde deberia haber numeros, burst negativo) se valida aqui y
 * se traduce a un Error con numero de linea. El dominio nunca ve una cadena.
 */
#include "infrastructure/process_sources.h"

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CSV_LINE_CAPACITY 512

typedef struct {
    ProcessSource base;
    char path[512];
    char description[576];
} CsvProcessSource;

static const char *csv_describe(const ProcessSource *self)
{
    return ((const CsvProcessSource *)self)->description;
}

static char *trim(char *text)
{
    while (*text != '\0' && isspace((unsigned char)*text)) {
        text++;
    }
    char *end = text + strlen(text);
    while (end > text && isspace((unsigned char)end[-1])) {
        *--end = '\0';
    }
    return text;
}

static bool is_blank_or_comment(const char *line)
{
    return line[0] == '\0' || line[0] == '#';
}

/* Convierte un campo a entero rechazando basura ("4abc" es un error, no un 4). */
static bool parse_int_field(const char *field, const char *field_name, int line_number,
                            int *out, Error *error)
{
    if (field == NULL || *field == '\0') {
        return error_set(error, ERROR_PARSE, "linea %d: falta la columna '%s'",
                         line_number, field_name);
    }
    char *end = NULL;
    long value = strtol(field, &end, 10);
    if (end == field || *trim(end) != '\0') {
        return error_set(error, ERROR_PARSE, "linea %d: '%s' no es un entero valido en '%s'",
                         line_number, field, field_name);
    }
    if (value < INT_MIN || value > INT_MAX) {
        return error_set(error, ERROR_PARSE, "linea %d: valor fuera de rango en '%s'",
                         line_number, field_name);
    }
    *out = (int)value;
    return true;
}

/*
 * La cabecera se reconoce por empezar con una letra, pero solo se acepta ANTES
 * del primer proceso: asi un archivo con comentarios previos a la cabecera
 * funciona, y una letra en medio de los datos sigue siendo un error de formato.
 */
static bool looks_like_header(const char *first_field, const ProcessTable *table)
{
    return process_table_size(table) == 0 && isalpha((unsigned char)first_field[0]) != 0;
}

static bool parse_line(char *line, int line_number, ProcessTable *table, Error *error)
{
    char *raw_pid_field = strtok(line, ",;\n");
    if (raw_pid_field == NULL) {
        return true; /* linea vacia */
    }
    char *pid_field = trim(raw_pid_field);
    if (is_blank_or_comment(pid_field)) {
        return true; /* linea vacia o comentario: se ignora en silencio */
    }
    if (looks_like_header(pid_field, table)) {
        return true; /* cabecera del CSV */
    }

    char *arrival_field = strtok(NULL, ",;\n");
    char *burst_field = strtok(NULL, ",;\n");

    int pid = 0;
    int arrival_time = 0;
    int burst_time = 0;
    if (!parse_int_field(pid_field, "pid", line_number, &pid, error) ||
        !parse_int_field(arrival_field ? trim(arrival_field) : NULL, "arrival_time", line_number,
                         &arrival_time, error) ||
        !parse_int_field(burst_field ? trim(burst_field) : NULL, "burst_time", line_number,
                         &burst_time, error)) {
        return false;
    }
    if (!process_table_emplace(table, pid, arrival_time, burst_time, error)) {
        /* Se enriquece el error del dominio con la ubicacion en el archivo. */
        Error detail = *error;
        return error_set(error, detail.code, "linea %d: %s", line_number, detail.message);
    }
    return true;
}

static bool csv_load(ProcessSource *self, ProcessTable *table, Error *error)
{
    CsvProcessSource *source = (CsvProcessSource *)self;
    FILE *file = fopen(source->path, "r");
    if (file == NULL) {
        return error_set(error, ERROR_IO, "no se pudo abrir el archivo de procesos '%s'",
                         source->path);
    }

    char line[CSV_LINE_CAPACITY];
    int line_number = 0;
    while (fgets(line, sizeof(line), file) != NULL) {
        line_number++;
        if (!parse_line(line, line_number, table, error)) {
            fclose(file);
            return false;
        }
    }
    fclose(file);

    if (process_table_size(table) == 0) {
        return error_set(error, ERROR_PARSE, "el archivo '%s' no contiene procesos validos",
                         source->path);
    }
    return true;
}

static void csv_destroy(ProcessSource *self)
{
    free(self);
}

static const ProcessSourceVTable kCsvVTable = { csv_describe, csv_load, csv_destroy };

ProcessSource *csv_process_source_create(const char *path, Error *error)
{
    if (path == NULL || path[0] == '\0') {
        error_set(error, ERROR_INVALID_ARGUMENT, "ruta de archivo de procesos vacia");
        return NULL;
    }
    CsvProcessSource *source = calloc(1, sizeof(*source));
    if (source == NULL) {
        error_set(error, ERROR_OUT_OF_MEMORY, "sin memoria para la fuente CSV");
        return NULL;
    }
    snprintf(source->path, sizeof(source->path), "%s", path);
    snprintf(source->description, sizeof(source->description), "archivo CSV '%s'", source->path);
    source->base.vtable = &kCsvVTable;
    return &source->base;
}
