/*
 * CS472 HW 2
 * Jason R. Carrete
 * vector.c
 *
 * This module implements a simple vector or growable array for characters.
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "vector.h"
#include "log.h"

void vector_create(struct vector *vec, const size_t init_cap, const double scale) {
    assert(vec);
    if (vec) {
        vec->capacity = init_cap;
        vec->size = 0;
        vec->scale = scale;
        vec->arr = (char *) calloc(init_cap, sizeof(char));
        if (!(vec->arr)) {
            logerr("Failed to allocate memory for vector");
            puts("A fatal error occurred. See log for details");
            exit(EXIT_FAILURE);
        }
    }
}

void vector_append(struct vector *vec, const char ch) {
    assert(vec);
    if (vec->size == vec->capacity) {
        vec->capacity = (size_t) (vec->capacity * vec->scale);
        vec->arr = reallocarray(vec->arr, vec->capacity, sizeof *(vec->arr));
        if (!(vec->arr)) {
            logerr("Failed to reallocate memory for vector");
            puts("A fatal error occurred. See log for details");
            exit(EXIT_FAILURE);
        }
    }
    vec->arr[(vec->size)++] = ch;
}

void vector_append_str(struct vector *vec, const char *str) {
    assert(vec);
    for (const char *p = str; *p; p++) {
        vector_append(vec, *p);
    }
}

void vector_free(struct vector *vec) {
    assert(vec);
    free(vec->arr);
}
