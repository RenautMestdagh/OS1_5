/**
 * \author Mathieu Erbas
 */

#pragma once

#ifndef _GNU_SOURCE
    #define _GNU_SOURCE
#endif

#include <assert.h>
#include <malloc.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <pthread.h>

typedef uint16_t sensor_id_t;
typedef double sensor_value_t;
typedef time_t sensor_ts_t; // UTC timestamp as returned by time() - notice that the size of time_t is different on 32/64 bit machine

typedef struct {
    sensor_id_t id;
    sensor_value_t value;
    pthread_mutex_t mutex;
    sensor_ts_t ts;
} sensor_data_t;

#ifndef TIMEOUT
    #define TIMEOUT 600
#endif

typedef unsigned int uint;

// stringify preprocessor directives using 2-level preprocessor magic
// this avoids using directives like -DDB_NAME=\"some_db_name\"
#define REAL_TO_STRING(s) #s
#define TO_STRING(s) REAL_TO_STRING(s) // force macro-expansion on s before stringify s

#define ASSERT_ELSE_PERROR(cond)                                                                \
    do {                                                                                        \
        bool x = (cond);                                                                        \
        if (!x) {                                                                               \
            perror("Assertion " #cond " failed");                                               \
            printf("%s:%d: %s: Assertion `%s` failed.\n", __FILE__, __LINE__, __func__, #cond); \
            abort();                                                                            \
        }                                                                                       \
    } while (false)
