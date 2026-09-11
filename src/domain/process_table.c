#include "domain/process_table.h"

#include <stdlib.h>

#define PROCESS_TABLE_INITIAL_CAPACITY 8

struct ProcessTable {
    Process **items;
    size_t count;
    size_t capacity;
};

ProcessTable *process_table_create(Error *error)
{
    ProcessTable *table = calloc(1, sizeof(*table));
    if (table == NULL) {
        error_set(error, ERROR_OUT_OF_MEMORY, "sin memoria para la tabla de procesos");
        return NULL;
    }
    table->items = calloc(PROCESS_TABLE_INITIAL_CAPACITY, sizeof(*table->items));
    if (table->items == NULL) {
        free(table);
        error_set(error, ERROR_OUT_OF_MEMORY, "sin memoria para la tabla de procesos");
        return NULL;
    }
    table->capacity = PROCESS_TABLE_INITIAL_CAPACITY;
    return table;
}

void process_table_destroy(ProcessTable *table)
{
    if (table == NULL) {
        return;
    }
    for (size_t i = 0; i < table->count; i++) {
        process_destroy(table->items[i]);
    }
    free(table->items);
    free(table);
}

static bool ensure_capacity(ProcessTable *table, Error *error)
{
    if (table->count < table->capacity) {
        return true;
    }
    size_t new_capacity = table->capacity * 2;
    Process **grown = realloc(table->items, new_capacity * sizeof(*grown));
    if (grown == NULL) {
        return error_set(error, ERROR_OUT_OF_MEMORY,
                         "no se pudo ampliar la tabla de procesos a %zu", new_capacity);
    }
    table->items = grown;
    table->capacity = new_capacity;
    return true;
}

bool process_table_add(ProcessTable *table, Process *process, Error *error)
{
    if (table == NULL || process == NULL) {
        return error_set(error, ERROR_INVALID_ARGUMENT, "tabla o proceso nulo");
    }
    if (process_table_find(table, process_pid(process)) != NULL) {
        return error_set(error, ERROR_DUPLICATED_PID,
                         "PID duplicado: P%d ya existe en la tabla", process_pid(process));
    }
    if (!ensure_capacity(table, error)) {
        return false;
    }
    table->items[table->count++] = process;
    return true;
}

bool process_table_emplace(ProcessTable *table, int pid, int arrival_time,
                          int burst_time, Error *error)
{
    Process *process = process_create(pid, arrival_time, burst_time, error);
    if (process == NULL) {
        return false;
    }
    if (!process_table_add(table, process, error)) {
        process_destroy(process);
        return false;
    }
    return true;
}

size_t process_table_size(const ProcessTable *table)
{
    return table == NULL ? 0u : table->count;
}

Process *process_table_at(const ProcessTable *table, size_t index)
{
    if (table == NULL || index >= table->count) {
        return NULL;
    }
    return table->items[index];
}

const Process *process_table_find(const ProcessTable *table, int pid)
{
    for (size_t i = 0; i < table->count; i++) {
        if (process_pid(table->items[i]) == pid) {
            return table->items[i];
        }
    }
    return NULL;
}

size_t process_table_count_terminated(const ProcessTable *table)
{
    size_t terminated = 0;
    for (size_t i = 0; i < table->count; i++) {
        if (process_is_terminated(table->items[i])) {
            terminated++;
        }
    }
    return terminated;
}

static int compare_by_arrival(const void *left, const void *right)
{
    const Process *a = *(const Process *const *)left;
    const Process *b = *(const Process *const *)right;
    if (process_arrival_time(a) != process_arrival_time(b)) {
        return process_arrival_time(a) - process_arrival_time(b);
    }
    return process_pid(a) - process_pid(b);
}

void process_table_sort_by_arrival(ProcessTable *table)
{
    if (table == NULL || table->count < 2) {
        return;
    }
    qsort(table->items, table->count, sizeof(*table->items), compare_by_arrival);
}
