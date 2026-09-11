/*
 * ready_queue.h - ABSTRACCION de cola de listos (Dependency Inversion).
 *
 * El planificador NUNCA depende de un arreglo circular concreto: depende de
 * esta interfaz. Cambiar la disciplina interna de una cola (FIFO, por tiempo
 * restante, por prioridad externa...) no obliga a tocar el planificador ni el
 * motor de simulacion.
 *
 * En C la interfaz se modela con una tabla de punteros a funcion (vtable) y
 * la implementacion concreta "hereda" embebiendo `ReadyQueue base` como primer
 * campo del struct.
 */
#ifndef DOMAIN_READY_QUEUE_H
#define DOMAIN_READY_QUEUE_H

#include <stdbool.h>
#include <stddef.h>

#include "domain/error.h"
#include "domain/process.h"

typedef struct ReadyQueue ReadyQueue;

typedef struct ReadyQueueVTable {
    const char *name;
    bool (*enqueue)(ReadyQueue *self, Process *process, Error *error);
    Process *(*dequeue)(ReadyQueue *self);
    const Process *(*peek)(const ReadyQueue *self);
    size_t (*size)(const ReadyQueue *self);
    void (*destroy)(ReadyQueue *self);
} ReadyQueueVTable;

struct ReadyQueue {
    const ReadyQueueVTable *vtable;
};

/* Envoltorios: el codigo cliente llama estas funciones, no la vtable. */
const char *ready_queue_name(const ReadyQueue *queue);
bool ready_queue_enqueue(ReadyQueue *queue, Process *process, Error *error);
Process *ready_queue_dequeue(ReadyQueue *queue);
const Process *ready_queue_peek(const ReadyQueue *queue);
size_t ready_queue_size(const ReadyQueue *queue);
bool ready_queue_is_empty(const ReadyQueue *queue);
void ready_queue_destroy(ReadyQueue *queue);

/*
 * Fabrica de colas inyectable: permite que una politica cree sus niveles sin
 * conocer la implementacion concreta (Factory Method + DIP).
 */
typedef ReadyQueue *(*ReadyQueueFactory)(int level, Error *error);

/* Implementaciones disponibles (LSP: intercambiables sin cambiar clientes). */
ReadyQueue *fifo_ready_queue_create(int level, Error *error);
ReadyQueue *shortest_remaining_ready_queue_create(int level, Error *error);

#endif /* DOMAIN_READY_QUEUE_H */
