/*
 * FORGE Language Toolchain
 * parser.c - Recursive descent parser implementation
 *
 * The parser consumes tokens from the lexer and produces an AST.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "parser/parser.h"
#include "parser/ast.h"
#include "util/memory.h"
#include "util/dynarray.h"

/* ─────────────────────────────────────────────────────────────────────────────
 * Dynamic arrays for collecting AST nodes during parsing
 * ───────────────────────────────────────────────────────────────────────────── */

/* Typedef for pointer type to work with DYNARRAY_DEFINE macro */
typedef forge_node_t* forge_node_ptr;
DYNARRAY_DEFINE(node_array, forge_node_ptr)
DYNARRAY_DEFINE(param_array, forge_param_t)

/* ─────────────────────────────────────────────────────────────────────────────
 * Error Handling
 * ───────────────────────────────────────────────────────────────────────────── */

static void parser_error(forge_parser_t* p, const char* fmt, ...) {
    forge_token_t tok = CURRENT(p);

    fprintf(stderr, "%s:%d:%d: error: ", p->filename, tok.line, tok.column);

    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);

    fprintf(stderr, "\n");

    p->had_error = 1;
    p->error_count++;
    p->panic_mode = 1;
}

/* Synchronize after an error by skipping to a synchronization point */
static void synchronize(forge_parser_t* p) {
    p->panic_mode = 0;

    while (!AT_END(p)) {
        /* Synchronize at newlines followed by statement/declaration starters */
        if (CURRENT(p).type == TOK_NEWLINE) {
            (void)ADVANCE(p);
            /* Check for declaration or statement starters */
            forge_token_type_t next = CURRENT(p).type;
            if (next == TOK_PROC || next == TOK_RECORD || next == TOK_CHANNEL ||
                next == TOK_VAR || next == TOK_CONST || next == TOK_EXPORT ||
                next == TOK_TYPE || next == TOK_ON || next == TOK_IMPORT ||
                next == TOK_IF || next == TOK_WHILE || next == TOK_FOR ||
                next == TOK_LOOP || next == TOK_RETURN) {
                return;
            }
        } else {
            (void)ADVANCE(p);
        }
    }
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Token Utilities
 * ───────────────────────────────────────────────────────────────────────────── */

/* Match and consume if token matches, return 1 if matched */
static int match(forge_parser_t* p, forge_token_type_t type) {
    if (CHECK(p, type)) {
        (void)ADVANCE(p);
        return 1;
    }
    return 0;
}

/* Expect a specific token type, error if not found */
static forge_token_t expect(forge_parser_t* p, forge_token_type_t type, const char* msg) {
    if (CHECK(p, type)) {
        return ADVANCE(p);
    }
    parser_error(p, "%s", msg);
    /* Return current token anyway to allow parsing to continue */
    return CURRENT(p);
}

/* Skip newlines (used when they're optional between constructs) */
static void skip_newlines(forge_parser_t* p) {
    while (match(p, TOK_NEWLINE)) {
        /* skip */
    }
}

/* Consume a required newline */
static void consume_newline(forge_parser_t* p) {
    if (!match(p, TOK_NEWLINE) && !CHECK(p, TOK_EOF)) {
        parser_error(p, "expected newline");
    }
}

/* Note: token_type_name() is declared in lexer.h */

/* ─────────────────────────────────────────────────────────────────────────────
 * Forward Declarations
 * ───────────────────────────────────────────────────────────────────────────── */

static forge_node_t* parse_top_decl(forge_parser_t* p, int exported);
static forge_node_t* parse_type(forge_parser_t* p);

/* ─────────────────────────────────────────────────────────────────────────────
 * Public API
 * ───────────────────────────────────────────────────────────────────────────── */

forge_parser_t* parser_create(forge_token_t* tokens, int count,
                               forge_arena_t* arena,
                               forge_strtable_t* strtable,
                               const char* filename) {
    forge_parser_t* p = malloc(sizeof(forge_parser_t));
    if (!p) return NULL;

    p->tokens = tokens;
    p->count = count;
    p->pos = 0;
    p->arena = arena;
    p->strtable = strtable;
    p->filename = filename;
    p->had_error = 0;
    p->error_count = 0;
    p->panic_mode = 0;

    return p;
}

void parser_destroy(forge_parser_t* p) {
    if (p) {
        free(p);
    }
}

int parser_had_error(forge_parser_t* p) {
    return p->had_error;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Import Parsing
 * ───────────────────────────────────────────────────────────────────────────── */

/*
 * Check if a token can be used as a module path component.
 * Some keywords like 'str', 'int', etc. can be module names (e.g., forge.str).
 */
static int is_module_ident(forge_token_type_t t) {
    return t == TOK_IDENT ||
           t == TOK_STR_KW ||    /* forge.str */
           t == TOK_INT_KW ||    /* forge.int (potential future use) */
           t == TOK_MAP;         /* forge.map (potential future use) */
}

/* Get the identifier string for a module path token */
static const char* module_ident_name(forge_parser_t* p, forge_token_t tok) {
    switch (tok.type) {
        case TOK_STR_KW: return strtable_intern_cstr(p->strtable, "str");
        case TOK_INT_KW: return strtable_intern_cstr(p->strtable, "int");
        case TOK_MAP:    return strtable_intern_cstr(p->strtable, "map");
        case TOK_IDENT:  return tok.val.str_val;
        default:         return NULL;
    }
}

/*
 * import_decl ::= 'import' qualified_name ('as' IDENT)? NEWLINE
 * qualified_name ::= IDENT ('.' IDENT)*
 */
forge_node_t* parse_import(forge_parser_t* p) {
    int line = CURRENT(p).line;
    int col = CURRENT(p).column;

    expect(p, TOK_IMPORT, "expected 'import'");

    /* Parse qualified name (e.g., forge.io, forge.str) */
    forge_token_t first = expect(p, TOK_IDENT, "expected module name");

    /* Build the module path string */
    /* Start with first identifier */
    char path_buf[256];
    int path_len = 0;

    if (first.val.str_val) {
        size_t len = strlen(first.val.str_val);
        if (len < sizeof(path_buf)) {
            strcpy(path_buf, first.val.str_val);
            path_len = (int)len;
        }
    }

    /* Continue with .ident sequences (allow keywords like 'str' as module names) */
    while (match(p, TOK_DOT)) {
        if (!is_module_ident(CURRENT(p).type)) {
            parser_error(p, "expected identifier after '.'");
            break;
        }
        forge_token_t next = ADVANCE(p);
        const char* name = module_ident_name(p, next);
        if (name && path_len + 1 + (int)strlen(name) < (int)sizeof(path_buf)) {
            path_buf[path_len++] = '.';
            strcpy(path_buf + path_len, name);
            path_len += (int)strlen(name);
        }
    }
    path_buf[path_len] = '\0';

    /* Intern the path string */
    const char* module_path = strtable_intern_cstr(p->strtable, path_buf);

    /* Optional alias */
    const char* alias = NULL;
    if (match(p, TOK_AS)) {
        forge_token_t alias_tok = expect(p, TOK_IDENT, "expected alias name after 'as'");
        alias = alias_tok.val.str_val;
    }

    consume_newline(p);

    return ast_import(p->arena, module_path, alias, line, col);
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Type Parsing
 * ───────────────────────────────────────────────────────────────────────────── */

/* Check if token is a primitive type keyword */
static int is_primitive_type(forge_token_type_t type) {
    return type == TOK_INT_KW || type == TOK_INT8_KW || type == TOK_INT16_KW ||
           type == TOK_INT32_KW || type == TOK_UINT_KW || type == TOK_UINT8_KW ||
           type == TOK_UINT16_KW || type == TOK_UINT32_KW || type == TOK_FLOAT_KW ||
           type == TOK_FLOAT32_KW || type == TOK_STR_KW || type == TOK_BOOL ||
           type == TOK_BYTE || type == TOK_VOID;
}

/* Get the string name for a primitive type token */
static const char* primitive_type_name(forge_parser_t* p, forge_token_type_t type) {
    switch (type) {
        case TOK_INT_KW:     return strtable_intern_cstr(p->strtable, "int");
        case TOK_INT8_KW:    return strtable_intern_cstr(p->strtable, "int8");
        case TOK_INT16_KW:   return strtable_intern_cstr(p->strtable, "int16");
        case TOK_INT32_KW:   return strtable_intern_cstr(p->strtable, "int32");
        case TOK_UINT_KW:    return strtable_intern_cstr(p->strtable, "uint");
        case TOK_UINT8_KW:   return strtable_intern_cstr(p->strtable, "uint8");
        case TOK_UINT16_KW:  return strtable_intern_cstr(p->strtable, "uint16");
        case TOK_UINT32_KW:  return strtable_intern_cstr(p->strtable, "uint32");
        case TOK_FLOAT_KW:   return strtable_intern_cstr(p->strtable, "float");
        case TOK_FLOAT32_KW: return strtable_intern_cstr(p->strtable, "float32");
        case TOK_STR_KW:     return strtable_intern_cstr(p->strtable, "str");
        case TOK_BOOL:       return strtable_intern_cstr(p->strtable, "bool");
        case TOK_BYTE:       return strtable_intern_cstr(p->strtable, "byte");
        case TOK_VOID:       return strtable_intern_cstr(p->strtable, "void");
        default:             break;
    }
    return strtable_intern_cstr(p->strtable, "unknown");
}


/*
 * type ::= primitive_type
 *        | IDENT                    (named type)
 *        | '?' type                 (optional)
 *        | '[' ']' type             (dynamic array)
 *        | '[' type ';' INT ']'     (fixed array)
 *        | 'map' '[' type ',' type ']'  (map)
 *        | 'ref' type               (reference)
 */
static forge_node_t* parse_type(forge_parser_t* p) {
    int line = CURRENT(p).line;
    int col = CURRENT(p).column;

    /* Optional type: ?T */
    if (match(p, TOK_QUESTION)) {
        forge_node_t* inner = parse_type(p);
        return ast_type_optional(p->arena, inner, line, col);
    }

    /* Array types: []T or [T; N] */
    if (match(p, TOK_LBRACKET)) {
        if (match(p, TOK_RBRACKET)) {
            /* Dynamic array: []T */
            forge_node_t* elem = parse_type(p);
            return ast_type_dyn_array(p->arena, elem, line, col);
        } else {
            /* Fixed array: [T; N] */
            forge_node_t* elem = parse_type(p);
            expect(p, TOK_COMMA, "expected ',' in fixed array type");
            forge_token_t size_tok = expect(p, TOK_INT_LIT, "expected array size");
            expect(p, TOK_RBRACKET, "expected ']'");
            int size = (int)size_tok.val.int_val;
            return ast_type_fixed_array(p->arena, elem, size, line, col);
        }
    }

    /* Map type: map[K, V] */
    if (match(p, TOK_MAP)) {
        expect(p, TOK_LBRACKET, "expected '[' after 'map'");
        forge_node_t* key = parse_type(p);
        expect(p, TOK_COMMA, "expected ',' in map type");
        forge_node_t* val = parse_type(p);
        expect(p, TOK_RBRACKET, "expected ']'");
        return ast_type_map(p->arena, key, val, line, col);
    }

    /* Reference type: ref T */
    if (match(p, TOK_REF)) {
        forge_node_t* inner = parse_type(p);
        return ast_type_ref(p->arena, inner, line, col);
    }

    /* Primitive types */
    if (is_primitive_type(CURRENT(p).type)) {
        forge_token_t tok = ADVANCE(p);
        const char* name = primitive_type_name(p, tok.type);
        return ast_type_prim(p->arena, name, line, col);
    }

    /* Named type (user-defined) */
    if (CHECK(p, TOK_IDENT)) {
        forge_token_t tok = ADVANCE(p);
        return ast_type_named(p->arena, tok.val.str_val, line, col);
    }

    parser_error(p, "expected type");
    return NULL;
}

/* Expose parse_type_expr as the public wrapper */
forge_node_t* parse_type_expr(forge_parser_t* p) {
    return parse_type(p);
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Top-Level Declaration Parsing
 * ───────────────────────────────────────────────────────────────────────────── */

/*
 * top_level_decl ::= ('export')? ( proc_decl
 *                                | record_decl
 *                                | channel_decl
 *                                | const_decl
 *                                | var_decl
 *                                | type_alias
 *                                | on_handler )
 */
static forge_node_t* parse_top_decl(forge_parser_t* p, int exported) {
    int line = CURRENT(p).line;
    int col = CURRENT(p).column;
    (void)line; (void)col;  /* Will be used by declaration parsers */

    switch (CURRENT(p).type) {
        case TOK_PROC:
            return parse_proc_decl(p, exported);
        case TOK_RECORD:
            return parse_record_decl(p, exported);
        case TOK_CHANNEL:
            return parse_channel_decl(p, exported);
        case TOK_CONST:
            return parse_const_decl(p, exported);
        case TOK_VAR:
            return parse_var_decl(p, exported);
        case TOK_TYPE:
            return parse_type_alias(p, exported);
        case TOK_ON:
            if (exported) {
                parser_error(p, "'on' handlers cannot be exported");
            }
            return parse_on_handler(p);
        default:
            parser_error(p, "expected declaration");
            return NULL;
    }
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Program Parsing
 * ───────────────────────────────────────────────────────────────────────────── */

/*
 * program ::= ( import_decl | top_level_decl )* EOF
 */
forge_node_t* parse_program(forge_parser_t* p) {
    /* Collect imports and declarations separately */
    node_array imports;
    node_array decls;
    node_array_init(&imports);
    node_array_init(&decls);

    /* Skip leading newlines */
    skip_newlines(p);

    while (!AT_END(p)) {
        /* Skip blank lines */
        if (match(p, TOK_NEWLINE)) {
            continue;
        }

        /* Handle imports */
        if (CHECK(p, TOK_IMPORT)) {
            forge_node_t* imp = parse_import(p);
            if (imp) {
                node_array_push(&imports, imp);
            }
            continue;
        }

        /* Handle export keyword */
        int exported = 0;
        if (match(p, TOK_EXPORT)) {
            exported = 1;
        }

        /* Parse top-level declaration */
        forge_node_t* decl = parse_top_decl(p, exported);
        if (decl) {
            node_array_push(&decls, decl);
        }

        /* Recovery from errors */
        if (p->panic_mode) {
            synchronize(p);
        }
    }

    /* Copy arrays to arena */
    forge_node_t** arena_imports = NULL;
    forge_node_t** arena_decls = NULL;

    if (imports.len > 0) {
        arena_imports = ARENA_ALLOC_ARRAY(p->arena, forge_node_t*, imports.len);
        for (int i = 0; i < imports.len; i++) {
            arena_imports[i] = imports.data[i];
        }
    }

    if (decls.len > 0) {
        arena_decls = ARENA_ALLOC_ARRAY(p->arena, forge_node_t*, decls.len);
        for (int i = 0; i < decls.len; i++) {
            arena_decls[i] = decls.data[i];
        }
    }

    forge_node_t* program = ast_program(p->arena, arena_imports, imports.len,
                                         arena_decls, decls.len);

    /* Free temporary arrays */
    node_array_free(&imports);
    node_array_free(&decls);

    return program;
}

/* Main parsing entry point */
forge_node_t* parser_parse(forge_parser_t* p) {
    return parse_program(p);
}



/* ─────────────────────────────────────────────────────────────────────────────
 * Procedure Declarations (Task 2.4)
 * ───────────────────────────────────────────────────────────────────────────── */

/*
 * param ::= ( 'ref' )? IDENT ':' type
 */
static forge_param_t parse_param(forge_parser_t* p) {
    forge_param_t param = { NULL, NULL, 0 };

    /* Optional 'ref' modifier */
    if (match(p, TOK_REF)) {
        param.is_ref = 1;
    }

    /* Parameter name */
    forge_token_t name_tok = expect(p, TOK_IDENT, "expected parameter name");
    param.name = name_tok.val.str_val;

    /* Colon and type */
    expect(p, TOK_COLON, "expected ':' after parameter name");
    param.type_expr = parse_type(p);

    return param;
}

/*
 * param_list ::= param ( ',' param )*
 * Returns allocated array of forge_param_t
 */
static forge_param_t* parse_param_list(forge_parser_t* p, int* out_count) {
    /* Use dynamic array for collecting params */
    int capacity = 8;
    int count = 0;
    forge_param_t* params = malloc(capacity * sizeof(forge_param_t));

    /* Parse first parameter */
    params[count++] = parse_param(p);

    /* Parse remaining parameters */
    while (match(p, TOK_COMMA)) {
        if (count >= capacity) {
            capacity *= 2;
            params = realloc(params, capacity * sizeof(forge_param_t));
        }
        params[count++] = parse_param(p);
    }

    /* Copy to arena and free temp buffer */
    forge_param_t* arena_params = ARENA_ALLOC_ARRAY(p->arena, forge_param_t, count);
    for (int i = 0; i < count; i++) {
        arena_params[i] = params[i];
    }
    free(params);

    *out_count = count;
    return arena_params;
}

/*
 * proc_decl ::= 'proc' IDENT '(' param_list? ')' '->' type ':' NEWLINE block
 */
forge_node_t* parse_proc_decl(forge_parser_t* p, int exported) {
    int line = CURRENT(p).line;
    int col = CURRENT(p).column;

    /* 'proc' keyword */
    expect(p, TOK_PROC, "expected 'proc'");

    /* Procedure name */
    forge_token_t name_tok = expect(p, TOK_IDENT, "expected procedure name");
    const char* name = name_tok.val.str_val;

    /* Opening parenthesis */
    expect(p, TOK_LPAREN, "expected '(' after procedure name");

    /* Parameter list (optional) */
    forge_param_t* params = NULL;
    int param_count = 0;

    if (!CHECK(p, TOK_RPAREN)) {
        params = parse_param_list(p, &param_count);
    }

    /* Closing parenthesis */
    expect(p, TOK_RPAREN, "expected ')' after parameters");

    /* Return type: '->' type */
    expect(p, TOK_ARROW, "expected '->' for return type");
    forge_node_t* return_type = parse_type(p);

    /* Colon before block */
    expect(p, TOK_COLON, "expected ':' after return type");

    /* Newline before block */
    expect(p, TOK_NEWLINE, "expected newline after procedure header");

    /* Parse the procedure body block */
    forge_node_t* body = parse_block(p);

    return ast_proc_decl(p->arena, name, params, param_count,
                         return_type, body, exported, line, col);
}

/*
 * record_decl ::= 'record' IDENT ':' NEWLINE INDENT field_decl+ DEDENT
 * field_decl  ::= IDENT ':' type NEWLINE
 */
forge_node_t* parse_record_decl(forge_parser_t* p, int exported) {
    int line = CURRENT(p).line;
    int col = CURRENT(p).column;

    /* Consume 'record' */
    expect(p, TOK_RECORD, "expected 'record'");

    /* Record name */
    forge_token_t name_tok = expect(p, TOK_IDENT, "expected record name");
    const char* name = name_tok.val.str_val;

    /* Colon */
    expect(p, TOK_COLON, "expected ':' after record name");

    /* Newline before indent */
    expect(p, TOK_NEWLINE, "expected newline after ':'");

    /* Expect INDENT */
    expect(p, TOK_INDENT, "expected indented record body");

    /* Collect fields */
    node_array fields;
    node_array_init(&fields);

    while (!CHECK(p, TOK_DEDENT) && !AT_END(p)) {
        /* Skip blank lines */
        if (match(p, TOK_NEWLINE)) {
            continue;
        }

        int field_line = CURRENT(p).line;
        int field_col = CURRENT(p).column;

        /* Field name */
        forge_token_t field_name_tok = expect(p, TOK_IDENT, "expected field name");
        const char* field_name = field_name_tok.val.str_val;

        /* Colon */
        expect(p, TOK_COLON, "expected ':' after field name");

        /* Field type */
        forge_node_t* field_type = parse_type(p);

        /* Create field node */
        forge_node_t* field = ast_field_def(p->arena, field_name, field_type, field_line, field_col);
        node_array_push(&fields, field);

        /* Expect newline after field */
        expect(p, TOK_NEWLINE, "expected newline after field");
    }

    /* Check we got at least one field */
    if (fields.len == 0) {
        parser_error(p, "record must have at least one field");
    }

    /* Expect DEDENT */
    expect(p, TOK_DEDENT, "expected dedent after record body");

    /* Copy fields to arena */
    forge_node_t** arena_fields = NULL;
    if (fields.len > 0) {
        arena_fields = ARENA_ALLOC_ARRAY(p->arena, forge_node_t*, fields.len);
        for (int i = 0; i < fields.len; i++) {
            arena_fields[i] = fields.data[i];
        }
    }

    forge_node_t* record = ast_record_decl(p->arena, name, arena_fields, fields.len,
                                            exported, line, col);
    node_array_free(&fields);

    return record;
}

/*
 * channel_decl ::= 'channel' IDENT ':' type NEWLINE
 */
forge_node_t* parse_channel_decl(forge_parser_t* p, int exported) {
    int line = CURRENT(p).line;
    int col = CURRENT(p).column;

    /* Consume 'channel' */
    expect(p, TOK_CHANNEL, "expected 'channel'");

    /* Channel name */
    forge_token_t name_tok = expect(p, TOK_IDENT, "expected channel name");
    const char* name = name_tok.val.str_val;

    /* Colon */
    expect(p, TOK_COLON, "expected ':' after channel name");

    /* Payload type (void for signal-only channels) */
    forge_node_t* payload_type = parse_type(p);

    /* Newline */
    expect(p, TOK_NEWLINE, "expected newline after channel declaration");

    return ast_channel_decl(p->arena, name, payload_type, exported, line, col);
}

/* var declaration */
forge_node_t* parse_var_decl(forge_parser_t* p, int exported) {
    int line = CURRENT(p).line;
    int col = CURRENT(p).column;

    expect(p, TOK_VAR, "expected 'var'");
    forge_token_t name_tok = expect(p, TOK_IDENT, "expected variable name");
    const char* name = name_tok.val.str_val;

    /* Optional type annotation: : type */
    forge_node_t* type_expr = NULL;
    if (match(p, TOK_COLON)) {
        type_expr = parse_type(p);
    }

    /* Optional initializer: = expr */
    forge_node_t* init = NULL;
    if (match(p, TOK_ASSIGN)) {
        init = parse_expr(p);
    }

    consume_newline(p);

    return ast_var_decl(p->arena, name, type_expr, init, exported, line, col);
}

/* const declaration */
forge_node_t* parse_const_decl(forge_parser_t* p, int exported) {
    int line = CURRENT(p).line;
    int col = CURRENT(p).column;

    expect(p, TOK_CONST, "expected 'const'");
    forge_token_t name_tok = expect(p, TOK_IDENT, "expected constant name");
    const char* name = name_tok.val.str_val;

    /* Type annotation is required for const */
    expect(p, TOK_COLON, "expected ':' after constant name");
    forge_node_t* type_expr = parse_type(p);

    /* Initializer is required for const */
    expect(p, TOK_ASSIGN, "expected '=' for constant initializer");
    forge_node_t* init = parse_expr(p);

    consume_newline(p);

    return ast_const_decl(p->arena, name, type_expr, init, exported, line, col);
}

/* type alias declaration */
forge_node_t* parse_type_alias(forge_parser_t* p, int exported) {
    int line = CURRENT(p).line;
    int col = CURRENT(p).column;

    expect(p, TOK_TYPE, "expected 'type'");
    forge_token_t name_tok = expect(p, TOK_IDENT, "expected type alias name");
    const char* name = name_tok.val.str_val;

    expect(p, TOK_ASSIGN, "expected '=' after type alias name");
    forge_node_t* type_expr = parse_type(p);

    consume_newline(p);

    return ast_type_alias(p->arena, name, type_expr, exported, line, col);
}

/*
 * on_handler ::= 'on' IDENT ( 'as' IDENT )? ':' NEWLINE block
 *
 * Registers a handler body that fires whenever the named channel emits.
 * The optional 'as' clause binds the emitted payload to a local name.
 * Void channels (no payload) omit the 'as' clause entirely.
 *
 * Examples:
 *   on depth_update as d:
 *       draw_depth(d)
 *
 *   on shutdown_signal:
 *       running = false
 */
forge_node_t* parse_on_handler(forge_parser_t* p) {
    int line = CURRENT(p).line;
    int col  = CURRENT(p).column;

    /* Consume 'on' */
    expect(p, TOK_ON, "expected 'on'");

    /* Channel name */
    forge_token_t ch_tok = expect(p, TOK_IDENT, "expected channel name after 'on'");
    const char* channel_name = ch_tok.val.str_val;

    /* Optional 'as' binding for the payload value */
    const char* param_name = NULL;
    if (match(p, TOK_AS)) {
        forge_token_t param_tok = expect(p, TOK_IDENT,
                                         "expected parameter name after 'as'");
        param_name = param_tok.val.str_val;
    }

    /* Colon then newline before the indented body (same as proc_decl) */
    expect(p, TOK_COLON,   "expected ':' after on handler header");
    expect(p, TOK_NEWLINE, "expected newline after ':'");

    /* Indented handler body */
    forge_node_t* body = parse_block(p);

    return ast_on_handler(p->arena, channel_name, param_name, body, line, col);
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Block and Statement Parsing (Task 2.4+)
 * ───────────────────────────────────────────────────────────────────────────── */

/*
 * block ::= INDENT statement+ DEDENT
 */
forge_node_t* parse_block(forge_parser_t* p) {
    int line = CURRENT(p).line;
    int col = CURRENT(p).column;

    /* Expect INDENT */
    expect(p, TOK_INDENT, "expected indented block");

    /* Collect statements */
    node_array stmts;
    node_array_init(&stmts);

    while (!CHECK(p, TOK_DEDENT) && !AT_END(p)) {
        /* Skip blank lines */
        if (match(p, TOK_NEWLINE)) {
            continue;
        }

        forge_node_t* stmt = parse_stmt(p);
        if (stmt) {
            node_array_push(&stmts, stmt);
        }

        /* Recovery on error */
        if (p->panic_mode) {
            synchronize(p);
        }
    }

    /* Expect DEDENT */
    expect(p, TOK_DEDENT, "expected dedent after block");

    /* Copy to arena */
    forge_node_t** arena_stmts = NULL;
    if (stmts.len > 0) {
        arena_stmts = ARENA_ALLOC_ARRAY(p->arena, forge_node_t*, stmts.len);
        for (int i = 0; i < stmts.len; i++) {
            arena_stmts[i] = stmts.data[i];
        }
    }

    forge_node_t* block = ast_block(p->arena, arena_stmts, stmts.len, line, col);
    node_array_free(&stmts);

    return block;
}

/*
 * statement ::= return_stmt | var_decl | const_decl | if_stmt | while_stmt |
 *               for_stmt | loop_stmt | break_stmt | continue_stmt | emit_stmt |
 *               expr_stmt
 */
forge_node_t* parse_stmt(forge_parser_t* p) {
    int line = CURRENT(p).line;
    int col = CURRENT(p).column;

    /* Return statement: return expr? */
    if (match(p, TOK_RETURN)) {
        forge_node_t* value = NULL;
        if (!CHECK(p, TOK_NEWLINE) && !CHECK(p, TOK_DEDENT) && !AT_END(p)) {
            value = parse_expr(p);
        }
        if (!CHECK(p, TOK_DEDENT) && !AT_END(p)) {
            expect(p, TOK_NEWLINE, "expected newline after return statement");
        }
        return ast_return(p->arena, value, line, col);
    }

    /* Break statement */
    if (match(p, TOK_BREAK)) {
        if (!CHECK(p, TOK_DEDENT) && !AT_END(p)) {
            expect(p, TOK_NEWLINE, "expected newline after break");
        }
        return ast_break(p->arena, line, col);
    }

    /* Continue statement */
    if (match(p, TOK_CONTINUE)) {
        if (!CHECK(p, TOK_DEDENT) && !AT_END(p)) {
            expect(p, TOK_NEWLINE, "expected newline after continue");
        }
        return ast_continue(p->arena, line, col);
    }

    /* If statement */
    if (CHECK(p, TOK_IF)) {
        return parse_if_stmt(p);
    }

    /* While statement */
    if (CHECK(p, TOK_WHILE)) {
        return parse_while_stmt(p);
    }

    /* For statement */
    if (CHECK(p, TOK_FOR)) {
        return parse_for_stmt(p);
    }

    /* Loop statement (infinite loop) */
    if (CHECK(p, TOK_LOOP)) {
        return parse_loop_stmt(p);
    }

    /* Emit statement */
    if (CHECK(p, TOK_EMIT)) {
        return parse_emit_stmt(p);
    }

    /* Free statement: free(expr) */
    if (match(p, TOK_FREE)) {
        expect(p, TOK_LPAREN, "expected '(' after 'free'");
        forge_node_t* expr = parse_expr(p);
        expect(p, TOK_RPAREN, "expected ')' after free expression");
        if (!CHECK(p, TOK_DEDENT) && !AT_END(p)) {
            expect(p, TOK_NEWLINE, "expected newline after free statement");
        }
        return ast_free_stmt(p->arena, expr, line, col);
    }

    /* With alloc statement: with alloc(Type, count) as name: */
    if (match(p, TOK_WITH)) {
        /* Expect 'alloc' identifier (not a keyword, but used like one here) */
        forge_token_t alloc_tok = CURRENT(p);
        if (alloc_tok.type != TOK_IDENT || strcmp(alloc_tok.val.str_val, "alloc") != 0) {
            parser_error(p, "expected 'alloc' after 'with'");
        }
        (void)ADVANCE(p);

        expect(p, TOK_LPAREN, "expected '(' after 'alloc'");

        /* Parse type expression */
        forge_node_t* type_expr = parse_type(p);

        /* Optional count argument */
        forge_node_t* size_expr = NULL;
        if (match(p, TOK_COMMA)) {
            size_expr = parse_expr(p);
        }

        expect(p, TOK_RPAREN, "expected ')' after alloc arguments");
        expect(p, TOK_AS, "expected 'as' after alloc(...)");

        forge_token_t name_tok = expect(p, TOK_IDENT, "expected variable name after 'as'");
        const char* var_name = name_tok.val.str_val;

        expect(p, TOK_COLON, "expected ':' after variable name");
        if (!CHECK(p, TOK_DEDENT) && !AT_END(p)) {
            expect(p, TOK_NEWLINE, "expected newline after 'with alloc' header");
        }

        forge_node_t* body = parse_block(p);
        return ast_with_alloc(p->arena, var_name, type_expr, size_expr, body, line, col);
    }

    /* Variable declaration inside procedure */
    if (CHECK(p, TOK_VAR)) {
        return parse_var_decl(p, 0);  /* Not exported inside proc */
    }

    /* Constant declaration inside procedure */
    if (CHECK(p, TOK_CONST)) {
        return parse_const_decl(p, 0);  /* Not exported inside proc */
    }

    /* Default: expression statement (or assignment) */
    forge_node_t* expr = parse_expr(p);

    /* Check for assignment operator */
    if (CHECK(p, TOK_ASSIGN) || CHECK(p, TOK_PLUS_EQ) ||
        CHECK(p, TOK_MINUS_EQ) || CHECK(p, TOK_STAR_EQ) ||
        CHECK(p, TOK_SLASH_EQ) || CHECK(p, TOK_PERCENT_EQ) ||
        CHECK(p, TOK_AMP_EQ) || CHECK(p, TOK_PIPE_EQ) ||
        CHECK(p, TOK_CARET_EQ) || CHECK(p, TOK_LSHIFT_EQ) ||
        CHECK(p, TOK_RSHIFT_EQ)) {
        return parse_assign_stmt(p, expr, line, col);
    }

    /* Consume newline after statement */
    if (!CHECK(p, TOK_DEDENT) && !AT_END(p)) {
        expect(p, TOK_NEWLINE, "expected newline after statement");
    }

    return ast_expr_stmt(p->arena, expr, line, col);
}

/*
 * if_stmt ::= 'if' expr ':' NEWLINE block
 *             ( 'elif' expr ':' NEWLINE block )*
 *             ( 'else' ':' NEWLINE block )?
 */
forge_node_t* parse_if_stmt(forge_parser_t* p) {
    int line = CURRENT(p).line;
    int col = CURRENT(p).column;

    /* Consume 'if' */
    expect(p, TOK_IF, "expected 'if'");

    /* Condition */
    forge_node_t* condition = parse_expr(p);

    /* Colon and newline */
    expect(p, TOK_COLON, "expected ':' after if condition");
    expect(p, TOK_NEWLINE, "expected newline after ':'");

    /* Then body */
    forge_node_t* then_body = parse_block(p);

    /* Collect elif clauses */
    node_array elif_conds;
    node_array elif_bodies;
    node_array_init(&elif_conds);
    node_array_init(&elif_bodies);

    while (CHECK(p, TOK_ELIF)) {
        (void)ADVANCE(p);  /* consume 'elif' */

        forge_node_t* elif_cond = parse_expr(p);
        expect(p, TOK_COLON, "expected ':' after elif condition");
        expect(p, TOK_NEWLINE, "expected newline after ':'");
        forge_node_t* elif_body = parse_block(p);

        node_array_push(&elif_conds, elif_cond);
        node_array_push(&elif_bodies, elif_body);
    }

    /* Optional else clause */
    forge_node_t* else_body = NULL;
    if (match(p, TOK_ELSE)) {
        expect(p, TOK_COLON, "expected ':' after else");
        expect(p, TOK_NEWLINE, "expected newline after ':'");
        else_body = parse_block(p);
    }

    /* Copy elif arrays to arena */
    forge_node_t** arena_elif_conds = NULL;
    forge_node_t** arena_elif_bodies = NULL;
    if (elif_conds.len > 0) {
        arena_elif_conds = ARENA_ALLOC_ARRAY(p->arena, forge_node_t*, elif_conds.len);
        arena_elif_bodies = ARENA_ALLOC_ARRAY(p->arena, forge_node_t*, elif_bodies.len);
        for (int i = 0; i < elif_conds.len; i++) {
            arena_elif_conds[i] = elif_conds.data[i];
            arena_elif_bodies[i] = elif_bodies.data[i];
        }
    }

    forge_node_t* result = ast_if(p->arena, condition, then_body,
                                   arena_elif_conds, arena_elif_bodies, elif_conds.len,
                                   else_body, line, col);

    node_array_free(&elif_conds);
    node_array_free(&elif_bodies);

    return result;
}

/*
 * while_stmt ::= 'while' expr ':' NEWLINE block
 */
forge_node_t* parse_while_stmt(forge_parser_t* p) {
    int line = CURRENT(p).line;
    int col = CURRENT(p).column;

    /* Consume 'while' */
    expect(p, TOK_WHILE, "expected 'while'");

    /* Condition */
    forge_node_t* condition = parse_expr(p);

    /* Colon and newline */
    expect(p, TOK_COLON, "expected ':' after while condition");
    expect(p, TOK_NEWLINE, "expected newline after ':'");

    /* Body */
    forge_node_t* body = parse_block(p);

    return ast_while(p->arena, condition, body, line, col);
}

/*
 * for_stmt ::= 'for' IDENT 'in' ( range_expr | expr ) ':' NEWLINE block
 * range_expr ::= 'range' '(' expr ',' expr ( ',' expr )? ')'
 */
forge_node_t* parse_for_stmt(forge_parser_t* p) {
    int line = CURRENT(p).line;
    int col = CURRENT(p).column;

    /* Consume 'for' */
    expect(p, TOK_FOR, "expected 'for'");

    /* Loop variable */
    forge_token_t var_tok = expect(p, TOK_IDENT, "expected loop variable");
    const char* var_name = var_tok.val.str_val;

    /* 'in' keyword */
    expect(p, TOK_IN, "expected 'in' after loop variable");

    /* Iterable expression (range or collection) */
    forge_node_t* iterable = parse_expr(p);

    /* Colon and newline */
    expect(p, TOK_COLON, "expected ':' after for iterable");
    expect(p, TOK_NEWLINE, "expected newline after ':'");

    /* Body */
    forge_node_t* body = parse_block(p);

    return ast_for(p->arena, var_name, iterable, body, line, col);
}

/*
 * loop_stmt ::= 'loop' ':' NEWLINE block
 */
forge_node_t* parse_loop_stmt(forge_parser_t* p) {
    int line = CURRENT(p).line;
    int col = CURRENT(p).column;

    /* Consume 'loop' */
    expect(p, TOK_LOOP, "expected 'loop'");

    /* Colon and newline */
    expect(p, TOK_COLON, "expected ':' after loop");
    expect(p, TOK_NEWLINE, "expected newline after ':'");

    /* Body */
    forge_node_t* body = parse_block(p);

    return ast_loop(p->arena, body, line, col);
}

/*
 * emit_stmt ::= 'emit' qualified_name ( '->' expr )? NEWLINE
 */
forge_node_t* parse_emit_stmt(forge_parser_t* p) {
    int line = CURRENT(p).line;
    int col = CURRENT(p).column;

    /* Consume 'emit' */
    expect(p, TOK_EMIT, "expected 'emit'");

    /* Channel name (identifier or qualified name) */
    forge_token_t name_tok = expect(p, TOK_IDENT, "expected channel name");
    const char* channel_name = name_tok.val.str_val;

    /* Optional payload: -> expr */
    forge_node_t* payload = NULL;
    if (match(p, TOK_ARROW)) {
        payload = parse_expr(p);
    }

    /* Newline */
    if (!CHECK(p, TOK_DEDENT) && !AT_END(p)) {
        expect(p, TOK_NEWLINE, "expected newline after emit statement");
    }

    return ast_emit(p->arena, channel_name, payload, line, col);
}

/*
 * assign_stmt ::= target assign_op expr NEWLINE
 * (Called after target has been parsed)
 */
forge_node_t* parse_assign_stmt(forge_parser_t* p, forge_node_t* target, int line, int col) {
    /* Capture which assignment operator was used */
    int op = CURRENT(p).type;

    /* Consume the assignment operator */
    (void)ADVANCE(p);

    /* Parse the value expression */
    forge_node_t* value = parse_expr(p);

    /* Newline */
    if (!CHECK(p, TOK_DEDENT) && !AT_END(p)) {
        expect(p, TOK_NEWLINE, "expected newline after assignment");
    }

    /* Plain assignment (=) vs compound assignment (+=, -=, etc.) */
    if (op == TOK_ASSIGN) {
        return ast_assign(p->arena, target, value, line, col);
    } else {
        return ast_compound_assign(p->arena, op, target, value, line, col);
    }
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Expression Parsing - Precedence Climbing
 *
 * Precedence (lowest to highest):
 *   1. or
 *   2. and
 *   3. not (unary)
 *   4. ==, !=, <, >, <=, >=
 *   5. |
 *   6. ^
 *   7. &
 *   8. <<, >>
 *   9. +, -
 *  10. *, /, %
 *  11. -, ~ (unary)
 *  12. postfix: call, index, field access
 *  13. primary: literals, identifiers, parenthesized
 * ───────────────────────────────────────────────────────────────────────────── */

/* Forward declarations for expression parsing */
static forge_node_t* parse_or_expr(forge_parser_t* p);
static forge_node_t* parse_and_expr(forge_parser_t* p);
static forge_node_t* parse_not_expr(forge_parser_t* p);
static forge_node_t* parse_cmp_expr(forge_parser_t* p);
static forge_node_t* parse_bitor_expr(forge_parser_t* p);
static forge_node_t* parse_bitxor_expr(forge_parser_t* p);
static forge_node_t* parse_bitand_expr(forge_parser_t* p);
static forge_node_t* parse_shift_expr(forge_parser_t* p);
static forge_node_t* parse_add_expr(forge_parser_t* p);
static forge_node_t* parse_mul_expr(forge_parser_t* p);
static forge_node_t* parse_unary_expr(forge_parser_t* p);
static forge_node_t* parse_postfix_expr(forge_parser_t* p);
static forge_node_t* parse_primary_expr(forge_parser_t* p);

/* Entry point for expression parsing */
forge_node_t* parse_expr(forge_parser_t* p) {
    return parse_or_expr(p);
}

/* or_expr ::= and_expr ( ('or' and_expr) | ('or_else' and_expr) )* */
static forge_node_t* parse_or_expr(forge_parser_t* p) {
    forge_node_t* left = parse_and_expr(p);

    for (;;) {
        if (match(p, TOK_OR)) {
            int line = PREVIOUS(p).line;
            int col = PREVIOUS(p).column;
            forge_node_t* right = parse_and_expr(p);
            left = ast_binary_op(p->arena, TOK_OR, left, right, line, col);
        } else if (match(p, TOK_OR_ELSE)) {
            int line = PREVIOUS(p).line;
            int col = PREVIOUS(p).column;
            forge_node_t* fallback = parse_and_expr(p);
            left = ast_or_else(p->arena, left, fallback, line, col);
        } else {
            break;
        }
    }

    return left;
}

/* and_expr ::= not_expr ( 'and' not_expr )* */
static forge_node_t* parse_and_expr(forge_parser_t* p) {
    forge_node_t* left = parse_not_expr(p);

    while (match(p, TOK_AND)) {
        int line = PREVIOUS(p).line;
        int col = PREVIOUS(p).column;
        forge_node_t* right = parse_not_expr(p);
        left = ast_binary_op(p->arena, TOK_AND, left, right, line, col);
    }

    return left;
}

/* not_expr ::= 'not' not_expr | cmp_expr */
static forge_node_t* parse_not_expr(forge_parser_t* p) {
    if (match(p, TOK_NOT)) {
        int line = PREVIOUS(p).line;
        int col = PREVIOUS(p).column;
        forge_node_t* operand = parse_not_expr(p);
        return ast_unary_op(p->arena, TOK_NOT, operand, line, col);
    }

    return parse_cmp_expr(p);
}

/* cmp_expr ::= bitor_expr ( ( '==' | '!=' | '<' | '>' | '<=' | '>=' ) bitor_expr
 *                          | 'is' ('some' | 'none') )* */
static forge_node_t* parse_cmp_expr(forge_parser_t* p) {
    forge_node_t* left = parse_bitor_expr(p);

    for (;;) {
        if (CHECK(p, TOK_EQ) || CHECK(p, TOK_NEQ) ||
            CHECK(p, TOK_LT) || CHECK(p, TOK_GT) ||
            CHECK(p, TOK_LEQ) || CHECK(p, TOK_GEQ)) {
            int op = CURRENT(p).type;
            int line = CURRENT(p).line;
            int col = CURRENT(p).column;
            (void)ADVANCE(p);
            forge_node_t* right = parse_bitor_expr(p);
            left = ast_binary_op(p->arena, op, left, right, line, col);
        } else if (match(p, TOK_IS)) {
            int line = PREVIOUS(p).line;
            int col = PREVIOUS(p).column;
            if (match(p, TOK_SOME)) {
                left = ast_is_some(p->arena, left, line, col);
            } else if (match(p, TOK_NONE)) {
                left = ast_is_none(p->arena, left, line, col);
            } else {
                parser_error(p, "expected 'some' or 'none' after 'is'");
                return left;
            }
        } else {
            break;
        }
    }

    return left;
}

/* bitor_expr ::= bitxor_expr ( '|' bitxor_expr )* */
static forge_node_t* parse_bitor_expr(forge_parser_t* p) {
    forge_node_t* left = parse_bitxor_expr(p);

    while (match(p, TOK_PIPE)) {
        int line = PREVIOUS(p).line;
        int col = PREVIOUS(p).column;
        forge_node_t* right = parse_bitxor_expr(p);
        left = ast_binary_op(p->arena, TOK_PIPE, left, right, line, col);
    }

    return left;
}

/* bitxor_expr ::= bitand_expr ( '^' bitand_expr )* */
static forge_node_t* parse_bitxor_expr(forge_parser_t* p) {
    forge_node_t* left = parse_bitand_expr(p);

    while (match(p, TOK_CARET)) {
        int line = PREVIOUS(p).line;
        int col = PREVIOUS(p).column;
        forge_node_t* right = parse_bitand_expr(p);
        left = ast_binary_op(p->arena, TOK_CARET, left, right, line, col);
    }

    return left;
}

/* bitand_expr ::= shift_expr ( '&' shift_expr )* */
static forge_node_t* parse_bitand_expr(forge_parser_t* p) {
    forge_node_t* left = parse_shift_expr(p);

    while (match(p, TOK_AMP)) {
        int line = PREVIOUS(p).line;
        int col = PREVIOUS(p).column;
        forge_node_t* right = parse_shift_expr(p);
        left = ast_binary_op(p->arena, TOK_AMP, left, right, line, col);
    }

    return left;
}

/* shift_expr ::= add_expr ( ( '<<' | '>>' ) add_expr )* */
static forge_node_t* parse_shift_expr(forge_parser_t* p) {
    forge_node_t* left = parse_add_expr(p);

    while (CHECK(p, TOK_LSHIFT) || CHECK(p, TOK_RSHIFT)) {
        int op = CURRENT(p).type;
        int line = CURRENT(p).line;
        int col = CURRENT(p).column;
        (void)ADVANCE(p);
        forge_node_t* right = parse_add_expr(p);
        left = ast_binary_op(p->arena, op, left, right, line, col);
    }

    return left;
}

/* add_expr ::= mul_expr ( ( '+' | '-' ) mul_expr )* */
static forge_node_t* parse_add_expr(forge_parser_t* p) {
    forge_node_t* left = parse_mul_expr(p);

    while (CHECK(p, TOK_PLUS) || CHECK(p, TOK_MINUS)) {
        int op = CURRENT(p).type;
        int line = CURRENT(p).line;
        int col = CURRENT(p).column;
        (void)ADVANCE(p);
        forge_node_t* right = parse_mul_expr(p);
        left = ast_binary_op(p->arena, op, left, right, line, col);
    }

    return left;
}

/* mul_expr ::= unary_expr ( ( '*' | '/' | '%' ) unary_expr )* */
static forge_node_t* parse_mul_expr(forge_parser_t* p) {
    forge_node_t* left = parse_unary_expr(p);

    while (CHECK(p, TOK_STAR) || CHECK(p, TOK_SLASH) || CHECK(p, TOK_PERCENT)) {
        int op = CURRENT(p).type;
        int line = CURRENT(p).line;
        int col = CURRENT(p).column;
        (void)ADVANCE(p);
        forge_node_t* right = parse_unary_expr(p);
        left = ast_binary_op(p->arena, op, left, right, line, col);
    }

    return left;
}

/* unary_expr ::= ( '-' | '~' ) unary_expr | postfix_expr */
static forge_node_t* parse_unary_expr(forge_parser_t* p) {
    if (CHECK(p, TOK_MINUS) || CHECK(p, TOK_TILDE)) {
        int op = CURRENT(p).type;
        int line = CURRENT(p).line;
        int col = CURRENT(p).column;
        (void)ADVANCE(p);
        forge_node_t* operand = parse_unary_expr(p);
        return ast_unary_op(p->arena, op, operand, line, col);
    }

    return parse_postfix_expr(p);
}

/* postfix_expr ::= primary_expr ( '.' IDENT | '[' expr ']' | '(' arg_list? ')' )* */
static forge_node_t* parse_postfix_expr(forge_parser_t* p) {
    forge_node_t* expr = parse_primary_expr(p);

    for (;;) {
        if (match(p, TOK_DOT)) {
            /* Field access: expr.field (allow keywords like 'str' for module paths) */
            int line = PREVIOUS(p).line;
            int col = PREVIOUS(p).column;
            const char* field_name = NULL;
            if (is_module_ident(CURRENT(p).type)) {
                forge_token_t field = ADVANCE(p);
                field_name = module_ident_name(p, field);
            } else {
                forge_token_t field = expect(p, TOK_IDENT, "expected field name after '.'");
                field_name = field.val.str_val;
            }
            expr = ast_field_access(p->arena, expr, field_name, line, col);
        }
        else if (expr->kind == NODE_IDENT && match(p, TOK_LBRACE)) {
            /* Record literal: TypeName { field: value, ... } */
            int line = PREVIOUS(p).line;
            int col = PREVIOUS(p).column;
            const char* type_name = expr->data.name;

            /* Parse field initializers */
            int capacity = 8;
            int count = 0;
            forge_field_init_t* fields = ARENA_ALLOC_ARRAY(p->arena, forge_field_init_t, capacity);

            if (!CHECK(p, TOK_RBRACE)) {
                do {
                    if (count >= capacity) {
                        /* Grow the array */
                        int new_capacity = capacity * 2;
                        forge_field_init_t* new_fields = ARENA_ALLOC_ARRAY(p->arena, forge_field_init_t, new_capacity);
                        for (int i = 0; i < count; i++) {
                            new_fields[i] = fields[i];
                        }
                        fields = new_fields;
                        capacity = new_capacity;
                    }

                    forge_token_t field_name = expect(p, TOK_IDENT, "expected field name");
                    expect(p, TOK_COLON, "expected ':' after field name");
                    forge_node_t* value = parse_expr(p);

                    fields[count].name = field_name.val.str_val;
                    fields[count].value = value;
                    count++;
                } while (match(p, TOK_COMMA));
            }

            expect(p, TOK_RBRACE, "expected '}' after record literal");
            expr = ast_record_lit(p->arena, type_name, fields, count, line, col);
        }
        else if (match(p, TOK_LBRACKET)) {
            /* Index access: expr[index] */
            int line = PREVIOUS(p).line;
            int col = PREVIOUS(p).column;
            forge_node_t* index = parse_expr(p);
            expect(p, TOK_RBRACKET, "expected ']' after index");
            expr = ast_index(p->arena, expr, index, line, col);
        }
        else if (match(p, TOK_LPAREN)) {
            /* Function call: expr(args) */
            int line = PREVIOUS(p).line;
            int col = PREVIOUS(p).column;

            node_array args;
            node_array_init(&args);

            /* Parse argument list */
            if (!CHECK(p, TOK_RPAREN)) {
                do {
                    /* Skip optional 'ref' modifier for now */
                    (void)match(p, TOK_REF);
                    forge_node_t* arg = parse_expr(p);
                    node_array_push(&args, arg);
                } while (match(p, TOK_COMMA));
            }

            expect(p, TOK_RPAREN, "expected ')' after arguments");

            /* Copy args to arena */
            forge_node_t** arena_args = NULL;
            if (args.len > 0) {
                arena_args = ARENA_ALLOC_ARRAY(p->arena, forge_node_t*, args.len);
                for (int i = 0; i < args.len; i++) {
                    arena_args[i] = args.data[i];
                }
            }

            expr = ast_call(p->arena, expr, arena_args, args.len, line, col);
            node_array_free(&args);
        }
        else {
            break;
        }
    }

    return expr;
}

/* primary_expr ::= INT_LIT | FLOAT_LIT | STR_LIT | 'true' | 'false' | 'none'
 *                | IDENT | '(' expr ')' | '[' array_elements ']' | 'range' '(' ... ')' */
static forge_node_t* parse_primary_expr(forge_parser_t* p) {
    int line = CURRENT(p).line;
    int col = CURRENT(p).column;

    /* Integer literal */
    if (CHECK(p, TOK_INT_LIT)) {
        forge_token_t tok = ADVANCE(p);
        return ast_int_lit(p->arena, tok.val.int_val, line, col);
    }

    /* Float literal */
    if (CHECK(p, TOK_FLOAT_LIT)) {
        forge_token_t tok = ADVANCE(p);
        return ast_float_lit(p->arena, tok.val.float_val, line, col);
    }

    /* String literal */
    if (CHECK(p, TOK_STR_LIT) || CHECK(p, TOK_RAW_STR_LIT)) {
        forge_token_t tok = ADVANCE(p);
        return ast_str_lit(p->arena, tok.val.str_val, line, col);
    }

    /* Boolean literals */
    if (match(p, TOK_TRUE)) {
        return ast_bool_lit(p->arena, 1, line, col);
    }
    if (match(p, TOK_FALSE)) {
        return ast_bool_lit(p->arena, 0, line, col);
    }

    /* None literal */
    if (match(p, TOK_NONE)) {
        return ast_none_lit(p->arena, line, col);
    }

    /* some(expr) - wrap value in optional */
    if (match(p, TOK_SOME)) {
        expect(p, TOK_LPAREN, "expected '(' after 'some'");
        forge_node_t* expr = parse_expr(p);
        expect(p, TOK_RPAREN, "expected ')' after some expression");
        return ast_some(p->arena, expr, line, col);
    }

    /* Parenthesized expression */
    if (match(p, TOK_LPAREN)) {
        forge_node_t* expr = parse_expr(p);
        expect(p, TOK_RPAREN, "expected ')' after expression");
        return expr;
    }

    /* Array literal: [ elem1, elem2, ... ] */
    if (match(p, TOK_LBRACKET)) {
        node_array elements;
        node_array_init(&elements);

        if (!CHECK(p, TOK_RBRACKET)) {
            do {
                forge_node_t* elem = parse_expr(p);
                node_array_push(&elements, elem);
            } while (match(p, TOK_COMMA));
        }

        expect(p, TOK_RBRACKET, "expected ']' after array elements");

        /* Copy to arena */
        forge_node_t** arena_elems = NULL;
        if (elements.len > 0) {
            arena_elems = ARENA_ALLOC_ARRAY(p->arena, forge_node_t*, elements.len);
            for (int i = 0; i < elements.len; i++) {
                arena_elems[i] = elements.data[i];
            }
        }

        forge_node_t* result = ast_array_lit(p->arena, arena_elems, elements.len, line, col);
        node_array_free(&elements);
        return result;
    }

    /* Range expression: range(start, end) or range(start, end, step) */
    if (match(p, TOK_RANGE)) {
        expect(p, TOK_LPAREN, "expected '(' after 'range'");
        forge_node_t* start = parse_expr(p);
        expect(p, TOK_COMMA, "expected ',' after range start");
        forge_node_t* end = parse_expr(p);

        /* Optional step argument (for future use) */
        if (match(p, TOK_COMMA)) {
            (void)parse_expr(p);  /* Parse but ignore step for now */
        }

        expect(p, TOK_RPAREN, "expected ')' after range arguments");

        /* Store as a range node */
        return ast_range(p->arena, start, end, 0, line, col);
    }

    /* Type conversion/cast: int(expr), float(expr), str(expr), etc. */
    if (is_primitive_type(CURRENT(p).type)) {
        forge_token_t type_tok = ADVANCE(p);
        if (match(p, TOK_LPAREN)) {
            /* This is a type cast: type(expr) */
            forge_node_t* expr = parse_expr(p);
            expect(p, TOK_RPAREN, "expected ')' after cast expression");

            /* Create the type node for the cast target */
            const char* type_name = primitive_type_name(p, type_tok.type);
            forge_node_t* type_node = ast_type_prim(p->arena, type_name, line, col);

            return ast_cast(p->arena, type_node, expr, line, col);
        } else {
            /* Type keyword without '(' - this is an error in expression context */
            parser_error(p, "expected '(' after type name for cast expression");
            return NULL;
        }
    }

    /* Identifier (simple or qualified) */
    if (CHECK(p, TOK_IDENT)) {
        forge_token_t tok = ADVANCE(p);
        return ast_ident(p->arena, tok.val.str_val, line, col);
    }

    parser_error(p, "expected expression");
    return NULL;
}