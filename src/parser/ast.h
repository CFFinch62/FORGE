/*
 * FORGE Language Toolchain
 * ast.h - Abstract Syntax Tree node definitions
 *
 * All AST nodes are allocated from an arena. The node structure uses a
 * tagged union to represent all possible node kinds. Each node carries
 * line/column info for error reporting and a resolved_type slot for
 * the type checker.
 */

#ifndef FORGE_AST_H
#define FORGE_AST_H

#include "common.h"
#include "util/memory.h"

/* ─────────────────────────────────────────────────────────────────────────────
 * Node Kinds
 * ───────────────────────────────────────────────────────────────────────────── */

typedef enum {
    /* Top-level */
    NODE_PROGRAM,
    NODE_IMPORT,

    /* Declarations */
    NODE_PROC_DECL,
    NODE_RECORD_DECL,
    NODE_CHANNEL_DECL,
    NODE_ON_HANDLER,
    NODE_VAR_DECL,
    NODE_CONST_DECL,
    NODE_TYPE_ALIAS,
    NODE_FIELD_DEF,        /* Field within record */
    NODE_PARAM,            /* Parameter in proc */

    /* Statements */
    NODE_BLOCK,
    NODE_ASSIGN,
    NODE_COMPOUND_ASSIGN,
    NODE_IF,
    NODE_WHILE,
    NODE_FOR,
    NODE_LOOP,
    NODE_RETURN,
    NODE_BREAK,
    NODE_CONTINUE,
    NODE_EMIT,
    NODE_WITH_ALLOC,
    NODE_FREE,
    NODE_PANIC,
    NODE_ASSERT,
    NODE_EXPR_STMT,

    /* Expressions */
    NODE_INT_LIT,
    NODE_FLOAT_LIT,
    NODE_STR_LIT,
    NODE_BOOL_LIT,
    NODE_NONE_LIT,
    NODE_IDENT,
    NODE_QUALIFIED_IDENT,
    NODE_BINARY_OP,
    NODE_UNARY_OP,
    NODE_CALL,
    NODE_FIELD_ACCESS,
    NODE_INDEX,
    NODE_RECORD_LITERAL,
    NODE_ARRAY_LITERAL,
    NODE_CAST,
    NODE_SOME,             /* some(expr) */
    NODE_OR_ELSE,          /* expr or_else fallback */
    NODE_IS_SOME,          /* x is some */
    NODE_IS_NONE,          /* x is none */
    NODE_RANGE,            /* start..end or start..=end */

    /* Type expressions (used in type annotations) */
    NODE_TYPE_PRIM,        /* int, float, str, etc. */
    NODE_TYPE_OPTIONAL,    /* ?T */
    NODE_TYPE_FIXED_ARRAY, /* [T; N] */
    NODE_TYPE_DYN_ARRAY,   /* []T */
    NODE_TYPE_MAP,         /* map[K, V] */
    NODE_TYPE_NAMED,       /* user-defined type name */
    NODE_TYPE_REF,         /* ref T */
} forge_node_kind_t;

/* ─────────────────────────────────────────────────────────────────────────────
 * Parameter structure (used in proc declarations)
 * ───────────────────────────────────────────────────────────────────────────── */

typedef struct {
    const char*   name;
    forge_node_t* type_expr;   /* Type annotation */
    int           is_ref;      /* ref parameter? */
} forge_param_t;

/* ─────────────────────────────────────────────────────────────────────────────
 * Field initializer (used in record literals)
 * ───────────────────────────────────────────────────────────────────────────── */

typedef struct {
    const char*   name;
    forge_node_t* value;
} forge_field_init_t;

/* ─────────────────────────────────────────────────────────────────────────────
 * AST Node Structure (forward declared in common.h)
 * ───────────────────────────────────────────────────────────────────────────── */

struct forge_node {
    forge_node_kind_t  kind;
    int                line;
    int                column;
    forge_type_t*      resolved_type;  /* Filled by type checker, NULL initially */

    /* Tagged union for node-specific data */
    union {
        /* === Literals === */

        /* NODE_INT_LIT */
        long long int_val;

        /* NODE_FLOAT_LIT */
        double float_val;

        /* NODE_STR_LIT */
        const char* str_val;

        /* NODE_BOOL_LIT */
        int bool_val;

        /* NODE_IDENT, NODE_TYPE_NAMED, NODE_TYPE_PRIM */
        const char* name;

        /* === Operators === */

        /* NODE_BINARY_OP */
        struct {
            int            op;    /* Token type: TOK_PLUS, TOK_EQ, etc. */
            forge_node_t*  left;
            forge_node_t*  right;
        } binop;

        /* NODE_UNARY_OP */
        struct {
            int            op;    /* Token type: TOK_MINUS, TOK_NOT, etc. */
            forge_node_t*  operand;
        } unop;

        /* NODE_COMPOUND_ASSIGN (e.g., x += 1) */
        struct {
            int            op;    /* Compound op: TOK_PLUS_EQ, etc. */
            forge_node_t*  target;
            forge_node_t*  value;
        } compound_assign;

        /* === Calls and Access === */

        /* NODE_CALL */
        struct {
            forge_node_t*  callee;
            forge_node_t** args;
            int            arg_count;
        } call;

        /* NODE_FIELD_ACCESS (e.g., record.field) */
        struct {
            forge_node_t*  object;
            const char*    field_name;
        } field_access;

        /* NODE_INDEX (e.g., arr[i]) */
        struct {
            forge_node_t*  object;
            forge_node_t*  index;
        } index;

        /* NODE_QUALIFIED_IDENT (e.g., module.name) */
        struct {
            const char*    module_name;
            const char*    symbol_name;
        } qualified;

        /* === Literals (compound) === */

        /* NODE_RECORD_LITERAL */
        struct {
            const char*        type_name;
            forge_field_init_t* fields;
            int                field_count;
        } record_lit;

        /* NODE_ARRAY_LITERAL */
        struct {
            forge_node_t** elements;
            int            count;
        } array_lit;

        /* === Declarations === */

        /* NODE_PROGRAM */
        struct {
            forge_node_t** imports;
            int            import_count;
            forge_node_t** decls;
            int            decl_count;
        } program;

        /* NODE_IMPORT */
        struct {
            const char*    module_path;  /* e.g., "forge.io" */
            const char*    alias;        /* optional alias, may be NULL */
        } import;

        /* NODE_PROC_DECL */
        struct {
            const char*    name;
            forge_param_t* params;
            int            param_count;
            forge_node_t*  return_type;  /* Type expression, NULL for void */
            forge_node_t*  body;         /* NODE_BLOCK */
            int            exported;
        } proc;

        /* NODE_RECORD_DECL */
        struct {
            const char*    name;
            forge_node_t** fields;       /* Array of NODE_FIELD_DEF */
            int            field_count;
            int            exported;
        } record;

        /* NODE_FIELD_DEF (within record) */
        struct {
            const char*    name;
            forge_node_t*  type_expr;
        } field_def;

        /* NODE_CHANNEL_DECL */
        struct {
            const char*    name;
            forge_node_t*  payload_type; /* NULL for void channels */
            int            exported;
        } channel;

        /* NODE_ON_HANDLER */
        struct {
            const char*    channel_name;
            const char*    param_name;   /* Payload parameter name, may be NULL */
            forge_node_t*  body;         /* NODE_BLOCK */
        } on_handler;

        /* NODE_VAR_DECL, NODE_CONST_DECL */
        struct {
            const char*    name;
            forge_node_t*  type_expr;    /* May be NULL (type inference) */
            forge_node_t*  init_expr;    /* May be NULL for uninitialized vars */
            int            exported;
        } var_decl;

        /* NODE_TYPE_ALIAS */
        struct {
            const char*    name;
            forge_node_t*  type_expr;
            int            exported;
        } type_alias;

        /* === Statements === */

        /* NODE_BLOCK */
        struct {
            forge_node_t** stmts;
            int            count;
        } block;

        /* NODE_ASSIGN */
        struct {
            forge_node_t*  target;
            forge_node_t*  value;
        } assign;

        /* NODE_IF */
        struct {
            forge_node_t*  condition;
            forge_node_t*  then_body;      /* NODE_BLOCK */
            forge_node_t** elif_conditions;
            forge_node_t** elif_bodies;
            int            elif_count;
            forge_node_t*  else_body;      /* May be NULL */
        } if_stmt;

        /* NODE_WHILE */
        struct {
            forge_node_t*  condition;
            forge_node_t*  body;           /* NODE_BLOCK */
        } while_stmt;

        /* NODE_FOR */
        struct {
            const char*    var_name;
            forge_node_t*  iterable;       /* Range or array expression */
            forge_node_t*  body;           /* NODE_BLOCK */
        } for_stmt;

        /* NODE_LOOP (infinite loop) */
        struct {
            forge_node_t*  body;           /* NODE_BLOCK */
        } loop_stmt;

        /* NODE_RETURN */
        struct {
            forge_node_t*  value;          /* May be NULL for void return */
        } return_stmt;

        /* NODE_EMIT */
        struct {
            const char*    channel_name;
            forge_node_t*  payload;        /* May be NULL for void channels */
        } emit_stmt;

        /* NODE_WITH_ALLOC */
        struct {
            const char*    var_name;
            forge_node_t*  type_expr;
            forge_node_t*  size_expr;      /* count expression (may be NULL) */
            forge_node_t*  body;           /* NODE_BLOCK */
        } with_alloc;

        /* NODE_FREE */
        struct {
            forge_node_t*  expr;
        } free_stmt;

        /* NODE_PANIC, NODE_ASSERT */
        struct {
            forge_node_t*  expr;           /* Message or condition */
        } panic_assert;

        /* NODE_EXPR_STMT */
        struct {
            forge_node_t*  expr;
        } expr_stmt;

        /* === Type Expressions === */

        /* NODE_TYPE_OPTIONAL (?T) */
        struct {
            forge_node_t*  inner_type;
        } type_optional;

        /* NODE_TYPE_FIXED_ARRAY ([T; N]) */
        struct {
            forge_node_t*  elem_type;
            int            size;
        } type_fixed_array;

        /* NODE_TYPE_DYN_ARRAY ([]T) */
        struct {
            forge_node_t*  elem_type;
        } type_dyn_array;

        /* NODE_TYPE_MAP (map[K, V]) */
        struct {
            forge_node_t*  key_type;
            forge_node_t*  val_type;
        } type_map;

        /* NODE_TYPE_REF (ref T) */
        struct {
            forge_node_t*  inner_type;
        } type_ref;

        /* === Misc Expressions === */

        /* NODE_CAST */
        struct {
            forge_node_t*  target_type;    /* Type expression */
            forge_node_t*  expr;
        } cast;

        /* NODE_SOME */
        struct {
            forge_node_t*  expr;
        } some;

        /* NODE_OR_ELSE */
        struct {
            forge_node_t*  optional_expr;
            forge_node_t*  fallback;
        } or_else;

        /* NODE_IS_SOME, NODE_IS_NONE */
        struct {
            forge_node_t*  expr;
        } is_check;

        /* NODE_RANGE */
        struct {
            forge_node_t*  start;
            forge_node_t*  end;
            int            inclusive;      /* 1 for ..=, 0 for .. */
        } range;

    } data;  /* Named union for C99 compatibility */
};

/* ─────────────────────────────────────────────────────────────────────────────
 * AST Node Constructors
 * ───────────────────────────────────────────────────────────────────────────── */

/* Literals */
forge_node_t* ast_int_lit(forge_arena_t* a, long long val, int line, int col);
forge_node_t* ast_float_lit(forge_arena_t* a, double val, int line, int col);
forge_node_t* ast_str_lit(forge_arena_t* a, const char* val, int line, int col);
forge_node_t* ast_bool_lit(forge_arena_t* a, int val, int line, int col);
forge_node_t* ast_none_lit(forge_arena_t* a, int line, int col);

/* Identifiers */
forge_node_t* ast_ident(forge_arena_t* a, const char* name, int line, int col);
forge_node_t* ast_qualified_ident(forge_arena_t* a, const char* module,
                                   const char* symbol, int line, int col);

/* Operators */
forge_node_t* ast_binary_op(forge_arena_t* a, int op,
                             forge_node_t* left, forge_node_t* right,
                             int line, int col);
forge_node_t* ast_unary_op(forge_arena_t* a, int op,
                            forge_node_t* operand, int line, int col);

/* Expressions */
forge_node_t* ast_call(forge_arena_t* a, forge_node_t* callee,
                        forge_node_t** args, int arg_count, int line, int col);
forge_node_t* ast_field_access(forge_arena_t* a, forge_node_t* object,
                                const char* field, int line, int col);
forge_node_t* ast_index(forge_arena_t* a, forge_node_t* object,
                         forge_node_t* index, int line, int col);
forge_node_t* ast_cast(forge_arena_t* a, forge_node_t* target_type,
                        forge_node_t* expr, int line, int col);
forge_node_t* ast_range(forge_arena_t* a, forge_node_t* start,
                         forge_node_t* end, int inclusive, int line, int col);
forge_node_t* ast_array_lit(forge_arena_t* a, forge_node_t** elements, int count,
                             int line, int col);
forge_node_t* ast_record_lit(forge_arena_t* a, const char* type_name,
                              forge_field_init_t* fields, int field_count,
                              int line, int col);
forge_node_t* ast_some(forge_arena_t* a, forge_node_t* expr, int line, int col);
forge_node_t* ast_or_else(forge_arena_t* a, forge_node_t* optional_expr,
                           forge_node_t* fallback, int line, int col);
forge_node_t* ast_is_some(forge_arena_t* a, forge_node_t* expr, int line, int col);
forge_node_t* ast_is_none(forge_arena_t* a, forge_node_t* expr, int line, int col);

/* Statements */
forge_node_t* ast_block(forge_arena_t* a, forge_node_t** stmts, int count,
                         int line, int col);
forge_node_t* ast_assign(forge_arena_t* a, forge_node_t* target,
                          forge_node_t* value, int line, int col);
forge_node_t* ast_compound_assign(forge_arena_t* a, int op, forge_node_t* target,
                                   forge_node_t* value, int line, int col);
forge_node_t* ast_if(forge_arena_t* a, forge_node_t* cond, forge_node_t* then_body,
                      forge_node_t** elif_conds, forge_node_t** elif_bodies, int elif_count,
                      forge_node_t* else_body, int line, int col);
forge_node_t* ast_while(forge_arena_t* a, forge_node_t* cond, forge_node_t* body,
                         int line, int col);
forge_node_t* ast_for(forge_arena_t* a, const char* var, forge_node_t* iterable,
                       forge_node_t* body, int line, int col);
forge_node_t* ast_loop(forge_arena_t* a, forge_node_t* body, int line, int col);
forge_node_t* ast_return(forge_arena_t* a, forge_node_t* value, int line, int col);
forge_node_t* ast_break(forge_arena_t* a, int line, int col);
forge_node_t* ast_continue(forge_arena_t* a, int line, int col);
forge_node_t* ast_emit(forge_arena_t* a, const char* channel_name, forge_node_t* payload,
                        int line, int col);
forge_node_t* ast_free_stmt(forge_arena_t* a, forge_node_t* expr, int line, int col);
forge_node_t* ast_with_alloc(forge_arena_t* a, const char* var_name,
                              forge_node_t* type_expr, forge_node_t* size_expr,
                              forge_node_t* body, int line, int col);
forge_node_t* ast_expr_stmt(forge_arena_t* a, forge_node_t* expr, int line, int col);

/* Declarations */
forge_node_t* ast_var_decl(forge_arena_t* a, const char* name, forge_node_t* type_expr,
                            forge_node_t* init, int exported, int line, int col);
forge_node_t* ast_const_decl(forge_arena_t* a, const char* name, forge_node_t* type_expr,
                              forge_node_t* init, int exported, int line, int col);
forge_node_t* ast_proc_decl(forge_arena_t* a, const char* name,
                             forge_param_t* params, int param_count,
                             forge_node_t* return_type, forge_node_t* body,
                             int exported, int line, int col);
forge_node_t* ast_program(forge_arena_t* a, forge_node_t** imports, int import_count,
                           forge_node_t** decls, int decl_count);
forge_node_t* ast_import(forge_arena_t* a, const char* module_path, const char* alias,
                          int line, int col);
forge_node_t* ast_record_decl(forge_arena_t* a, const char* name,
                               forge_node_t** fields, int field_count,
                               int exported, int line, int col);
forge_node_t* ast_field_def(forge_arena_t* a, const char* name, forge_node_t* type_expr,
                             int line, int col);
forge_node_t* ast_channel_decl(forge_arena_t* a, const char* name, forge_node_t* payload_type,
                                int exported, int line, int col);
forge_node_t* ast_on_handler(forge_arena_t* a, const char* channel_name, const char* param_name,
                              forge_node_t* body, int line, int col);
forge_node_t* ast_type_alias(forge_arena_t* a, const char* name, forge_node_t* type_expr,
                              int exported, int line, int col);

/* Type expressions */
forge_node_t* ast_type_prim(forge_arena_t* a, const char* name, int line, int col);
forge_node_t* ast_type_named(forge_arena_t* a, const char* name, int line, int col);
forge_node_t* ast_type_optional(forge_arena_t* a, forge_node_t* inner, int line, int col);
forge_node_t* ast_type_fixed_array(forge_arena_t* a, forge_node_t* elem, int size,
                                    int line, int col);
forge_node_t* ast_type_dyn_array(forge_arena_t* a, forge_node_t* elem, int line, int col);
forge_node_t* ast_type_map(forge_arena_t* a, forge_node_t* key, forge_node_t* val,
                            int line, int col);
forge_node_t* ast_type_ref(forge_arena_t* a, forge_node_t* inner, int line, int col);

/* ─────────────────────────────────────────────────────────────────────────────
 * AST Utilities
 * ───────────────────────────────────────────────────────────────────────────── */

/* Get human-readable name for node kind */
const char* ast_node_kind_name(forge_node_kind_t kind);

/* Get human-readable name for operator (token type) */
const char* ast_op_name(int op);

/* Pretty-print AST (for debugging) */
void ast_print(forge_node_t* node, int depth);

#endif /* FORGE_AST_H */

