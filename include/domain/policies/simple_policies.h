/*
 * simple_policies.h - Politicas de referencia (FCFS y Round Robin).
 *
 * No son adornos: son la demostracion de que el patron Strategy funciona. El
 * motor de simulacion, las metricas, los observadores y el exportador CSV se
 * reutilizan tal cual con ellas, y sirven de linea base para comparar MLFQ en
 * el informe de analisis.
 */
#ifndef DOMAIN_POLICIES_SIMPLE_POLICIES_H
#define DOMAIN_POLICIES_SIMPLE_POLICIES_H

#include "domain/event_bus.h"
#include "domain/ready_queue.h"
#include "domain/scheduling_policy.h"

/* FCFS: no expropiativo, una sola cola, el primero en llegar termina primero. */
SchedulingPolicy *fcfs_policy_create(ReadyQueueFactory queue_factory, EventBus *bus, Error *error);

/* Round Robin: una sola cola FIFO y un quantum fijo, sin niveles ni democion. */
SchedulingPolicy *round_robin_policy_create(int quantum, ReadyQueueFactory queue_factory,
                                            EventBus *bus, Error *error);

#endif /* DOMAIN_POLICIES_SIMPLE_POLICIES_H */
