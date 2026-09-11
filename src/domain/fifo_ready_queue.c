/*
 * fifo_ready_queue.c - Cola FIFO (la disciplina Round Robin de cada nivel).
 *
 * Se implementa como buffer circular que crece por duplicacion. Motivo de la
 * eleccion: encolar y desencolar son O(1) y son las dos operaciones que el
 * planificador ejecuta en cada ciclo de reloj; una lista enlazada daria la
 * misma complejidad pero con una asignacion de memoria por cada movimiento
 * entre colas, que en MLFQ ocurre constantemente.
 */
#include "domain/ready_queue.h"

#include <stdlib.h>

#define FIFO_INITIAL_CAPACITY 8

typedef struct {
    ReadyQueue base;
    Process **items;
    size_t capacity;
    size_t count;
    size_t head;
} FifoReadyQueue;

static bool fifo_grow(FifoReadyQueue *queue, Error *error)
{
    size_t new_capacity = queue->capacity * 2;
    Process **items = calloc(new_capacity, sizeof(*items));
    if (items == NULL) {
        return error_set(error, ERROR_OUT_OF_MEMORY,
                         "no se pudo ampliar la cola FIFO a %zu elementos", new_capacity);
    }
    /* Se re-linealiza el buffer circular al copiar. */
    for (size_t i = 0; i < queue->count; i++) {
        items[i] = queue->items[(queue->head + i) % queue->capacity];
    }
    free(queue->items);
    queue->items = items;
    queue->capacity = new_capacity;
    queue->head = 0;
    return true;
}

static bool fifo_enqueue(ReadyQueue *self, Process *process, Error *error)
{
    FifoReadyQueue *queue = (FifoReadyQueue *)self;
    if (queue->count == queue->capacity && !fifo_grow(queue, error)) {
        return false;
    }
    queue->items[(queue->head + queue->count) % queue->capacity] = process;
    queue->count++;
    return true;
}

static Process *fifo_dequeue(ReadyQueue *self)
{
    FifoReadyQueue *queue = (FifoReadyQueue *)self;
    if (queue->count == 0) {
        return NULL;
    }
    Process *process = queue->items[queue->head];
    queue->head = (queue->head + 1) % queue->capacity;
    queue->count--;
    return process;
}

static const Process *fifo_peek(const ReadyQueue *self)
{
    const FifoReadyQueue *queue = (const FifoReadyQueue *)self;
    return queue->count == 0 ? NULL : queue->items[queue->head];
}

static size_t fifo_size(const ReadyQueue *self)
{
    return ((const FifoReadyQueue *)self)->count;
}

static void fifo_destroy(ReadyQueue *self)
{
    FifoReadyQueue *queue = (FifoReadyQueue *)self;
    free(queue->items);
    free(queue);
}

static const ReadyQueueVTable kFifoVTable = {
    "FIFO (Round Robin)",
    fifo_enqueue,
    fifo_dequeue,
    fifo_peek,
    fifo_size,
    fifo_destroy
};

ReadyQueue *fifo_ready_queue_create(int level, Error *error)
{
    (void)level; /* la implementacion FIFO no depende del nivel */
    FifoReadyQueue *queue = calloc(1, sizeof(*queue));
    if (queue == NULL) {
        error_set(error, ERROR_OUT_OF_MEMORY, "sin memoria para la cola FIFO");
        return NULL;
    }
    queue->items = calloc(FIFO_INITIAL_CAPACITY, sizeof(*queue->items));
    if (queue->items == NULL) {
        free(queue);
        error_set(error, ERROR_OUT_OF_MEMORY, "sin memoria para la cola FIFO");
        return NULL;
    }
    queue->capacity = FIFO_INITIAL_CAPACITY;
    queue->base.vtable = &kFifoVTable;
    return &queue->base;
}
