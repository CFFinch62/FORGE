/*
 * FORGE Language Toolchain
 * test_env.c - Unit tests for environment/scope management
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "interp/env.h"

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
 * Tests - Basic Operations
 * ───────────────────────────────────────────────────────────────────────────── */

static int test_create_destroy(void) {
    forge_env_t* env = env_create(NULL);
    ASSERT(env != NULL, "env_create should return non-NULL");
    ASSERT(env->parent == NULL, "root env should have NULL parent");
    ASSERT(env_count(env) == 0, "new env should be empty");
    env_destroy(env);
    return 1;
}

static int test_define_get(void) {
    forge_env_t* env = env_create(NULL);
    
    env_define(env, "x", val_int(42));
    env_define(env, "y", val_str_lit("hello"));
    
    ASSERT(env_count(env) == 2, "should have 2 bindings");
    
    forge_value_t x = env_get(env, "x");
    forge_value_t y = env_get(env, "y");
    forge_value_t z = env_get(env, "z");
    
    ASSERT(x.kind == VAL_INT && x.as.i == 42, "x should be 42");
    ASSERT(y.kind == VAL_STR && strcmp(y.as.str.data, "hello") == 0, "y should be 'hello'");
    ASSERT(z.kind == VAL_NONE, "z should be NONE (not found)");
    
    val_free(&x);
    val_free(&y);
    env_destroy(env);
    return 1;
}

static int test_has(void) {
    forge_env_t* env = env_create(NULL);
    
    env_define(env, "exists", val_int(1));
    
    ASSERT(env_has(env, "exists"), "should have 'exists'");
    ASSERT(!env_has(env, "missing"), "should not have 'missing'");
    
    env_destroy(env);
    return 1;
}

static int test_update(void) {
    forge_env_t* env = env_create(NULL);
    
    env_define(env, "counter", val_int(0));
    
    forge_value_t v1 = env_get(env, "counter");
    ASSERT(v1.as.i == 0, "initial value should be 0");
    val_free(&v1);
    
    int updated = env_update(env, "counter", val_int(10));
    ASSERT(updated == 1, "update should succeed");
    
    forge_value_t v2 = env_get(env, "counter");
    ASSERT(v2.as.i == 10, "updated value should be 10");
    val_free(&v2);
    
    /* Update non-existent */
    int failed = env_update(env, "nope", val_int(99));
    ASSERT(failed == 0, "update of non-existent should fail");
    
    env_destroy(env);
    return 1;
}

static int test_redefine(void) {
    forge_env_t* env = env_create(NULL);
    
    env_define(env, "val", val_int(100));
    env_define(env, "val", val_int(200));  /* redefine in same scope */
    
    ASSERT(env_count(env) == 1, "should still have 1 binding");
    
    forge_value_t v = env_get(env, "val");
    ASSERT(v.as.i == 200, "redefined value should be 200");
    val_free(&v);
    
    env_destroy(env);
    return 1;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Tests - Nested Scopes
 * ───────────────────────────────────────────────────────────────────────────── */

static int test_parent_lookup(void) {
    forge_env_t* global = env_create(NULL);
    forge_env_t* local = env_create(global);
    
    env_define(global, "global_var", val_int(100));
    env_define(local, "local_var", val_int(200));
    
    /* Local can see both */
    forge_value_t g = env_get(local, "global_var");
    forge_value_t l = env_get(local, "local_var");
    ASSERT(g.as.i == 100, "local should see global_var");
    ASSERT(l.as.i == 200, "local should see local_var");
    val_free(&g);
    val_free(&l);
    
    /* Global can only see its own */
    forge_value_t g2 = env_get(global, "global_var");
    forge_value_t l2 = env_get(global, "local_var");
    ASSERT(g2.as.i == 100, "global sees global_var");
    ASSERT(l2.kind == VAL_NONE, "global should NOT see local_var");
    val_free(&g2);
    
    env_destroy(local);
    env_destroy(global);
    return 1;
}

static int test_shadowing(void) {
    forge_env_t* outer = env_create(NULL);
    forge_env_t* inner = env_create(outer);

    env_define(outer, "x", val_int(1));
    env_define(inner, "x", val_int(2));  /* shadows outer x */

    forge_value_t outer_x = env_get(outer, "x");
    forge_value_t inner_x = env_get(inner, "x");

    ASSERT(outer_x.as.i == 1, "outer x should be 1");
    ASSERT(inner_x.as.i == 2, "inner x should shadow to 2");

    val_free(&outer_x);
    val_free(&inner_x);
    env_destroy(inner);
    env_destroy(outer);
    return 1;
}

static int test_update_parent(void) {
    forge_env_t* outer = env_create(NULL);
    forge_env_t* inner = env_create(outer);

    env_define(outer, "x", val_int(10));

    /* Update from inner scope should modify outer */
    int updated = env_update(inner, "x", val_int(20));
    ASSERT(updated == 1, "update should succeed");

    forge_value_t v = env_get(outer, "x");
    ASSERT(v.as.i == 20, "outer x should be updated to 20");
    val_free(&v);

    env_destroy(inner);
    env_destroy(outer);
    return 1;
}

static int test_has_local(void) {
    forge_env_t* outer = env_create(NULL);
    forge_env_t* inner = env_create(outer);

    env_define(outer, "outer_only", val_int(1));
    env_define(inner, "inner_only", val_int(2));

    ASSERT(env_has_local(outer, "outer_only"), "outer has outer_only locally");
    ASSERT(!env_has_local(outer, "inner_only"), "outer doesn't have inner_only locally");
    ASSERT(env_has_local(inner, "inner_only"), "inner has inner_only locally");
    ASSERT(!env_has_local(inner, "outer_only"), "inner doesn't have outer_only locally");

    /* But env_has should find outer_only from inner */
    ASSERT(env_has(inner, "outer_only"), "inner can see outer_only via parent");

    env_destroy(inner);
    env_destroy(outer);
    return 1;
}

static int test_get_ptr(void) {
    forge_env_t* env = env_create(NULL);

    env_define(env, "counter", val_int(0));

    forge_value_t* ptr = env_get_ptr(env, "counter");
    ASSERT(ptr != NULL, "should get pointer");
    ASSERT(ptr->kind == VAL_INT, "should be INT");
    ASSERT(ptr->as.i == 0, "should be 0");

    /* Modify directly */
    ptr->as.i = 999;

    forge_value_t v = env_get(env, "counter");
    ASSERT(v.as.i == 999, "direct modification should work");
    val_free(&v);

    env_destroy(env);
    return 1;
}

static int test_deep_nesting(void) {
    /* Create chain: global -> level1 -> level2 -> level3 */
    forge_env_t* global = env_create(NULL);
    forge_env_t* level1 = env_create(global);
    forge_env_t* level2 = env_create(level1);
    forge_env_t* level3 = env_create(level2);

    env_define(global, "g", val_int(0));
    env_define(level1, "l1", val_int(1));
    env_define(level2, "l2", val_int(2));
    env_define(level3, "l3", val_int(3));

    /* level3 should see all */
    forge_value_t g = env_get(level3, "g");
    forge_value_t l1 = env_get(level3, "l1");
    forge_value_t l2 = env_get(level3, "l2");
    forge_value_t l3 = env_get(level3, "l3");

    ASSERT(g.as.i == 0, "should see g");
    ASSERT(l1.as.i == 1, "should see l1");
    ASSERT(l2.as.i == 2, "should see l2");
    ASSERT(l3.as.i == 3, "should see l3");

    val_free(&g);
    val_free(&l1);
    val_free(&l2);
    val_free(&l3);

    env_destroy(level3);
    env_destroy(level2);
    env_destroy(level1);
    env_destroy(global);
    return 1;
}

static int test_string_values(void) {
    forge_env_t* env = env_create(NULL);

    /* Test with owned strings (heap allocated) */
    forge_value_t alice = val_str_copy("Alice", 5);
    env_define(env, "name", alice);
    val_free(&alice);  /* env_define copies, so we free the original */

    forge_value_t v1 = env_get(env, "name");
    ASSERT(strcmp(v1.as.str.data, "Alice") == 0, "should get Alice");
    val_free(&v1);

    /* Update to new value */
    forge_value_t bob = val_str_copy("Bob", 3);
    env_update(env, "name", bob);
    val_free(&bob);  /* env_update copies, so we free the original */

    forge_value_t v2 = env_get(env, "name");
    ASSERT(strcmp(v2.as.str.data, "Bob") == 0, "should get Bob");
    val_free(&v2);

    env_destroy(env);
    return 1;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Main
 * ───────────────────────────────────────────────────────────────────────────── */

int main(void) {
    printf("\n=== FORGE Environment Tests ===\n\n");

    RUN_TEST("Create and destroy", test_create_destroy);
    RUN_TEST("Define and get", test_define_get);
    RUN_TEST("Has", test_has);
    RUN_TEST("Update", test_update);
    RUN_TEST("Redefine in same scope", test_redefine);
    RUN_TEST("Parent lookup", test_parent_lookup);
    RUN_TEST("Shadowing", test_shadowing);
    RUN_TEST("Update parent scope", test_update_parent);
    RUN_TEST("Has local vs global", test_has_local);
    RUN_TEST("Get pointer (direct access)", test_get_ptr);
    RUN_TEST("Deep nesting (4 levels)", test_deep_nesting);
    RUN_TEST("String values (heap)", test_string_values);

    printf("\n=== Results: %d/%d tests passed ===\n\n", tests_passed, tests_run);

    return tests_passed == tests_run ? 0 : 1;
}

