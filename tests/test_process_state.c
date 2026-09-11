/*
 * Pruebas del patron State: transiciones legales, transiciones rechazadas y
 * registro de los instantes de tiempo del proceso.
 */
#include "domain/process.h"
#include "test_framework.h"
#include "test_suites.h"

static void test_validaciones_de_construccion(void)
{
    TEST_BEGIN("rechaza burst_time y arrival_time invalidos");
    Error error;
    error_clear(&error);

    CHECK(process_create(1, 0, 0, &error) == NULL, "burst_time = 0 debe fallar");
    CHECK_INT_EQ(error.code, ERROR_INVALID_ARGUMENT, "codigo de error de burst invalido");

    error_clear(&error);
    CHECK(process_create(2, 0, -5, &error) == NULL, "burst_time negativo debe fallar");

    error_clear(&error);
    CHECK(process_create(3, -1, 5, &error) == NULL, "arrival_time negativo debe fallar");

    error_clear(&error);
    Process *valid = process_create(4, 3, 5, &error);
    CHECK(valid != NULL, "un proceso valido se construye");
    CHECK_INT_EQ(process_remaining_time(valid), 5, "remaining_time inicia igual al burst");
    CHECK_INT_EQ(process_start_time(valid), PROCESS_TIME_UNSET, "start_time inicia sin definir");
    CHECK_INT_EQ(process_state_id(valid), PROCESS_STATE_NEW, "estado inicial NEW");
    process_destroy(valid);
}

static void test_secuencia_legal(void)
{
    TEST_BEGIN("secuencia NEW->READY->RUNNING->READY->RUNNING->TERMINATED");
    Error error;
    error_clear(&error);
    Process *process = process_create(1, 2, 2, &error);

    CHECK(process_admit(process, 2, &error), "admitir desde NEW");
    CHECK_INT_EQ(process_state_id(process), PROCESS_STATE_READY, "queda READY");

    CHECK(process_dispatch(process, 5, &error), "despachar desde READY");
    CHECK_INT_EQ(process_state_id(process), PROCESS_STATE_RUNNING, "queda RUNNING");
    CHECK_INT_EQ(process_start_time(process), 5, "start_time = primer despacho");
    CHECK_INT_EQ(process_first_response_time(process), 5, "first_response_time = primer despacho");

    CHECK(process_consume_cycle(process, &error), "consume un ciclo de CPU");
    CHECK_INT_EQ(process_remaining_time(process), 1, "remaining_time disminuye");

    CHECK(process_preempt(process, 6, &error), "expropiar desde RUNNING");
    CHECK(process_dispatch(process, 9, &error), "segundo despacho");
    CHECK_INT_EQ(process_start_time(process), 5,
                 "start_time NO se sobreescribe en el segundo despacho");
    CHECK_INT_EQ(process_dispatch_count(process), 2, "cuenta dos despachos");

    CHECK(process_consume_cycle(process, &error), "consume el ultimo ciclo");
    CHECK(process_terminate(process, 10, &error), "termina");
    CHECK_INT_EQ(process_state_id(process), PROCESS_STATE_TERMINATED, "queda TERMINATED");
    CHECK_INT_EQ(process_finish_time(process), 10, "finish_time registrado");
    CHECK(!error_is_set(&error), "ninguna transicion legal produjo error");
    process_destroy(process);
}

static void test_transiciones_ilegales(void)
{
    TEST_BEGIN("rechaza transiciones ilegales con mensaje explicito");
    Error error;
    error_clear(&error);
    Process *process = process_create(7, 0, 3, &error);

    CHECK(!process_dispatch(process, 0, &error), "no se puede despachar un proceso NEW");
    CHECK_INT_EQ(error.code, ERROR_INVALID_STATE_TRANSITION, "codigo de transicion invalida");
    CHECK(strstr(error.message, "NEW") != NULL, "el mensaje nombra el estado actual");

    error_clear(&error);
    CHECK(!process_consume_cycle(process, &error), "solo un proceso RUNNING consume CPU");

    error_clear(&error);
    process_admit(process, 0, &error);
    CHECK(!process_admit(process, 0, &error), "no se puede admitir dos veces");

    error_clear(&error);
    process_dispatch(process, 0, &error);
    CHECK(!process_terminate(process, 1, &error), "no termina con rafaga pendiente");

    error_clear(&error);
    CHECK(process_block(process, 1, &error), "RUNNING puede bloquearse (WAITING)");
    CHECK_INT_EQ(process_state_id(process), PROCESS_STATE_WAITING, "queda WAITING");
    CHECK(!process_dispatch(process, 2, &error), "un proceso WAITING no se despacha");
    error_clear(&error);
    CHECK(process_resume(process, 3, &error), "WAITING puede volver a READY");
    process_destroy(process);
}

static void test_movimiento_entre_colas(void)
{
    TEST_BEGIN("contabiliza demociones al cambiar de nivel");
    Error error;
    error_clear(&error);
    Process *process = process_create(1, 0, 4, &error);

    CHECK(process_move_to_queue(process, 1, &error), "mover a Q1");
    CHECK(process_move_to_queue(process, 2, &error), "mover a Q2");
    CHECK_INT_EQ(process_demotion_count(process), 2, "dos demociones");
    CHECK(process_move_to_queue(process, 0, &error), "promover a Q0 (boost)");
    CHECK_INT_EQ(process_demotion_count(process), 2, "una promocion no cuenta como democion");
    CHECK_INT_EQ(process_current_queue(process), 0, "queda en Q0");
    CHECK(!process_move_to_queue(process, -1, &error), "nivel negativo es invalido");
    process_destroy(process);
}

void suite_process_state(void)
{
    test_validaciones_de_construccion();
    test_secuencia_legal();
    test_transiciones_ilegales();
    test_movimiento_entre_colas();
}
