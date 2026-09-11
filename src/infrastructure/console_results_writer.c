/*
 * console_results_writer.c - Adaptador de salida: tabla legible en consola.
 *
 * Demuestra que agregar una salida nueva no exige tocar el nucleo: es otra
 * implementacion del mismo puerto, registrada en el arranque.
 */
#include "infrastructure/results_writers.h"

#include <stdlib.h>

typedef struct {
    ResultsWriter base;
    FILE *stream;   /* no es propiedad de este objeto: no se cierra */
} ConsoleResultsWriter;

static const char *console_describe(const ResultsWriter *self)
{
    (void)self;
    return "tabla en consola";
}

static bool console_write(ResultsWriter *self, const MetricsReport *report, Error *error)
{
    ConsoleResultsWriter *writer = (ConsoleResultsWriter *)self;
    FILE *out = writer->stream;
    (void)error;

    fprintf(out, "\n=== Metricas por proceso ===\n");
    fprintf(out, "%-5s %8s %6s %6s %7s %9s %11s %8s %6s %10s\n",
            "PID", "Arrival", "Burst", "Start", "Finish", "Response", "Turnaround",
            "Waiting", "QFinal", "Demociones");
    for (size_t i = 0; i < report->count; i++) {
        const ProcessMetrics *row = &report->rows[i];
        fprintf(out, "P%-4d %8d %6d %6d %7d %9d %11d %8d %6d %10d\n",
                row->pid, row->arrival_time, row->burst_time, row->start_time,
                row->finish_time, row->response_time, row->turnaround_time,
                row->waiting_time, row->final_queue, row->demotions);
    }

    fprintf(out, "\n=== Promedios ===\n");
    fprintf(out, "Response time promedio   : %.2f ciclos\n", report->average_response_time);
    fprintf(out, "Turnaround time promedio : %.2f ciclos\n", report->average_turnaround_time);
    fprintf(out, "Waiting time promedio    : %.2f ciclos\n", report->average_waiting_time);
    fprintf(out, "Ciclos totales           : %d (ociosos: %d)\n",
            report->total_cycles, report->idle_cycles);
    fprintf(out, "Uso de CPU               : %.2f%%\n", report->cpu_utilization);
    fprintf(out, "Throughput               : %.4f procesos/ciclo\n", report->throughput);
    fprintf(out, "Cambios de contexto      : %d\n", report->context_switches);
    return true;
}

static void console_destroy(ResultsWriter *self)
{
    free(self);
}

static const ResultsWriterVTable kConsoleVTable = { console_describe, console_write, console_destroy };

ResultsWriter *console_results_writer_create(FILE *stream, Error *error)
{
    if (stream == NULL) {
        error_set(error, ERROR_INVALID_ARGUMENT, "flujo de salida nulo");
        return NULL;
    }
    ConsoleResultsWriter *writer = calloc(1, sizeof(*writer));
    if (writer == NULL) {
        error_set(error, ERROR_OUT_OF_MEMORY, "sin memoria para el escritor de consola");
        return NULL;
    }
    writer->base.vtable = &kConsoleVTable;
    writer->stream = stream;
    return &writer->base;
}
