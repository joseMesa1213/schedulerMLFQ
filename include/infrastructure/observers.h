/*
 * observers.h - Observadores concretos del bus de eventos.
 *
 * Los tres consumen el MISMO flujo de eventos y hacen cosas distintas. Ninguno
 * es conocido por el motor de simulacion: se suscriben en el arranque.
 *   - trace: traza cronologica legible (util para defender el comportamiento).
 *   - gantt: linea de tiempo ASCII por proceso.
 *   - stats: contadores agregados (demociones, boosts, expropiaciones).
 */
#ifndef INFRASTRUCTURE_OBSERVERS_H
#define INFRASTRUCTURE_OBSERVERS_H

#include <stdio.h>

#include "domain/event_bus.h"
#include "domain/scheduling_policy.h"

typedef struct TraceObserver TraceObserver;
typedef struct GanttObserver GanttObserver;
typedef struct StatsObserver StatsObserver;
typedef struct WaitGapObserver WaitGapObserver;

/* `policy` es opcional: si se pasa, la traza incluye la ocupacion de las colas. */
TraceObserver *trace_observer_create(FILE *stream, const SchedulingPolicy *policy, Error *error);
SimulationObserver *trace_observer_as_observer(TraceObserver *observer);
void trace_observer_destroy(TraceObserver *observer);

GanttObserver *gantt_observer_create(Error *error);
SimulationObserver *gantt_observer_as_observer(GanttObserver *observer);
void gantt_observer_print(const GanttObserver *observer, FILE *stream);
void gantt_observer_destroy(GanttObserver *observer);

StatsObserver *stats_observer_create(Error *error);
SimulationObserver *stats_observer_as_observer(StatsObserver *observer);
void stats_observer_print(const StatsObserver *observer, FILE *stream);
int stats_observer_demotions(const StatsObserver *observer);
int stats_observer_boosts(const StatsObserver *observer);
int stats_observer_preemptions(const StatsObserver *observer);
void stats_observer_destroy(StatsObserver *observer);

/*
 * Mide la ESPERA CONTINUA MAXIMA de cada proceso: el intervalo mas largo que
 * pasa listo sin recibir CPU. Es la metrica que hace visible la inanicion, y
 * se agrego sin modificar el motor ni las metricas del dominio: solo escucha
 * los eventos que ya existian.
 */
WaitGapObserver *wait_gap_observer_create(Error *error);
SimulationObserver *wait_gap_observer_as_observer(WaitGapObserver *observer);
void wait_gap_observer_print(const WaitGapObserver *observer, FILE *stream);
int wait_gap_observer_longest_for(const WaitGapObserver *observer, int pid);
void wait_gap_observer_destroy(WaitGapObserver *observer);

#endif /* INFRASTRUCTURE_OBSERVERS_H */
