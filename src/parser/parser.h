/*
 * FORGE Language Toolchain
 * parser.h - Recursive descent parser
 *
 * The parser consumes a token stream from the lexer and produces an AST.
 * It uses arena allocation for all AST nodes, making cleanup trivial.
 */

#ifndef FORGE_PARSER_H
#define FORGE_PARSER_H

#include "common.h"
#include "lexer/lexer.h"
#include "parser/ast.h"
#include "util/memory.h"

/* ───────────────────────────────────────────────────────────────────────────
 * Parser State
 * ─────────────────────────────────────────────────────────────────────────── */

struct forge_parser {
    forge_token_t*    tokens;      /* Token array from lexer */
    int               count;       /* Number of tokens */
    int               pos;         /* Current position in token array */
    forge_arena_t*    arena;       /* Arena for AST allocation */
    forge_strtable_t* strtable;    /* String intern table */
    const char*       filename;    /* Source filename for error reporting */
    int               had_error;   /* Error flag */
    int               error_count; /* Number of errors */
    int               panic_mode;  /* In panic mode (skip tokens until sync point) */
};

/* ───────────────────────────────────────────────────────────────────────────
 * Parser Helper Macros
 * ─────────────────────────────────────────────────────────────────────────── */

/* Get current token */
#define CURRENT(p)    ((p)->tokens[(p)->pos])

/* Peek n tokens ahead (0 = current) */
#define PEEK(p, n)    ((p)->tokens[(p)->pos + (n)])

/* Advance and return the previous token */
#define ADVANCE(p)    ((p)->tokens[(p)->pos++])

/* Get the previously consumed token */
#define PREVIOUS(p)   ((p)->tokens[(p)->pos - 1])

/* Check if current token is of type t */
#define CHECK(p, t)   (CURRENT(p).type == (t))

/* Check if at end of file */
#define AT_END(p)     (CHECK(p, TOK_EOF))

/* ───────────────────────────────────────────────────────────────────────────
 * Public API
 * ─────────────────────────────────────────────────────────────────────────── */

/*
 * Create a new parser.
 * The token array should come from a lexer and remain valid for the parser's lifetime.
 * The arena and strtable must also remain valid.
 */
forge_parser_t* parser_create(forge_token_t* tokens, int count,
                               forge_arena_t* arena,
                               forge_strtable_t* strtable,
                               const char* filename);

/*
 * Parse the entire program.
 * Returns the root NODE_PROGRAM node, or NULL on fatal error.
 * Non-fatal errors are reported but parsing continues.
 */
forge_node_t* parser_parse(forge_parser_t* p);

/*
 * Destroy the parser.
 * Does NOT free the arena, strtable, or tokens (caller owns them).
 */
void parser_destroy(forge_parser_t* p);

/*
 * Check if parser encountered any errors.
 */
int parser_had_error(forge_parser_t* p);

/* ───────────────────────────────────────────────────────────────────────────
 * Internal Parsing Functions (exposed for testing)
 * ─────────────────────────────────────────────────────────────────────────── */

/* Top-level */
forge_node_t* parse_program(forge_parser_t* p);
forge_node_t* parse_import(forge_parser_t* p);
forge_node_t* parse_top_level_decl(forge_parser_t* p, int exported);

/* Declarations */
forge_node_t* parse_proc_decl(forge_parser_t* p, int exported);
forge_node_t* parse_record_decl(forge_parser_t* p, int exported);
forge_node_t* parse_channel_decl(forge_parser_t* p, int exported);
forge_node_t* parse_var_decl(forge_parser_t* p, int exported);
forge_node_t* parse_const_decl(forge_parser_t* p, int exported);
forge_node_t* parse_type_alias(forge_parser_t* p, int exported);
forge_node_t* parse_on_handler(forge_parser_t* p);

/* Statements */
forge_node_t* parse_block(forge_parser_t* p);
forge_node_t* parse_stmt(forge_parser_t* p);
forge_node_t* parse_if_stmt(forge_parser_t* p);
forge_node_t* parse_while_stmt(forge_parser_t* p);
forge_node_t* parse_for_stmt(forge_parser_t* p);
forge_node_t* parse_loop_stmt(forge_parser_t* p);
forge_node_t* parse_emit_stmt(forge_parser_t* p);
forge_node_t* parse_assign_stmt(forge_parser_t* p, forge_node_t* target, int line, int col);

/* Expressions */
forge_node_t* parse_expr(forge_parser_t* p);
forge_node_t* parse_type_expr(forge_parser_t* p);

#endif /* FORGE_PARSER_H */

