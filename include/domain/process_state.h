/*
 * process_state.h - Patron STATE para el ciclo de vida de un proceso.
 *
 * Cada estado (NEW, READY, RUNNING, WAITING, TERMINATED) es un objeto
 * inmutable y compartido (singleton) que define QUE transiciones son legales
 * desde el. El proceso solo delega: no contiene un switch gigante ni cadenas
 * de `if (estado == X && estado_destino == Y)`.
 *
 * Beneficio concreto: una transicion invalida (p. ej. despachar un proceso
 * TERMINATED) se rechaza en un unico lugar y con un mensaje explicito, en vez
 * de corromper las metricas de forma silenciosa.
 */
#ifndef DOMAIN_PROCESS_STATE_H
#define DOMAIN_PROCESS_STATE_H

#include <stdbool.h>

#include "domain/error.h"

typedef struct Process Process;
typedef struct ProcessState ProcessState;

typedef enum {
    PROCESS_STATE_NEW = 0,
    PROCESS_STATE_READY,
    PROCESS_STATE_RUNNING,
    PROCESS_STATE_WAITING,
    PROCESS_STATE_TERMINATED
} ProcessStateId;

/*
 * Contrato de un estado. Cada puntero implementa la transicion valida y, si
 * no lo es para ese estado, devuelve false describiendo el motivo.
 */
struct ProcessState {
    ProcessStateId id;
    const char *name;
    bool (*admit)(Process *process, int cycle, Error *error);
    bool (*dispatch)(Process *process, int cycle, Error *error);
    bool (*preempt)(Process *process, int cycle, Error *error);
    bool (*block)(Process *process, int cycle, Error *error);
    bool (*resume)(Process *process, int cycle, Error *error);
    bool (*terminate)(Process *process, int cycle, Error *error);
    bool (*consume_cycle)(Process *process, Error *error);
};

const ProcessState *process_state_new(void);
const ProcessState *process_state_ready(void);
const ProcessState *process_state_running(void);
const ProcessState *process_state_waiting(void);
const ProcessState *process_state_terminated(void);

const char *process_state_id_name(ProcessStateId id);

#endif /* DOMAIN_PROCESS_STATE_H */
