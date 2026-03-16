/*
 * FORGE Language Toolchain
 * test_interp_stmt.c - Unit tests for statement execution
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

static void test_var_decl(void) {
    forge_arena_t* arena = arena_create(4096);
    forge_strtable_t* st = strtable_create();
    forge_interp_t* interp = make_interp(arena, st);
    forge_env_t* env = interp->globals;
    
    TEST("Variable declaration with init");
    {
        /* var x = 42 */
        const char* name = strtable_intern(st, "x", 1);
        forge_node_t* init = ast_int_lit(arena, 42, 1, 1);
        forge_node_t* decl = ast_var_decl(arena, name, NULL, init, 0, 1, 1);
        
        forge_result_t res = interp_exec_stmt(interp, env, decl);
        forge_value_t v = env_get(env, name);
        
        if (res.flow == FLOW_NORMAL && v.kind == VAL_INT && v.as.i == 42) PASS();
        else FAIL("expected x = 42");
        val_free(&v);
        val_free(&res.value);
    }
    
    TEST("Variable declaration without init");
    {
        /* var y */
        const char* name = strtable_intern(st, "y", 1);
        forge_node_t* decl = ast_var_decl(arena, name, NULL, NULL, 0, 1, 1);
        
        forge_result_t res = interp_exec_stmt(interp, env, decl);
        forge_value_t v = env_get(env, name);
        
        if (res.flow == FLOW_NORMAL && v.kind == VAL_NONE) PASS();
        else FAIL("expected y = none");
        val_free(&v);
        val_free(&res.value);
    }
    
    interp_destroy(interp);
    strtable_destroy(st);
    arena_destroy(arena);
}

static void test_assignment(void) {
    forge_arena_t* arena = arena_create(4096);
    forge_strtable_t* st = strtable_create();
    forge_interp_t* interp = make_interp(arena, st);
    forge_env_t* env = interp->globals;
    
    /* Define variable first */
    const char* name = strtable_intern(st, "x", 1);
    env_define(env, name, val_int(10));
    
    TEST("Assignment to variable");
    {
        /* x = 100 */
        forge_node_t* target = ast_ident(arena, name, 1, 1);
        forge_node_t* value = ast_int_lit(arena, 100, 1, 1);
        forge_node_t* stmt = ast_assign(arena, target, value, 1, 1);
        
        forge_result_t res = interp_exec_stmt(interp, env, stmt);
        forge_value_t v = env_get(env, name);
        
        if (res.flow == FLOW_NORMAL && v.kind == VAL_INT && v.as.i == 100) PASS();
        else FAIL("expected x = 100");
        val_free(&v);
        val_free(&res.value);
    }
    
    TEST("Assignment to undefined variable (error)");
    {
        const char* undef = strtable_intern(st, "undefined_z", 11);
        forge_node_t* target = ast_ident(arena, undef, 1, 1);
        forge_node_t* value = ast_int_lit(arena, 50, 1, 1);
        forge_node_t* stmt = ast_assign(arena, target, value, 1, 1);
        
        interp->had_error = 0;
        forge_result_t res = interp_exec_stmt(interp, env, stmt);
        
        if (interp->had_error) PASS();
        else FAIL("expected error");
        val_free(&res.value);
    }
    
    interp_destroy(interp);
    strtable_destroy(st);
    arena_destroy(arena);
}

static void test_if_stmt(void) {
    forge_arena_t* arena = arena_create(4096);
    forge_strtable_t* st = strtable_create();
    forge_interp_t* interp = make_interp(arena, st);
    forge_env_t* env = interp->globals;
    
    const char* result = strtable_intern(st, "result", 6);
    env_define(env, result, val_int(0));
    
    TEST("If true branch");
    {
        /* if true: result = 1 */
        forge_node_t* cond = ast_bool_lit(arena, 1, 1, 1);

        forge_node_t* assign_target = ast_ident(arena, result, 1, 1);
        forge_node_t* assign_val = ast_int_lit(arena, 1, 1, 1);
        forge_node_t* assign = ast_assign(arena, assign_target, assign_val, 1, 1);
        forge_node_t* stmts[] = { assign };
        forge_node_t* then_block = ast_block(arena, stmts, 1, 1, 1);

        forge_node_t* if_stmt = ast_if(arena, cond, then_block, NULL, NULL, 0, NULL, 1, 1);

        forge_result_t res = interp_exec_stmt(interp, env, if_stmt);
        forge_value_t v = env_get(env, result);

        if (res.flow == FLOW_NORMAL && v.kind == VAL_INT && v.as.i == 1) PASS();
        else FAIL("expected result = 1");
        val_free(&v);
        val_free(&res.value);
    }

    TEST("If false - else branch");
    {
        env_update(env, result, val_int(0));

        forge_node_t* cond = ast_bool_lit(arena, 0, 1, 1);

        forge_node_t* then_target = ast_ident(arena, result, 1, 1);
        forge_node_t* then_val = ast_int_lit(arena, 10, 1, 1);
        forge_node_t* then_assign = ast_assign(arena, then_target, then_val, 1, 1);
        forge_node_t* then_stmts[] = { then_assign };
        forge_node_t* then_block = ast_block(arena, then_stmts, 1, 1, 1);

        forge_node_t* else_target = ast_ident(arena, result, 1, 1);
        forge_node_t* else_val = ast_int_lit(arena, 20, 1, 1);
        forge_node_t* else_assign = ast_assign(arena, else_target, else_val, 1, 1);
        forge_node_t* else_stmts[] = { else_assign };
        forge_node_t* else_block = ast_block(arena, else_stmts, 1, 1, 1);

        forge_node_t* if_stmt = ast_if(arena, cond, then_block, NULL, NULL, 0, else_block, 1, 1);

        forge_result_t res = interp_exec_stmt(interp, env, if_stmt);
        forge_value_t v = env_get(env, result);

        if (res.flow == FLOW_NORMAL && v.kind == VAL_INT && v.as.i == 20) PASS();
        else FAIL("expected result = 20");
        val_free(&v);
        val_free(&res.value);
    }

    interp_destroy(interp);
    strtable_destroy(st);
    arena_destroy(arena);
}

static void test_while_loop(void) {
    forge_arena_t* arena = arena_create(4096);
    forge_strtable_t* st = strtable_create();
    forge_interp_t* interp = make_interp(arena, st);
    forge_env_t* env = interp->globals;

    const char* counter = strtable_intern(st, "counter", 7);

    TEST("While loop counts to 5");
    {
        env_define(env, counter, val_int(0));

        forge_node_t* cond_left = ast_ident(arena, counter, 1, 1);
        forge_node_t* cond_right = ast_int_lit(arena, 5, 1, 1);
        forge_node_t* cond = ast_binary_op(arena, TOK_LT, cond_left, cond_right, 1, 1);

        forge_node_t* add_left = ast_ident(arena, counter, 1, 1);
        forge_node_t* add_right = ast_int_lit(arena, 1, 1, 1);
        forge_node_t* add = ast_binary_op(arena, TOK_PLUS, add_left, add_right, 1, 1);
        forge_node_t* assign_target = ast_ident(arena, counter, 1, 1);
        forge_node_t* assign = ast_assign(arena, assign_target, add, 1, 1);
        forge_node_t* body_stmts[] = { assign };
        forge_node_t* body = ast_block(arena, body_stmts, 1, 1, 1);

        forge_node_t* while_stmt = ast_while(arena, cond, body, 1, 1);

        forge_result_t res = interp_exec_stmt(interp, env, while_stmt);
        forge_value_t v = env_get(env, counter);

        if (res.flow == FLOW_NORMAL && v.kind == VAL_INT && v.as.i == 5) PASS();
        else FAIL("expected counter = 5");
        val_free(&v);
        val_free(&res.value);
    }

    interp_destroy(interp);
    strtable_destroy(st);
    arena_destroy(arena);
}

static void test_for_loop(void) {
    forge_arena_t* arena = arena_create(4096);
    forge_strtable_t* st = strtable_create();
    forge_interp_t* interp = make_interp(arena, st);
    forge_env_t* env = interp->globals;

    const char* sum = strtable_intern(st, "sum", 3);
    const char* i_var = strtable_intern(st, "i", 1);

    TEST("For loop with range (sum 0..5)");
    {
        env_define(env, sum, val_int(0));

        forge_node_t* range_start = ast_int_lit(arena, 0, 1, 1);
        forge_node_t* range_end = ast_int_lit(arena, 5, 1, 1);
        forge_node_t* range = ast_range(arena, range_start, range_end, 0, 1, 1);

        forge_node_t* add_left = ast_ident(arena, sum, 1, 1);
        forge_node_t* add_right = ast_ident(arena, i_var, 1, 1);
        forge_node_t* add = ast_binary_op(arena, TOK_PLUS, add_left, add_right, 1, 1);
        forge_node_t* assign_target = ast_ident(arena, sum, 1, 1);
        forge_node_t* assign = ast_assign(arena, assign_target, add, 1, 1);
        forge_node_t* body_stmts[] = { assign };
        forge_node_t* body = ast_block(arena, body_stmts, 1, 1, 1);

        forge_node_t* for_stmt = ast_for(arena, i_var, range, body, 1, 1);

        forge_result_t res = interp_exec_stmt(interp, env, for_stmt);
        forge_value_t v = env_get(env, sum);

        /* 0+1+2+3+4 = 10 */
        if (res.flow == FLOW_NORMAL && v.kind == VAL_INT && v.as.i == 10) PASS();
        else FAIL("expected sum = 10");
        val_free(&v);
        val_free(&res.value);
    }

    interp_destroy(interp);
    strtable_destroy(st);
    arena_destroy(arena);
}

static void test_return_break_continue(void) {
    forge_arena_t* arena = arena_create(4096);
    forge_strtable_t* st = strtable_create();
    forge_interp_t* interp = make_interp(arena, st);
    forge_env_t* env = interp->globals;

    TEST("Return statement with value");
    {
        forge_node_t* val = ast_int_lit(arena, 42, 1, 1);
        forge_node_t* ret = ast_return(arena, val, 1, 1);

        forge_result_t res = interp_exec_stmt(interp, env, ret);

        if (res.flow == FLOW_RETURN && res.value.kind == VAL_INT && res.value.as.i == 42) PASS();
        else FAIL("expected FLOW_RETURN with 42");
        val_free(&res.value);
    }

    TEST("Return statement without value");
    {
        forge_node_t* ret = ast_return(arena, NULL, 1, 1);

        forge_result_t res = interp_exec_stmt(interp, env, ret);

        if (res.flow == FLOW_RETURN && res.value.kind == VAL_VOID) PASS();
        else FAIL("expected FLOW_RETURN with void");
        val_free(&res.value);
    }

    TEST("Break statement");
    {
        forge_node_t* brk = ast_break(arena, 1, 1);

        forge_result_t res = interp_exec_stmt(interp, env, brk);

        if (res.flow == FLOW_BREAK) PASS();
        else FAIL("expected FLOW_BREAK");
        val_free(&res.value);
    }

    TEST("Continue statement");
    {
        forge_node_t* cont = ast_continue(arena, 1, 1);

        forge_result_t res = interp_exec_stmt(interp, env, cont);

        if (res.flow == FLOW_CONTINUE) PASS();
        else FAIL("expected FLOW_CONTINUE");
        val_free(&res.value);
    }

    interp_destroy(interp);
    strtable_destroy(st);
    arena_destroy(arena);
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Main
 * ───────────────────────────────────────────────────────────────────────────── */

int main(void) {
    printf("\n=== FORGE Interpreter Statement Tests ===\n\n");

    test_var_decl();
    test_assignment();
    test_if_stmt();
    test_while_loop();
    test_for_loop();
    test_return_break_continue();

    printf("\n=== Results: %d/%d tests passed ===\n\n", pass_count, test_count);
    return (pass_count == test_count) ? 0 : 1;
}

