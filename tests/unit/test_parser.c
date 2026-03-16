/*
 * test_parser.c - Unit tests for the FORGE parser
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "parser/ast.h"
#include "util/strtable.h"
#include "util/memory.h"

static int tests_run = 0;
static int tests_passed = 0;

#define RUN_TEST(name, test_fn) do { \
    tests_run++; \
    printf("  [%d] %-50s ", tests_run, name); \
    if (test_fn()) { \
        tests_passed++; \
        printf("✓\n"); \
    } else { \
        printf("✗\n"); \
    } \
} while(0)

/* Helper to parse a source string */
static forge_node_t* parse_source(const char* source, forge_arena_t* arena,
                                   forge_strtable_t* strtable, int* had_error) {
    forge_lexer_t* lexer = lexer_create(source, strlen(source), "test.fg", strtable);
    lexer_tokenize(lexer);

    /* Get tokens from lexer */
    int count = lexer->tokens.len;
    forge_token_t* tokens = malloc(count * sizeof(forge_token_t));
    for (int i = 0; i < count; i++) {
        tokens[i] = forge_token_array_get(&lexer->tokens, i);
    }
    lexer_destroy(lexer);

    forge_parser_t* parser = parser_create(tokens, count, arena, strtable, "test.fg");
    forge_node_t* result = parser_parse(parser);
    *had_error = parser_had_error(parser);
    parser_destroy(parser);
    free(tokens);
    return result;
}

/* Test simple void procedure */
static int test_simple_void_proc(void) {
    forge_arena_t* arena = arena_create(4096);
    forge_strtable_t* strtable = strtable_create();

    const char* source = "proc main() -> void:\n    return\n";
    int had_error = 0;
    forge_node_t* program = parse_source(source, arena, strtable, &had_error);

    int success = !had_error && program != NULL &&
                  program->kind == NODE_PROGRAM &&
                  program->data.program.decl_count == 1 &&
                  program->data.program.decls[0]->kind == NODE_PROC_DECL &&
                  strcmp(program->data.program.decls[0]->data.proc.name, "main") == 0;

    arena_destroy(arena);
    strtable_destroy(strtable);
    return success;
}

/* Test procedure with parameters */
static int test_proc_with_params(void) {
    forge_arena_t* arena = arena_create(4096);
    forge_strtable_t* strtable = strtable_create();

    const char* source = "proc add(a: int, b: int) -> int:\n    return a\n";
    int had_error = 0;
    forge_node_t* program = parse_source(source, arena, strtable, &had_error);

    int success = !had_error && program != NULL &&
                  program->data.program.decl_count == 1;

    if (success) {
        forge_node_t* proc = program->data.program.decls[0];
        success = proc->kind == NODE_PROC_DECL &&
                  strcmp(proc->data.proc.name, "add") == 0 &&
                  proc->data.proc.param_count == 2;
    }

    arena_destroy(arena);
    strtable_destroy(strtable);
    return success;
}

/* Test procedure with ref parameter - ref is part of the type, not a separate modifier */
static int test_proc_with_ref_param(void) {
    forge_arena_t* arena = arena_create(4096);
    forge_strtable_t* strtable = strtable_create();

    /* Using grammar-based syntax: ref before identifier */
    const char* source = "proc swap(ref a: int, ref b: int) -> void:\n    return\n";
    int had_error = 0;
    forge_node_t* program = parse_source(source, arena, strtable, &had_error);

    int success = !had_error && program != NULL &&
                  program->data.program.decl_count == 1;

    if (success) {
        forge_node_t* proc = program->data.program.decls[0];
        success = proc->kind == NODE_PROC_DECL &&
                  proc->data.proc.param_count == 2 &&
                  proc->data.proc.params[0].is_ref == 1 &&
                  proc->data.proc.params[1].is_ref == 1;
    }

    arena_destroy(arena);
    strtable_destroy(strtable);
    return success;
}

/* Test exported procedure */
static int test_exported_proc(void) {
    forge_arena_t* arena = arena_create(4096);
    forge_strtable_t* strtable = strtable_create();

    const char* source = "export proc read() -> float:\n    return 1.0\n";
    int had_error = 0;
    forge_node_t* program = parse_source(source, arena, strtable, &had_error);

    int success = !had_error && program != NULL &&
                  program->data.program.decl_count == 1;

    if (success) {
        forge_node_t* proc = program->data.program.decls[0];
        success = proc->kind == NODE_PROC_DECL &&
                  proc->data.proc.exported == 1;
    }

    arena_destroy(arena);
    strtable_destroy(strtable);
    return success;
}

/* Test simple record declaration */
static int test_simple_record(void) {
    forge_arena_t* arena = arena_create(4096);
    forge_strtable_t* strtable = strtable_create();

    const char* source =
        "record Sensor:\n"
        "    id: int\n"
        "    value: float\n";
    int had_error = 0;
    forge_node_t* program = parse_source(source, arena, strtable, &had_error);

    int success = !had_error && program != NULL &&
                  program->data.program.decl_count == 1;

    if (success) {
        forge_node_t* rec = program->data.program.decls[0];
        success = rec->kind == NODE_RECORD_DECL &&
                  strcmp(rec->data.record.name, "Sensor") == 0 &&
                  rec->data.record.field_count == 2;
    }

    arena_destroy(arena);
    strtable_destroy(strtable);
    return success;
}

/* Test record with various field types */
static int test_record_with_complex_types(void) {
    forge_arena_t* arena = arena_create(4096);
    forge_strtable_t* strtable = strtable_create();

    const char* source =
        "record Config:\n"
        "    name: str\n"
        "    values: []float\n"
        "    enabled: ?bool\n";
    int had_error = 0;
    forge_node_t* program = parse_source(source, arena, strtable, &had_error);

    int success = !had_error && program != NULL &&
                  program->data.program.decl_count == 1;

    if (success) {
        forge_node_t* rec = program->data.program.decls[0];
        success = rec->kind == NODE_RECORD_DECL &&
                  rec->data.record.field_count == 3;

        /* Check first field type */
        if (success && rec->data.record.fields[0]) {
            forge_node_t* field = rec->data.record.fields[0];
            success = field->kind == NODE_FIELD_DEF &&
                      strcmp(field->data.field_def.name, "name") == 0;
        }
    }

    arena_destroy(arena);
    strtable_destroy(strtable);
    return success;
}

/* Test exported record */
static int test_exported_record(void) {
    forge_arena_t* arena = arena_create(4096);
    forge_strtable_t* strtable = strtable_create();

    const char* source =
        "export record Point:\n"
        "    x: float\n"
        "    y: float\n";
    int had_error = 0;
    forge_node_t* program = parse_source(source, arena, strtable, &had_error);

    int success = !had_error && program != NULL &&
                  program->data.program.decl_count == 1;

    if (success) {
        forge_node_t* rec = program->data.program.decls[0];
        success = rec->kind == NODE_RECORD_DECL &&
                  rec->data.record.exported == 1;
    }

    arena_destroy(arena);
    strtable_destroy(strtable);
    return success;
}

/* Test simple channel declaration */
static int test_simple_channel(void) {
    forge_arena_t* arena = arena_create(4096);
    forge_strtable_t* strtable = strtable_create();

    const char* source = "channel depth_reading: float\n";
    int had_error = 0;
    forge_node_t* program = parse_source(source, arena, strtable, &had_error);

    int success = !had_error && program != NULL &&
                  program->data.program.decl_count == 1;

    if (success) {
        forge_node_t* ch = program->data.program.decls[0];
        success = ch->kind == NODE_CHANNEL_DECL &&
                  strcmp(ch->data.channel.name, "depth_reading") == 0 &&
                  ch->data.channel.payload_type != NULL;
    }

    arena_destroy(arena);
    strtable_destroy(strtable);
    return success;
}

/* Test void channel (signal only, no payload) */
static int test_void_channel(void) {
    forge_arena_t* arena = arena_create(4096);
    forge_strtable_t* strtable = strtable_create();

    const char* source = "channel shutdown: void\n";
    int had_error = 0;
    forge_node_t* program = parse_source(source, arena, strtable, &had_error);

    int success = !had_error && program != NULL &&
                  program->data.program.decl_count == 1;

    if (success) {
        forge_node_t* ch = program->data.program.decls[0];
        success = ch->kind == NODE_CHANNEL_DECL &&
                  strcmp(ch->data.channel.name, "shutdown") == 0;
    }

    arena_destroy(arena);
    strtable_destroy(strtable);
    return success;
}

/* Test exported channel */
static int test_exported_channel(void) {
    forge_arena_t* arena = arena_create(4096);
    forge_strtable_t* strtable = strtable_create();

    const char* source = "export channel nmea_sentence: str\n";
    int had_error = 0;
    forge_node_t* program = parse_source(source, arena, strtable, &had_error);

    int success = !had_error && program != NULL &&
                  program->data.program.decl_count == 1;

    if (success) {
        forge_node_t* ch = program->data.program.decls[0];
        success = ch->kind == NODE_CHANNEL_DECL &&
                  ch->data.channel.exported == 1;
    }

    arena_destroy(arena);
    strtable_destroy(strtable);
    return success;
}

/* ─── Statement Tests ─── */

/* Test if/elif/else statement */
static int test_if_elif_else(void) {
    forge_arena_t* arena = arena_create(4096);
    forge_strtable_t* strtable = strtable_create();

    const char* source =
        "proc check(x: int) -> void:\n"
        "    if x:\n"
        "        return\n"
        "    elif y:\n"
        "        return\n"
        "    else:\n"
        "        return\n";
    int had_error = 0;
    forge_node_t* program = parse_source(source, arena, strtable, &had_error);

    int success = !had_error && program != NULL &&
                  program->data.program.decl_count == 1;

    if (success) {
        forge_node_t* proc = program->data.program.decls[0];
        forge_node_t* body = proc->data.proc.body;
        success = body->kind == NODE_BLOCK &&
                  body->data.block.count == 1 &&
                  body->data.block.stmts[0]->kind == NODE_IF;

        if (success) {
            forge_node_t* if_stmt = body->data.block.stmts[0];
            success = if_stmt->data.if_stmt.elif_count == 1 &&
                      if_stmt->data.if_stmt.else_body != NULL;
        }
    }

    arena_destroy(arena);
    strtable_destroy(strtable);
    return success;
}

/* Test while loop */
static int test_while_loop(void) {
    forge_arena_t* arena = arena_create(4096);
    forge_strtable_t* strtable = strtable_create();

    const char* source =
        "proc loop_test() -> void:\n"
        "    while x:\n"
        "        return\n";
    int had_error = 0;
    forge_node_t* program = parse_source(source, arena, strtable, &had_error);

    int success = !had_error && program != NULL;

    if (success) {
        forge_node_t* proc = program->data.program.decls[0];
        forge_node_t* body = proc->data.proc.body;
        success = body->data.block.stmts[0]->kind == NODE_WHILE &&
                  body->data.block.stmts[0]->data.while_stmt.condition != NULL &&
                  body->data.block.stmts[0]->data.while_stmt.body != NULL;
    }

    arena_destroy(arena);
    strtable_destroy(strtable);
    return success;
}

/* Test for loop */
static int test_for_loop(void) {
    forge_arena_t* arena = arena_create(4096);
    forge_strtable_t* strtable = strtable_create();

    const char* source =
        "proc iterate() -> void:\n"
        "    for i in items:\n"
        "        return\n";
    int had_error = 0;
    forge_node_t* program = parse_source(source, arena, strtable, &had_error);

    int success = !had_error && program != NULL;

    if (success) {
        forge_node_t* proc = program->data.program.decls[0];
        forge_node_t* body = proc->data.proc.body;
        forge_node_t* for_stmt = body->data.block.stmts[0];
        success = for_stmt->kind == NODE_FOR &&
                  strcmp(for_stmt->data.for_stmt.var_name, "i") == 0 &&
                  for_stmt->data.for_stmt.iterable != NULL &&
                  for_stmt->data.for_stmt.body != NULL;
    }

    arena_destroy(arena);
    strtable_destroy(strtable);
    return success;
}

/* Test loop (infinite) */
static int test_infinite_loop(void) {
    forge_arena_t* arena = arena_create(4096);
    forge_strtable_t* strtable = strtable_create();

    const char* source =
        "proc forever() -> void:\n"
        "    loop:\n"
        "        break\n";
    int had_error = 0;
    forge_node_t* program = parse_source(source, arena, strtable, &had_error);

    int success = !had_error && program != NULL;

    if (success) {
        forge_node_t* proc = program->data.program.decls[0];
        forge_node_t* body = proc->data.proc.body;
        forge_node_t* loop = body->data.block.stmts[0];
        success = loop->kind == NODE_LOOP &&
                  loop->data.loop_stmt.body != NULL &&
                  loop->data.loop_stmt.body->data.block.stmts[0]->kind == NODE_BREAK;
    }

    arena_destroy(arena);
    strtable_destroy(strtable);
    return success;
}

/* Test break and continue */
static int test_break_continue(void) {
    forge_arena_t* arena = arena_create(4096);
    forge_strtable_t* strtable = strtable_create();

    const char* source =
        "proc control() -> void:\n"
        "    loop:\n"
        "        continue\n"
        "        break\n";
    int had_error = 0;
    forge_node_t* program = parse_source(source, arena, strtable, &had_error);

    int success = !had_error && program != NULL;

    if (success) {
        forge_node_t* proc = program->data.program.decls[0];
        forge_node_t* loop = proc->data.proc.body->data.block.stmts[0];
        forge_node_t* loop_body = loop->data.loop_stmt.body;
        success = loop_body->data.block.count == 2 &&
                  loop_body->data.block.stmts[0]->kind == NODE_CONTINUE &&
                  loop_body->data.block.stmts[1]->kind == NODE_BREAK;
    }

    arena_destroy(arena);
    strtable_destroy(strtable);
    return success;
}

/* Test emit statement */
static int test_emit_stmt(void) {
    forge_arena_t* arena = arena_create(4096);
    forge_strtable_t* strtable = strtable_create();

    const char* source =
        "proc send_data() -> void:\n"
        "    emit data_ready -> 42\n";
    int had_error = 0;
    forge_node_t* program = parse_source(source, arena, strtable, &had_error);

    int success = !had_error && program != NULL;

    if (success) {
        forge_node_t* proc = program->data.program.decls[0];
        forge_node_t* emit = proc->data.proc.body->data.block.stmts[0];
        success = emit->kind == NODE_EMIT &&
                  strcmp(emit->data.emit_stmt.channel_name, "data_ready") == 0 &&
                  emit->data.emit_stmt.payload != NULL;
    }

    arena_destroy(arena);
    strtable_destroy(strtable);
    return success;
}

/* Test assignment statement */
static int test_assignment(void) {
    forge_arena_t* arena = arena_create(4096);
    forge_strtable_t* strtable = strtable_create();

    const char* source =
        "proc assign_test() -> void:\n"
        "    x = 42\n";
    int had_error = 0;
    forge_node_t* program = parse_source(source, arena, strtable, &had_error);

    int success = !had_error && program != NULL;

    if (success) {
        forge_node_t* proc = program->data.program.decls[0];
        forge_node_t* assign = proc->data.proc.body->data.block.stmts[0];
        success = assign->kind == NODE_ASSIGN &&
                  assign->data.assign.target->kind == NODE_IDENT &&
                  assign->data.assign.value->kind == NODE_INT_LIT;
    }

    arena_destroy(arena);
    strtable_destroy(strtable);
    return success;
}

/* ─── Expression Tests ─── */

/* Test arithmetic expression with precedence */
static int test_arithmetic_precedence(void) {
    forge_arena_t* arena = arena_create(4096);
    forge_strtable_t* strtable = strtable_create();

    /* 1 + 2 * 3 should parse as 1 + (2 * 3) */
    const char* source =
        "proc calc() -> void:\n"
        "    x = 1 + 2 * 3\n";
    int had_error = 0;
    forge_node_t* program = parse_source(source, arena, strtable, &had_error);

    int success = !had_error && program != NULL;

    if (success) {
        forge_node_t* proc = program->data.program.decls[0];
        forge_node_t* assign = proc->data.proc.body->data.block.stmts[0];
        forge_node_t* expr = assign->data.assign.value;
        /* Should be: BINARY_OP(+, 1, BINARY_OP(*, 2, 3)) */
        success = expr->kind == NODE_BINARY_OP &&
                  expr->data.binop.op == TOK_PLUS &&
                  expr->data.binop.left->kind == NODE_INT_LIT &&
                  expr->data.binop.right->kind == NODE_BINARY_OP &&
                  expr->data.binop.right->data.binop.op == TOK_STAR;
    }

    arena_destroy(arena);
    strtable_destroy(strtable);
    return success;
}

/* Test comparison expression */
static int test_comparison_expr(void) {
    forge_arena_t* arena = arena_create(4096);
    forge_strtable_t* strtable = strtable_create();

    const char* source =
        "proc test() -> void:\n"
        "    x = a < b\n";
    int had_error = 0;
    forge_node_t* program = parse_source(source, arena, strtable, &had_error);

    int success = !had_error && program != NULL;

    if (success) {
        forge_node_t* proc = program->data.program.decls[0];
        forge_node_t* assign = proc->data.proc.body->data.block.stmts[0];
        forge_node_t* expr = assign->data.assign.value;
        success = expr->kind == NODE_BINARY_OP &&
                  expr->data.binop.op == TOK_LT;
    }

    arena_destroy(arena);
    strtable_destroy(strtable);
    return success;
}

/* Test logical expression */
static int test_logical_expr(void) {
    forge_arena_t* arena = arena_create(4096);
    forge_strtable_t* strtable = strtable_create();

    const char* source =
        "proc test() -> void:\n"
        "    x = a and b or c\n";
    int had_error = 0;
    forge_node_t* program = parse_source(source, arena, strtable, &had_error);

    int success = !had_error && program != NULL;

    if (success) {
        forge_node_t* proc = program->data.program.decls[0];
        forge_node_t* assign = proc->data.proc.body->data.block.stmts[0];
        forge_node_t* expr = assign->data.assign.value;
        /* a and b or c parses as (a and b) or c */
        success = expr->kind == NODE_BINARY_OP &&
                  expr->data.binop.op == TOK_OR &&
                  expr->data.binop.left->kind == NODE_BINARY_OP &&
                  expr->data.binop.left->data.binop.op == TOK_AND;
    }

    arena_destroy(arena);
    strtable_destroy(strtable);
    return success;
}

/* Test unary expression */
static int test_unary_expr(void) {
    forge_arena_t* arena = arena_create(4096);
    forge_strtable_t* strtable = strtable_create();

    const char* source =
        "proc test() -> void:\n"
        "    x = -42\n";
    int had_error = 0;
    forge_node_t* program = parse_source(source, arena, strtable, &had_error);

    int success = !had_error && program != NULL;

    if (success) {
        forge_node_t* proc = program->data.program.decls[0];
        forge_node_t* assign = proc->data.proc.body->data.block.stmts[0];
        forge_node_t* expr = assign->data.assign.value;
        success = expr->kind == NODE_UNARY_OP &&
                  expr->data.unop.op == TOK_MINUS &&
                  expr->data.unop.operand->kind == NODE_INT_LIT;
    }

    arena_destroy(arena);
    strtable_destroy(strtable);
    return success;
}

/* Test function call */
static int test_function_call(void) {
    forge_arena_t* arena = arena_create(4096);
    forge_strtable_t* strtable = strtable_create();

    const char* source =
        "proc test() -> void:\n"
        "    x = foo(1, 2, 3)\n";
    int had_error = 0;
    forge_node_t* program = parse_source(source, arena, strtable, &had_error);

    int success = !had_error && program != NULL;

    if (success) {
        forge_node_t* proc = program->data.program.decls[0];
        forge_node_t* assign = proc->data.proc.body->data.block.stmts[0];
        forge_node_t* call = assign->data.assign.value;
        success = call->kind == NODE_CALL &&
                  call->data.call.arg_count == 3 &&
                  call->data.call.callee->kind == NODE_IDENT;
    }

    arena_destroy(arena);
    strtable_destroy(strtable);
    return success;
}

/* Test array indexing */
static int test_array_index(void) {
    forge_arena_t* arena = arena_create(4096);
    forge_strtable_t* strtable = strtable_create();

    const char* source =
        "proc test() -> void:\n"
        "    x = arr[0]\n";
    int had_error = 0;
    forge_node_t* program = parse_source(source, arena, strtable, &had_error);

    int success = !had_error && program != NULL;

    if (success) {
        forge_node_t* proc = program->data.program.decls[0];
        forge_node_t* assign = proc->data.proc.body->data.block.stmts[0];
        forge_node_t* idx = assign->data.assign.value;
        success = idx->kind == NODE_INDEX &&
                  idx->data.index.object->kind == NODE_IDENT &&
                  idx->data.index.index->kind == NODE_INT_LIT;
    }

    arena_destroy(arena);
    strtable_destroy(strtable);
    return success;
}

/* Test field access */
static int test_field_access(void) {
    forge_arena_t* arena = arena_create(4096);
    forge_strtable_t* strtable = strtable_create();

    const char* source =
        "proc test() -> void:\n"
        "    x = obj.field\n";
    int had_error = 0;
    forge_node_t* program = parse_source(source, arena, strtable, &had_error);

    int success = !had_error && program != NULL;

    if (success) {
        forge_node_t* proc = program->data.program.decls[0];
        forge_node_t* assign = proc->data.proc.body->data.block.stmts[0];
        forge_node_t* access = assign->data.assign.value;
        success = access->kind == NODE_FIELD_ACCESS &&
                  access->data.field_access.object->kind == NODE_IDENT &&
                  strcmp(access->data.field_access.field_name, "field") == 0;
    }

    arena_destroy(arena);
    strtable_destroy(strtable);
    return success;
}

/* Test array literal */
static int test_array_literal(void) {
    forge_arena_t* arena = arena_create(4096);
    forge_strtable_t* strtable = strtable_create();

    const char* source =
        "proc test() -> void:\n"
        "    x = [1, 2, 3]\n";
    int had_error = 0;
    forge_node_t* program = parse_source(source, arena, strtable, &had_error);

    int success = !had_error && program != NULL;

    if (success) {
        forge_node_t* proc = program->data.program.decls[0];
        forge_node_t* assign = proc->data.proc.body->data.block.stmts[0];
        forge_node_t* arr = assign->data.assign.value;
        success = arr->kind == NODE_ARRAY_LITERAL &&
                  arr->data.array_lit.count == 3;
    }

    arena_destroy(arena);
    strtable_destroy(strtable);
    return success;
}

/* Test parenthesized expression */
static int test_parenthesized_expr(void) {
    forge_arena_t* arena = arena_create(4096);
    forge_strtable_t* strtable = strtable_create();

    /* (1 + 2) * 3 should parse as BINARY_OP(*, BINARY_OP(+, 1, 2), 3) */
    const char* source =
        "proc test() -> void:\n"
        "    x = (1 + 2) * 3\n";
    int had_error = 0;
    forge_node_t* program = parse_source(source, arena, strtable, &had_error);

    int success = !had_error && program != NULL;

    if (success) {
        forge_node_t* proc = program->data.program.decls[0];
        forge_node_t* assign = proc->data.proc.body->data.block.stmts[0];
        forge_node_t* expr = assign->data.assign.value;
        success = expr->kind == NODE_BINARY_OP &&
                  expr->data.binop.op == TOK_STAR &&
                  expr->data.binop.left->kind == NODE_BINARY_OP &&
                  expr->data.binop.left->data.binop.op == TOK_PLUS;
    }

    arena_destroy(arena);
    strtable_destroy(strtable);
    return success;
}

int main(void) {
    printf("\n=== FORGE Parser Tests ===\n\n");

    /* Procedure tests */
    RUN_TEST("Parse simple void procedure", test_simple_void_proc);
    RUN_TEST("Parse procedure with parameters", test_proc_with_params);
    RUN_TEST("Parse procedure with ref parameters", test_proc_with_ref_param);
    RUN_TEST("Parse exported procedure", test_exported_proc);

    /* Record tests */
    RUN_TEST("Parse simple record", test_simple_record);
    RUN_TEST("Parse record with complex types", test_record_with_complex_types);
    RUN_TEST("Parse exported record", test_exported_record);

    /* Channel tests */
    RUN_TEST("Parse simple channel", test_simple_channel);
    RUN_TEST("Parse void channel", test_void_channel);
    RUN_TEST("Parse exported channel", test_exported_channel);

    /* Statement tests */
    RUN_TEST("Parse if/elif/else statement", test_if_elif_else);
    RUN_TEST("Parse while loop", test_while_loop);
    RUN_TEST("Parse for loop", test_for_loop);
    RUN_TEST("Parse infinite loop", test_infinite_loop);
    RUN_TEST("Parse break and continue", test_break_continue);
    RUN_TEST("Parse emit statement", test_emit_stmt);
    RUN_TEST("Parse assignment statement", test_assignment);

    /* Expression tests */
    RUN_TEST("Parse arithmetic precedence", test_arithmetic_precedence);
    RUN_TEST("Parse comparison expression", test_comparison_expr);
    RUN_TEST("Parse logical expression", test_logical_expr);
    RUN_TEST("Parse unary expression", test_unary_expr);
    RUN_TEST("Parse function call", test_function_call);
    RUN_TEST("Parse array indexing", test_array_index);
    RUN_TEST("Parse field access", test_field_access);
    RUN_TEST("Parse array literal", test_array_literal);
    RUN_TEST("Parse parenthesized expression", test_parenthesized_expr);

    printf("\n=== Results: %d/%d tests passed ===\n\n", tests_passed, tests_run);

    return tests_passed == tests_run ? 0 : 1;
}

