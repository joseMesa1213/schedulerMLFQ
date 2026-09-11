#include "domain/metrics.h"

#include <stdlib.h>

bool metrics_compute_for_process(const Process *process, ProcessMetrics *out, Error *error)
{
    if (process == NULL || out == NULL) {
        return error_set(error, ERROR_INVALID_ARGUMENT, "proceso o salida nula en metricas");
    }
    if (!process_is_terminated(process)) {
        return error_set(error, ERROR_INVALID_ARGUMENT,
                         "P%d no ha terminado (estado %s): no se pueden calcular metricas",
                         process_pid(process), process_state_name(process));
    }

    out->pid = process_pid(process);
    out->arrival_time = process_arrival_time(process);
    out->burst_time = process_burst_time(process);
    out->start_time = process_start_time(process);
    out->finish_time = process_finish_time(process);
    out->response_time = process_first_response_time(process) - process_arrival_time(process);
    out->turnaround_time = process_finish_time(process) - process_arrival_time(process);
    out->waiting_time = out->turnaround_time - process_burst_time(process);
    out->final_queue = process_current_queue(process);
    out->demotions = process_demotion_count(process);
    out->dispatches = process_dispatch_count(process);
    return true;
}

MetricsReport *metrics_report_create(const ProcessTable *table, int total_cycles,
                                     int idle_cycles, int context_switches, Error *error)
{
    size_t count = process_table_size(table);
    if (count == 0) {
        error_set(error, ERROR_INVALID_ARGUMENT, "no hay procesos para calcular metricas");
        return NULL;
    }

    MetricsReport *report = calloc(1, sizeof(*report));
    if (report == NULL) {
        error_set(error, ERROR_OUT_OF_MEMORY, "sin memoria para el informe de metricas");
        return NULL;
    }
    report->rows = calloc(count, sizeof(*report->rows));
    if (report->rows == NULL) {
        free(report);
        error_set(error, ERROR_OUT_OF_MEMORY, "sin memoria para las filas de metricas");
        return NULL;
    }
    report->count = count;

    long response_sum = 0;
    long turnaround_sum = 0;
    long waiting_sum = 0;
    for (size_t i = 0; i < count; i++) {
        if (!metrics_compute_for_process(process_table_at(table, i), &report->rows[i], error)) {
            metrics_report_destroy(report);
            return NULL;
        }
        response_sum += report->rows[i].response_time;
        turnaround_sum += report->rows[i].turnaround_time;
        waiting_sum += report->rows[i].waiting_time;
    }

    report->average_response_time = (double)response_sum / (double)count;
    report->average_turnaround_time = (double)turnaround_sum / (double)count;
    report->average_waiting_time = (double)waiting_sum / (double)count;
    report->total_cycles = total_cycles;
    report->idle_cycles = idle_cycles;
    report->context_switches = context_switches;
    report->cpu_utilization = total_cycles > 0
        ? 100.0 * (double)(total_cycles - idle_cycles) / (double)total_cycles
        : 0.0;
    report->throughput = total_cycles > 0 ? (double)count / (double)total_cycles : 0.0;
    return report;
}

void metrics_report_destroy(MetricsReport *report)
{
    if (report == NULL) {
        return;
    }
    free(report->rows);
    free(report);
}
