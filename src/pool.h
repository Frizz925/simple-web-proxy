#include <stdlib.h>

typedef struct {
    void *base, *head;
} pool_t;

void pool_init(pool_t *pool, size_t size, size_t count);
void *pool_allocate(pool_t *pool);
void pool_deallocate(pool_t *pool, void *ptr);
void pool_deinit(pool_t *pool);