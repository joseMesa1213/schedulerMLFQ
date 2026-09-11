#include "domain/policies/mlfq_policy.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int mlfq_demote_one_level(int current_level, int level_count)
{
    int next = current_level + 1;
    return next < level_count ? next : level_count - 1;
}

int mlfq_demote_to_lowest(int current_level, int level_count)
{
    (void)current_level;
    return level_count - 1;
}

MlfqConfig mlfq_config_default(void)
{
    MlfqConfig config = {
        .level_count = 3,
        .quantums = { 2, 4, 8, 0, 0, 0, 0, 0 },
        .boost_interval = 20,
        .preempt_on_higher_priority = true,
        .demotion_rule = mlfq_demote_one_level,
        .queue_factory = fifo_ready_queue_create,
        .event_bus = NULL
    };
    return config;
}

typedef struct {
    SchedulingPolicy base;
    ReadyQueue *levels[MLFQ_MAX_LEVELS];
    int level_count;
    int quantums[MLFQ_MAX_LEVELS];
    int boost_interval;
    bool preempt_on_higher_priority;
    MlfqDemotionRule demotion_rule;
    EventBus *event_bus;

    /* Ocupante actual de la CPU y ciclos que ya consumio de su quantum.
     * El nivel del proceso NO se duplica aqui: la fuente de verdad es
     * process_current_queue(), para que no puedan desincronizarse. */
    Process *cpu_holder;
    int consumed_quantum;
} MlfqPolicy;

static int quantum_of(const MlfqPolicy *policy, const Process *process)
{
    return policy->quantums[process_current_queue(process)];
}

/* Nivel de mayor prioridad con procesos listos, o -1 si todas estan vacias. */
static int highest_ready_level(const MlfqPolicy *policy)
{
    for (int level = 0; level < policy->level_count; level++) {
        if (!ready_queue_is_empty(policy->levels[level])) {
            return level;
        }
    }
    return -1;
}

static bool enqueue_at_level(MlfqPolicy *policy, Process *process, int level, Error *error)
{
    if (!process_move_to_queue(process, level, error)) {
        return false;
    }
    return ready_queue_enqueue(policy->levels[level], process, error);
}

static bool mlfq_admit(SchedulingPolicy *self, Process *process, int cycle, Error *error)
{
    MlfqPolicy *policy = (MlfqPolicy *)self;
    (void)cycle;
    /* Todo proceso nuevo entra por la cola de mayor prioridad: es la hipotesis
     * optimista de MLFQ (se asume interactivo hasta que demuestre lo contrario). */
    return enqueue_at_level(policy, process, 0, error);
}

/*
 * Quantum agotado: el proceso baja de nivel segun la regla inyectada y vuelve
 * al final de la cola destino. Un unico lugar concentra el movimiento entre
 * colas (DRY): no hay codigo repetido por nivel.
 */
static bool expire_quantum(MlfqPolicy *policy, int cycle, Error *error)
{
    Process *process = policy->cpu_holder;
    int from_level = process_current_queue(process);
    int to_level = policy->demotion_rule(from_level, policy->level_count);

    event_bus_publish_queue_change(policy->event_bus, EVENT_QUANTUM_EXPIRED, cycle,
                                   process, from_level, to_level);
    if (to_level != from_level) {
        event_bus_publish_queue_change(policy->event_bus, EVENT_PROCESS_DEMOTED, cycle,
                                       process, from_level, to_level);
    }
    if (!enqueue_at_level(policy, process, to_level, error)) {
        return false;
    }
    policy->cpu_holder = NULL;
    policy->consumed_quantum = 0;
    return true;
}

/*
 * Priority boost: cada S ciclos todos los procesos regresan a Q0. Evita la
 * inanicion de los procesos largos que ya fueron degradados hasta el fondo.
 * Decision documentada: el proceso en CPU tambien es promovido y su quantum se
 * reinicia con el valor de Q0, pero NO pierde la CPU en ese mismo ciclo (no
 * tiene sentido expropiarlo para volver a elegirlo).
 */
static bool apply_priority_boost(MlfqPolicy *policy, int cycle, Error *error)
{
    event_bus_publish_simple(policy->event_bus, EVENT_PRIORITY_BOOST, cycle, NULL);

    for (int level = 1; level < policy->level_count; level++) {
        Process *process = NULL;
        while ((process = ready_queue_dequeue(policy->levels[level])) != NULL) {
            event_bus_publish_queue_change(policy->event_bus, EVENT_PRIORITY_BOOST, cycle,
                                           process, level, 0);
            if (!enqueue_at_level(policy, process, 0, error)) {
                return false;
            }
        }
    }

    if (policy->cpu_holder != NULL && process_current_queue(policy->cpu_holder) != 0) {
        event_bus_publish_queue_change(policy->event_bus, EVENT_PRIORITY_BOOST, cycle,
                                       policy->cpu_holder,
                                       process_current_queue(policy->cpu_holder), 0);
        if (!process_move_to_queue(policy->cpu_holder, 0, error)) {
            return false;
        }
        policy->consumed_quantum = 0;
    }
    return true;
}

/* Expropiacion por prioridad: el enunciado exige ejecutar siempre la cola de
 * mayor prioridad no vacia, asi que si llega alguien mejor el ocupante vuelve
 * al final de SU MISMO nivel (no se degrada: no agoto su quantum). */
static bool preempt_holder(MlfqPolicy *policy, int cycle, Error *error)
{
    (void)cycle;
    Process *process = policy->cpu_holder;
    int level = process_current_queue(process);
    if (!enqueue_at_level(policy, process, level, error)) {
        return false;
    }
    /* El evento EVENT_PROCESS_PREEMPTED lo publica el motor, que es el dueño de
     * la transicion de estado RUNNING->READY: aqui solo se reordena la cola. */
    policy->cpu_holder = NULL;
    policy->consumed_quantum = 0;
    return true;
}

static Process *mlfq_select_for_cycle(SchedulingPolicy *self, int cycle, Error *error)
{
    MlfqPolicy *policy = (MlfqPolicy *)self;

    if (policy->cpu_holder != NULL && policy->consumed_quantum >= quantum_of(policy, policy->cpu_holder)) {
        if (!expire_quantum(policy, cycle, error)) {
            return NULL;
        }
    }

    if (policy->boost_interval > 0 && cycle > 0 && cycle % policy->boost_interval == 0) {
        if (!apply_priority_boost(policy, cycle, error)) {
            return NULL;
        }
    }

    if (policy->cpu_holder != NULL && policy->preempt_on_higher_priority) {
        int best = highest_ready_level(policy);
        if (best >= 0 && best < process_current_queue(policy->cpu_holder)) {
            if (!preempt_holder(policy, cycle, error)) {
                return NULL;
            }
        }
    }

    if (policy->cpu_holder == NULL) {
        int level = highest_ready_level(policy);
        if (level < 0) {
            return NULL; /* CPU ociosa */
        }
        policy->cpu_holder = ready_queue_dequeue(policy->levels[level]);
        policy->consumed_quantum = 0;
    }
    return policy->cpu_holder;
}

static bool mlfq_notify_cycle_result(SchedulingPolicy *self, Process *process,
                                     bool completed, int cycle, Error *error)
{
    MlfqPolicy *policy = (MlfqPolicy *)self;
    (void)cycle;
    if (process != policy->cpu_holder) {
        return error_set(error, ERROR_INVALID_ARGUMENT,
                         "el motor reporto P%d pero la CPU la tenia %s",
                         process_pid(process),
                         policy->cpu_holder ? "otro proceso" : "nadie");
    }
    policy->consumed_quantum++;
    if (completed) {
        policy->cpu_holder = NULL;
        policy->consumed_quantum = 0;
    }
    return true;
}

static void mlfq_describe_queues(const SchedulingPolicy *self, char *buffer, size_t capacity)
{
    const MlfqPolicy *policy = (const MlfqPolicy *)self;
    size_t used = 0;
    buffer[0] = '\0';
    for (int level = 0; level < policy->level_count && used + 1 < capacity; level++) {
        int written = snprintf(buffer + used, capacity - used, "%sQ%d:%zu",
                               level == 0 ? "" : " ", level,
                               ready_queue_size(policy->levels[level]));
        if (written < 0) {
            return;
        }
        used += (size_t)written;
    }
}

static void mlfq_destroy(SchedulingPolicy *self)
{
    MlfqPolicy *policy = (MlfqPolicy *)self;
    for (int level = 0; level < policy->level_count; level++) {
        ready_queue_destroy(policy->levels[level]);
    }
    free(policy);
}

static const SchedulingPolicyVTable kMlfqVTable = {
    "MLFQ (Multi-Level Feedback Queue)",
    mlfq_admit,
    mlfq_select_for_cycle,
    mlfq_notify_cycle_result,
    mlfq_describe_queues,
    mlfq_destroy
};

static bool validate_config(const MlfqConfig *config, Error *error)
{
    if (config == NULL) {
        return error_set(error, ERROR_INVALID_ARGUMENT, "configuracion MLFQ nula");
    }
    if (config->level_count < 1 || config->level_count > MLFQ_MAX_LEVELS) {
        return error_set(error, ERROR_INVALID_ARGUMENT,
                         "numero de niveles invalido (%d): debe estar entre 1 y %d",
                         config->level_count, MLFQ_MAX_LEVELS);
    }
    for (int level = 0; level < config->level_count; level++) {
        if (config->quantums[level] <= 0) {
            return error_set(error, ERROR_INVALID_ARGUMENT,
                             "quantum invalido en Q%d (%d): debe ser > 0",
                             level, config->quantums[level]);
        }
    }
    return true;
}

SchedulingPolicy *mlfq_policy_create(const MlfqConfig *config, Error *error)
{
    if (!validate_config(config, error)) {
        return NULL;
    }

    MlfqPolicy *policy = calloc(1, sizeof(*policy));
    if (policy == NULL) {
        error_set(error, ERROR_OUT_OF_MEMORY, "sin memoria para la politica MLFQ");
        return NULL;
    }
    policy->base.vtable = &kMlfqVTable;
    policy->level_count = config->level_count;
    memcpy(policy->quantums, config->quantums, sizeof(policy->quantums));
    policy->boost_interval = config->boost_interval;
    policy->preempt_on_higher_priority = config->preempt_on_higher_priority;
    policy->demotion_rule = config->demotion_rule ? config->demotion_rule : mlfq_demote_one_level;
    policy->event_bus = config->event_bus;

    ReadyQueueFactory factory = config->queue_factory ? config->queue_factory : fifo_ready_queue_create;
    for (int level = 0; level < policy->level_count; level++) {
        policy->levels[level] = factory(level, error);
        if (policy->levels[level] == NULL) {
            mlfq_destroy(&policy->base);
            return NULL;
        }
    }
    return &policy->base;
}
