/*
 * process_source.h - PUERTO de entrada: de donde salen los procesos.
 *
 * Es a la vez un Factory Method (cada implementacion fabrica los Process) y el
 * punto de inversion de dependencias: el caso de uso depende de esta interfaz,
 * mientras que "leer un CSV" o "generar procesos aleatorios" son detalles de
 * infraestructura que la implementan.
 */
#ifndef APPLICATION_PORTS_PROCESS_SOURCE_H
#define APPLICATION_PORTS_PROCESS_SOURCE_H

#include "domain/error.h"
#include "domain/process_table.h"

typedef struct ProcessSource ProcessSource;

typedef struct ProcessSourceVTable {
    const char *(*describe)(const ProcessSource *self);
    bool (*load)(ProcessSource *self, ProcessTable *table, Error *error);
    void (*destroy)(ProcessSource *self);
} ProcessSourceVTable;

struct ProcessSource {
    const ProcessSourceVTable *vtable;
};

const char *process_source_describe(const ProcessSource *source);
bool process_source_load(ProcessSource *source, ProcessTable *table, Error *error);
void process_source_destroy(ProcessSource *source);

#endif /* APPLICATION_PORTS_PROCESS_SOURCE_H */
