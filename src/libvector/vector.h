#pragma once

// Note: this include is a beta feature for design- and compile-time
#include "../liblinux_util/mscfix.h"

#include <stdint.h>
#include <stdbool.h>
// ReSharper disable once CppUnusedIncludeDirective
#include <stddef.h>

// макросы всем!
#ifndef min
#define min(a,b) ((a) < (b) ? (a) : (b))
#endif

#ifndef max
#define max(a,b) ((a) > (b) ? (a) : (b))
#endif

// Vector growth factor, multiplied to 1024 for faster processing w/o floats
#define VECTOR_GROWTH_FACTOR 1536

// Datatype for vector objects
typedef union __vector_val_t {
    uint64_t lng;
    void *ptr;
} VECTOR_VALTYPE;

// A simple array-like collection with ability to grow, containing (by default) pointer-size integers
typedef struct __vector {
    // pointer to internal array
    VECTOR_VALTYPE *array;
    // actual count of items
    size_t size;
    // reserved count
    size_t capacity;
    // how many new place vector will allocate if size >= capacity
    uint32_t _growth_factor;
} VECTOR;

// Initializes vector, allocating memory ahead for `initial_capacity' elems
bool vector_init(VECTOR *vect, size_t initial_capacity);
// Pushes `value' to the end of vector. Value is _copied_.
bool vector_push_back(VECTOR *vect, VECTOR_VALTYPE value);
// Copies value from the end of vector and decrement size
bool vector_pop_back(VECTOR *vect, VECTOR_VALTYPE *out_value);
// Cut unoccupied memory
bool vector_shrink_to_fit(VECTOR *vect);
// Returns pointer to array of items
VECTOR_VALTYPE *vector_array_ptr(VECTOR *vect);
// Free memory (without zeroing out size and capacity)
void vector_free(VECTOR *vect);
