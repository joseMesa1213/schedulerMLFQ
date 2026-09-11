/*
 * main.c - COMPOSITION ROOT (raiz de composicion).
 *
 * Es el unico lugar del programa donde se decide QUE implementaciones
 * concretas se usan: aqui se crea la fuente de procesos, la politica de
 * planificacion, las colas, los observadores y las salidas, y se inyectan en
 * el caso de uso. Ninguna capa interna hace `new` de una clase concreta.
 *
 * Consecuencia practica: la direccion de las dependencias apunta siempre hacia
 * el dominio (cli -> infrastructure -> application -> domain), nunca al revés.
 */
#include <stdio.h>
#include <stdlib.h>

#include "application/run_simulation_use_case.h"
#include "cli/cli_options.h"
#include "domain/policies/simple_policies.h"
#include "infrastructure/observers.h"
#include "infrastructure/process_sources.h"
#include "infrastructure/results_writers.h"

/* Todo lo que hay que liberar al final, en un solo lugar. */
typedef struct {
    ProcessSource *source;
    SchedulingPolicy *policy;
    EventBus *bus;
    TraceObserver *trace;
    GanttObserver *gantt;
    StatsObserver *stats;
    WaitGapObserver *wait_gaps;
    ResultsWriter *console_writer;
    ResultsWriter *csv_writer;
} Composition;

static void composition_release(Composition *composition)
{
    results_writer_destroy(composition->csv_writer);
    results_writer_destroy(composition->console_writer);
    wait_gap_observer_destroy(composition->wait_gaps);
    stats_observer_destroy(composition->stats);
    gantt_observer_destroy(composition->gantt);
    trace_observer_destroy(composition->trace);
    event_bus_destroy(composition->bus);
    scheduling_policy_destroy(composition->policy);
    process_source_destroy(composition->source);
}

static ProcessSource *build_process_source(const CliOptions *options, Error *error)
{
    switch (options->input_kind) {
    case INPUT_CSV_FILE:
        return csv_process_source_create(options->input_path, error);
    case INPUT_RANDOM:
        return random_process_source_create(options->random_count, options->random_seed,
                                          /* max_arrival */ 10, /* min_burst */ 2,
                                          /* max_burst */ 12, error);
    case INPUT_BUILTIN_SCENARIO:
    default:
        return builtin_scenario_process_source_create(error);
    }
}

static ReadyQueueFactory select_queue_factory(const CliOptions *options)
{
    return options->queue_kind == QUEUE_SHORTEST_REMAINING
        ? shortest_remaining_ready_queue_create
        : fifo_ready_queue_create;
}

static SchedulingPolicy *build_policy(const CliOptions *options, EventBus *bus, Error *error)
{
    ReadyQueueFactory queue_factory = select_queue_factory(options);

    switch (options->policy_kind) {
    case POLICY_FCFS:
        return fcfs_policy_create(queue_factory, bus, error);
    case POLICY_ROUND_ROBIN:
        return round_robin_policy_create(options->quantums[0], queue_factory, bus, error);
    case POLICY_MLFQ:
    default: {
        MlfqConfig config = mlfq_config_default();
        config.level_count = options->level_count;
        for (int level = 0; level < options->level_count; level++) {
            config.quantums[level] = options->quantums[level];
        }
        config.boost_interval = options->boost_interval;
        config.preempt_on_higher_priority = options->preempt_on_higher_priority;
        config.demotion_rule = options->demote_to_lowest ? mlfq_demote_to_lowest
                                                         : mlfq_demote_one_level;
        config.queue_factory = queue_factory;
        config.event_bus = bus;
        return mlfq_policy_create(&config, error);
    }
    }
}

static bool subscribe_observers(Composition *composition, const CliOptions *options, Error *error)
{
    composition->stats = stats_observer_create(error);
    if (composition->stats == NULL ||
        !event_bus_subscribe(composition->bus, stats_observer_as_observer(composition->stats), error)) {
        return false;
    }

    composition->wait_gaps = wait_gap_observer_create(error);
    if (composition->wait_gaps == NULL ||
        !event_bus_subscribe(composition->bus,
                             wait_gap_observer_as_observer(composition->wait_gaps), error)) {
        return false;
    }

    if (options->show_gantt) {
        composition->gantt = gantt_observer_create(error);
        if (composition->gantt == NULL ||
            !event_bus_subscribe(composition->bus, gantt_observer_as_observer(composition->gantt), error)) {
            return false;
        }
    }

    if (options->show_trace) {
        composition->trace = trace_observer_create(stdout, composition->policy, error);
        if (composition->trace == NULL ||
            !event_bus_subscribe(composition->bus, trace_observer_as_observer(composition->trace), error)) {
            return false;
        }
    }
    return true;
}

static bool build_writers(Composition *composition, const CliOptions *options,
                          ResultsWriter **writers, size_t *writer_count, Error *error)
{
    composition->console_writer = console_results_writer_create(stdout, error);
    if (composition->console_writer == NULL) {
        return false;
    }
    writers[(*writer_count)++] = composition->console_writer;

    if (options->write_csv) {
        composition->csv_writer = csv_results_writer_create(options->output_path, error);
        if (composition->csv_writer == NULL) {
            return false;
        }
        writers[(*writer_count)++] = composition->csv_writer;
    }
    return true;
}

static int fail(const Error *error)
{
    /* Se vacia stdout primero para que el mensaje de error aparezca DESPUES del
     * resumen de configuracion y no intercalado con el. */
    fflush(stdout);
    fprintf(stderr, "\nError [%s]: %s\n", error_code_name(error->code), error_message(error));
    return EXIT_FAILURE;
}

int main(int argc, char **argv)
{
    Error error;
    error_clear(&error);

    CliOptions options;
    if (!cli_options_parse(argc, argv, &options, &error)) {
        fprintf(stderr, "Error [%s]: %s\n\n", error_code_name(error.code), error_message(&error));
        cli_options_print_usage(argv[0]);
        return EXIT_FAILURE;
    }
    if (options.show_help) {
        cli_options_print_usage(argv[0]);
        return EXIT_SUCCESS;
    }

    Composition composition = { 0 };
    ResultsWriter *writers[2];
    size_t writer_count = 0;
    int exit_code = EXIT_SUCCESS;

    composition.bus = event_bus_create(&error);
    composition.source = composition.bus != NULL ? build_process_source(&options, &error) : NULL;
    composition.policy = composition.source != NULL ? build_policy(&options, composition.bus, &error) : NULL;

    if (composition.policy == NULL ||
        !subscribe_observers(&composition, &options, &error) ||
        !build_writers(&composition, &options, writers, &writer_count, &error)) {
        composition_release(&composition);
        return fail(&error);
    }

    cli_options_print_summary(&options);
    printf("Procesos   : %s\n", process_source_describe(composition.source));

    RunSimulationRequest request = {
        .source = composition.source,
        .policy = composition.policy,
        .event_bus = composition.bus,
        .writers = writers,
        .writer_count = writer_count,
        .max_cycles = SIMULATION_DEFAULT_MAX_CYCLES
    };
    RunSimulationResponse response = { 0 };

    if (options.show_trace) {
        printf("\n=== Traza de eventos ===\n");
    }

    if (run_simulation_use_case_execute(&request, &response, &error)) {
        gantt_observer_print(composition.gantt, stdout);
        stats_observer_print(composition.stats, stdout);
        wait_gap_observer_print(composition.wait_gaps, stdout);
        if (options.write_csv) {
            printf("\nResultados exportados a '%s'.\n", options.output_path);
        }
    } else {
        exit_code = fail(&error);
    }

    run_simulation_response_release(&response);
    composition_release(&composition);
    return exit_code;
}
