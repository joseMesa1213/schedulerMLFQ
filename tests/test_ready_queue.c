/*
 * Pruebas de la abstraccion ReadyQueue: las dos implementaciones se manejan
 * con el mismo codigo cliente (sustituibilidad / LSP).
 */
#include "domain/ready_queue.h"
#include "test_framework.h"
#include "test_suites.h"

static void test_orden_fifo(void)
{
    TEST_BEGIN("la cola FIFO conserva el orden de llegada y crece sin limite fijo");
    Error error;
    error_clear(&error);
    ReadyQueue *queue = fifo_ready_queue_create(0, &error);
    CHECK(queue != NULL, "se crea la cola FIFO");

    Process *processes[20];
    for (int i = 0; i < 20; i++) {
        processes[i] = process_create(i + 1, 0, 5, &error);
        CHECK(ready_queue_enqueue(queue, processes[i], &error), "encola");
    }
    CHECK_INT_EQ(ready_queue_size(queue), 20, "20 elementos (crecio mas alla de la capacidad inicial)");
    CHECK_INT_EQ(process_pid(ready_queue_peek(queue)), 1, "peek devuelve el primero");

    for (int i = 0; i < 20; i++) {
        Process *dequeued = ready_queue_dequeue(queue);
        CHECK_INT_EQ(process_pid(dequeued), i + 1, "sale en orden de llegada");
    }
    CHECK(ready_queue_is_empty(queue), "queda vacia");
    CHECK(ready_queue_dequeue(queue) == NULL, "desencolar vacia devuelve NULL");

    for (int i = 0; i < 20; i++) {
        process_destroy(processes[i]);
    }
    ready_queue_destroy(queue);
}

static void test_orden_por_tiempo_restante(void)
{
    TEST_BEGIN("la cola alternativa ordena por menor tiempo restante");
    Error error;
    error_clear(&error);
    ReadyQueue *queue = shortest_remaining_ready_queue_create(0, &error);

    Process *largo = process_create(1, 0, 9, &error);
    Process *corto = process_create(2, 0, 2, &error);
    Process *medio = process_create(3, 0, 5, &error);
    ready_queue_enqueue(queue, largo, &error);
    ready_queue_enqueue(queue, corto, &error);
    ready_queue_enqueue(queue, medio, &error);

    CHECK_INT_EQ(process_pid(ready_queue_dequeue(queue)), 2, "primero el de rafaga 2");
    CHECK_INT_EQ(process_pid(ready_queue_dequeue(queue)), 3, "luego el de rafaga 5");
    CHECK_INT_EQ(process_pid(ready_queue_dequeue(queue)), 1, "al final el de rafaga 9");

    process_destroy(largo);
    process_destroy(corto);
    process_destroy(medio);
    ready_queue_destroy(queue);
}

void suite_ready_queue(void)
{
    test_orden_fifo();
    test_orden_por_tiempo_restante();
}
