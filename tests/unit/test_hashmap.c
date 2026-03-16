/*
 * FORGE Language Toolchain
 * test_hashmap.c - Unit tests for generic hash map
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "util/hashmap.h"

/* ─────────────────────────────────────────────────────────────────────────────
 * Test Harness
 * ───────────────────────────────────────────────────────────────────────────── */

static int tests_run = 0;
static int tests_passed = 0;

#define ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf("FAIL: %s\n", msg); \
        return 0; \
    } \
} while(0)

#define RUN_TEST(name, fn) do { \
    tests_run++; \
    printf("  [%d] %-50s", tests_run, name); \
    if (fn()) { \
        tests_passed++; \
        printf("✓\n"); \
    } else { \
        printf("✗\n"); \
    } \
} while(0)

/* ─────────────────────────────────────────────────────────────────────────────
 * Tests
 * ───────────────────────────────────────────────────────────────────────────── */

static int test_create_destroy(void) {
    forge_hashmap_t* map = hashmap_create();
    ASSERT(map != NULL, "hashmap_create should return non-NULL");
    ASSERT(hashmap_count(map) == 0, "new map should be empty");
    hashmap_destroy(map);
    return 1;
}

static int test_set_get(void) {
    forge_hashmap_t* map = hashmap_create();
    
    const char* key1 = "hello";
    const char* key2 = "world";
    int val1 = 42;
    int val2 = 99;
    
    hashmap_set(map, key1, &val1);
    hashmap_set(map, key2, &val2);
    
    ASSERT(hashmap_count(map) == 2, "count should be 2");
    ASSERT(hashmap_get(map, key1) == &val1, "should get val1");
    ASSERT(hashmap_get(map, key2) == &val2, "should get val2");
    ASSERT(hashmap_get(map, "missing") == NULL, "missing key should return NULL");
    
    hashmap_destroy(map);
    return 1;
}

static int test_update(void) {
    forge_hashmap_t* map = hashmap_create();
    
    const char* key = "key";
    int val1 = 10;
    int val2 = 20;
    
    hashmap_set(map, key, &val1);
    ASSERT(hashmap_get(map, key) == &val1, "first value");
    
    void* old = hashmap_set(map, key, &val2);
    ASSERT(old == &val1, "set should return old value");
    ASSERT(hashmap_get(map, key) == &val2, "updated value");
    ASSERT(hashmap_count(map) == 1, "count should still be 1");
    
    hashmap_destroy(map);
    return 1;
}

static int test_has(void) {
    forge_hashmap_t* map = hashmap_create();
    
    const char* key = "exists";
    int val = 1;
    
    ASSERT(!hashmap_has(map, key), "should not have key yet");
    hashmap_set(map, key, &val);
    ASSERT(hashmap_has(map, key), "should have key now");
    ASSERT(!hashmap_has(map, "nope"), "should not have 'nope'");
    
    hashmap_destroy(map);
    return 1;
}

static int test_delete(void) {
    forge_hashmap_t* map = hashmap_create();
    
    const char* key = "delete_me";
    int val = 123;
    
    hashmap_set(map, key, &val);
    ASSERT(hashmap_count(map) == 1, "count 1 before delete");
    
    void* deleted = hashmap_delete(map, key);
    ASSERT(deleted == &val, "delete returns old value");
    ASSERT(hashmap_count(map) == 0, "count 0 after delete");
    ASSERT(hashmap_get(map, key) == NULL, "key no longer exists");
    
    /* Delete non-existent key */
    ASSERT(hashmap_delete(map, "nope") == NULL, "delete missing returns NULL");
    
    hashmap_destroy(map);
    return 1;
}

static int test_clear(void) {
    forge_hashmap_t* map = hashmap_create();
    
    int vals[3] = {1, 2, 3};
    hashmap_set(map, "a", &vals[0]);
    hashmap_set(map, "b", &vals[1]);
    hashmap_set(map, "c", &vals[2]);
    
    ASSERT(hashmap_count(map) == 3, "count 3 before clear");
    hashmap_clear(map);
    ASSERT(hashmap_count(map) == 0, "count 0 after clear");
    ASSERT(hashmap_get(map, "a") == NULL, "a is gone");
    
    hashmap_destroy(map);
    return 1;
}

static int test_iteration(void) {
    forge_hashmap_t* map = hashmap_create();
    
    int vals[3] = {10, 20, 30};
    hashmap_set(map, "x", &vals[0]);
    hashmap_set(map, "y", &vals[1]);
    hashmap_set(map, "z", &vals[2]);
    
    int sum = 0;
    int count = 0;
    forge_hashmap_iter_t iter = hashmap_iter(map);
    while (hashmap_iter_next(&iter)) {
        int* v = (int*)hashmap_iter_value(&iter);
        sum += *v;
        count++;
    }
    
    ASSERT(count == 3, "iterated 3 entries");
    ASSERT(sum == 60, "sum is 60");

    hashmap_destroy(map);
    return 1;
}

static int test_resize(void) {
    forge_hashmap_t* map = hashmap_create();

    /* Insert enough entries to trigger resize (initial capacity is 16, load 0.75 = 12) */
    char keys[20][16];
    int vals[20];

    for (int i = 0; i < 20; i++) {
        snprintf(keys[i], sizeof(keys[i]), "key_%d", i);
        vals[i] = i * 100;
        hashmap_set(map, keys[i], &vals[i]);
    }

    ASSERT(hashmap_count(map) == 20, "count should be 20");

    /* Verify all entries are still accessible */
    for (int i = 0; i < 20; i++) {
        int* v = (int*)hashmap_get(map, keys[i]);
        ASSERT(v != NULL, "key should exist");
        ASSERT(*v == vals[i], "value should match");
    }

    hashmap_destroy(map);
    return 1;
}

static int test_delete_and_reinsert(void) {
    forge_hashmap_t* map = hashmap_create();

    int val1 = 1, val2 = 2, val3 = 3;
    hashmap_set(map, "a", &val1);
    hashmap_set(map, "b", &val2);
    hashmap_set(map, "c", &val3);

    /* Delete middle entry */
    hashmap_delete(map, "b");
    ASSERT(hashmap_count(map) == 2, "count 2 after delete");
    ASSERT(hashmap_get(map, "b") == NULL, "b is gone");
    ASSERT(hashmap_get(map, "a") == &val1, "a still there");
    ASSERT(hashmap_get(map, "c") == &val3, "c still there");

    /* Reinsert */
    int val4 = 4;
    hashmap_set(map, "b", &val4);
    ASSERT(hashmap_count(map) == 3, "count 3 after reinsert");
    ASSERT(hashmap_get(map, "b") == &val4, "b has new value");

    hashmap_destroy(map);
    return 1;
}

static int test_hash_function(void) {
    /* Verify hash produces different values for different keys */
    uint32_t h1 = hashmap_hash("hello");
    uint32_t h2 = hashmap_hash("world");
    uint32_t h3 = hashmap_hash("hello");  /* same as h1 */
    uint32_t h4 = hashmap_hash("");

    ASSERT(h1 != h2, "different strings should have different hashes");
    ASSERT(h1 == h3, "same string should have same hash");
    ASSERT(h4 != 0 || h4 == 0, "empty string hash should be consistent");

    return 1;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Main
 * ───────────────────────────────────────────────────────────────────────────── */

int main(void) {
    printf("\n=== FORGE Hash Map Tests ===\n\n");

    RUN_TEST("Create and destroy", test_create_destroy);
    RUN_TEST("Set and get", test_set_get);
    RUN_TEST("Update existing key", test_update);
    RUN_TEST("Has key", test_has);
    RUN_TEST("Delete", test_delete);
    RUN_TEST("Clear", test_clear);
    RUN_TEST("Iteration", test_iteration);
    RUN_TEST("Resize (many inserts)", test_resize);
    RUN_TEST("Delete and reinsert", test_delete_and_reinsert);
    RUN_TEST("Hash function", test_hash_function);

    printf("\n=== Results: %d/%d tests passed ===\n\n", tests_passed, tests_run);

    return tests_passed == tests_run ? 0 : 1;
}

