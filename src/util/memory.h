/*
 * FORGE Language Toolchain
 * memory.h - Arena allocator and memory utilities
 *
 * The arena allocator provides efficient bulk allocation for AST nodes
 * and other parse-time data. It uses bump-pointer allocation within
 * linked blocks, making allocation O(1) and cleanup trivial.
 */

#ifndef FORGE_MEMORY_H
#define FORGE_MEMORY_H

#include "common.h"

/* ─────────────────────────────────────────────────────────────────────────────
 * Arena Block (Internal)
 * ───────────────────────────────────────────────────────────────────────────── */

#define ARENA_DEFAULT_BLOCK_SIZE (64 * 1024)  /* 64 KB per block */
#define ARENA_ALIGNMENT 8                      /* Align to 8 bytes (for 64-bit) */

typedef struct forge_arena_block {
    struct forge_arena_block* next;   /* Next block in linked list */
    size_t                    size;   /* Total size of this block's data area */
    size_t                    used;   /* Bytes used in this block */
    char                      data[]; /* Flexible array member - actual storage */
} forge_arena_block_t;

/* ─────────────────────────────────────────────────────────────────────────────
 * Arena Structure
 * ───────────────────────────────────────────────────────────────────────────── */

/* Note: forge_arena_t is forward-declared in common.h */
struct forge_arena {
    forge_arena_block_t* head;        /* First block (most recently allocated) */
    size_t               block_size;  /* Size for new blocks */
    size_t               total_allocated; /* Total bytes allocated across all blocks */
    size_t               total_used;  /* Total bytes used across all blocks */
};

/* ─────────────────────────────────────────────────────────────────────────────
 * Public API
 * ───────────────────────────────────────────────────────────────────────────── */

/*
 * Create a new arena with the specified initial block capacity.
 * Pass 0 for default block size (64KB).
 */
forge_arena_t* arena_create(size_t initial_capacity);

/*
 * Allocate `size` bytes from the arena.
 * Returns 8-byte aligned pointer. Never returns NULL (aborts on OOM).
 * Memory is zero-initialized.
 */
void* arena_alloc(forge_arena_t* arena, size_t size);

/*
 * Allocate space for `count` elements of `elem_size` bytes each.
 * Equivalent to arena_alloc(arena, count * elem_size) but clearer intent.
 */
void* arena_alloc_array(forge_arena_t* arena, size_t count, size_t elem_size);

/*
 * Destroy the arena and free all memory.
 * After this call, all pointers obtained from this arena are invalid.
 */
void arena_destroy(forge_arena_t* arena);

/*
 * Reset the arena for reuse.
 * Keeps all allocated blocks but marks them as empty.
 * Useful when parsing multiple files with the same arena.
 */
void arena_reset(forge_arena_t* arena);

/*
 * Get statistics about arena usage.
 */
size_t arena_bytes_allocated(forge_arena_t* arena);
size_t arena_bytes_used(forge_arena_t* arena);

/* ─────────────────────────────────────────────────────────────────────────────
 * Convenience Macros
 * ───────────────────────────────────────────────────────────────────────────── */

/* Allocate a single instance of a type */
#define ARENA_ALLOC(arena, type) \
    ((type*)arena_alloc((arena), sizeof(type)))

/* Allocate an array of N elements of a type */
#define ARENA_ALLOC_ARRAY(arena, type, n) \
    ((type*)arena_alloc_array((arena), (n), sizeof(type)))

/* Duplicate a string into the arena */
char* arena_strdup(forge_arena_t* arena, const char* str);

/* Duplicate a string with known length into the arena */
char* arena_strndup(forge_arena_t* arena, const char* str, size_t len);

#endif /* FORGE_MEMORY_H */

