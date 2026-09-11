/*
 * Pruebas de validacion de entradas: el enunciado exige mensajes claros y no
 * fallos silenciosos ante archivos mal formados.
 */
#include <stdio.h>

#include "infrastructure/process_sources.h"
#include "test_framework.h"
#include "test_suites.h"

static const char *kTempPath = "build/test_procesos.csv";

static void write_temp_file(const char *content)
{
    FILE *file = fopen(kTempPath, "w");
    if (file != NULL) {
        fputs(content, file);
        fclose(file);
    }
}

static bool load_temp_file(const char *content, Error *error)
{
    write_temp_file(content);
    error_clear(error);
    ProcessTable *table = process_table_create(error);
    ProcessSource *source = csv_process_source_create(kTempPath, error);
    bool ok = process_source_load(source, table, error);
    process_source_destroy(source);
    process_table_destroy(table);
    return ok;
}

static void test_csv_valido(void)
{
    TEST_BEGIN("lee un CSV valido con cabecera y comentarios");
    Error error;
    error_clear(&error);
    write_temp_file("pid,arrival_time,burst_time\n# comentario\n1,0,8\n2,1,4\n\n3,2,9\n");

    ProcessTable *table = process_table_create(&error);
    ProcessSource *source = csv_process_source_create(kTempPath, &error);
    CHECK(process_source_load(source, table, &error), "carga sin errores");
    CHECK_INT_EQ(process_table_size(table), 3, "tres procesos");
    CHECK(process_table_find(table, 2) != NULL, "encuentra P2");
    process_source_destroy(source);
    process_table_destroy(table);
}

/* Caso de regresion: la cabecera puede venir despues de lineas de comentario,
 * pero una letra en medio de los datos sigue siendo un error de formato. */
static void test_cabecera_despues_de_comentarios(void)
{
    TEST_BEGIN("acepta la cabecera aunque este precedida por comentarios");
    Error error;
    error_clear(&error);
    write_temp_file("# comentario 1\n# comentario 2\npid,arrival_time,burst_time\n1,0,8\n2,1,4\n");

    ProcessTable *table = process_table_create(&error);
    ProcessSource *source = csv_process_source_create(kTempPath, &error);
    CHECK(process_source_load(source, table, &error), "carga sin errores");
    CHECK_INT_EQ(process_table_size(table), 2, "dos procesos");
    process_source_destroy(source);
    process_table_destroy(table);

    CHECK(!load_temp_file("1,0,8\npid,arrival,burst\n2,1,4\n", &error),
          "una cabecera en medio de los datos si es un error");
}

static void test_entradas_invalidas(void)
{
    TEST_BEGIN("rechaza entradas invalidas indicando la linea");
    Error error;

    CHECK(!load_temp_file("1,0,-5\n", &error), "burst negativo falla");
    CHECK(strstr(error.message, "linea 1") != NULL, "el mensaje indica la linea");

    CHECK(!load_temp_file("1,0,8\n2,x,4\n", &error), "texto donde va un entero falla");
    CHECK(strstr(error.message, "linea 2") != NULL, "indica la linea 2");

    CHECK(!load_temp_file("1,0\n", &error), "faltan columnas falla");
    CHECK(strstr(error.message, "burst_time") != NULL, "nombra la columna ausente");

    CHECK(!load_temp_file("1,0,8\n1,2,3\n", &error), "PID duplicado falla");
    CHECK_INT_EQ(error.code, ERROR_DUPLICATED_PID, "codigo de PID duplicado");

    CHECK(!load_temp_file("# solo comentarios\n", &error), "archivo sin procesos falla");

    error_clear(&error);
    ProcessTable *table = process_table_create(&error);
    ProcessSource *missing = csv_process_source_create("build/no_existe_jamas.csv", &error);
    CHECK(!process_source_load(missing, table, &error), "archivo inexistente falla");
    CHECK_INT_EQ(error.code, ERROR_IO, "reporta error de E/S");
    process_source_destroy(missing);
    process_table_destroy(table);
}

static void test_fuente_aleatoria_reproducible(void)
{
    TEST_BEGIN("la fuente aleatoria es reproducible con la misma semilla");
    Error error;
    error_clear(&error);

    int first_bursts[5];
    for (int repetition = 0; repetition < 2; repetition++) {
        ProcessTable *table = process_table_create(&error);
        ProcessSource *source = random_process_source_create(5, 7u, 10, 2, 12, &error);
        CHECK(process_source_load(source, table, &error), "genera la carga");
        for (size_t i = 0; i < process_table_size(table); i++) {
            int burst = process_burst_time(process_table_at(table, i));
            if (repetition == 0) {
                first_bursts[i] = burst;
                CHECK(burst >= 2 && burst <= 12, "la rafaga respeta el rango pedido");
            } else {
                CHECK_INT_EQ(burst, first_bursts[i], "misma semilla, misma carga");
            }
        }
        process_source_destroy(source);
        process_table_destroy(table);
    }

    error_clear(&error);
    CHECK(random_process_source_create(0, 1u, 5, 1, 3, &error) == NULL, "cero procesos es invalido");
    error_clear(&error);
    CHECK(random_process_source_create(3, 1u, 5, 5, 2, &error) == NULL, "rango de rafaga invertido es invalido");
}

void suite_process_sources(void)
{
    test_csv_valido();
    test_cabecera_despues_de_comentarios();
    test_entradas_invalidas();
    test_fuente_aleatoria_reproducible();
}
