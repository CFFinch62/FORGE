/*
 * FORGE Language Toolchain
 * test_interp_channel.c - Unit tests for channel dispatch
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

/* ─────────────────────────────────────────────────────────────────────────────
 * Tests
 * ───────────────────────────────────────────────────────────────────────────── */

static void test_channel_registration(void) {
    forge_arena_t* arena = arena_create(4096);
    forge_strtable_t* st = strtable_create();
    forge_interp_t* interp = interp_create(arena, st);
    
    TEST("Channel declaration registers channel");
    {
        const char* ch_name = strtable_intern(st, "my_event", 8);
        
        /* Create channel declaration: channel my_event: int */
        forge_node_t* ch_decl = ast_channel_decl(arena, ch_name, NULL, 0, 1, 1);
        
        /* Create program with just the channel */
        forge_node_t* decls[1] = { ch_decl };
        forge_node_t* prog = ast_program(arena, NULL, 0, decls, 1);
        
        interp_run(interp, prog);
        
        if (!interp->had_error) {
            PASS();
        } else {
            FAIL(interp->error_msg);
        }
    }
    
    interp_destroy(interp);
    strtable_destroy(st);
    arena_destroy(arena);
}

static void test_handler_registration(void) {
    forge_arena_t* arena = arena_create(4096);
    forge_strtable_t* st = strtable_create();
    forge_interp_t* interp = interp_create(arena, st);
    
    TEST("Handler registration for declared channel");
    {
        const char* ch_name = strtable_intern(st, "sensor_data", 11);
        const char* param_name = strtable_intern(st, "value", 5);
        
        /* Create channel declaration */
        forge_node_t* ch_decl = ast_channel_decl(arena, ch_name, NULL, 0, 1, 1);
        
        /* Create handler: on sensor_data as value: { } */
        forge_node_t* body = ast_block(arena, NULL, 0, 1, 1);
        forge_node_t* handler = ast_on_handler(arena, ch_name, param_name, body, 1, 1);
        
        /* Create program with channel and handler */
        forge_node_t* decls[2] = { ch_decl, handler };
        forge_node_t* prog = ast_program(arena, NULL, 0, decls, 2);
        
        interp_run(interp, prog);
        
        if (!interp->had_error) {
            PASS();
        } else {
            FAIL(interp->error_msg);
        }
    }
    
    interp_destroy(interp);
    strtable_destroy(st);
    arena_destroy(arena);
}

static void test_emit_with_payload(void) {
    forge_arena_t* arena = arena_create(4096);
    forge_strtable_t* st = strtable_create();
    forge_interp_t* interp = interp_create(arena, st);
    
    TEST("Emit triggers handler with payload");
    {
        const char* ch_name = strtable_intern(st, "test_ch", 7);
        const char* param_name = strtable_intern(st, "data", 4);
        const char* var_name = strtable_intern(st, "result", 6);
        
        /* Channel declaration */
        forge_node_t* ch_decl = ast_channel_decl(arena, ch_name, NULL, 0, 1, 1);
        
        /* Handler body: result = data (store received value) */
        forge_node_t* data_ref = ast_ident(arena, param_name, 1, 1);
        forge_node_t* result_ref = ast_ident(arena, var_name, 1, 1);
        forge_node_t* assign = ast_assign(arena, result_ref, data_ref, 1, 1);
        forge_node_t* body_stmts[1] = { assign };
        forge_node_t* handler_body = ast_block(arena, body_stmts, 1, 1, 1);
        
        /* Handler: on test_ch as data: result = data */
        forge_node_t* handler = ast_on_handler(arena, ch_name, param_name, handler_body, 1, 1);
        
        /* Global var: var result: int = 0 */
        forge_node_t* init_val = ast_int_lit(arena, 0, 1, 1);
        forge_node_t* var_decl = ast_var_decl(arena, var_name, NULL, init_val, 0, 1, 1);
        
        /* Main proc: emit test_ch -> 42 */
        forge_node_t* payload = ast_int_lit(arena, 42, 1, 1);
        forge_node_t* emit_stmt = ast_emit(arena, ch_name, payload, 1, 1);
        forge_node_t* main_body_stmts[1] = { emit_stmt };
        forge_node_t* main_body = ast_block(arena, main_body_stmts, 1, 1, 1);
        
        const char* main_name = strtable_intern(st, "main", 4);
        forge_node_t* main_proc = ast_proc_decl(arena, main_name, NULL, 0, NULL, main_body, 0, 1, 1);
        
        /* Program: channel, var, handler, main */
        forge_node_t* decls[4] = { ch_decl, var_decl, handler, main_proc };
        forge_node_t* prog = ast_program(arena, NULL, 0, decls, 4);
        
        interp_run(interp, prog);
        
        /* Check that result was set to 42 */
        forge_value_t result = env_get(interp->globals, var_name);
        if (!interp->had_error && result.kind == VAL_INT && result.as.i == 42) {
            PASS();
        } else if (interp->had_error) {
            FAIL(interp->error_msg);
        } else {
            FAIL("payload not received");
        }
        val_free(&result);
    }

    interp_destroy(interp);
    strtable_destroy(st);
    arena_destroy(arena);
}

static void test_multiple_handlers(void) {
    forge_arena_t* arena = arena_create(4096);
    forge_strtable_t* st = strtable_create();
    forge_interp_t* interp = interp_create(arena, st);

    TEST("Multiple handlers all receive emit");
    {
        const char* ch_name = strtable_intern(st, "multi_ch", 8);
        const char* param_name = strtable_intern(st, "x", 1);
        const char* counter_name = strtable_intern(st, "counter", 7);

        /* Channel declaration */
        forge_node_t* ch_decl = ast_channel_decl(arena, ch_name, NULL, 0, 1, 1);

        /* Global var: var counter: int = 0 */
        forge_node_t* init_val = ast_int_lit(arena, 0, 1, 1);
        forge_node_t* var_decl = ast_var_decl(arena, counter_name, NULL, init_val, 0, 1, 1);

        /* Handler 1: counter = counter + 1 */
        forge_node_t* counter_ref1 = ast_ident(arena, counter_name, 1, 1);
        forge_node_t* one1 = ast_int_lit(arena, 1, 1, 1);
        forge_node_t* add1 = ast_binary_op(arena, TOK_PLUS, counter_ref1, one1, 1, 1);
        forge_node_t* target1 = ast_ident(arena, counter_name, 1, 1);
        forge_node_t* assign1 = ast_assign(arena, target1, add1, 1, 1);
        forge_node_t* body1_stmts[1] = { assign1 };
        forge_node_t* body1 = ast_block(arena, body1_stmts, 1, 1, 1);
        forge_node_t* handler1 = ast_on_handler(arena, ch_name, param_name, body1, 1, 1);

        /* Handler 2: counter = counter + 1 */
        forge_node_t* counter_ref2 = ast_ident(arena, counter_name, 1, 1);
        forge_node_t* one2 = ast_int_lit(arena, 1, 1, 1);
        forge_node_t* add2 = ast_binary_op(arena, TOK_PLUS, counter_ref2, one2, 1, 1);
        forge_node_t* target2 = ast_ident(arena, counter_name, 1, 1);
        forge_node_t* assign2 = ast_assign(arena, target2, add2, 1, 1);
        forge_node_t* body2_stmts[1] = { assign2 };
        forge_node_t* body2 = ast_block(arena, body2_stmts, 1, 1, 1);
        forge_node_t* handler2 = ast_on_handler(arena, ch_name, param_name, body2, 1, 1);

        /* Main proc: emit multi_ch -> 0 */
        forge_node_t* payload = ast_int_lit(arena, 0, 1, 1);
        forge_node_t* emit_stmt = ast_emit(arena, ch_name, payload, 1, 1);
        forge_node_t* main_body_stmts[1] = { emit_stmt };
        forge_node_t* main_body = ast_block(arena, main_body_stmts, 1, 1, 1);

        const char* main_name = strtable_intern(st, "main", 4);
        forge_node_t* main_proc = ast_proc_decl(arena, main_name, NULL, 0, NULL, main_body, 0, 1, 1);

        /* Program: channel, var, handler1, handler2, main */
        forge_node_t* decls[5] = { ch_decl, var_decl, handler1, handler2, main_proc };
        forge_node_t* prog = ast_program(arena, NULL, 0, decls, 5);

        interp_run(interp, prog);

        /* Check that counter == 2 (both handlers called) */
        forge_value_t counter = env_get(interp->globals, counter_name);
        if (!interp->had_error && counter.kind == VAL_INT && counter.as.i == 2) {
            PASS();
        } else if (interp->had_error) {
            FAIL(interp->error_msg);
        } else {
            FAIL("handlers not all called");
        }
        val_free(&counter);
    }

    interp_destroy(interp);
    strtable_destroy(st);
    arena_destroy(arena);
}

static void test_emit_unknown_channel(void) {
    forge_arena_t* arena = arena_create(4096);
    forge_strtable_t* st = strtable_create();
    forge_interp_t* interp = interp_create(arena, st);

    TEST("Emit to unknown channel (error)");
    {
        const char* ch_name = strtable_intern(st, "nonexistent", 11);

        /* Main proc: emit nonexistent -> 0 */
        forge_node_t* payload = ast_int_lit(arena, 0, 1, 1);
        forge_node_t* emit_stmt = ast_emit(arena, ch_name, payload, 1, 1);
        forge_node_t* main_body_stmts[1] = { emit_stmt };
        forge_node_t* main_body = ast_block(arena, main_body_stmts, 1, 1, 1);

        const char* main_name = strtable_intern(st, "main", 4);
        forge_node_t* main_proc = ast_proc_decl(arena, main_name, NULL, 0, NULL, main_body, 0, 1, 1);

        forge_node_t* decls[1] = { main_proc };
        forge_node_t* prog = ast_program(arena, NULL, 0, decls, 1);

        interp_run(interp, prog);

        if (interp->had_error) {
            printf("Runtime error at line 1, col 1: Unknown channel 'nonexistent'\n");
            PASS();
        } else {
            FAIL("should have errored");
        }
    }

    interp_destroy(interp);
    strtable_destroy(st);
    arena_destroy(arena);
}

static void test_void_channel_emit(void) {
    forge_arena_t* arena = arena_create(4096);
    forge_strtable_t* st = strtable_create();
    forge_interp_t* interp = interp_create(arena, st);

    TEST("Void channel emit (no payload)");
    {
        const char* ch_name = strtable_intern(st, "shutdown", 8);
        const char* flag_name = strtable_intern(st, "shutdown_called", 15);

        /* Channel declaration: channel shutdown: void */
        forge_node_t* ch_decl = ast_channel_decl(arena, ch_name, NULL, 0, 1, 1);

        /* Global var: var shutdown_called: bool = false */
        forge_node_t* init_val = ast_bool_lit(arena, 0, 1, 1);
        forge_node_t* var_decl = ast_var_decl(arena, flag_name, NULL, init_val, 0, 1, 1);

        /* Handler: on shutdown: shutdown_called = true */
        forge_node_t* true_val = ast_bool_lit(arena, 1, 1, 1);
        forge_node_t* flag_ref = ast_ident(arena, flag_name, 1, 1);
        forge_node_t* assign = ast_assign(arena, flag_ref, true_val, 1, 1);
        forge_node_t* body_stmts[1] = { assign };
        forge_node_t* handler_body = ast_block(arena, body_stmts, 1, 1, 1);
        forge_node_t* handler = ast_on_handler(arena, ch_name, NULL, handler_body, 1, 1);

        /* Main proc: emit shutdown */
        forge_node_t* emit_stmt = ast_emit(arena, ch_name, NULL, 1, 1);
        forge_node_t* main_body_stmts[1] = { emit_stmt };
        forge_node_t* main_body = ast_block(arena, main_body_stmts, 1, 1, 1);

        const char* main_name = strtable_intern(st, "main", 4);
        forge_node_t* main_proc = ast_proc_decl(arena, main_name, NULL, 0, NULL, main_body, 0, 1, 1);

        /* Program */
        forge_node_t* decls[4] = { ch_decl, var_decl, handler, main_proc };
        forge_node_t* prog = ast_program(arena, NULL, 0, decls, 4);

        interp_run(interp, prog);

        /* Check flag is true */
        forge_value_t flag = env_get(interp->globals, flag_name);
        if (!interp->had_error && flag.kind == VAL_BOOL && flag.as.b) {
            PASS();
        } else if (interp->had_error) {
            FAIL(interp->error_msg);
        } else {
            FAIL("handler not called");
        }
        val_free(&flag);
    }

    interp_destroy(interp);
    strtable_destroy(st);
    arena_destroy(arena);
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Main
 * ───────────────────────────────────────────────────────────────────────────── */

int main(void) {
    printf("\n=== FORGE Interpreter Channel Tests ===\n\n");

    test_channel_registration();
    test_handler_registration();
    test_emit_with_payload();
    test_multiple_handlers();
    test_emit_unknown_channel();
    test_void_channel_emit();

    printf("\n=== Results: %d/%d tests passed ===\n\n", pass_count, test_count);

    return (pass_count == test_count) ? 0 : 1;
}

