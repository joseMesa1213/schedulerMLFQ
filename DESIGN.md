# DESIGN.md — Decisiones de diseño y justificación

> Documento exigido por el punto 10 del enunciado. Explica **qué** se decidió, **por qué**,
> **qué alternativa se descartó** y **dónde se puede verificar en el código**.

> **Diagramas:** cada patrón y cada capa de este documento están dibujados en
> [`docs/diagramas/patrones-diseno.drawio`](docs/diagramas/patrones-diseno.drawio) (7 páginas,
> formato draw.io). La correspondencia página ↔ sección ↔ archivos está en
> [`docs/diagramas/README.md`](docs/diagramas/README.md).

Índice: [1. Resumen](#1-resumen-de-la-arquitectura) · [2. Arquitectura limpia](#2-arquitectura-limpia-capas-y-regla-de-dependencia) ·
[3. Modelo del dominio](#3-modelo-del-dominio-y-decisiones-de-simulación) · [4. Patrones](#4-patrones-de-diseño-aplicados) ·
[5. SOLID](#5-principios-solid-evidencia-en-el-código) · [6. Buenas prácticas](#6-buenas-prácticas-de-programación) ·
[7. Pruebas](#7-estrategia-de-pruebas) · [8. Límites](#8-límites-conocidos-y-extensiones-naturales)

---

## 1. Resumen de la arquitectura

La decisión estructural central es **separar el motor de simulación de la política de
planificación**:

- El **motor** (`src/application/simulation_engine.c`) es dueño del reloj discreto y del
  ciclo de vida de los procesos. No sabe cuántas colas hay, qué es un quantum ni qué es un
  *priority boost*.
- La **política** (`src/policies/mlfq_policy.c`) es dueña de las colas, del quantum, de la
  democión y del boost. No sabe qué es un estado `RUNNING` ni cómo se calcula un
  *turnaround*.

Ambos se comunican por un contrato de cuatro operaciones por ciclo
(`include/domain/scheduling_policy.h:32`):

```
1. admit(p, t)                 -> el proceso p acaba de llegar y está listo
2. select_for_cycle(t)         -> ¿quién ocupa la CPU en el ciclo t? (o NULL = ociosa)
3. [el motor ejecuta 1 ciclo del proceso elegido]
4. notify_cycle_result(p, terminó, t) -> la política contabiliza el quantum consumido
```

Todo lo demás (métricas, exportación, traza, Gantt, validación de entradas) cuelga de esa
separación sin acoplarse a ella.

**¿Por qué este reparto y no otro?** La alternativa natural en C es un único
`schedule()` de 200 líneas con tres arreglos de colas y contadores globales. Funciona, pero
entonces *cada* pregunta del enunciado ("¿y sin boost?", "¿y con un quantum de 1?", "¿y
FCFS?") obliga a editar y volver a leer ese mismo bloque. Con la separación, cada pregunta
es una **bandera de línea de comandos** y una fila de la tabla de análisis.

---

## 2. Arquitectura limpia: capas y regla de dependencia

```
        ┌──────────────────────────────────────────────────────────┐
        │  cli             main.c (composition root), cli_options  │
        ├──────────────────────────────────────────────────────────┤
        │  infrastructure  csv_process_source, random_process_...  │
        │                  csv_results_writer, console_results_... │
        │                  trace/gantt/stats/wait_gap observers    │
        ├──────────────────────────────────────────────────────────┤
        │  application     run_simulation_use_case                 │
        │                  simulation_engine                       │
        │                  ports/: ProcessSource, ResultsWriter    │
        ├──────────────────────────────────────────────────────────┤
        │  domain          Process, ProcessState, ProcessTable     │
        │                  ReadyQueue, SchedulingPolicy, Metrics   │
        │                  EventBus, SimulationEvent, Error        │
        └──────────────────────────────────────────────────────────┘
                     dependencias: SIEMPRE hacia abajo
```

**Diagrama:** página 1 de `docs/diagramas/patrones-diseno.drawio`.

**Regla de dependencia.** Ninguna capa interna incluye una externa:

```bash
grep -rn '#include "infrastructure\|#include "application\|#include "cli' src/domain src/policies
# → sin resultados
```

**Independencia de tecnología.** El dominio no abre archivos ni imprime: no aparece
`FILE`, `fopen` ni `printf` en `src/domain/` ni en `src/policies/` (solo `snprintf`, que
formatea cadenas **en memoria** para los mensajes de error). El formato CSV existe en
exactamente dos archivos, ambos de infraestructura: `csv_process_source.c` (entrada) y
`csv_results_writer.c` (salida).

**Puertos y adaptadores.** Las dos fronteras con el mundo exterior son interfaces
declaradas por la capa de aplicación e implementadas por la de infraestructura:

| Puerto (aplicación) | Adaptadores (infraestructura) |
|---|---|
| `ProcessSource` | `csv_process_source`, `random_process_source`, `builtin_scenario_process_source` |
| `ResultsWriter` | `csv_results_writer`, `console_results_writer` |

**Raíz de composición.** `src/cli/main.c` es el **único** archivo que menciona
implementaciones concretas y las inyecta (`build_process_source`, `build_policy`,
`subscribe_observers`, `build_writers`). Ninguna capa interna construye una dependencia
concreta. Ese archivo es también el único responsable de liberar todo lo que creó
(`composition_release`, `src/cli/main.c:35`), lo que en C importa tanto como el diseño:
`leaks --atExit -- ./build/scheduler` reporta **0 fugas**.

### Cómo se modela una "interfaz" en C

C no tiene clases ni herencia, así que cada interfaz es una **tabla de punteros a función
(vtable)** y la implementación concreta embebe la interfaz como **primer campo** de su
struct:

```c
/* interfaz */                       /* implementación */
struct ReadyQueue {                  typedef struct {
    const ReadyQueueVTable *vtable;      ReadyQueue base;   /* primer campo */
};                                       Process **items; size_t head, count;
                                     } FifoReadyQueue;
```

Como `base` es el primer campo, `(FifoReadyQueue *)self` y `&queue->base` son conversiones
válidas y sin coste. El cliente solo llama a las funciones envoltorio
(`ready_queue_enqueue`, …), que además centralizan la validación de punteros nulos.
Es el equivalente exacto de una interfaz con despacho dinámico, y es el mismo mecanismo que
usa el propio kernel de Linux para sus `file_operations`.

---

## 3. Modelo del dominio y decisiones de simulación

### 3.1 `Process` es un tipo opaco (encapsulamiento real)

`struct Process` **no está en el header público**: solo se declara `typedef struct Process
Process;` en `include/domain/process.h`, y los campos viven en
`src/domain/process_internal.h`, que únicamente incluyen `process.c` y `process_state.c`.

Consecuencia: es **imposible** escribir `process->remaining_time = 0` desde el planificador,
el exportador o las pruebas. El compilador lo impide. Todo cambio de estado pasa por una
operación con nombre y validación. Ésta es la respuesta concreta al criterio de
"encapsulamiento: el estado interno de un proceso no debe ser modificado libremente desde
cualquier parte del código".

El precio pagado es que el acceso es por *getters* y que `process_state.c` necesita una
vista privada del struct. Se aceptó porque el error que previene (una métrica corrompida
por una escritura suelta) es exactamente el tipo de fallo silencioso que el enunciado penaliza.

### 3.2 Convención temporal (decidida y documentada)

El ciclo `t` representa el intervalo `[t, t+1)`. Por lo tanto:

- `start_time` = ciclo del primer despacho.
- `first_response_time` = igual al primer despacho (el proceso "responde" cuando obtiene CPU).
- `finish_time` = `t + 1` si su último ciclo de ejecución fue `t`
  (`simulation_engine.c`, `process_terminate(selected, cycle + 1, …)`).

Sin fijar esta convención, `turnaround` y `waiting` quedan desplazados en 1 ciclo y la
discusión de resultados se vuelve imposible. Verificación: con la CPU sin ociosos, la suma
de `burst` debe ser igual al total de ciclos simulados (26 en el escenario del enunciado), y
es una aserción de la prueba end-to-end.

### 3.3 Orden de las decisiones dentro de un ciclo

`mlfq_select_for_cycle` (`src/policies/mlfq_policy.c:160`) aplica, en este orden:

1. **¿Quantum agotado?** → democión según la regla inyectada y reencolado al final del nivel destino.
2. **¿Toca priority boost?** (`t > 0 && t % S == 0`) → todos vuelven a Q0.
3. **¿Hay alguien de mayor prioridad?** → expropiación del ocupante, que vuelve al final de **su mismo** nivel.
4. **¿CPU libre?** → se toma la cabeza de la cola de mayor prioridad no vacía.

El orden no es arbitrario: si el boost se evaluara **antes** de la expiración del quantum,
un proceso que justo agotó su quantum sería promovido a Q0 y *además* conservaría su turno,
premiando precisamente al proceso intensivo en CPU que MLFQ quiere degradar.

Dos decisiones más, documentadas en el código porque no son evidentes:

- **En el boost, el proceso que está en CPU también se promueve y su quantum se reinicia,
  pero no pierde la CPU en ese mismo ciclo**: expropiarlo para volver a elegirlo sería un
  cambio de contexto contable sin efecto real.
- **El expropiado por prioridad no se degrada**, porque no agotó su quantum. Degradarlo
  castigaría a un proceso por la llegada de otro, algo que MLFQ no pretende.

### 3.4 Por qué la cola es un buffer circular

`fifo_ready_queue.c` usa un arreglo circular que duplica su capacidad al llenarse. Encolar
y desencolar son `O(1)`, que son las dos operaciones ejecutadas en **cada** ciclo de reloj.
Una lista enlazada daría la misma complejidad, pero con una asignación de memoria por cada
movimiento entre colas, y en MLFQ los procesos cambian de cola constantemente. El comentario
que explica esta elección está en la cabecera del archivo, no repetido línea por línea.

---

## 4. Patrones de diseño aplicados

El enunciado advierte que "usar un patrón sin necesidad real resta puntos tanto como no
usarlo cuando aporta valor". Por eso cada patrón se justifica con el problema concreto que
resuelve **en este proyecto**, y al final se listan los que se descartaron.

### 4.1 State — ciclo de vida del proceso

**Diagrama:** página 2 de `docs/diagramas/patrones-diseno.drawio`.

**Archivos:** `include/domain/process_state.h`, `src/domain/process_state.c`.

**Problema.** Un proceso pasa por `NEW → READY → RUNNING → {READY | WAITING | TERMINATED}`.
Las transiciones legales dependen del estado actual, y una transición ilegal (despachar un
proceso ya terminado, consumir CPU estando en `READY`, terminar con ráfaga pendiente)
corrompe las métricas **sin dar ningún síntoma**.

**Solución.** Cada estado es un objeto inmutable y compartido con una tabla de transiciones
(`kStateNew`, `kStateReady`, `kStateRunning`, `kStateWaiting`, `kStateTerminated`, en
`src/domain/process_state.c:103` y siguientes). `Process` solo delega:

```c
bool process_dispatch(Process *process, int cycle, Error *error) {
    return process->state->dispatch(process, cycle, error);   /* sin un solo switch */
}
```

Las transiciones no permitidas se rellenan con funciones `reject_*` que comparten un único
helper (`reject`, `process_state.c:9`) y producen un mensaje uniforme:
`"transicion invalida: no se puede 'despachar' un proceso P7 en estado NEW"`.

**Valor real, no decorativo.** El estado `RUNNING` es el único que sabe descontar ráfaga
(`running_consume_cycle`) y el estado `READY` es el único que fija `start_time` y
`first_response_time`, **y solo la primera vez** (`ready_dispatch`). Gracias a eso, el motor
de simulación no contiene ningún `if (es_la_primera_vez)`: la regla vive donde pertenece.
Está cubierto por la suite `test_process_state.c`, incluyendo el rechazo de cada transición
ilegal.

**Alternativa descartada.** Un `enum` con `switch` en cada operación. Se descartó porque
cada nueva operación obliga a tocar todos los `switch` y el compilador no ayuda a detectar
los casos olvidados; aquí, añadir un estado es añadir una tabla.

### 4.2 Strategy — política de planificación

**Diagrama:** páginas 3 y 6 de `docs/diagramas/patrones-diseno.drawio`.

**Archivos:** `include/domain/scheduling_policy.h`, `src/policies/mlfq_policy.c`,
`src/policies/simple_policies.c`.

**Problema.** El enunciado pide MLFQ, pero también pide poder "agregar una nueva política de
cola sin modificar el núcleo del scheduler" y sugiere explícitamente "intercambiar MLFQ por
FCFS/RR sin tocar el motor de simulación".

**Solución.** `SchedulingPolicy` es el contrato de cuatro operaciones de la sección 1. El
motor depende únicamente de él. Se implementaron **tres** políticas:

| Política | Archivo | Comportamiento |
|---|---|---|
| MLFQ | `mlfq_policy.c` | N niveles, quantum por nivel, democión, boost, expropiación |
| Round Robin | `simple_policies.c` | una cola, quantum fijo |
| FCFS | `simple_policies.c` | una cola, no expropiativo (`quantum = INT_MAX`) |

**Prueba de que el patrón funciona de verdad**: `make test` corre el escenario del enunciado
con las tres políticas usando *el mismo* motor, *las mismas* métricas y *el mismo* CSV, y
`./build/scheduler --policy fcfs` produce resultados distintos sin que se haya recompilado
una sola línea del motor. La tabla de [`ANALISIS.md`](ANALISIS.md) es literalmente el
patrón Strategy ejecutándose diez veces.

**Refinamiento: la política es configurable, no hard-coded.** `MlfqConfig`
(`include/domain/policies/mlfq_policy.h`) expone niveles, quantums, intervalo de boost,
expropiación, **la fábrica de colas** y **la regla de democión**:

```c
typedef int (*MlfqDemotionRule)(int current_level, int level_count);
int mlfq_demote_one_level(int current_level, int level_count);  /* la del enunciado */
int mlfq_demote_to_lowest(int current_level, int level_count);  /* alternativa */
```

Es la respuesta directa al criterio de OCP del enunciado ("agregar una cola adicional **o
una regla de democión distinta** sin modificar el núcleo"): `--quantums 2,4,8,16` agrega un
cuarto nivel y `--demote-to-lowest` cambia la regla, ambos sin editar `mlfq_policy.c`.

### 4.3 Observer — eventos de la simulación

**Diagrama:** página 4 de `docs/diagramas/patrones-diseno.drawio`.

**Archivos:** `include/domain/event_bus.h`, `src/domain/event_bus.c`,
`src/infrastructure/{trace,gantt,stats,wait_gap}_observer.c`.

**Problema.** Hacen falta cuatro vistas distintas de lo que ocurre dentro de la simulación
(traza legible, diagrama de Gantt, contadores agregados, espera máxima por proceso) y el
enunciado sugiere el patrón "si se decide registrar eventos de simulación". Sin él, el motor
terminaría lleno de `printf` y de contadores que no le corresponden, y cada nueva vista
sería una modificación del motor.

**Solución.** El motor y la política **publican** eventos (`LLEGADA`, `DESPACHO`,
`CICLO_EJECUTADO`, `EXPROPIACION`, `QUANTUM_AGOTADO`, `DEMOCION`, `PRIORITY_BOOST`,
`FIN_PROCESO`, `CPU_OCIOSA`, …) y no conocen a ningún suscriptor. El evento es un DTO
inmutable con `const Process *`: un observador **no puede** alterar la simulación.

Reparto de quién publica qué, deliberado para que no haya eventos duplicados:

- el **motor** publica los eventos del *ciclo de vida* (llegada, despacho, ciclo ejecutado, expropiación, fin, CPU ociosa);
- la **política** publica los eventos de *su* vocabulario (quantum agotado, democión, boost).

**La justificación más fuerte del patrón en este proyecto** es
`wait_gap_observer.c`: mide la **espera continua máxima** de cada proceso, que es la métrica
que hace visible la inanición (sección 4 de `ANALISIS.md`). Se agregó **después** de tener
el simulador terminado, **sin modificar ni el motor, ni la política, ni las métricas del
dominio**: solo escucha eventos que ya existían. Eso es exactamente el beneficio que promete
Observer, y aquí es comprobable en el historial del código.

Detalle de diseño menor pero útil: `event_bus_publish(NULL, …)` es una operación válida y
sin efecto (*Null Object*), de modo que el motor puede correr sin observadores —como en las
pruebas— sin llenarse de `if (bus != NULL)`.

### 4.4 Factory Method — origen de los procesos

**Diagrama:** página 5 de `docs/diagramas/patrones-diseno.drawio`.

**Archivos:** `include/application/ports/process_source.h`, los tres
`*_process_source.c`, y `ReadyQueueFactory` en `include/domain/ready_queue.h:50`.

**Problema.** Los procesos pueden venir de un CSV, de un generador aleatorio o del escenario
integrado del enunciado; y las colas de MLFQ pueden ser FIFO u ordenadas. Quien los usa no
debería saber de dónde salen.

**Solución.** `ProcessSource` es una fábrica polimórfica de `Process` (a la vez que el
puerto de entrada de la arquitectura), y `ReadyQueueFactory` es un puntero a función que se
inyecta en la política para que ésta cree sus niveles sin conocer la clase concreta:

```c
ReadyQueueFactory factory = config->queue_factory ? config->queue_factory : fifo_ready_queue_create;
for (int level = 0; level < policy->level_count; level++)
    policy->levels[level] = factory(level, error);
```

Gracias a eso `--queue srtf` cambia la disciplina interna de **todas** las colas sin tocar
el planificador. Que la sustitución funcione sin sorpresas es la comprobación práctica del
principio de sustitución de Liskov.

### 4.5 Patrones evaluados y descartados (evitar *over-engineering*)

| Patrón | Por qué **no** se usó |
|---|---|
| **Singleton** | Un bus de eventos global ahorraría un parámetro, pero convertiría a las pruebas en dependientes de estado compartido. El bus se inyecta. |
| **Command** | Los eventos son hechos pasados, no acciones a ejecutar ni deshacer. No hay *undo* ni cola de comandos que justifique el envoltorio. |
| **Decorator** | No hay comportamiento que apilar dinámicamente sobre una cola o una política; agregarlo sería una capa de indirección sin cliente. |
| **Visitor** | La jerarquía de eventos es un `enum` estable y pequeño; un `switch` en cada observador es más legible que una doble despacho. |
| **Template Method** | Se prefirió composición: FCFS y RR comparten implementación por **parámetro** (`quantum = INT_MAX`), no por herencia. |

---

## 5. Principios SOLID: evidencia en el código

### SRP — Responsabilidad única

Cada módulo tiene una razón para cambiar, y son razones distintas:

| Módulo | Única responsabilidad |
|---|---|
| `process.c` + `process_state.c` | ciclo de vida y datos de un proceso |
| `process_table.c` | poseer la colección y garantizar PIDs únicos |
| `fifo_ready_queue.c` | disciplina de una cola |
| `mlfq_policy.c` | decidir quién ejecuta (niveles, quantum, democión, boost) |
| `simulation_engine.c` | avanzar el reloj y mantener coherente el ciclo de vida |
| `metrics.c` | convertir estado final en números |
| `csv_results_writer.c` | conocer el formato CSV de salida |
| `csv_process_source.c` | conocer el formato CSV de entrada y validarlo |
| `cli_options.c` | traducir `argv` a configuración validada |
| `main.c` | decidir e inyectar las implementaciones concretas |

El contraejemplo que el enunciado quiere evitar ("funciones de 100+ líneas que mezclan
lectura de datos, lógica del scheduler y exportación") no existe aquí: la función más larga
del proyecto es `cli_options_parse` (parseo de banderas, plano y sin lógica de dominio) y
el motor completo cabe en unas 70 líneas apoyándose en tres helpers con nombre.

### OCP — Abierto/cerrado

Extensiones que **no** requieren modificar el núcleo:

| Extensión | Cómo se logra | Núcleo modificado |
|---|---|---|
| Cuarto nivel de cola | `--quantums 2,4,8,16` | ninguno |
| Otra regla de democión | `--demote-to-lowest` (o inyectar una `MlfqDemotionRule` propia) | ninguno |
| Otra disciplina de cola | `--queue srtf` (nueva implementación de `ReadyQueue`) | ninguno |
| Otra política completa | nuevo archivo que implemente `SchedulingPolicy` | ninguno |
| Otra fuente de procesos | nuevo `ProcessSource` (p. ej. JSON, red) | ninguno |
| Otra salida | nuevo `ResultsWriter` (p. ej. JSON, SQLite) | ninguno |
| Nueva vista/metrica en vivo | nuevo observador | ninguno |

`wait_gap_observer.c` es la prueba histórica: se añadió al final y el motor no cambió.

### LSP — Sustitución

Cualquier `ReadyQueue` es intercambiable: `shortest_remaining_ready_queue` sustituye a
`fifo_ready_queue` sin que el planificador lo note (`--queue srtf` corre y produce
resultados coherentes; la tabla de análisis lo incluye). Las tres políticas son
intercambiables ante el motor, y las tres fuentes de procesos ante el caso de uso. Los
contratos son honestos: `dequeue` sobre una cola vacía devuelve `NULL` en **ambas**
implementaciones, no un comportamiento distinto por implementación.

### ISP — Segregación de interfaces

Las interfaces son mínimas y nadie implementa métodos que no necesita:
`SimulationObserver` tiene **un** método (`on_event`); `ProcessSource` y `ResultsWriter`
tienen tres (describir, operar, destruir). El único método opcional del sistema es
`describe_queues` en `SchedulingPolicy`, pensado solo para trazas: puede ser `NULL` y el
envoltorio (`scheduling_policy_describe_queues`) devuelve `"-"` en ese caso, de modo que
ninguna política está obligada a implementarlo.

### DIP — Inversión de dependencias

- El motor depende de `SchedulingPolicy` (abstracción), nunca de `MlfqPolicy`.
- La política depende de `ReadyQueue` (abstracción), nunca del arreglo circular; ni siquiera
  construye sus colas: recibe una fábrica.
- El caso de uso depende de `ProcessSource` y `ResultsWriter`, nunca de "archivo CSV".
- Quien decide las implementaciones concretas es la capa **más externa** (`main.c`), que es
  exactamente lo que significa "inversión".

Comprobación mecánica: `grep -rn "mlfq_policy_create" src/application/` no devuelve nada.

---

## 6. Buenas prácticas de programación

**Nomenclatura.** Sin `x`, `temp` ni `datos2`. Los nombres dicen el propósito y la unidad:
`consumed_quantum`, `highest_ready_level`, `first_response_time`, `boost_interval`,
`preempt_on_higher_priority`. Convención uniforme: `snake_case` para funciones y variables,
`PascalCase` para tipos, prefijo de módulo en toda función pública
(`process_`, `ready_queue_`, `mlfq_`, `metrics_`), y `kNombre` para constantes de archivo.

**Funciones pequeñas con una sola responsabilidad.** El motor se lee como el enunciado:
`admit_arrivals`, `switch_context`, `validate_config`, y un bucle principal que solo
orquesta. En la política, cada regla es su propia función: `expire_quantum`,
`apply_priority_boost`, `preempt_holder`, `highest_ready_level`.

**DRY.** El movimiento de un proceso entre colas ocurre en **un** lugar
(`enqueue_at_level`, `mlfq_policy.c`), usado por la admisión, la democión, el boost y la
expropiación; no hay lógica repetida por nivel —el criterio que el enunciado menciona
explícitamente—. El rechazo de transiciones de estado comparte el helper `reject`. FCFS y
Round Robin comparten implementación y difieren en un parámetro. Los envoltorios de las
interfaces (`ready_queue.c`, `ports.c`, `scheduling_policy.c`) concentran la validación de
nulos para que ninguna implementación la repita.

**Manejo de errores explícito, sin fallos silenciosos.** C no tiene excepciones, así que la
convención es uniforme: toda operación que puede fallar recibe un `Error *` y devuelve
`bool`; `error_set` siempre devuelve `false` para permitir `return error_set(...)`. No hay
un solo `exit()` ni `assert()` en las capas internas: el error viaja hasta `main`, que lo
imprime con su código y su detalle.

Validaciones implementadas: `burst_time <= 0`, `arrival_time < 0`, `pid < 0`, PID duplicado,
quantum `<= 0`, número de niveles fuera de rango, `--boost` negativo, opción desconocida,
archivo inexistente, columna faltante, valor no numérico (`"4abc"` es un error, no un 4),
archivo sin procesos válidos, y una cota de ciclos que convierte un posible cuelgue en un
error legible (`ERROR_SIMULATION_LIMIT`). Los errores de E/S se enriquecen con el número de
línea:

```
$ ./build/scheduler --input examples/procesos_invalido.csv
Error [INVALID_ARGUMENT]: linea 5: P2: burst_time invalido (-4): debe ser > 0
```

**Comentarios útiles, no obvios.** Cada archivo empieza explicando *por qué* existe y qué
decisión encapsula (por qué la cola es circular, por qué `Process` es opaco, por qué el
boost se evalúa después de la expiración del quantum, por qué el evento de expropiación lo
publica el motor y no la política). No hay comentarios que narren lo que el código ya dice.

**Sin advertencias.** Todo compila con `-std=c11 -Wall -Wextra -Werror -pedantic`.
`leaks --atExit` reporta 0 fugas tanto en el simulador como en la batería de pruebas.

---

## 7. Estrategia de pruebas

`make test` → **215 verificaciones, 0 fallos**, en seis suites:

| Suite | Qué asegura |
|---|---|
| `test_process_state.c` | transiciones legales e **ilegales**; `start_time` no se sobrescribe en el segundo despacho; las promociones no cuentan como demociones |
| `test_ready_queue.c` | orden FIFO y crecimiento más allá de la capacidad inicial; orden de la cola alternativa (LSP) |
| `test_metrics.c` | las tres fórmulas, el caso `waiting = 0`, los promedios, el **rechazo de métricas de un proceso no terminado**, PIDs duplicados |
| `test_mlfq_policy.c` | democión al agotar quantum; **no** democión si termina antes; boost que devuelve a Q0 y reinicia el quantum; sin boost se queda en el fondo; expropiación por prioridad; regla de democión alternativa; configuración inválida |
| `test_simulation_engine.c` | escenario del enunciado con **valores calculados a mano**; invariante "ciclos totales = suma de ráfagas"; FCFS y RR sobre el mismo motor; conteo de ciclos ociosos |
| `test_process_sources.c` | CSV válido con cabecera/comentarios; cada error de formato con su número de línea; reproducibilidad de la carga aleatoria |

Dos decisiones de prueba que vale la pena defender:

1. **La prueba end-to-end usa valores derivados a mano**, no capturados de la ejecución. Los
   26 ciclos, los `finish_time` (23, 14, 26, 21) y los promedios (1.50 / 19.50 / 13.00) se
   obtuvieron trazando el escenario con lápiz y papel antes de ejecutar el programa, y
   coincidieron con la implementación. Una prueba que solo congela la salida actual no
   detecta que la salida actual esté mal.
2. **La política se prueba sin el motor.** `test_mlfq_policy.c` reproduce a mano el
   protocolo por ciclo (`drive_one_cycle`), lo que permite afirmar sobre el nivel de cola de
   un proceso *en un ciclo exacto* y documenta el contrato de Strategy.

---

## 8. Límites conocidos y extensiones naturales

Se declaran explícitamente para no presentar como completo algo que no lo es:

- **No hay E/S bloqueante.** El estado `WAITING` está implementado y probado (`block` /
  `resume`), pero el motor nunca lo provoca porque el enunciado modela procesos puramente
  de CPU. Es el punto de extensión evidente: un `ProcessSource` que aporte ráfagas de E/S y
  un dispositivo simulado usarían ese estado sin cambiar el patrón State.
- **Un solo procesador.** El contrato `select_for_cycle` devuelve un proceso por ciclo.
  Extender a N CPUs cambiaría la firma a una lista y afectaría al motor y a las políticas
  (no a métricas, salidas ni observadores).
- **`event_bus` con capacidad fija** (16 observadores) y `wait_gap_observer` con 128
  procesos: límites deliberados con error explícito al excederse, en vez de asignación
  dinámica que aquí no aportaría nada.
- **El diagrama de Gantt es ASCII** y se degrada silenciosamente si no puede crecer: una
  vista de depuración no debe hacer fallar una simulación válida.
