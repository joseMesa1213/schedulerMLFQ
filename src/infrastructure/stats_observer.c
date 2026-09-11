/*
 * stats_observer.c - Observador que agrega contadores de la simulacion.
 *
 * Sirve de doble uso: se imprime en el informe y las pruebas unitarias lo usan
 * como sonda para verificar que la democion y el boost realmente ocurrieron,
 * sin tener que exponer el interior de la politica MLFQ.
 */
#include "infrastructure/observers.h"

#include <stdlib.h>

struct StatsObserver {
    SimulationObserver base;
    int demotions;
    int boosts;
    int preemptions;
    int quantum_expirations;
    int dispatches;
    int completions;
};

static void stats_on_event(SimulationObserver *self, const SimulationEvent *event)
{
    StatsObserver *stats = (StatsObserver *)self;
    switch (event->type) {
    case EVENT_PROCESS_DEMOTED:   stats->demotions++; break;
    case EVENT_QUANTUM_EXPIRED:   stats->quantum_expirations++; break;
    case EVENT_PROCESS_PREEMPTED: stats->preemptions++; break;
    case EVENT_PROCESS_DISPATCHED:stats->dispatches++; break;
    case EVENT_PROCESS_COMPLETED: stats->completions++; break;
    case EVENT_PRIORITY_BOOST:
        /* El boost global se publica una vez sin proceso y luego una vez por
         * proceso promovido; solo se cuenta el evento global. */
        if (event->process == NULL) {
            stats->boosts++;
        }
        break;
    default:
        break;
    }
}

StatsObserver *stats_observer_create(Error *error)
{
    StatsObserver *stats = calloc(1, sizeof(*stats));
    if (stats == NULL) {
        error_set(error, ERROR_OUT_OF_MEMORY, "sin memoria para el observador de estadisticas");
        return NULL;
    }
    stats->base.name = "estadisticas";
    stats->base.on_event = stats_on_event;
    return stats;
}

SimulationObserver *stats_observer_as_observer(StatsObserver *observer)
{
    return observer == NULL ? NULL : &observer->base;
}

void stats_observer_print(const StatsObserver *observer, FILE *stream)
{
    if (observer == NULL || stream == NULL) {
        return;
    }
    fprintf(stream, "\n=== Eventos observados ===\n");
    fprintf(stream, "Despachos            : %d\n", observer->dispatches);
    fprintf(stream, "Quantums agotados    : %d\n", observer->quantum_expirations);
    fprintf(stream, "Demociones           : %d\n", observer->demotions);
    fprintf(stream, "Priority boosts      : %d\n", observer->boosts);
    fprintf(stream, "Expropiaciones       : %d\n", observer->preemptions);
    fprintf(stream, "Procesos completados : %d\n", observer->completions);
}

int stats_observer_demotions(const StatsObserver *observer)   { return observer->demotions; }
int stats_observer_boosts(const StatsObserver *observer)       { return observer->boosts; }
int stats_observer_preemptions(const StatsObserver *observer)   { return observer->preemptions; }

void stats_observer_destroy(StatsObserver *observer)
{
    free(observer);
}
