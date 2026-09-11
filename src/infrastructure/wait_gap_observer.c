/*
 * wait_gap_observer.c - Observador de espera continua maxima (inanicion).
 *
 * Para cada proceso mide el intervalo mas largo, en ciclos, que estuvo listo
 * sin recibir CPU. El turnaround no revela la inanicion cuando la CPU esta
 * saturada (el ultimo proceso en terminar siempre termina al final del trabajo
 * total), pero esta metrica si: es la que permite comparar el efecto del
 * priority boost.
 *
 * Que exista sin haber tocado el motor de simulacion es la mejor justificacion
 * del patron Observer en este proyecto.
 */
#include "infrastructure/observers.h"

#include <stdlib.h>

#define WAIT_GAP_MAX_PROCESSES 128

typedef struct {
    int pid;
    int ready_since;    /* ciclo desde el cual esta esperando CPU */
    int longest_wait;
    int longest_wait_at;
} WaitGapEntry;

struct WaitGapObserver {
    SimulationObserver base;
    WaitGapEntry entries[WAIT_GAP_MAX_PROCESSES];
    size_t count;
};

static WaitGapEntry *entry_for(WaitGapObserver *observer, int pid)
{
    for (size_t i = 0; i < observer->count; i++) {
        if (observer->entries[i].pid == pid) {
            return &observer->entries[i];
        }
    }
    if (observer->count == WAIT_GAP_MAX_PROCESSES) {
        return NULL;
    }
    WaitGapEntry *entry = &observer->entries[observer->count++];
    entry->pid = pid;
    entry->ready_since = 0;
    entry->longest_wait = 0;
    entry->longest_wait_at = 0;
    return entry;
}

static void wait_gap_on_event(SimulationObserver *self, const SimulationEvent *event)
{
    WaitGapObserver *observer = (WaitGapObserver *)self;
    if (event->process == NULL) {
        return;
    }
    WaitGapEntry *entry = entry_for(observer, process_pid(event->process));
    if (entry == NULL) {
        return;
    }

    if (event->type == EVENT_PROCESS_ARRIVED) {
        entry->ready_since = event->cycle;
    } else if (event->type == EVENT_CYCLE_EXECUTED) {
        int waited = event->cycle - entry->ready_since;
        if (waited > entry->longest_wait) {
            entry->longest_wait = waited;
            entry->longest_wait_at = event->cycle;
        }
        /* Desde el proximo ciclo vuelve a contar como espera. */
        entry->ready_since = event->cycle + 1;
    }
}

WaitGapObserver *wait_gap_observer_create(Error *error)
{
    WaitGapObserver *observer = calloc(1, sizeof(*observer));
    if (observer == NULL) {
        error_set(error, ERROR_OUT_OF_MEMORY, "sin memoria para el observador de esperas");
        return NULL;
    }
    observer->base.name = "espera-maxima";
    observer->base.on_event = wait_gap_on_event;
    return observer;
}

SimulationObserver *wait_gap_observer_as_observer(WaitGapObserver *observer)
{
    return observer == NULL ? NULL : &observer->base;
}

void wait_gap_observer_print(const WaitGapObserver *observer, FILE *stream)
{
    if (observer == NULL || stream == NULL) {
        return;
    }
    fprintf(stream, "\n=== Espera continua maxima (indicador de inanicion) ===\n");
    for (size_t i = 0; i < observer->count; i++) {
        fprintf(stream, "P%-4d espera continua maxima: %3d ciclos (hasta el ciclo %d)\n",
                observer->entries[i].pid, observer->entries[i].longest_wait,
                observer->entries[i].longest_wait_at);
    }
}

int wait_gap_observer_longest_for(const WaitGapObserver *observer, int pid)
{
    if (observer == NULL) {
        return -1;
    }
    for (size_t i = 0; i < observer->count; i++) {
        if (observer->entries[i].pid == pid) {
            return observer->entries[i].longest_wait;
        }
    }
    return -1;
}

void wait_gap_observer_destroy(WaitGapObserver *observer)
{
    free(observer);
}
