#include "application/simulation_engine.h"

/* Admite en la politica todos los procesos cuyo arrival_time es este ciclo. */
static bool admit_arrivals(const SimulationConfig *config, int cycle, Error *error)
{
    size_t count = process_table_size(config->processes);
    for (size_t i = 0; i < count; i++) {
        Process *process = process_table_at(config->processes, i);
        if (process_arrival_time(process) != cycle) {
            continue;
        }
        if (!process_admit(process, cycle, error) ||
            !scheduling_policy_admit(config->policy, process, cycle, error)) {
            return false;
        }
        event_bus_publish_simple(config->event_bus, EVENT_PROCESS_ARRIVED, cycle, process);
    }
    return true;
}

/*
 * Cambio de contexto: el motor detecta por si mismo que la politica eligio un
 * proceso distinto al que tenia la CPU, y solo entonces aplica las
 * transiciones de estado. La politica no necesita saber nada de estados.
 */
static bool switch_context(Process *previous, Process *selected, int cycle,
                           EventBus *bus, SimulationResult *result, Error *error)
{
    if (previous != NULL) {
        if (!process_preempt(previous, cycle, error)) {
            return false;
        }
        event_bus_publish_simple(bus, EVENT_PROCESS_PREEMPTED, cycle, previous);
    }
    if (!process_dispatch(selected, cycle, error)) {
        return false;
    }
    result->context_switches++;
    event_bus_publish_queue_change(bus, EVENT_PROCESS_DISPATCHED, cycle, selected,
                                   process_current_queue(selected),
                                   process_current_queue(selected));
    return true;
}

static bool validate_config(const SimulationConfig *config, Error *error)
{
    if (config == NULL || config->processes == NULL || config->policy == NULL) {
        return error_set(error, ERROR_INVALID_ARGUMENT,
                         "configuracion de simulacion incompleta (procesos o politica nulos)");
    }
    if (process_table_size(config->processes) == 0) {
        return error_set(error, ERROR_INVALID_ARGUMENT, "no hay procesos que simular");
    }
    return true;
}

bool simulation_engine_run(const SimulationConfig *config, SimulationResult *result, Error *error)
{
    if (!validate_config(config, error) || result == NULL) {
        return error_is_set(error) ? false
            : error_set(error, ERROR_INVALID_ARGUMENT, "resultado de simulacion nulo");
    }

    result->total_cycles = 0;
    result->idle_cycles = 0;
    result->context_switches = 0;

    const int max_cycles = config->max_cycles > 0 ? config->max_cycles : SIMULATION_DEFAULT_MAX_CYCLES;
    const size_t total_processes = process_table_size(config->processes);
    EventBus *bus = config->event_bus;

    process_table_sort_by_arrival(config->processes);
    event_bus_publish_simple(bus, EVENT_SIMULATION_STARTED, 0, NULL);

    Process *running = NULL;
    int cycle = 0;

    while (process_table_count_terminated(config->processes) < total_processes) {
        if (cycle >= max_cycles) {
            return error_set(error, ERROR_SIMULATION_LIMIT,
                             "la simulacion excedio %d ciclos: posible politica que nunca "
                             "selecciona procesos listos", max_cycles);
        }

        if (!admit_arrivals(config, cycle, error)) {
            return false;
        }

        Process *selected = scheduling_policy_select_for_cycle(config->policy, cycle, error);
        if (selected == NULL) {
            if (error_is_set(error)) {
                return false;
            }
            /* Nadie listo: la CPU queda ociosa este ciclo (hay procesos que aun
             * no han llegado). */
            result->idle_cycles++;
            event_bus_publish_simple(bus, EVENT_CPU_IDLE, cycle, NULL);
            cycle++;
            continue;
        }

        if (selected != running) {
            if (!switch_context(running, selected, cycle, bus, result, error)) {
                return false;
            }
        }

        if (!process_consume_cycle(selected, error)) {
            return false;
        }
        event_bus_publish_queue_change(bus, EVENT_CYCLE_EXECUTED, cycle, selected,
                                       process_current_queue(selected),
                                       process_current_queue(selected));

        const bool completed = !process_has_pending_work(selected);
        if (completed) {
            /* finish_time = cycle + 1 porque el proceso libera la CPU al final
             * del ciclo que acaba de ejecutar. */
            if (!process_terminate(selected, cycle + 1, error)) {
                return false;
            }
            event_bus_publish_simple(bus, EVENT_PROCESS_COMPLETED, cycle + 1, selected);
            running = NULL;
        } else {
            running = selected;
        }

        if (!scheduling_policy_notify_cycle_result(config->policy, selected, completed, cycle, error)) {
            return false;
        }
        cycle++;
    }

    result->total_cycles = cycle;
    event_bus_publish_simple(bus, EVENT_SIMULATION_FINISHED, cycle, NULL);
    return true;
}
