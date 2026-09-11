#include "domain/process_state.h"

#include "domain/process_internal.h"

/*
 * Helper unico para rechazar transiciones ilegales (DRY): todos los estados
 * comparten el mismo formato de mensaje en lugar de repetirlo por caso.
 */
static bool reject(const Process *process, const char *operation, Error *error)
{
    return error_set(error, ERROR_INVALID_STATE_TRANSITION,
                     "transicion invalida: no se puede '%s' un proceso P%d en estado %s",
                     operation, process->pid, process->state->name);
}

static bool reject_admit(Process *p, int cycle, Error *e)    { (void)cycle; return reject(p, "admitir", e); }
static bool reject_dispatch(Process *p, int cycle, Error *e) { (void)cycle; return reject(p, "despachar", e); }
static bool reject_preempt(Process *p, int cycle, Error *e)  { (void)cycle; return reject(p, "expropiar", e); }
static bool reject_block(Process *p, int cycle, Error *e)    { (void)cycle; return reject(p, "bloquear", e); }
static bool reject_resume(Process *p, int cycle, Error *e)   { (void)cycle; return reject(p, "reanudar", e); }
static bool reject_terminate(Process *p, int cycle, Error *e){ (void)cycle; return reject(p, "terminar", e); }
static bool reject_consume(Process *p, Error *e)             { return reject(p, "consumir ciclo de CPU", e); }

/* --- NEW ------------------------------------------------------------- */

static bool new_admit(Process *process, int cycle, Error *error)
{
    (void)cycle;
    (void)error;
    process->state = process_state_ready();
    return true;
}

/* --- READY ----------------------------------------------------------- */

static bool ready_dispatch(Process *process, int cycle, Error *error)
{
    (void)error;
    /* start_time y first_response_time se fijan una sola vez: es la primera
     * vez que el proceso obtiene CPU. Concentrarlo aqui evita que el motor de
     * simulacion tenga que recordar "si es la primera vez, entonces...". */
    if (process->start_time == PROCESS_TIME_UNSET) {
        process->start_time = cycle;
        process->first_response_time = cycle;
    }
    process->dispatch_count++;
    process->state = process_state_running();
    return true;
}

/* --- RUNNING --------------------------------------------------------- */

static bool running_preempt(Process *process, int cycle, Error *error)
{
    (void)cycle;
    (void)error;
    process->state = process_state_ready();
    return true;
}

static bool running_block(Process *process, int cycle, Error *error)
{
    (void)cycle;
    (void)error;
    process->state = process_state_waiting();
    return true;
}

static bool running_terminate(Process *process, int cycle, Error *error)
{
    if (process->remaining_time > 0) {
        return error_set(error, ERROR_INVALID_STATE_TRANSITION,
                         "P%d no puede terminar: le quedan %d ciclos de rafaga",
                         process->pid, process->remaining_time);
    }
    process->finish_time = cycle;
    process->state = process_state_terminated();
    return true;
}

static bool running_consume_cycle(Process *process, Error *error)
{
    if (process->remaining_time <= 0) {
        return error_set(error, ERROR_INVALID_STATE_TRANSITION,
                         "P%d ya no tiene rafaga pendiente", process->pid);
    }
    process->remaining_time--;
    return true;
}

/* --- WAITING --------------------------------------------------------- */

static bool waiting_resume(Process *process, int cycle, Error *error)
{
    (void)cycle;
    (void)error;
    process->state = process_state_ready();
    return true;
}

/* --- Tablas de estado (singletons inmutables) ------------------------ */

static const ProcessState kStateNew = {
    PROCESS_STATE_NEW, "NEW",
    new_admit, reject_dispatch, reject_preempt, reject_block, reject_resume,
    reject_terminate, reject_consume
};

static const ProcessState kStateReady = {
    PROCESS_STATE_READY, "READY",
    reject_admit, ready_dispatch, reject_preempt, reject_block, reject_resume,
    reject_terminate, reject_consume
};

static const ProcessState kStateRunning = {
    PROCESS_STATE_RUNNING, "RUNNING",
    reject_admit, reject_dispatch, running_preempt, running_block, reject_resume,
    running_terminate, running_consume_cycle
};

static const ProcessState kStateWaiting = {
    PROCESS_STATE_WAITING, "WAITING",
    reject_admit, reject_dispatch, reject_preempt, reject_block, waiting_resume,
    reject_terminate, reject_consume
};

static const ProcessState kStateTerminated = {
    PROCESS_STATE_TERMINATED, "TERMINATED",
    reject_admit, reject_dispatch, reject_preempt, reject_block, reject_resume,
    reject_terminate, reject_consume
};

const ProcessState *process_state_new(void)        { return &kStateNew; }
const ProcessState *process_state_ready(void)      { return &kStateReady; }
const ProcessState *process_state_running(void)    { return &kStateRunning; }
const ProcessState *process_state_waiting(void)    { return &kStateWaiting; }
const ProcessState *process_state_terminated(void) { return &kStateTerminated; }

const char *process_state_id_name(ProcessStateId id)
{
    switch (id) {
    case PROCESS_STATE_NEW:        return kStateNew.name;
    case PROCESS_STATE_READY:      return kStateReady.name;
    case PROCESS_STATE_RUNNING:    return kStateRunning.name;
    case PROCESS_STATE_WAITING:    return kStateWaiting.name;
    case PROCESS_STATE_TERMINATED: return kStateTerminated.name;
    }
    return "UNKNOWN";
}
