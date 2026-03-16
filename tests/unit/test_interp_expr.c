/*
 * FORGE Language Toolchain
 * test_interp_expr.c - Unit tests for expression evaluation
 */

#include "util/memory.h"
#include "util/strtable.h"
#include "lexer/lexer.h"
#include "parser/ast.h"
#include "interp/value.h"
#include "interp/env.h"
#include "interp/interp.h"
#include <stdio.h>
#include <string.h>

static int test_count = 0;
static int pass_count = 0;

#define TEST(name) do { \
    test_count++; \
    printf("  [%d] %-45s", test_count, name); \
} while(0)

#define PASS() do { pass_count++; printf("✓\n"); } while(0)
#define FAIL(msg) do { printf("✗ (%s)\n", msg); } while(0)

/* Helper: create a test interpreter */
static forge_interp_t* make_interp(forge_arena_t* arena, forge_strtable_t* st) {
    return interp_create(arena, st);
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Tests
 * ───────────────────────────────────────────────────────────────────────────── */

static void test_literals(void) {
    forge_arena_t* arena = arena_create(4096);
    forge_strtable_t* st = strtable_create();
    forge_interp_t* interp = make_interp(arena, st);
    forge_env_t* env = interp->globals;
    
    TEST("Integer literal");
    {
        forge_node_t* n = ast_int_lit(arena, 42, 1, 1);
        forge_value_t v = interp_eval_expr(interp, env, n);
        if (v.kind == VAL_INT && v.as.i == 42) PASS();
        else FAIL("expected 42");
        val_free(&v);
    }
    
    TEST("Float literal");
    {
        forge_node_t* n = ast_float_lit(arena, 3.14, 1, 1);
        forge_value_t v = interp_eval_expr(interp, env, n);
        if (v.kind == VAL_FLOAT && v.as.f == 3.14) PASS();
        else FAIL("expected 3.14");
        val_free(&v);
    }
    
    TEST("Boolean literal (true)");
    {
        forge_node_t* n = ast_bool_lit(arena, 1, 1, 1);
        forge_value_t v = interp_eval_expr(interp, env, n);
        if (v.kind == VAL_BOOL && v.as.b == 1) PASS();
        else FAIL("expected true");
        val_free(&v);
    }
    
    TEST("String literal");
    {
        const char* s = strtable_intern(st, "hello", 5);
        forge_node_t* n = ast_str_lit(arena, s, 1, 1);
        forge_value_t v = interp_eval_expr(interp, env, n);
        if (v.kind == VAL_STR && strcmp(v.as.str.data, "hello") == 0) PASS();
        else FAIL("expected 'hello'");
        val_free(&v);
    }
    
    TEST("None literal");
    {
        forge_node_t* n = ast_none_lit(arena, 1, 1);
        forge_value_t v = interp_eval_expr(interp, env, n);
        if (v.kind == VAL_NONE) PASS();
        else FAIL("expected none");
        val_free(&v);
    }
    
    interp_destroy(interp);
    strtable_destroy(st);
    arena_destroy(arena);
}

static void test_identifiers(void) {
    forge_arena_t* arena = arena_create(4096);
    forge_strtable_t* st = strtable_create();
    forge_interp_t* interp = make_interp(arena, st);
    forge_env_t* env = interp->globals;
    
    /* Define a variable */
    env_define(env, "x", val_int(100));
    
    TEST("Identifier lookup");
    {
        const char* name = strtable_intern(st, "x", 1);
        forge_node_t* n = ast_ident(arena, name, 1, 1);
        forge_value_t v = interp_eval_expr(interp, env, n);
        if (v.kind == VAL_INT && v.as.i == 100) PASS();
        else FAIL("expected 100");
        val_free(&v);
    }

    TEST("Undefined identifier (error)");
    {
        const char* name = strtable_intern(st, "undefined_var", 13);
        forge_node_t* n = ast_ident(arena, name, 1, 1);
        interp->had_error = 0;  /* Reset error */
        forge_value_t v = interp_eval_expr(interp, env, n);
        if (interp->had_error) PASS();
        else FAIL("expected error");
        val_free(&v);
    }
    
    interp_destroy(interp);
    strtable_destroy(st);
    arena_destroy(arena);
}

static void test_arithmetic(void) {
    forge_arena_t* arena = arena_create(4096);
    forge_strtable_t* st = strtable_create();
    forge_interp_t* interp = make_interp(arena, st);
    forge_env_t* env = interp->globals;
    
    TEST("Integer addition (3 + 4)");
    {
        forge_node_t* left = ast_int_lit(arena, 3, 1, 1);
        forge_node_t* right = ast_int_lit(arena, 4, 1, 1);
        forge_node_t* n = ast_binary_op(arena, TOK_PLUS, left, right, 1, 1);
        forge_value_t v = interp_eval_expr(interp, env, n);
        if (v.kind == VAL_INT && v.as.i == 7) PASS();
        else FAIL("expected 7");
        val_free(&v);
    }
    
    TEST("Integer subtraction (10 - 3)");
    {
        forge_node_t* left = ast_int_lit(arena, 10, 1, 1);
        forge_node_t* right = ast_int_lit(arena, 3, 1, 1);
        forge_node_t* n = ast_binary_op(arena, TOK_MINUS, left, right, 1, 1);
        forge_value_t v = interp_eval_expr(interp, env, n);
        if (v.kind == VAL_INT && v.as.i == 7) PASS();
        else FAIL("expected 7");
        val_free(&v);
    }

    TEST("Integer multiplication (5 * 6)");
    {
        forge_node_t* left = ast_int_lit(arena, 5, 1, 1);
        forge_node_t* right = ast_int_lit(arena, 6, 1, 1);
        forge_node_t* n = ast_binary_op(arena, TOK_STAR, left, right, 1, 1);
        forge_value_t v = interp_eval_expr(interp, env, n);
        if (v.kind == VAL_INT && v.as.i == 30) PASS();
        else FAIL("expected 30");
        val_free(&v);
    }

    TEST("Integer division (20 / 4)");
    {
        forge_node_t* left = ast_int_lit(arena, 20, 1, 1);
        forge_node_t* right = ast_int_lit(arena, 4, 1, 1);
        forge_node_t* n = ast_binary_op(arena, TOK_SLASH, left, right, 1, 1);
        forge_value_t v = interp_eval_expr(interp, env, n);
        if (v.kind == VAL_INT && v.as.i == 5) PASS();
        else FAIL("expected 5");
        val_free(&v);
    }

    TEST("Integer modulo (17 % 5)");
    {
        forge_node_t* left = ast_int_lit(arena, 17, 1, 1);
        forge_node_t* right = ast_int_lit(arena, 5, 1, 1);
        forge_node_t* n = ast_binary_op(arena, TOK_PERCENT, left, right, 1, 1);
        forge_value_t v = interp_eval_expr(interp, env, n);
        if (v.kind == VAL_INT && v.as.i == 2) PASS();
        else FAIL("expected 2");
        val_free(&v);
    }

    TEST("Float addition (1.5 + 2.5)");
    {
        forge_node_t* left = ast_float_lit(arena, 1.5, 1, 1);
        forge_node_t* right = ast_float_lit(arena, 2.5, 1, 1);
        forge_node_t* n = ast_binary_op(arena, TOK_PLUS, left, right, 1, 1);
        forge_value_t v = interp_eval_expr(interp, env, n);
        if (v.kind == VAL_FLOAT && v.as.f == 4.0) PASS();
        else FAIL("expected 4.0");
        val_free(&v);
    }

    TEST("Division by zero (error)");
    {
        forge_node_t* left = ast_int_lit(arena, 10, 1, 1);
        forge_node_t* right = ast_int_lit(arena, 0, 1, 1);
        forge_node_t* n = ast_binary_op(arena, TOK_SLASH, left, right, 1, 1);
        interp->had_error = 0;
        forge_value_t v = interp_eval_expr(interp, env, n);
        if (interp->had_error) PASS();
        else FAIL("expected division by zero error");
        val_free(&v);
    }

    interp_destroy(interp);
    strtable_destroy(st);
    arena_destroy(arena);
}

static void test_comparisons(void) {
    forge_arena_t* arena = arena_create(4096);
    forge_strtable_t* st = strtable_create();
    forge_interp_t* interp = make_interp(arena, st);
    forge_env_t* env = interp->globals;

    TEST("Equality (5 == 5)");
    {
        forge_node_t* left = ast_int_lit(arena, 5, 1, 1);
        forge_node_t* right = ast_int_lit(arena, 5, 1, 1);
        forge_node_t* n = ast_binary_op(arena, TOK_EQ, left, right, 1, 1);
        forge_value_t v = interp_eval_expr(interp, env, n);
        if (v.kind == VAL_BOOL && v.as.b == 1) PASS();
        else FAIL("expected true");
        val_free(&v);
    }

    TEST("Inequality (5 != 3)");
    {
        forge_node_t* left = ast_int_lit(arena, 5, 1, 1);
        forge_node_t* right = ast_int_lit(arena, 3, 1, 1);
        forge_node_t* n = ast_binary_op(arena, TOK_NEQ, left, right, 1, 1);
        forge_value_t v = interp_eval_expr(interp, env, n);
        if (v.kind == VAL_BOOL && v.as.b == 1) PASS();
        else FAIL("expected true");
        val_free(&v);
    }

    TEST("Less than (3 < 5)");
    {
        forge_node_t* left = ast_int_lit(arena, 3, 1, 1);
        forge_node_t* right = ast_int_lit(arena, 5, 1, 1);
        forge_node_t* n = ast_binary_op(arena, TOK_LT, left, right, 1, 1);
        forge_value_t v = interp_eval_expr(interp, env, n);
        if (v.kind == VAL_BOOL && v.as.b == 1) PASS();
        else FAIL("expected true");
        val_free(&v);
    }

    TEST("Greater than or equal (5 >= 5)");
    {
        forge_node_t* left = ast_int_lit(arena, 5, 1, 1);
        forge_node_t* right = ast_int_lit(arena, 5, 1, 1);
        forge_node_t* n = ast_binary_op(arena, TOK_GEQ, left, right, 1, 1);
        forge_value_t v = interp_eval_expr(interp, env, n);
        if (v.kind == VAL_BOOL && v.as.b == 1) PASS();
        else FAIL("expected true");
        val_free(&v);
    }

    interp_destroy(interp);
    strtable_destroy(st);
    arena_destroy(arena);
}

static void test_unary(void) {
    forge_arena_t* arena = arena_create(4096);
    forge_strtable_t* st = strtable_create();
    forge_interp_t* interp = make_interp(arena, st);
    forge_env_t* env = interp->globals;

    TEST("Negation (-42)");
    {
        forge_node_t* operand = ast_int_lit(arena, 42, 1, 1);
        forge_node_t* n = ast_unary_op(arena, TOK_MINUS, operand, 1, 1);
        forge_value_t v = interp_eval_expr(interp, env, n);
        if (v.kind == VAL_INT && v.as.i == -42) PASS();
        else FAIL("expected -42");
        val_free(&v);
    }

    TEST("Logical NOT (!true)");
    {
        forge_node_t* operand = ast_bool_lit(arena, 1, 1, 1);
        forge_node_t* n = ast_unary_op(arena, TOK_NOT, operand, 1, 1);
        forge_value_t v = interp_eval_expr(interp, env, n);
        if (v.kind == VAL_BOOL && v.as.b == 0) PASS();
        else FAIL("expected false");
        val_free(&v);
    }

    TEST("Logical NOT (!false)");
    {
        forge_node_t* operand = ast_bool_lit(arena, 0, 1, 1);
        forge_node_t* n = ast_unary_op(arena, TOK_NOT, operand, 1, 1);
        forge_value_t v = interp_eval_expr(interp, env, n);
        if (v.kind == VAL_BOOL && v.as.b == 1) PASS();
        else FAIL("expected true");
        val_free(&v);
    }

    interp_destroy(interp);
    strtable_destroy(st);
    arena_destroy(arena);
}

static void test_logical(void) {
    forge_arena_t* arena = arena_create(4096);
    forge_strtable_t* st = strtable_create();
    forge_interp_t* interp = make_interp(arena, st);
    forge_env_t* env = interp->globals;

    TEST("Logical AND (true and true)");
    {
        forge_node_t* left = ast_bool_lit(arena, 1, 1, 1);
        forge_node_t* right = ast_bool_lit(arena, 1, 1, 1);
        forge_node_t* n = ast_binary_op(arena, TOK_AND, left, right, 1, 1);
        forge_value_t v = interp_eval_expr(interp, env, n);
        if (v.kind == VAL_BOOL && v.as.b == 1) PASS();
        else FAIL("expected true");
        val_free(&v);
    }

    TEST("Logical AND (true and false)");
    {
        forge_node_t* left = ast_bool_lit(arena, 1, 1, 1);
        forge_node_t* right = ast_bool_lit(arena, 0, 1, 1);
        forge_node_t* n = ast_binary_op(arena, TOK_AND, left, right, 1, 1);
        forge_value_t v = interp_eval_expr(interp, env, n);
        if (v.kind == VAL_BOOL && v.as.b == 0) PASS();
        else FAIL("expected false");
        val_free(&v);
    }

    TEST("Logical OR (false or true)");
    {
        forge_node_t* left = ast_bool_lit(arena, 0, 1, 1);
        forge_node_t* right = ast_bool_lit(arena, 1, 1, 1);
        forge_node_t* n = ast_binary_op(arena, TOK_OR, left, right, 1, 1);
        forge_value_t v = interp_eval_expr(interp, env, n);
        if (v.kind == VAL_BOOL && v.as.b == 1) PASS();
        else FAIL("expected true");
        val_free(&v);
    }

    interp_destroy(interp);
    strtable_destroy(st);
    arena_destroy(arena);
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Main
 * ───────────────────────────────────────────────────────────────────────────── */

int main(void) {
    printf("\n=== FORGE Interpreter Expression Tests ===\n\n");

    test_literals();
    test_identifiers();
    test_arithmetic();
    test_comparisons();
    test_unary();
    test_logical();

    printf("\n=== Results: %d/%d tests passed ===\n\n", pass_count, test_count);
    return (pass_count == test_count) ? 0 : 1;
}

