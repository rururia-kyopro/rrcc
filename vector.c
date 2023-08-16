#include <stdlib.h>
#include <assert.h>
#include "vector.h"

static void vector_ensure(Vector *vec, int size);

Vector *new_vector() {
    Vector *vec = calloc(1, sizeof(Vector));
    return vec;
}

void vector_push(Vector *vec, void *x) {
    vector_ensure(vec, vec->size + 1);
    vec->ptr[vec->size] = x;
    vec->size++;
}

void *vector_pop(Vector *vec) {
    assert(vec->size != 0);
    vec->size--;
    return vec->ptr[vec->size];
}

void *vector_get(Vector *vec, int i) {
    return vec->ptr[i];
}

int vector_size(Vector *vec) {
    return vec->size;
}

static void vector_ensure(Vector *vec, int size) {
    if(vec->capacity >= size) {
        return;
    }
    vec->capacity = vec->capacity;
    if(vec->capacity < size) {
        vec->capacity = size;
    }
    vec->ptr = realloc(vec->ptr, sizeof(void *) * vec->capacity);
}
