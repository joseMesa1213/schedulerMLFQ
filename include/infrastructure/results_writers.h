/*
 * results_writers.h - Adaptadores concretos del puerto ResultsWriter.
 */
#ifndef INFRASTRUCTURE_RESULTS_WRITERS_H
#define INFRASTRUCTURE_RESULTS_WRITERS_H

#include <stdio.h>

#include "application/ports/results_writer.h"

/* results.csv con las columnas exigidas por el enunciado. */
ResultsWriter *csv_results_writer_create(const char *path, Error *error);

/* Tabla legible en consola + resumen de promedios. `stream` no se cierra. */
ResultsWriter *console_results_writer_create(FILE *stream, Error *error);

#endif /* INFRASTRUCTURE_RESULTS_WRITERS_H */
