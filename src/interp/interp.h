/*
 * FORGE Language Toolchain
 * interp.h - Tree-walking interpreter
 *
 * The interpreter evaluates AST nodes directly, walking the tree
 * and computing results. It manages environments (scopes) and
 * control flow (return, break, continue).
 */

#ifndef FORGE_INTERP_H
#define FORGE_INTERP_H

#include "common.h"
#include "parser/ast.h"
#include "interp/value.h"
#include "interp/env.h"
#include "util/hashmap.h"

/* ─────────────────────────────────────────────────────────────────────────────
 * Control Flow
 * ───────────────────────────────────────────────────────────────────────────── */

typedef enum {
    FLOW_NORMAL,        /* Normal execution */
    FLOW_RETURN,        /* Returning from procedure */
    FLOW_BREAK,         /* Breaking from loop */
    FLOW_CONTINUE,      /* Continuing loop */
} forge_flow_t;

/* Result of executing a statement or expression */
typedef struct {
    forge_flow_t   flow;
    forge_value_t  value;    /* Return value (if FLOW_RETURN) or expression result */
} forge_result_t;

/* ─────────────────────────────────────────────────────────────────────────────
 * Interpreter State
 * ───────────────────────────────────────────────────────────────────────────── */

/* Note: forge_interp_t is forward-declared in common.h */

#define INTERP_MAX_CALL_DEPTH 256

typedef struct {
    const char*     proc_name;
    const char*     filename;
    int             line;
} forge_call_frame_t;

/* Module info stored for each loaded module */
typedef struct {
    const char*         name;        /* Module name (e.g., "math", "forge.io") */
    const char*         filepath;    /* Source file path */
    forge_node_t*       program;     /* Parsed AST (NODE_PROGRAM) */
    forge_env_t*        env;         /* Module's global environment */
    forge_hashmap_t*    procedures;  /* Module's procedures */
    forge_hashmap_t*    records;     /* Module's record definitions */
    int                 initialized; /* Whether init() has been called */
    int                 is_stdlib;   /* 1 if this is a stdlib module (forge.*) */
} forge_module_t;

struct forge_interp {
    forge_arena_t*      arena;       /* Arena for AST allocations */
    forge_strtable_t*   strtable;    /* String interning */

    /* Module table: module name -> forge_module_t* */
    forge_hashmap_t*    modules;

    /* Current module being executed */
    forge_module_t*     current_module;

    /* Main module (entry point) */
    forge_module_t*     main_module;

    /* Procedure table: name -> NODE_PROC_DECL (for main module compat) */
    forge_hashmap_t*    procedures;

    /* Record definitions: name -> NODE_RECORD_DECL (for main module compat) */
    forge_hashmap_t*    records;

    /* Global environment (main module's env) */
    forge_env_t*        globals;

    /* Channel registry: channel name -> forge_channel_t* */
    forge_hashmap_t*    channels;

    /* Current source file for error reporting (when no module context) */
    const char*         current_filename;

    /* Call stack for error reporting */
    forge_call_frame_t  call_stack[INTERP_MAX_CALL_DEPTH];
    int                 call_depth;

    /* Error state */
    int                 had_error;
    char                error_msg[1024];
};

/* ─────────────────────────────────────────────────────────────────────────────
 * Interpreter Lifecycle
 * ───────────────────────────────────────────────────────────────────────────── */

/*
 * Create a new interpreter with the given arena and string table.
 */
forge_interp_t* interp_create(forge_arena_t* arena, forge_strtable_t* strtable);

/*
 * Destroy the interpreter and free all resources.
 */
void interp_destroy(forge_interp_t* interp);

/* ─────────────────────────────────────────────────────────────────────────────
 * Program Execution
 * ───────────────────────────────────────────────────────────────────────────── */

/*
 * Execute a program (NODE_PROGRAM).
 * Registers all declarations, then calls main() if present.
 */
void interp_run(forge_interp_t* interp, forge_node_t* program);

/* ─────────────────────────────────────────────────────────────────────────────
 * Module Loading
 * ───────────────────────────────────────────────────────────────────────────── */

/*
 * Load a module from a parsed program AST.
 * Returns the module, or NULL on error.
 */
forge_module_t* interp_load_module(forge_interp_t* interp, const char* name,
                                    const char* filepath, forge_node_t* program);

/*
 * Get a loaded module by name. Returns NULL if not found.
 */
forge_module_t* interp_get_module(forge_interp_t* interp, const char* name);

/*
 * Initialize a module (call init() if present).
 */
void interp_init_module(forge_interp_t* interp, forge_module_t* module);

/*
 * Look up a procedure in a module by name.
 */
forge_node_t* interp_module_get_proc(forge_module_t* module, const char* name);

/*
 * Look up a record definition in a module by name.
 */
forge_node_t* interp_module_get_record(forge_module_t* module, const char* name);

/* ─────────────────────────────────────────────────────────────────────────────
 * Channel System
 * ───────────────────────────────────────────────────────────────────────────── */

#define INTERP_MAX_HANDLERS 64

/*
 * Handler entry: either a NODE_ON_HANDLER or a node + module pair
 */
typedef struct {
    forge_node_t*   handler;      /* NODE_ON_HANDLER */
    forge_module_t* module;       /* Module where handler is defined (NULL = main) */
} forge_handler_entry_t;

/*
 * Channel runtime structure.
 * Stores channel info and registered handlers.
 */
typedef struct {
    const char*           name;          /* Channel name */
    forge_node_t*         decl;          /* NODE_CHANNEL_DECL */
    forge_handler_entry_t handlers[INTERP_MAX_HANDLERS];
    int                   handler_count;
} forge_channel_t;

/*
 * Register a channel declaration.
 */
void interp_register_channel(forge_interp_t* interp, forge_node_t* decl);

/*
 * Register a handler for a channel.
 */
void interp_register_handler(forge_interp_t* interp, forge_node_t* handler,
                              forge_module_t* module);

/*
 * Emit a message on a channel.
 * Calls all registered handlers synchronously.
 */
void interp_emit(forge_interp_t* interp, forge_env_t* env,
                  const char* channel_name, forge_value_t* payload);

/* ─────────────────────────────────────────────────────────────────────────────
 * Expression Evaluation
 * ───────────────────────────────────────────────────────────────────────────── */

/*
 * Evaluate an expression node and return its value.
 * The caller is responsible for calling val_free on the result.
 */
forge_value_t interp_eval_expr(forge_interp_t* interp, forge_env_t* env,
                                forge_node_t* expr);

/* ─────────────────────────────────────────────────────────────────────────────
 * Statement Execution
 * ───────────────────────────────────────────────────────────────────────────── */

/*
 * Execute a statement or block.
 * Returns result with flow control status and optional value.
 */
forge_result_t interp_exec_stmt(forge_interp_t* interp, forge_env_t* env,
                                 forge_node_t* stmt);

/*
 * Execute a block (list of statements).
 */
forge_result_t interp_exec_block(forge_interp_t* interp, forge_env_t* env,
                                  forge_node_t* block);

/* ─────────────────────────────────────────────────────────────────────────────
 * Procedure Calls
 * ───────────────────────────────────────────────────────────────────────────── */

/*
 * Call a procedure by name with arguments.
 */
forge_value_t interp_call_proc(forge_interp_t* interp, forge_env_t* env,
                                const char* name, forge_value_t* args, int arg_count);

/* ─────────────────────────────────────────────────────────────────────────────
 * Error Handling
 * ───────────────────────────────────────────────────────────────────────────── */

/*
 * Report a runtime error. Sets had_error flag.
 */
void interp_error(forge_interp_t* interp, int line, int col, const char* fmt, ...);

/*
 * Print the current call stack (for debugging).
 */
void interp_print_stack(forge_interp_t* interp);

/*
 * Set command-line arguments for forge.sys.args().
 * Call this before running any program.
 */
void interp_set_args(int argc, char** argv);

#endif /* FORGE_INTERP_H */

