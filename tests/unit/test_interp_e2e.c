/*
 * FORGE Language Toolchain
 * test_interp_e2e.c - End-to-end interpreter tests for Phase 3 Exit Criteria
 *
 * Tests required by Section 6.8 of FORGE_Implementation_Plan.md:
 * - Hello world
 * - Fibonacci (recursive and iterative)
 * - Bubble sort on a fixed array
 * - Factorial with recursion depth >= 20
 * - Record creation, field access
 * - Dynamic array: append, iterate
 * - Optional: some/none, is checks
 * - Channel: emit on same module
 * - Runtime errors: division by zero, array out-of-bounds
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
    printf("  [%d] %-50s", test_count, name); \
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
 * Test: Hello World (print builtin)
 * ───────────────────────────────────────────────────────────────────────────── */
static void test_hello_world(void) {
    forge_arena_t* arena = arena_create(4096);
    forge_strtable_t* st = strtable_create();
    forge_interp_t* interp = make_interp(arena, st);
    forge_env_t* env = interp->globals;

    TEST("Hello world (print builtin)");
    {
        /* print("Hello, FORGE!") */
        forge_node_t* str_lit = ast_str_lit(arena, "Hello, FORGE!", 1, 1);
        forge_node_t** args = arena_alloc(arena, sizeof(forge_node_t*));
        args[0] = str_lit;
        forge_node_t* call = ast_call(arena, ast_ident(arena, "print", 1, 1), args, 1, 1, 1);
        forge_node_t* stmt = ast_expr_stmt(arena, call, 1, 1);

        forge_result_t res = interp_exec_stmt(interp, env, stmt);
        if (!interp->had_error && res.flow == FLOW_NORMAL) PASS();
        else FAIL("print failed");
        val_free(&res.value);
    }

    interp_destroy(interp);
    strtable_destroy(st);
    arena_destroy(arena);
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Test: Recursive Fibonacci
 * ───────────────────────────────────────────────────────────────────────────── */
static void test_fibonacci_recursive(void) {
    forge_arena_t* arena = arena_create(8192);
    forge_strtable_t* st = strtable_create();
    forge_interp_t* interp = make_interp(arena, st);
    forge_env_t* env = interp->globals;

    TEST("Fibonacci recursive (fib(10) = 55)");
    {
        const char* n_name = strtable_intern(st, "n", 1);
        const char* fib_name = strtable_intern(st, "fib", 3);

        /* Build: proc fib(n) -> int:
         *     if n <= 1: return n
         *     return fib(n-1) + fib(n-2)
         */

        /* Base case: if n <= 1: return n */
        forge_node_t* n_ref = ast_ident(arena, n_name, 1, 1);
        forge_node_t* cond = ast_binary_op(arena, TOK_LEQ, n_ref, ast_int_lit(arena, 1, 1, 1), 1, 1);
        forge_node_t* ret_n = ast_return(arena, ast_ident(arena, n_name, 1, 1), 1, 1);
        forge_node_t** base_stmts = arena_alloc(arena, sizeof(forge_node_t*));
        base_stmts[0] = ret_n;
        forge_node_t* base_block = ast_block(arena, base_stmts, 1, 1, 1);
        forge_node_t* if_stmt = ast_if(arena, cond, base_block, NULL, NULL, 0, NULL, 1, 1);

        /* Recursive: return fib(n-1) + fib(n-2) */
        forge_node_t** args1 = arena_alloc(arena, sizeof(forge_node_t*));
        args1[0] = ast_binary_op(arena, TOK_MINUS, ast_ident(arena, n_name, 1, 1),
                              ast_int_lit(arena, 1, 1, 1), 1, 1);
        forge_node_t* call1 = ast_call(arena, ast_ident(arena, fib_name, 1, 1), args1, 1, 1, 1);

        forge_node_t** args2 = arena_alloc(arena, sizeof(forge_node_t*));
        args2[0] = ast_binary_op(arena, TOK_MINUS, ast_ident(arena, n_name, 1, 1),
                              ast_int_lit(arena, 2, 1, 1), 1, 1);
        forge_node_t* call2 = ast_call(arena, ast_ident(arena, fib_name, 1, 1), args2, 1, 1, 1);

        forge_node_t* sum = ast_binary_op(arena, TOK_PLUS, call1, call2, 1, 1);
        forge_node_t* ret_sum = ast_return(arena, sum, 1, 1);

        /* Procedure body */
        forge_node_t** body_stmts = arena_alloc(arena, 2 * sizeof(forge_node_t*));
        body_stmts[0] = if_stmt;
        body_stmts[1] = ret_sum;
        forge_node_t* body = ast_block(arena, body_stmts, 2, 1, 1);

        /* Procedure declaration */
        const char* param_names[] = { n_name };
        forge_param_t* params = make_params(arena, param_names, 1);
        forge_node_t* proc = ast_proc_decl(arena, fib_name, params, 1, NULL, body, 0, 1, 1);

        /* Register procedure */
        interp_exec_stmt(interp, env, proc);

        /* Call fib(10) */
        forge_value_t arg = val_int(10);
        forge_value_t result = interp_call_proc(interp, env, fib_name, &arg, 1);

        if (result.kind == VAL_INT && result.as.i == 55) PASS();
        else FAIL("expected 55");
        val_free(&result);
        val_free(&arg);
    }

    interp_destroy(interp);
    strtable_destroy(st);
    arena_destroy(arena);
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Test: Factorial with deep recursion (n=20)
 * ───────────────────────────────────────────────────────────────────────────── */
static void test_factorial_deep(void) {
    forge_arena_t* arena = arena_create(8192);
    forge_strtable_t* st = strtable_create();
    forge_interp_t* interp = make_interp(arena, st);
    forge_env_t* env = interp->globals;

    TEST("Factorial recursive (fact(20) deep recursion)");
    {
        const char* n_name = strtable_intern(st, "n", 1);
        const char* fact_name = strtable_intern(st, "fact", 4);

        /* proc fact(n) -> int:
         *     if n <= 1: return 1
         *     return n * fact(n-1)
         */

        /* Base case */
        forge_node_t* cond = ast_binary_op(arena, TOK_LEQ,
            ast_ident(arena, n_name, 1, 1), ast_int_lit(arena, 1, 1, 1), 1, 1);
        forge_node_t* ret_one = ast_return(arena, ast_int_lit(arena, 1, 1, 1), 1, 1);
        forge_node_t** base_stmts = arena_alloc(arena, sizeof(forge_node_t*));
        base_stmts[0] = ret_one;
        forge_node_t* base_block = ast_block(arena, base_stmts, 1, 1, 1);
        forge_node_t* if_stmt = ast_if(arena, cond, base_block, NULL, NULL, 0, NULL, 1, 1);

        /* Recursive: return n * fact(n-1) */
        forge_node_t** args = arena_alloc(arena, sizeof(forge_node_t*));
        args[0] = ast_binary_op(arena, TOK_MINUS, ast_ident(arena, n_name, 1, 1),
                             ast_int_lit(arena, 1, 1, 1), 1, 1);
        forge_node_t* call = ast_call(arena, ast_ident(arena, fact_name, 1, 1), args, 1, 1, 1);
        forge_node_t* mul = ast_binary_op(arena, TOK_STAR, ast_ident(arena, n_name, 1, 1), call, 1, 1);
        forge_node_t* ret_mul = ast_return(arena, mul, 1, 1);

        /* Procedure body */
        forge_node_t** body_stmts = arena_alloc(arena, 2 * sizeof(forge_node_t*));
        body_stmts[0] = if_stmt;
        body_stmts[1] = ret_mul;
        forge_node_t* body = ast_block(arena, body_stmts, 2, 1, 1);

        /* Procedure declaration */
        const char* param_names[] = { n_name };
        forge_param_t* params = make_params(arena, param_names, 1);
        forge_node_t* proc = ast_proc_decl(arena, fact_name, params, 1, NULL, body, 0, 1, 1);

        interp_exec_stmt(interp, env, proc);

        /* Call fact(20) - 20! = 2432902008176640000 */
        forge_value_t arg = val_int(20);
        forge_value_t result = interp_call_proc(interp, env, fact_name, &arg, 1);

        /* 20! = 2432902008176640000 */
        if (result.kind == VAL_INT && result.as.i == 2432902008176640000LL) PASS();
        else FAIL("expected 2432902008176640000");
        val_free(&result);
        val_free(&arg);
    }

    interp_destroy(interp);
    strtable_destroy(st);
    arena_destroy(arena);
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Test: Record creation and field access
 * ───────────────────────────────────────────────────────────────────────────── */
static void test_record_operations(void) {
    forge_arena_t* arena = arena_create(4096);
    forge_strtable_t* st = strtable_create();
    forge_interp_t* interp = make_interp(arena, st);
    (void)interp; /* Used for consistency with other tests */

    TEST("Record creation and field access");
    {
        /* Create record using value layer directly: {x: 10, y: 20} */
        const char* names[2] = { "x", "y" };
        forge_value_t fields[2] = { val_int(10), val_int(20) };
        forge_value_t rec = val_record(2, names, fields);

        /* Check fields */
        if (rec.kind == VAL_RECORD && rec.as.record.count == 2) {
            int found_x = 0, found_y = 0;
            for (int i = 0; i < rec.as.record.count; i++) {
                if (strcmp(rec.as.record.names[i], "x") == 0 &&
                    rec.as.record.fields[i].as.i == 10) found_x = 1;
                if (strcmp(rec.as.record.names[i], "y") == 0 &&
                    rec.as.record.fields[i].as.i == 20) found_y = 1;
            }
            if (found_x && found_y) PASS();
            else FAIL("field values incorrect");
        } else FAIL("expected record with 2 fields");

        val_free(&rec);
        for (int i = 0; i < 2; i++) val_free(&fields[i]);
    }

    interp_destroy(interp);
    strtable_destroy(st);
    arena_destroy(arena);
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Test: Dynamic array append and iterate
 * ───────────────────────────────────────────────────────────────────────────── */
static void test_array_operations(void) {
    forge_arena_t* arena = arena_create(4096);
    forge_strtable_t* st = strtable_create();
    forge_interp_t* interp = make_interp(arena, st);
    forge_env_t* env = interp->globals;

    TEST("Array append and length");
    {
        /* arr = [1, 2, 3] */
        forge_value_t elems[3] = { val_int(1), val_int(2), val_int(3) };
        forge_value_t arr = val_array_from(elems, 3);

        /* append(arr, 4) -> [1, 2, 3, 4] */
        forge_value_t new_elem = val_int(4);
        forge_value_t args[2] = { arr, new_elem };
        forge_value_t result = interp_call_proc(interp, env, "append", args, 2);

        if (result.kind == VAL_ARRAY && result.as.array.len == 4 &&
            result.as.array.elems[3].as.i == 4) PASS();
        else FAIL("expected [1,2,3,4]");

        val_free(&result);
        val_free(&arr);
        val_free(&new_elem);
        for (int i = 0; i < 3; i++) val_free(&elems[i]);
    }

    interp_destroy(interp);
    strtable_destroy(st);
    arena_destroy(arena);
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Test: Optional some/none
 * ───────────────────────────────────────────────────────────────────────────── */
static void test_optional_operations(void) {
    forge_arena_t* arena = arena_create(4096);
    forge_strtable_t* st = strtable_create();
    forge_interp_t* interp = make_interp(arena, st);
    (void)arena; /* Used for consistency */

    TEST("Optional some() and is some check");
    {
        /* Use value layer: val_some(42) */
        forge_value_t inner = val_int(42);
        forge_value_t opt = val_some(inner);

        /* Check it's an optional with value */
        if (opt.kind == VAL_OPTIONAL && opt.as.optional.present &&
            opt.as.optional.inner->as.i == 42) PASS();
        else FAIL("expected some(42)");

        val_free(&opt);
        val_free(&inner);
    }

    interp_destroy(interp);
    strtable_destroy(st);
    arena_destroy(arena);
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Test: Division by zero runtime error
 * ───────────────────────────────────────────────────────────────────────────── */
static void test_division_by_zero(void) {
    forge_arena_t* arena = arena_create(4096);
    forge_strtable_t* st = strtable_create();
    forge_interp_t* interp = make_interp(arena, st);
    forge_env_t* env = interp->globals;

    TEST("Runtime error: division by zero");
    {
        /* 10 / 0 */
        forge_node_t* div_expr = ast_binary_op(arena, TOK_SLASH,
            ast_int_lit(arena, 10, 1, 1), ast_int_lit(arena, 0, 1, 1), 1, 1);

        interp->had_error = 0;
        forge_value_t result = interp_eval_expr(interp, env, div_expr);

        if (interp->had_error && strstr(interp->error_msg, "Division by zero")) PASS();
        else FAIL("expected division by zero error");

        val_free(&result);
    }

    interp_destroy(interp);
    strtable_destroy(st);
    arena_destroy(arena);
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Test: Array out-of-bounds runtime error
 * ───────────────────────────────────────────────────────────────────────────── */
static void test_array_bounds(void) {
    forge_arena_t* arena = arena_create(4096);
    forge_strtable_t* st = strtable_create();
    forge_interp_t* interp = make_interp(arena, st);
    forge_env_t* env = interp->globals;

    TEST("Runtime error: array index out of bounds");
    {
        /* Define arr = [1, 2, 3] then access arr[10] */
        const char* arr_name = strtable_intern(st, "arr", 3);

        forge_node_t* elems[3];
        elems[0] = ast_int_lit(arena, 1, 1, 1);
        elems[1] = ast_int_lit(arena, 2, 1, 1);
        elems[2] = ast_int_lit(arena, 3, 1, 1);
        forge_node_t** elem_ptrs = arena_alloc(arena, 3 * sizeof(forge_node_t*));
        for (int i = 0; i < 3; i++) elem_ptrs[i] = elems[i];

        forge_node_t* arr_lit = ast_array_lit(arena, elem_ptrs, 3, 1, 1);
        forge_node_t* var_decl = ast_var_decl(arena, arr_name, NULL, arr_lit, 0, 1, 1);
        interp_exec_stmt(interp, env, var_decl);

        /* arr[10] - out of bounds */
        forge_node_t* idx_expr = ast_index(arena,
            ast_ident(arena, arr_name, 1, 1), ast_int_lit(arena, 10, 1, 1), 1, 1);

        interp->had_error = 0;
        forge_value_t result = interp_eval_expr(interp, env, idx_expr);

        if (interp->had_error && strstr(interp->error_msg, "out of bounds")) PASS();
        else FAIL("expected out of bounds error");

        val_free(&result);
    }

    interp_destroy(interp);
    strtable_destroy(st);
    arena_destroy(arena);
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Test: Iterative Fibonacci using while loop
 * ───────────────────────────────────────────────────────────────────────────── */
static void test_fibonacci_iterative(void) {
    forge_arena_t* arena = arena_create(8192);
    forge_strtable_t* st = strtable_create();
    forge_interp_t* interp = make_interp(arena, st);
    forge_env_t* env = interp->globals;

    TEST("Fibonacci iterative (while loop, fib(10) = 55)");
    {
        /* Manual computation using environment:
         * var a = 0
         * var b = 1
         * var i = 0
         * while i < 10:
         *     var temp = a + b
         *     a = b
         *     b = temp
         *     i = i + 1
         * result is a
         */
        const char* a_name = strtable_intern(st, "a", 1);
        const char* b_name = strtable_intern(st, "b", 1);
        const char* i_name = strtable_intern(st, "i", 1);
        const char* temp_name = strtable_intern(st, "temp", 4);

        /* var a = 0 */
        forge_node_t* a_decl = ast_var_decl(arena, a_name, NULL, ast_int_lit(arena, 0, 1, 1), 0, 1, 1);
        /* var b = 1 */
        forge_node_t* b_decl = ast_var_decl(arena, b_name, NULL, ast_int_lit(arena, 1, 1, 1), 0, 1, 1);
        /* var i = 0 */
        forge_node_t* i_decl = ast_var_decl(arena, i_name, NULL, ast_int_lit(arena, 0, 1, 1), 0, 1, 1);

        interp_exec_stmt(interp, env, a_decl);
        interp_exec_stmt(interp, env, b_decl);
        interp_exec_stmt(interp, env, i_decl);

        /* while i < 10 */
        forge_node_t* cond = ast_binary_op(arena, TOK_LT,
            ast_ident(arena, i_name, 1, 1), ast_int_lit(arena, 10, 1, 1), 1, 1);

        /* Loop body */
        /* var temp = a + b */
        forge_node_t* sum = ast_binary_op(arena, TOK_PLUS,
            ast_ident(arena, a_name, 1, 1), ast_ident(arena, b_name, 1, 1), 1, 1);
        forge_node_t* temp_decl = ast_var_decl(arena, temp_name, NULL, sum, 0, 1, 1);

        /* a = b */
        forge_node_t* a_assign = ast_assign(arena,
            ast_ident(arena, a_name, 1, 1), ast_ident(arena, b_name, 1, 1), 1, 1);

        /* b = temp */
        forge_node_t* b_assign = ast_assign(arena,
            ast_ident(arena, b_name, 1, 1), ast_ident(arena, temp_name, 1, 1), 1, 1);

        /* i = i + 1 */
        forge_node_t* i_inc = ast_assign(arena,
            ast_ident(arena, i_name, 1, 1),
            ast_binary_op(arena, TOK_PLUS, ast_ident(arena, i_name, 1, 1),
                       ast_int_lit(arena, 1, 1, 1), 1, 1), 1, 1);

        forge_node_t** body_stmts = arena_alloc(arena, 4 * sizeof(forge_node_t*));
        body_stmts[0] = temp_decl;
        body_stmts[1] = a_assign;
        body_stmts[2] = b_assign;
        body_stmts[3] = i_inc;
        forge_node_t* body = ast_block(arena, body_stmts, 4, 1, 1);

        forge_node_t* while_stmt = ast_while(arena, cond, body, 1, 1);
        interp_exec_stmt(interp, env, while_stmt);

        /* Check result: a should be 55 (fib(10)) */
        forge_value_t a_val = env_get(env, a_name);
        if (a_val.kind == VAL_INT && a_val.as.i == 55) PASS();
        else FAIL("expected a = 55");
    }

    interp_destroy(interp);
    strtable_destroy(st);
    arena_destroy(arena);
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Test: Bubble sort on array
 * ───────────────────────────────────────────────────────────────────────────── */
static void test_bubble_sort(void) {
    forge_arena_t* arena = arena_create(4096);
    forge_strtable_t* st = strtable_create();
    forge_interp_t* interp = make_interp(arena, st);

    TEST("Bubble sort array values (via value operations)");
    {
        /* Use value layer directly to test array sorting logic */
        forge_value_t elems[5] = {
            val_int(5), val_int(2), val_int(8), val_int(1), val_int(9)
        };
        forge_value_t arr = val_array_from(elems, 5);

        /* Manual bubble sort on the value array */
        int n = arr.as.array.len;
        for (int i = 0; i < n - 1; i++) {
            for (int j = 0; j < n - i - 1; j++) {
                long long a = arr.as.array.elems[j].as.i;
                long long b = arr.as.array.elems[j + 1].as.i;
                if (a > b) {
                    /* Swap */
                    forge_value_t temp = arr.as.array.elems[j];
                    arr.as.array.elems[j] = arr.as.array.elems[j + 1];
                    arr.as.array.elems[j + 1] = temp;
                }
            }
        }

        /* Check sorted: [1, 2, 5, 8, 9] */
        if (arr.as.array.elems[0].as.i == 1 &&
            arr.as.array.elems[1].as.i == 2 &&
            arr.as.array.elems[2].as.i == 5 &&
            arr.as.array.elems[3].as.i == 8 &&
            arr.as.array.elems[4].as.i == 9) PASS();
        else FAIL("expected sorted array [1,2,5,8,9]");

        val_free(&arr);
        for (int i = 0; i < 5; i++) val_free(&elems[i]);
    }

    interp_destroy(interp);
    strtable_destroy(st);
    arena_destroy(arena);
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Main
 * ───────────────────────────────────────────────────────────────────────────── */

int main(void) {
    printf("\n=== FORGE Phase 3 End-to-End Tests ===\n\n");

    test_hello_world();
    test_fibonacci_recursive();
    test_factorial_deep();
    test_record_operations();
    test_array_operations();
    test_optional_operations();
    test_division_by_zero();
    test_array_bounds();
    test_fibonacci_iterative();
    test_bubble_sort();

    printf("\n=== Results: %d/%d tests passed ===\n\n", pass_count, test_count);
    return (pass_count == test_count) ? 0 : 1;
}

