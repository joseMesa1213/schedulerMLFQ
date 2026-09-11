/*
 * process.h - Entidad del dominio: un proceso simulado.
 *
 * El struct es OPACO a proposito (encapsulamiento): ningun modulo externo
 * puede escribir `process->remaining_time = 0`. El estado interno solo cambia
 * a traves de operaciones del ciclo de vida, que a su vez delegan en el
 * patron State.
 */
#ifndef DOMAIN_PROCESS_H
#define DOMAIN_PROCESS_H

#include <stdbool.h>

#include "domain/error.h"
#include "domain/process_state.h"

/* Valor centinela para instantes de tiempo aun no ocurridos. */
#define PROCESS_TIME_UNSET (-1)

typedef struct Process Process;

/* Fabrica de la entidad: valida sus invariantes (burst > 0, arrival >= 0). */
Process *process_create(int pid, int arrival_time, int burst_time, Error *error);
void process_destroy(Process *process);

/* --- Consultas (solo lectura) --- */
int process_pid(const Process *process);
int process_arrival_time(const Process *process);
int process_burst_time(const Process *process);
int process_remaining_time(const Process *process);
int process_start_time(const Process *process);
int process_finish_time(const Process *process);
int process_first_response_time(const Process *process);
int process_current_queue(const Process *process);
int process_dispatch_count(const Process *process);
int process_demotion_count(const Process *process);
bool process_has_pending_work(const Process *process);
bool process_is_terminated(const Process *process);
ProcessStateId process_state_id(const Process *process);
const char *process_state_name(const Process *process);

/* --- Transiciones del ciclo de vida (patron State) --- */
bool process_admit(Process *process, int cycle, Error *error);
bool process_dispatch(Process *process, int cycle, Error *error);
bool process_preempt(Process *process, int cycle, Error *error);
bool process_block(Process *process, int cycle, Error *error);
bool process_resume(Process *process, int cycle, Error *error);
bool process_terminate(Process *process, int cycle, Error *error);

/* Ejecuta exactamente un ciclo de reloj de CPU (solo valido en RUNNING). */
bool process_consume_cycle(Process *process, Error *error);

/* Reubica el proceso en un nivel de cola. Lo usan las politicas de
 * planificacion; valida que el nivel no sea negativo. */
bool process_move_to_queue(Process *process, int queue_level, Error *error);

#endif /* DOMAIN_PROCESS_H */
