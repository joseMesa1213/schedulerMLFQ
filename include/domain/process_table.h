/*
 * process_table.h - Coleccion propietaria de los procesos de la simulacion.
 *
 * Responsabilidad unica: guardar los procesos, garantizar que no existan PID
 * duplicados y liberar la memoria. No sabe nada de colas, quantums ni CSV.
 */
#ifndef DOMAIN_PROCESS_TABLE_H
#define DOMAIN_PROCESS_TABLE_H

#include <stddef.h>

#include "domain/error.h"
#include "domain/process.h"

typedef struct ProcessTable ProcessTable;

ProcessTable *process_table_create(Error *error);
void process_table_destroy(ProcessTable *table);

/* Toma posesion del proceso: la tabla lo liberara al destruirse.
 * Si falla (PID duplicado o sin memoria), el proceso NO se adopta. */
bool process_table_add(ProcessTable *table, Process *process, Error *error);

/* Atajo de creacion + adopcion, para las fuentes de procesos. */
bool process_table_emplace(ProcessTable *table, int pid, int arrival_time,
                          int burst_time, Error *error);

size_t process_table_size(const ProcessTable *table);
Process *process_table_at(const ProcessTable *table, size_t index);
const Process *process_table_find(const ProcessTable *table, int pid);
size_t process_table_count_terminated(const ProcessTable *table);

/* Ordena por arrival_time (y PID como desempate) para que el motor pueda
 * admitir llegadas en orden determinista. */
void process_table_sort_by_arrival(ProcessTable *table);

#endif /* DOMAIN_PROCESS_TABLE_H */
