/*
 * run_simulation_use_case.h - Caso de uso: "ejecutar una simulacion".
 *
 * Orquesta el flujo completo (cargar -> simular -> medir -> publicar) usando
 * solo abstracciones. Es la unica pieza que conoce el orden de los pasos, y no
 * conoce ninguna tecnologia concreta: ni archivos, ni consola, ni CSV.
 */
#ifndef APPLICATION_RUN_SIMULATION_USE_CASE_H
#define APPLICATION_RUN_SIMULATION_USE_CASE_H

#include <stddef.h>

#include "application/ports/process_source.h"
#include "application/ports/results_writer.h"
#include "application/simulation_engine.h"
#include "domain/metrics.h"

typedef struct {
    ProcessSource *source;          /* requerido */
    SchedulingPolicy *policy;       /* requerido */
    EventBus *event_bus;            /* opcional */
    ResultsWriter **writers;        /* opcional: 0..n salidas */
    size_t writer_count;
    int max_cycles;
} RunSimulationRequest;

typedef struct {
    MetricsReport *report;   /* propiedad del llamador: destruir al terminar */
    SimulationResult simulation;
} RunSimulationResponse;

bool run_simulation_use_case_execute(const RunSimulationRequest *request,
                                    RunSimulationResponse *response, Error *error);
void run_simulation_response_release(RunSimulationResponse *response);

#endif /* APPLICATION_RUN_SIMULATION_USE_CASE_H */
