/*
 * metrics.h - Calculo de metricas (responsabilidad unica y aislada).
 *
 * Este modulo NO simula y NO escribe archivos: solo transforma el estado final
 * de los procesos en numeros. Por eso es trivialmente testeable con pruebas
 * unitarias, que es exactamente lo que exige el enunciado.
 *
 *   response_time   = first_response_time - arrival_time
 *   turnaround_time = finish_time         - arrival_time
 *   waiting_time    = turnaround_time     - burst_time
 */
#ifndef DOMAIN_METRICS_H
#define DOMAIN_METRICS_H

#include <stddef.h>

#include "domain/error.h"
#include "domain/process_table.h"

typedef struct {
    int pid;
    int arrival_time;
    int burst_time;
    int start_time;
    int finish_time;
    int response_time;
    int turnaround_time;
    int waiting_time;
    int final_queue;
    int demotions;
    int dispatches;
} ProcessMetrics;

typedef struct {
    ProcessMetrics *rows;
    size_t count;
    double average_response_time;
    double average_turnaround_time;
    double average_waiting_time;
    int total_cycles;
    int idle_cycles;
    int context_switches;
    double cpu_utilization;   /* porcentaje 0..100 */
    double throughput;        /* procesos por ciclo */
} MetricsReport;

/*
 * Construye el informe. Falla si algun proceso no termino, porque una metrica
 * calculada sobre un proceso incompleto seria un dato incorrecto presentado
 * como valido.
 */
MetricsReport *metrics_report_create(const ProcessTable *table, int total_cycles,
                                     int idle_cycles, int context_switches, Error *error);
void metrics_report_destroy(MetricsReport *report);

/* Calculo puntual de una fila, reutilizado por el informe y por las pruebas. */
bool metrics_compute_for_process(const Process *process, ProcessMetrics *out, Error *error);

#endif /* DOMAIN_METRICS_H */
