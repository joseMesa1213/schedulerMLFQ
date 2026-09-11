<!-- Generado por scripts/experimentos.sh: no editar a mano -->

## A. Escenario del enunciado: P1(0,8) P2(1,4) P3(2,9) P4(3,5)

| Variante | Response prom. | Turnaround prom. | Waiting prom. | Cambios de contexto | Demociones |
|---|---|---|---|---|---|
| MLFQ 2/4/8, boost=20 (base) | 1.50 | 19.50 | 13.00 | 10 | 7 |
| MLFQ 2/4/8, boost=3 (muy frecuente) | 1.50 | 19.00 | 12.50 | 13 | 10 |
| MLFQ 2/4/8, sin boost | 1.50 | 19.50 | 13.00 | 10 | 6 |
| MLFQ 1/4/8 (quantum de Q0 muy corto) | 0.00 | 18.25 | 11.75 | 12 | 8 |
| MLFQ 8/8/8 (quantum de Q0 largo) | 8.50 | 16.25 | 9.75 | 5 | 1 |
| MLFQ 2/4/8/16 (cuarto nivel, sin tocar el nucleo) | 1.50 | 19.50 | 13.00 | 10 | 7 |
| MLFQ 2/4/8 con colas por menor tiempo restante | 1.50 | 16.75 | 10.25 | 10 | 6 |
| MLFQ 2/4/8, democion directa al ultimo nivel | 1.50 | 18.75 | 12.25 | 10 | 6 |
| Round Robin q=2 | 2.00 | 19.25 | 12.75 | 13 | 0 |
| FCFS | 8.75 | 15.25 | 8.75 | 4 | 0 |

## B. Inanicion: P1 (burst=30) contra 25 procesos cortos que llegan cada 2 ciclos

| Variante | Espera continua max. de P1 | Waiting de P1 | Turnaround de P1 | Turnaround prom. | Waiting prom. |
|---|---|---|---|---|---|
| MLFQ sin boost | 50 | 50 | 80 | 5.96 | 2.88 |
| MLFQ con boost=20 | 20 | 50 | 80 | 7.50 | 4.42 |
| MLFQ con boost=10 | 10 | 50 | 80 | 9.81 | 6.73 |
| MLFQ con boost=4 | 12 | 50 | 80 | 12.27 | 9.19 |
| FCFS (referencia) | 0 | 0 | 30 | 30.96 | 27.88 |
