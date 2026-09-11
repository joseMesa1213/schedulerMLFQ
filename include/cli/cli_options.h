/*
 * cli_options.h - Traduccion de argumentos de linea de comandos a una
 * configuracion validada.
 *
 * Es un adaptador de entrada mas: aisla el `argv` (detalle de la consola) del
 * resto del programa. main() no vuelve a mirar cadenas.
 */
#ifndef CLI_CLI_OPTIONS_H
#define CLI_CLI_OPTIONS_H

#include <stdbool.h>

#include "domain/error.h"
#include "domain/policies/mlfq_policy.h"

typedef enum { INPUT_BUILTIN_SCENARIO, INPUT_CSV_FILE, INPUT_RANDOM } InputKind;
typedef enum { POLICY_MLFQ, POLICY_ROUND_ROBIN, POLICY_FCFS } PolicyKind;
typedef enum { QUEUE_FIFO, QUEUE_SHORTEST_REMAINING } QueueKind;

typedef struct {
    InputKind input_kind;
    char input_path[512];
    int random_count;
    unsigned int random_seed;

    PolicyKind policy_kind;
    int level_count;
    int quantums[MLFQ_MAX_LEVELS];
    int boost_interval;              /* 0 => sin boost */
    bool preempt_on_higher_priority;
    QueueKind queue_kind;
    bool demote_to_lowest;           /* regla de democion alternativa */

    char output_path[512];
    bool write_csv;
    bool show_trace;
    bool show_gantt;
    bool show_help;
    char label[64];                  /* etiqueta para los experimentos */
} CliOptions;

CliOptions cli_options_default(void);
bool cli_options_parse(int argc, char **argv, CliOptions *options, Error *error);
void cli_options_print_usage(const char *program_name);
void cli_options_print_summary(const CliOptions *options);

#endif /* CLI_CLI_OPTIONS_H */
