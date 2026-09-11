/*
 * random_process_source.c - Adaptador de entrada: carga sintetica reproducible.
 *
 * Usa un generador congruencial propio en lugar de rand() para que la
 * secuencia sea identica en cualquier plataforma: un experimento del informe
 * debe poder repetirse con la misma semilla.
 */
#include "infrastructure/process_sources.h"

#include <stdio.h>
#include <stdlib.h>

typedef struct {
    ProcessSource base;
    int count;
    unsigned int seed;
    int max_arrival;
    int min_burst;
    int max_burst;
    char description[160];
} RandomProcessSource;

static const char *random_describe(const ProcessSource *self)
{
    return ((const RandomProcessSource *)self)->description;
}

static unsigned int next_random(unsigned int *state)
{
    *state = (*state * 1103515245u) + 12345u;
    return (*state >> 16) & 0x7fffu;
}

static int random_in_range(unsigned int *state, int low, int high)
{
    int span = high - low + 1;
    return low + (int)(next_random(state) % (unsigned int)span);
}

static bool random_load(ProcessSource *self, ProcessTable *table, Error *error)
{
    RandomProcessSource *source = (RandomProcessSource *)self;
    unsigned int state = source->seed;
    for (int i = 0; i < source->count; i++) {
        int arrival_time = random_in_range(&state, 0, source->max_arrival);
        int burst_time = random_in_range(&state, source->min_burst, source->max_burst);
        if (!process_table_emplace(table, i + 1, arrival_time, burst_time, error)) {
            return false;
        }
    }
    return true;
}

static void random_destroy(ProcessSource *self)
{
    free(self);
}

static const ProcessSourceVTable kRandomVTable = { random_describe, random_load, random_destroy };

ProcessSource *random_process_source_create(int count, unsigned int seed, int max_arrival,
                                           int min_burst, int max_burst, Error *error)
{
    if (count <= 0) {
        error_set(error, ERROR_INVALID_ARGUMENT, "cantidad de procesos invalida (%d)", count);
        return NULL;
    }
    if (min_burst <= 0 || max_burst < min_burst) {
        error_set(error, ERROR_INVALID_ARGUMENT,
                  "rango de rafaga invalido (%d..%d)", min_burst, max_burst);
        return NULL;
    }
    if (max_arrival < 0) {
        error_set(error, ERROR_INVALID_ARGUMENT, "llegada maxima invalida (%d)", max_arrival);
        return NULL;
    }

    RandomProcessSource *source = calloc(1, sizeof(*source));
    if (source == NULL) {
        error_set(error, ERROR_OUT_OF_MEMORY, "sin memoria para la fuente aleatoria");
        return NULL;
    }
    source->base.vtable = &kRandomVTable;
    source->count = count;
    source->seed = seed;
    source->max_arrival = max_arrival;
    source->min_burst = min_burst;
    source->max_burst = max_burst;
    snprintf(source->description, sizeof(source->description),
             "carga aleatoria (%d procesos, semilla %u, llegadas 0..%d, rafagas %d..%d)",
             count, seed, max_arrival, min_burst, max_burst);
    return &source->base;
}
