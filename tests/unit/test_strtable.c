/*
 * FORGE Language Toolchain
 * test_strtable.c - Unit tests for string interning table
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "util/strtable.h"

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
    forge_strtable_t* t = strtable_create();
    ASSERT(t != NULL);
    ASSERT(strtable_count(t) == 0);
    strtable_destroy(t);
}

TEST(intern_single) {
    forge_strtable_t* t = strtable_create();
    const char* s = strtable_intern(t, "hello", 5);
    ASSERT(s != NULL);
    ASSERT(strcmp(s, "hello") == 0);
    ASSERT(strtable_count(t) == 1);
    strtable_destroy(t);
}

TEST(intern_returns_same_pointer) {
    forge_strtable_t* t = strtable_create();
    const char* s1 = strtable_intern(t, "hello", 5);
    const char* s2 = strtable_intern(t, "hello", 5);
    ASSERT(s1 == s2);  /* Same pointer, not just equal strings */
    ASSERT(strtable_count(t) == 1);
    strtable_destroy(t);
}

TEST(intern_different_strings) {
    forge_strtable_t* t = strtable_create();
    const char* s1 = strtable_intern(t, "hello", 5);
    const char* s2 = strtable_intern(t, "world", 5);
    ASSERT(s1 != s2);
    ASSERT(strcmp(s1, "hello") == 0);
    ASSERT(strcmp(s2, "world") == 0);
    ASSERT(strtable_count(t) == 2);
    strtable_destroy(t);
}

TEST(intern_cstr) {
    forge_strtable_t* t = strtable_create();
    const char* s1 = strtable_intern_cstr(t, "hello");
    const char* s2 = strtable_intern(t, "hello", 5);
    ASSERT(s1 == s2);
    strtable_destroy(t);
}

TEST(find_existing) {
    forge_strtable_t* t = strtable_create();
    const char* s1 = strtable_intern(t, "hello", 5);
    const char* s2 = strtable_find(t, "hello", 5);
    ASSERT(s1 == s2);
    strtable_destroy(t);
}

TEST(find_nonexistent) {
    forge_strtable_t* t = strtable_create();
    strtable_intern(t, "hello", 5);
    const char* s = strtable_find(t, "world", 5);
    ASSERT(s == NULL);
    strtable_destroy(t);
}

TEST(intern_empty_string) {
    forge_strtable_t* t = strtable_create();
    const char* s = strtable_intern(t, "", 0);
    ASSERT(s != NULL);
    ASSERT(strcmp(s, "") == 0);
    ASSERT(strtable_count(t) == 1);
    strtable_destroy(t);
}

TEST(intern_triggers_resize) {
    forge_strtable_t* t = strtable_create();
    char buf[16];
    
    /* Insert enough strings to trigger multiple resizes */
    for (int i = 0; i < 200; i++) {
        snprintf(buf, sizeof(buf), "str_%d", i);
        const char* s = strtable_intern_cstr(t, buf);
        ASSERT(s != NULL);
        ASSERT(strcmp(s, buf) == 0);
    }
    
    ASSERT(strtable_count(t) == 200);
    
    /* Verify all strings still accessible after resizes */
    for (int i = 0; i < 200; i++) {
        snprintf(buf, sizeof(buf), "str_%d", i);
        const char* s = strtable_find(t, buf, strlen(buf));
        ASSERT(s != NULL);
        ASSERT(strcmp(s, buf) == 0);
    }
    
    strtable_destroy(t);
}

TEST(hash_consistency) {
    /* Same string should always produce same hash */
    uint32_t h1 = strtable_hash("hello", 5);
    uint32_t h2 = strtable_hash("hello", 5);
    ASSERT(h1 == h2);
    
    /* Different strings should (usually) produce different hashes */
    uint32_t h3 = strtable_hash("world", 5);
    ASSERT(h1 != h3);
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Main
 * ───────────────────────────────────────────────────────────────────────────── */

int main(void) {
    printf("=== String Table Unit Tests ===\n\n");
    
    RUN_TEST(create_destroy);
    RUN_TEST(intern_single);
    RUN_TEST(intern_returns_same_pointer);
    RUN_TEST(intern_different_strings);
    RUN_TEST(intern_cstr);
    RUN_TEST(find_existing);
    RUN_TEST(find_nonexistent);
    RUN_TEST(intern_empty_string);
    RUN_TEST(intern_triggers_resize);
    RUN_TEST(hash_consistency);
    
    printf("\n=== Results: %d passed, %d failed ===\n", tests_passed, tests_failed);
    
    return tests_failed > 0 ? 1 : 0;
}

