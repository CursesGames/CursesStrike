// Note: this include is a beta feature for design- and compile-time
#include "../liblinux_util/mscfix.h"

#include <stdlib.h>
#include "vector.h"

#ifdef VALGRIND_SUCKS
//#pragma message "Yeah, valgrind sucks on realloc()! We will try to work it out."
#define realloc(ptr,size) __force_realloc(ptr,size)
#include <string.h>

void *__force_realloc(void *ptr, size_t size) {
    void *mem = malloc(size);
    if(mem == NULL)
        return NULL; // could not malloc more
    memcpy(mem, ptr, size);
    free(ptr);
    return mem;
}
#endif

bool vector_init(VECTOR *vect, size_t initial_capacity) {
    if ((vect->array = (VECTOR_VALTYPE*)malloc(sizeof(VECTOR_VALTYPE) * initial_capacity)) == NULL)
        return false;
    vect->_growth_factor = VECTOR_GROWTH_FACTOR;
    vect->size = 0;
    vect->capacity = initial_capacity;
    return true;
}

bool vector_resize(VECTOR *vect, size_t new_capacity) {
    if (new_capacity == vect->capacity)
        return true; // nothing to be done
    if (new_capacity == 0) // чтобы не был совсем уж пустой вектор, это вызывает ошибку realloc
        new_capacity = 1;
    void *mem = realloc(vect->array, sizeof(VECTOR_VALTYPE) * new_capacity);
    if (mem == NULL)
        return false; // could not realloc
    vect->array = mem;
    vect->capacity = new_capacity;
    return true;
}

bool vector_push_back(VECTOR *vect, VECTOR_VALTYPE value) {
    if (vect->size >= vect->capacity) {
        if (!vector_resize(vect, max(vect->size + 1, vect->capacity * vect->_growth_factor / 1024)))
            return false;
    }
    vect->array[vect->size] = value;
    ++vect->size;
    return true;
}

bool vector_pop_back(VECTOR *vect, VECTOR_VALTYPE *out_value) {
    if (vect->size == 0)
        return false;
    --vect->size;
    *out_value = vect->array[vect->size];
    return true;
}

bool vector_shrink_to_fit(VECTOR *vect) {
    // version 1. smart realloc
    if (!vector_resize(vect, vect->size))
        return false;
    vect->capacity = vect->size;
    return true;
}

VECTOR_VALTYPE *vector_array_ptr(VECTOR *vect) {
    return vect->array;
}

void vector_clear_fast(VECTOR *vect) {
    vect->size = 0;
}

void vector_free(VECTOR *vect) {
    free(vect->array);
}
