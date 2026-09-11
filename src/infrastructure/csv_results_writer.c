/*
 * csv_results_writer.c - Adaptador de salida: results.csv.
 *
 * Unico modulo del programa que conoce el formato CSV de resultados. El
 * dominio jamas incluye <stdio.h> para esto (regla de dependencia de la
 * arquitectura limpia).
 */
#include "infrastructure/results_writers.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    ResultsWriter base;
    char path[512];
    char description[576];
} CsvResultsWriter;

static const char *csv_describe(const ResultsWriter *self)
{
    return ((const CsvResultsWriter *)self)->description;
}

static bool csv_write(ResultsWriter *self, const MetricsReport *report, Error *error)
{
    CsvResultsWriter *writer = (CsvResultsWriter *)self;
    FILE *file = fopen(writer->path, "w");
    if (file == NULL) {
        return error_set(error, ERROR_IO, "no se pudo crear el archivo de resultados '%s'",
                         writer->path);
    }

    fprintf(file, "PID,Arrival,Burst,Start,Finish,Response,Turnaround,Waiting\n");
    for (size_t i = 0; i < report->count; i++) {
        const ProcessMetrics *row = &report->rows[i];
        fprintf(file, "P%d,%d,%d,%d,%d,%d,%d,%d\n",
                row->pid, row->arrival_time, row->burst_time, row->start_time,
                row->finish_time, row->response_time, row->turnaround_time, row->waiting_time);
    }
    /* Fila de promedios: se marca con la etiqueta AVG y deja vacias las
     * columnas que no tienen promedio con sentido fisico. */
    fprintf(file, "AVG,,,,,%.2f,%.2f,%.2f\n",
            report->average_response_time, report->average_turnaround_time,
            report->average_waiting_time);

    if (ferror(file) != 0) {
        fclose(file);
        return error_set(error, ERROR_IO, "error de escritura en '%s'", writer->path);
    }
    if (fclose(file) != 0) {
        return error_set(error, ERROR_IO, "no se pudo cerrar '%s'", writer->path);
    }
    return true;
}

static void csv_destroy(ResultsWriter *self)
{
    free(self);
}

static const ResultsWriterVTable kCsvVTable = { csv_describe, csv_write, csv_destroy };

ResultsWriter *csv_results_writer_create(const char *path, Error *error)
{
    if (path == NULL || path[0] == '\0') {
        error_set(error, ERROR_INVALID_ARGUMENT, "ruta de salida vacia");
        return NULL;
    }
    CsvResultsWriter *writer = calloc(1, sizeof(*writer));
    if (writer == NULL) {
        error_set(error, ERROR_OUT_OF_MEMORY, "sin memoria para el escritor CSV");
        return NULL;
    }
    snprintf(writer->path, sizeof(writer->path), "%s", path);
    snprintf(writer->description, sizeof(writer->description), "CSV '%s'", writer->path);
    writer->base.vtable = &kCsvVTable;
    return &writer->base;
}
