#include "domain/event_bus.h"

#include <stdlib.h>

#define EVENT_BUS_MAX_OBSERVERS 16

struct EventBus {
    SimulationObserver *observers[EVENT_BUS_MAX_OBSERVERS];
    size_t count;
};

const char *simulation_event_type_name(SimulationEventType type)
{
    switch (type) {
    case EVENT_SIMULATION_STARTED:  return "SIMULACION_INICIADA";
    case EVENT_PROCESS_ARRIVED:     return "LLEGADA";
    case EVENT_PROCESS_DISPATCHED:  return "DESPACHO";
    case EVENT_CYCLE_EXECUTED:      return "CICLO_EJECUTADO";
    case EVENT_PROCESS_PREEMPTED:   return "EXPROPIACION";
    case EVENT_QUANTUM_EXPIRED:     return "QUANTUM_AGOTADO";
    case EVENT_PROCESS_DEMOTED:     return "DEMOCION";
    case EVENT_PRIORITY_BOOST:      return "PRIORITY_BOOST";
    case EVENT_PROCESS_COMPLETED:   return "FIN_PROCESO";
    case EVENT_CPU_IDLE:            return "CPU_OCIOSA";
    case EVENT_SIMULATION_FINISHED: return "SIMULACION_FINALIZADA";
    }
    return "DESCONOCIDO";
}

EventBus *event_bus_create(Error *error)
{
    EventBus *bus = calloc(1, sizeof(*bus));
    if (bus == NULL) {
        error_set(error, ERROR_OUT_OF_MEMORY, "sin memoria para el bus de eventos");
        return NULL;
    }
    return bus;
}

void event_bus_destroy(EventBus *bus)
{
    free(bus);
}

bool event_bus_subscribe(EventBus *bus, SimulationObserver *observer, Error *error)
{
    if (bus == NULL || observer == NULL || observer->on_event == NULL) {
        return error_set(error, ERROR_INVALID_ARGUMENT, "observador invalido");
    }
    if (bus->count == EVENT_BUS_MAX_OBSERVERS) {
        return error_set(error, ERROR_INVALID_ARGUMENT,
                         "limite de observadores alcanzado (%d)", EVENT_BUS_MAX_OBSERVERS);
    }
    bus->observers[bus->count++] = observer;
    return true;
}

void event_bus_publish(EventBus *bus, const SimulationEvent *event)
{
    if (bus == NULL || event == NULL) {
        return;
    }
    for (size_t i = 0; i < bus->count; i++) {
        SimulationObserver *observer = bus->observers[i];
        observer->on_event(observer, event);
    }
}

void event_bus_publish_simple(EventBus *bus, SimulationEventType type, int cycle,
                              const Process *process)
{
    event_bus_publish_queue_change(bus, type, cycle, process,
                                   SIMULATION_QUEUE_NOT_APPLICABLE,
                                   SIMULATION_QUEUE_NOT_APPLICABLE);
}

void event_bus_publish_queue_change(EventBus *bus, SimulationEventType type, int cycle,
                                    const Process *process, int from_queue, int to_queue)
{
    if (bus == NULL) {
        return;
    }
    SimulationEvent event = {
        .type = type,
        .cycle = cycle,
        .process = process,
        .from_queue = from_queue,
        .to_queue = to_queue,
        .detail = NULL
    };
    event_bus_publish(bus, &event);
}
