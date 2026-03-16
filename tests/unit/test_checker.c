/*
 * FORGE Language Toolchain
 * test_checker.c - Unit tests for type checker
 */

#include "util/memory.h"
#include "util/strtable.h"
#include "util/hashmap.h"
#include "lexer/lexer.h"
#include "parser/ast.h"
#include "typecheck/types.h"
#include "typecheck/checker.h"
#include <stdio.h>
#include <string.h>

static int test_count = 0;
static int pass_count = 0;

#define TEST(name) do { \
    test_count++; \
    printf("  [%d] %-50s", test_count, name); \
} while(0)

#define PASS() do { pass_count++; printf("✓\n"); } while(0)
#define FAIL(msg) do { printf("✗ (%s)\n", msg); } while(0)

/* ─────────────────────────────────────────────────────────────────────────────
 * Helper: Create checker context
 * ───────────────────────────────────────────────────────────────────────────── */

typedef struct {
    forge_arena_t* arena;
    forge_strtable_t* strtable;
    forge_checker_t* checker;
} test_ctx_t;

static test_ctx_t make_ctx(void) {
    test_ctx_t ctx;
    ctx.arena = arena_create(4096);
    ctx.strtable = strtable_create();
    ctx.checker = checker_create(ctx.arena, ctx.strtable, "test.fg");
    return ctx;
}

static void free_ctx(test_ctx_t* ctx) {
    checker_destroy(ctx->checker);
    strtable_destroy(ctx->strtable);
    arena_destroy(ctx->arena);
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Test: Literal Type Inference
 * ───────────────────────────────────────────────────────────────────────────── */

static void test_literal_types(void) {
    test_ctx_t ctx = make_ctx();

    TEST("Int literal -> TY_INT");
    {
        forge_node_t* n = ast_int_lit(ctx.arena, 42, 1, 1);
        forge_type_t* t = checker_type_of(ctx.checker, n);
        if (t && t->kind == TY_INT) PASS();
        else FAIL("expected TY_INT");
    }

    TEST("Float literal -> TY_FLOAT");
    {
        forge_node_t* n = ast_float_lit(ctx.arena, 3.14, 1, 1);
        forge_type_t* t = checker_type_of(ctx.checker, n);
        if (t && t->kind == TY_FLOAT) PASS();
        else FAIL("expected TY_FLOAT");
    }

    TEST("String literal -> TY_STR");
    {
        forge_node_t* n = ast_str_lit(ctx.arena, "hello", 1, 1);
        forge_type_t* t = checker_type_of(ctx.checker, n);
        if (t && t->kind == TY_STR) PASS();
        else FAIL("expected TY_STR");
    }

    TEST("Bool literal (true) -> TY_BOOL");
    {
        forge_node_t* n = ast_bool_lit(ctx.arena, 1, 1, 1);
        forge_type_t* t = checker_type_of(ctx.checker, n);
        if (t && t->kind == TY_BOOL) PASS();
        else FAIL("expected TY_BOOL");
    }

    TEST("None literal -> TY_NONE");
    {
        forge_node_t* n = ast_none_lit(ctx.arena, 1, 1);
        forge_type_t* t = checker_type_of(ctx.checker, n);
        if (t && t->kind == TY_NONE) PASS();
        else FAIL("expected TY_NONE");
    }

    free_ctx(&ctx);
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Test: Binary Operation Type Inference
 * ───────────────────────────────────────────────────────────────────────────── */

static void test_binary_op_types(void) {
    test_ctx_t ctx = make_ctx();

    TEST("int + int -> int");
    {
        forge_node_t* left = ast_int_lit(ctx.arena, 10, 1, 1);
        forge_node_t* right = ast_int_lit(ctx.arena, 20, 1, 5);
        forge_node_t* n = ast_binary_op(ctx.arena, TOK_PLUS, left, right, 1, 3);
        forge_type_t* t = checker_type_of(ctx.checker, n);
        if (t && t->kind == TY_INT) PASS();
        else FAIL("expected TY_INT");
    }

    TEST("float + float -> float");
    {
        forge_node_t* left = ast_float_lit(ctx.arena, 1.5, 1, 1);
        forge_node_t* right = ast_float_lit(ctx.arena, 2.5, 1, 5);
        forge_node_t* n = ast_binary_op(ctx.arena, TOK_PLUS, left, right, 1, 3);
        forge_type_t* t = checker_type_of(ctx.checker, n);
        if (t && t->kind == TY_FLOAT) PASS();
        else FAIL("expected TY_FLOAT");
    }

    TEST("int + float -> float");
    {
        forge_node_t* left = ast_int_lit(ctx.arena, 10, 1, 1);
        forge_node_t* right = ast_float_lit(ctx.arena, 2.5, 1, 5);
        forge_node_t* n = ast_binary_op(ctx.arena, TOK_PLUS, left, right, 1, 3);
        forge_type_t* t = checker_type_of(ctx.checker, n);
        if (t && t->kind == TY_FLOAT) PASS();
        else FAIL("expected TY_FLOAT");
    }

    TEST("str + str -> str (concatenation)");
    {
        forge_node_t* left = ast_str_lit(ctx.arena, "hello", 1, 1);
        forge_node_t* right = ast_str_lit(ctx.arena, "world", 1, 10);
        forge_node_t* n = ast_binary_op(ctx.arena, TOK_PLUS, left, right, 1, 7);
        forge_type_t* t = checker_type_of(ctx.checker, n);
        if (t && t->kind == TY_STR) PASS();
        else FAIL("expected TY_STR");
    }

    TEST("int == int -> bool");
    {
        forge_node_t* left = ast_int_lit(ctx.arena, 10, 1, 1);
        forge_node_t* right = ast_int_lit(ctx.arena, 10, 1, 5);
        forge_node_t* n = ast_binary_op(ctx.arena, TOK_EQ, left, right, 1, 3);
        forge_type_t* t = checker_type_of(ctx.checker, n);
        if (t && t->kind == TY_BOOL) PASS();
        else FAIL("expected TY_BOOL");
    }

    TEST("int < int -> bool");
    {
        forge_node_t* left = ast_int_lit(ctx.arena, 5, 1, 1);
        forge_node_t* right = ast_int_lit(ctx.arena, 10, 1, 5);
        forge_node_t* n = ast_binary_op(ctx.arena, TOK_LT, left, right, 1, 3);
        forge_type_t* t = checker_type_of(ctx.checker, n);
        if (t && t->kind == TY_BOOL) PASS();
        else FAIL("expected TY_BOOL");
    }

    TEST("bool and bool -> bool");
    {
        forge_node_t* left = ast_bool_lit(ctx.arena, 1, 1, 1);
        forge_node_t* right = ast_bool_lit(ctx.arena, 0, 1, 10);
        forge_node_t* n = ast_binary_op(ctx.arena, TOK_AND, left, right, 1, 6);
        forge_type_t* t = checker_type_of(ctx.checker, n);
        if (t && t->kind == TY_BOOL) PASS();
        else FAIL("expected TY_BOOL");
    }

    free_ctx(&ctx);
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Test: Unary Operation Type Inference
 * ───────────────────────────────────────────────────────────────────────────── */

static void test_unary_op_types(void) {
    test_ctx_t ctx = make_ctx();

    TEST("-int -> int");
    {
        forge_node_t* operand = ast_int_lit(ctx.arena, 42, 1, 2);
        forge_node_t* n = ast_unary_op(ctx.arena, TOK_MINUS, operand, 1, 1);
        forge_type_t* t = checker_type_of(ctx.checker, n);
        if (t && t->kind == TY_INT) PASS();
        else FAIL("expected TY_INT");
    }

    TEST("-float -> float");
    {
        forge_node_t* operand = ast_float_lit(ctx.arena, 3.14, 1, 2);
        forge_node_t* n = ast_unary_op(ctx.arena, TOK_MINUS, operand, 1, 1);
        forge_type_t* t = checker_type_of(ctx.checker, n);
        if (t && t->kind == TY_FLOAT) PASS();
        else FAIL("expected TY_FLOAT");
    }

    TEST("not bool -> bool");
    {
        forge_node_t* operand = ast_bool_lit(ctx.arena, 1, 1, 5);
        forge_node_t* n = ast_unary_op(ctx.arena, TOK_NOT, operand, 1, 1);
        forge_type_t* t = checker_type_of(ctx.checker, n);
        if (t && t->kind == TY_BOOL) PASS();
        else FAIL("expected TY_BOOL");
    }

    free_ctx(&ctx);
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Test: Variable Lookup
 * ───────────────────────────────────────────────────────────────────────────── */

static void test_variable_lookup(void) {
    test_ctx_t ctx = make_ctx();

    TEST("Undefined variable -> error type");
    {
        /* Suppress error output for this test */
        forge_node_t* n = ast_ident(ctx.arena, "undefined_var", 1, 1);
        forge_type_t* t = checker_type_of(ctx.checker, n);
        if (t && t->kind == TY_ERROR) PASS();
        else FAIL("expected TY_ERROR for undefined var");
    }

    TEST("Defined local variable -> correct type");
    {
        /* Register a variable in local scope */
        forge_type_t* int_type = type_prim(ctx.arena, TY_INT);
        hashmap_set(ctx.checker->local_vars, "x", int_type);

        forge_node_t* n = ast_ident(ctx.arena, "x", 1, 1);
        forge_type_t* t = checker_type_of(ctx.checker, n);
        if (t && t->kind == TY_INT) PASS();
        else FAIL("expected TY_INT");
    }

    free_ctx(&ctx);
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Test: Type Predicates
 * ───────────────────────────────────────────────────────────────────────────── */

static void test_type_predicates(void) {
    test_ctx_t ctx = make_ctx();

    TEST("type_is_numeric(TY_INT) = true");
    {
        forge_type_t* t = type_prim(ctx.arena, TY_INT);
        if (type_is_numeric(t)) PASS();
        else FAIL("expected true");
    }

    TEST("type_is_numeric(TY_FLOAT) = true");
    {
        forge_type_t* t = type_prim(ctx.arena, TY_FLOAT);
        if (type_is_numeric(t)) PASS();
        else FAIL("expected true");
    }

    TEST("type_is_numeric(TY_STR) = false");
    {
        forge_type_t* t = type_prim(ctx.arena, TY_STR);
        if (!type_is_numeric(t)) PASS();
        else FAIL("expected false");
    }

    TEST("type_is_integer(TY_INT) = true");
    {
        forge_type_t* t = type_prim(ctx.arena, TY_INT);
        if (type_is_integer(t)) PASS();
        else FAIL("expected true");
    }

    TEST("type_is_integer(TY_FLOAT) = false");
    {
        forge_type_t* t = type_prim(ctx.arena, TY_FLOAT);
        if (!type_is_integer(t)) PASS();
        else FAIL("expected false");
    }

    TEST("type_equal(TY_INT, TY_INT) = true");
    {
        forge_type_t* a = type_prim(ctx.arena, TY_INT);
        forge_type_t* b = type_prim(ctx.arena, TY_INT);
        if (type_equal(a, b)) PASS();
        else FAIL("expected true");
    }

    TEST("type_equal(TY_INT, TY_FLOAT) = false");
    {
        forge_type_t* a = type_prim(ctx.arena, TY_INT);
        forge_type_t* b = type_prim(ctx.arena, TY_FLOAT);
        if (!type_equal(a, b)) PASS();
        else FAIL("expected false");
    }

    free_ctx(&ctx);
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Test: Type Constructors
 * ───────────────────────────────────────────────────────────────────────────── */

static void test_type_constructors(void) {
    test_ctx_t ctx = make_ctx();

    TEST("type_optional(?int)");
    {
        forge_type_t* inner = type_prim(ctx.arena, TY_INT);
        forge_type_t* opt = type_optional(ctx.arena, inner);
        if (opt && opt->kind == TY_OPTIONAL && opt->as.optional.inner->kind == TY_INT) PASS();
        else FAIL("expected ?int");
    }

    TEST("type_dyn_array([]int)");
    {
        forge_type_t* elem = type_prim(ctx.arena, TY_INT);
        forge_type_t* arr = type_dyn_array(ctx.arena, elem);
        if (arr && arr->kind == TY_DYN_ARRAY && arr->as.dyn_array.elem_type->kind == TY_INT) PASS();
        else FAIL("expected []int");
    }

    TEST("type_fixed_array([5]int)");
    {
        forge_type_t* elem = type_prim(ctx.arena, TY_INT);
        forge_type_t* arr = type_fixed_array(ctx.arena, elem, 5);
        if (arr && arr->kind == TY_FIXED_ARRAY &&
            arr->as.fixed_array.elem_type->kind == TY_INT &&
            arr->as.fixed_array.size == 5) PASS();
        else FAIL("expected [5]int");
    }

    TEST("type_map(map[str]int)");
    {
        forge_type_t* key = type_prim(ctx.arena, TY_STR);
        forge_type_t* val = type_prim(ctx.arena, TY_INT);
        forge_type_t* m = type_map(ctx.arena, key, val);
        if (m && m->kind == TY_MAP &&
            m->as.map.key_type->kind == TY_STR &&
            m->as.map.val_type->kind == TY_INT) PASS();
        else FAIL("expected map[str]int");
    }

    free_ctx(&ctx);
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Test: type_to_str
 * ───────────────────────────────────────────────────────────────────────────── */

static void test_type_to_str(void) {
    test_ctx_t ctx = make_ctx();

    TEST("type_to_str(TY_INT) = \"int\"");
    {
        forge_type_t* t = type_prim(ctx.arena, TY_INT);
        char* s = type_to_str(t);
        if (s && strcmp(s, "int") == 0) PASS();
        else FAIL(s ? s : "null");
        forge_free(s);
    }

    TEST("type_to_str(?str) = \"?str\"");
    {
        forge_type_t* inner = type_prim(ctx.arena, TY_STR);
        forge_type_t* opt = type_optional(ctx.arena, inner);
        char* s = type_to_str(opt);
        if (s && strcmp(s, "?str") == 0) PASS();
        else FAIL(s ? s : "null");
        forge_free(s);
    }

    TEST("type_to_str([]float) = \"[]float\"");
    {
        forge_type_t* elem = type_prim(ctx.arena, TY_FLOAT);
        forge_type_t* arr = type_dyn_array(ctx.arena, elem);
        char* s = type_to_str(arr);
        if (s && strcmp(s, "[]float") == 0) PASS();
        else FAIL(s ? s : "null");
        forge_free(s);
    }

    free_ctx(&ctx);
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Main
 * ───────────────────────────────────────────────────────────────────────────── */

int main(void) {
    printf("\n=== Type Checker Tests ===\n\n");

    printf("Literal Types:\n");
    test_literal_types();

    printf("\nBinary Operation Types:\n");
    test_binary_op_types();

    printf("\nUnary Operation Types:\n");
    test_unary_op_types();

    printf("\nVariable Lookup:\n");
    test_variable_lookup();

    printf("\nType Predicates:\n");
    test_type_predicates();

    printf("\nType Constructors:\n");
    test_type_constructors();

    printf("\ntype_to_str:\n");
    test_type_to_str();

    printf("\n=== Results: %d/%d passed ===\n\n", pass_count, test_count);
    return (pass_count == test_count) ? 0 : 1;
}

