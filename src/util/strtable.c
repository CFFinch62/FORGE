/*
 * FORGE Language Toolchain
 * strtable.c - String interning table implementation
 */

#include "util/strtable.h"

/* ─────────────────────────────────────────────────────────────────────────────
 * Constants
 * ───────────────────────────────────────────────────────────────────────────── */

#define STRTABLE_INITIAL_CAPACITY   64
#define STRTABLE_MAX_LOAD           0.75

/* FNV-1a hash constants for 32-bit */
#define FNV_OFFSET_BASIS    0x811c9dc5
#define FNV_PRIME           0x01000193

/* ─────────────────────────────────────────────────────────────────────────────
 * Hash Function
 * ───────────────────────────────────────────────────────────────────────────── */

uint32_t strtable_hash(const char* s, int len) {
    uint32_t hash = FNV_OFFSET_BASIS;
    for (int i = 0; i < len; i++) {
        hash ^= (uint8_t)s[i];
        hash *= FNV_PRIME;
    }
    return hash;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Internal Helpers
 * ───────────────────────────────────────────────────────────────────────────── */

static bool entry_matches(forge_strtable_entry_t* entry, const char* s, int len, uint32_t hash) {
    if (entry->str == NULL) return false;
    if (entry->hash != hash) return false;
    if (entry->len != len) return false;
    return memcmp(entry->str, s, len) == 0;
}

static int find_slot(forge_strtable_t* t, const char* s, int len, uint32_t hash) {
    int index = hash & (t->capacity - 1);  /* capacity is power of 2 */
    
    for (;;) {
        forge_strtable_entry_t* entry = &t->entries[index];
        
        if (entry->str == NULL) {
            /* Empty slot */
            return index;
        }
        
        if (entry_matches(entry, s, len, hash)) {
            /* Found existing entry */
            return index;
        }
        
        /* Linear probe to next slot */
        index = (index + 1) & (t->capacity - 1);
    }
}

static void strtable_resize(forge_strtable_t* t) {
    int old_capacity = t->capacity;
    forge_strtable_entry_t* old_entries = t->entries;
    
    t->capacity *= 2;
    t->entries = forge_calloc(t->capacity, sizeof(forge_strtable_entry_t));
    t->count = 0;
    
    /* Rehash all existing entries */
    for (int i = 0; i < old_capacity; i++) {
        if (old_entries[i].str != NULL) {
            int slot = find_slot(t, old_entries[i].str, old_entries[i].len, old_entries[i].hash);
            t->entries[slot] = old_entries[i];
            t->count++;
        }
    }
    
    forge_free(old_entries);
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Public API
 * ───────────────────────────────────────────────────────────────────────────── */

forge_strtable_t* strtable_create(void) {
    forge_strtable_t* t = forge_malloc(sizeof(forge_strtable_t));
    t->capacity = STRTABLE_INITIAL_CAPACITY;
    t->count = 0;
    t->entries = forge_calloc(t->capacity, sizeof(forge_strtable_entry_t));
    return t;
}

const char* strtable_intern(forge_strtable_t* t, const char* s, int len) {
    if (s == NULL || len < 0) return NULL;
    
    /* Check load factor and resize if needed */
    if ((double)(t->count + 1) / t->capacity > STRTABLE_MAX_LOAD) {
        strtable_resize(t);
    }
    
    uint32_t hash = strtable_hash(s, len);
    int slot = find_slot(t, s, len, hash);
    
    if (t->entries[slot].str != NULL) {
        /* Already interned */
        return t->entries[slot].str;
    }
    
    /* Create new entry */
    char* copy = forge_malloc(len + 1);
    memcpy(copy, s, len);
    copy[len] = '\0';
    
    t->entries[slot].str = copy;
    t->entries[slot].len = len;
    t->entries[slot].hash = hash;
    t->count++;
    
    return copy;
}

const char* strtable_intern_cstr(forge_strtable_t* t, const char* s) {
    if (s == NULL) return NULL;
    return strtable_intern(t, s, strlen(s));
}

const char* strtable_find(forge_strtable_t* t, const char* s, int len) {
    if (s == NULL || len < 0) return NULL;
    
    uint32_t hash = strtable_hash(s, len);
    int index = hash & (t->capacity - 1);
    
    for (;;) {
        forge_strtable_entry_t* entry = &t->entries[index];
        
        if (entry->str == NULL) return NULL;
        if (entry_matches(entry, s, len, hash)) return entry->str;
        
        index = (index + 1) & (t->capacity - 1);
    }
}

int strtable_count(forge_strtable_t* t) {
    return t->count;
}

void strtable_destroy(forge_strtable_t* t) {
    if (t == NULL) return;
    
    for (int i = 0; i < t->capacity; i++) {
        if (t->entries[i].str != NULL) {
            forge_free(t->entries[i].str);
        }
    }
    
    forge_free(t->entries);
    forge_free(t);
}

