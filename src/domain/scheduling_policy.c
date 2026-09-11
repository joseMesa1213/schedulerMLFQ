#include "domain/scheduling_policy.h"

#include <stdio.h>

const char *scheduling_policy_name(const SchedulingPolicy *policy)
{
    return policy == NULL ? "(nula)" : policy->vtable->name;
}

bool scheduling_policy_admit(SchedulingPolicy *policy, Process *process, int cycle, Error *error)
{
    if (policy == NULL || process == NULL) {
        return error_set(error, ERROR_INVALID_ARGUMENT, "politica o proceso nulo al admitir");
    }
    return policy->vtable->admit(policy, process, cycle, error);
}

Process *scheduling_policy_select_for_cycle(SchedulingPolicy *policy, int cycle, Error *error)
{
    if (policy == NULL) {
        error_set(error, ERROR_INVALID_ARGUMENT, "politica nula al seleccionar proceso");
        return NULL;
    }
    return policy->vtable->select_for_cycle(policy, cycle, error);
}

bool scheduling_policy_notify_cycle_result(SchedulingPolicy *policy, Process *process,
                                           bool completed, int cycle, Error *error)
{
    if (policy == NULL || process == NULL) {
        return error_set(error, ERROR_INVALID_ARGUMENT, "politica o proceso nulo al notificar");
    }
    return policy->vtable->notify_cycle_result(policy, process, completed, cycle, error);
}

void scheduling_policy_describe_queues(const SchedulingPolicy *policy, char *buffer, size_t capacity)
{
    if (buffer == NULL || capacity == 0) {
        return;
    }
    if (policy == NULL || policy->vtable->describe_queues == NULL) {
        snprintf(buffer, capacity, "-");
        return;
    }
    policy->vtable->describe_queues(policy, buffer, capacity);
}

void scheduling_policy_destroy(SchedulingPolicy *policy)
{
    if (policy != NULL) {
        policy->vtable->destroy(policy);
    }
}
