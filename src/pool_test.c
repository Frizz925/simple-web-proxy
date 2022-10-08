#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "pool.h"

#define POOL_TEST_MESSAGE "Hello, world!"
#define POOL_TEST_COUNT 16

typedef struct {
    int index;
    char message[16];
} pool_test_payload_t;

int main() {
    pool_t pool;
    pool_init(&pool, sizeof(pool_test_payload_t), POOL_TEST_COUNT);

    pool_test_payload_t *payloads[POOL_TEST_COUNT];
    for (int i = 0; i < POOL_TEST_COUNT; i++) {
        pool_test_payload_t *payload = (pool_test_payload_t *)pool_allocate(&pool);
        assert(payload);
        strcpy(payload->message, POOL_TEST_MESSAGE);
        payload->index = i;
        payloads[i] = payload;
    }
    assert(!pool_allocate(&pool));

    for (int i = 0; i < POOL_TEST_COUNT; i++) {
        pool_test_payload_t *payload = payloads[i];
        assert(payload->index == i);
        assert(!strcmp(payload->message, POOL_TEST_MESSAGE));
        pool_deallocate(&pool, payload);
    }

    pool_deinit(&pool);
}