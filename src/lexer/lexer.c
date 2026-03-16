/*
 * FORGE Language Toolchain
 * lexer.c - Lexical analyzer implementation
 */

#include "lexer/lexer.h"
#include <ctype.h>

/* ─────────────────────────────────────────────────────────────────────────────
 * Keyword Table
 * ───────────────────────────────────────────────────────────────────────────── */

typedef struct {
    const char*         word;
    forge_token_type_t  type;
} keyword_entry_t;

/* Keywords sorted alphabetically for binary search */
static const keyword_entry_t keywords[] = {
    { "and",      TOK_AND },
    { "as",       TOK_AS },
    { "assert",   TOK_ASSERT },
    { "bool",     TOK_BOOL },
    { "break",    TOK_BREAK },
    { "byte",     TOK_BYTE },
    { "channel",  TOK_CHANNEL },
    { "const",    TOK_CONST },
    { "continue", TOK_CONTINUE },
    { "elif",     TOK_ELIF },
    { "else",     TOK_ELSE },
    { "emit",     TOK_EMIT },
    { "export",   TOK_EXPORT },
    { "false",    TOK_FALSE },
    { "float",    TOK_FLOAT_KW },
    { "float32",  TOK_FLOAT32_KW },
    { "for",      TOK_FOR },
    { "free",     TOK_FREE },
    { "if",       TOK_IF },
    { "import",   TOK_IMPORT },
    { "in",       TOK_IN },
    { "int",      TOK_INT_KW },
    { "int16",    TOK_INT16_KW },
    { "int32",    TOK_INT32_KW },
    { "int8",     TOK_INT8_KW },
    { "is",       TOK_IS },
    { "loop",     TOK_LOOP },
    { "map",      TOK_MAP },
    { "none",     TOK_NONE },
    { "not",      TOK_NOT },
    { "on",       TOK_ON },
    { "or",       TOK_OR },
    { "or_else",  TOK_OR_ELSE },
    { "panic",    TOK_PANIC },
    { "proc",     TOK_PROC },
    { "range",    TOK_RANGE },
    { "record",   TOK_RECORD },
    { "ref",      TOK_REF },
    { "return",   TOK_RETURN },
    { "some",     TOK_SOME },
    { "str",      TOK_STR_KW },
    { "true",     TOK_TRUE },
    { "type",     TOK_TYPE },
    { "uint",     TOK_UINT_KW },
    { "uint16",   TOK_UINT16_KW },
    { "uint32",   TOK_UINT32_KW },
    { "uint8",    TOK_UINT8_KW },
    { "var",      TOK_VAR },
    { "void",     TOK_VOID },
    { "while",    TOK_WHILE },
    { "with",     TOK_WITH },
};

static const int keyword_count = sizeof(keywords) / sizeof(keywords[0]);

/* ─────────────────────────────────────────────────────────────────────────────
 * Helper Functions
 * ───────────────────────────────────────────────────────────────────────────── */

static inline bool is_at_end(forge_lexer_t* lex) {
    return lex->pos >= lex->source_len;
}

static inline char peek(forge_lexer_t* lex) {
    if (is_at_end(lex)) return '\0';
    return lex->source[lex->pos];
}

static inline char peek_next(forge_lexer_t* lex) {
    if (lex->pos + 1 >= lex->source_len) return '\0';
    return lex->source[lex->pos + 1];
}

static inline char advance(forge_lexer_t* lex) {
    char c = lex->source[lex->pos++];
    if (c == '\n') {
        lex->line++;
        lex->column = 1;
    } else {
        lex->column++;
    }
    return c;
}

static inline bool match(forge_lexer_t* lex, char expected) {
    if (is_at_end(lex)) return false;
    if (lex->source[lex->pos] != expected) return false;
    advance(lex);
    return true;
}

static inline bool is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static inline bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

static inline bool is_alnum(char c) {
    return is_alpha(c) || is_digit(c);
}

static inline bool is_hex_digit(char c) {
    return is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

static inline bool is_binary_digit(char c) {
    return c == '0' || c == '1';
}

static inline bool is_octal_digit(char c) {
    return c >= '0' && c <= '7';
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Token Emission
 * ───────────────────────────────────────────────────────────────────────────── */

static void emit_token(forge_lexer_t* lex, forge_token_type_t type,
                       const char* start, int length, int line, int column) {
    forge_token_t tok = {0};
    tok.type = type;
    tok.start = start;
    tok.length = length;
    tok.line = line;
    tok.column = column;
    forge_token_array_push(&lex->tokens, tok);
}

static void emit_simple(forge_lexer_t* lex, forge_token_type_t type,
                        const char* start, int line, int column) {
    emit_token(lex, type, start, lex->pos - (start - lex->source), line, column);
}

static void lexer_error(forge_lexer_t* lex, const char* message) {
    fprintf(stderr, "%s:%d:%d: error: %s\n",
            lex->filename, lex->line, lex->column, message);
    lex->had_error = 1;
    lex->error_count++;

    /* Emit error token */
    forge_token_t tok = {0};
    tok.type = TOK_ERROR;
    tok.start = lex->source + lex->pos;
    tok.length = 1;
    tok.line = lex->line;
    tok.column = lex->column;
    forge_token_array_push(&lex->tokens, tok);
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Keyword Lookup
 * ───────────────────────────────────────────────────────────────────────────── */

static forge_token_type_t lookup_keyword(const char* name, int len) {
    /* Binary search through sorted keyword table */
    int lo = 0, hi = keyword_count - 1;

    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        int cmp = strncmp(name, keywords[mid].word, len);

        if (cmp == 0) {
            /* Check exact length match */
            if (keywords[mid].word[len] == '\0') {
                return keywords[mid].type;
            }
            /* Our string is shorter */
            cmp = -1;
        }

        if (cmp < 0) {
            hi = mid - 1;
        } else {
            lo = mid + 1;
        }
    }

    return TOK_IDENT;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Number Scanning
 * ───────────────────────────────────────────────────────────────────────────── */

static void scan_number(forge_lexer_t* lex, const char* start, int start_line, int start_col) {
    bool is_float = false;
    long long int_val = 0;
    double float_val = 0.0;

    /* Check for hex, binary, octal prefix */
    if (peek(lex) == '0' && !is_at_end(lex)) {
        char next = peek_next(lex);

        if (next == 'x' || next == 'X') {
            /* Hexadecimal */
            advance(lex); advance(lex);  /* Skip 0x */
            while (is_hex_digit(peek(lex)) || peek(lex) == '_') {
                char c = advance(lex);
                if (c != '_') {
                    int_val = int_val * 16;
                    if (c >= '0' && c <= '9') int_val += c - '0';
                    else if (c >= 'a' && c <= 'f') int_val += c - 'a' + 10;
                    else int_val += c - 'A' + 10;
                }
            }
            goto emit_int;
        } else if (next == 'b' || next == 'B') {
            /* Binary */
            advance(lex); advance(lex);  /* Skip 0b */
            while (is_binary_digit(peek(lex)) || peek(lex) == '_') {
                char c = advance(lex);
                if (c != '_') int_val = int_val * 2 + (c - '0');
            }
            goto emit_int;
        } else if (next == 'o' || next == 'O') {
            /* Octal */
            advance(lex); advance(lex);  /* Skip 0o */
            while (is_octal_digit(peek(lex)) || peek(lex) == '_') {
                char c = advance(lex);
                if (c != '_') int_val = int_val * 8 + (c - '0');
            }
            goto emit_int;
        }
    }

    /* Decimal integer or float */
    while (is_digit(peek(lex)) || peek(lex) == '_') {
        char c = advance(lex);
        if (c != '_') int_val = int_val * 10 + (c - '0');
    }

    /* Check for decimal point (but not range ..) */
    if (peek(lex) == '.' && peek_next(lex) != '.') {
        is_float = true;
        float_val = (double)int_val;
        advance(lex);  /* Consume '.' */

        double frac = 0.1;
        while (is_digit(peek(lex)) || peek(lex) == '_') {
            char c = advance(lex);
            if (c != '_') {
                float_val += (c - '0') * frac;
                frac *= 0.1;
            }
        }
    }

    /* Check for exponent */
    if (peek(lex) == 'e' || peek(lex) == 'E') {
        if (!is_float) {
            is_float = true;
            float_val = (double)int_val;
        }
        advance(lex);  /* Consume 'e' */

        int exp_sign = 1;
        if (peek(lex) == '+') advance(lex);
        else if (peek(lex) == '-') { advance(lex); exp_sign = -1; }

        int exp = 0;
        while (is_digit(peek(lex))) {
            exp = exp * 10 + (advance(lex) - '0');
        }

        double multiplier = 1.0;
        for (int i = 0; i < exp; i++) multiplier *= 10.0;
        if (exp_sign < 0) float_val /= multiplier;
        else float_val *= multiplier;
    }

    if (is_float) {
        forge_token_t tok = {0};
        tok.type = TOK_FLOAT_LIT;
        tok.start = start;
        tok.length = lex->pos - (start - lex->source);
        tok.line = start_line;
        tok.column = start_col;
        tok.val.float_val = float_val;
        forge_token_array_push(&lex->tokens, tok);
        return;
    }

emit_int:;
    forge_token_t tok = {0};
    tok.type = TOK_INT_LIT;
    tok.start = start;
    tok.length = lex->pos - (start - lex->source);
    tok.line = start_line;
    tok.column = start_col;
    tok.val.int_val = int_val;
    forge_token_array_push(&lex->tokens, tok);
}

/* ─────────────────────────────────────────────────────────────────────────────
 * String Scanning
 * ───────────────────────────────────────────────────────────────────────────── */

static void scan_string(forge_lexer_t* lex, const char* start, int start_line, int start_col) {
    /* Build unescaped string in a buffer */
    char buffer[4096];
    int buf_len = 0;

    while (!is_at_end(lex) && peek(lex) != '"') {
        if (peek(lex) == '\n') {
            lexer_error(lex, "unterminated string (newline in string)");
            return;
        }

        if (peek(lex) == '\\') {
            advance(lex);  /* Consume backslash */
            if (is_at_end(lex)) {
                lexer_error(lex, "unterminated string (EOF after escape)");
                return;
            }

            char esc = advance(lex);
            switch (esc) {
                case 'n':  buffer[buf_len++] = '\n'; break;
                case 't':  buffer[buf_len++] = '\t'; break;
                case 'r':  buffer[buf_len++] = '\r'; break;
                case '\\': buffer[buf_len++] = '\\'; break;
                case '"':  buffer[buf_len++] = '"'; break;
                case '0':  buffer[buf_len++] = '\0'; break;
                default:
                    lexer_error(lex, "invalid escape sequence");
                    buffer[buf_len++] = esc;
                    break;
            }
        } else {
            buffer[buf_len++] = advance(lex);
        }

        if (buf_len >= 4095) {
            lexer_error(lex, "string too long");
            return;
        }
    }

    if (is_at_end(lex)) {
        lexer_error(lex, "unterminated string");
        return;
    }

    advance(lex);  /* Consume closing quote */

    /* Intern the string */
    const char* interned = strtable_intern(lex->strtable, buffer, buf_len);

    forge_token_t tok = {0};
    tok.type = TOK_STR_LIT;
    tok.start = start;
    tok.length = lex->pos - (start - lex->source);
    tok.line = start_line;
    tok.column = start_col;
    tok.val.str_val = interned;
    forge_token_array_push(&lex->tokens, tok);
}

static void scan_raw_string(forge_lexer_t* lex, const char* start, int start_line, int start_col) {
    /* Raw strings: no escape processing */
    const char* str_start = lex->source + lex->pos;
    int str_len = 0;

    while (!is_at_end(lex) && peek(lex) != '`') {
        advance(lex);
        str_len++;
    }

    if (is_at_end(lex)) {
        lexer_error(lex, "unterminated raw string");
        return;
    }

    advance(lex);  /* Consume closing backtick */

    const char* interned = strtable_intern(lex->strtable, str_start, str_len);

    forge_token_t tok = {0};
    tok.type = TOK_RAW_STR_LIT;
    tok.start = start;
    tok.length = lex->pos - (start - lex->source);
    tok.line = start_line;
    tok.column = start_col;
    tok.val.str_val = interned;
    forge_token_array_push(&lex->tokens, tok);
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Identifier Scanning
 * ───────────────────────────────────────────────────────────────────────────── */

static void scan_identifier(forge_lexer_t* lex, const char* start, int start_line, int start_col) {
    while (is_alnum(peek(lex))) {
        advance(lex);
    }

    int len = lex->pos - (start - lex->source);
    forge_token_type_t type = lookup_keyword(start, len);

    forge_token_t tok = {0};
    tok.type = type;
    tok.start = start;
    tok.length = len;
    tok.line = start_line;
    tok.column = start_col;

    if (type == TOK_IDENT) {
        tok.val.str_val = strtable_intern(lex->strtable, start, len);
    }

    forge_token_array_push(&lex->tokens, tok);
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Indentation Handling
 * ───────────────────────────────────────────────────────────────────────────── */

static void handle_indentation(forge_lexer_t* lex) {
    int spaces = 0;
    int indent_start_col = lex->column;

    /* Count leading spaces */
    while (peek(lex) == ' ') {
        advance(lex);
        spaces++;
    }

    /* Tab check */
    if (peek(lex) == '\t') {
        lexer_error(lex, "tabs are not allowed for indentation");
        /* Skip all tabs */
        while (peek(lex) == '\t') advance(lex);
        return;
    }

    /* Skip blank lines for indentation purposes (don't change indent level) */
    if (peek(lex) == '\n' || is_at_end(lex)) {
        return;
    }

    int current_indent = lex->indent_stack[lex->indent_depth];

    if (spaces > current_indent) {
        /* INDENT */
        if (lex->indent_depth >= LEXER_MAX_INDENT_DEPTH - 1) {
            lexer_error(lex, "maximum indentation depth exceeded");
            return;
        }
        lex->indent_stack[++lex->indent_depth] = spaces;
        emit_token(lex, TOK_INDENT, lex->source + lex->pos, 0, lex->line, indent_start_col);
    } else if (spaces < current_indent) {
        /* DEDENT(s) */
        while (lex->indent_depth > 0 && spaces < lex->indent_stack[lex->indent_depth]) {
            lex->indent_depth--;
            emit_token(lex, TOK_DEDENT, lex->source + lex->pos, 0, lex->line, indent_start_col);
        }

        /* Check for inconsistent dedent */
        if (spaces != lex->indent_stack[lex->indent_depth]) {
            lexer_error(lex, "inconsistent dedent");
        }
    }
    /* else: spaces == current_indent, no action */

    /* If this is a comment-only line, the comment will be skipped by scan_token
     * and no statement tokens are emitted, but INDENT/DEDENT was already handled above */
}

static void emit_pending_dedents(forge_lexer_t* lex) {
    /* Emit all remaining DEDENTs at EOF */
    while (lex->indent_depth > 0) {
        lex->indent_depth--;
        emit_token(lex, TOK_DEDENT, lex->source + lex->pos, 0, lex->line, lex->column);
    }
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Skip Whitespace and Comments (within line)
 * ───────────────────────────────────────────────────────────────────────────── */

static void skip_whitespace(forge_lexer_t* lex) {
    while (!is_at_end(lex)) {
        char c = peek(lex);
        if (c == ' ' || c == '\t' || c == '\r') {
            advance(lex);
        } else {
            break;
        }
    }
}

static void skip_comment(forge_lexer_t* lex) {
    /* Skip to end of line */
    while (!is_at_end(lex) && peek(lex) != '\n') {
        advance(lex);
    }
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Main Token Scanner
 * ───────────────────────────────────────────────────────────────────────────── */

static void scan_token(forge_lexer_t* lex) {
    skip_whitespace(lex);

    if (is_at_end(lex)) return;

    const char* start = lex->source + lex->pos;
    int start_line = lex->line;
    int start_col = lex->column;

    char c = advance(lex);

    /* Single-character tokens */
    switch (c) {
        case '(': lex->paren_depth++; emit_simple(lex, TOK_LPAREN, start, start_line, start_col); return;
        case ')': lex->paren_depth--; emit_simple(lex, TOK_RPAREN, start, start_line, start_col); return;
        case '[': lex->bracket_depth++; emit_simple(lex, TOK_LBRACKET, start, start_line, start_col); return;
        case ']': lex->bracket_depth--; emit_simple(lex, TOK_RBRACKET, start, start_line, start_col); return;
        case '{': lex->brace_depth++; emit_simple(lex, TOK_LBRACE, start, start_line, start_col); return;
        case '}': lex->brace_depth--; emit_simple(lex, TOK_RBRACE, start, start_line, start_col); return;
        case ',': emit_simple(lex, TOK_COMMA, start, start_line, start_col); return;
        case ':': emit_simple(lex, TOK_COLON, start, start_line, start_col); return;
        case '?': emit_simple(lex, TOK_QUESTION, start, start_line, start_col); return;
        case '~': emit_simple(lex, TOK_TILDE, start, start_line, start_col); return;

        case '+':
            emit_simple(lex, match(lex, '=') ? TOK_PLUS_EQ : TOK_PLUS, start, start_line, start_col);
            return;
        case '*':
            emit_simple(lex, match(lex, '=') ? TOK_STAR_EQ : TOK_STAR, start, start_line, start_col);
            return;
        case '/':
            emit_simple(lex, match(lex, '=') ? TOK_SLASH_EQ : TOK_SLASH, start, start_line, start_col);
            return;
        case '%':
            emit_simple(lex, match(lex, '=') ? TOK_PERCENT_EQ : TOK_PERCENT, start, start_line, start_col);
            return;
        case '^':
            emit_simple(lex, match(lex, '=') ? TOK_CARET_EQ : TOK_CARET, start, start_line, start_col);
            return;

        case '-':
            if (match(lex, '>')) emit_simple(lex, TOK_ARROW, start, start_line, start_col);
            else if (match(lex, '=')) emit_simple(lex, TOK_MINUS_EQ, start, start_line, start_col);
            else emit_simple(lex, TOK_MINUS, start, start_line, start_col);
            return;

        case '=':
            emit_simple(lex, match(lex, '=') ? TOK_EQ : TOK_ASSIGN, start, start_line, start_col);
            return;

        case '!':
            if (match(lex, '=')) emit_simple(lex, TOK_NEQ, start, start_line, start_col);
            else lexer_error(lex, "unexpected character '!'");
            return;

        case '<':
            if (match(lex, '<')) {
                emit_simple(lex, match(lex, '=') ? TOK_LSHIFT_EQ : TOK_LSHIFT, start, start_line, start_col);
            } else if (match(lex, '=')) {
                emit_simple(lex, TOK_LEQ, start, start_line, start_col);
            } else {
                emit_simple(lex, TOK_LT, start, start_line, start_col);
            }
            return;

        case '>':
            if (match(lex, '>')) {
                emit_simple(lex, match(lex, '=') ? TOK_RSHIFT_EQ : TOK_RSHIFT, start, start_line, start_col);
            } else if (match(lex, '=')) {
                emit_simple(lex, TOK_GEQ, start, start_line, start_col);
            } else {
                emit_simple(lex, TOK_GT, start, start_line, start_col);
            }
            return;

        case '&':
            emit_simple(lex, match(lex, '=') ? TOK_AMP_EQ : TOK_AMP, start, start_line, start_col);
            return;

        case '|':
            emit_simple(lex, match(lex, '=') ? TOK_PIPE_EQ : TOK_PIPE, start, start_line, start_col);
            return;

        case '.':
            if (match(lex, '.')) {
                emit_simple(lex, match(lex, '=') ? TOK_DOTDOT_EQ : TOK_DOTDOT, start, start_line, start_col);
            } else {
                emit_simple(lex, TOK_DOT, start, start_line, start_col);
            }
            return;

        case '\\':
            /* Line continuation: skip newline */
            skip_whitespace(lex);
            if (peek(lex) == '\n') {
                advance(lex);
                lex->at_line_start = 0;  /* Don't process indentation */
            } else if (!is_at_end(lex)) {
                lexer_error(lex, "expected newline after '\\'");
            }
            return;

        case '#':
            skip_comment(lex);
            return;

        case '\n':
            /* Only emit NEWLINE if not inside brackets */
            if (lex->paren_depth == 0 && lex->bracket_depth == 0 && lex->brace_depth == 0) {
                emit_simple(lex, TOK_NEWLINE, start, start_line, start_col);
                lex->at_line_start = 1;
            }
            return;

        case '"':
            scan_string(lex, start, start_line, start_col);
            return;

        case '`':
            scan_raw_string(lex, start, start_line, start_col);
            return;
    }

    /* Numbers */
    if (is_digit(c)) {
        lex->pos--;  /* Back up to re-read the digit */
        lex->column--;
        scan_number(lex, start, start_line, start_col);
        return;
    }

    /* Identifiers and keywords */
    if (is_alpha(c)) {
        scan_identifier(lex, start, start_line, start_col);
        return;
    }

    /* Unknown character */
    char msg[64];
    snprintf(msg, sizeof(msg), "unexpected character '%c' (0x%02x)", c, (unsigned char)c);
    lexer_error(lex, msg);
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Public API
 * ───────────────────────────────────────────────────────────────────────────── */

forge_lexer_t* lexer_create(const char* source, int len, const char* filename,
                             forge_strtable_t* strtable) {
    forge_lexer_t* lex = forge_calloc(1, sizeof(forge_lexer_t));
    lex->source = source;
    lex->source_len = len;
    lex->filename = filename;
    lex->pos = 0;
    lex->line = 1;
    lex->column = 1;
    lex->indent_stack[0] = 0;
    lex->indent_depth = 0;
    lex->at_line_start = 1;
    lex->strtable = strtable;
    forge_token_array_init(&lex->tokens);
    return lex;
}

void lexer_tokenize(forge_lexer_t* lex) {
    while (!is_at_end(lex)) {
        /* Handle indentation at line start */
        if (lex->at_line_start) {
            lex->at_line_start = 0;
            handle_indentation(lex);
        }

        scan_token(lex);
    }

    /* Emit remaining DEDENTs */
    emit_pending_dedents(lex);

    /* Emit EOF */
    emit_token(lex, TOK_EOF, lex->source + lex->pos, 0, lex->line, lex->column);
}

void lexer_destroy(forge_lexer_t* lex) {
    if (lex == NULL) return;
    forge_token_array_free(&lex->tokens);
    forge_free(lex);
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Debug Functions
 * ───────────────────────────────────────────────────────────────────────────── */

const char* token_type_name(forge_token_type_t type) {
    switch (type) {
        case TOK_INT_LIT: return "INT_LIT";
        case TOK_FLOAT_LIT: return "FLOAT_LIT";
        case TOK_STR_LIT: return "STR_LIT";
        case TOK_RAW_STR_LIT: return "RAW_STR_LIT";
        case TOK_AND: return "AND";
        case TOK_AS: return "AS";
        case TOK_ASSERT: return "ASSERT";
        case TOK_BOOL: return "BOOL";
        case TOK_BREAK: return "BREAK";
        case TOK_BYTE: return "BYTE";
        case TOK_CHANNEL: return "CHANNEL";
        case TOK_CONST: return "CONST";
        case TOK_CONTINUE: return "CONTINUE";
        case TOK_ELIF: return "ELIF";
        case TOK_ELSE: return "ELSE";
        case TOK_EMIT: return "EMIT";
        case TOK_EXPORT: return "EXPORT";
        case TOK_FALSE: return "FALSE";
        case TOK_FLOAT_KW: return "FLOAT_KW";
        case TOK_FLOAT32_KW: return "FLOAT32_KW";
        case TOK_FOR: return "FOR";
        case TOK_FREE: return "FREE";
        case TOK_IF: return "IF";
        case TOK_IMPORT: return "IMPORT";
        case TOK_IN: return "IN";
        case TOK_INT_KW: return "INT_KW";
        case TOK_INT8_KW: return "INT8_KW";
        case TOK_INT16_KW: return "INT16_KW";
        case TOK_INT32_KW: return "INT32_KW";
        case TOK_IS: return "IS";
        case TOK_LOOP: return "LOOP";
        case TOK_MAP: return "MAP";
        case TOK_NONE: return "NONE";
        case TOK_NOT: return "NOT";
        case TOK_ON: return "ON";
        case TOK_OR: return "OR";
        case TOK_OR_ELSE: return "OR_ELSE";
        case TOK_PANIC: return "PANIC";
        case TOK_PROC: return "PROC";
        case TOK_RANGE: return "RANGE";
        case TOK_RECORD: return "RECORD";
        case TOK_REF: return "REF";
        case TOK_RETURN: return "RETURN";
        case TOK_SOME: return "SOME";
        case TOK_STR_KW: return "STR_KW";
        case TOK_TRUE: return "TRUE";
        case TOK_TYPE: return "TYPE";
        case TOK_UINT_KW: return "UINT_KW";
        case TOK_UINT8_KW: return "UINT8_KW";
        case TOK_UINT16_KW: return "UINT16_KW";
        case TOK_UINT32_KW: return "UINT32_KW";
        case TOK_VAR: return "VAR";
        case TOK_VOID: return "VOID";
        case TOK_WHILE: return "WHILE";
        case TOK_WITH: return "WITH";
        case TOK_IDENT: return "IDENT";
        case TOK_PLUS: return "PLUS";
        case TOK_MINUS: return "MINUS";
        case TOK_STAR: return "STAR";
        case TOK_SLASH: return "SLASH";
        case TOK_PERCENT: return "PERCENT";
        case TOK_EQ: return "EQ";
        case TOK_NEQ: return "NEQ";
        case TOK_LT: return "LT";
        case TOK_GT: return "GT";
        case TOK_LEQ: return "LEQ";
        case TOK_GEQ: return "GEQ";
        case TOK_AMP: return "AMP";
        case TOK_PIPE: return "PIPE";
        case TOK_CARET: return "CARET";
        case TOK_TILDE: return "TILDE";
        case TOK_LSHIFT: return "LSHIFT";
        case TOK_RSHIFT: return "RSHIFT";
        case TOK_ASSIGN: return "ASSIGN";
        case TOK_PLUS_EQ: return "PLUS_EQ";
        case TOK_MINUS_EQ: return "MINUS_EQ";
        case TOK_STAR_EQ: return "STAR_EQ";
        case TOK_SLASH_EQ: return "SLASH_EQ";
        case TOK_PERCENT_EQ: return "PERCENT_EQ";
        case TOK_AMP_EQ: return "AMP_EQ";
        case TOK_PIPE_EQ: return "PIPE_EQ";
        case TOK_CARET_EQ: return "CARET_EQ";
        case TOK_LSHIFT_EQ: return "LSHIFT_EQ";
        case TOK_RSHIFT_EQ: return "RSHIFT_EQ";
        case TOK_ARROW: return "ARROW";
        case TOK_DOTDOT: return "DOTDOT";
        case TOK_DOTDOT_EQ: return "DOTDOT_EQ";
        case TOK_DOT: return "DOT";
        case TOK_COMMA: return "COMMA";
        case TOK_COLON: return "COLON";
        case TOK_QUESTION: return "QUESTION";
        case TOK_LPAREN: return "LPAREN";
        case TOK_RPAREN: return "RPAREN";
        case TOK_LBRACKET: return "LBRACKET";
        case TOK_RBRACKET: return "RBRACKET";
        case TOK_LBRACE: return "LBRACE";
        case TOK_RBRACE: return "RBRACE";
        case TOK_BACKSLASH: return "BACKSLASH";
        case TOK_NEWLINE: return "NEWLINE";
        case TOK_INDENT: return "INDENT";
        case TOK_DEDENT: return "DEDENT";
        case TOK_EOF: return "EOF";
        case TOK_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

void lexer_print_tokens(forge_lexer_t* lex) {
    printf("=== Tokens (%d total) ===\n", lex->tokens.len);

    for (int i = 0; i < lex->tokens.len; i++) {
        forge_token_t* tok = forge_token_array_get_ptr(&lex->tokens, i);
        printf("%4d:%-3d  %-14s", tok->line, tok->column, token_type_name(tok->type));

        switch (tok->type) {
            case TOK_INT_LIT:
                printf("  %lld", tok->val.int_val);
                break;
            case TOK_FLOAT_LIT:
                printf("  %g", tok->val.float_val);
                break;
            case TOK_STR_LIT:
            case TOK_RAW_STR_LIT:
                printf("  \"%s\"", tok->val.str_val);
                break;
            case TOK_IDENT:
                printf("  %s", tok->val.str_val);
                break;
            case TOK_NEWLINE:
            case TOK_INDENT:
            case TOK_DEDENT:
            case TOK_EOF:
                /* No extra info */
                break;
            default:
                if (tok->length > 0 && tok->length < 20) {
                    printf("  '%.*s'", tok->length, tok->start);
                }
                break;
        }

        printf("\n");
    }
}

