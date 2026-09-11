/*
 * process_internal.h - Vista privada del paquete `domain` sobre Process.
 *
 * NO se publica en include/: solo process.c y process_state.c la incluyen.
 * Asi el patron State puede mutar la entidad sin exponer los campos al resto
 * del sistema (encapsulamiento a nivel de modulo).
 */
#ifndef DOMAIN_PROCESS_INTERNAL_H
#define DOMAIN_PROCESS_INTERNAL_H

#include "domain/process.h"
#include "domain/process_state.h"

struct Process {
    int pid;
    int arrival_time;
    int burst_time;
    int remaining_time;
    int start_time;
    int finish_time;
    int first_response_time;
    int current_queue;
    int dispatch_count;   /* cuantas veces tomo la CPU: mide cambios de contexto */
    int demotion_count;   /* cuantas veces fue degradado de nivel */
    const ProcessState *state;
};

#endif /* DOMAIN_PROCESS_INTERNAL_H */
