# COMPILACION.md — Guía de compilación y pruebas (macOS y Windows)

El proyecto es **C11 puro sin dependencias externas**: no usa librerías de terceros, ni
`pthread`, ni funciones POSIX específicas. Compila con `clang`, `gcc` y —con un ajuste de
banderas— `MSVC`. Solo hacen falta **un compilador C11** y **GNU make** (y ni siquiera make:
en la sección 3.3 hay un comando único que compila todo).

## Resumen ultrarrápido

| Plataforma | Preparación | Compilar y probar |
|---|---|---|
| **macOS** | `xcode-select --install` | `make && make test` |
| **Windows** (MSYS2, recomendado) | instalar MSYS2 + `pacman -S mingw-w64-ucrt-x86_64-gcc make` | `make && make test` |
| **Windows** (WSL2) | `wsl --install -d Ubuntu` + `sudo apt install build-essential` | `make && make test` |
| **Windows** (sin make) | instalar MinGW-w64 o LLVM | ver el comando único de la sección 3.3 |
| **Linux** | `sudo apt install build-essential` | `make && make test` |

Resultado esperado de `make test` en cualquier plataforma:

```
6 suite(s), 215 verificaciones, 0 fallos
RESULTADO: OK
```

---

## 0. Demostración guiada (lo más rápido para verificar que todo funciona)

Cada plataforma tiene un script que compila, ejecuta las pruebas y recorre el funcionamiento
del simulador en diez pasos, mostrando de cada uno **solo lo esencial**:

| Plataforma | Comando |
|---|---|
| macOS, Linux, WSL, MSYS2 | `./scripts/demo.sh` |
| Windows (cmd.exe o doble clic) | `scripts\demo.bat` |
| Windows (PowerShell) | `.\scripts\demo.ps1` |

Ambos aceptan una opción para **pausar entre pasos**, pensada para presentar en vivo:

```bash
./scripts/demo.sh -p            # macOS / Linux / WSL / MSYS2
```
```powershell
.\scripts\demo.ps1 -Pausa      # Windows
```

Los diez pasos son: compilación · pruebas unitarias · escenario del enunciado · línea de
tiempo · traza de eventos · validación de entradas · las tres políticas (Strategy) ·
extensiones sin tocar el núcleo (OCP) · inanición con y sin *priority boost* · `results.csv`.

El script de PowerShell usa `make` si lo encuentra y, si no, **compila directamente con gcc,
clang o cc**: sirve igual en una instalación de Windows sin make. Los archivos auxiliares van
a un directorio temporal que se borra al terminar; lo único que queda en el proyecto es
`results.csv`, que es entregable.

> Si en Windows aparecen caracteres extraños en lugar de tildes, ejecutar `chcp 65001` antes
> del script, o usar Windows Terminal en vez de la consola heredada.

---

## 1. Requisitos

| Requisito | Por qué | Cómo verificar |
|---|---|---|
| Compilador con C11 | el código usa `<stdbool.h>`, inicializadores designados y `snprintf` | `cc --version` |
| GNU make (3.81 o superior) | el `Makefile` usa `$(wildcard)` y `$(patsubst)` | `make --version` |
| `bash` + `awk` (**opcional**) | solo para `make experiments` | `bash --version` |

No se necesita CMake, ni Autotools, ni gestor de paquetes, ni conexión a internet.

---

## 2. macOS

### 2.1 Instalar las herramientas

```bash
xcode-select --install          # instala clang y make (no requiere Xcode completo)
```

Verificar:

```bash
cc --version        # Apple clang version 21.x  (cualquier versión >= 11 sirve)
make --version      # GNU Make 3.81 (la que trae macOS; es suficiente)
```

### 2.2 Compilar, ejecutar y probar

```bash
cd simulador_scheduler
make                # compila build/scheduler
make test           # 6 suite(s), 215 verificaciones, 0 fallos
make run            # escenario del enunciado -> consola + results.csv
make trace          # igual, con la traza ciclo a ciclo
make experiments    # regenera docs/resultados/comparativa.md
make clean
```

`make` es incremental: al cambiar un solo `.c` recompila ese archivo y vuelve a enlazar.

### 2.3 Comprobar fugas de memoria (herramienta nativa de macOS)

```bash
leaks --atExit -- ./build/scheduler --no-csv
leaks --atExit -- ./build/run_tests
```

Salida esperada en ambos casos: `0 leaks for 0 total leaked bytes`.

Si `leaks` pide permisos, ejecutarlo con `MallocStackLogging=1` delante para obtener además
la pila de la asignación culpable.

### 2.4 Usar gcc en lugar de clang (opcional)

Compilar con **dos** compiladores distintos es la forma más económica de detectar código que
depende de un comportamiento no estándar:

```bash
brew install gcc
make clean && make CC=gcc-14 && make test CC=gcc-14
```

### 2.5 Apple Silicon (M1/M2/M3/M4) e Intel

No hay nada específico por arquitectura: el proyecto no tiene ensamblador, ni supone tamaños
de tipos, ni depende del orden de bytes. Se desarrolló y verificó en **arm64**.

---

## 3. Windows

Hay cuatro caminos. **Se recomienda el A** (MSYS2): es el que reproduce la experiencia de
macOS/Linux sin virtualización y permite usar el `Makefile`, los sanitizers y el script de
experimentos tal cual.

### 3.1 Opción A — MSYS2 + MinGW-w64 (recomendada)

1. Descargar e instalar MSYS2 desde <https://www.msys2.org> (instalador `.exe`, siguiente,
   siguiente).
2. Abrir el menú Inicio → **"MSYS2 UCRT64"** (importante: *no* la terminal "MSYS", sino la
   **UCRT64**, que produce ejecutables nativos de Windows).
3. Instalar el compilador y make:

```bash
pacman -Syu                                        # actualizar (puede pedir cerrar y reabrir)
pacman -S mingw-w64-ucrt-x86_64-gcc make git       # compilador + make
```

4. Compilar y probar (la ruta de Windows `C:\Users\...` se escribe `/c/Users/...`):

```bash
cd /c/Users/TuUsuario/Documents/simulador_scheduler
make
make test
make run
```

El `Makefile` detecta Windows y produce **`build/scheduler.exe`** y **`build/run_tests.exe`**
automáticamente (`ifeq ($(OS),Windows_NT)`); los objetivos `make run` y `make test` funcionan
igual.

> **Advertencia importante:** ejecutar `make` desde `cmd.exe` o PowerShell **no** funciona,
> aunque MSYS2 esté instalado, porque el `Makefile` usa `mkdir -p` y `rm -rf`, que son
> comandos del shell de Unix. Hay que usar la terminal **MSYS2 UCRT64**. Si se prefiere no
> abrirla, usar el comando único de la sección 3.3.

### 3.2 Opción B — WSL2 (subsistema de Linux en Windows)

Es la opción más cómoda si además se quiere `valgrind`, que en Windows nativo no existe.

En PowerShell **como administrador**:

```powershell
wsl --install -d Ubuntu
```

Reiniciar, abrir Ubuntu y dentro:

```bash
sudo apt update && sudo apt install -y build-essential valgrind
cd /mnt/c/Users/TuUsuario/Documents/simulador_scheduler   # los discos de Windows están en /mnt
make && make test
valgrind --leak-check=full ./build/run_tests              # equivalente a `leaks` de macOS
```

> Nota de rendimiento: compilar en `/mnt/c` es notablemente más lento que en el sistema de
> archivos de Linux. Si molesta, copiar el proyecto a `~/simulador_scheduler` y trabajar ahí.

### 3.3 Opción C — Windows sin make (un solo comando)

Con MinGW-w64 (`gcc`) o LLVM (`clang`) en el `PATH`, desde `cmd.exe` o PowerShell:

```bat
REM 1) El simulador
gcc -std=c11 -Wall -Wextra -pedantic -O2 -Iinclude -Isrc ^
    src\domain\*.c src\policies\*.c src\application\*.c src\infrastructure\*.c src\cli\*.c ^
    -o scheduler.exe

REM 2) Las pruebas (todo menos main.c del simulador, más tests\*.c)
gcc -std=c11 -Wall -Wextra -pedantic -O2 -Iinclude -Isrc ^
    src\domain\*.c src\policies\*.c src\application\*.c src\infrastructure\*.c ^
    src\cli\cli_options.c tests\*.c ^
    -o run_tests.exe

scheduler.exe
run_tests.exe
```

En PowerShell, reemplazar el `^` de continuación de línea por un acento grave `` ` ``, o
escribir cada comando en una sola línea.

El equivalente en bash/zsh (macOS, Linux, MSYS2) —**verificado en esta máquina**— es:

```bash
cc -std=c11 -Wall -Wextra -pedantic -O2 -Iinclude -Isrc \
   src/domain/*.c src/policies/*.c src/application/*.c src/infrastructure/*.c src/cli/*.c \
   -o scheduler

cc -std=c11 -Wall -Wextra -pedantic -O2 -Iinclude -Isrc \
   src/domain/*.c src/policies/*.c src/application/*.c src/infrastructure/*.c \
   src/cli/cli_options.c tests/*.c \
   -o run_tests
```

La única regla que hay que recordar: **`src/cli/main.c` y `tests/main.c` no pueden entrar en
el mismo enlace**, porque cada uno define su propia función `main`.

### 3.4 Opción D — Visual Studio / MSVC (`cl.exe`)

Abrir **"Developer Command Prompt for VS 2022"** (no el `cmd` normal: ese no tiene `cl` en el
`PATH`) y ejecutar:

```bat
cl /std:c11 /W4 /permissive- /nologo /I include /I src ^
   src\domain\*.c src\policies\*.c src\application\*.c src\infrastructure\*.c src\cli\*.c ^
   /Fe:scheduler.exe

cl /std:c11 /W4 /permissive- /nologo /I include /I src ^
   src\domain\*.c src\policies\*.c src\application\*.c src\infrastructure\*.c ^
   src\cli\cli_options.c tests\*.c ^
   /Fe:run_tests.exe
```

Equivalencia de banderas entre compiladores:

| Intención | gcc / clang | MSVC |
|---|---|---|
| Estándar C11 | `-std=c11` | `/std:c11` |
| Advertencias altas | `-Wall -Wextra -pedantic` | `/W4 /permissive-` |
| Advertencias como errores | `-Werror` | `/WX` |
| Optimizar | `-O2` | `/O2` |
| Símbolos de depuración | `-g` | `/Zi` |
| Directorio de cabeceras | `-Iinclude` | `/I include` |
| Nombre del ejecutable | `-o scheduler` | `/Fe:scheduler.exe` |
| AddressSanitizer | `-fsanitize=address` | `/fsanitize=address` |

**Advertencias honestas sobre MSVC:**

- Este camino **no se ejecutó** al preparar el proyecto (la máquina de desarrollo es macOS
  arm64); las banderas son las documentadas por Microsoft, pero conviene reservar tiempo para
  ajustarlas si algo no compila de primera.
- El código se preparó pensando en MSVC: se eliminó el único **literal compuesto**
  (`&(MlfqConfig){...}`) que había en las pruebas, precisamente porque es la construcción de
  C11 que MSVC soporta peor. No hay VLA, ni `__attribute__`, ni extensiones GNU.
- `/W4` es más ruidoso que `-Wall -Wextra`: puede emitir advertencias informativas (por
  ejemplo C4100 sobre parámetros no usados, que en este proyecto se anulan con `(void)param`).
  No usar `/WX` la primera vez.

### 3.5 Notas específicas de Windows

| Tema | Situación |
|---|---|
| **Extensión `.exe`** | El `Makefile` la agrega automáticamente cuando detecta `OS=Windows_NT`. |
| **Finales de línea CRLF** | **Soportados y verificados.** Un CSV guardado en Windows (`\r\n`) se lee correctamente: el parser recorta los espacios en blanco, y `\r` lo es. |
| **Separador `;`** | **Soportado y verificado.** Excel en español guarda con punto y coma; el parser acepta `,` y `;`. |
| **`scripts/experimentos.sh`** | Requiere `bash` y `awk`: funciona en MSYS2 y WSL, **no** en `cmd.exe` ni PowerShell. Sin ellos, ejecutar el simulador a mano con las banderas que documenta `ANALISIS.md`. |
| **Permisos del script** | Si aparece `Permission denied`, ejecutar `chmod +x scripts/experimentos.sh`. |
| **Rutas con espacios o tildes** | Encerrar la ruta en comillas: `--input "C:\Mis Documentos\procesos.csv"`. |
| **Antivirus** | Algunos antivirus retrasan la primera ejecución de un `.exe` recién enlazado; no es un fallo del programa. |

---

## 4. Linux (por completitud)

```bash
sudo apt update && sudo apt install -y build-essential valgrind   # Debian/Ubuntu
# sudo dnf install gcc make valgrind                              # Fedora
make && make test
valgrind --leak-check=full --error-exitcode=1 ./build/run_tests
```

---

## 5. Guía de pruebas

### 5.1 Ejecutar las pruebas

```bash
make test                          # las seis suites
./build/run_tests                  # igual, sin pasar por make
./build/run_tests mlfq             # una sola suite
./build/run_tests metrics engine   # varias
./build/run_tests --list           # nombres disponibles
```

Ejecutar una sola suite acorta el ciclo de trabajo al depurar: `./build/run_tests mlfq` corre
22 verificaciones en lugar de 215.

### 5.2 Qué cubre cada suite

| Nombre | Archivo | Qué verifica |
|---|---|---|
| `state` | `tests/test_process_state.c` | Transiciones legales del ciclo de vida y **rechazo** de las ilegales; que `start_time` no se sobrescriba en el segundo despacho; que una promoción por boost no cuente como democión; validación de `burst_time`/`arrival_time` |
| `queue` | `tests/test_ready_queue.c` | Orden FIFO y crecimiento del buffer circular más allá de su capacidad inicial; orden de la cola por menor tiempo restante (sustituibilidad) |
| `metrics` | `tests/test_metrics.c` | Las tres fórmulas obligatorias; el caso `waiting = 0`; los promedios del informe; el **rechazo** de calcular métricas de un proceso no terminado; PIDs duplicados |
| `mlfq` | `tests/test_mlfq_policy.c` | **Democión** al agotar el quantum; **no** democión si termina antes; **priority boost** que devuelve a Q0 y reinicia el quantum; sin boost se queda en el fondo; expropiación por mayor prioridad; regla de democión alternativa; configuración inválida |
| `engine` | `tests/test_simulation_engine.c` | Escenario del enunciado con **valores calculados a mano**; invariante "ciclos totales = suma de ráfagas"; FCFS y Round Robin sobre el mismo motor; conteo de ciclos ociosos |
| `input` | `tests/test_process_sources.c` | CSV válido con cabecera y comentarios; cabecera precedida por comentarios (caso de regresión); cada error de formato con su número de línea; reproducibilidad de la carga aleatoria |

### 5.3 Cómo leer la salida

**Todo en verde:**

```
Suite: politica MLFQ (democion, boost, prioridad)
  - un proceso que agota el quantum de Q0 (2 ciclos) baja a Q1
  - un proceso que termina dentro del quantum NO baja de nivel
  ...
6 suite(s), 215 verificaciones, 0 fallos
RESULTADO: OK
```

Cada línea con `-` es una prueba; el conteo final es de *verificaciones* individuales
(aserciones), no de pruebas.

**Un fallo** (salida real, provocada a propósito cambiando un valor esperado):

```
Suite: calculo de metricas
  - response = primera respuesta - llegada; turnaround = fin - llegada; waiting = turnaround - burst
    FALLO [response = primera respuesta - llegada; ...] response = 5 - 2: se esperaba 4 y se obtuvo 3 (tests/test_metrics.c:32)
6 suite(s), 215 verificaciones, 1 fallos
RESULTADO: FALLIDO
```

El mensaje da: la prueba, la verificación concreta, el valor esperado, el obtenido y el
**archivo con número de línea**. Las pruebas **no se detienen** en el primer fallo: así un
cambio equivocado muestra de una vez todo lo que rompió.

### 5.4 Códigos de salida

| Código | Significado | Útil para |
|---|---|---|
| `0` | todas las verificaciones pasaron | integración continua |
| `1` | al menos una falló | `make test` corta la cadena de objetivos |
| `2` | el nombre de suite indicado no existe | detectar errores de tipeo en el argumento |

### 5.5 Detección de errores de memoria y comportamiento indefinido

```bash
make sanitize
```

Recompila **en `build/asan/`** (no mezcla objetos con la compilación normal, que es una fuente
clásica de fallos difusos de enlazado) y ejecuta las pruebas con **AddressSanitizer** y
**UndefinedBehaviorSanitizer** activos. Detecta accesos fuera de rango, uso después de liberar,
doble liberación, fugas y desbordamientos de entero con signo. Resultado esperado: las 215
verificaciones en verde y ni una sola alerta del sanitizer.

Disponible con `clang` y `gcc` (macOS, Linux, WSL, MSYS2 con clang). No con MinGW clásico.

Herramientas equivalentes por plataforma:

| Plataforma | Fugas de memoria | Errores de memoria |
|---|---|---|
| macOS | `leaks --atExit -- ./build/run_tests` | `make sanitize` |
| Linux / WSL | `valgrind --leak-check=full ./build/run_tests` | `make sanitize` |
| Windows (MSYS2/clang) | `make sanitize` (ASan reporta fugas) | `make sanitize` |
| Windows (MSVC) | — | `cl /fsanitize=address ...` |

### 5.6 Añadir una prueba nueva

1. Escribirla en el archivo de la suite que corresponda, con las macros `CHECK`,
   `CHECK_INT_EQ` o `CHECK_DOUBLE_EQ`, y anunciarla con `TEST_BEGIN("qué se verifica")`.
2. Llamarla desde la función `suite_*` al final de ese archivo.
3. `make test`. No hay que registrar el archivo en ningún sitio: el `Makefile` recoge
   `tests/*.c` con `$(wildcard)`.

Para crear una **suite** nueva: agregar `tests/test_x.c` con su función `suite_x`, declararla
en `tests/test_suites.h` y añadir una fila a la tabla `kSuites` de `tests/main.c`.

Convención usada en el proyecto: el texto de `TEST_BEGIN` describe la **regla del enunciado**
que se está verificando ("un proceso que termina dentro del quantum NO baja de nivel"), no la
función que se llama. Así la salida de `make test` se puede leer como una lista de requisitos
cumplidos.

---

## 6. Los experimentos del análisis

```bash
make experiments      # equivale a ./scripts/experimentos.sh
```

Corre 15 variantes, guarda la salida de cada una en `docs/resultados/<etiqueta>.txt` y `.csv`,
y regenera la tabla comparativa de `docs/resultados/comparativa.md` que sustenta
[`ANALISIS.md`](ANALISIS.md). Requiere `bash` y `awk` (macOS, Linux, WSL, MSYS2).

Sin bash, cada fila de la tabla se puede reproducir a mano:

```bash
./build/scheduler --boost 0  --label sin-boost   --output sin-boost.csv
./build/scheduler --boost 3  --label boost-3     --output boost-3.csv
./build/scheduler --quantums 1,4,8 --label q0-1  --output q0-1.csv
./build/scheduler --policy fcfs --label fcfs     --output fcfs.csv
```

---

## 7. Banderas de compilación y por qué están

| Bandera | Para qué |
|---|---|
| `-std=c11` | Fija el estándar; evita depender de extensiones del compilador |
| `-Wall -Wextra` | Advertencias habituales y adicionales (variables sin usar, comparaciones sospechosas, campos sin inicializar) |
| `-Werror` | Convierte toda advertencia en error: **el proyecto no admite advertencias** |
| `-pedantic` | Rechaza extensiones no estándar; es lo que garantiza que compile con otro compilador |
| `-O2` | Optimización; también activa análisis de flujo que descubre más advertencias |
| `-g` | Símbolos de depuración, para que `lldb`/`gdb` y los sanitizers muestren nombres y líneas |

Se pueden sobrescribir sin editar el `Makefile`:

```bash
make CC=gcc-14                                  # otro compilador
make CFLAGS="-std=c11 -Wall -O0 -g"             # sin -Werror, para experimentar
make WARNINGS="-Wall"                            # menos advertencias
```

---

## 8. Solución de problemas

| Síntoma | Causa | Solución |
|---|---|---|
| `make: command not found` (Windows) | make no está instalado o no se está en la terminal MSYS2 | `pacman -S make` y usar la terminal **MSYS2 UCRT64**; o el comando único de la sección 3.3 |
| `mkdir: illegal option -- p` (Windows) | se ejecutó `make` desde `cmd.exe`/PowerShell | usar la terminal MSYS2 o WSL |
| `cl : command not found` | `cmd` normal en vez del de Visual Studio | abrir "Developer Command Prompt for VS" |
| `Undefined symbols: ___asan_init` al enlazar | quedaron objetos compilados **con** sanitizer y se está enlazando **sin** él (o al revés) | `make clean && make`. Por eso `make sanitize` compila en `build/asan/` |
| `duplicate symbol _main` | se incluyeron `src/cli/main.c` y `tests/main.c` en el mismo enlace | usar los comandos de la sección 3.3 tal como están |
| `fatal error: 'domain/process.h' file not found` | faltan `-Iinclude -Isrc` | compilar desde la raíz del proyecto y con ambas banderas |
| `Permission denied` al correr `experimentos.sh` | el script perdió el bit de ejecución (típico al descomprimir en Windows) | `chmod +x scripts/experimentos.sh` |
| `no se pudo abrir el archivo de procesos '...'` | ruta relativa incorrecta o espacios sin comillas | ejecutar desde la raíz del proyecto; entrecomillar la ruta |
| `linea N: '...' no es un entero valido` | el CSV tiene una celda no numérica o una columna de más | revisar la línea N; se aceptan `,` y `;` como separador, y `#` para comentarios |
| `la simulacion excedio 100000 ciclos` | una política modificada nunca selecciona procesos listos | revisar `select_for_cycle`: es la red de seguridad que convierte un cuelgue en un error legible |
| `make test` no recompila tras editar una prueba | reloj del sistema o del archivo desfasado (común en carpetas compartidas) | `touch tests/*.c && make test`, o `make clean && make test` |
| El ejecutable no aparece en Windows | se buscó `build/scheduler` sin `.exe` | el binario es `build/scheduler.exe` |

---

## 9. Qué se verificó y en qué máquina

Transparencia sobre el alcance de las pruebas de este documento:

**Verificado ejecutándolo** en macOS 26.6.2, arm64 (Apple Silicon), Apple clang 21.0.0,
GNU Make 3.81:

- `make`, `make run`, `make trace`, `make test`, `make sanitize`, `make experiments`, `make clean`
- Compilación **sin una sola advertencia** con `-Wall -Wextra -Werror -pedantic`
- **215 verificaciones, 0 fallos**; y el formato del mensaje de fallo, provocando uno a propósito
- **0 fugas de memoria** (`leaks --atExit`) en el simulador y en las pruebas
- AddressSanitizer + UndefinedBehaviorSanitizer: sin alertas
- Los dos comandos únicos de compilación sin `make` (simulador y pruebas)
- Lectura de CSV con finales de línea **CRLF de Windows** y con separador **`;`**
- Las doce combinaciones de banderas documentadas en el `README.md`
- `scripts/demo.sh` completo, con y sin `-p`
- Las diez expresiones de extracción de texto que usa `scripts/demo.ps1`, simuladas contra la
  salida real del simulador (para descartar secciones vacías en Windows)

**No verificado en esta máquina** (no hay Windows ni PowerShell disponibles): los pasos de
MSYS2, WSL2 y MSVC, y la ejecución de `scripts/demo.ps1` / `scripts/demo.bat`. Son
procedimientos estándar y tanto el código como los scripts se prepararon para ellos:

- `Makefile` sin `find`, extensión `.exe` automática, sin literales compuestos ni extensiones GNU;
- `demo.ps1` guardado en **UTF-8 con BOM y saltos CRLF** (así PowerShell 5.1 lee bien las
  tildes), sin depender de `awk`, `sed` ni `grep`, y con alternativa a `make`;
- sus diez expresiones de extracción de texto **sí** se validaron contra la salida real del
  simulador, replicando la lógica del script.

Aun así se declaran como no ejecutados en lugar de darlos por buenos.
