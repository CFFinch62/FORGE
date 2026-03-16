/*
 * FORGE Language Toolchain
 * test_dynarray.c - Unit tests for dynamic array
 */

#include <stdio.h>
#include <string.h>
#include "util/dynarray.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) static void test_##name(void)
#define RUN_TEST(name) do { \
    printf("  %-40s", #name); \
    test_##name(); \
    printf(" PASS\n"); \
    tests_passed++; \
} while(0)

#define ASSERT(cond) do { \
    if (!(cond)) { \
        printf(" FAIL\n    Assertion failed: %s\n", #cond); \
        tests_failed++; \
        return; \
    } \
} while(0)

/* Define test array types */
DYNARRAY_DEFINE(test_int_array, int)
DYNARRAY_DEFINE(test_str_array, const char*)

/* ─────────────────────────────────────────────────────────────────────────────
 * Tests
 * ───────────────────────────────────────────────────────────────────────────── */

TEST(init_empty) {
    test_int_array arr = {0};
    ASSERT(arr.data == NULL);
    ASSERT(arr.len == 0);
    ASSERT(arr.cap == 0);
    ASSERT(test_int_array_empty(&arr));
}

TEST(push_single) {
    test_int_array arr = {0};
    test_int_array_push(&arr, 42);
    ASSERT(arr.len == 1);
    ASSERT(test_int_array_get(&arr, 0) == 42);
    test_int_array_free(&arr);
}

TEST(push_multiple) {
    test_int_array arr = {0};
    for (int i = 0; i < 100; i++) {
        test_int_array_push(&arr, i * 10);
    }
    ASSERT(arr.len == 100);
    for (int i = 0; i < 100; i++) {
        ASSERT(test_int_array_get(&arr, i) == i * 10);
    }
    test_int_array_free(&arr);
}

TEST(get_ptr) {
    test_int_array arr = {0};
    test_int_array_push(&arr, 42);
    int* ptr = test_int_array_get_ptr(&arr, 0);
    ASSERT(*ptr == 42);
    *ptr = 100;
    ASSERT(test_int_array_get(&arr, 0) == 100);
    test_int_array_free(&arr);
}

TEST(set) {
    test_int_array arr = {0};
    test_int_array_push(&arr, 1);
    test_int_array_push(&arr, 2);
    test_int_array_push(&arr, 3);
    test_int_array_set(&arr, 1, 99);
    ASSERT(test_int_array_get(&arr, 1) == 99);
    test_int_array_free(&arr);
}

TEST(pop) {
    test_int_array arr = {0};
    test_int_array_push(&arr, 1);
    test_int_array_push(&arr, 2);
    test_int_array_push(&arr, 3);
    ASSERT(test_int_array_pop(&arr) == 3);
    ASSERT(arr.len == 2);
    ASSERT(test_int_array_pop(&arr) == 2);
    ASSERT(arr.len == 1);
    test_int_array_free(&arr);
}

TEST(clear) {
    test_int_array arr = {0};
    test_int_array_push(&arr, 1);
    test_int_array_push(&arr, 2);
    test_int_array_clear(&arr);
    ASSERT(arr.len == 0);
    ASSERT(arr.cap > 0);  /* Capacity preserved */
    test_int_array_free(&arr);
}

TEST(free_resets) {
    test_int_array arr = {0};
    test_int_array_push(&arr, 1);
    test_int_array_free(&arr);
    ASSERT(arr.data == NULL);
    ASSERT(arr.len == 0);
    ASSERT(arr.cap == 0);
}

TEST(string_array) {
    test_str_array arr = {0};
    test_str_array_push(&arr, "hello");
    test_str_array_push(&arr, "world");
    ASSERT(arr.len == 2);
    ASSERT(strcmp(test_str_array_get(&arr, 0), "hello") == 0);
    ASSERT(strcmp(test_str_array_get(&arr, 1), "world") == 0);
    test_str_array_free(&arr);
}

TEST(capacity_growth) {
    test_int_array arr = {0};
    /* Push enough to trigger multiple resizes */
    for (int i = 0; i < 1000; i++) {
        test_int_array_push(&arr, i);
    }
    ASSERT(arr.len == 1000);
    ASSERT(arr.cap >= 1000);
    /* Verify all values correct */
    for (int i = 0; i < 1000; i++) {
        ASSERT(test_int_array_get(&arr, i) == i);
    }
    test_int_array_free(&arr);
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Main
 * ───────────────────────────────────────────────────────────────────────────── */

int main(void) {
    printf("=== Dynamic Array Unit Tests ===\n\n");
    
    RUN_TEST(init_empty);
    RUN_TEST(push_single);
    RUN_TEST(push_multiple);
    RUN_TEST(get_ptr);
    RUN_TEST(set);
    RUN_TEST(pop);
    RUN_TEST(clear);
    RUN_TEST(free_resets);
    RUN_TEST(string_array);
    RUN_TEST(capacity_growth);
    
    printf("\n=== Results: %d passed, %d failed ===\n", tests_passed, tests_failed);
    
    return tests_failed > 0 ? 1 : 0;
}

