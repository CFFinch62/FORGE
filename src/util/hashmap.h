/*
 * FORGE Language Toolchain
 * hashmap.h - Generic hash map with string keys
 *
 * Open-addressing hash map with FNV-1a hashing, linear probing, and automatic
 * resizing at 0.75 load factor. Keys are interned strings (const char*).
 * Values are void* pointers.
 */

#ifndef FORGE_HASHMAP_H
#define FORGE_HASHMAP_H

#include "common.h"

/* ─────────────────────────────────────────────────────────────────────────────
 * Types
 * ───────────────────────────────────────────────────────────────────────────── */

/* Hash map entry */
typedef struct {
    const char* key;        /* Interned string key (NULL = empty slot) */
    void*       value;      /* User data pointer */
    uint32_t    hash;       /* Cached hash value */
} forge_hashmap_entry_t;

/* Hash map */
typedef struct {
    forge_hashmap_entry_t*  entries;
    int                     count;      /* Number of entries */
    int                     capacity;   /* Size of entries array (power of 2) */
} forge_hashmap_t;

/* Iterator for walking the map */
typedef struct {
    forge_hashmap_t*        map;
    int                     index;      /* Current index in entries array */
} forge_hashmap_iter_t;

/* ─────────────────────────────────────────────────────────────────────────────
 * Public API
 * ───────────────────────────────────────────────────────────────────────────── */

/*
 * Create a new hash map.
 * Returns NULL on allocation failure.
 */
forge_hashmap_t* hashmap_create(void);

/*
 * Destroy a hash map and free its memory.
 * Does NOT free the keys (assumed interned) or values (caller owns them).
 */
void hashmap_destroy(forge_hashmap_t* map);

/*
 * Insert or update a key-value pair.
 * Key must be a stable, interned string pointer.
 * Returns the previous value if updating, or NULL if new insertion.
 */
void* hashmap_set(forge_hashmap_t* map, const char* key, void* value);

/*
 * Look up a key.
 * Returns the value pointer, or NULL if not found.
 */
void* hashmap_get(forge_hashmap_t* map, const char* key);

/*
 * Check if a key exists.
 */
int hashmap_has(forge_hashmap_t* map, const char* key);

/*
 * Remove a key from the map.
 * Returns the removed value, or NULL if not found.
 * Note: Uses tombstone deletion to preserve probe chains.
 */
void* hashmap_delete(forge_hashmap_t* map, const char* key);

/*
 * Get the number of entries in the map.
 */
int hashmap_count(forge_hashmap_t* map);

/*
 * Clear all entries from the map.
 * Does NOT free keys or values.
 */
void hashmap_clear(forge_hashmap_t* map);

/* ─────────────────────────────────────────────────────────────────────────────
 * Iteration API
 * ───────────────────────────────────────────────────────────────────────────── */

/*
 * Initialize an iterator.
 */
forge_hashmap_iter_t hashmap_iter(forge_hashmap_t* map);

/*
 * Advance to the next entry.
 * Returns 1 if there is a next entry, 0 if iteration is complete.
 * After returning 1, iter->index points to a valid entry.
 */
int hashmap_iter_next(forge_hashmap_iter_t* iter);

/*
 * Get the key at the current iterator position.
 * Only valid after hashmap_iter_next() returns 1.
 */
const char* hashmap_iter_key(forge_hashmap_iter_t* iter);

/*
 * Get the value at the current iterator position.
 * Only valid after hashmap_iter_next() returns 1.
 */
void* hashmap_iter_value(forge_hashmap_iter_t* iter);

/* ─────────────────────────────────────────────────────────────────────────────
 * Hash Function (exported for testing/reuse)
 * ───────────────────────────────────────────────────────────────────────────── */

/*
 * Compute FNV-1a hash of a null-terminated string.
 */
uint32_t hashmap_hash(const char* key);

#endif /* FORGE_HASHMAP_H */

