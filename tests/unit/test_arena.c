/*
 * FORGE Language Toolchain
 * test_arena.c - Unit tests for arena allocator
 */

#include <stdio.h>
#include <string.h>
#include "util/memory.h"

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

/* ─────────────────────────────────────────────────────────────────────────────
 * Tests
 * ───────────────────────────────────────────────────────────────────────────── */

TEST(create_destroy) {
    forge_arena_t* arena = arena_create(0);
    ASSERT(arena != NULL);
    ASSERT(arena_bytes_allocated(arena) == ARENA_DEFAULT_BLOCK_SIZE);
    ASSERT(arena_bytes_used(arena) == 0);
    arena_destroy(arena);
}

TEST(create_custom_size) {
    forge_arena_t* arena = arena_create(1024);
    ASSERT(arena != NULL);
    ASSERT(arena_bytes_allocated(arena) == 1024);
    arena_destroy(arena);
}

TEST(alloc_single) {
    forge_arena_t* arena = arena_create(0);
    int* p = arena_alloc(arena, sizeof(int));
    ASSERT(p != NULL);
    ASSERT(*p == 0);  /* Zero-initialized */
    *p = 42;
    ASSERT(*p == 42);
    arena_destroy(arena);
}

TEST(alloc_multiple) {
    forge_arena_t* arena = arena_create(0);
    int* a = arena_alloc(arena, sizeof(int));
    int* b = arena_alloc(arena, sizeof(int));
    int* c = arena_alloc(arena, sizeof(int));
    
    ASSERT(a != b);
    ASSERT(b != c);
    ASSERT(a != c);
    
    *a = 1;
    *b = 2;
    *c = 3;
    
    ASSERT(*a == 1);
    ASSERT(*b == 2);
    ASSERT(*c == 3);
    
    arena_destroy(arena);
}

TEST(alignment) {
    forge_arena_t* arena = arena_create(0);
    
    /* Allocate various sizes and check alignment */
    char* p1 = arena_alloc(arena, 1);
    char* p2 = arena_alloc(arena, 3);
    char* p3 = arena_alloc(arena, 7);
    double* p4 = arena_alloc(arena, sizeof(double));
    
    ASSERT(((uintptr_t)p1 % ARENA_ALIGNMENT) == 0);
    ASSERT(((uintptr_t)p2 % ARENA_ALIGNMENT) == 0);
    ASSERT(((uintptr_t)p3 % ARENA_ALIGNMENT) == 0);
    ASSERT(((uintptr_t)p4 % ARENA_ALIGNMENT) == 0);
    
    arena_destroy(arena);
}

TEST(alloc_array) {
    forge_arena_t* arena = arena_create(0);
    
    int* arr = arena_alloc_array(arena, 100, sizeof(int));
    ASSERT(arr != NULL);
    
    /* All elements should be zero */
    for (int i = 0; i < 100; i++) {
        ASSERT(arr[i] == 0);
    }
    
    /* Write and verify */
    for (int i = 0; i < 100; i++) {
        arr[i] = i * i;
    }
    for (int i = 0; i < 100; i++) {
        ASSERT(arr[i] == i * i);
    }
    
    arena_destroy(arena);
}

TEST(alloc_macro) {
    forge_arena_t* arena = arena_create(0);
    
    typedef struct { int x; int y; } point_t;
    
    point_t* p = ARENA_ALLOC(arena, point_t);
    ASSERT(p != NULL);
    ASSERT(p->x == 0);
    ASSERT(p->y == 0);
    
    p->x = 10;
    p->y = 20;
    ASSERT(p->x == 10);
    ASSERT(p->y == 20);
    
    arena_destroy(arena);
}

TEST(alloc_array_macro) {
    forge_arena_t* arena = arena_create(0);
    
    double* arr = ARENA_ALLOC_ARRAY(arena, double, 50);
    ASSERT(arr != NULL);
    
    for (int i = 0; i < 50; i++) {
        arr[i] = i * 0.5;
    }
    
    ASSERT(arr[0] == 0.0);
    ASSERT(arr[10] == 5.0);
    ASSERT(arr[49] == 24.5);
    
    arena_destroy(arena);
}

TEST(strdup) {
    forge_arena_t* arena = arena_create(0);

    const char* original = "Hello, FORGE!";
    char* copy = arena_strdup(arena, original);

    ASSERT(copy != NULL);
    ASSERT(copy != original);
    ASSERT(strcmp(copy, original) == 0);

    arena_destroy(arena);
}

TEST(strndup) {
    forge_arena_t* arena = arena_create(0);

    const char* original = "Hello, World!";
    char* copy = arena_strndup(arena, original, 5);

    ASSERT(copy != NULL);
    ASSERT(strcmp(copy, "Hello") == 0);
    ASSERT(strlen(copy) == 5);

    arena_destroy(arena);
}

TEST(multiple_blocks) {
    /* Use small blocks to force multiple allocations */
    forge_arena_t* arena = arena_create(256);

    /* Allocate more than one block's worth */
    for (int i = 0; i < 100; i++) {
        int* p = arena_alloc(arena, 64);
        ASSERT(p != NULL);
        *p = i;
    }

    ASSERT(arena_bytes_allocated(arena) > 256);

    arena_destroy(arena);
}

TEST(oversized_allocation) {
    /* Test allocation larger than block size */
    forge_arena_t* arena = arena_create(256);

    char* big = arena_alloc(arena, 1024);
    ASSERT(big != NULL);

    /* Should have allocated a larger block */
    ASSERT(arena_bytes_allocated(arena) >= 1024);

    /* Fill it to verify it's usable */
    memset(big, 'X', 1024);

    arena_destroy(arena);
}

TEST(reset) {
    forge_arena_t* arena = arena_create(1024);

    /* Allocate some data */
    for (int i = 0; i < 10; i++) {
        arena_alloc(arena, 100);
    }

    ASSERT(arena_bytes_used(arena) > 0);

    /* Reset */
    arena_reset(arena);

    ASSERT(arena_bytes_used(arena) == 0);

    /* Should be able to allocate again */
    int* p = arena_alloc(arena, sizeof(int));
    ASSERT(p != NULL);
    *p = 123;
    ASSERT(*p == 123);

    arena_destroy(arena);
}

TEST(stress_10000_nodes) {
    /* Simulate AST node allocation - 10000 nodes */
    forge_arena_t* arena = arena_create(0);

    typedef struct {
        int kind;
        int line;
        int column;
        void* left;
        void* right;
        char data[48];  /* ~64 bytes total per node */
    } fake_node_t;

    fake_node_t* nodes[10000];

    for (int i = 0; i < 10000; i++) {
        nodes[i] = ARENA_ALLOC(arena, fake_node_t);
        ASSERT(nodes[i] != NULL);
        nodes[i]->kind = i % 50;
        nodes[i]->line = i;
        nodes[i]->column = i % 80;
    }

    /* Verify all nodes are still valid */
    for (int i = 0; i < 10000; i++) {
        ASSERT(nodes[i]->kind == i % 50);
        ASSERT(nodes[i]->line == i);
        ASSERT(nodes[i]->column == i % 80);
    }

    /* Check stats */
    ASSERT(arena_bytes_used(arena) >= 10000 * sizeof(fake_node_t));

    arena_destroy(arena);
}

TEST(zero_size_alloc) {
    forge_arena_t* arena = arena_create(0);

    void* p = arena_alloc(arena, 0);
    ASSERT(p != NULL);  /* Should allocate at least 1 byte */

    arena_destroy(arena);
}

TEST(null_strdup) {
    forge_arena_t* arena = arena_create(0);

    char* copy = arena_strdup(arena, NULL);
    ASSERT(copy == NULL);

    copy = arena_strndup(arena, NULL, 10);
    ASSERT(copy == NULL);

    arena_destroy(arena);
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Main
 * ───────────────────────────────────────────────────────────────────────────── */

int main(void) {
    printf("=== Arena Allocator Unit Tests ===\n\n");

    RUN_TEST(create_destroy);
    RUN_TEST(create_custom_size);
    RUN_TEST(alloc_single);
    RUN_TEST(alloc_multiple);
    RUN_TEST(alignment);
    RUN_TEST(alloc_array);
    RUN_TEST(alloc_macro);
    RUN_TEST(alloc_array_macro);
    RUN_TEST(strdup);
    RUN_TEST(strndup);
    RUN_TEST(multiple_blocks);
    RUN_TEST(oversized_allocation);
    RUN_TEST(reset);
    RUN_TEST(stress_10000_nodes);
    RUN_TEST(zero_size_alloc);
    RUN_TEST(null_strdup);

    printf("\n=== Results: %d passed, %d failed ===\n", tests_passed, tests_failed);

    return tests_failed > 0 ? 1 : 0;
}

