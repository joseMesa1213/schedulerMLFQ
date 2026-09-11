/*
 * Pruebas end-to-end del motor con el escenario del enunciado.
 *
 * Los valores esperados fueron calculados a mano siguiendo las reglas del
 * enunciado (Q0=2, Q1=4, Q2=8, boost cada 20, se ejecuta siempre la cola de
 * mayor prioridad no vacia). Si el simulador cambia de comportamiento, esta
 * prueba lo detecta.
 */
#include "application/run_simulation_use_case.h"
#include "domain/policies/mlfq_policy.h"
#include "domain/policies/simple_policies.h"
#include "infrastructure/observers.h"
#include "infrastructure/process_sources.h"
#include "test_framework.h"
#include "test_suites.h"

static const ProcessMetrics *find_row(const MetricsReport *report, int pid)
{
    for (size_t i = 0; i < report->count; i++) {
        if (report->rows[i].pid == pid) {
            return &report->rows[i];
        }
    }
    return NULL;
}

static void check_row(const MetricsReport *report, int pid, int start, int finish,
                      int response, int turnaround, int waiting)
{
    const ProcessMetrics *row = find_row(report, pid);
    char label[96];
    CHECK(row != NULL, "existe la fila del proceso");
    if (row == NULL) {
        return;
    }
    snprintf(label, sizeof(label), "P%d start", pid);
    CHECK_INT_EQ(row->start_time, start, label);
    snprintf(label, sizeof(label), "P%d finish", pid);
    CHECK_INT_EQ(row->finish_time, finish, label);
    snprintf(label, sizeof(label), "P%d response", pid);
    CHECK_INT_EQ(row->response_time, response, label);
    snprintf(label, sizeof(label), "P%d turnaround", pid);
    CHECK_INT_EQ(row->turnaround_time, turnaround, label);
    snprintf(label, sizeof(label), "P%d waiting", pid);
    CHECK_INT_EQ(row->waiting_time, waiting, label);
}

static void test_escenario_del_enunciado(void)
{
    TEST_BEGIN("escenario P1(0,8) P2(1,4) P3(2,9) P4(3,5) con MLFQ 2/4/8 y boost 20");
    Error error;
    error_clear(&error);

    EventBus *bus = event_bus_create(&error);
    StatsObserver *stats = stats_observer_create(&error);
    event_bus_subscribe(bus, stats_observer_as_observer(stats), &error);

    MlfqConfig config = mlfq_config_default();
    config.event_bus = bus;
    SchedulingPolicy *policy = mlfq_policy_create(&config, &error);
    ProcessSource *source = builtin_scenario_process_source_create(&error);

    RunSimulationRequest request = {
        .source = source, .policy = policy, .event_bus = bus,
        .writers = NULL, .writer_count = 0, .max_cycles = 1000
    };
    RunSimulationResponse response = { 0 };
    CHECK(run_simulation_use_case_execute(&request, &response, &error), "la simulacion termina bien");

    if (response.report != NULL) {
        CHECK_INT_EQ(response.simulation.total_cycles, 26, "26 ciclos (suma de rafagas, sin ociosos)");
        CHECK_INT_EQ(response.simulation.idle_cycles, 0, "sin ciclos ociosos");
        CHECK_INT_EQ(response.simulation.context_switches, 10, "10 despachos");

        check_row(response.report, 1, /*start*/ 0, /*finish*/ 23, /*resp*/ 0, /*turn*/ 23, /*wait*/ 15);
        check_row(response.report, 2, 2, 14, 1, 13, 9);
        check_row(response.report, 3, 4, 26, 2, 24, 15);
        check_row(response.report, 4, 6, 21, 3, 18, 13);

        CHECK_DOUBLE_EQ(response.report->average_response_time, 1.5, "response promedio");
        CHECK_DOUBLE_EQ(response.report->average_turnaround_time, 19.5, "turnaround promedio");
        CHECK_DOUBLE_EQ(response.report->average_waiting_time, 13.0, "waiting promedio");
        CHECK_DOUBLE_EQ(response.report->cpu_utilization, 100.0, "CPU siempre ocupada");
        CHECK_INT_EQ(stats_observer_boosts(stats), 1, "un solo boost (en el ciclo 20)");
        CHECK_INT_EQ(stats_observer_demotions(stats), 7, "siete demociones");
    }

    run_simulation_response_release(&response);
    scheduling_policy_destroy(policy);
    process_source_destroy(source);
    stats_observer_destroy(stats);
    event_bus_destroy(bus);
}

/* La suma de rafagas es un invariante: ninguna politica puede inventar ni
 * perder ciclos de CPU. Sirve como prueba de propiedad para las tres. */
static void run_and_check_total_work(SchedulingPolicy *policy, const char *label)
{
    Error error;
    error_clear(&error);
    ProcessSource *source = builtin_scenario_process_source_create(&error);
    RunSimulationRequest request = {
        .source = source, .policy = policy, .event_bus = NULL,
        .writers = NULL, .writer_count = 0, .max_cycles = 1000
    };
    RunSimulationResponse response = { 0 };
    char message[128];

    snprintf(message, sizeof(message), "%s: la simulacion termina", label);
    CHECK(run_simulation_use_case_execute(&request, &response, &error), message);
    if (response.report != NULL) {
        snprintf(message, sizeof(message), "%s: ciclos totales = suma de rafagas (26)", label);
        CHECK_INT_EQ(response.simulation.total_cycles, 26, message);
        snprintf(message, sizeof(message), "%s: todos los procesos terminaron", label);
        CHECK_INT_EQ(response.report->count, 4, message);
    }
    run_simulation_response_release(&response);
    process_source_destroy(source);
}

static void test_politicas_intercambiables(void)
{
    TEST_BEGIN("el mismo motor corre FCFS y Round Robin (patron Strategy)");
    Error error;
    error_clear(&error);

    SchedulingPolicy *fcfs = fcfs_policy_create(NULL, NULL, &error);
    run_and_check_total_work(fcfs, "FCFS");
    scheduling_policy_destroy(fcfs);

    SchedulingPolicy *round_robin = round_robin_policy_create(2, NULL, NULL, &error);
    run_and_check_total_work(round_robin, "RR(2)");
    scheduling_policy_destroy(round_robin);

    /* FCFS no expropia: P1 corre 0..7 completo, por lo que su turnaround es su
     * propia rafaga. Es la comprobacion de que la politica realmente cambio. */
    error_clear(&error);
    SchedulingPolicy *fcfs2 = fcfs_policy_create(NULL, NULL, &error);
    ProcessSource *source = builtin_scenario_process_source_create(&error);
    RunSimulationRequest request = {
        .source = source, .policy = fcfs2, .event_bus = NULL,
        .writers = NULL, .writer_count = 0, .max_cycles = 1000
    };
    RunSimulationResponse response = { 0 };
    run_simulation_use_case_execute(&request, &response, &error);
    if (response.report != NULL) {
        check_row(response.report, 1, 0, 8, 0, 8, 0);
        CHECK_INT_EQ(response.simulation.context_switches, 4, "FCFS solo despacha una vez por proceso");
    }
    run_simulation_response_release(&response);
    process_source_destroy(source);
    scheduling_policy_destroy(fcfs2);
}

static void test_cpu_ociosa(void)
{
    TEST_BEGIN("contabiliza ciclos ociosos cuando ningun proceso ha llegado");
    Error error;
    error_clear(&error);

    ProcessTable *table = process_table_create(&error);
    process_table_emplace(table, 1, 5, 3, &error);   /* llega en t=5 */
    /* Una sola cola con quantum 4: aisla el conteo de ciclos ociosos de
     * cualquier efecto de democion o boost. */
    MlfqConfig single_level = mlfq_config_default();
    single_level.level_count = 1;
    single_level.quantums[0] = 4;
    single_level.boost_interval = 0;
    SchedulingPolicy *policy = mlfq_policy_create(&single_level, &error);

    SimulationConfig config = { table, policy, NULL, 100 };
    SimulationResult result = { 0 };
    CHECK(simulation_engine_run(&config, &result, &error), "corre con CPU ociosa inicial");
    CHECK_INT_EQ(result.idle_cycles, 5, "cinco ciclos ociosos antes de la llegada");
    CHECK_INT_EQ(result.total_cycles, 8, "5 ociosos + 3 de rafaga");

    MetricsReport *report = metrics_report_create(table, result.total_cycles, result.idle_cycles,
                                                  result.context_switches, &error);
    if (report != NULL) {
        CHECK_INT_EQ(report->rows[0].waiting_time, 0, "no espera: la CPU estaba libre");
        CHECK_DOUBLE_EQ(report->cpu_utilization, 37.5, "uso de CPU 3/8");
    }
    metrics_report_destroy(report);
    scheduling_policy_destroy(policy);
    process_table_destroy(table);
}

void suite_simulation_engine(void)
{
    test_escenario_del_enunciado();
    test_politicas_intercambiables();
    test_cpu_ociosa();
}
