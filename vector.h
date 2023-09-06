typedef struct Vector Vector;

struct Vector {
    int size;
    int capacity;
    void **ptr;
};

Vector *new_vector();
void vector_push(Vector *vec, void *x);
void *vector_pop(Vector *vec);
void *vector_get(Vector *vec, int i);
void vector_set(Vector *vec, int i, void *x);
int vector_size(Vector *vec);
void vector_remove(Vector *vec, void *x);
void *vector_last(Vector *vec);
Vector *vector_dup(Vector *orig);
