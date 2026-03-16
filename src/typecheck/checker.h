/*
 * FORGE Language Toolchain
 * checker.h - Static type checker
 *
 * The type checker performs a two-pass analysis:
 *   Pass 1: Declaration collection - gathers all top-level symbols
 *   Pass 2: Body checking - type-checks procedure bodies and initializers
 *
 * After checking, all expression nodes have their resolved_type field set.
 */

#ifndef FORGE_CHECKER_H
#define FORGE_CHECKER_H

#include "common.h"
#include "util/memory.h"
#include "util/strtable.h"
#include "util/hashmap.h"
#include "typecheck/types.h"
#include "parser/ast.h"

/* ─────────────────────────────────────────────────────────────────────────────
 * Checker State
 * ───────────────────────────────────────────────────────────────────────────── */

/* Procedure signature for type checking */
typedef struct {
    const char*     name;
    forge_type_t**  param_types;    /* Array of parameter types */
    const char**    param_names;    /* Array of parameter names */
    int             param_count;
    forge_type_t*   return_type;    /* NULL for void procedures */
    forge_node_t*   decl_node;      /* For error reporting */
} forge_proc_sig_t;

/* Channel declaration for type checking */
typedef struct {
    const char*     name;
    forge_type_t*   payload_type;   /* Type of emitted values */
    forge_node_t*   decl_node;
} forge_channel_decl_t;

/* Note: forge_checker_t is forward-declared in common.h */
struct forge_checker {
    /* Symbol tables: name -> typed entry */
    forge_hashmap_t*    types;      /* record and alias declarations -> forge_type_t* */
    forge_hashmap_t*    procs;      /* procedure signatures -> forge_proc_sig_t* */
    forge_hashmap_t*    channels;   /* channel declarations -> forge_channel_decl_t* */
    forge_hashmap_t*    consts;     /* constant types -> forge_type_t* */

    /* Module imports: alias -> module checker */
    forge_hashmap_t*    imports;

    /* Current context during Pass 2 */
    forge_type_t*       current_return_type;   /* For checking return statements */
    forge_hashmap_t*    local_vars;            /* Current scope: name -> forge_type_t* */

    /* Memory management */
    forge_arena_t*      arena;      /* Type allocation arena */
    forge_strtable_t*   strtable;   /* String interning */

    /* Error reporting */
    const char*         filename;
    int                 had_error;
    int                 error_count;
    int                 warning_count;
};

/* ─────────────────────────────────────────────────────────────────────────────
 * Checker Lifecycle
 * ───────────────────────────────────────────────────────────────────────────── */

/*
 * Create a new type checker.
 * The arena and strtable are borrowed (not owned by checker).
 */
forge_checker_t* checker_create(forge_arena_t* arena, forge_strtable_t* strtable,
                                 const char* filename);

/*
 * Destroy the type checker and free owned resources.
 * Does not free the arena or strtable.
 */
void checker_destroy(forge_checker_t* checker);

/* ─────────────────────────────────────────────────────────────────────────────
 * Type Checking
 * ───────────────────────────────────────────────────────────────────────────── */

/*
 * Type check a program (MODULE node).
 * Returns 0 on success, non-zero if errors were found.
 * After checking, all expression nodes have resolved_type set.
 */
int checker_check(forge_checker_t* checker, forge_node_t* program);

/*
 * Get the type of an expression node.
 * Returns NULL if the node cannot be typed (error already reported).
 * Sets node->resolved_type as a side effect.
 */
forge_type_t* checker_type_of(forge_checker_t* checker, forge_node_t* node);

/* ─────────────────────────────────────────────────────────────────────────────
 * Symbol Table Access
 * ───────────────────────────────────────────────────────────────────────────── */

/* Look up a type by name (records and aliases) */
forge_type_t* checker_lookup_type(forge_checker_t* checker, const char* name);

/* Look up a procedure signature by name */
forge_proc_sig_t* checker_lookup_proc(forge_checker_t* checker, const char* name);

/* Look up a channel declaration by name */
forge_channel_decl_t* checker_lookup_channel(forge_checker_t* checker, const char* name);

/* Look up a local variable type */
forge_type_t* checker_lookup_var(forge_checker_t* checker, const char* name);

/* ─────────────────────────────────────────────────────────────────────────────
 * Error Reporting
 * ───────────────────────────────────────────────────────────────────────────── */

/* Report a type error at the given node */
void checker_error(forge_checker_t* checker, forge_node_t* node,
                   const char* fmt, ...);

/* Report a warning at the given node */
void checker_warning(forge_checker_t* checker, forge_node_t* node,
                     const char* fmt, ...);

#endif /* FORGE_CHECKER_H */

