/*
 * Pruebas de la politica MLFQ en aislamiento (sin motor, sin archivos).
 *
 * Se reproduce a mano el protocolo por ciclo que usa el motor: seleccionar,
 * ejecutar un ciclo, notificar. Esto documenta el contrato de Strategy y
 * permite afirmar sobre el nivel de cola en un ciclo exacto.
 */
#include "domain/policies/mlfq_policy.h"
#include "infrastructure/observers.h"
#include "test_framework.h"
#include "test_suites.h"

typedef struct {
    SchedulingPolicy *policy;
    Process *running;
} PolicyDriver;

static Process *drive_one_cycle(PolicyDriver *driver, int cycle, Error *error)
{
    Process *selected = scheduling_policy_select_for_cycle(driver->policy, cycle, error);
    if (selected == NULL) {
        return NULL;
    }
    if (selected != driver->running) {
        if (driver->running != NULL) {
            process_preempt(driver->running, cycle, error);
        }
        process_dispatch(selected, cycle, error);
    }
    process_consume_cycle(selected, error);

    const bool completed = !process_has_pending_work(selected);
    if (completed) {
        process_terminate(selected, cycle + 1, error);
        driver->running = NULL;
    } else {
        driver->running = selected;
    }
    scheduling_policy_notify_cycle_result(driver->policy, selected, completed, cycle, error);
    return selected;
}

static SchedulingPolicy *create_policy(int boost_interval, bool demote_to_lowest,
                                       EventBus *bus, Error *error)
{
    MlfqConfig config = mlfq_config_default();
    config.boost_interval = boost_interval;
    config.demotion_rule = demote_to_lowest ? mlfq_demote_to_lowest : mlfq_demote_one_level;
    config.event_bus = bus;
    return mlfq_policy_create(&config, error);
}

static void test_democion_al_agotar_quantum(void)
{
    TEST_BEGIN("un proceso que agota el quantum de Q0 (2 ciclos) baja a Q1");
    Error error;
    error_clear(&error);
    SchedulingPolicy *policy = create_policy(0, false, NULL, &error);
    Process *process = process_create(1, 0, 10, &error);
    process_admit(process, 0, &error);
    scheduling_policy_admit(policy, process, 0, &error);

    PolicyDriver driver = { policy, NULL };
    drive_one_cycle(&driver, 0, &error);
    drive_one_cycle(&driver, 1, &error);
    CHECK_INT_EQ(process_current_queue(process), 0, "durante el quantum sigue en Q0");

    drive_one_cycle(&driver, 2, &error);
    CHECK_INT_EQ(process_current_queue(process), 1, "al agotar el quantum baja a Q1");
    CHECK_INT_EQ(process_demotion_count(process), 1, "una democion");

    /* Q1 tiene quantum 4: recien en el sexto ciclo baja a Q2. */
    for (int cycle = 3; cycle <= 5; cycle++) {
        drive_one_cycle(&driver, cycle, &error);
    }
    CHECK_INT_EQ(process_current_queue(process), 1, "sigue en Q1 mientras dure su quantum");
    drive_one_cycle(&driver, 6, &error);
    CHECK_INT_EQ(process_current_queue(process), 2, "al agotar Q1 baja a Q2");
    CHECK(!error_is_set(&error), "sin errores");

    process_destroy(process);
    scheduling_policy_destroy(policy);
}

static void test_sin_democion_si_termina_antes(void)
{
    TEST_BEGIN("un proceso que termina dentro del quantum NO baja de nivel");
    Error error;
    error_clear(&error);
    SchedulingPolicy *policy = create_policy(0, false, NULL, &error);
    Process *process = process_create(1, 0, 2, &error);
    process_admit(process, 0, &error);
    scheduling_policy_admit(policy, process, 0, &error);

    PolicyDriver driver = { policy, NULL };
    drive_one_cycle(&driver, 0, &error);
    drive_one_cycle(&driver, 1, &error);

    CHECK(process_is_terminated(process), "termino exactamente al consumir su quantum");
    CHECK_INT_EQ(process_current_queue(process), 0, "permanece en Q0");
    CHECK_INT_EQ(process_demotion_count(process), 0, "cero demociones");

    process_destroy(process);
    scheduling_policy_destroy(policy);
}

static void test_priority_boost(void)
{
    TEST_BEGIN("el priority boost devuelve los procesos a Q0 cada S ciclos");
    Error error;
    error_clear(&error);
    EventBus *bus = event_bus_create(&error);
    StatsObserver *stats = stats_observer_create(&error);
    event_bus_subscribe(bus, stats_observer_as_observer(stats), &error);

    SchedulingPolicy *policy = create_policy(6, false, bus, &error);
    Process *process = process_create(1, 0, 20, &error);
    process_admit(process, 0, &error);
    scheduling_policy_admit(policy, process, 0, &error);

    PolicyDriver driver = { policy, NULL };
    for (int cycle = 0; cycle <= 5; cycle++) {
        drive_one_cycle(&driver, cycle, &error);
    }
    CHECK_INT_EQ(process_current_queue(process), 1, "antes del boost ya habia bajado a Q1");

    drive_one_cycle(&driver, 6, &error);
    CHECK_INT_EQ(process_current_queue(process), 0, "el boost lo devuelve a Q0");
    CHECK_INT_EQ(stats_observer_boosts(stats), 1, "se publico un evento de boost");

    /* Con el boost el quantum vuelve a ser el de Q0 (2 ciclos). */
    drive_one_cycle(&driver, 7, &error);
    drive_one_cycle(&driver, 8, &error);
    CHECK_INT_EQ(process_current_queue(process), 1, "vuelve a bajar tras agotar Q0");

    process_destroy(process);
    scheduling_policy_destroy(policy);
    stats_observer_destroy(stats);
    event_bus_destroy(bus);
}

static void test_sin_boost_el_proceso_queda_en_el_fondo(void)
{
    TEST_BEGIN("sin boost (S=0) el proceso largo se queda en la ultima cola");
    Error error;
    error_clear(&error);
    SchedulingPolicy *policy = create_policy(0, false, NULL, &error);
    Process *process = process_create(1, 0, 30, &error);
    process_admit(process, 0, &error);
    scheduling_policy_admit(policy, process, 0, &error);

    PolicyDriver driver = { policy, NULL };
    for (int cycle = 0; cycle <= 20; cycle++) {
        drive_one_cycle(&driver, cycle, &error);
    }
    CHECK_INT_EQ(process_current_queue(process), 2, "termina en Q2 y ahi se queda");

    process_destroy(process);
    scheduling_policy_destroy(policy);
}

static void test_expropiacion_por_mayor_prioridad(void)
{
    TEST_BEGIN("un proceso que llega a Q0 expropia al que ejecuta en una cola inferior");
    Error error;
    error_clear(&error);
    SchedulingPolicy *policy = create_policy(0, false, NULL, &error);

    Process *largo = process_create(1, 0, 20, &error);
    process_admit(largo, 0, &error);
    scheduling_policy_admit(policy, largo, 0, &error);

    PolicyDriver driver = { policy, NULL };
    for (int cycle = 0; cycle <= 6; cycle++) {
        drive_one_cycle(&driver, cycle, &error);
    }
    CHECK_INT_EQ(process_current_queue(largo), 2, "el proceso largo ya esta en Q2");

    Process *interactivo = process_create(2, 7, 2, &error);
    process_admit(interactivo, 7, &error);
    scheduling_policy_admit(policy, interactivo, 7, &error);

    Process *selected = drive_one_cycle(&driver, 7, &error);
    CHECK(selected == interactivo, "en el ciclo 7 ejecuta el proceso de Q0");
    CHECK_INT_EQ(process_current_queue(largo), 2, "el expropiado NO se degrada mas");
    CHECK_INT_EQ(process_state_id(largo), PROCESS_STATE_READY, "el expropiado vuelve a READY");

    process_destroy(largo);
    process_destroy(interactivo);
    scheduling_policy_destroy(policy);
}

static void test_regla_de_democion_alternativa(void)
{
    TEST_BEGIN("la regla de democion es un punto de extension (baja directo al fondo)");
    Error error;
    error_clear(&error);
    SchedulingPolicy *policy = create_policy(0, true, NULL, &error);
    Process *process = process_create(1, 0, 10, &error);
    process_admit(process, 0, &error);
    scheduling_policy_admit(policy, process, 0, &error);

    PolicyDriver driver = { policy, NULL };
    drive_one_cycle(&driver, 0, &error);
    drive_one_cycle(&driver, 1, &error);
    drive_one_cycle(&driver, 2, &error);
    CHECK_INT_EQ(process_current_queue(process), 2,
                 "con la regla alternativa pasa de Q0 directo a Q2");

    process_destroy(process);
    scheduling_policy_destroy(policy);
}

static void test_configuracion_invalida(void)
{
    TEST_BEGIN("valida la configuracion de la politica");
    Error error;
    error_clear(&error);

    MlfqConfig config = mlfq_config_default();
    config.level_count = 0;
    CHECK(mlfq_policy_create(&config, &error) == NULL, "cero niveles es invalido");

    error_clear(&error);
    config = mlfq_config_default();
    config.quantums[1] = 0;
    CHECK(mlfq_policy_create(&config, &error) == NULL, "quantum cero es invalido");
    CHECK(strstr(error.message, "Q1") != NULL, "el mensaje indica el nivel afectado");
}

void suite_mlfq_policy(void)
{
    test_democion_al_agotar_quantum();
    test_sin_democion_si_termina_antes();
    test_priority_boost();
    test_sin_boost_el_proceso_queda_en_el_fondo();
    test_expropiacion_por_mayor_prioridad();
    test_regla_de_democion_alternativa();
    test_configuracion_invalida();
}
