/*
 * FORGE Language Toolchain
 * test_interp_module.c - Unit tests for module loading and cross-module access
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

static void test_module_load(void) {
    forge_arena_t* arena = arena_create(4096);
    forge_strtable_t* st = strtable_create();
    forge_interp_t* interp = make_interp(arena, st);
    
    TEST("Load module registers in hashmap");
    {
        const char* mod_name = strtable_intern(st, "math", 4);
        
        /* Create a simple module program with one procedure */
        const char* proc_name = strtable_intern(st, "add", 3);
        forge_node_t* body = ast_block(arena, NULL, 0, 1, 1);
        forge_node_t* proc = ast_proc_decl(arena, proc_name, NULL, 0, NULL, body, 1, 1, 1);
        
        forge_node_t* decls[1] = { proc };
        forge_node_t* program = ast_program(arena, NULL, 0, decls, 1);
        
        forge_module_t* module = interp_load_module(interp, mod_name, "math.frg", program);
        
        if (module && hashmap_has(interp->modules, mod_name)) PASS();
        else FAIL("module not loaded");
    }
    
    interp_destroy(interp);
    strtable_destroy(st);
    arena_destroy(arena);
}

static void test_module_get(void) {
    forge_arena_t* arena = arena_create(4096);
    forge_strtable_t* st = strtable_create();
    forge_interp_t* interp = make_interp(arena, st);
    
    TEST("Get module by name");
    {
        const char* mod_name = strtable_intern(st, "utils", 5);
        
        forge_node_t* program = ast_program(arena, NULL, 0, NULL, 0);
        interp_load_module(interp, mod_name, "utils.frg", program);
        
        forge_module_t* found = interp_get_module(interp, mod_name);
        
        if (found && strcmp(found->name, mod_name) == 0) PASS();
        else FAIL("module not found");
    }
    
    interp_destroy(interp);
    strtable_destroy(st);
    arena_destroy(arena);
}

static void test_module_var_access(void) {
    forge_arena_t* arena = arena_create(4096);
    forge_strtable_t* st = strtable_create();
    forge_interp_t* interp = make_interp(arena, st);
    forge_env_t* env = interp->globals;
    
    TEST("Access module variable via qualified ident");
    {
        const char* mod_name = strtable_intern(st, "constants", 9);
        const char* var_name = strtable_intern(st, "PI", 2);
        
        /* Create module with const PI = 3.14159 */
        forge_node_t* value = ast_float_lit(arena, 3.14159, 1, 1);
        forge_node_t* const_decl = ast_const_decl(arena, var_name, NULL, value, 1, 1, 1);

        forge_node_t* decls[1] = { const_decl };
        forge_node_t* program = ast_program(arena, NULL, 0, decls, 1);

        interp_load_module(interp, mod_name, "constants.frg", program);

        /* Now access constants.PI via qualified identifier */
        forge_node_t* qident = ast_qualified_ident(arena, mod_name, var_name, 1, 1);
        forge_value_t result = interp_eval_expr(interp, env, qident);

        if (result.kind == VAL_FLOAT && result.as.f > 3.14 && result.as.f < 3.15) {
            PASS();
        } else {
            FAIL("wrong value");
        }
        val_free(&result);
    }
    
    interp_destroy(interp);
    strtable_destroy(st);
    arena_destroy(arena);
}

static void test_module_proc_call(void) {
    forge_arena_t* arena = arena_create(4096);
    forge_strtable_t* st = strtable_create();
    forge_interp_t* interp = make_interp(arena, st);
    forge_env_t* env = interp->globals;
    
    TEST("Call procedure in another module");
    {
        const char* mod_name = strtable_intern(st, "mymath", 6);
        const char* proc_name = strtable_intern(st, "double_it", 9);
        const char* param_x = strtable_intern(st, "x", 1);
        
        /* Create module with: proc double_it(x) -> return x * 2 */
        const char* param_names[1] = { param_x };
        forge_param_t* params = make_params(arena, param_names, 1);

        /* Body: return x * 2 */
        forge_node_t* x_ref = ast_ident(arena, param_x, 1, 1);
        forge_node_t* two = ast_int_lit(arena, 2, 1, 1);
        forge_node_t* mult = ast_binary_op(arena, TOK_STAR, x_ref, two, 1, 1);
        forge_node_t* ret_stmt = ast_return(arena, mult, 1, 1);
        forge_node_t* body_stmts[1] = { ret_stmt };
        forge_node_t* body = ast_block(arena, body_stmts, 1, 1, 1);

        forge_node_t* proc = ast_proc_decl(arena, proc_name, params, 1, NULL, body, 1, 1, 1);
        forge_node_t* decls[1] = { proc };
        forge_node_t* program = ast_program(arena, NULL, 0, decls, 1);

        interp_load_module(interp, mod_name, "mymath.frg", program);

        /* Call mymath.double_it(21) */
        forge_node_t* callee = ast_qualified_ident(arena, mod_name, proc_name, 1, 1);
        forge_node_t* arg = ast_int_lit(arena, 21, 1, 1);
        forge_node_t* args[1] = { arg };
        forge_node_t* call = ast_call(arena, callee, args, 1, 1, 1);

        forge_value_t result = interp_eval_expr(interp, env, call);

        if (result.kind == VAL_INT && result.as.i == 42) {
            PASS();
        } else {
            FAIL("expected 42");
        }
        val_free(&result);
    }

    interp_destroy(interp);
    strtable_destroy(st);
    arena_destroy(arena);
}

static void test_module_not_found(void) {
    forge_arena_t* arena = arena_create(4096);
    forge_strtable_t* st = strtable_create();
    forge_interp_t* interp = make_interp(arena, st);
    forge_env_t* env = interp->globals;

    TEST("Access undefined module (error)");
    {
        const char* mod_name = strtable_intern(st, "nonexistent", 11);
        const char* var_name = strtable_intern(st, "foo", 3);

        forge_node_t* qident = ast_qualified_ident(arena, mod_name, var_name, 1, 1);

        interp->had_error = 0;
        forge_value_t result = interp_eval_expr(interp, env, qident);

        if (interp->had_error) PASS();
        else FAIL("expected error");
        val_free(&result);
    }

    interp_destroy(interp);
    strtable_destroy(st);
    arena_destroy(arena);
}

static void test_module_undefined_symbol(void) {
    forge_arena_t* arena = arena_create(4096);
    forge_strtable_t* st = strtable_create();
    forge_interp_t* interp = make_interp(arena, st);
    forge_env_t* env = interp->globals;

    TEST("Access undefined symbol in module (error)");
    {
        const char* mod_name = strtable_intern(st, "empty_mod", 9);
        const char* var_name = strtable_intern(st, "missing", 7);

        /* Create empty module */
        forge_node_t* program = ast_program(arena, NULL, 0, NULL, 0);
        interp_load_module(interp, mod_name, "empty.frg", program);

        forge_node_t* qident = ast_qualified_ident(arena, mod_name, var_name, 1, 1);

        interp->had_error = 0;
        forge_value_t result = interp_eval_expr(interp, env, qident);

        if (interp->had_error) PASS();
        else FAIL("expected error");
        val_free(&result);
    }

    interp_destroy(interp);
    strtable_destroy(st);
    arena_destroy(arena);
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Main
 * ───────────────────────────────────────────────────────────────────────────── */

int main(void) {
    printf("=== FORGE Interpreter Module Tests ===\n\n");

    test_module_load();
    test_module_get();
    test_module_var_access();
    test_module_proc_call();
    test_module_not_found();
    test_module_undefined_symbol();

    printf("\n=== Results: %d/%d tests passed ===\n", pass_count, test_count);
    return (pass_count == test_count) ? 0 : 1;
}

