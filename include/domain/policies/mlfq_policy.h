/*
 * mlfq_policy.h - Multi-Level Feedback Queue como implementacion de Strategy.
 *
 * Todo lo que la hace configurable viaja en `MlfqConfig`, no en constantes
 * dentro del .c. Agregar un cuarto nivel, cambiar los quantums, desactivar el
 * boost, cambiar la disciplina interna de las colas o cambiar la REGLA DE
 * DEMOCION son cambios de configuracion/inyeccion, no de codigo (OCP).
 */
#ifndef DOMAIN_POLICIES_MLFQ_POLICY_H
#define DOMAIN_POLICIES_MLFQ_POLICY_H

#include "domain/event_bus.h"
#include "domain/ready_queue.h"
#include "domain/scheduling_policy.h"

#define MLFQ_MAX_LEVELS 8

/*
 * Regla de democion: dado el nivel actual y la cantidad de niveles, devuelve
 * el nivel destino cuando un proceso agota su quantum. Es un punto de
 * extension: `mlfq_demote_one_level` es la del enunciado, pero se puede
 * inyectar otra (p. ej. bajar directo al ultimo nivel) sin tocar la politica.
 */
typedef int (*MlfqDemotionRule)(int current_level, int level_count);

int mlfq_demote_one_level(int current_level, int level_count);
int mlfq_demote_to_lowest(int current_level, int level_count);

typedef struct {
    int level_count;                    /* 1..MLFQ_MAX_LEVELS */
    int quantums[MLFQ_MAX_LEVELS];      /* quantum por nivel, en ciclos */
    int boost_interval;                 /* <= 0 desactiva el priority boost */
    bool preempt_on_higher_priority;    /* expropiar si llega alguien mejor */
    MlfqDemotionRule demotion_rule;     /* NULL => mlfq_demote_one_level */
    ReadyQueueFactory queue_factory;    /* NULL => fifo_ready_queue_create */
    EventBus *event_bus;                /* opcional */
} MlfqConfig;

/* Configuracion por defecto del enunciado: Q0=2, Q1=4, Q2=8, boost cada 20. */
MlfqConfig mlfq_config_default(void);

SchedulingPolicy *mlfq_policy_create(const MlfqConfig *config, Error *error);

#endif /* DOMAIN_POLICIES_MLFQ_POLICY_H */
