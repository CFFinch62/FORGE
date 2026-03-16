/*
 * FORGE Language Toolchain
 * emit_llvm.h - LLVM IR emitter interface
 *
 * The LLVM emitter takes a type-checked AST and produces LLVM IR text format (.ll).
 * The generated IR can be processed by LLVM tools:
 *   - opt: LLVM optimizer
 *   - llc: LLVM static compiler (to native code)
 *   - lli: LLVM interpreter (for testing)
 *
 * LLVM IR is in SSA (Static Single Assignment) form, which requires:
 *   - Each variable is assigned exactly once
 *   - Use of phi nodes at control flow merge points
 *   - Explicit load/store for mutable variables (using alloca)
 */

#ifndef FORGE_EMIT_LLVM_H
#define FORGE_EMIT_LLVM_H

#include "common.h"
#include "util/memory.h"
#include "util/strtable.h"
#include "util/hashmap.h"
#include "parser/ast.h"
#include "typecheck/types.h"

/* ─────────────────────────────────────────────────────────────────────────────
 * LLVM Emitter Context
 * ───────────────────────────────────────────────────────────────────────────── */

/* Variable scope entry - stack-like scoping for shadowed variables */
typedef struct llvm_var_scope {
    forge_hashmap_t*        vars;       /* Maps var name -> unique LLVM name */
    struct llvm_var_scope*  parent;     /* Enclosing scope */
} llvm_var_scope_t;

typedef struct {
    FILE*              out;            /* Output file */
    forge_arena_t*     arena;          /* Arena for temp allocations */
    forge_strtable_t*  strtable;       /* String interning table */

    /* SSA state */
    int                reg_counter;    /* Next SSA register number (%0, %1, ...) */
    int                label_counter;  /* Next label number (label0, label1, ...) */
    int                str_counter;    /* String constant counter (@.str.0, ...) */
    int                var_counter;    /* Unique variable name counter */

    /* Variable scopes - stack of hashmaps for shadowed variable support */
    llvm_var_scope_t*  var_scope;      /* Current variable scope */

    /* String constants collected during emission */
    struct {
        const char** strings;
        int count;
        int capacity;
    } string_constants;

    /* Module metadata */
    const char*        module_name;
    const char*        source_file;

    /* Current context */
    int                in_loop;        /* Inside a loop? */
    const char*        loop_exit_label;/* Label for break */
    const char*        loop_cont_label;/* Label for continue */
    int                block_terminated; /* Has current basic block been terminated? */
    forge_type_t*      current_ret_type; /* Return type of current procedure */
    int                current_block_id; /* Current basic block label id (-1 for entry) */
} forge_llvm_emitter_t;

/* ─────────────────────────────────────────────────────────────────────────────
 * Emitter Lifecycle
 * ───────────────────────────────────────────────────────────────────────────── */

/* Create LLVM emitter context */
forge_llvm_emitter_t* llvm_emitter_create(FILE* out, forge_arena_t* arena,
                                           forge_strtable_t* strtable,
                                           const char* module_name,
                                           const char* source_file);

/* Emit a complete program as LLVM IR */
void llvm_emitter_emit_program(forge_llvm_emitter_t* e, forge_node_t* program);

/* Destroy emitter context (does not close file) */
void llvm_emitter_destroy(forge_llvm_emitter_t* e);

/* ─────────────────────────────────────────────────────────────────────────────
 * Helper Functions (exposed for testing)
 * ───────────────────────────────────────────────────────────────────────────── */

/* Emit a single statement, returns last SSA register used (or -1 if none) */
int llvm_emit_stmt(forge_llvm_emitter_t* e, forge_node_t* node);

/* Emit an expression, returns the SSA register containing the result */
int llvm_emit_expr(forge_llvm_emitter_t* e, forge_node_t* node);

/* Emit a type as LLVM type string */
void llvm_emit_type(forge_llvm_emitter_t* e, forge_type_t* type);

/* Get LLVM type string for a FORGE type (returns static or arena-allocated string) */
const char* llvm_type_str(forge_llvm_emitter_t* e, forge_type_t* type);

/* ─────────────────────────────────────────────────────────────────────────────
 * SSA Helpers
 * ───────────────────────────────────────────────────────────────────────────── */

/* Get next SSA register number and increment counter */
int llvm_next_reg(forge_llvm_emitter_t* e);

/* Get next label and increment counter */
int llvm_next_label(forge_llvm_emitter_t* e);

/* ─────────────────────────────────────────────────────────────────────────────
 * Variable Scope Helpers
 * ───────────────────────────────────────────────────────────────────────────── */

/* Push a new variable scope (e.g., entering a block) */
void llvm_push_scope(forge_llvm_emitter_t* e);

/* Pop the current variable scope (e.g., leaving a block) */
void llvm_pop_scope(forge_llvm_emitter_t* e);

/* Declare a variable in the current scope, returns unique LLVM name */
const char* llvm_declare_var(forge_llvm_emitter_t* e, const char* name);

/* Look up a variable, returns unique LLVM name or NULL if not found */
const char* llvm_lookup_var(forge_llvm_emitter_t* e, const char* name);

/* ─────────────────────────────────────────────────────────────────────────────
 * Helper Macros
 * ───────────────────────────────────────────────────────────────────────────── */

#define LLVM_EMIT(e, ...)      fprintf((e)->out, __VA_ARGS__)
#define LLVM_EMITLN(e, ...)    do { fprintf((e)->out, __VA_ARGS__); fputc('\n', (e)->out); } while(0)
#define LLVM_NEWLINE(e)        fputc('\n', (e)->out)

#endif /* FORGE_EMIT_LLVM_H */

