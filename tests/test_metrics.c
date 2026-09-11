/*
 * Pruebas del calculo de metricas: es el corazon del entregable, asi que se
 * verifican las tres formulas, los promedios y el rechazo de datos incompletos.
 */
#include "domain/metrics.h"
#include "test_framework.h"
#include "test_suites.h"

/* Simula "a mano" el paso de un proceso por la CPU para dejarlo TERMINATED. */
static Process *make_terminated_process(int pid, int arrival, int burst,
                                        int first_dispatch, int finish, Error *error)
{
    Process *process = process_create(pid, arrival, burst, error);
    process_admit(process, arrival, error);
    process_dispatch(process, first_dispatch, error);
    for (int i = 0; i < burst; i++) {
        process_consume_cycle(process, error);
    }
    process_terminate(process, finish, error);
    return process;
}

static void test_formulas(void)
{
    TEST_BEGIN("response = primera respuesta - llegada; turnaround = fin - llegada; waiting = turnaround - burst");
    Error error;
    error_clear(&error);

    Process *process = make_terminated_process(1, 2, 4, 5, 20, &error);
    ProcessMetrics metrics;
    CHECK(metrics_compute_for_process(process, &metrics, &error), "calcula las metricas");
    CHECK_INT_EQ(metrics.response_time, 3, "response = 5 - 2");
    CHECK_INT_EQ(metrics.turnaround_time, 18, "turnaround = 20 - 2");
    CHECK_INT_EQ(metrics.waiting_time, 14, "waiting = 18 - 4");
    process_destroy(process);
}

static void test_caso_sin_espera(void)
{
    TEST_BEGIN("un proceso que corre sin interrupciones tiene waiting = 0");
    Error error;
    error_clear(&error);
    Process *process = make_terminated_process(1, 0, 6, 0, 6, &error);
    ProcessMetrics metrics;
    metrics_compute_for_process(process, &metrics, &error);
    CHECK_INT_EQ(metrics.response_time, 0, "response 0");
    CHECK_INT_EQ(metrics.turnaround_time, 6, "turnaround = burst");
    CHECK_INT_EQ(metrics.waiting_time, 0, "waiting 0");
    process_destroy(process);
}

static void test_rechaza_proceso_no_terminado(void)
{
    TEST_BEGIN("no calcula metricas de un proceso que no termino");
    Error error;
    error_clear(&error);
    Process *process = process_create(9, 0, 3, &error);
    ProcessMetrics metrics;
    CHECK(!metrics_compute_for_process(process, &metrics, &error), "debe fallar");
    CHECK_INT_EQ(error.code, ERROR_INVALID_ARGUMENT, "reporta argumento invalido");
    process_destroy(process);
}

static void test_promedios_del_informe(void)
{
    TEST_BEGIN("los promedios del informe se calculan sobre todos los procesos");
    Error error;
    error_clear(&error);
    ProcessTable *table = process_table_create(&error);

    /* waiting: (10-0)-4 = 6 ; (12-2)-3 = 7 */
    process_table_add(table, make_terminated_process(1, 0, 4, 1, 10, &error), &error);
    process_table_add(table, make_terminated_process(2, 2, 3, 4, 12, &error), &error);

    MetricsReport *report = metrics_report_create(table, 12, 2, 5, &error);
    CHECK(report != NULL, "se construye el informe");
    CHECK_INT_EQ(report->count, 2, "dos filas");
    CHECK_DOUBLE_EQ(report->average_response_time, 1.5, "promedio de response (1 y 2)");
    CHECK_DOUBLE_EQ(report->average_turnaround_time, 10.0, "promedio de turnaround (10 y 10)");
    CHECK_DOUBLE_EQ(report->average_waiting_time, 6.5, "promedio de waiting (6 y 7)");
    CHECK_DOUBLE_EQ(report->cpu_utilization, 100.0 * 10.0 / 12.0, "uso de CPU descuenta ociosos");
    CHECK_INT_EQ(report->context_switches, 5, "propaga los cambios de contexto");

    metrics_report_destroy(report);
    process_table_destroy(table);
}

static void test_pid_duplicado(void)
{
    TEST_BEGIN("la tabla de procesos rechaza PID duplicados");
    Error error;
    error_clear(&error);
    ProcessTable *table = process_table_create(&error);
    CHECK(process_table_emplace(table, 1, 0, 5, &error), "primer P1");
    CHECK(!process_table_emplace(table, 1, 3, 2, &error), "segundo P1 debe fallar");
    CHECK_INT_EQ(error.code, ERROR_DUPLICATED_PID, "codigo de PID duplicado");
    CHECK_INT_EQ(process_table_size(table), 1, "no se agrego el duplicado");
    process_table_destroy(table);
}

void suite_metrics(void)
{
    test_formulas();
    test_caso_sin_espera();
    test_rechaza_proceso_no_terminado();
    test_promedios_del_informe();
    test_pid_duplicado();
}
