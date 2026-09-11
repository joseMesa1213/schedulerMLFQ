/*
 * process_sources.h - Adaptadores concretos del puerto ProcessSource.
 *
 * Cada uno es una fabrica de procesos con un origen distinto. Todos cumplen el
 * mismo contrato, por lo que el caso de uso los trata igual (LSP).
 */
#ifndef INFRASTRUCTURE_PROCESS_SOURCES_H
#define INFRASTRUCTURE_PROCESS_SOURCES_H

#include "application/ports/process_source.h"

/* Lee un CSV con columnas pid,arrival_time,burst_time (admite cabecera y
 * comentarios que empiecen con '#'). */
ProcessSource *csv_process_source_create(const char *path, Error *error);

/* Genera una carga sintetica reproducible a partir de una semilla. */
ProcessSource *random_process_source_create(int count, unsigned int seed, int max_arrival,
                                           int min_burst, int max_burst, Error *error);

/* Escenario de prueba sugerido por el enunciado del laboratorio. */
ProcessSource *builtin_scenario_process_source_create(Error *error);

#endif /* INFRASTRUCTURE_PROCESS_SOURCES_H */
