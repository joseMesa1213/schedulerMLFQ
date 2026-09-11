/*
 * main.c - Ejecutor de las pruebas unitarias: `make test`.
 *
 * Sin argumentos ejecuta las seis suites. Con argumentos ejecuta solo las
 * indicadas, lo que acorta el ciclo de trabajo cuando se esta depurando una
 * sola parte:
 *
 *   ./build/run_tests              todas
 *   ./build/run_tests mlfq         solo la politica MLFQ
 *   ./build/run_tests metrics engine
 *   ./build/run_tests --list       nombres disponibles
 */
#include <stdio.h>
#include <string.h>

#include "test_framework.h"
#include "test_suites.h"

int g_checks_run = 0;
int g_checks_failed = 0;
const char *g_current_test = "(sin nombre)";

typedef struct {
    const char *name;
    const char *description;
    void (*run)(void);
} Suite;

static const Suite kSuites[] = {
    { "state",   "ciclo de vida del proceso (patron State)",       suite_process_state },
    { "queue",   "colas de listos (abstraccion ReadyQueue)",       suite_ready_queue },
    { "metrics", "calculo de metricas",                            suite_metrics },
    { "mlfq",    "politica MLFQ (democion, boost, prioridad)",     suite_mlfq_policy },
    { "engine",  "motor de simulacion end-to-end",                 suite_simulation_engine },
    { "input",   "validacion de entradas",                         suite_process_sources }
};

static const size_t kSuiteCount = sizeof(kSuites) / sizeof(kSuites[0]);

static void print_available_suites(void)
{
    printf("Suites disponibles:\n");
    for (size_t i = 0; i < kSuiteCount; i++) {
        printf("  %-8s %s\n", kSuites[i].name, kSuites[i].description);
    }
}

static bool was_requested(const Suite *suite, int argc, char **argv)
{
    if (argc <= 1) {
        return true; /* sin argumentos: todas */
    }
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], suite->name) == 0) {
            return true;
        }
    }
    return false;
}

int main(int argc, char **argv)
{
    if (argc > 1 && (strcmp(argv[1], "--list") == 0 || strcmp(argv[1], "--help") == 0)) {
        print_available_suites();
        return 0;
    }

    printf("Ejecutando pruebas del simulador MLFQ\n\n");

    size_t executed = 0;
    for (size_t i = 0; i < kSuiteCount; i++) {
        if (!was_requested(&kSuites[i], argc, argv)) {
            continue;
        }
        printf("Suite: %s\n", kSuites[i].description);
        kSuites[i].run();
        executed++;
    }

    if (executed == 0) {
        printf("Ninguna suite coincide con los argumentos indicados.\n\n");
        print_available_suites();
        return 2;
    }

    printf("\n%zu suite(s), %d verificaciones, %d fallos\n", executed, g_checks_run, g_checks_failed);
    if (g_checks_failed == 0) {
        printf("RESULTADO: OK\n");
        return 0;
    }
    printf("RESULTADO: FALLIDO\n");
    return 1;
}
