# Simulador de Scheduler MLFQ (Multi-Level Feedback Queue)

Simulador de planificación de procesos por **ciclos de reloj discretos** con política
**MLFQ**: colas con prioridades, quantum distinto por nivel, democión al agotar el
quantum, *priority boost* periódico, cálculo de métricas (*response*, *turnaround*,
*waiting*) y exportación a `results.csv`.

Implementado en **C11 puro** (sin dependencias externas), con arquitectura por capas
(Ports & Adapters), patrones **State**, **Strategy**, **Observer** y **Factory Method**,
y pruebas unitarias propias.

| Documento | Contenido |
|---|---|
| [`DESIGN.md`](DESIGN.md) | Decisiones de diseño, patrones, principios SOLID y arquitectura limpia (justificación) |
| [`ANALISIS.md`](ANALISIS.md) | Respuestas a las preguntas de análisis con datos medidos |
| [`COMPILACION.md`](COMPILACION.md) | Guía de compilación y pruebas en macOS y Windows |
| [`docs/diagramas/`](docs/diagramas/) | Diagramas del diseño en formato draw.io (7 páginas) y sus referencias |
| [`docs/resultados/comparativa.md`](docs/resultados/comparativa.md) | Tabla comparativa generada automáticamente |

---

## 1. Compilar y ejecutar

**Demostración guiada** (la vía más rápida para ver todo funcionando):

```bash
./scripts/demo.sh            # macOS / Linux / WSL / MSYS2  (-p pausa entre pasos)
scripts\demo.bat             # Windows: cmd.exe o doble clic
```

Compila, corre las pruebas y recorre en diez pasos el escenario del enunciado, la línea de
tiempo, la traza, la validación de entradas, las tres políticas, las extensiones sin tocar el
núcleo y la inanición con y sin *priority boost*, mostrando de cada uno solo lo esencial.

**Objetivos de `make`:**

```bash
make              # compila build/scheduler (-std=c11 -Wall -Wextra -Werror -pedantic)
make run          # escenario del enunciado -> consola + results.csv
make trace        # igual, mostrando la traza ciclo a ciclo
make test         # ejecuta las pruebas unitarias
make experiments  # corre todas las variantes del análisis y regenera la comparativa
make clean
```

Requisitos: un compilador C11 (`cc`/`clang`/`gcc`) y `make`. Nada más.
Instrucciones detalladas por sistema operativo (macOS, Windows con MSYS2/WSL/MSVC, Linux),
guía de pruebas y solución de problemas: **[`COMPILACION.md`](COMPILACION.md)**.

## 2. Opciones de línea de comandos

```
Entrada de procesos:
  --input <archivo.csv>   Carga procesos desde un CSV (pid,arrival,burst)
  --random <n>            Genera n procesos sintéticos
  --seed <n>              Semilla para --random (por defecto 42)
  (sin opciones)          Usa el escenario del enunciado P1..P4

Política de planificación:
  --policy mlfq|rr|fcfs   Política a simular (por defecto mlfq)
  --quantums 2,4,8        Quantum por nivel; define el número de niveles
  --boost <s>             Priority boost cada s ciclos (0 = desactivado)
  --queue fifo|srtf       Disciplina interna de cada cola
  --no-preempt            No expropiar cuando llega un proceso de mayor prioridad
  --demote-to-lowest      Regla de democión alternativa: baja al último nivel

Salidas:
  --output <archivo.csv>  Ruta del CSV de resultados (por defecto results.csv)
  --no-csv                No escribir el CSV
  --trace                 Traza de eventos ciclo a ciclo
  --no-gantt              No imprimir la línea de tiempo
  --label <texto>         Etiqueta del experimento
  --help
```

Ejemplos:

```bash
./build/scheduler                                     # escenario del enunciado
./build/scheduler --input examples/procesos_enunciado.csv --trace
./build/scheduler --boost 0 --label sin-boost         # ¿qué pasa sin boost?
./build/scheduler --quantums 2,4,8,16                 # cuarto nivel, sin recompilar el núcleo
./build/scheduler --policy fcfs                       # misma simulación, otra política
./build/scheduler --input examples/procesos_invalido.csv   # demuestra la validación
```

## 3. Especificación implementada

- **Q0** prioridad alta, quantum 2 · **Q1** media, quantum 4 · **Q2** baja, quantum 8
  (configurable con `--quantums`; se admiten hasta 8 niveles).
- Se ejecuta **siempre** la cola de mayor prioridad no vacía; dentro de cada cola,
  **Round Robin**.
- **Democión**: si un proceso consume todo su quantum y le queda ráfaga, baja un nivel.
- **Sin democión** si termina antes de agotar el quantum.
- **Priority boost**: cada `S` ciclos todos los procesos vuelven a Q0 (por defecto S=20).
- **Expropiación**: si llega un proceso a una cola de mayor prioridad que la del proceso
  en CPU, éste vuelve al final de **su mismo nivel** (no se degrada: no agotó su quantum).
- Simulación **por ciclos discretos**. El ciclo `t` representa el intervalo `[t, t+1)`;
  un proceso que ejecuta su último ciclo en `t` tiene `finish_time = t + 1`.

Métricas: `response = first_response_time − arrival_time`,
`turnaround = finish_time − arrival_time`, `waiting = turnaround − burst_time`.

## 4. Formatos de archivo

**Entrada** (`examples/procesos_enunciado.csv`): admite cabecera, líneas vacías y
comentarios con `#`.

```csv
pid,arrival_time,burst_time
1,0,8
2,1,4
```

**Salida** (`results.csv`): las columnas exigidas por el enunciado, más una fila `AVG`
con los promedios.

```csv
PID,Arrival,Burst,Start,Finish,Response,Turnaround,Waiting
P1,0,8,0,23,0,23,15
P2,1,4,2,14,1,13,9
P3,2,9,4,26,2,24,15
P4,3,5,6,21,3,18,13
AVG,,,,,1.50,19.50,13.00
```

## 5. Resultado del escenario del enunciado

`P1(0,8) P2(1,4) P3(2,9) P4(3,5)`, MLFQ 2/4/8, boost cada 20 ciclos:

```
=== Metricas por proceso ===
PID    Arrival  Burst  Start  Finish  Response  Turnaround  Waiting QFinal Demociones
P1           0      8      0      23         0          23       15      0          2
P2           1      4      2      14         1          13        9      1          1
P3           2      9      4      26         2          24       15      1          3
P4           3      5      6      21         3          18       13      0          1

Response promedio 1.50 · Turnaround promedio 19.50 · Waiting promedio 13.00
26 ciclos, 0 ociosos, uso de CPU 100%, 10 cambios de contexto

=== Linea de tiempo (el digito indica el nivel de cola en que ejecuto) ===
t      01234567890123456789012345
P1     00......1111.........00...
P2     ..00........11............
P3     ....00........1111.....001
P4     ......00..........110.....
IDLE   ..........................
```

En la línea de tiempo se lee el comportamiento completo de MLFQ: los cuatro procesos
estrenan en Q0 (`0`), van cayendo a Q1 (`1`) al agotar su quantum, P1 y P3 llegan a Q2, y
en el ciclo 20 el *priority boost* devuelve a todos a Q0 (P4 y P3 vuelven a ejecutar con
`0`).

## 6. Estructura del proyecto

```
include/                     Interfaces públicas (una carpeta por capa)
  domain/                      entidades y reglas de negocio
    process.h                  entidad Process (struct OPACO)
    process_state.h            patrón State: NEW/READY/RUNNING/WAITING/TERMINATED
    process_table.h            colección propietaria de procesos
    ready_queue.h              ABSTRACCIÓN de cola + 2 implementaciones
    scheduling_policy.h        patrón Strategy: contrato de planificación
    metrics.h                  cálculo de métricas
    event_bus.h                patrón Observer (sujeto)
    simulation_event.h         vocabulario de eventos
    error.h                    reporte de errores
    policies/mlfq_policy.h     MLFQ configurable (quantums, boost, regla de democión)
    policies/simple_policies.h FCFS y Round Robin
  application/
    simulation_engine.h        reloj discreto + ciclo de vida
    run_simulation_use_case.h  caso de uso: cargar -> simular -> medir -> publicar
    ports/process_source.h     PUERTO de entrada
    ports/results_writer.h     PUERTO de salida
  infrastructure/
    process_sources.h          adaptadores: CSV, aleatorio, escenario integrado
    results_writers.h          adaptadores: CSV, consola
    observers.h                adaptadores: traza, Gantt, estadísticas, espera máxima
  cli/cli_options.h            parseo de argumentos

src/                         Implementaciones (misma jerarquía)
  domain/process_internal.h    vista PRIVADA del paquete domain sobre Process
  cli/main.c                   COMPOSITION ROOT: aquí se decide e inyecta todo

tests/                       Pruebas unitarias (6 suites, 215 verificaciones)
examples/                    CSV de ejemplo (incluye uno inválido a propósito)
scripts/experimentos.sh      Genera la tabla comparativa del análisis
docs/resultados/             Salidas de cada experimento (CSV + consola)
```

## 7. Arquitectura: capas y regla de dependencia

```
        ┌──────────────────────────────────────────────────────────┐
        │  cli            main.c (composition root), cli_options   │  ← detalles
        ├──────────────────────────────────────────────────────────┤
        │  infrastructure  CSV entrada · CSV salida · consola ·    │
        │                  observadores (traza, Gantt, stats)      │
        ├──────────────────────────────────────────────────────────┤
        │  application     run_simulation_use_case                 │
        │                  simulation_engine                       │
        │                  PUERTOS: ProcessSource, ResultsWriter   │
        ├──────────────────────────────────────────────────────────┤
        │  domain          Process + ProcessState (State)          │
        │                  ReadyQueue (abstracción) · Metrics      │
        │                  SchedulingPolicy (Strategy) · EventBus  │  ← núcleo
        └──────────────────────────────────────────────────────────┘
                    Las dependencias apuntan SIEMPRE hacia abajo.
```

Verificable en un comando:

```bash
grep -rn '#include "infrastructure\|#include "application\|#include "cli' src/domain src/policies
# (sin resultados: el dominio no conoce a las capas externas)
```

El dominio tampoco abre archivos ni escribe en consola: no usa `FILE`, `fopen` ni
`printf` (solo `snprintf` para formatear mensajes de error **en memoria**).

## 8. Pruebas

```bash
make test                          # las seis suites
./build/run_tests mlfq             # una sola suite
./build/run_tests --list           # nombres disponibles
make sanitize                      # pruebas con AddressSanitizer + UBSan
```

Seis suites: ciclo de vida del proceso (State), colas de listos, cálculo de métricas,
política MLFQ (democión, boost, prioridad, regla alternativa), motor end-to-end
(escenario del enunciado con valores calculados a mano) y validación de entradas.
**215 verificaciones, 0 fallos.** Sin fugas de memoria (`leaks --atExit`: 0 leaks).
