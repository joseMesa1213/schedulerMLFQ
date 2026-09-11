/*
 * builtin_scenario_process_source.c - Escenario de prueba del enunciado.
 *
 * Existe para que `make run` reproduzca exactamente el caso pedido sin
 * depender de un archivo externo, y para que las pruebas end-to-end tengan
 * una entrada estable.
 */
#include "infrastructure/process_sources.h"

#include <stdlib.h>

typedef struct {
    int pid;
    int arrival_time;
    int burst_time;
} ScenarioRow;

/* P1: arrival=0 burst=8 | P2: 1,4 | P3: 2,9 | P4: 3,5 */
static const ScenarioRow kScenario[] = {
    { 1, 0, 8 },
    { 2, 1, 4 },
    { 3, 2, 9 },
    { 4, 3, 5 }
};

static const char *builtin_describe(const ProcessSource *self)
{
    (void)self;
    return "escenario integrado del enunciado (P1..P4)";
}

static bool builtin_load(ProcessSource *self, ProcessTable *table, Error *error)
{
    (void)self;
    for (size_t i = 0; i < sizeof(kScenario) / sizeof(kScenario[0]); i++) {
        if (!process_table_emplace(table, kScenario[i].pid, kScenario[i].arrival_time,
                                  kScenario[i].burst_time, error)) {
            return false;
        }
    }
    return true;
}

static void builtin_destroy(ProcessSource *self)
{
    free(self);
}

static const ProcessSourceVTable kBuiltinVTable = { builtin_describe, builtin_load, builtin_destroy };

ProcessSource *builtin_scenario_process_source_create(Error *error)
{
    ProcessSource *source = calloc(1, sizeof(*source));
    if (source == NULL) {
        error_set(error, ERROR_OUT_OF_MEMORY, "sin memoria para el escenario integrado");
        return NULL;
    }
    source->vtable = &kBuiltinVTable;
    return source;
}
