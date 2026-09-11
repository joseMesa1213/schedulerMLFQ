#include "domain/policies/simple_policies.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

/*
 * FCFS y Round Robin comparten estructura (una cola + un ocupante de CPU) y
 * solo difieren en el quantum: FCFS equivale a un quantum infinito. Se
 * comparte la implementacion para no duplicar logica (DRY) y se distingue el
 * nombre publico de cada politica en su propia vtable.
 */
typedef struct {
    SchedulingPolicy base;
    ReadyQueue *queue;
    int quantum;              /* INT_MAX => no expropiativo (FCFS) */
    EventBus *event_bus;
    Process *cpu_holder;
    int consumed_quantum;
} SingleQueuePolicy;

static bool single_admit(SchedulingPolicy *self, Process *process, int cycle, Error *error)
{
    SingleQueuePolicy *policy = (SingleQueuePolicy *)self;
    (void)cycle;
    if (!process_move_to_queue(process, 0, error)) {
        return false;
    }
    return ready_queue_enqueue(policy->queue, process, error);
}

static Process *single_select_for_cycle(SchedulingPolicy *self, int cycle, Error *error)
{
    SingleQueuePolicy *policy = (SingleQueuePolicy *)self;

    if (policy->cpu_holder != NULL && policy->consumed_quantum >= policy->quantum) {
        event_bus_publish_simple(policy->event_bus, EVENT_QUANTUM_EXPIRED, cycle,
                                 policy->cpu_holder);
        if (!ready_queue_enqueue(policy->queue, policy->cpu_holder, error)) {
            return NULL;
        }
        policy->cpu_holder = NULL;
        policy->consumed_quantum = 0;
    }

    if (policy->cpu_holder == NULL) {
        policy->cpu_holder = ready_queue_dequeue(policy->queue);
        policy->consumed_quantum = 0;
    }
    return policy->cpu_holder;
}

static bool single_notify_cycle_result(SchedulingPolicy *self, Process *process,
                                       bool completed, int cycle, Error *error)
{
    SingleQueuePolicy *policy = (SingleQueuePolicy *)self;
    (void)cycle;
    if (process != policy->cpu_holder) {
        return error_set(error, ERROR_INVALID_ARGUMENT,
                         "inconsistencia: se reporto P%d sin tener la CPU", process_pid(process));
    }
    policy->consumed_quantum++;
    if (completed) {
        policy->cpu_holder = NULL;
        policy->consumed_quantum = 0;
    }
    return true;
}

static void single_describe_queues(const SchedulingPolicy *self, char *buffer, size_t capacity)
{
    const SingleQueuePolicy *policy = (const SingleQueuePolicy *)self;
    snprintf(buffer, capacity, "Q0:%zu", ready_queue_size(policy->queue));
}

static void single_destroy(SchedulingPolicy *self)
{
    SingleQueuePolicy *policy = (SingleQueuePolicy *)self;
    ready_queue_destroy(policy->queue);
    free(policy);
}

static const SchedulingPolicyVTable kFcfsVTable = {
    "FCFS (First Come First Served)",
    single_admit, single_select_for_cycle, single_notify_cycle_result,
    single_describe_queues, single_destroy
};

static const SchedulingPolicyVTable kRoundRobinVTable = {
    "Round Robin",
    single_admit, single_select_for_cycle, single_notify_cycle_result,
    single_describe_queues, single_destroy
};

static SchedulingPolicy *create_single_queue_policy(const SchedulingPolicyVTable *vtable,
                                                    int quantum, ReadyQueueFactory queue_factory,
                                                    EventBus *bus, Error *error)
{
    SingleQueuePolicy *policy = calloc(1, sizeof(*policy));
    if (policy == NULL) {
        error_set(error, ERROR_OUT_OF_MEMORY, "sin memoria para la politica %s", vtable->name);
        return NULL;
    }
    policy->base.vtable = vtable;
    policy->quantum = quantum;
    policy->event_bus = bus;
    policy->queue = (queue_factory ? queue_factory : fifo_ready_queue_create)(0, error);
    if (policy->queue == NULL) {
        free(policy);
        return NULL;
    }
    return &policy->base;
}

SchedulingPolicy *fcfs_policy_create(ReadyQueueFactory queue_factory, EventBus *bus, Error *error)
{
    return create_single_queue_policy(&kFcfsVTable, INT_MAX, queue_factory, bus, error);
}

SchedulingPolicy *round_robin_policy_create(int quantum, ReadyQueueFactory queue_factory,
                                            EventBus *bus, Error *error)
{
    if (quantum <= 0) {
        error_set(error, ERROR_INVALID_ARGUMENT, "quantum invalido (%d) para Round Robin", quantum);
        return NULL;
    }
    return create_single_queue_policy(&kRoundRobinVTable, quantum, queue_factory, bus, error);
}
