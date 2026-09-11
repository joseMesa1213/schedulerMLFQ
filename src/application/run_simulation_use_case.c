#include "application/run_simulation_use_case.h"

static bool publish_results(const RunSimulationRequest *request, const MetricsReport *report,
                            Error *error)
{
    for (size_t i = 0; i < request->writer_count; i++) {
        if (!results_writer_write(request->writers[i], report, error)) {
            return false;
        }
    }
    return true;
}

bool run_simulation_use_case_execute(const RunSimulationRequest *request,
                                    RunSimulationResponse *response, Error *error)
{
    if (request == NULL || response == NULL || request->source == NULL || request->policy == NULL) {
        return error_set(error, ERROR_INVALID_ARGUMENT, "peticion de simulacion incompleta");
    }
    response->report = NULL;

    ProcessTable *processes = process_table_create(error);
    if (processes == NULL) {
        return false;
    }
    if (!process_source_load(request->source, processes, error)) {
        process_table_destroy(processes);
        return false;
    }

    SimulationConfig config = {
        .processes = processes,
        .policy = request->policy,
        .event_bus = request->event_bus,
        .max_cycles = request->max_cycles
    };
    if (!simulation_engine_run(&config, &response->simulation, error)) {
        process_table_destroy(processes);
        return false;
    }

    response->report = metrics_report_create(processes, response->simulation.total_cycles,
                                             response->simulation.idle_cycles,
                                             response->simulation.context_switches, error);
    /* La tabla ya no se necesita: el informe es un valor independiente de las
     * entidades, de modo que la memoria del dominio se libera aqui mismo. */
    process_table_destroy(processes);
    if (response->report == NULL) {
        return false;
    }

    if (!publish_results(request, response->report, error)) {
        run_simulation_response_release(response);
        return false;
    }
    return true;
}

void run_simulation_response_release(RunSimulationResponse *response)
{
    if (response == NULL) {
        return;
    }
    metrics_report_destroy(response->report);
    response->report = NULL;
}
