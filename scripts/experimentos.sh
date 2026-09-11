#!/usr/bin/env bash
#
# experimentos.sh - Corre las variantes que sustentan la seccion de analisis
# del informe y arma una tabla comparativa en Markdown.
#
# Cada fila cambia UN SOLO parametro sobre la misma carga de trabajo, para que
# la comparacion sea honesta y las conclusiones sean reproducibles.
set -euo pipefail

BIN=build/scheduler
OUT_DIR=docs/resultados
TABLE="$OUT_DIR/comparativa.md"

mkdir -p "$OUT_DIR"
make --no-print-directory >/dev/null

# Columna de la fila AVG del CSV: 6=Response, 7=Turnaround, 8=Waiting.
avg_col() { awk -F, -v col="$2" '$1=="AVG" { print $col }' "$1"; }
waiting_de() { awk -F, -v pid="P$2" '$1==pid { print $8 }' "$1"; }
turnaround_de() { awk -F, -v pid="P$2" '$1==pid { print $7 }' "$1"; }
metrica_txt() { awk -F: -v clave="$2" '$1 ~ clave { gsub(/ /,"",$2); print $2 }' "$1"; }

corre() {
  local etiqueta="$1"; shift
  "$BIN" --label "$etiqueta" --output "$OUT_DIR/${etiqueta}.csv" --no-gantt "$@" \
      >"$OUT_DIR/${etiqueta}.txt" 2>&1
}

fila_a() {
  local etiqueta="$1" descripcion="$2"; shift 2
  corre "$etiqueta" "$@"
  local csv="$OUT_DIR/${etiqueta}.csv" txt="$OUT_DIR/${etiqueta}.txt"
  printf '| %s | %s | %s | %s | %s | %s |\n' \
    "$descripcion" "$(avg_col "$csv" 6)" "$(avg_col "$csv" 7)" "$(avg_col "$csv" 8)" \
    "$(metrica_txt "$txt" '^Cambios de contexto')" \
    "$(metrica_txt "$txt" '^Demociones')" >>"$TABLE"
}

# Espera continua maxima de un PID, leida de la salida del simulador.
espera_maxima_de() {
  awk -v pid="P$2" '/espera continua maxima/ && $1==pid { print $5 }' "$1"
}

fila_b() {
  local etiqueta="$1" descripcion="$2"; shift 2
  corre "$etiqueta" --input examples/starvation.csv "$@"
  local csv="$OUT_DIR/${etiqueta}.csv" txt="$OUT_DIR/${etiqueta}.txt"
  printf '| %s | %s | %s | %s | %s | %s |\n' \
    "$descripcion" "$(espera_maxima_de "$txt" 1)" "$(waiting_de "$csv" 1)" \
    "$(turnaround_de "$csv" 1)" "$(avg_col "$csv" 7)" "$(avg_col "$csv" 8)" >>"$TABLE"
}

{
  echo "<!-- Generado por scripts/experimentos.sh: no editar a mano -->"
  echo
  echo "## A. Escenario del enunciado: P1(0,8) P2(1,4) P3(2,9) P4(3,5)"
  echo
  echo "| Variante | Response prom. | Turnaround prom. | Waiting prom. | Cambios de contexto | Demociones |"
  echo "|---|---|---|---|---|---|"
} >"$TABLE"

fila_a "mlfq-base"      "MLFQ 2/4/8, boost=20 (base)"
fila_a "mlfq-boost-3"   "MLFQ 2/4/8, boost=3 (muy frecuente)"                 --boost 3
fila_a "mlfq-sin-boost" "MLFQ 2/4/8, sin boost"                               --boost 0
fila_a "mlfq-q0-1"      "MLFQ 1/4/8 (quantum de Q0 muy corto)"                --quantums 1,4,8
fila_a "mlfq-q0-8"      "MLFQ 8/8/8 (quantum de Q0 largo)"                    --quantums 8,8,8
fila_a "mlfq-4-niveles" "MLFQ 2/4/8/16 (cuarto nivel, sin tocar el nucleo)"   --quantums 2,4,8,16
fila_a "mlfq-srtf"      "MLFQ 2/4/8 con colas por menor tiempo restante"      --queue srtf
fila_a "mlfq-fondo"     "MLFQ 2/4/8, democion directa al ultimo nivel"        --demote-to-lowest
fila_a "rr-2"           "Round Robin q=2"                                     --policy rr --quantums 2
fila_a "fcfs"           "FCFS"                                                --policy fcfs

{
  echo
  echo "## B. Inanicion: P1 (burst=30) contra 25 procesos cortos que llegan cada 2 ciclos"
  echo
  echo "| Variante | Espera continua max. de P1 | Waiting de P1 | Turnaround de P1 | Turnaround prom. | Waiting prom. |"
  echo "|---|---|---|---|---|---|"
} >>"$TABLE"

fila_b "starv-sin-boost" "MLFQ sin boost"       --boost 0
fila_b "starv-boost-20"  "MLFQ con boost=20"    --boost 20
fila_b "starv-boost-10"  "MLFQ con boost=10"    --boost 10
fila_b "starv-boost-4"   "MLFQ con boost=4"     --boost 4
fila_b "starv-fcfs"      "FCFS (referencia)"    --policy fcfs

echo "Tabla comparativa generada en $TABLE"
cat "$TABLE"
