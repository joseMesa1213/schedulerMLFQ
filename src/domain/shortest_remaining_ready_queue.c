/*
 * shortest_remaining_ready_queue.c - Cola ordenada por tiempo restante.
 *
 * Existe para demostrar OCP/LSP de forma verificable: es una segunda
 * implementacion de `ReadyQueue` que se puede inyectar en la politica MLFQ
 * (bandera --queue srtf) SIN modificar ni una linea del planificador ni del
 * motor. Empate: se conserva el orden de llegada a la cola (estable).
 */
#include "domain/ready_queue.h"

#include <stdlib.h>

#define SRTF_INITIAL_CAPACITY 8

typedef struct {
    ReadyQueue base;
    Process **items; /* items[0] es el proximo a ejecutar */
    size_t capacity;
    size_t count;
} SortedReadyQueue;

static bool sorted_grow(SortedReadyQueue *queue, Error *error)
{
    size_t new_capacity = queue->capacity * 2;
    Process **items = realloc(queue->items, new_capacity * sizeof(*items));
    if (items == NULL) {
        return error_set(error, ERROR_OUT_OF_MEMORY,
                         "no se pudo ampliar la cola ordenada a %zu", new_capacity);
    }
    queue->items = items;
    queue->capacity = new_capacity;
    return true;
}

static bool sorted_enqueue(ReadyQueue *self, Process *process, Error *error)
{
    SortedReadyQueue *queue = (SortedReadyQueue *)self;
    if (queue->count == queue->capacity && !sorted_grow(queue, error)) {
        return false;
    }
    size_t position = queue->count;
    while (position > 0 &&
           process_remaining_time(queue->items[position - 1]) > process_remaining_time(process)) {
        queue->items[position] = queue->items[position - 1];
        position--;
    }
    queue->items[position] = process;
    queue->count++;
    return true;
}

static Process *sorted_dequeue(ReadyQueue *self)
{
    SortedReadyQueue *queue = (SortedReadyQueue *)self;
    if (queue->count == 0) {
        return NULL;
    }
    Process *process = queue->items[0];
    for (size_t i = 1; i < queue->count; i++) {
        queue->items[i - 1] = queue->items[i];
    }
    queue->count--;
    return process;
}

static const Process *sorted_peek(const ReadyQueue *self)
{
    const SortedReadyQueue *queue = (const SortedReadyQueue *)self;
    return queue->count == 0 ? NULL : queue->items[0];
}

static size_t sorted_size(const ReadyQueue *self)
{
    return ((const SortedReadyQueue *)self)->count;
}

static void sorted_destroy(ReadyQueue *self)
{
    SortedReadyQueue *queue = (SortedReadyQueue *)self;
    free(queue->items);
    free(queue);
}

static const ReadyQueueVTable kSortedVTable = {
    "Menor tiempo restante primero",
    sorted_enqueue,
    sorted_dequeue,
    sorted_peek,
    sorted_size,
    sorted_destroy
};

ReadyQueue *shortest_remaining_ready_queue_create(int level, Error *error)
{
    (void)level;
    SortedReadyQueue *queue = calloc(1, sizeof(*queue));
    if (queue == NULL) {
        error_set(error, ERROR_OUT_OF_MEMORY, "sin memoria para la cola ordenada");
        return NULL;
    }
    queue->items = calloc(SRTF_INITIAL_CAPACITY, sizeof(*queue->items));
    if (queue->items == NULL) {
        free(queue);
        error_set(error, ERROR_OUT_OF_MEMORY, "sin memoria para la cola ordenada");
        return NULL;
    }
    queue->capacity = SRTF_INITIAL_CAPACITY;
    queue->base.vtable = &kSortedVTable;
    return &queue->base;
}
