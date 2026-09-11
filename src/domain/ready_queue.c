#include "domain/ready_queue.h"

const char *ready_queue_name(const ReadyQueue *queue)
{
    return queue == NULL ? "(nula)" : queue->vtable->name;
}

bool ready_queue_enqueue(ReadyQueue *queue, Process *process, Error *error)
{
    if (queue == NULL || process == NULL) {
        return error_set(error, ERROR_INVALID_ARGUMENT, "cola o proceso nulo al encolar");
    }
    return queue->vtable->enqueue(queue, process, error);
}

Process *ready_queue_dequeue(ReadyQueue *queue)
{
    return queue == NULL ? NULL : queue->vtable->dequeue(queue);
}

const Process *ready_queue_peek(const ReadyQueue *queue)
{
    return queue == NULL ? NULL : queue->vtable->peek(queue);
}

size_t ready_queue_size(const ReadyQueue *queue)
{
    return queue == NULL ? 0u : queue->vtable->size(queue);
}

bool ready_queue_is_empty(const ReadyQueue *queue)
{
    return ready_queue_size(queue) == 0u;
}

void ready_queue_destroy(ReadyQueue *queue)
{
    if (queue != NULL) {
        queue->vtable->destroy(queue);
    }
}
