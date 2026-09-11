#include "cli/cli_options.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

CliOptions cli_options_default(void)
{
    MlfqConfig defaults = mlfq_config_default();
    CliOptions options = {
        .input_kind = INPUT_BUILTIN_SCENARIO,
        .input_path = "",
        .random_count = 8,
        .random_seed = 42u,
        .policy_kind = POLICY_MLFQ,
        .level_count = defaults.level_count,
        .quantums = { 2, 4, 8, 0, 0, 0, 0, 0 },
        .boost_interval = defaults.boost_interval,
        .preempt_on_higher_priority = true,
        .queue_kind = QUEUE_FIFO,
        .demote_to_lowest = false,
        .output_path = "results.csv",
        .write_csv = true,
        .show_trace = false,
        .show_gantt = true,
        .show_help = false,
        .label = "mlfq-base"
    };
    return options;
}

static bool parse_int_argument(const char *value, const char *flag, int *out, Error *error)
{
    if (value == NULL) {
        return error_set(error, ERROR_INVALID_ARGUMENT, "la opcion %s requiere un valor", flag);
    }
    char *end = NULL;
    long parsed = strtol(value, &end, 10);
    if (end == value || *end != '\0') {
        return error_set(error, ERROR_INVALID_ARGUMENT,
                         "valor invalido para %s: '%s' no es un entero", flag, value);
    }
    *out = (int)parsed;
    return true;
}

/* Parsea "2,4,8" en la lista de quantums y deduce el numero de niveles. */
static bool parse_quantums(const char *value, CliOptions *options, Error *error)
{
    if (value == NULL) {
        return error_set(error, ERROR_INVALID_ARGUMENT, "la opcion --quantums requiere valores");
    }
    char buffer[128];
    snprintf(buffer, sizeof(buffer), "%s", value);

    int level_count = 0;
    for (char *token = strtok(buffer, ","); token != NULL; token = strtok(NULL, ",")) {
        if (level_count == MLFQ_MAX_LEVELS) {
            return error_set(error, ERROR_INVALID_ARGUMENT,
                             "demasiados niveles: el maximo es %d", MLFQ_MAX_LEVELS);
        }
        int quantum = 0;
        if (!parse_int_argument(token, "--quantums", &quantum, error)) {
            return false;
        }
        if (quantum <= 0) {
            return error_set(error, ERROR_INVALID_ARGUMENT,
                             "quantum invalido (%d) en --quantums: debe ser > 0", quantum);
        }
        options->quantums[level_count++] = quantum;
    }
    if (level_count == 0) {
        return error_set(error, ERROR_INVALID_ARGUMENT, "--quantums no contiene valores");
    }
    options->level_count = level_count;
    return true;
}

static bool parse_policy(const char *value, CliOptions *options, Error *error)
{
    if (value == NULL) {
        return error_set(error, ERROR_INVALID_ARGUMENT, "--policy requiere un valor");
    }
    if (strcmp(value, "mlfq") == 0) {
        options->policy_kind = POLICY_MLFQ;
    } else if (strcmp(value, "rr") == 0) {
        options->policy_kind = POLICY_ROUND_ROBIN;
    } else if (strcmp(value, "fcfs") == 0) {
        options->policy_kind = POLICY_FCFS;
    } else {
        return error_set(error, ERROR_INVALID_ARGUMENT,
                         "politica desconocida '%s' (use mlfq, rr o fcfs)", value);
    }
    return true;
}

static bool parse_queue(const char *value, CliOptions *options, Error *error)
{
    if (value != NULL && strcmp(value, "fifo") == 0) {
        options->queue_kind = QUEUE_FIFO;
        return true;
    }
    if (value != NULL && strcmp(value, "srtf") == 0) {
        options->queue_kind = QUEUE_SHORTEST_REMAINING;
        return true;
    }
    return error_set(error, ERROR_INVALID_ARGUMENT,
                     "tipo de cola desconocido '%s' (use fifo o srtf)",
                     value == NULL ? "" : value);
}

static void copy_string_argument(char *destination, size_t capacity, const char *value)
{
    snprintf(destination, capacity, "%s", value);
}

bool cli_options_parse(int argc, char **argv, CliOptions *options, Error *error)
{
    *options = cli_options_default();

    for (int i = 1; i < argc; i++) {
        const char *flag = argv[i];
        const char *value = (i + 1 < argc) ? argv[i + 1] : NULL;

        if (strcmp(flag, "--help") == 0 || strcmp(flag, "-h") == 0) {
            options->show_help = true;
            return true;
        } else if (strcmp(flag, "--input") == 0) {
            if (value == NULL) {
                return error_set(error, ERROR_INVALID_ARGUMENT, "--input requiere una ruta");
            }
            options->input_kind = INPUT_CSV_FILE;
            copy_string_argument(options->input_path, sizeof(options->input_path), value);
            i++;
        } else if (strcmp(flag, "--random") == 0) {
            if (!parse_int_argument(value, "--random", &options->random_count, error)) {
                return false;
            }
            options->input_kind = INPUT_RANDOM;
            i++;
        } else if (strcmp(flag, "--seed") == 0) {
            int seed = 0;
            if (!parse_int_argument(value, "--seed", &seed, error)) {
                return false;
            }
            options->random_seed = (unsigned int)seed;
            i++;
        } else if (strcmp(flag, "--policy") == 0) {
            if (!parse_policy(value, options, error)) {
                return false;
            }
            i++;
        } else if (strcmp(flag, "--quantums") == 0) {
            if (!parse_quantums(value, options, error)) {
                return false;
            }
            i++;
        } else if (strcmp(flag, "--boost") == 0) {
            if (!parse_int_argument(value, "--boost", &options->boost_interval, error)) {
                return false;
            }
            if (options->boost_interval < 0) {
                return error_set(error, ERROR_INVALID_ARGUMENT,
                                 "--boost no puede ser negativo (use 0 para desactivarlo)");
            }
            i++;
        } else if (strcmp(flag, "--queue") == 0) {
            if (!parse_queue(value, options, error)) {
                return false;
            }
            i++;
        } else if (strcmp(flag, "--output") == 0) {
            if (value == NULL) {
                return error_set(error, ERROR_INVALID_ARGUMENT, "--output requiere una ruta");
            }
            copy_string_argument(options->output_path, sizeof(options->output_path), value);
            options->write_csv = true;
            i++;
        } else if (strcmp(flag, "--label") == 0) {
            if (value == NULL) {
                return error_set(error, ERROR_INVALID_ARGUMENT, "--label requiere un texto");
            }
            copy_string_argument(options->label, sizeof(options->label), value);
            i++;
        } else if (strcmp(flag, "--no-csv") == 0) {
            options->write_csv = false;
        } else if (strcmp(flag, "--trace") == 0) {
            options->show_trace = true;
        } else if (strcmp(flag, "--no-gantt") == 0) {
            options->show_gantt = false;
        } else if (strcmp(flag, "--no-preempt") == 0) {
            options->preempt_on_higher_priority = false;
        } else if (strcmp(flag, "--demote-to-lowest") == 0) {
            options->demote_to_lowest = true;
        } else {
            return error_set(error, ERROR_INVALID_ARGUMENT,
                             "opcion desconocida '%s' (use --help)", flag);
        }
    }

    if (options->policy_kind != POLICY_MLFQ && options->level_count > 1) {
        /* rr/fcfs solo usan el primer quantum: se avisa en vez de ignorar en
         * silencio una opcion del usuario. */
        fprintf(stderr, "Aviso: la politica seleccionada usa una sola cola; "
                        "se aplica el quantum %d.\n", options->quantums[0]);
    }
    return true;
}

void cli_options_print_usage(const char *program_name)
{
    printf("Simulador de scheduler MLFQ\n\n");
    printf("Uso: %s [opciones]\n\n", program_name);
    printf("Entrada de procesos:\n");
    printf("  --input <archivo.csv>   Carga procesos desde un CSV (pid,arrival,burst)\n");
    printf("  --random <n>            Genera n procesos sinteticos\n");
    printf("  --seed <n>              Semilla para --random (por defecto 42)\n");
    printf("  (sin opciones)          Usa el escenario del enunciado P1..P4\n\n");
    printf("Politica de planificacion:\n");
    printf("  --policy mlfq|rr|fcfs   Politica a simular (por defecto mlfq)\n");
    printf("  --quantums 2,4,8        Quantum por nivel; define el numero de niveles\n");
    printf("  --boost <s>             Priority boost cada s ciclos (0 = desactivado)\n");
    printf("  --queue fifo|srtf       Disciplina interna de cada cola\n");
    printf("  --no-preempt            No expropiar cuando llega un proceso de mayor prioridad\n");
    printf("  --demote-to-lowest      Regla de democion alternativa: baja al ultimo nivel\n\n");
    printf("Salidas:\n");
    printf("  --output <archivo.csv>  Ruta del CSV de resultados (por defecto results.csv)\n");
    printf("  --no-csv                No escribir el CSV\n");
    printf("  --trace                 Imprimir la traza de eventos ciclo a ciclo\n");
    printf("  --no-gantt              No imprimir la linea de tiempo\n");
    printf("  --label <texto>         Etiqueta del experimento en el encabezado\n");
    printf("  --help                  Esta ayuda\n");
}

void cli_options_print_summary(const CliOptions *options)
{
    printf("=== Configuracion (%s) ===\n", options->label);
    printf("Politica   : %s\n", options->policy_kind == POLICY_MLFQ ? "MLFQ"
                                : options->policy_kind == POLICY_ROUND_ROBIN ? "Round Robin" : "FCFS");
    if (options->policy_kind == POLICY_MLFQ) {
        printf("Niveles    : %d (quantums:", options->level_count);
        for (int level = 0; level < options->level_count; level++) {
            printf(" Q%d=%d", level, options->quantums[level]);
        }
        printf(")\n");
        printf("Boost      : %s\n", options->boost_interval > 0 ? "cada S ciclos" : "desactivado");
        if (options->boost_interval > 0) {
            printf("S          : %d ciclos\n", options->boost_interval);
        }
        printf("Democion   : %s\n", options->demote_to_lowest ? "directo al ultimo nivel"
                                                             : "un nivel por vez");
        printf("Expropiar  : %s\n", options->preempt_on_higher_priority ? "si" : "no");
    } else {
        printf("Quantum    : %d\n", options->quantums[0]);
    }
    printf("Colas      : %s\n", options->queue_kind == QUEUE_FIFO ? "FIFO" : "menor tiempo restante");
}
