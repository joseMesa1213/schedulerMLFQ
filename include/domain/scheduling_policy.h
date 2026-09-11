/*
 * scheduling_policy.h - ESTRATEGIA de planificacion (patron STRATEGY).
 *
 * Este es el contrato que separa el "motor de simulacion" (reloj discreto,
 * transiciones de estado, publicacion de eventos, metricas) de la "politica"
 * (quien ejecuta en cada ciclo). Cambiar MLFQ por FCFS o Round Robin es
 * cambiar la implementacion inyectada: el motor no se recompila ni se modifica
 * (OCP + DIP).
 *
 * Reparto de responsabilidades:
 *   - La politica es dueña de las colas, del quantum y de la degradacion.
 *   - El motor es dueño del reloj, del ciclo de vida del proceso y de los
 *     eventos "que le pasan" al proceso.
 *
 * Protocolo por ciclo de reloj (lo ejecuta el motor):
 *   1. admit(p)                -> para cada proceso que llega en este ciclo
 *   2. select_for_cycle(cycle) -> la politica devuelve quien debe ejecutar
 *   3. el motor ejecuta 1 ciclo del proceso elegido
 *   4. notify_cycle_result(...)-> la politica contabiliza el quantum usado
 */
#ifndef DOMAIN_SCHEDULING_POLICY_H
#define DOMAIN_SCHEDULING_POLICY_H

#include <stdbool.h>
#include <stddef.h>

#include "domain/error.h"
#include "domain/process.h"

typedef struct SchedulingPolicy SchedulingPolicy;

typedef struct SchedulingPolicyVTable {
    const char *name;

    /* Un proceso pasa a estar listo (recien llegado). La politica decide en
     * que cola lo coloca. */
    bool (*admit)(SchedulingPolicy *self, Process *process, int cycle, Error *error);

    /* Devuelve el proceso que debe ocupar la CPU durante `cycle`, o NULL si no
     * hay ninguno listo (CPU ociosa). Aqui ocurren la expiracion de quantum,
     * la democion y el priority boost. */
    Process *(*select_for_cycle)(SchedulingPolicy *self, int cycle, Error *error);

    /* El motor informa el resultado del ciclo ejecutado. */
    bool (*notify_cycle_result)(SchedulingPolicy *self, Process *process,
                                bool completed, int cycle, Error *error);

    /* Opcional (puede ser NULL): descripcion textual del estado de las colas,
     * usada solo para trazas de depuracion. */
    void (*describe_queues)(const SchedulingPolicy *self, char *buffer, size_t capacity);

    void (*destroy)(SchedulingPolicy *self);
} SchedulingPolicyVTable;

struct SchedulingPolicy {
    const SchedulingPolicyVTable *vtable;
};

const char *scheduling_policy_name(const SchedulingPolicy *policy);
bool scheduling_policy_admit(SchedulingPolicy *policy, Process *process, int cycle, Error *error);
Process *scheduling_policy_select_for_cycle(SchedulingPolicy *policy, int cycle, Error *error);
bool scheduling_policy_notify_cycle_result(SchedulingPolicy *policy, Process *process,
                                           bool completed, int cycle, Error *error);
void scheduling_policy_describe_queues(const SchedulingPolicy *policy, char *buffer, size_t capacity);
void scheduling_policy_destroy(SchedulingPolicy *policy);

#endif /* DOMAIN_SCHEDULING_POLICY_H */
