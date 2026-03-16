/*
 * FORGE Language Toolchain
 * test_value.c - Unit tests for runtime values
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "interp/value.h"

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
 * Tests - Primitives
 * ───────────────────────────────────────────────────────────────────────────── */

static int test_int(void) {
    forge_value_t v = val_int(42);
    ASSERT(v.kind == VAL_INT, "kind should be VAL_INT");
    ASSERT(v.as.i == 42, "value should be 42");
    
    forge_value_t neg = val_int(-100);
    ASSERT(neg.as.i == -100, "negative value");
    
    return 1;
}

static int test_uint(void) {
    forge_value_t v = val_uint(12345);
    ASSERT(v.kind == VAL_UINT, "kind should be VAL_UINT");
    ASSERT(v.as.u == 12345, "value should be 12345");
    return 1;
}

static int test_float(void) {
    forge_value_t v = val_float(3.14159);
    ASSERT(v.kind == VAL_FLOAT, "kind should be VAL_FLOAT");
    ASSERT(v.as.f > 3.14 && v.as.f < 3.15, "value should be ~3.14");
    return 1;
}

static int test_bool(void) {
    forge_value_t t = val_bool(1);
    forge_value_t f = val_bool(0);
    forge_value_t truthy = val_bool(42);  /* non-zero = true */
    
    ASSERT(t.kind == VAL_BOOL, "kind should be VAL_BOOL");
    ASSERT(t.as.b == 1, "true should be 1");
    ASSERT(f.as.b == 0, "false should be 0");
    ASSERT(truthy.as.b == 1, "non-zero should become 1");
    return 1;
}

static int test_byte(void) {
    forge_value_t v = val_byte(255);
    ASSERT(v.kind == VAL_BYTE, "kind should be VAL_BYTE");
    ASSERT(v.as.i == 255, "value should be 255");
    return 1;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Tests - Strings
 * ───────────────────────────────────────────────────────────────────────────── */

static int test_str_literal(void) {
    forge_value_t v = val_str_lit("hello");
    ASSERT(v.kind == VAL_STR, "kind should be VAL_STR");
    ASSERT(v.as.str.len == 5, "length should be 5");
    ASSERT(strcmp(v.as.str.data, "hello") == 0, "data should be 'hello'");
    ASSERT(v.as.str.owned == 0, "literal should not be owned");
    /* No val_free needed for literals */
    return 1;
}

static int test_str_copy(void) {
    forge_value_t v = val_str_copy("world", 5);
    ASSERT(v.kind == VAL_STR, "kind should be VAL_STR");
    ASSERT(v.as.str.len == 5, "length should be 5");
    ASSERT(strcmp(v.as.str.data, "world") == 0, "data should be 'world'");
    ASSERT(v.as.str.owned == 1, "copy should be owned");
    val_free(&v);
    return 1;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Tests - Void/None/Optional
 * ───────────────────────────────────────────────────────────────────────────── */

static int test_void_none(void) {
    forge_value_t v = val_void();
    forge_value_t n = val_none();
    
    ASSERT(v.kind == VAL_VOID, "kind should be VAL_VOID");
    ASSERT(n.kind == VAL_NONE, "kind should be VAL_NONE");
    return 1;
}

static int test_optional(void) {
    forge_value_t inner = val_int(99);
    forge_value_t opt = val_some(inner);
    
    ASSERT(opt.kind == VAL_OPTIONAL, "kind should be VAL_OPTIONAL");
    ASSERT(opt.as.optional.present == 1, "should be present");
    ASSERT(opt.as.optional.inner->kind == VAL_INT, "inner should be INT");
    ASSERT(opt.as.optional.inner->as.i == 99, "inner value should be 99");
    
    val_free(&opt);
    return 1;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Tests - Arrays
 * ───────────────────────────────────────────────────────────────────────────── */

static int test_array_create(void) {
    forge_value_t arr = val_array(0, 10);
    ASSERT(arr.kind == VAL_ARRAY, "kind should be VAL_ARRAY");
    ASSERT(arr.as.array.len == 0, "length should be 0");
    ASSERT(arr.as.array.cap == 10, "capacity should be 10");
    val_free(&arr);
    return 1;
}

static int test_array_push(void) {
    forge_value_t arr = val_array(0, 2);

    val_array_push(&arr, val_int(10));
    val_array_push(&arr, val_int(20));
    val_array_push(&arr, val_int(30));  /* triggers resize */

    ASSERT(val_array_len(&arr) == 3, "length should be 3");

    val_free(&arr);
    return 1;
}

static int test_array_get_set(void) {
    forge_value_t arr = val_array(3, 3);

    val_array_set(&arr, 0, val_int(100));
    val_array_set(&arr, 1, val_int(200));
    val_array_set(&arr, 2, val_int(300));

    forge_value_t v0 = val_array_get(&arr, 0);
    forge_value_t v1 = val_array_get(&arr, 1);
    forge_value_t v2 = val_array_get(&arr, 2);
    forge_value_t vx = val_array_get(&arr, 99);  /* out of bounds */

    ASSERT(v0.as.i == 100, "arr[0] == 100");
    ASSERT(v1.as.i == 200, "arr[1] == 200");
    ASSERT(v2.as.i == 300, "arr[2] == 300");
    ASSERT(vx.kind == VAL_NONE, "out of bounds returns NONE");

    val_free(&v0);
    val_free(&v1);
    val_free(&v2);
    val_free(&arr);
    return 1;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Tests - Records
 * ───────────────────────────────────────────────────────────────────────────── */

static int test_record(void) {
    const char* names[] = {"x", "y"};
    forge_value_t fields[] = {val_int(10), val_int(20)};

    forge_value_t rec = val_record(2, names, fields);

    ASSERT(rec.kind == VAL_RECORD, "kind should be VAL_RECORD");
    ASSERT(rec.as.record.count == 2, "count should be 2");
    ASSERT(rec.as.record.fields[0].as.i == 10, "x should be 10");
    ASSERT(rec.as.record.fields[1].as.i == 20, "y should be 20");

    val_free(&rec);
    return 1;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Tests - Copy and Equality
 * ───────────────────────────────────────────────────────────────────────────── */

static int test_copy_primitives(void) {
    forge_value_t orig = val_int(42);
    forge_value_t copy = val_copy(orig);

    ASSERT(copy.kind == VAL_INT, "copy kind should be VAL_INT");
    ASSERT(copy.as.i == 42, "copy value should be 42");
    return 1;
}

static int test_copy_string(void) {
    forge_value_t orig = val_str_copy("test", 4);
    forge_value_t copy = val_copy(orig);

    ASSERT(copy.kind == VAL_STR, "copy kind should be VAL_STR");
    ASSERT(copy.as.str.len == 4, "copy length should be 4");
    ASSERT(strcmp(copy.as.str.data, "test") == 0, "copy data should be 'test'");
    ASSERT(copy.as.str.data != orig.as.str.data, "copy should have different pointer");

    val_free(&orig);
    val_free(&copy);
    return 1;
}

static int test_equality(void) {
    ASSERT(val_equal(val_int(5), val_int(5)), "5 == 5");
    ASSERT(!val_equal(val_int(5), val_int(6)), "5 != 6");
    ASSERT(!val_equal(val_int(5), val_float(5.0)), "int != float");
    ASSERT(val_equal(val_bool(1), val_bool(1)), "true == true");
    ASSERT(val_equal(val_str_lit("hi"), val_str_lit("hi")), "\"hi\" == \"hi\"");
    return 1;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Tests - To String
 * ───────────────────────────────────────────────────────────────────────────── */

static int test_to_str(void) {
    char* s;

    s = val_to_str(val_int(123));
    ASSERT(strcmp(s, "123") == 0, "int to str");
    forge_free(s);

    s = val_to_str(val_bool(1));
    ASSERT(strcmp(s, "true") == 0, "bool to str");
    forge_free(s);

    s = val_to_str(val_str_lit("hello"));
    ASSERT(strcmp(s, "hello") == 0, "str to str");
    forge_free(s);

    s = val_to_str(val_none());
    ASSERT(strcmp(s, "none") == 0, "none to str");
    forge_free(s);

    return 1;
}

static int test_array_to_str(void) {
    forge_value_t arr = val_array(0, 4);
    val_array_push(&arr, val_int(1));
    val_array_push(&arr, val_int(2));
    val_array_push(&arr, val_int(3));

    char* s = val_to_str(arr);
    ASSERT(strcmp(s, "[1, 2, 3]") == 0, "array to str");
    forge_free(s);

    val_free(&arr);
    return 1;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Tests - Truthiness
 * ───────────────────────────────────────────────────────────────────────────── */

static int test_truthiness(void) {
    ASSERT(val_is_truthy(val_bool(1)), "true is truthy");
    ASSERT(!val_is_truthy(val_bool(0)), "false is not truthy");
    ASSERT(val_is_truthy(val_int(1)), "1 is truthy");
    ASSERT(!val_is_truthy(val_int(0)), "0 is not truthy");
    ASSERT(val_is_truthy(val_str_lit("x")), "non-empty string is truthy");
    ASSERT(!val_is_truthy(val_str_lit("")), "empty string is not truthy");
    ASSERT(!val_is_truthy(val_none()), "none is not truthy");
    return 1;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Main
 * ───────────────────────────────────────────────────────────────────────────── */

int main(void) {
    printf("\n=== FORGE Value Layer Tests ===\n\n");

    RUN_TEST("Integer values", test_int);
    RUN_TEST("Unsigned integer values", test_uint);
    RUN_TEST("Float values", test_float);
    RUN_TEST("Boolean values", test_bool);
    RUN_TEST("Byte values", test_byte);
    RUN_TEST("String literal", test_str_literal);
    RUN_TEST("String copy", test_str_copy);
    RUN_TEST("Void and none", test_void_none);
    RUN_TEST("Optional (some)", test_optional);
    RUN_TEST("Array create", test_array_create);
    RUN_TEST("Array push", test_array_push);
    RUN_TEST("Array get/set", test_array_get_set);
    RUN_TEST("Record", test_record);
    RUN_TEST("Copy primitives", test_copy_primitives);
    RUN_TEST("Copy string (deep)", test_copy_string);
    RUN_TEST("Equality", test_equality);
    RUN_TEST("To string", test_to_str);
    RUN_TEST("Array to string", test_array_to_str);
    RUN_TEST("Truthiness", test_truthiness);

    printf("\n=== Results: %d/%d tests passed ===\n\n", tests_passed, tests_run);

    return tests_passed == tests_run ? 0 : 1;
}

