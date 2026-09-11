/*
 * simulation_event.h - Vocabulario de eventos de la simulacion (Observer).
 *
 * El evento es un DTO inmutable: describe algo que ya ocurrio, sin decir que
 * hacer con ello. Quien lo publica (motor / politica) no conoce a los
 * suscriptores, y quien lo consume (log, diagrama de Gantt, estadisticas) no
 * puede alterar la simulacion.
 */
#ifndef DOMAIN_SIMULATION_EVENT_H
#define DOMAIN_SIMULATION_EVENT_H

#include "domain/process.h"

#define SIMULATION_QUEUE_NOT_APPLICABLE (-1)

typedef enum {
    EVENT_SIMULATION_STARTED,
    EVENT_PROCESS_ARRIVED,
    EVENT_PROCESS_DISPATCHED,
    EVENT_CYCLE_EXECUTED,
    EVENT_PROCESS_PREEMPTED,
    EVENT_QUANTUM_EXPIRED,
    EVENT_PROCESS_DEMOTED,
    EVENT_PRIORITY_BOOST,
    EVENT_PROCESS_COMPLETED,
    EVENT_CPU_IDLE,
    EVENT_SIMULATION_FINISHED
} SimulationEventType;

typedef struct {
    SimulationEventType type;
    int cycle;
    const Process *process;  /* NULL en eventos globales */
    int from_queue;          /* SIMULATION_QUEUE_NOT_APPLICABLE si no aplica */
    int to_queue;
    const char *detail;      /* literal opcional, no se libera */
} SimulationEvent;

const char *simulation_event_type_name(SimulationEventType type);

#endif /* DOMAIN_SIMULATION_EVENT_H */
