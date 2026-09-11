/*
 * gantt_observer.c - Observador que reconstruye la linea de tiempo.
 *
 * Solo escucha EVENT_CYCLE_EXECUTED y EVENT_CPU_IDLE: con eso puede dibujar
 * quien tuvo la CPU en cada ciclo y en que nivel de cola estaba. Es el mejor
 * ejemplo de por que Observer aporta valor aqui: esta vista no existiria sin
 * inspeccionar el interior del motor si no fuera por los eventos.
 */
#include "infrastructure/observers.h"

#include <stdlib.h>

#define GANTT_INITIAL_CAPACITY 128
#define GANTT_MAX_TRACKED_PIDS 64

typedef struct {
    int pid;        /* -1 => CPU ociosa */
    int queue_level;
} GanttSlot;

struct GanttObserver {
    SimulationObserver base;
    GanttSlot *slots;
    size_t capacity;
    size_t count;
    int pids[GANTT_MAX_TRACKED_PIDS];
    size_t pid_count;
};

static void remember_pid(GanttObserver *observer, int pid)
{
    for (size_t i = 0; i < observer->pid_count; i++) {
        if (observer->pids[i] == pid) {
            return;
        }
    }
    if (observer->pid_count < GANTT_MAX_TRACKED_PIDS) {
        observer->pids[observer->pid_count++] = pid;
    }
}

static void append_slot(GanttObserver *observer, int pid, int queue_level)
{
    if (observer->count == observer->capacity) {
        size_t new_capacity = observer->capacity * 2;
        GanttSlot *slots = realloc(observer->slots, new_capacity * sizeof(*slots));
        if (slots == NULL) {
            return; /* la vista se degrada, pero la simulacion no falla por un log */
        }
        observer->slots = slots;
        observer->capacity = new_capacity;
    }
    observer->slots[observer->count].pid = pid;
    observer->slots[observer->count].queue_level = queue_level;
    observer->count++;
}

static void gantt_on_event(SimulationObserver *self, const SimulationEvent *event)
{
    GanttObserver *observer = (GanttObserver *)self;
    if (event->type == EVENT_CYCLE_EXECUTED) {
        remember_pid(observer, process_pid(event->process));
        append_slot(observer, process_pid(event->process), event->to_queue);
    } else if (event->type == EVENT_CPU_IDLE) {
        append_slot(observer, -1, SIMULATION_QUEUE_NOT_APPLICABLE);
    }
}

GanttObserver *gantt_observer_create(Error *error)
{
    GanttObserver *observer = calloc(1, sizeof(*observer));
    if (observer == NULL) {
        error_set(error, ERROR_OUT_OF_MEMORY, "sin memoria para el observador de Gantt");
        return NULL;
    }
    observer->slots = calloc(GANTT_INITIAL_CAPACITY, sizeof(*observer->slots));
    if (observer->slots == NULL) {
        free(observer);
        error_set(error, ERROR_OUT_OF_MEMORY, "sin memoria para la linea de tiempo");
        return NULL;
    }
    observer->capacity = GANTT_INITIAL_CAPACITY;
    observer->base.name = "gantt";
    observer->base.on_event = gantt_on_event;
    return observer;
}

SimulationObserver *gantt_observer_as_observer(GanttObserver *observer)
{
    return observer == NULL ? NULL : &observer->base;
}

static void print_time_ruler(const GanttObserver *observer, FILE *stream)
{
    fprintf(stream, "%-6s ", "t");
    for (size_t t = 0; t < observer->count; t++) {
        fprintf(stream, "%d", (int)(t % 10));
    }
    fprintf(stream, "\n");
}

void gantt_observer_print(const GanttObserver *observer, FILE *stream)
{
    if (observer == NULL || stream == NULL) {
        return;
    }
    fprintf(stream, "\n=== Linea de tiempo (el digito indica el nivel de cola en que ejecuto) ===\n");
    print_time_ruler(observer, stream);

    for (size_t i = 0; i < observer->pid_count; i++) {
        int pid = observer->pids[i];
        fprintf(stream, "P%-5d ", pid);
        for (size_t t = 0; t < observer->count; t++) {
            if (observer->slots[t].pid == pid) {
                fprintf(stream, "%d", observer->slots[t].queue_level);
            } else {
                fprintf(stream, ".");
            }
        }
        fprintf(stream, "\n");
    }

    fprintf(stream, "%-6s ", "IDLE");
    for (size_t t = 0; t < observer->count; t++) {
        fprintf(stream, "%s", observer->slots[t].pid == -1 ? "x" : ".");
    }
    fprintf(stream, "\n");
}

void gantt_observer_destroy(GanttObserver *observer)
{
    if (observer == NULL) {
        return;
    }
    free(observer->slots);
    free(observer);
}
