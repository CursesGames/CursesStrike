#pragma once

// Note: this include is a beta feature for design- and compile-time
#include "../liblinux_util/mscfix.h"

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifndef max
#define max(a,b) ((a) > (b) ? (a) : (b))
#endif

// Vector growth factor, multiplied to 1024 for faster processing w/o floats
#define VECTOR_GROWTH_FACTOR 1536

// Datatype for vector objects
typedef uint64_t __vector_val_t;

// A simple array-like collection with ability to grow, containing (by default) pointer-size integers
typedef struct __vector {
	__vector_val_t *array;
	size_t size;
	size_t capacity;
	uint32_t _growth_factor; 
} VECTOR;

bool vector_init(VECTOR *vect, size_t initial_capacity);
// Pushes `value' to the end of vector. Value is _copied_.
bool vector_push_back(VECTOR *vect, __vector_val_t value);
bool vector_pop_back(VECTOR *vect, __vector_val_t *out_value);
bool vector_shrink_to_fit(VECTOR *vect);
__vector_val_t *vector_array_ptr(VECTOR *vect);
void vector_free(VECTOR *vect);
