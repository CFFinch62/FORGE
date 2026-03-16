/*
 * FORGE Language Toolchain
 * test_lexer.c - Unit tests for lexer
 */

#include <stdio.h>
#include <string.h>
#include "lexer/lexer.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) static void test_##name(void)
#define RUN_TEST(name) do { \
    printf("  %-40s", #name); \
    test_##name(); \
    printf(" PASS\n"); \
    tests_passed++; \
} while(0)

#define ASSERT(cond) do { \
    if (!(cond)) { \
        printf(" FAIL\n    Assertion failed: %s\n", #cond); \
        tests_failed++; \
        return; \
    } \
} while(0)

#define ASSERT_TOK(lex, idx, expected_type) do { \
    ASSERT((idx) < (lex)->tokens.len); \
    ASSERT(forge_token_array_get(&(lex)->tokens, (idx)).type == (expected_type)); \
} while(0)

/* Helper to create lexer, tokenize, and return it */
static forge_lexer_t* lex_string(const char* src, forge_strtable_t* st) {
    forge_lexer_t* lex = lexer_create(src, strlen(src), "test.fg", st);
    lexer_tokenize(lex);
    return lex;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Tests
 * ───────────────────────────────────────────────────────────────────────────── */

TEST(empty_source) {
    forge_strtable_t* st = strtable_create();
    forge_lexer_t* lex = lex_string("", st);
    ASSERT(lex->tokens.len == 1);
    ASSERT_TOK(lex, 0, TOK_EOF);
    lexer_destroy(lex);
    strtable_destroy(st);
}

TEST(single_char_tokens) {
    forge_strtable_t* st = strtable_create();
    forge_lexer_t* lex = lex_string("( ) [ ] { } , : ? ~", st);
    ASSERT_TOK(lex, 0, TOK_LPAREN);
    ASSERT_TOK(lex, 1, TOK_RPAREN);
    ASSERT_TOK(lex, 2, TOK_LBRACKET);
    ASSERT_TOK(lex, 3, TOK_RBRACKET);
    ASSERT_TOK(lex, 4, TOK_LBRACE);
    ASSERT_TOK(lex, 5, TOK_RBRACE);
    ASSERT_TOK(lex, 6, TOK_COMMA);
    ASSERT_TOK(lex, 7, TOK_COLON);
    ASSERT_TOK(lex, 8, TOK_QUESTION);
    ASSERT_TOK(lex, 9, TOK_TILDE);
    lexer_destroy(lex);
    strtable_destroy(st);
}

TEST(operators) {
    forge_strtable_t* st = strtable_create();
    forge_lexer_t* lex = lex_string("+ - * / % == != < > <= >=", st);
    ASSERT_TOK(lex, 0, TOK_PLUS);
    ASSERT_TOK(lex, 1, TOK_MINUS);
    ASSERT_TOK(lex, 2, TOK_STAR);
    ASSERT_TOK(lex, 3, TOK_SLASH);
    ASSERT_TOK(lex, 4, TOK_PERCENT);
    ASSERT_TOK(lex, 5, TOK_EQ);
    ASSERT_TOK(lex, 6, TOK_NEQ);
    ASSERT_TOK(lex, 7, TOK_LT);
    ASSERT_TOK(lex, 8, TOK_GT);
    ASSERT_TOK(lex, 9, TOK_LEQ);
    ASSERT_TOK(lex, 10, TOK_GEQ);
    lexer_destroy(lex);
    strtable_destroy(st);
}

TEST(compound_assign) {
    forge_strtable_t* st = strtable_create();
    forge_lexer_t* lex = lex_string("+= -= *= /= %= &= |= ^=", st);
    ASSERT_TOK(lex, 0, TOK_PLUS_EQ);
    ASSERT_TOK(lex, 1, TOK_MINUS_EQ);
    ASSERT_TOK(lex, 2, TOK_STAR_EQ);
    ASSERT_TOK(lex, 3, TOK_SLASH_EQ);
    ASSERT_TOK(lex, 4, TOK_PERCENT_EQ);
    ASSERT_TOK(lex, 5, TOK_AMP_EQ);
    ASSERT_TOK(lex, 6, TOK_PIPE_EQ);
    ASSERT_TOK(lex, 7, TOK_CARET_EQ);
    lexer_destroy(lex);
    strtable_destroy(st);
}

TEST(arrow_and_range) {
    forge_strtable_t* st = strtable_create();
    forge_lexer_t* lex = lex_string("-> .. ..= .", st);
    ASSERT_TOK(lex, 0, TOK_ARROW);
    ASSERT_TOK(lex, 1, TOK_DOTDOT);
    ASSERT_TOK(lex, 2, TOK_DOTDOT_EQ);
    ASSERT_TOK(lex, 3, TOK_DOT);
    lexer_destroy(lex);
    strtable_destroy(st);
}

TEST(shift_operators) {
    forge_strtable_t* st = strtable_create();
    forge_lexer_t* lex = lex_string("<< >> <<= >>=", st);
    ASSERT_TOK(lex, 0, TOK_LSHIFT);
    ASSERT_TOK(lex, 1, TOK_RSHIFT);
    ASSERT_TOK(lex, 2, TOK_LSHIFT_EQ);
    ASSERT_TOK(lex, 3, TOK_RSHIFT_EQ);
    lexer_destroy(lex);
    strtable_destroy(st);
}

TEST(integer_decimal) {
    forge_strtable_t* st = strtable_create();
    forge_lexer_t* lex = lex_string("42 0 123_456", st);
    ASSERT_TOK(lex, 0, TOK_INT_LIT);
    ASSERT(forge_token_array_get(&lex->tokens, 0).val.int_val == 42);
    ASSERT_TOK(lex, 1, TOK_INT_LIT);
    ASSERT(forge_token_array_get(&lex->tokens, 1).val.int_val == 0);
    ASSERT_TOK(lex, 2, TOK_INT_LIT);
    ASSERT(forge_token_array_get(&lex->tokens, 2).val.int_val == 123456);
    lexer_destroy(lex);
    strtable_destroy(st);
}

TEST(integer_hex_bin_oct) {
    forge_strtable_t* st = strtable_create();
    forge_lexer_t* lex = lex_string("0xFF 0b1010 0o77", st);
    ASSERT_TOK(lex, 0, TOK_INT_LIT);
    ASSERT(forge_token_array_get(&lex->tokens, 0).val.int_val == 255);
    ASSERT_TOK(lex, 1, TOK_INT_LIT);
    ASSERT(forge_token_array_get(&lex->tokens, 1).val.int_val == 10);
    ASSERT_TOK(lex, 2, TOK_INT_LIT);
    ASSERT(forge_token_array_get(&lex->tokens, 2).val.int_val == 63);
    lexer_destroy(lex);
    strtable_destroy(st);
}

TEST(float_literals) {
    forge_strtable_t* st = strtable_create();
    forge_lexer_t* lex = lex_string("3.14 0.5 1e10 2.5e-3", st);
    ASSERT_TOK(lex, 0, TOK_FLOAT_LIT);
    ASSERT(forge_token_array_get(&lex->tokens, 0).val.float_val > 3.13);
    ASSERT(forge_token_array_get(&lex->tokens, 0).val.float_val < 3.15);
    ASSERT_TOK(lex, 1, TOK_FLOAT_LIT);
    ASSERT_TOK(lex, 2, TOK_FLOAT_LIT);
    ASSERT_TOK(lex, 3, TOK_FLOAT_LIT);
    lexer_destroy(lex);
    strtable_destroy(st);
}

TEST(string_literal) {
    forge_strtable_t* st = strtable_create();
    forge_lexer_t* lex = lex_string("\"hello world\"", st);
    ASSERT_TOK(lex, 0, TOK_STR_LIT);
    ASSERT(strcmp(forge_token_array_get(&lex->tokens, 0).val.str_val, "hello world") == 0);
    lexer_destroy(lex);
    strtable_destroy(st);
}

TEST(string_escapes) {
    forge_strtable_t* st = strtable_create();
    forge_lexer_t* lex = lex_string("\"a\\nb\\tc\"", st);
    ASSERT_TOK(lex, 0, TOK_STR_LIT);
    ASSERT(strcmp(forge_token_array_get(&lex->tokens, 0).val.str_val, "a\nb\tc") == 0);
    lexer_destroy(lex);
    strtable_destroy(st);
}

TEST(raw_string) {
    forge_strtable_t* st = strtable_create();
    forge_lexer_t* lex = lex_string("`raw\\nstring`", st);
    ASSERT_TOK(lex, 0, TOK_RAW_STR_LIT);
    /* Raw string should NOT process escapes */
    ASSERT(strcmp(forge_token_array_get(&lex->tokens, 0).val.str_val, "raw\\nstring") == 0);
    lexer_destroy(lex);
    strtable_destroy(st);
}

TEST(keywords) {
    forge_strtable_t* st = strtable_create();
    forge_lexer_t* lex = lex_string("proc var if else while for return", st);
    ASSERT_TOK(lex, 0, TOK_PROC);
    ASSERT_TOK(lex, 1, TOK_VAR);
    ASSERT_TOK(lex, 2, TOK_IF);
    ASSERT_TOK(lex, 3, TOK_ELSE);
    ASSERT_TOK(lex, 4, TOK_WHILE);
    ASSERT_TOK(lex, 5, TOK_FOR);
    ASSERT_TOK(lex, 6, TOK_RETURN);
    lexer_destroy(lex);
    strtable_destroy(st);
}

TEST(identifiers) {
    forge_strtable_t* st = strtable_create();
    forge_lexer_t* lex = lex_string("foo bar_123 _private", st);
    ASSERT_TOK(lex, 0, TOK_IDENT);
    ASSERT(strcmp(forge_token_array_get(&lex->tokens, 0).val.str_val, "foo") == 0);
    ASSERT_TOK(lex, 1, TOK_IDENT);
    ASSERT_TOK(lex, 2, TOK_IDENT);
    lexer_destroy(lex);
    strtable_destroy(st);
}

TEST(comment) {
    forge_strtable_t* st = strtable_create();
    forge_lexer_t* lex = lex_string("x # this is a comment\ny", st);
    ASSERT_TOK(lex, 0, TOK_IDENT);
    ASSERT_TOK(lex, 1, TOK_NEWLINE);
    ASSERT_TOK(lex, 2, TOK_IDENT);
    lexer_destroy(lex);
    strtable_destroy(st);
}

TEST(indent_dedent_simple) {
    forge_strtable_t* st = strtable_create();
    const char* src = "if x:\n    y\nz";
    forge_lexer_t* lex = lex_string(src, st);
    /* if x : NEWLINE INDENT y NEWLINE DEDENT z EOF */
    ASSERT_TOK(lex, 0, TOK_IF);
    ASSERT_TOK(lex, 1, TOK_IDENT);
    ASSERT_TOK(lex, 2, TOK_COLON);
    ASSERT_TOK(lex, 3, TOK_NEWLINE);
    ASSERT_TOK(lex, 4, TOK_INDENT);
    ASSERT_TOK(lex, 5, TOK_IDENT);
    ASSERT_TOK(lex, 6, TOK_NEWLINE);
    ASSERT_TOK(lex, 7, TOK_DEDENT);
    ASSERT_TOK(lex, 8, TOK_IDENT);
    lexer_destroy(lex);
    strtable_destroy(st);
}

TEST(multiple_dedents) {
    forge_strtable_t* st = strtable_create();
    const char* src = "a:\n    b:\n        c\nd";
    forge_lexer_t* lex = lex_string(src, st);
    /* Should have 2 DEDENTs before 'd' */
    int dedent_count = 0;
    for (int i = 0; i < lex->tokens.len; i++) {
        if (forge_token_array_get(&lex->tokens, i).type == TOK_DEDENT) {
            dedent_count++;
        }
    }
    ASSERT(dedent_count == 2);
    lexer_destroy(lex);
    strtable_destroy(st);
}

TEST(implicit_continuation_parens) {
    forge_strtable_t* st = strtable_create();
    const char* src = "x = (1 +\n2)";
    forge_lexer_t* lex = lex_string(src, st);
    /* Should NOT have NEWLINE inside parens */
    int newline_count = 0;
    for (int i = 0; i < lex->tokens.len; i++) {
        if (forge_token_array_get(&lex->tokens, i).type == TOK_NEWLINE) {
            newline_count++;
        }
    }
    ASSERT(newline_count == 0);
    lexer_destroy(lex);
    strtable_destroy(st);
}

TEST(range_vs_float) {
    forge_strtable_t* st = strtable_create();
    /* 1..10 should be INT_LIT DOTDOT INT_LIT, not FLOAT */
    forge_lexer_t* lex = lex_string("1..10", st);
    ASSERT_TOK(lex, 0, TOK_INT_LIT);
    ASSERT_TOK(lex, 1, TOK_DOTDOT);
    ASSERT_TOK(lex, 2, TOK_INT_LIT);
    lexer_destroy(lex);
    strtable_destroy(st);
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Main
 * ───────────────────────────────────────────────────────────────────────────── */

int main(void) {
    printf("=== Lexer Unit Tests ===\n\n");

    RUN_TEST(empty_source);
    RUN_TEST(single_char_tokens);
    RUN_TEST(operators);
    RUN_TEST(compound_assign);
    RUN_TEST(arrow_and_range);
    RUN_TEST(shift_operators);
    RUN_TEST(integer_decimal);
    RUN_TEST(integer_hex_bin_oct);
    RUN_TEST(float_literals);
    RUN_TEST(string_literal);
    RUN_TEST(string_escapes);
    RUN_TEST(raw_string);
    RUN_TEST(keywords);
    RUN_TEST(identifiers);
    RUN_TEST(comment);
    RUN_TEST(indent_dedent_simple);
    RUN_TEST(multiple_dedents);
    RUN_TEST(implicit_continuation_parens);
    RUN_TEST(range_vs_float);

    printf("\n=== Results: %d passed, %d failed ===\n", tests_passed, tests_failed);

    return tests_failed > 0 ? 1 : 0;
}

