/*
 * ports.c - Envoltorios de los puertos de la capa de aplicacion.
 *
 * Centralizan la validacion de punteros nulos para que ninguna implementacion
 * concreta tenga que repetirla (DRY).
 */
#include "application/ports/process_source.h"
#include "application/ports/results_writer.h"

const char *process_source_describe(const ProcessSource *source)
{
    return source == NULL ? "(fuente nula)" : source->vtable->describe(source);
}

bool process_source_load(ProcessSource *source, ProcessTable *table, Error *error)
{
    if (source == NULL || table == NULL) {
        return error_set(error, ERROR_INVALID_ARGUMENT, "fuente de procesos o tabla nula");
    }
    return source->vtable->load(source, table, error);
}

void process_source_destroy(ProcessSource *source)
{
    if (source != NULL) {
        source->vtable->destroy(source);
    }
}

const char *results_writer_describe(const ResultsWriter *writer)
{
    return writer == NULL ? "(salida nula)" : writer->vtable->describe(writer);
}

bool results_writer_write(ResultsWriter *writer, const MetricsReport *report, Error *error)
{
    if (writer == NULL || report == NULL) {
        return error_set(error, ERROR_INVALID_ARGUMENT, "escritor de resultados o informe nulo");
    }
    return writer->vtable->write(writer, report, error);
}

void results_writer_destroy(ResultsWriter *writer)
{
    if (writer != NULL) {
        writer->vtable->destroy(writer);
    }
}
