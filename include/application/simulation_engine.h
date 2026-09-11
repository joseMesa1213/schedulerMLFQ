/*
 * simulation_engine.h - Motor de simulacion por ciclos de reloj discretos.
 *
 * Responsabilidad unica: hacer avanzar el reloj y mantener coherente el ciclo
 * de vida de los procesos. NO sabe cuantas colas hay, ni que es un quantum, ni
 * que es un priority boost: todo eso lo decide la SchedulingPolicy inyectada.
 * Tampoco imprime nada ni escribe archivos: solo publica eventos.
 *
 * Convencion temporal (importante para leer las metricas):
 *   el ciclo `t` representa el intervalo [t, t+1). Un proceso que ejecuta su
 *   ultimo ciclo en t tiene finish_time = t + 1.
 */
#ifndef APPLICATION_SIMULATION_ENGINE_H
#define APPLICATION_SIMULATION_ENGINE_H

#include "domain/error.h"
#include "domain/event_bus.h"
#include "domain/process_table.h"
#include "domain/scheduling_policy.h"

/* Cota de seguridad: evita un bucle infinito si una politica mal implementada
 * nunca elige un proceso listo. Se reporta como error, no como cuelgue. */
#define SIMULATION_DEFAULT_MAX_CYCLES 100000

typedef struct {
    int total_cycles;      /* ciclos simulados, incluidos los ociosos */
    int idle_cycles;
    int context_switches;  /* despachos que cambiaron el proceso en CPU */
} SimulationResult;

typedef struct {
    ProcessTable *processes;    /* requerido */
    SchedulingPolicy *policy;   /* requerido (Strategy inyectada) */
    EventBus *event_bus;        /* opcional */
    int max_cycles;             /* <= 0 => SIMULATION_DEFAULT_MAX_CYCLES */
} SimulationConfig;

bool simulation_engine_run(const SimulationConfig *config, SimulationResult *result, Error *error);

#endif /* APPLICATION_SIMULATION_ENGINE_H */
