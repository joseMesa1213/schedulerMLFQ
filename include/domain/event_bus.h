/*
 * event_bus.h - Sujeto del patron OBSERVER.
 *
 * Cualquier numero de observadores se suscribe; el motor solo publica. Asi el
 * motor de simulacion no acumula responsabilidades de presentacion (SRP) y se
 * pueden agregar nuevas vistas (traza, Gantt, estadisticas, futura UI) sin
 * modificarlo (OCP).
 */
#ifndef DOMAIN_EVENT_BUS_H
#define DOMAIN_EVENT_BUS_H

#include "domain/error.h"
#include "domain/simulation_event.h"

typedef struct SimulationObserver SimulationObserver;

/* Interfaz de un observador: un unico metodo (Interface Segregation). */
struct SimulationObserver {
    const char *name;
    void (*on_event)(SimulationObserver *self, const SimulationEvent *event);
};

typedef struct EventBus EventBus;

EventBus *event_bus_create(Error *error);
void event_bus_destroy(EventBus *bus);

/* El bus NO toma posesion del observador (lo crea y libera la capa externa). */
bool event_bus_subscribe(EventBus *bus, SimulationObserver *observer, Error *error);

/* Publicar en un bus NULL es una operacion valida y sin efecto: permite
 * ejecutar la simulacion sin observadores sin llenar el motor de `if`. */
void event_bus_publish(EventBus *bus, const SimulationEvent *event);

/* Azucar sintactico usado por el motor y las politicas. */
void event_bus_publish_simple(EventBus *bus, SimulationEventType type, int cycle,
                              const Process *process);
void event_bus_publish_queue_change(EventBus *bus, SimulationEventType type, int cycle,
                                    const Process *process, int from_queue, int to_queue);

#endif /* DOMAIN_EVENT_BUS_H */
