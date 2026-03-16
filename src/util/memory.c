/*
 * FORGE Language Toolchain
 * memory.c - Arena allocator implementation
 */

#include "util/memory.h"

/* ─────────────────────────────────────────────────────────────────────────────
 * Internal Helpers
 * ───────────────────────────────────────────────────────────────────────────── */

/* Align size up to ARENA_ALIGNMENT boundary */
static inline size_t align_up(size_t size) {
    return (size + ARENA_ALIGNMENT - 1) & ~(ARENA_ALIGNMENT - 1);
}

/* Create a new block with the given data size */
static forge_arena_block_t* block_create(size_t data_size) {
    /* Allocate block header + data area */
    forge_arena_block_t* block = forge_malloc(sizeof(forge_arena_block_t) + data_size);
    block->next = NULL;
    block->size = data_size;
    block->used = 0;
    return block;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Public API
 * ───────────────────────────────────────────────────────────────────────────── */

forge_arena_t* arena_create(size_t initial_capacity) {
    forge_arena_t* arena = forge_malloc(sizeof(forge_arena_t));
    
    /* Use default if not specified */
    size_t block_size = initial_capacity > 0 ? initial_capacity : ARENA_DEFAULT_BLOCK_SIZE;
    
    arena->block_size = block_size;
    arena->head = block_create(block_size);
    arena->total_allocated = block_size;
    arena->total_used = 0;
    
    return arena;
}

void* arena_alloc(forge_arena_t* arena, size_t size) {
    if (size == 0) {
        size = 1;  /* Always allocate at least 1 byte */
    }
    
    size_t aligned_size = align_up(size);
    
    /* Check if current block has space */
    forge_arena_block_t* block = arena->head;
    
    if (block->used + aligned_size > block->size) {
        /* Need a new block */
        size_t new_block_size = arena->block_size;
        
        /* Handle oversized allocations */
        if (aligned_size > new_block_size) {
            new_block_size = aligned_size;
        }
        
        forge_arena_block_t* new_block = block_create(new_block_size);
        new_block->next = arena->head;
        arena->head = new_block;
        arena->total_allocated += new_block_size;
        block = new_block;
    }
    
    /* Bump-pointer allocation */
    void* ptr = block->data + block->used;
    block->used += aligned_size;
    arena->total_used += aligned_size;
    
    /* Zero-initialize */
    memset(ptr, 0, aligned_size);
    
    return ptr;
}

void* arena_alloc_array(forge_arena_t* arena, size_t count, size_t elem_size) {
    /* Check for overflow */
    if (count > 0 && elem_size > SIZE_MAX / count) {
        fprintf(stderr, "arena_alloc_array: allocation size overflow\n");
        abort();
    }
    return arena_alloc(arena, count * elem_size);
}

void arena_destroy(forge_arena_t* arena) {
    if (arena == NULL) return;
    
    /* Free all blocks */
    forge_arena_block_t* block = arena->head;
    while (block != NULL) {
        forge_arena_block_t* next = block->next;
        forge_free(block);
        block = next;
    }
    
    forge_free(arena);
}

void arena_reset(forge_arena_t* arena) {
    if (arena == NULL) return;
    
    /* Keep only the first block, free the rest */
    forge_arena_block_t* block = arena->head;
    
    /* Find the last (original) block */
    while (block->next != NULL) {
        forge_arena_block_t* to_free = block;
        block = block->next;
        arena->total_allocated -= to_free->size;
        forge_free(to_free);
    }
    
    /* Reset the remaining block */
    arena->head = block;
    block->used = 0;
    arena->total_used = 0;
}

size_t arena_bytes_allocated(forge_arena_t* arena) {
    return arena->total_allocated;
}

size_t arena_bytes_used(forge_arena_t* arena) {
    return arena->total_used;
}

char* arena_strdup(forge_arena_t* arena, const char* str) {
    if (str == NULL) return NULL;
    size_t len = strlen(str);
    return arena_strndup(arena, str, len);
}

char* arena_strndup(forge_arena_t* arena, const char* str, size_t len) {
    if (str == NULL) return NULL;
    char* copy = arena_alloc(arena, len + 1);
    memcpy(copy, str, len);
    copy[len] = '\0';
    return copy;
}

