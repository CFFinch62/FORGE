/*
 * FORGE Language Toolchain
 * strtable.h - String interning table
 *
 * Every identifier and string literal is interned — stored once, referenced
 * by pointer. Pointer comparison then replaces string comparison throughout
 * the toolchain.
 *
 * Implementation: open-addressing hash table, FNV-1a hash, linear probing,
 * load factor <= 0.75, doubles capacity on resize.
 */

#ifndef FORGE_STRTABLE_H
#define FORGE_STRTABLE_H

#include "common.h"

/* ─────────────────────────────────────────────────────────────────────────────
 * String Table Entry
 * ───────────────────────────────────────────────────────────────────────────── */

typedef struct {
    char*    str;       /* Owned, null-terminated copy of the string */
    int      len;       /* Length excluding null terminator */
    uint32_t hash;      /* Cached hash value */
} forge_strtable_entry_t;

/* ─────────────────────────────────────────────────────────────────────────────
 * String Table
 * ───────────────────────────────────────────────────────────────────────────── */

struct forge_strtable {
    forge_strtable_entry_t* entries;    /* Array of entries */
    int                     capacity;   /* Total slots */
    int                     count;      /* Number of occupied slots */
};

/* ─────────────────────────────────────────────────────────────────────────────
 * Public API
 * ───────────────────────────────────────────────────────────────────────────── */

/*
 * Create a new string table with default initial capacity.
 * Returns NULL on allocation failure.
 */
forge_strtable_t* strtable_create(void);

/*
 * Intern a string. If the string already exists in the table, returns
 * the existing pointer. Otherwise, copies the string into the table
 * and returns the new pointer.
 *
 * The returned pointer is valid for the lifetime of the string table.
 * The input string does not need to be null-terminated if len is provided.
 *
 * Parameters:
 *   t   - The string table
 *   s   - The string to intern (not necessarily null-terminated)
 *   len - Length of the string in bytes
 *
 * Returns: Pointer to the interned string (always null-terminated)
 */
const char* strtable_intern(forge_strtable_t* t, const char* s, int len);

/*
 * Convenience function to intern a null-terminated string.
 */
const char* strtable_intern_cstr(forge_strtable_t* t, const char* s);

/*
 * Check if a string is already interned (without interning it).
 * Returns the interned pointer if found, NULL otherwise.
 */
const char* strtable_find(forge_strtable_t* t, const char* s, int len);

/*
 * Get the number of interned strings.
 */
int strtable_count(forge_strtable_t* t);

/*
 * Destroy the string table and free all interned strings.
 */
void strtable_destroy(forge_strtable_t* t);

/* ─────────────────────────────────────────────────────────────────────────────
 * Hash Function (exposed for testing)
 * ───────────────────────────────────────────────────────────────────────────── */

/*
 * FNV-1a hash function for strings.
 */
uint32_t strtable_hash(const char* s, int len);

#endif /* FORGE_STRTABLE_H */

