#!/usr/bin/env bash
#
# demo.sh - Demostración guiada del simulador MLFQ para macOS (y Linux/WSL).
#
# Muestra el funcionamiento completo del proyecto en diez pasos, imprimiendo
# solo lo esencial de cada uno: la idea es que quepa en pantalla y se pueda
# narrar en vivo, no volcar toda la salida del programa.
#
#   ./scripts/demo.sh              corrido continuo
#   ./scripts/demo.sh -p           pausa entre pasos (para presentar en vivo)
#   ./scripts/demo.sh --help
#
# Los archivos auxiliares se escriben en un directorio temporal que se borra al
# terminar; lo único que queda en el proyecto es results.csv, que es entregable.

set -eu

cd "$(dirname "$0")/.."

BIN=build/scheduler
PAUSA=0

for argumento in "$@"; do
  case "$argumento" in
    -p|--pausa) PAUSA=1 ;;
    -h|--help)
      sed -n '/^# demo.sh/,/^# terminar/p' "$0" | sed 's/^# \{0,1\}//'
      exit 0 ;;
    *)
      echo "Opción desconocida: $argumento (use --help)" >&2
      exit 1 ;;
  esac
done

TEMPORAL="$(mktemp -d)"
trap 'rm -rf "$TEMPORAL"' EXIT

# --- Presentación en pantalla -------------------------------------------------

if [ -t 1 ]; then
  NEGRITA="$(printf '\033[1m')"; APAGADO="$(printf '\033[2m')"
  VERDE="$(printf '\033[32m')";  CIAN="$(printf '\033[36m')"
  NORMAL="$(printf '\033[0m')"
  ANCHO="$(tput cols 2>/dev/null || echo 100)"
else
  NEGRITA=""; APAGADO=""; VERDE=""; CIAN=""; NORMAL=""; ANCHO=100
fi

PASO=0
TOTAL=10

paso() {
  PASO=$((PASO + 1))
  printf '\n%s%s── %d/%d · %s %s%s\n' "$NEGRITA" "$CIAN" "$PASO" "$TOTAL" "$1" \
    "$(printf '%.0s─' $(seq 1 $((ANCHO > 60 ? 20 : 3))))" "$NORMAL"
}

comando() { printf '%s   $ %s%s\n' "$APAGADO" "$1" "$NORMAL"; }
nota()    { printf '%s   → %s%s\n' "$VERDE" "$1" "$NORMAL"; }
sangrar() { sed 's/^/   /' | cut -c "1-$ANCHO"; }

pausar() {
  [ "$PAUSA" -eq 1 ] || return 0
  printf '\n%s   [ENTER para continuar]%s' "$APAGADO" "$NORMAL"
  read -r _ || true
  printf '\n'
}

# --- 1. Compilación -----------------------------------------------------------

printf '%s%s\n  SIMULADOR DE SCHEDULER MLFQ — demostración\n%s' "$NEGRITA" "$CIAN" "$NORMAL"
printf '%s  C11 sin dependencias · arquitectura por capas · State + Strategy + Observer%s\n' \
  "$APAGADO" "$NORMAL"

paso "Compilación"
comando "make clean && make"
make clean >/dev/null
if make >"$TEMPORAL/build.log" 2>&1; then
  nota "compilado sin una sola advertencia con -Wall -Wextra -Werror -pedantic"
  printf '%s   (%s archivos objeto)%s\n' "$APAGADO" "$(grep -c ' -c ' "$TEMPORAL/build.log")" "$NORMAL"
else
  tail -20 "$TEMPORAL/build.log" | sangrar
  echo "La compilación falló." >&2
  exit 1
fi
pausar

# --- 2. Pruebas ---------------------------------------------------------------

paso "Pruebas unitarias"
comando "make test"
make test >"$TEMPORAL/test.log" 2>&1 || true
grep -E "^Suite:" "$TEMPORAL/test.log" | sangrar
printf '\n'
grep -E "verificaciones|RESULTADO" "$TEMPORAL/test.log" | sangrar
pausar

# --- 3. Escenario del enunciado ----------------------------------------------

paso "Escenario del enunciado: P1(0,8) P2(1,4) P3(2,9) P4(3,5)"
comando "./$BIN"
./$BIN >"$TEMPORAL/base.log"
sed -n '/=== Metricas por proceso ===/,/^$/p' "$TEMPORAL/base.log" | sangrar
grep -E "promedio|Ciclos totales|Uso de CPU" "$TEMPORAL/base.log" | sangrar
nota "response = primera respuesta − llegada · turnaround = fin − llegada · waiting = turnaround − burst"
pausar

# --- 4. Línea de tiempo -------------------------------------------------------

paso "Línea de tiempo: se lee el algoritmo completo"
sed -n '/=== Linea de tiempo/,/^IDLE/p' "$TEMPORAL/base.log" | sangrar
nota "el dígito es el NIVEL DE COLA en que ejecutó: 00 en Q0, 1111 en Q1 tras la democión"
nota "en el ciclo 20 el priority boost devuelve a P3 y P4 a Q0 (vuelven a ejecutar con 0)"
pausar

# --- 5. Traza (Observer) ------------------------------------------------------

paso "Traza de eventos (patrón Observer), primeros ciclos"
comando "./$BIN --trace"
./$BIN --trace --no-csv 2>/dev/null | sed -n '/=== Traza/,$p' | sed -n '2,16p' | sangrar
nota "el motor publica eventos; la traza, el Gantt y las estadísticas son observadores"
pausar

# --- 6. Validación de entradas -----------------------------------------------

paso "Validación de entradas (sin fallos silenciosos)"
comando "./$BIN --input examples/procesos_invalido.csv"
./$BIN --input examples/procesos_invalido.csv >/dev/null 2>"$TEMPORAL/error.log" || CODIGO=$?
sed '/^[[:space:]]*$/d' "$TEMPORAL/error.log" | sangrar
nota "código de salida ${CODIGO:-0}: indica el archivo, la línea, el proceso, el valor y la regla violada"
pausar

# --- 7. Strategy: tres políticas ---------------------------------------------

promedio() { awk -F, -v columna="$2" '$1=="AVG" { print $columna }' "$1"; }

paso "Strategy: el mismo motor con tres políticas distintas"
printf '   %-14s %10s %12s %10s %8s\n' "Política" "Response" "Turnaround" "Waiting" "Cambios"
printf '   %-14s %10s %12s %10s %8s\n' "--------------" "----------" "------------" "----------" "--------"
for entrada in "MLFQ 2/4/8:--policy mlfq" "Round Robin:--policy rr --quantums 2" "FCFS:--policy fcfs"; do
  etiqueta="${entrada%%:*}"; banderas="${entrada#*:}"
  # shellcheck disable=SC2086
  ./$BIN $banderas --output "$TEMPORAL/p.csv" --no-gantt >"$TEMPORAL/p.log" 2>/dev/null
  printf '   %-14s %10s %12s %10s %8s\n' "$etiqueta" \
    "$(promedio "$TEMPORAL/p.csv" 6)" "$(promedio "$TEMPORAL/p.csv" 7)" \
    "$(promedio "$TEMPORAL/p.csv" 8)" \
    "$(awk -F: '/^Cambios de contexto/ { gsub(/ /,"",$2); print $2 }' "$TEMPORAL/p.log")"
done
nota "MLFQ da un tiempo de respuesta 5.8× mejor que FCFS a cambio de 28% de turnaround"
nota "el motor de simulación no se recompiló: la política es una dependencia inyectada"
pausar

# --- 8. OCP: extensiones sin tocar el núcleo ---------------------------------

paso "Abierto/cerrado: extensiones que son banderas, no ediciones de código"
for entrada in \
  "cuarto nivel de cola:--quantums 2,4,8,16" \
  "otra regla de democion:--demote-to-lowest" \
  "otra disciplina de cola:--queue srtf" \
  "sin priority boost:--boost 0" \
  "quantum minimo en Q0:--quantums 1,4,8"; do
  etiqueta="${entrada%%:*}"; banderas="${entrada#*:}"
  # shellcheck disable=SC2086
  ./$BIN $banderas --output "$TEMPORAL/o.csv" --no-gantt >/dev/null 2>&1
  printf '   %-24s %-20s turnaround %s · waiting %s\n' "$etiqueta" "$banderas" \
    "$(promedio "$TEMPORAL/o.csv" 7)" "$(promedio "$TEMPORAL/o.csv" 8)"
done
nota "cinco variaciones de comportamiento, cero modificaciones al motor o a la política"
pausar

# --- 9. Inanición ------------------------------------------------------------

paso "Inanición: P1 (burst=30) contra 25 procesos cortos que llegan cada 2 ciclos"
for intervalo in 0 10; do
  if [ "$intervalo" -eq 0 ]; then
    printf '\n   %sSIN priority boost:%s\n' "$NEGRITA" "$NORMAL"
  else
    printf '\n   %sCON priority boost cada %s ciclos:%s\n' "$NEGRITA" "$intervalo" "$NORMAL"
  fi
  ./$BIN --input examples/starvation.csv --boost "$intervalo" --no-csv >"$TEMPORAL/s.log" 2>/dev/null
  sed -n '/=== Linea de tiempo/,/^IDLE/p' "$TEMPORAL/s.log" \
    | grep -E "^(t|P1) " | sangrar
  espera="$(awk '/espera continua maxima/ && $1=="P1" { print $5 }' "$TEMPORAL/s.log")"
  printf '%s   → P1 estuvo hasta %s ciclos seguidos sin CPU%s\n' "$VERDE" "$espera" "$NORMAL"
done
nota "sin boost la espera no está acotada; con boost queda acotada por S"
pausar

# --- Cierre -------------------------------------------------------------------

paso "Salida exportada: results.csv"
comando "cat results.csv"
sangrar <results.csv
printf '\n'
nota "columnas exactas del enunciado, más una fila AVG con los promedios"

printf '\n%s%s  Fin de la demostración.%s\n' "$NEGRITA" "$VERDE" "$NORMAL"
printf '%s  Más detalle: README.md · DESIGN.md · ANALISIS.md · PRESENTACION.md · COMPILACION.md%s\n' \
  "$APAGADO" "$NORMAL"
printf '%s  Análisis completo con 15 variantes: make experiments%s\n\n' "$APAGADO" "$NORMAL"
