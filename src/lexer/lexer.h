/*
 * FORGE Language Toolchain
 * lexer.h - Lexical analyzer / tokenizer
 *
 * The lexer reads UTF-8 source and emits a flat array of tokens.
 * It handles indentation-based scoping via INDENT/DEDENT tokens.
 */

#ifndef FORGE_LEXER_H
#define FORGE_LEXER_H

#include "common.h"
#include "util/strtable.h"
#include "util/dynarray.h"

/* ─────────────────────────────────────────────────────────────────────────────
 * Token Types
 * ───────────────────────────────────────────────────────────────────────────── */

typedef enum {
    /* Literals */
    TOK_INT_LIT,
    TOK_FLOAT_LIT,
    TOK_STR_LIT,
    TOK_RAW_STR_LIT,
    
    /* Keywords (alphabetical) */
    TOK_AND, TOK_AS, TOK_ASSERT, TOK_BOOL, TOK_BREAK, TOK_BYTE,
    TOK_CHANNEL, TOK_CONST, TOK_CONTINUE,
    TOK_ELIF, TOK_ELSE, TOK_EMIT, TOK_EXPORT,
    TOK_FALSE, TOK_FLOAT_KW, TOK_FLOAT32_KW, TOK_FOR, TOK_FREE,
    TOK_IF, TOK_IMPORT, TOK_IN, TOK_INT_KW, TOK_INT8_KW, TOK_INT16_KW, TOK_INT32_KW,
    TOK_IS, TOK_LOOP, TOK_MAP,
    TOK_NONE, TOK_NOT,
    TOK_ON, TOK_OR, TOK_OR_ELSE,
    TOK_PANIC, TOK_PROC,
    TOK_RANGE, TOK_RECORD, TOK_REF, TOK_RETURN,
    TOK_SOME, TOK_STR_KW,
    TOK_TRUE, TOK_TYPE,
    TOK_UINT_KW, TOK_UINT8_KW, TOK_UINT16_KW, TOK_UINT32_KW,
    TOK_VAR, TOK_VOID,
    TOK_WHILE, TOK_WITH,
    
    /* Identifiers */
    TOK_IDENT,
    
    /* Operators */
    TOK_PLUS,           /* + */
    TOK_MINUS,          /* - */
    TOK_STAR,           /* * */
    TOK_SLASH,          /* / */
    TOK_PERCENT,        /* % */
    TOK_EQ,             /* == */
    TOK_NEQ,            /* != */
    TOK_LT,             /* < */
    TOK_GT,             /* > */
    TOK_LEQ,            /* <= */
    TOK_GEQ,            /* >= */
    TOK_AMP,            /* & */
    TOK_PIPE,           /* | */
    TOK_CARET,          /* ^ */
    TOK_TILDE,          /* ~ */
    TOK_LSHIFT,         /* << */
    TOK_RSHIFT,         /* >> */
    TOK_ASSIGN,         /* = */
    TOK_PLUS_EQ,        /* += */
    TOK_MINUS_EQ,       /* -= */
    TOK_STAR_EQ,        /* *= */
    TOK_SLASH_EQ,       /* /= */
    TOK_PERCENT_EQ,     /* %= */
    TOK_AMP_EQ,         /* &= */
    TOK_PIPE_EQ,        /* |= */
    TOK_CARET_EQ,       /* ^= */
    TOK_LSHIFT_EQ,      /* <<= */
    TOK_RSHIFT_EQ,      /* >>= */
    TOK_ARROW,          /* -> */
    TOK_DOTDOT,         /* .. */
    TOK_DOTDOT_EQ,      /* ..= */
    
    /* Punctuation */
    TOK_DOT,            /* . */
    TOK_COMMA,          /* , */
    TOK_COLON,          /* : */
    TOK_QUESTION,       /* ? */
    TOK_LPAREN,         /* ( */
    TOK_RPAREN,         /* ) */
    TOK_LBRACKET,       /* [ */
    TOK_RBRACKET,       /* ] */
    TOK_LBRACE,         /* { */
    TOK_RBRACE,         /* } */
    TOK_BACKSLASH,      /* \ (line continuation) */
    
    /* Structural */
    TOK_NEWLINE,
    TOK_INDENT,
    TOK_DEDENT,
    TOK_EOF,
    TOK_ERROR
} forge_token_type_t;

/* ─────────────────────────────────────────────────────────────────────────────
 * Token Structure
 * ───────────────────────────────────────────────────────────────────────────── */

struct forge_token {
    forge_token_type_t  type;
    const char*         start;      /* Pointer into source buffer (not owned) */
    int                 length;     /* Byte length in source */
    int                 line;
    int                 column;
    union {
        long long       int_val;    /* TOK_INT_LIT */
        double          float_val;  /* TOK_FLOAT_LIT */
        const char*     str_val;    /* TOK_STR_LIT, TOK_RAW_STR_LIT, TOK_IDENT — interned */
    } val;
};

/* Define token array type */
DYNARRAY_DEFINE(forge_token_array, forge_token_t)

/* ─────────────────────────────────────────────────────────────────────────────
 * Lexer Structure
 * ───────────────────────────────────────────────────────────────────────────── */

#define LEXER_MAX_INDENT_DEPTH 256

struct forge_lexer {
    const char*         source;         /* Full source buffer */
    int                 source_len;
    const char*         filename;
    int                 pos;            /* Current position in source */
    int                 line;
    int                 column;
    
    /* Indentation tracking */
    int                 indent_stack[LEXER_MAX_INDENT_DEPTH];
    int                 indent_depth;
    int                 pending_dedents;
    int                 at_line_start;
    
    /* Bracket nesting (for implicit line continuation) */
    int                 paren_depth;    /* () */
    int                 bracket_depth;  /* [] */
    int                 brace_depth;    /* {} */
    
    /* Token output */
    forge_token_array   tokens;
    
    /* String table for interning */
    forge_strtable_t*   strtable;
    
    /* Error state */
    int                 had_error;
    int                 error_count;
};

/* ─────────────────────────────────────────────────────────────────────────────
 * Public API
 * ───────────────────────────────────────────────────────────────────────────── */

/*
 * Create a new lexer for the given source.
 * The source buffer must remain valid for the lifetime of the lexer.
 * The strtable is used for string interning and must remain valid.
 */
forge_lexer_t* lexer_create(const char* source, int len, const char* filename,
                             forge_strtable_t* strtable);

/*
 * Tokenize the entire source. Call once after lexer_create.
 * After this call, access tokens via lexer->tokens.
 */
void lexer_tokenize(forge_lexer_t* lex);

/*
 * Destroy the lexer and free associated memory.
 * Does NOT free the strtable (caller owns it).
 */
void lexer_destroy(forge_lexer_t* lex);

/*
 * Print all tokens to stdout (for debugging).
 */
void lexer_print_tokens(forge_lexer_t* lex);

/*
 * Get the name of a token type (for debugging/errors).
 */
const char* token_type_name(forge_token_type_t type);

#endif /* FORGE_LEXER_H */

