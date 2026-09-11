/*
 * results_writer.h - PUERTO de salida: donde se publican los resultados.
 *
 * El dominio produce un MetricsReport; este puerto lo consume. Gracias a esto
 * el nucleo nunca incluye <stdio.h> para "exportar a CSV": se pueden agregar
 * salidas (CSV, tabla en consola, JSON, base de datos) sin tocar la simulacion.
 */
#ifndef APPLICATION_PORTS_RESULTS_WRITER_H
#define APPLICATION_PORTS_RESULTS_WRITER_H

#include "domain/error.h"
#include "domain/metrics.h"

typedef struct ResultsWriter ResultsWriter;

typedef struct ResultsWriterVTable {
    const char *(*describe)(const ResultsWriter *self);
    bool (*write)(ResultsWriter *self, const MetricsReport *report, Error *error);
    void (*destroy)(ResultsWriter *self);
} ResultsWriterVTable;

struct ResultsWriter {
    const ResultsWriterVTable *vtable;
};

const char *results_writer_describe(const ResultsWriter *writer);
bool results_writer_write(ResultsWriter *writer, const MetricsReport *report, Error *error);
void results_writer_destroy(ResultsWriter *writer);

#endif /* APPLICATION_PORTS_RESULTS_WRITER_H */
