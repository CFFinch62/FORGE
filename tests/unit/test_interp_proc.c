/*
 * FORGE Language Toolchain
 * test_interp_proc.c - Unit tests for procedure declarations and calls
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

/* Helper: create parameters array in arena */
static forge_param_t* make_params(forge_arena_t* arena, const char** names, int count) {
    if (count == 0) return NULL;
    forge_param_t* params = arena_alloc(arena, sizeof(forge_param_t) * count);
    for (int i = 0; i < count; i++) {
        params[i].name = names[i];
        params[i].type_expr = NULL;
        params[i].is_ref = 0;
    }
    return params;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Tests
 * ───────────────────────────────────────────────────────────────────────────── */

static void test_proc_decl(void) {
    forge_arena_t* arena = arena_create(4096);
    forge_strtable_t* st = strtable_create();
    forge_interp_t* interp = make_interp(arena, st);
    forge_env_t* env = interp->globals;
    
    TEST("Procedure declaration registers in hashmap");
    {
        const char* proc_name = strtable_intern(st, "my_proc", 7);
        forge_node_t* body = ast_block(arena, NULL, 0, 1, 1);
        forge_node_t* proc = ast_proc_decl(arena, proc_name, NULL, 0, NULL, body, 0, 1, 1);
        
        forge_result_t res = interp_exec_stmt(interp, env, proc);
        
        int found = hashmap_has(interp->procedures, proc_name);
        
        if (res.flow == FLOW_NORMAL && found) PASS();
        else FAIL("procedure not registered");
        val_free(&res.value);
    }
    
    interp_destroy(interp);
    strtable_destroy(st);
    arena_destroy(arena);
}

static void test_proc_call_simple(void) {
    forge_arena_t* arena = arena_create(4096);
    forge_strtable_t* st = strtable_create();
    forge_interp_t* interp = make_interp(arena, st);
    forge_env_t* env = interp->globals;
    
    const char* result_name = strtable_intern(st, "result", 6);
    env_define(env, result_name, val_int(0));
    
    TEST("Simple procedure call (no args, no return)");
    {
        /* proc set_result(): result = 42 */
        const char* proc_name = strtable_intern(st, "set_result", 10);
        
        forge_node_t* assign_target = ast_ident(arena, result_name, 1, 1);
        forge_node_t* assign_val = ast_int_lit(arena, 42, 1, 1);
        forge_node_t* assign = ast_assign(arena, assign_target, assign_val, 1, 1);
        forge_node_t* body_stmts[] = { assign };
        forge_node_t* body = ast_block(arena, body_stmts, 1, 1, 1);
        
        forge_node_t* proc = ast_proc_decl(arena, proc_name, NULL, 0, NULL, body, 0, 1, 1);
        interp_exec_stmt(interp, env, proc);
        
        /* Call set_result() */
        forge_value_t ret = interp_call_proc(interp, env, proc_name, NULL, 0);
        forge_value_t v = env_get(env, result_name);
        
        if (v.kind == VAL_INT && v.as.i == 42) PASS();
        else FAIL("expected result = 42");
        val_free(&v);
        val_free(&ret);
    }
    
    interp_destroy(interp);
    strtable_destroy(st);
    arena_destroy(arena);
}

static void test_proc_with_args(void) {
    forge_arena_t* arena = arena_create(4096);
    forge_strtable_t* st = strtable_create();
    forge_interp_t* interp = make_interp(arena, st);
    forge_env_t* env = interp->globals;
    
    TEST("Procedure with arguments");
    {
        /* proc add(a, b): return a + b */
        const char* proc_name = strtable_intern(st, "add", 3);
        const char* a_name = strtable_intern(st, "a", 1);
        const char* b_name = strtable_intern(st, "b", 1);
        
        const char* param_names[] = { a_name, b_name };
        forge_param_t* params = make_params(arena, param_names, 2);
        
        /* Body: return a + b */
        forge_node_t* a_ident = ast_ident(arena, a_name, 1, 1);
        forge_node_t* b_ident = ast_ident(arena, b_name, 1, 1);
        forge_node_t* add_expr = ast_binary_op(arena, TOK_PLUS, a_ident, b_ident, 1, 1);
        forge_node_t* ret_stmt = ast_return(arena, add_expr, 1, 1);
        forge_node_t* body_stmts[] = { ret_stmt };
        forge_node_t* body = ast_block(arena, body_stmts, 1, 1, 1);
        
        forge_node_t* proc = ast_proc_decl(arena, proc_name, params, 2, NULL, body, 0, 1, 1);
        interp_exec_stmt(interp, env, proc);
        
        /* Call add(3, 5) */
        forge_value_t args[] = { val_int(3), val_int(5) };
        forge_value_t ret = interp_call_proc(interp, env, proc_name, args, 2);
        
        if (ret.kind == VAL_INT && ret.as.i == 8) PASS();
        else FAIL("expected return value 8");
        val_free(&ret);
    }

    interp_destroy(interp);
    strtable_destroy(st);
    arena_destroy(arena);
}

static void test_proc_return_value(void) {
    forge_arena_t* arena = arena_create(4096);
    forge_strtable_t* st = strtable_create();
    forge_interp_t* interp = make_interp(arena, st);
    forge_env_t* env = interp->globals;

    TEST("Procedure returns value");
    {
        /* proc get_five(): return 5 */
        const char* proc_name = strtable_intern(st, "get_five", 8);

        forge_node_t* val = ast_int_lit(arena, 5, 1, 1);
        forge_node_t* ret_stmt = ast_return(arena, val, 1, 1);
        forge_node_t* body_stmts[] = { ret_stmt };
        forge_node_t* body = ast_block(arena, body_stmts, 1, 1, 1);

        forge_node_t* proc = ast_proc_decl(arena, proc_name, NULL, 0, NULL, body, 0, 1, 1);
        interp_exec_stmt(interp, env, proc);

        forge_value_t ret = interp_call_proc(interp, env, proc_name, NULL, 0);

        if (ret.kind == VAL_INT && ret.as.i == 5) PASS();
        else FAIL("expected return value 5");
        val_free(&ret);
    }

    interp_destroy(interp);
    strtable_destroy(st);
    arena_destroy(arena);
}

static void test_recursive_proc(void) {
    forge_arena_t* arena = arena_create(8192);
    forge_strtable_t* st = strtable_create();
    forge_interp_t* interp = make_interp(arena, st);
    forge_env_t* env = interp->globals;

    TEST("Recursive procedure (factorial)");
    {
        /* proc fact(n): if n <= 1: return 1 else: return n * fact(n - 1) */
        const char* proc_name = strtable_intern(st, "fact", 4);
        const char* n_name = strtable_intern(st, "n", 1);

        const char* param_names[] = { n_name };
        forge_param_t* params = make_params(arena, param_names, 1);

        /* Condition: n <= 1 */
        forge_node_t* n_ident = ast_ident(arena, n_name, 1, 1);
        forge_node_t* one = ast_int_lit(arena, 1, 1, 1);
        forge_node_t* cond = ast_binary_op(arena, TOK_LEQ, n_ident, one, 1, 1);

        /* Then: return 1 */
        forge_node_t* ret_one = ast_return(arena, ast_int_lit(arena, 1, 1, 1), 1, 1);
        forge_node_t* then_stmts[] = { ret_one };
        forge_node_t* then_block = ast_block(arena, then_stmts, 1, 1, 1);

        /* Else: return n * fact(n - 1) */
        forge_node_t* n_ident2 = ast_ident(arena, n_name, 1, 1);
        forge_node_t* n_minus_1 = ast_binary_op(arena, TOK_MINUS,
            ast_ident(arena, n_name, 1, 1), ast_int_lit(arena, 1, 1, 1), 1, 1);
        forge_node_t* call_args[] = { n_minus_1 };
        forge_node_t* fact_call = ast_call(arena, ast_ident(arena, proc_name, 1, 1), call_args, 1, 1, 1);
        forge_node_t* mult = ast_binary_op(arena, TOK_STAR, n_ident2, fact_call, 1, 1);
        forge_node_t* ret_mult = ast_return(arena, mult, 1, 1);
        forge_node_t* else_stmts[] = { ret_mult };
        forge_node_t* else_block = ast_block(arena, else_stmts, 1, 1, 1);

        /* If statement */
        forge_node_t* if_stmt = ast_if(arena, cond, then_block, NULL, NULL, 0, else_block, 1, 1);
        forge_node_t* body_stmts[] = { if_stmt };
        forge_node_t* body = ast_block(arena, body_stmts, 1, 1, 1);

        forge_node_t* proc = ast_proc_decl(arena, proc_name, params, 1, NULL, body, 0, 1, 1);
        interp_exec_stmt(interp, env, proc);

        /* Call fact(5) = 120 */
        forge_value_t args[] = { val_int(5) };
        forge_value_t ret = interp_call_proc(interp, env, proc_name, args, 1);

        if (ret.kind == VAL_INT && ret.as.i == 120) PASS();
        else FAIL("expected fact(5) = 120");
        val_free(&ret);
    }

    interp_destroy(interp);
    strtable_destroy(st);
    arena_destroy(arena);
}

static void test_proc_wrong_arg_count(void) {
    forge_arena_t* arena = arena_create(4096);
    forge_strtable_t* st = strtable_create();
    forge_interp_t* interp = make_interp(arena, st);
    forge_env_t* env = interp->globals;

    TEST("Procedure with wrong argument count (error)");
    {
        const char* proc_name = strtable_intern(st, "needs_two", 9);
        const char* a_name = strtable_intern(st, "a", 1);
        const char* b_name = strtable_intern(st, "b", 1);

        const char* param_names[] = { a_name, b_name };
        forge_param_t* params = make_params(arena, param_names, 2);

        forge_node_t* body = ast_block(arena, NULL, 0, 1, 1);

        forge_node_t* proc = ast_proc_decl(arena, proc_name, params, 2, NULL, body, 0, 1, 1);
        interp_exec_stmt(interp, env, proc);

        /* Call with wrong number of args */
        interp->had_error = 0;
        forge_value_t args[] = { val_int(1) };
        forge_value_t ret = interp_call_proc(interp, env, proc_name, args, 1);

        if (interp->had_error) PASS();
        else FAIL("expected argument count error");
        val_free(&ret);
    }

    interp_destroy(interp);
    strtable_destroy(st);
    arena_destroy(arena);
}

static void test_undefined_proc(void) {
    forge_arena_t* arena = arena_create(4096);
    forge_strtable_t* st = strtable_create();
    forge_interp_t* interp = make_interp(arena, st);
    forge_env_t* env = interp->globals;

    TEST("Call undefined procedure (error)");
    {
        interp->had_error = 0;
        forge_value_t ret = interp_call_proc(interp, env, "nonexistent", NULL, 0);

        if (interp->had_error) PASS();
        else FAIL("expected undefined procedure error");
        val_free(&ret);
    }

    interp_destroy(interp);
    strtable_destroy(st);
    arena_destroy(arena);
}

static void test_builtin_str(void) {
    forge_arena_t* arena = arena_create(4096);
    forge_strtable_t* st = strtable_create();
    forge_interp_t* interp = make_interp(arena, st);
    forge_env_t* env = interp->globals;

    TEST("Builtin str() converts int to string");
    {
        forge_value_t args[1] = { val_int(42) };
        forge_value_t ret = interp_call_proc(interp, env, "str", args, 1);

        if (ret.kind == VAL_STR && strcmp(ret.as.str.data, "42") == 0) PASS();
        else FAIL("expected string '42'");
        val_free(&ret);
        val_free(&args[0]);
    }

    interp_destroy(interp);
    strtable_destroy(st);
    arena_destroy(arena);
}

static void test_builtin_append(void) {
    forge_arena_t* arena = arena_create(4096);
    forge_strtable_t* st = strtable_create();
    forge_interp_t* interp = make_interp(arena, st);
    forge_env_t* env = interp->globals;

    TEST("Builtin append() adds element to array");
    {
        forge_value_t elems[2] = { val_int(1), val_int(2) };
        forge_value_t arr = val_array_from(elems, 2);
        forge_value_t new_elem = val_int(3);
        forge_value_t args[2] = { arr, new_elem };

        forge_value_t ret = interp_call_proc(interp, env, "append", args, 2);

        if (ret.kind == VAL_ARRAY && ret.as.array.len == 3 &&
            ret.as.array.elems[2].as.i == 3) PASS();
        else FAIL("expected array [1, 2, 3]");

        val_free(&ret);
        val_free(&args[0]);
        val_free(&args[1]);
        val_free(&elems[0]);
        val_free(&elems[1]);
    }

    interp_destroy(interp);
    strtable_destroy(st);
    arena_destroy(arena);
}

static void test_builtin_type(void) {
    forge_arena_t* arena = arena_create(4096);
    forge_strtable_t* st = strtable_create();
    forge_interp_t* interp = make_interp(arena, st);
    forge_env_t* env = interp->globals;

    TEST("Builtin type() returns type name");
    {
        forge_value_t args[1] = { val_int(42) };
        forge_value_t ret = interp_call_proc(interp, env, "type", args, 1);

        if (ret.kind == VAL_STR && strcmp(ret.as.str.data, "int") == 0) PASS();
        else FAIL("expected string 'int'");
        val_free(&ret);
        val_free(&args[0]);
    }

    interp_destroy(interp);
    strtable_destroy(st);
    arena_destroy(arena);
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Main
 * ───────────────────────────────────────────────────────────────────────────── */

int main(void) {
    printf("\n=== FORGE Interpreter Procedure Tests ===\n\n");

    test_proc_decl();
    test_proc_call_simple();
    test_proc_with_args();
    test_proc_return_value();
    test_recursive_proc();
    test_proc_wrong_arg_count();
    test_undefined_proc();
    test_builtin_str();
    test_builtin_append();
    test_builtin_type();

    printf("\n=== Results: %d/%d tests passed ===\n\n", pass_count, test_count);
    return (pass_count == test_count) ? 0 : 1;
}

