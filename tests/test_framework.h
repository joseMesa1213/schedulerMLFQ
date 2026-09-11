/*
 * test_framework.h - Micro framework de pruebas (sin dependencias externas).
 *
 * Se mantiene deliberadamente minimo: el enunciado pide pruebas que validen
 * metricas y logica de democion/boost, no una libreria de testing.
 */
#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

extern int g_checks_run;
extern int g_checks_failed;
extern const char *g_current_test;

#define TEST_BEGIN(name) \
    do { g_current_test = (name); printf("  - %s\n", (name)); } while (0)

#define CHECK(condition, description)                                              \
    do {                                                                           \
        g_checks_run++;                                                            \
        if (!(condition)) {                                                        \
            g_checks_failed++;                                                     \
            printf("    FALLO [%s] %s (%s:%d)\n", g_current_test, (description),   \
                   __FILE__, __LINE__);                                            \
        }                                                                          \
    } while (0)

#define CHECK_INT_EQ(actual, expected, description)                                \
    do {                                                                           \
        g_checks_run++;                                                            \
        long long actual_value = (long long)(actual);                              \
        long long expected_value = (long long)(expected);                          \
        if (actual_value != expected_value) {                                      \
            g_checks_failed++;                                                     \
            printf("    FALLO [%s] %s: se esperaba %lld y se obtuvo %lld (%s:%d)\n",\
                   g_current_test, (description), expected_value, actual_value,    \
                   __FILE__, __LINE__);                                            \
        }                                                                          \
    } while (0)

#define CHECK_DOUBLE_EQ(actual, expected, description)                             \
    do {                                                                           \
        g_checks_run++;                                                            \
        double difference = (double)(actual) - (double)(expected);                 \
        if (difference > 0.0001 || difference < -0.0001) {                          \
            g_checks_failed++;                                                     \
            printf("    FALLO [%s] %s: se esperaba %.4f y se obtuvo %.4f (%s:%d)\n",\
                   g_current_test, (description), (double)(expected),              \
                   (double)(actual), __FILE__, __LINE__);                          \
        }                                                                          \
    } while (0)

#endif /* TEST_FRAMEWORK_H */
