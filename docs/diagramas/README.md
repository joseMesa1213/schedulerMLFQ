# Diagramas del diseño (formato draw.io)

Un solo archivo con **siete páginas**: [`patrones-diseno.drawio`](patrones-diseno.drawio).

Está guardado como **XML sin comprimir**, así que es legible, editable a mano y `git diff`
muestra cambios reales en lugar de un bloque base64 opaco.

## Cómo abrirlo

| Vía | Pasos |
|---|---|
| Navegador (sin instalar nada) | Ir a <https://app.diagrams.net> → *Archivo* → *Abrir desde* → *Dispositivo* → elegir `patrones-diseno.drawio`. También sirve arrastrar el archivo a la ventana. |
| VS Code | Instalar la extensión **Draw.io Integration** (`hediet.vscode-drawio`) y abrir el archivo: se edita dentro del editor. |
| Aplicación de escritorio | draw.io Desktop (macOS/Windows/Linux): *Archivo* → *Abrir*. |

Las siete páginas aparecen como **pestañas en la parte inferior** de la ventana.

## Qué hay en cada página

| # | Página | Qué demuestra | Código que representa |
|---|---|---|---|
| 1 | **Arquitectura y regla de dependencia** | Las cuatro capas y que las dependencias apuntan siempre al dominio; los dos `grep` que lo verifican | todo el árbol `src/` |
| 2 | **Patrón State** | Los cinco estados como objetos con su tabla de transiciones, y la máquina de estados resultante | `domain/process.h`, `domain/process_state.c` |
| 3 | **Patrón Strategy** | El motor depende de la interfaz; tres políticas la implementan; `MlfqConfig` como puntos de extensión (OCP) | `domain/scheduling_policy.h`, `policies/*.c` |
| 4 | **Patrón Observer** | Quién publica qué eventos, el bus que no conoce suscriptores, y los cuatro observadores | `domain/event_bus.c`, `infrastructure/*_observer.c` |
| 5 | **Factory Method, puertos y adaptadores** | Los dos puertos con sus adaptadores, la raíz de composición y `ReadyQueueFactory` | `application/ports/`, `infrastructure/`, `domain/ready_queue.h` |
| 6 | **Secuencia de un ciclo de reloj** | Cómo colaboran los tres patrones en los once mensajes de un ciclo | `application/simulation_engine.c` |
| 7 | **Algoritmo MLFQ** | Las tres colas, la democión, el *priority boost* y el orden de las cuatro decisiones | `policies/mlfq_policy.c` |

Cada caja lleva **debajo del título la ruta del archivo real** que representa. Las 29 rutas
citadas en los diagramas existen en el repositorio (comprobado automáticamente).

## Leyenda

Está también dentro de la página 1 del archivo.

**Colores = capa de la arquitectura:**

| Color | Capa |
|---|---|
| Azul | `domain` — entidades y reglas de negocio |
| Verde | `application` — casos de uso y puertos |
| Naranja | `infrastructure` — adaptadores (archivos, consola) |
| Violeta | `cli` — raíz de composición y argumentos |
| Amarillo | nota o decisión de diseño (no es código) |
| Rojo | elemento destacado (el observador que revela la inanición, el *priority boost*) |

**Flechas:**

| Trazo | Significado |
|---|---|
| Discontinua con punta hueca ▷ | *implementa* la interfaz (realización UML) |
| Discontinua con punta abierta → | *depende de* / usa |
| Continua con rombo ◆ | posee / contiene |
| Continua gruesa ▶ | flujo de control o de datos |
| Discontinua roja ▶ | *priority boost* |

## Referencias cruzadas: patrón → código → prueba → documento

| Patrón / principio | Página | Archivos | Prueba que lo respalda | Justificación escrita |
|---|---|---|---|---|
| **State** | 2 | `domain/process_state.c`, `domain/process_internal.h` | `./build/run_tests state` | [`DESIGN.md` §4.1](../../DESIGN.md) |
| **Strategy** | 3, 6 | `domain/scheduling_policy.h`, `policies/mlfq_policy.c`, `policies/simple_policies.c` | `./build/run_tests mlfq engine` | [`DESIGN.md` §4.2](../../DESIGN.md) |
| **Observer** | 4, 6 | `domain/event_bus.c`, `infrastructure/{trace,gantt,stats,wait_gap}_observer.c` | `./build/run_tests mlfq` (usa `StatsObserver` como sonda) | [`DESIGN.md` §4.3](../../DESIGN.md) |
| **Factory Method** | 5 | `application/ports/process_source.h`, los tres `*_process_source.c`, `ReadyQueueFactory` | `./build/run_tests input queue` | [`DESIGN.md` §4.4](../../DESIGN.md) |
| **Arquitectura limpia** | 1 | estructura completa de `src/` | los dos `grep` de la página 1 | [`DESIGN.md` §2](../../DESIGN.md) |
| **SRP / OCP / LSP / ISP / DIP** | 1, 3, 5 | ver las tablas de cada página | `make test` completo | [`DESIGN.md` §5](../../DESIGN.md) |
| **Algoritmo MLFQ** | 7 | `policies/mlfq_policy.c` | `./build/run_tests mlfq` | [`ANALISIS.md`](../../ANALISIS.md) |

## Exportar para las diapositivas

En draw.io: *Archivo* → *Exportar como* → *PNG…* (o *PDF*). Para presentar:

- Marcar **Transparent Background** si el fondo de la diapositiva no es blanco.
- Subir el **Zoom** a 200–300 % para que el texto de 8–9 px (las rutas de archivo) se lea
  al proyectar.
- *Selection Only* permite exportar un solo bloque de una página, útil para llevar a la
  diapositiva únicamente el diagrama de una interfaz.
- Con *Página actual* se exporta una imagen por pestaña; con *Todas las páginas* en PDF se
  obtiene un documento de siete hojas listo para anexar al informe.

## Editarlos

Se pueden modificar libremente en draw.io: el archivo es la única fuente. Si se agregan
cajas, conviene mantener dos convenciones para que sigan siendo útiles:

1. **El color indica la capa** (ver leyenda). Un elemento de infraestructura pintado de azul
   comunica lo contrario de lo que el diseño defiende.
2. **Cada caja cita su archivo** debajo del título, en 8 px gris. Es lo que permite pasar del
   diagrama al código sin buscar.
