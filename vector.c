#include <stdlib.h>
#include <assert.h>
#include <string.h>
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

void vector_remove(Vector *vec, void *x) {
    for(int i = 0; i < vector_size(vec); i++) {
        if(vector_get(vec, i) == x) {
            memmove(vec->ptr + i, vec->ptr + i + 1, (vec->size - i - 1) * sizeof(void *));
            vec->size--;
            return;
        }
    }
}

Vector *vector_dup(Vector *orig) {
    Vector *vec = new_vector();
    vec->size = orig->size;
    if(vec->size) {
        vec->ptr = calloc(1, orig->size * sizeof(void *));
        memcpy(vec->ptr, orig->ptr, orig->size * sizeof(void *));
    }
    return vec;
}
