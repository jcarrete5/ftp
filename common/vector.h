/*
 * CS472 HW 2
 * Jason R. Carrete
 * vector.h
 *
 * Header for vector.c
 */

#ifndef COMMON_VECTOR_H
#define COMMON_VECTOR_H

#include <stddef.h>

struct vector {
    /* Number of elements allocated for this vector. */
    size_t capacity;
    /* Number of elements in this vector. */
    size_t size;
    /* Factor at which the array grows when resized. */
    double scale;
    char *arr;
};

void vector_create(struct vector *vec, const size_t init_capacity, const double scale);

void vector_append(struct vector *vec, const char ch);

/*
 * Append a null-terminated string to the vector.
 * Precondition: str must be non-NULL.
 */
void vector_append_str(struct vector *vec, const char *str);

void vector_free(struct vector *vec);

#endif /* COMMON_VECTOR_H */
