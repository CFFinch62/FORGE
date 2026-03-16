/*
 * FORGE Language Toolchain
 * common.h - Shared types, macros, and forward declarations
 */

#ifndef FORGE_COMMON_H
#define FORGE_COMMON_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

/* ─────────────────────────────────────────────────────────────────────────────
 * Version
 * ───────────────────────────────────────────────────────────────────────────── */

#define FORGE_VERSION_MAJOR 0
#define FORGE_VERSION_MINOR 1
#define FORGE_VERSION_PATCH 0
#define FORGE_VERSION_STRING "0.1.0"

/* ─────────────────────────────────────────────────────────────────────────────
 * Common Macros
 * ───────────────────────────────────────────────────────────────────────────── */

#define FORGE_UNUSED(x) ((void)(x))

#define FORGE_ARRAY_LEN(arr) (sizeof(arr) / sizeof((arr)[0]))

#define FORGE_MAX(a, b) ((a) > (b) ? (a) : (b))
#define FORGE_MIN(a, b) ((a) < (b) ? (a) : (b))

/* ─────────────────────────────────────────────────────────────────────────────
 * Memory Allocation Wrappers
 * ───────────────────────────────────────────────────────────────────────────── */

static inline void* forge_malloc(size_t size) {
    void* ptr = malloc(size);
    if (!ptr && size > 0) {
        fprintf(stderr, "FORGE: Out of memory (requested %zu bytes)\n", size);
        exit(1);
    }
    return ptr;
}

static inline void* forge_calloc(size_t count, size_t size) {
    void* ptr = calloc(count, size);
    if (!ptr && count > 0 && size > 0) {
        fprintf(stderr, "FORGE: Out of memory (requested %zu bytes)\n", count * size);
        exit(1);
    }
    return ptr;
}

static inline void* forge_realloc(void* ptr, size_t size) {
    void* new_ptr = realloc(ptr, size);
    if (!new_ptr && size > 0) {
        fprintf(stderr, "FORGE: Out of memory (requested %zu bytes)\n", size);
        exit(1);
    }
    return new_ptr;
}

static inline void forge_free(void* ptr) {
    free(ptr);
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Forward Declarations
 * ───────────────────────────────────────────────────────────────────────────── */

/* String interning table */
typedef struct forge_strtable forge_strtable_t;

/* Lexer types */
typedef struct forge_lexer forge_lexer_t;
typedef struct forge_token forge_token_t;

/* Parser/AST types */
typedef struct forge_arena forge_arena_t;
typedef struct forge_node forge_node_t;
typedef struct forge_parser forge_parser_t;

/* Type system */
typedef struct forge_type forge_type_t;
typedef struct forge_checker forge_checker_t;

/* Interpreter types */
typedef struct forge_value forge_value_t;
typedef struct forge_env forge_env_t;
typedef struct forge_interp forge_interp_t;

#endif /* FORGE_COMMON_H */

