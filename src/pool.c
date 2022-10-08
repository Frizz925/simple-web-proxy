#include "pool.h"

struct pool_node_s {
    struct pool_node_s *next;
};

typedef struct pool_node_s pool_node_t;

void pool_init(pool_t *pool, size_t size, size_t count) {
    size_t sz_node = sizeof(pool_node_t) + size;
    pool_node_t *node = calloc(count, sz_node);
    pool->base = pool->head = node;

    for (int i = 1; i < count; i++) {
        void *base = node;
        node->next = base + sz_node;
        node = node->next;
    }
    node->next = NULL;
}

void *pool_allocate(pool_t *pool) {
    void *base = pool->head;
    if (!base) return NULL;
    pool->head = ((pool_node_t *)base)->next;
    return base + sizeof(pool_node_t);
}

void pool_deallocate(pool_t *pool, void *ptr) {
    pool_node_t *node = (pool_node_t *)(ptr - sizeof(pool_node_t));
    if (pool->head) node->next = pool->head;
    else node->next = NULL;
    pool->head = node;
}

void pool_deinit(pool_t *pool) {
    free(pool->base);
}