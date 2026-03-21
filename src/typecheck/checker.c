/*
 * FORGE Language Toolchain
 * checker.c - Static type checker implementation
 */

#include "typecheck/checker.h"
#include "lexer/lexer.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

/* ─────────────────────────────────────────────────────────────────────────────
 * Forward Declarations
 * ───────────────────────────────────────────────────────────────────────────── */

static int pass1_collect_declarations(forge_checker_t* checker, forge_node_t* program);
static int pass2_check_bodies(forge_checker_t* checker, forge_node_t* program);
static void check_statement(forge_checker_t* checker, forge_node_t* stmt);
static void check_block(forge_checker_t* checker, forge_node_t* block);

/* ─────────────────────────────────────────────────────────────────────────────
 * Checker Lifecycle
 * ───────────────────────────────────────────────────────────────────────────── */

forge_checker_t* checker_create(forge_arena_t* arena, forge_strtable_t* strtable,
                                 const char* filename) {
    forge_checker_t* c = forge_calloc(1, sizeof(forge_checker_t));

    c->types = hashmap_create();
    c->procs = hashmap_create();
    c->channels = hashmap_create();
    c->consts = hashmap_create();
    c->imports = hashmap_create();
    c->local_vars = hashmap_create();

    c->arena = arena;
    c->strtable = strtable;
    c->filename = filename;
    c->had_error = 0;
    c->error_count = 0;
    c->warning_count = 0;
    c->current_return_type = NULL;

    /* Register built-in functions */
    /* print(val: any) -> void */
    forge_proc_sig_t* print_sig = ARENA_ALLOC(arena, forge_proc_sig_t);
    print_sig->name = "print";
    print_sig->param_count = 1;
    print_sig->param_types = ARENA_ALLOC_ARRAY(arena, forge_type_t*, 1);
    print_sig->param_names = ARENA_ALLOC_ARRAY(arena, const char*, 1);
    print_sig->param_types[0] = type_prim(arena, TY_ANY);  /* Accept any type */
    print_sig->param_names[0] = "val";
    print_sig->return_type = type_prim(arena, TY_VOID);
    hashmap_set(c->procs, "print", print_sig);

    /* len(arr: any) -> int */
    forge_proc_sig_t* len_sig = ARENA_ALLOC(arena, forge_proc_sig_t);
    len_sig->name = "len";
    len_sig->param_count = 1;
    len_sig->param_types = ARENA_ALLOC_ARRAY(arena, forge_type_t*, 1);
    len_sig->param_names = ARENA_ALLOC_ARRAY(arena, const char*, 1);
    len_sig->param_types[0] = type_prim(arena, TY_ANY);
    len_sig->param_names[0] = "arr";
    len_sig->return_type = type_prim(arena, TY_INT);
    hashmap_set(c->procs, "len", len_sig);

    /* str(val: any) -> str */
    forge_proc_sig_t* str_sig = ARENA_ALLOC(arena, forge_proc_sig_t);
    str_sig->name = "str";
    str_sig->param_count = 1;
    str_sig->param_types = ARENA_ALLOC_ARRAY(arena, forge_type_t*, 1);
    str_sig->param_names = ARENA_ALLOC_ARRAY(arena, const char*, 1);
    str_sig->param_types[0] = type_prim(arena, TY_ANY);
    str_sig->param_names[0] = "val";
    str_sig->return_type = type_prim(arena, TY_STR);
    hashmap_set(c->procs, "str", str_sig);

    /* append(arr: any, val: any) -> any (returns array with element appended) */
    forge_proc_sig_t* append_sig = ARENA_ALLOC(arena, forge_proc_sig_t);
    append_sig->name = "append";
    append_sig->param_count = 2;
    append_sig->param_types = ARENA_ALLOC_ARRAY(arena, forge_type_t*, 2);
    append_sig->param_names = ARENA_ALLOC_ARRAY(arena, const char*, 2);
    append_sig->param_types[0] = type_prim(arena, TY_ANY);
    append_sig->param_types[1] = type_prim(arena, TY_ANY);
    append_sig->param_names[0] = "arr";
    append_sig->param_names[1] = "val";
    append_sig->return_type = type_prim(arena, TY_ANY);
    hashmap_set(c->procs, "append", append_sig);

    /* type(val: any) -> str */
    forge_proc_sig_t* type_sig = ARENA_ALLOC(arena, forge_proc_sig_t);
    type_sig->name = "type";
    type_sig->param_count = 1;
    type_sig->param_types = ARENA_ALLOC_ARRAY(arena, forge_type_t*, 1);
    type_sig->param_names = ARENA_ALLOC_ARRAY(arena, const char*, 1);
    type_sig->param_types[0] = type_prim(arena, TY_ANY);
    type_sig->param_names[0] = "val";
    type_sig->return_type = type_prim(arena, TY_STR);
    hashmap_set(c->procs, "type", type_sig);

    /* swap(a: any, b: any) -> void */
    forge_proc_sig_t* swap_sig = ARENA_ALLOC(arena, forge_proc_sig_t);
    swap_sig->name = "swap";
    swap_sig->param_count = 2;
    swap_sig->param_types = ARENA_ALLOC_ARRAY(arena, forge_type_t*, 2);
    swap_sig->param_names = ARENA_ALLOC_ARRAY(arena, const char*, 2);
    swap_sig->param_types[0] = type_prim(arena, TY_ANY);
    swap_sig->param_types[1] = type_prim(arena, TY_ANY);
    swap_sig->param_names[0] = "a";
    swap_sig->param_names[1] = "b";
    swap_sig->return_type = type_prim(arena, TY_VOID);
    hashmap_set(c->procs, "swap", swap_sig);

    return c;
}

void checker_destroy(forge_checker_t* checker) {
    if (!checker) return;

    hashmap_destroy(checker->types);
    hashmap_destroy(checker->procs);
    hashmap_destroy(checker->channels);
    hashmap_destroy(checker->consts);
    hashmap_destroy(checker->imports);
    hashmap_destroy(checker->local_vars);

    forge_free(checker);
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Error Reporting
 * ───────────────────────────────────────────────────────────────────────────── */

void checker_error(forge_checker_t* checker, forge_node_t* node,
                   const char* fmt, ...) {
    checker->had_error = 1;
    checker->error_count++;

    int line = node ? node->line : 0;
    int col = node ? node->column : 0;

    fprintf(stderr, "%s:%d:%d: error: ", checker->filename, line, col);

    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);

    fprintf(stderr, "\n");
}

void checker_warning(forge_checker_t* checker, forge_node_t* node,
                     const char* fmt, ...) {
    checker->warning_count++;

    int line = node ? node->line : 0;
    int col = node ? node->column : 0;

    fprintf(stderr, "%s:%d:%d: warning: ", checker->filename, line, col);

    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);

    fprintf(stderr, "\n");
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Symbol Table Access
 * ───────────────────────────────────────────────────────────────────────────── */

forge_type_t* checker_lookup_type(forge_checker_t* checker, const char* name) {
    return (forge_type_t*)hashmap_get(checker->types, name);
}

forge_proc_sig_t* checker_lookup_proc(forge_checker_t* checker, const char* name) {
    return (forge_proc_sig_t*)hashmap_get(checker->procs, name);
}

forge_channel_decl_t* checker_lookup_channel(forge_checker_t* checker, const char* name) {
    return (forge_channel_decl_t*)hashmap_get(checker->channels, name);
}

forge_type_t* checker_lookup_var(forge_checker_t* checker, const char* name) {
    return (forge_type_t*)hashmap_get(checker->local_vars, name);
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Main Entry Point
 * ───────────────────────────────────────────────────────────────────────────── */

int checker_check(forge_checker_t* checker, forge_node_t* program) {
    if (!checker || !program) return -1;

    if (program->kind != NODE_PROGRAM) {
        checker_error(checker, program, "expected PROGRAM node at top level");
        return -1;
    }

    /* Pass 1: Collect all declarations */
    if (pass1_collect_declarations(checker, program) != 0) {
        /* Continue to Pass 2 to report more errors */
    }

    /* Pass 2: Check procedure bodies and initializers */
    if (pass2_check_bodies(checker, program) != 0) {
        /* Errors already reported */
    }

    return checker->had_error ? checker->error_count : 0;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Type Node Resolution
 * ───────────────────────────────────────────────────────────────────────────── */

/* Convert a type AST node (NODE_TYPE_*) to a forge_type_t */
static forge_type_t* resolve_type_node(forge_checker_t* checker, forge_node_t* type_node) {
    if (!type_node) {
        return type_prim(checker->arena, TY_VOID);
    }

    forge_type_t* result = NULL;

    switch (type_node->kind) {
        case NODE_TYPE_PRIM: {
            /* Map primitive type name to kind - data.name is at union top level */
            const char* name = type_node->data.name;
            if (strcmp(name, "int") == 0)       result = type_prim(checker->arena, TY_INT);
            else if (strcmp(name, "int8") == 0)      result = type_prim(checker->arena, TY_INT8);
            else if (strcmp(name, "int16") == 0)     result = type_prim(checker->arena, TY_INT16);
            else if (strcmp(name, "int32") == 0)     result = type_prim(checker->arena, TY_INT32);
            else if (strcmp(name, "uint") == 0)      result = type_prim(checker->arena, TY_UINT);
            else if (strcmp(name, "uint8") == 0)     result = type_prim(checker->arena, TY_UINT8);
            else if (strcmp(name, "uint16") == 0)    result = type_prim(checker->arena, TY_UINT16);
            else if (strcmp(name, "uint32") == 0)    result = type_prim(checker->arena, TY_UINT32);
            else if (strcmp(name, "float") == 0)     result = type_prim(checker->arena, TY_FLOAT);
            else if (strcmp(name, "float32") == 0)   result = type_prim(checker->arena, TY_FLOAT32);
            else if (strcmp(name, "bool") == 0)      result = type_prim(checker->arena, TY_BOOL);
            else if (strcmp(name, "str") == 0)       result = type_prim(checker->arena, TY_STR);
            else if (strcmp(name, "byte") == 0)      result = type_prim(checker->arena, TY_BYTE);
            else if (strcmp(name, "void") == 0)      result = type_prim(checker->arena, TY_VOID);
            else {
                /* Check if it's a user-defined type */
                forge_type_t* user_type = checker_lookup_type(checker, name);
                if (user_type) {
                    result = user_type;
                } else {
                    /* Unknown type - create unresolved placeholder */
                    checker_error(checker, type_node, "unknown type '%s'", name);
                    result = type_error(checker->arena);
                }
            }
            break;
        }

        case NODE_TYPE_NAMED: {
            /* User-defined type reference */
            const char* name = type_node->data.name;
            forge_type_t* user_type = checker_lookup_type(checker, name);
            if (user_type) {
                result = user_type;
            } else {
                checker_error(checker, type_node, "unknown type '%s'", name);
                result = type_error(checker->arena);
            }
            break;
        }

        case NODE_TYPE_OPTIONAL: {
            forge_type_t* inner = resolve_type_node(checker, type_node->data.type_optional.inner_type);
            result = type_optional(checker->arena, inner);
            break;
        }

        case NODE_TYPE_FIXED_ARRAY: {
            forge_type_t* elem = resolve_type_node(checker, type_node->data.type_fixed_array.elem_type);
            result = type_fixed_array(checker->arena, elem, type_node->data.type_fixed_array.size);
            break;
        }

        case NODE_TYPE_DYN_ARRAY: {
            forge_type_t* elem = resolve_type_node(checker, type_node->data.type_dyn_array.elem_type);
            result = type_dyn_array(checker->arena, elem);
            break;
        }

        case NODE_TYPE_MAP: {
            forge_type_t* key = resolve_type_node(checker, type_node->data.type_map.key_type);
            forge_type_t* val = resolve_type_node(checker, type_node->data.type_map.val_type);
            result = type_map(checker->arena, key, val);
            break;
        }

        default:
            checker_error(checker, type_node, "invalid type node kind %d", type_node->kind);
            result = type_error(checker->arena);
            break;
    }

    /* Store resolved type on the type node for the emitter */
    type_node->resolved_type = result;
    return result;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Pass 1: Declaration Collection
 * ───────────────────────────────────────────────────────────────────────────── */

static int pass1_collect_declarations(forge_checker_t* checker, forge_node_t* program) {
    /* program->data.program.decls is a dynamic array of declaration nodes */
    forge_node_t** decls = program->data.program.decls;
    int decl_count = program->data.program.decl_count;

    for (int i = 0; i < decl_count; i++) {
        forge_node_t* decl = decls[i];

        switch (decl->kind) {
            case NODE_PROC_DECL: {
                /* Build procedure signature */
                const char* name = decl->data.proc.name;

                /* Check for duplicate */
                if (checker_lookup_proc(checker, name)) {
                    checker_error(checker, decl, "duplicate procedure '%s'", name);
                    continue;
                }

                /* Allocate signature in arena */
                forge_proc_sig_t* sig = ARENA_ALLOC(checker->arena, forge_proc_sig_t);
                sig->name = name;
                sig->param_count = decl->data.proc.param_count;
                sig->decl_node = decl;

                /* Allocate arrays for params */
                if (sig->param_count > 0) {
                    sig->param_types = ARENA_ALLOC_ARRAY(checker->arena, forge_type_t*, sig->param_count);
                    sig->param_names = ARENA_ALLOC_ARRAY(checker->arena, const char*, sig->param_count);

                    for (int p = 0; p < sig->param_count; p++) {
                        sig->param_names[p] = decl->data.proc.params[p].name;
                        /* Resolve parameter type */
                        forge_node_t* ptype_node = decl->data.proc.params[p].type_expr;
                        sig->param_types[p] = resolve_type_node(checker, ptype_node);
                    }
                } else {
                    sig->param_types = NULL;
                    sig->param_names = NULL;
                }

                /* Resolve return type */
                if (decl->data.proc.return_type) {
                    sig->return_type = resolve_type_node(checker, decl->data.proc.return_type);
                } else {
                    sig->return_type = NULL;  /* void */
                }

                hashmap_set(checker->procs, name, sig);
                break;
            }

            case NODE_RECORD_DECL: {
                const char* name = decl->data.record.name;

                if (checker_lookup_type(checker, name)) {
                    checker_error(checker, decl, "duplicate type '%s'", name);
                    continue;
                }

                /* Build record type - fields are NODE_FIELD_DEF nodes */
                int field_count = decl->data.record.field_count;
                const char** field_names = NULL;
                forge_type_t** field_types = NULL;

                if (field_count > 0) {
                    field_names = ARENA_ALLOC_ARRAY(checker->arena, const char*, field_count);
                    field_types = ARENA_ALLOC_ARRAY(checker->arena, forge_type_t*, field_count);

                    for (int f = 0; f < field_count; f++) {
                        forge_node_t* field_node = decl->data.record.fields[f];
                        field_names[f] = field_node->data.field_def.name;
                        field_types[f] = resolve_type_node(checker, field_node->data.field_def.type_expr);
                        /* Store resolved type on the field node for the emitter */
                        field_node->resolved_type = field_types[f];
                    }
                }

                forge_type_t* rec_type = type_record(checker->arena, name,
                                                      field_names, field_types, field_count);
                hashmap_set(checker->types, name, rec_type);

                /* Store the resolved type on the declaration node for the emitter */
                decl->resolved_type = rec_type;
                break;
            }

            case NODE_CHANNEL_DECL: {
                const char* name = decl->data.channel.name;

                if (checker_lookup_channel(checker, name)) {
                    checker_error(checker, decl, "duplicate channel '%s'", name);
                    continue;
                }

                forge_channel_decl_t* ch = ARENA_ALLOC(checker->arena, forge_channel_decl_t);
                ch->name = name;
                ch->decl_node = decl;

                if (decl->data.channel.payload_type) {
                    ch->payload_type = resolve_type_node(checker, decl->data.channel.payload_type);
                } else {
                    ch->payload_type = type_prim(checker->arena, TY_VOID);
                }

                hashmap_set(checker->channels, name, ch);
                break;
            }

            case NODE_VAR_DECL: {
                /* Top-level const declaration */
                const char* name = decl->data.var_decl.name;

                if (hashmap_get(checker->consts, name)) {
                    checker_error(checker, decl, "duplicate constant '%s'", name);
                    continue;
                }

                /* For constants, we need the type from either annotation or initializer */
                forge_type_t* const_type = NULL;
                if (decl->data.var_decl.type_expr) {
                    const_type = resolve_type_node(checker, decl->data.var_decl.type_expr);
                }
                /* If no type annotation, we'll infer in Pass 2 */

                if (const_type) {
                    hashmap_set(checker->consts, name, const_type);
                }
                break;
            }

            case NODE_IMPORT:
                /* TODO: Handle imports - load and check imported module */
                break;

            case NODE_ON_HANDLER:
                /* Handlers are checked in Pass 2 */
                break;

            default:
                /* Skip other node types for now */
                break;
        }
    }

    return checker->had_error ? -1 : 0;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Pass 2: Body Checking
 * ───────────────────────────────────────────────────────────────────────────── */

static int pass2_check_bodies(forge_checker_t* checker, forge_node_t* program) {
    forge_node_t** decls = program->data.program.decls;
    int decl_count = program->data.program.decl_count;

    for (int i = 0; i < decl_count; i++) {
        forge_node_t* decl = decls[i];

        switch (decl->kind) {
            case NODE_PROC_DECL: {
                /* Set up context for checking this procedure */
                forge_proc_sig_t* sig = checker_lookup_proc(checker, decl->data.proc.name);
                if (!sig) continue;  /* Error already reported in Pass 1 */

                checker->current_return_type = sig->return_type;

                /* Clear and populate local variables with parameters */
                hashmap_clear(checker->local_vars);
                for (int p = 0; p < sig->param_count; p++) {
                    hashmap_set(checker->local_vars, sig->param_names[p], sig->param_types[p]);
                }

                /* Check procedure body */
                if (decl->data.proc.body) {
                    check_block(checker, decl->data.proc.body);
                }

                checker->current_return_type = NULL;
                break;
            }

            case NODE_ON_HANDLER: {
                /* Check handler body */
                /* TODO: Get channel payload type for context */
                hashmap_clear(checker->local_vars);

                /* Add event parameter if present */
                if (decl->data.on_handler.param_name) {
                    forge_channel_decl_t* ch = checker_lookup_channel(checker,
                        decl->data.on_handler.channel_name);
                    if (ch) {
                        hashmap_set(checker->local_vars,
                                    decl->data.on_handler.param_name,
                                    ch->payload_type);
                    }
                }

                checker->current_return_type = NULL;
                if (decl->data.on_handler.body) {
                    check_block(checker, decl->data.on_handler.body);
                }
                break;
            }

            case NODE_VAR_DECL: {
                /* Top-level variable/constant - check initializer type */
                if (decl->data.var_decl.init_expr) {
                    forge_type_t* init_type = checker_type_of(checker, decl->data.var_decl.init_expr);

                    forge_type_t* decl_type = hashmap_get(checker->consts, decl->data.var_decl.name);
                    if (decl_type && init_type && !type_is_assignable(decl_type, init_type)) {
                        char* expected = type_to_str(decl_type);
                        char* got = type_to_str(init_type);
                        checker_error(checker, decl,
                            "type mismatch in constant '%s': expected %s, got %s",
                            decl->data.var_decl.name, expected, got);
                        forge_free(expected);
                        forge_free(got);
                    } else if (!decl_type && init_type) {
                        /* Infer type from initializer */
                        hashmap_set(checker->consts, decl->data.var_decl.name, init_type);
                    }
                }
                break;
            }

            default:
                break;
        }
    }

    return checker->had_error ? -1 : 0;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Statement Checking
 * ───────────────────────────────────────────────────────────────────────────── */

static void check_block(forge_checker_t* checker, forge_node_t* block) {
    if (!block || block->kind != NODE_BLOCK) return;

    forge_node_t** stmts = block->data.block.stmts;
    int stmt_count = block->data.block.count;

    for (int i = 0; i < stmt_count; i++) {
        check_statement(checker, stmts[i]);
    }
}

static void check_statement(forge_checker_t* checker, forge_node_t* stmt) {
    if (!stmt) return;

    switch (stmt->kind) {
        case NODE_VAR_DECL: {
            /* Local variable declaration */
            const char* name = stmt->data.var_decl.name;
            forge_type_t* decl_type = NULL;
            forge_type_t* init_type = NULL;

            if (stmt->data.var_decl.type_expr) {
                decl_type = resolve_type_node(checker, stmt->data.var_decl.type_expr);
            }
            if (stmt->data.var_decl.init_expr) {
                /* Allow empty array literal [] when target type is a dynamic array */
                if (decl_type && decl_type->kind == TY_DYN_ARRAY &&
                    stmt->data.var_decl.init_expr->kind == NODE_ARRAY_LITERAL &&
                    stmt->data.var_decl.init_expr->data.array_lit.count == 0) {
                    /* Empty array literal matches declared array type */
                    init_type = decl_type;
                    stmt->data.var_decl.init_expr->resolved_type = decl_type;
                } else {
                    init_type = checker_type_of(checker, stmt->data.var_decl.init_expr);
                }

                /* Propagate optional type to none literals for the emitter */
                if (decl_type && decl_type->kind == TY_OPTIONAL &&
                    stmt->data.var_decl.init_expr->kind == NODE_NONE_LIT) {
                    stmt->data.var_decl.init_expr->resolved_type = decl_type;
                }
            }

            if (decl_type && init_type && !type_is_assignable(decl_type, init_type)) {
                char* expected = type_to_str(decl_type);
                char* got = type_to_str(init_type);
                checker_error(checker, stmt,
                    "type mismatch in variable '%s': expected %s, got %s",
                    name, expected, got);
                forge_free(expected);
                forge_free(got);
            }

            /* Register variable in local scope */
            forge_type_t* var_type = decl_type ? decl_type : init_type;
            if (var_type) {
                hashmap_set(checker->local_vars, name, var_type);
                /* Store resolved type on the var decl node for the emitter */
                stmt->resolved_type = var_type;
            }
            break;
        }

        case NODE_ASSIGN: {
            /* Check target and value types match */
            forge_type_t* target_type = checker_type_of(checker, stmt->data.assign.target);
            forge_type_t* value_type = checker_type_of(checker, stmt->data.assign.value);

            if (target_type && value_type && !type_is_assignable(target_type, value_type)) {
                char* expected = type_to_str(target_type);
                char* got = type_to_str(value_type);
                checker_error(checker, stmt,
                    "type mismatch in assignment: expected %s, got %s",
                    expected, got);
                forge_free(expected);
                forge_free(got);
            }
            break;
        }

        case NODE_IF: {
            /* Condition must be bool */
            forge_type_t* cond_type = checker_type_of(checker, stmt->data.if_stmt.condition);
            if (cond_type && cond_type->kind != TY_BOOL) {
                char* got = type_to_str(cond_type);
                checker_error(checker, stmt->data.if_stmt.condition,
                    "if condition must be bool, got %s", got);
                forge_free(got);
            }
            check_block(checker, stmt->data.if_stmt.then_body);

            /* Check elif conditions and bodies */
            for (int i = 0; i < stmt->data.if_stmt.elif_count; i++) {
                forge_type_t* elif_cond_type = checker_type_of(checker, stmt->data.if_stmt.elif_conditions[i]);
                if (elif_cond_type && elif_cond_type->kind != TY_BOOL) {
                    char* got = type_to_str(elif_cond_type);
                    checker_error(checker, stmt->data.if_stmt.elif_conditions[i],
                        "elif condition must be bool, got %s", got);
                    forge_free(got);
                }
                check_block(checker, stmt->data.if_stmt.elif_bodies[i]);
            }

            if (stmt->data.if_stmt.else_body) {
                check_block(checker, stmt->data.if_stmt.else_body);
            }
            break;
        }

        case NODE_WHILE: {
            /* Condition must be bool */
            forge_type_t* cond_type = checker_type_of(checker, stmt->data.while_stmt.condition);
            if (cond_type && cond_type->kind != TY_BOOL) {
                char* got = type_to_str(cond_type);
                checker_error(checker, stmt->data.while_stmt.condition,
                    "while condition must be bool, got %s", got);
                forge_free(got);
            }
            check_block(checker, stmt->data.while_stmt.body);
            break;
        }

        case NODE_RETURN: {
            forge_type_t* ret_type = NULL;
            if (stmt->data.return_stmt.value) {
                ret_type = checker_type_of(checker, stmt->data.return_stmt.value);
            }

            if (checker->current_return_type) {
                if (!ret_type) {
                    char* expected = type_to_str(checker->current_return_type);
                    checker_error(checker, stmt,
                        "return statement missing value, expected %s", expected);
                    forge_free(expected);
                } else if (!type_is_assignable(checker->current_return_type, ret_type)) {
                    char* expected = type_to_str(checker->current_return_type);
                    char* got = type_to_str(ret_type);
                    checker_error(checker, stmt,
                        "return type mismatch: expected %s, got %s", expected, got);
                    forge_free(expected);
                    forge_free(got);
                }
            } else if (ret_type) {
                checker_error(checker, stmt,
                    "cannot return value from void procedure");
            }
            break;
        }

        case NODE_FOR: {
            /* For loop: check iterable, bind loop var, check body */
            forge_type_t* iter_type = checker_type_of(checker, stmt->data.for_stmt.iterable);
            forge_type_t* elem_type = NULL;

            if (iter_type) {
                /* Resolve through alias if needed */
                while (iter_type->kind == TY_ALIAS) {
                    iter_type = iter_type->as.alias.target;
                }

                if (iter_type->kind == TY_FIXED_ARRAY) {
                    elem_type = iter_type->as.fixed_array.elem_type;
                } else if (iter_type->kind == TY_DYN_ARRAY) {
                    elem_type = iter_type->as.dyn_array.elem_type;
                } else if (iter_type->kind == TY_STR) {
                    elem_type = type_prim(checker->arena, TY_BYTE);
                } else {
                    char* got = type_to_str(iter_type);
                    checker_error(checker, stmt->data.for_stmt.iterable,
                        "for loop requires iterable type (array/string/range), got %s", got);
                    forge_free(got);
                    elem_type = type_error(checker->arena);
                }
            }

            /* Bind loop variable */
            if (elem_type && stmt->data.for_stmt.var_name) {
                hashmap_set(checker->local_vars, stmt->data.for_stmt.var_name, elem_type);
            }

            check_block(checker, stmt->data.for_stmt.body);
            break;
        }

        case NODE_LOOP:
            /* Infinite loop - just check body */
            check_block(checker, stmt->data.loop_stmt.body);
            break;

        case NODE_EMIT: {
            /* Check channel exists and payload type matches */
            const char* ch_name = stmt->data.emit_stmt.channel_name;
            forge_channel_decl_t* ch = checker_lookup_channel(checker, ch_name);

            if (!ch) {
                checker_error(checker, stmt, "undefined channel '%s'", ch_name);
            } else if (ch->payload_type && stmt->data.emit_stmt.payload) {
                forge_type_t* payload_type = checker_type_of(checker, stmt->data.emit_stmt.payload);
                if (payload_type && !type_is_assignable(ch->payload_type, payload_type)) {
                    char* expected = type_to_str(ch->payload_type);
                    char* got = type_to_str(payload_type);
                    checker_error(checker, stmt->data.emit_stmt.payload,
                        "emit payload type mismatch for '%s': expected %s, got %s",
                        ch_name, expected, got);
                    forge_free(expected);
                    forge_free(got);
                }
            } else if (ch->payload_type && !stmt->data.emit_stmt.payload) {
                char* expected = type_to_str(ch->payload_type);
                checker_error(checker, stmt,
                    "emit to '%s' requires payload of type %s", ch_name, expected);
                forge_free(expected);
            } else if (!ch->payload_type && stmt->data.emit_stmt.payload) {
                checker_error(checker, stmt,
                    "channel '%s' does not accept a payload", ch_name);
            }
            break;
        }

        case NODE_FREE: {
            /* Type-check the expression being freed */
            checker_type_of(checker, stmt->data.free_stmt.expr);
            break;
        }

        case NODE_WITH_ALLOC: {
            /* Resolve the type and check the size expression */
            forge_type_t* alloc_type = NULL;
            if (stmt->data.with_alloc.type_expr) {
                alloc_type = resolve_type_node(checker, stmt->data.with_alloc.type_expr);
            }
            if (stmt->data.with_alloc.size_expr) {
                forge_type_t* size_type = checker_type_of(checker, stmt->data.with_alloc.size_expr);
                if (size_type && !type_is_integer(size_type)) {
                    char* got = type_to_str(size_type);
                    checker_error(checker, stmt->data.with_alloc.size_expr,
                        "alloc size must be integer, got %s", got);
                    forge_free(got);
                }
            }
            /* Bind the variable in local scope as a dynamic array of the element type */
            if (alloc_type && stmt->data.with_alloc.var_name) {
                forge_type_t* array_type = type_dyn_array(checker->arena, alloc_type);
                hashmap_set(checker->local_vars, stmt->data.with_alloc.var_name, array_type);
            }
            /* Check the body block */
            check_block(checker, stmt->data.with_alloc.body);
            break;
        }

        case NODE_BREAK:
        case NODE_CONTINUE:
            /* These are control flow; no type checking needed */
            break;

        case NODE_BLOCK:
            check_block(checker, stmt);
            break;

        case NODE_CALL:
            /* Expression statement - just type check it */
            checker_type_of(checker, stmt);
            break;

        case NODE_EXPR_STMT:
            /* Expression statement wrapper - type check the inner expression */
            checker_type_of(checker, stmt->data.expr_stmt.expr);
            break;

        case NODE_COMPOUND_ASSIGN: {
            /* Compound assignment (e.g., x += 1) */
            forge_type_t* target_type = checker_type_of(checker, stmt->data.compound_assign.target);
            forge_type_t* value_type = checker_type_of(checker, stmt->data.compound_assign.value);

            /* The target and value must be compatible for the compound operator */
            if (target_type && value_type) {
                /* For numeric compound ops, both must be numeric */
                if (!type_is_numeric(target_type) || !type_is_numeric(value_type)) {
                    int op = stmt->data.compound_assign.op;
                    /* String concatenation with += is allowed */
                    if (!(op == TOK_PLUS_EQ && target_type->kind == TY_STR && value_type->kind == TY_STR)) {
                        char* t = type_to_str(target_type);
                        char* v = type_to_str(value_type);
                        checker_error(checker, stmt,
                            "invalid compound assignment: %s and %s", t, v);
                        forge_free(t);
                        forge_free(v);
                    }
                }
            }
            break;
        }

        default:
            /* Unhandled statement kind */
            break;
    }
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Expression Type Inference (Task 4.3)
 * ───────────────────────────────────────────────────────────────────────────── */

/* Helper: check if an operation is comparison (returns bool) */
static int is_comparison_op(int op) {
    return op == TOK_EQ || op == TOK_NEQ ||
           op == TOK_LT || op == TOK_GT ||
           op == TOK_LEQ || op == TOK_GEQ;
}

/* Helper: check if an operation is logical (requires bool operands) */
static int is_logical_op(int op) {
    return op == TOK_AND || op == TOK_OR;
}

/* Helper: check if an operation is bitwise */
static int is_bitwise_op(int op) {
    return op == TOK_AMP || op == TOK_PIPE || op == TOK_CARET ||
           op == TOK_LSHIFT || op == TOK_RSHIFT;
}

forge_type_t* checker_type_of(forge_checker_t* checker, forge_node_t* node) {
    if (!node) return NULL;

    /* If already resolved, return cached type */
    if (node->resolved_type) {
        return node->resolved_type;
    }

    forge_type_t* result = NULL;

    switch (node->kind) {
        /* === Literals === */
        case NODE_INT_LIT:
            result = type_prim(checker->arena, TY_INT);
            break;

        case NODE_FLOAT_LIT:
            result = type_prim(checker->arena, TY_FLOAT);
            break;

        case NODE_STR_LIT:
            result = type_prim(checker->arena, TY_STR);
            break;

        case NODE_BOOL_LIT:
            result = type_prim(checker->arena, TY_BOOL);
            break;

        case NODE_NONE_LIT:
            result = type_prim(checker->arena, TY_NONE);
            break;

        /* === Identifiers === */
        case NODE_IDENT: {
            const char* name = node->data.name;
            result = checker_lookup_var(checker, name);
            if (!result) {
                result = hashmap_get(checker->consts, name);
            }
            if (!result) {
                checker_error(checker, node, "undefined variable '%s'", name);
                result = type_error(checker->arena);
            }
            break;
        }

        case NODE_QUALIFIED_IDENT: {
            /* TODO: Look up in imported module */
            const char* mod = node->data.qualified.module_name;
            const char* sym = node->data.qualified.symbol_name;
            checker_error(checker, node, "qualified identifier '%s.%s' not yet supported", mod, sym);
            result = type_error(checker->arena);
            break;
        }

        /* === Binary Operations === */
        case NODE_BINARY_OP: {
            int op = node->data.binop.op;
            forge_type_t* left_type = checker_type_of(checker, node->data.binop.left);
            forge_type_t* right_type = checker_type_of(checker, node->data.binop.right);

            if (!left_type || !right_type) {
                result = type_error(checker->arena);
                break;
            }

            if (is_comparison_op(op)) {
                /* Comparison: operands must be compatible, result is bool */
                if (!type_equal(left_type, right_type) &&
                    !(type_is_numeric(left_type) && type_is_numeric(right_type))) {
                    char* l = type_to_str(left_type);
                    char* r = type_to_str(right_type);
                    checker_error(checker, node, "cannot compare %s and %s", l, r);
                    forge_free(l);
                    forge_free(r);
                }
                result = type_prim(checker->arena, TY_BOOL);
            } else if (is_logical_op(op)) {
                /* Logical: both operands must be bool */
                if (left_type->kind != TY_BOOL) {
                    char* got = type_to_str(left_type);
                    checker_error(checker, node->data.binop.left,
                        "logical operator requires bool, got %s", got);
                    forge_free(got);
                }
                if (right_type->kind != TY_BOOL) {
                    char* got = type_to_str(right_type);
                    checker_error(checker, node->data.binop.right,
                        "logical operator requires bool, got %s", got);
                    forge_free(got);
                }
                result = type_prim(checker->arena, TY_BOOL);
            } else if (is_bitwise_op(op)) {
                /* Bitwise: both operands must be integers */
                if (!type_is_integer(left_type)) {
                    char* got = type_to_str(left_type);
                    checker_error(checker, node->data.binop.left,
                        "bitwise operator requires integer, got %s", got);
                    forge_free(got);
                }
                if (!type_is_integer(right_type)) {
                    char* got = type_to_str(right_type);
                    checker_error(checker, node->data.binop.right,
                        "bitwise operator requires integer, got %s", got);
                    forge_free(got);
                }
                result = left_type;  /* Result is same type as left operand */
            } else if (op == TOK_PLUS && left_type->kind == TY_STR) {
                /* String concatenation */
                if (right_type->kind != TY_STR) {
                    char* got = type_to_str(right_type);
                    checker_error(checker, node->data.binop.right,
                        "cannot concatenate str with %s", got);
                    forge_free(got);
                }
                result = type_prim(checker->arena, TY_STR);
            } else {
                /* Arithmetic: +, -, *, /, % */
                if (!type_is_numeric(left_type)) {
                    char* got = type_to_str(left_type);
                    checker_error(checker, node->data.binop.left,
                        "arithmetic operator requires numeric type, got %s", got);
                    forge_free(got);
                }
                if (!type_is_numeric(right_type)) {
                    char* got = type_to_str(right_type);
                    checker_error(checker, node->data.binop.right,
                        "arithmetic operator requires numeric type, got %s", got);
                    forge_free(got);
                }
                /* If either is float, result is float; else result matches left */
                if (left_type->kind == TY_FLOAT || left_type->kind == TY_FLOAT32 ||
                    right_type->kind == TY_FLOAT || right_type->kind == TY_FLOAT32) {
                    result = type_prim(checker->arena, TY_FLOAT);
                } else {
                    result = left_type;
                }
            }
            break;
        }

        /* === Unary Operations === */
        case NODE_UNARY_OP: {
            int op = node->data.unop.op;
            forge_type_t* operand_type = checker_type_of(checker, node->data.unop.operand);

            if (!operand_type) {
                result = type_error(checker->arena);
                break;
            }

            if (op == TOK_NOT) {
                /* Logical not: operand must be bool */
                if (operand_type->kind != TY_BOOL) {
                    char* got = type_to_str(operand_type);
                    checker_error(checker, node->data.unop.operand,
                        "'not' requires bool, got %s", got);
                    forge_free(got);
                }
                result = type_prim(checker->arena, TY_BOOL);
            } else if (op == TOK_MINUS) {
                /* Numeric negation */
                if (!type_is_numeric(operand_type)) {
                    char* got = type_to_str(operand_type);
                    checker_error(checker, node->data.unop.operand,
                        "unary '-' requires numeric type, got %s", got);
                    forge_free(got);
                }
                result = operand_type;
            } else if (op == TOK_TILDE) {
                /* Bitwise not */
                if (!type_is_integer(operand_type)) {
                    char* got = type_to_str(operand_type);
                    checker_error(checker, node->data.unop.operand,
                        "bitwise '~' requires integer, got %s", got);
                    forge_free(got);
                }
                result = operand_type;
            } else {
                checker_error(checker, node, "unknown unary operator");
                result = type_error(checker->arena);
            }
            break;
        }

        /* === Call === */
        case NODE_CALL: {
            forge_node_t* callee = node->data.call.callee;
            const char* name = NULL;
            int is_stdlib_call = 0;
            const char* stdlib_module = NULL;
            const char* stdlib_func = NULL;

            if (callee && callee->kind == NODE_IDENT) {
                name = callee->data.name;
            }
            else if (callee && callee->kind == NODE_QUALIFIED_IDENT) {
                /* module.proc() call - check if stdlib */
                const char* mod_name = callee->data.qualified.module_name;
                if (strncmp(mod_name, "forge.", 6) == 0) {
                    is_stdlib_call = 1;
                    stdlib_module = mod_name + 6;  /* Skip "forge." */
                    stdlib_func = callee->data.qualified.symbol_name;
                } else if (strcmp(mod_name, "gui") == 0 || strcmp(mod_name, "serial") == 0 ||
                           strcmp(mod_name, "nmea") == 0 || strcmp(mod_name, "io") == 0 ||
                           strcmp(mod_name, "str") == 0 || strcmp(mod_name, "math") == 0 ||
                           strcmp(mod_name, "sys") == 0 || strcmp(mod_name, "time") == 0 ||
                           strcmp(mod_name, "buf") == 0) {
                    is_stdlib_call = 1;
                    stdlib_module = mod_name;
                    stdlib_func = callee->data.qualified.symbol_name;
                }
            }
            else if (callee && callee->kind == NODE_FIELD_ACCESS) {
                /* Check for forge.*.func() pattern (chained field access) */
                forge_node_t* inner = callee->data.field_access.object;
                if (inner && inner->kind == NODE_FIELD_ACCESS) {
                    forge_node_t* root = inner->data.field_access.object;
                    if (root && root->kind == NODE_IDENT &&
                        strcmp(root->data.name, "forge") == 0) {
                        is_stdlib_call = 1;
                        stdlib_module = inner->data.field_access.field_name;
                        stdlib_func = callee->data.field_access.field_name;
                    }
                }
                /* Check for mod.func() pattern (single field access) */
                else if (inner && inner->kind == NODE_IDENT) {
                    const char* mod_name = inner->data.name;
                    if (strcmp(mod_name, "gui") == 0 || strcmp(mod_name, "serial") == 0 ||
                        strcmp(mod_name, "nmea") == 0 || strcmp(mod_name, "io") == 0 ||
                        strcmp(mod_name, "str") == 0 || strcmp(mod_name, "math") == 0 ||
                        strcmp(mod_name, "sys") == 0 || strcmp(mod_name, "time") == 0 ||
                        strcmp(mod_name, "buf") == 0) {
                        is_stdlib_call = 1;
                        stdlib_module = mod_name;
                        stdlib_func = callee->data.field_access.field_name;
                    }
                }
            }

            if (is_stdlib_call) {
                /* Type check arguments */
                for (int i = 0; i < node->data.call.arg_count; i++) {
                    checker_type_of(checker, node->data.call.args[i]);
                }

                /* Determine return type based on module and function */
                if (stdlib_module && stdlib_func) {
                    if (strcmp(stdlib_module, "io") == 0) {
                        /* forge.io functions */
                        if (strcmp(stdlib_func, "read_line") == 0 ||
                            strcmp(stdlib_func, "read_line_prompt") == 0 ||
                            strcmp(stdlib_func, "read_file") == 0) {
                            result = type_prim(checker->arena, TY_STR);
                        } else if (strcmp(stdlib_func, "file_exists") == 0 ||
                                   strcmp(stdlib_func, "write_file") == 0 ||
                                   strcmp(stdlib_func, "append_file") == 0) {
                            result = type_prim(checker->arena, TY_BOOL);
                        } else {
                            /* print, print_raw, eprint return void */
                            result = type_prim(checker->arena, TY_VOID);
                        }
                    } else if (strcmp(stdlib_module, "str") == 0) {
                        /* forge.str functions (to be implemented) */
                        if (strcmp(stdlib_func, "len") == 0 ||
                            strcmp(stdlib_func, "find") == 0 ||
                            strcmp(stdlib_func, "count") == 0) {
                            result = type_prim(checker->arena, TY_INT);
                        } else if (strcmp(stdlib_func, "contains") == 0 ||
                                   strcmp(stdlib_func, "starts_with") == 0 ||
                                   strcmp(stdlib_func, "ends_with") == 0) {
                            result = type_prim(checker->arena, TY_BOOL);
                        } else if (strcmp(stdlib_func, "split") == 0) {
                            /* Returns []str - dynamic array of strings */
                            forge_type_t* elem = type_prim(checker->arena, TY_STR);
                            result = type_dyn_array(checker->arena, elem);
                        } else {
                            /* Most string functions return str */
                            result = type_prim(checker->arena, TY_STR);
                        }
                    } else if (strcmp(stdlib_module, "math") == 0) {
                        /* forge.math functions */
                        /* Integer return types */
                        if (strcmp(stdlib_func, "abs_int") == 0 ||
                            strcmp(stdlib_func, "min_int") == 0 ||
                            strcmp(stdlib_func, "max_int") == 0 ||
                            strcmp(stdlib_func, "random_int") == 0) {
                            result = type_prim(checker->arena, TY_INT);
                        }
                        /* Void return type */
                        else if (strcmp(stdlib_func, "seed_random") == 0) {
                            result = type_prim(checker->arena, TY_VOID);
                        }
                        /* All other math functions return float */
                        else {
                            /* abs, min, max, clamp, pow, sqrt, cbrt,
                               floor, ceil, round, trunc, sin, cos, tan,
                               atan2, log, log10, log2, exp, random_float */
                            result = type_prim(checker->arena, TY_FLOAT);
                        }
                    } else if (strcmp(stdlib_module, "sys") == 0) {
                        /* forge.sys functions */
                        if (strcmp(stdlib_func, "args") == 0) {
                            /* Returns []str - dynamic array of strings */
                            forge_type_t* elem = type_prim(checker->arena, TY_STR);
                            result = type_dyn_array(checker->arena, elem);
                        } else if (strcmp(stdlib_func, "env") == 0) {
                            /* env returns str (empty if not found) */
                            result = type_prim(checker->arena, TY_STR);
                        } else if (strcmp(stdlib_func, "platform") == 0 ||
                                   strcmp(stdlib_func, "arch") == 0) {
                            result = type_prim(checker->arena, TY_STR);
                        } else if (strcmp(stdlib_func, "exit") == 0 ||
                                   strcmp(stdlib_func, "halt") == 0) {
                            result = type_prim(checker->arena, TY_VOID);
                        } else {
                            result = type_prim(checker->arena, TY_VOID);
                        }
                    } else if (strcmp(stdlib_module, "time") == 0) {
                        /* forge.time functions */
                        if (strcmp(stdlib_func, "now") == 0 ||
                            strcmp(stdlib_func, "elapsed_ms") == 0 ||
                            strcmp(stdlib_func, "lap") == 0) {
                            /* Returns uint (i64 in our implementation) */
                            result = type_prim(checker->arena, TY_INT);
                        } else if (strcmp(stdlib_func, "timestamp") == 0) {
                            result = type_prim(checker->arena, TY_STR);
                        } else if (strcmp(stdlib_func, "sleep") == 0) {
                            result = type_prim(checker->arena, TY_VOID);
                        } else if (strcmp(stdlib_func, "start_clock") == 0) {
                            /* Returns a Clock record - for now treat as int (opaque handle) */
                            result = type_prim(checker->arena, TY_INT);
                        } else {
                            result = type_prim(checker->arena, TY_VOID);
                        }
                    } else if (strcmp(stdlib_module, "buf") == 0) {
                        /* forge.buf functions */
                        if (strcmp(stdlib_func, "create") == 0) {
                            /* Returns buffer handle (int) */
                            result = type_prim(checker->arena, TY_INT);
                        } else if (strcmp(stdlib_func, "read_byte") == 0 ||
                                   strcmp(stdlib_func, "read_int16_le") == 0 ||
                                   strcmp(stdlib_func, "read_int32_le") == 0) {
                            /* Returns optional int (using int for now, -1 or INT64_MIN for none) */
                            result = type_prim(checker->arena, TY_INT);
                        } else if (strcmp(stdlib_func, "remaining") == 0 ||
                                   strcmp(stdlib_func, "length") == 0 ||
                                   strcmp(stdlib_func, "capacity") == 0 ||
                                   strcmp(stdlib_func, "position") == 0) {
                            result = type_prim(checker->arena, TY_INT);
                        } else if (strcmp(stdlib_func, "to_str") == 0 ||
                                   strcmp(stdlib_func, "to_hex") == 0) {
                            result = type_prim(checker->arena, TY_STR);
                        } else {
                            /* write_*, free_buf, seek, rewind -> void */
                            result = type_prim(checker->arena, TY_VOID);
                        }
                    } else if (strcmp(stdlib_module, "serial") == 0) {
                        /* forge.serial functions */
                        if (strcmp(stdlib_func, "open") == 0) {
                            /* Returns port handle (int), -1 on failure */
                            result = type_prim(checker->arena, TY_INT);
                        } else if (strcmp(stdlib_func, "read_byte") == 0 ||
                                   strcmp(stdlib_func, "bytes_available") == 0 ||
                                   strcmp(stdlib_func, "is_open") == 0 ||
                                   strcmp(stdlib_func, "get_baud") == 0) {
                            result = type_prim(checker->arena, TY_INT);
                        } else if (strcmp(stdlib_func, "read_line") == 0) {
                            result = type_prim(checker->arena, TY_STR);
                        } else {
                            /* close, write_byte, write_str, set_timeout, flush -> void */
                            result = type_prim(checker->arena, TY_VOID);
                        }
                    } else if (strcmp(stdlib_module, "nmea") == 0) {
                        /* forge.nmea functions - NMEA 0183 parsing */
                        if (strcmp(stdlib_func, "validate") == 0 ||
                            strcmp(stdlib_func, "field_count") == 0) {
                            /* Returns int (1/0 for validate, count for field_count) */
                            result = type_prim(checker->arena, TY_INT);
                        } else if (strcmp(stdlib_func, "checksum") == 0 ||
                                   strcmp(stdlib_func, "sentence_type") == 0 ||
                                   strcmp(stdlib_func, "get_talker") == 0 ||
                                   strcmp(stdlib_func, "get_field") == 0) {
                            /* Returns string */
                            result = type_prim(checker->arena, TY_STR);
                        } else if (strcmp(stdlib_func, "latitude") == 0 ||
                                   strcmp(stdlib_func, "longitude") == 0) {
                            /* Returns float (decimal degrees) */
                            result = type_prim(checker->arena, TY_FLOAT);
                        } else {
                            result = type_prim(checker->arena, TY_VOID);
                        }
                    } else if (strcmp(stdlib_module, "gui") == 0) {
                        /* forge.gui functions */
                        if (strcmp(stdlib_func, "window_open") == 0 ||
                            strcmp(stdlib_func, "is_key_pressed") == 0 ||
                            strcmp(stdlib_func, "is_key_down") == 0 ||
                            strcmp(stdlib_func, "is_key_released") == 0 ||
                            strcmp(stdlib_func, "is_mouse_pressed") == 0 ||
                            strcmp(stdlib_func, "is_mouse_down") == 0 ||
                            strcmp(stdlib_func, "button") == 0 ||
                            strcmp(stdlib_func, "color_button") == 0) {
                            result = type_prim(checker->arena, TY_BOOL);
                        } else if (strcmp(stdlib_func, "get_fps") == 0 ||
                                   strcmp(stdlib_func, "mouse_x") == 0 ||
                                   strcmp(stdlib_func, "mouse_y") == 0 ||
                                   strcmp(stdlib_func, "get_key_pressed") == 0 ||
                                   strcmp(stdlib_func, "measure_text") == 0 ||
                                   strcmp(stdlib_func, "checkbox") == 0 ||
                                   strcmp(stdlib_func, "dropdown") == 0 ||
                                   strcmp(stdlib_func, "log_count") == 0) {
                            result = type_prim(checker->arena, TY_INT);
                        } else if (strcmp(stdlib_func, "get_dt") == 0 ||
                                   strcmp(stdlib_func, "slider") == 0) {
                            result = type_prim(checker->arena, TY_FLOAT);
                        } else if (strcmp(stdlib_func, "textbox") == 0) {
                            result = type_prim(checker->arena, TY_STR);
                        } else {
                            /* init_window, close_window, set_target_fps,
                               begin_draw, end_draw, clear,
                               draw_line, draw_rect, draw_rect_lines,
                               draw_circle, draw_circle_lines, draw_text,
                               label, set_style_dark, set_style_light,
                               log_create, log_add, log_clear, log_draw -> void */
                            result = type_prim(checker->arena, TY_VOID);
                        }
                    } else {
                        /* Unknown stdlib module - default to void */
                        result = type_prim(checker->arena, TY_VOID);
                    }
                } else {
                    result = type_prim(checker->arena, TY_VOID);
                }
                break;
            }

            if (!name) {
                checker_error(checker, node, "invalid call target");
                result = type_error(checker->arena);
                break;
            }

            forge_proc_sig_t* sig = checker_lookup_proc(checker, name);

            if (!sig) {
                checker_error(checker, node, "undefined procedure '%s'", name);
                result = type_error(checker->arena);
            } else {
                int arg_count = node->data.call.arg_count;
                if (arg_count != sig->param_count) {
                    checker_error(checker, node,
                        "wrong number of arguments to '%s': expected %d, got %d",
                        name, sig->param_count, arg_count);
                } else {
                    for (int i = 0; i < arg_count; i++) {
                        forge_type_t* arg_type = checker_type_of(checker, node->data.call.args[i]);
                        if (arg_type && !type_is_assignable(sig->param_types[i], arg_type)) {
                            char* expected = type_to_str(sig->param_types[i]);
                            char* got = type_to_str(arg_type);
                            checker_error(checker, node->data.call.args[i],
                                "argument %d to '%s': expected %s, got %s",
                                i + 1, name, expected, got);
                            forge_free(expected);
                            forge_free(got);
                        }
                    }
                }
                result = sig->return_type ? sig->return_type : type_prim(checker->arena, TY_VOID);
            }
            break;
        }

        /* === Field Access === */
        case NODE_FIELD_ACCESS: {
            const char* field_name = node->data.field_access.field_name;
            forge_node_t* inner = node->data.field_access.object;

            /* Check for forge.math constants (PI, E, TAU) */
            if (inner && inner->kind == NODE_FIELD_ACCESS) {
                forge_node_t* root = inner->data.field_access.object;
                const char* mod = inner->data.field_access.field_name;
                if (root && root->kind == NODE_IDENT &&
                    strcmp(root->data.name, "forge") == 0 &&
                    strcmp(mod, "math") == 0) {
                    if (strcmp(field_name, "PI") == 0 ||
                        strcmp(field_name, "E") == 0 ||
                        strcmp(field_name, "TAU") == 0) {
                        result = type_prim(checker->arena, TY_FLOAT);
                        break;
                    }
                }
            }

            forge_type_t* obj_type = checker_type_of(checker, inner);

            if (!obj_type) {
                result = type_error(checker->arena);
                break;
            }

            /* Resolve through alias if needed */
            while (obj_type->kind == TY_ALIAS) {
                obj_type = obj_type->as.alias.target;
            }

            /* .value on optional type — unwrap to inner type */
            if (obj_type->kind == TY_OPTIONAL && strcmp(field_name, "value") == 0) {
                result = obj_type->as.optional.inner;
                break;
            }

            if (obj_type->kind != TY_RECORD) {
                char* got = type_to_str(obj_type);
                checker_error(checker, node, "cannot access field '%s' on non-record type %s",
                    field_name, got);
                forge_free(got);
                result = type_error(checker->arena);
                break;
            }

            /* Find field in record */
            result = NULL;
            for (int i = 0; i < obj_type->as.record.field_count; i++) {
                if (strcmp(obj_type->as.record.field_names[i], field_name) == 0) {
                    result = obj_type->as.record.field_types[i];
                    break;
                }
            }

            if (!result) {
                checker_error(checker, node, "record '%s' has no field '%s'",
                    obj_type->as.record.name, field_name);
                result = type_error(checker->arena);
            }
            break;
        }

        /* === Index Access === */
        case NODE_INDEX: {
            forge_type_t* obj_type = checker_type_of(checker, node->data.index.object);
            forge_type_t* idx_type = checker_type_of(checker, node->data.index.index);

            if (!obj_type || !idx_type) {
                result = type_error(checker->arena);
                break;
            }

            /* Resolve through alias if needed */
            while (obj_type->kind == TY_ALIAS) {
                obj_type = obj_type->as.alias.target;
            }

            if (obj_type->kind == TY_FIXED_ARRAY) {
                if (!type_is_integer(idx_type)) {
                    char* got = type_to_str(idx_type);
                    checker_error(checker, node->data.index.index,
                        "array index must be integer, got %s", got);
                    forge_free(got);
                }
                result = obj_type->as.fixed_array.elem_type;
            } else if (obj_type->kind == TY_DYN_ARRAY) {
                if (!type_is_integer(idx_type)) {
                    char* got = type_to_str(idx_type);
                    checker_error(checker, node->data.index.index,
                        "array index must be integer, got %s", got);
                    forge_free(got);
                }
                result = obj_type->as.dyn_array.elem_type;
            } else if (obj_type->kind == TY_MAP) {
                if (!type_is_assignable(obj_type->as.map.key_type, idx_type)) {
                    char* expected = type_to_str(obj_type->as.map.key_type);
                    char* got = type_to_str(idx_type);
                    checker_error(checker, node->data.index.index,
                        "map key type mismatch: expected %s, got %s", expected, got);
                    forge_free(expected);
                    forge_free(got);
                }
                result = obj_type->as.map.val_type;
            } else if (obj_type->kind == TY_STR) {
                if (!type_is_integer(idx_type)) {
                    char* got = type_to_str(idx_type);
                    checker_error(checker, node->data.index.index,
                        "string index must be integer, got %s", got);
                    forge_free(got);
                }
                result = type_prim(checker->arena, TY_BYTE);
            } else {
                char* got = type_to_str(obj_type);
                checker_error(checker, node, "type %s is not indexable", got);
                forge_free(got);
                result = type_error(checker->arena);
            }
            break;
        }

        /* === Array Literal === */
        case NODE_ARRAY_LITERAL: {
            if (node->data.array_lit.count == 0) {
                /* Empty array - if resolved_type was set by context (e.g. var decl), use it */
                if (node->resolved_type && node->resolved_type->kind == TY_DYN_ARRAY) {
                    result = node->resolved_type;
                    break;
                }
                /* Otherwise, type cannot be inferred */
                checker_error(checker, node, "cannot infer type of empty array literal");
                result = type_error(checker->arena);
                break;
            }

            /* Infer element type from first element */
            forge_type_t* elem_type = checker_type_of(checker, node->data.array_lit.elements[0]);
            if (!elem_type) {
                result = type_error(checker->arena);
                break;
            }

            /* Check all elements have compatible types */
            for (int i = 1; i < node->data.array_lit.count; i++) {
                forge_type_t* t = checker_type_of(checker, node->data.array_lit.elements[i]);
                if (t && !type_is_assignable(elem_type, t)) {
                    char* expected = type_to_str(elem_type);
                    char* got = type_to_str(t);
                    checker_error(checker, node->data.array_lit.elements[i],
                        "array element type mismatch: expected %s, got %s", expected, got);
                    forge_free(expected);
                    forge_free(got);
                }
            }

            result = type_dyn_array(checker->arena, elem_type);
            break;
        }

        /* === Record Literal === */
        case NODE_RECORD_LITERAL: {
            const char* type_name = node->data.record_lit.type_name;
            forge_type_t* rec_type = checker_lookup_type(checker, type_name);

            if (!rec_type) {
                checker_error(checker, node, "unknown record type '%s'", type_name);
                result = type_error(checker->arena);
                break;
            }

            /* Resolve through alias if needed */
            while (rec_type->kind == TY_ALIAS) {
                rec_type = rec_type->as.alias.target;
            }

            if (rec_type->kind != TY_RECORD) {
                checker_error(checker, node, "'%s' is not a record type", type_name);
                result = type_error(checker->arena);
                break;
            }

            /* Check each field initializer */
            for (int i = 0; i < node->data.record_lit.field_count; i++) {
                const char* fname = node->data.record_lit.fields[i].name;
                forge_node_t* fval = node->data.record_lit.fields[i].value;

                /* Find field in record type */
                forge_type_t* expected_type = NULL;
                for (int j = 0; j < rec_type->as.record.field_count; j++) {
                    if (strcmp(rec_type->as.record.field_names[j], fname) == 0) {
                        expected_type = rec_type->as.record.field_types[j];
                        break;
                    }
                }

                if (!expected_type) {
                    checker_error(checker, fval, "record '%s' has no field '%s'",
                        type_name, fname);
                    continue;
                }

                forge_type_t* actual_type = checker_type_of(checker, fval);
                if (actual_type && !type_is_assignable(expected_type, actual_type)) {
                    char* exp = type_to_str(expected_type);
                    char* got = type_to_str(actual_type);
                    checker_error(checker, fval,
                        "field '%s': expected %s, got %s", fname, exp, got);
                    forge_free(exp);
                    forge_free(got);
                }
            }

            result = rec_type;
            break;
        }

        /* === Cast === */
        case NODE_CAST: {
            forge_type_t* target = resolve_type_node(checker, node->data.cast.target_type);
            forge_type_t* expr_type = checker_type_of(checker, node->data.cast.expr);

            /* TODO: Validate that cast is legal */
            (void)expr_type;  /* Suppress unused warning */
            result = target;
            break;
        }

        /* === Optional Expressions === */
        case NODE_SOME: {
            forge_type_t* inner = checker_type_of(checker, node->data.some.expr);
            if (inner) {
                result = type_optional(checker->arena, inner);
            } else {
                result = type_error(checker->arena);
            }
            break;
        }

        case NODE_OR_ELSE: {
            forge_type_t* opt_type = checker_type_of(checker, node->data.or_else.optional_expr);
            forge_type_t* fallback_type = checker_type_of(checker, node->data.or_else.fallback);

            if (!opt_type || !fallback_type) {
                result = type_error(checker->arena);
                break;
            }

            /* Optional expression must be optional type */
            if (opt_type->kind != TY_OPTIONAL) {
                char* got = type_to_str(opt_type);
                checker_error(checker, node->data.or_else.optional_expr,
                    "or_else requires optional type, got %s", got);
                forge_free(got);
                result = fallback_type;
                break;
            }

            /* Fallback must match inner type */
            forge_type_t* inner = opt_type->as.optional.inner;
            if (!type_is_assignable(inner, fallback_type)) {
                char* expected = type_to_str(inner);
                char* got = type_to_str(fallback_type);
                checker_error(checker, node->data.or_else.fallback,
                    "or_else fallback type mismatch: expected %s, got %s", expected, got);
                forge_free(expected);
                forge_free(got);
            }
            result = inner;
            break;
        }

        case NODE_IS_SOME:
        case NODE_IS_NONE: {
            forge_type_t* expr_type = checker_type_of(checker, node->data.is_check.expr);

            if (expr_type && expr_type->kind != TY_OPTIONAL) {
                char* got = type_to_str(expr_type);
                checker_error(checker, node->data.is_check.expr,
                    "'is some/none' requires optional type, got %s", got);
                forge_free(got);
            }
            result = type_prim(checker->arena, TY_BOOL);
            break;
        }

        /* === Range === */
        case NODE_RANGE: {
            forge_type_t* start_type = checker_type_of(checker, node->data.range.start);
            forge_type_t* end_type = checker_type_of(checker, node->data.range.end);

            if (start_type && !type_is_integer(start_type)) {
                char* got = type_to_str(start_type);
                checker_error(checker, node->data.range.start,
                    "range start must be integer, got %s", got);
                forge_free(got);
            }
            if (end_type && !type_is_integer(end_type)) {
                char* got = type_to_str(end_type);
                checker_error(checker, node->data.range.end,
                    "range end must be integer, got %s", got);
                forge_free(got);
            }
            /* Range type is a special type - for now, mark it as int array */
            result = type_dyn_array(checker->arena, type_prim(checker->arena, TY_INT));
            break;
        }

        default:
            result = type_error(checker->arena);
            break;
    }

    node->resolved_type = result;
    return result;
}
