/*
 * trace_observer.c - Observador que imprime la traza ciclo a ciclo.
 */
#include "infrastructure/observers.h"

#include <stdlib.h>

struct TraceObserver {
    SimulationObserver base;
    FILE *stream;
    const SchedulingPolicy *policy;  /* opcional, solo lectura */
};

static void print_event(TraceObserver *observer, const SimulationEvent *event)
{
    char queues[128];
    scheduling_policy_describe_queues(observer->policy, queues, sizeof(queues));

    if (event->process == NULL) {
        fprintf(observer->stream, "[t=%3d] %-22s %-14s colas=%s\n",
                event->cycle, simulation_event_type_name(event->type), "", queues);
        return;
    }

    char transition[32] = "";
    if (event->from_queue != SIMULATION_QUEUE_NOT_APPLICABLE &&
        event->to_queue != SIMULATION_QUEUE_NOT_APPLICABLE &&
        event->from_queue != event->to_queue) {
        snprintf(transition, sizeof(transition), "Q%d->Q%d", event->from_queue, event->to_queue);
    } else if (event->to_queue != SIMULATION_QUEUE_NOT_APPLICABLE) {
        snprintf(transition, sizeof(transition), "Q%d", event->to_queue);
    }

    fprintf(observer->stream, "[t=%3d] %-22s P%-3d %-8s rest=%-3d %-11s colas=%s\n",
            event->cycle, simulation_event_type_name(event->type),
            process_pid(event->process), transition,
            process_remaining_time(event->process),
            process_state_name(event->process), queues);
}

static void trace_on_event(SimulationObserver *self, const SimulationEvent *event)
{
    print_event((TraceObserver *)self, event);
}

TraceObserver *trace_observer_create(FILE *stream, const SchedulingPolicy *policy, Error *error)
{
    if (stream == NULL) {
        error_set(error, ERROR_INVALID_ARGUMENT, "flujo nulo para la traza");
        return NULL;
    }
    TraceObserver *observer = calloc(1, sizeof(*observer));
    if (observer == NULL) {
        error_set(error, ERROR_OUT_OF_MEMORY, "sin memoria para el observador de traza");
        return NULL;
    }
    observer->base.name = "traza";
    observer->base.on_event = trace_on_event;
    observer->stream = stream;
    observer->policy = policy;
    return observer;
}

SimulationObserver *trace_observer_as_observer(TraceObserver *observer)
{
    return observer == NULL ? NULL : &observer->base;
}

void trace_observer_destroy(TraceObserver *observer)
{
    free(observer);
}
