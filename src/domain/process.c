#include "domain/process_internal.h"

#include <stdlib.h>

Process *process_create(int pid, int arrival_time, int burst_time, Error *error)
{
    /* Las invariantes se validan en la construccion: un Process que existe es
     * un Process valido, por lo que el resto del sistema no vuelve a
     * comprobarlas. */
    if (pid < 0) {
        error_set(error, ERROR_INVALID_ARGUMENT, "PID invalido (%d): debe ser >= 0", pid);
        return NULL;
    }
    if (arrival_time < 0) {
        error_set(error, ERROR_INVALID_ARGUMENT,
                  "P%d: arrival_time invalido (%d): debe ser >= 0", pid, arrival_time);
        return NULL;
    }
    if (burst_time <= 0) {
        error_set(error, ERROR_INVALID_ARGUMENT,
                  "P%d: burst_time invalido (%d): debe ser > 0", pid, burst_time);
        return NULL;
    }

    Process *process = calloc(1, sizeof(*process));
    if (process == NULL) {
        error_set(error, ERROR_OUT_OF_MEMORY, "sin memoria para el proceso P%d", pid);
        return NULL;
    }

    process->pid = pid;
    process->arrival_time = arrival_time;
    process->burst_time = burst_time;
    process->remaining_time = burst_time;
    process->start_time = PROCESS_TIME_UNSET;
    process->finish_time = PROCESS_TIME_UNSET;
    process->first_response_time = PROCESS_TIME_UNSET;
    process->current_queue = 0;
    process->state = process_state_new();
    return process;
}

void process_destroy(Process *process)
{
    free(process);
}

int process_pid(const Process *process)                 { return process->pid; }
int process_arrival_time(const Process *process)         { return process->arrival_time; }
int process_burst_time(const Process *process)           { return process->burst_time; }
int process_remaining_time(const Process *process)       { return process->remaining_time; }
int process_start_time(const Process *process)           { return process->start_time; }
int process_finish_time(const Process *process)          { return process->finish_time; }
int process_first_response_time(const Process *process)  { return process->first_response_time; }
int process_current_queue(const Process *process)        { return process->current_queue; }
int process_dispatch_count(const Process *process)       { return process->dispatch_count; }
int process_demotion_count(const Process *process)       { return process->demotion_count; }

bool process_has_pending_work(const Process *process)
{
    return process->remaining_time > 0;
}

bool process_is_terminated(const Process *process)
{
    return process->state->id == PROCESS_STATE_TERMINATED;
}

ProcessStateId process_state_id(const Process *process)
{
    return process->state->id;
}

const char *process_state_name(const Process *process)
{
    return process->state->name;
}

/*
 * Las transiciones son delegacion pura al objeto-estado actual: el "if" por
 * estado vive en la tabla de process_state.c, no aqui.
 */
bool process_admit(Process *process, int cycle, Error *error)
{
    return process->state->admit(process, cycle, error);
}

bool process_dispatch(Process *process, int cycle, Error *error)
{
    return process->state->dispatch(process, cycle, error);
}

bool process_preempt(Process *process, int cycle, Error *error)
{
    return process->state->preempt(process, cycle, error);
}

bool process_block(Process *process, int cycle, Error *error)
{
    return process->state->block(process, cycle, error);
}

bool process_resume(Process *process, int cycle, Error *error)
{
    return process->state->resume(process, cycle, error);
}

bool process_terminate(Process *process, int cycle, Error *error)
{
    return process->state->terminate(process, cycle, error);
}

bool process_consume_cycle(Process *process, Error *error)
{
    return process->state->consume_cycle(process, error);
}

bool process_move_to_queue(Process *process, int queue_level, Error *error)
{
    if (queue_level < 0) {
        return error_set(error, ERROR_INVALID_ARGUMENT,
                         "P%d: nivel de cola invalido (%d)", process->pid, queue_level);
    }
    if (queue_level > process->current_queue) {
        process->demotion_count++;
    }
    process->current_queue = queue_level;
    return true;
}
