/*
 * FORGE Language Toolchain
 * emit_c.h - C code emitter interface
 *
 * The C emitter takes a type-checked AST and produces a compilable C source file.
 * The generated C, when compiled and linked with forge_runtime.c, produces a
 * binary that behaves identically to `forge run`.
 */

#ifndef FORGE_EMIT_C_H
#define FORGE_EMIT_C_H

#include "common.h"
#include "util/memory.h"
#include "util/strtable.h"
#include "parser/ast.h"
#include "typecheck/types.h"

/* ─────────────────────────────────────────────────────────────────────────────
 * Emitter Context
 * ───────────────────────────────────────────────────────────────────────────── */

typedef struct {
    FILE*              out;          /* Output file */
    int                indent;       /* Current indentation level */
    forge_arena_t*     arena;        /* Arena for temp allocations */
    forge_strtable_t*  strtable;     /* String interning table */
    int                tmp_counter;  /* For unique temp variable names */
    int                label_counter;/* For unique label names */
    const char*        module_name;  /* Current module being emitted */
    const char*        source_file;  /* Original source filename */
    int                in_loop;      /* Are we inside a loop? (for break/continue) */
} forge_emitter_t;

/* ─────────────────────────────────────────────────────────────────────────────
 * Emitter Lifecycle
 * ───────────────────────────────────────────────────────────────────────────── */

/* Create emitter context */
forge_emitter_t* emitter_create(FILE* out, forge_arena_t* arena,
                                 forge_strtable_t* strtable,
                                 const char* module_name,
                                 const char* source_file);

/* Emit a complete program (NODE_PROGRAM) */
void emitter_emit_program(forge_emitter_t* e, forge_node_t* program);

/* Destroy emitter context (does not close file) */
void emitter_destroy(forge_emitter_t* e);

/* ─────────────────────────────────────────────────────────────────────────────
 * Helper Functions (exposed for testing)
 * ───────────────────────────────────────────────────────────────────────────── */

/* Emit a single statement */
void emit_stmt(forge_emitter_t* e, forge_node_t* node);

/* Emit an expression */
void emit_expr(forge_emitter_t* e, forge_node_t* node);

/* Emit a type as C type string */
void emit_type(forge_emitter_t* e, forge_type_t* type);

/* ─────────────────────────────────────────────────────────────────────────────
 * Helper Macros
 * ───────────────────────────────────────────────────────────────────────────── */

#define EMIT(e, ...)        fprintf((e)->out, __VA_ARGS__)
#define EMITLN(e, ...)      do { fprintf((e)->out, __VA_ARGS__); fputc('\n', (e)->out); } while(0)
#define NEWLINE(e)          fputc('\n', (e)->out)
#define INDENT(e)           emit_indent(e)
#define PUSH_INDENT(e)      ((e)->indent += 4)
#define POP_INDENT(e)       ((e)->indent -= 4)

/* Internal helper */
void emit_indent(forge_emitter_t* e);

#endif /* FORGE_EMIT_C_H */

