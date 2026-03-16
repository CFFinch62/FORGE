/*
 * FORGE Language Toolchain
 * hashmap.c - Generic hash map implementation
 */

#include "util/hashmap.h"

/* ─────────────────────────────────────────────────────────────────────────────
 * Constants
 * ───────────────────────────────────────────────────────────────────────────── */

#define HASHMAP_INITIAL_CAPACITY    16
#define HASHMAP_MAX_LOAD            0.75

/* FNV-1a hash constants for 32-bit */
#define FNV_OFFSET_BASIS    0x811c9dc5
#define FNV_PRIME           0x01000193

/* Tombstone marker for deleted entries */
#define TOMBSTONE ((const char*)(uintptr_t)1)

/* ─────────────────────────────────────────────────────────────────────────────
 * Hash Function
 * ───────────────────────────────────────────────────────────────────────────── */

uint32_t hashmap_hash(const char* key) {
    uint32_t hash = FNV_OFFSET_BASIS;
    while (*key) {
        hash ^= (uint8_t)*key++;
        hash *= FNV_PRIME;
    }
    return hash;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Internal Helpers
 * ───────────────────────────────────────────────────────────────────────────── */

static int find_slot(forge_hashmap_t* map, const char* key, uint32_t hash) {
    int mask = map->capacity - 1;
    int index = hash & mask;
    int tombstone_idx = -1;
    
    for (;;) {
        forge_hashmap_entry_t* entry = &map->entries[index];
        
        if (entry->key == NULL) {
            /* Empty slot - return tombstone if we found one, else this slot */
            return (tombstone_idx != -1) ? tombstone_idx : index;
        }
        
        if (entry->key == TOMBSTONE) {
            /* Remember first tombstone for reuse */
            if (tombstone_idx == -1) {
                tombstone_idx = index;
            }
        } else if (entry->key == key) {
            /* Found exact key (pointer comparison for interned strings) */
            return index;
        } else if (entry->hash == hash && strcmp(entry->key, key) == 0) {
            /* Hash match + string comparison for non-interned keys */
            return index;
        }
        
        /* Linear probe */
        index = (index + 1) & mask;
    }
}

static void hashmap_resize(forge_hashmap_t* map) {
    int old_capacity = map->capacity;
    forge_hashmap_entry_t* old_entries = map->entries;
    
    map->capacity *= 2;
    map->entries = forge_calloc(map->capacity, sizeof(forge_hashmap_entry_t));
    map->count = 0;
    
    /* Rehash all existing entries (skip tombstones) */
    for (int i = 0; i < old_capacity; i++) {
        if (old_entries[i].key != NULL && old_entries[i].key != TOMBSTONE) {
            int slot = find_slot(map, old_entries[i].key, old_entries[i].hash);
            map->entries[slot] = old_entries[i];
            map->count++;
        }
    }
    
    forge_free(old_entries);
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Public API
 * ───────────────────────────────────────────────────────────────────────────── */

forge_hashmap_t* hashmap_create(void) {
    forge_hashmap_t* map = forge_malloc(sizeof(forge_hashmap_t));
    if (!map) return NULL;
    
    map->capacity = HASHMAP_INITIAL_CAPACITY;
    map->count = 0;
    map->entries = forge_calloc(map->capacity, sizeof(forge_hashmap_entry_t));
    
    if (!map->entries) {
        forge_free(map);
        return NULL;
    }
    
    return map;
}

void hashmap_destroy(forge_hashmap_t* map) {
    if (!map) return;
    forge_free(map->entries);
    forge_free(map);
}

void* hashmap_set(forge_hashmap_t* map, const char* key, void* value) {
    /* Check load factor */
    if (map->count + 1 > map->capacity * HASHMAP_MAX_LOAD) {
        hashmap_resize(map);
    }
    
    uint32_t hash = hashmap_hash(key);
    int slot = find_slot(map, key, hash);
    forge_hashmap_entry_t* entry = &map->entries[slot];
    
    void* old_value = NULL;
    
    if (entry->key != NULL && entry->key != TOMBSTONE) {
        /* Update existing entry */
        old_value = entry->value;
        entry->value = value;
    } else {
        /* New entry */
        entry->key = key;
        entry->value = value;
        entry->hash = hash;
        map->count++;
    }
    
    return old_value;
}

void* hashmap_get(forge_hashmap_t* map, const char* key) {
    if (map->count == 0) return NULL;
    
    uint32_t hash = hashmap_hash(key);
    int slot = find_slot(map, key, hash);
    forge_hashmap_entry_t* entry = &map->entries[slot];
    
    if (entry->key != NULL && entry->key != TOMBSTONE) {
        return entry->value;
    }
    return NULL;
}

int hashmap_has(forge_hashmap_t* map, const char* key) {
    if (map->count == 0) return 0;

    uint32_t hash = hashmap_hash(key);
    int slot = find_slot(map, key, hash);
    forge_hashmap_entry_t* entry = &map->entries[slot];

    return (entry->key != NULL && entry->key != TOMBSTONE);
}

void* hashmap_delete(forge_hashmap_t* map, const char* key) {
    if (map->count == 0) return NULL;

    uint32_t hash = hashmap_hash(key);
    int slot = find_slot(map, key, hash);
    forge_hashmap_entry_t* entry = &map->entries[slot];

    if (entry->key == NULL || entry->key == TOMBSTONE) {
        return NULL;
    }

    void* old_value = entry->value;
    entry->key = TOMBSTONE;
    entry->value = NULL;
    map->count--;

    return old_value;
}

int hashmap_count(forge_hashmap_t* map) {
    return map->count;
}

void hashmap_clear(forge_hashmap_t* map) {
    memset(map->entries, 0, map->capacity * sizeof(forge_hashmap_entry_t));
    map->count = 0;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Iteration API
 * ───────────────────────────────────────────────────────────────────────────── */

forge_hashmap_iter_t hashmap_iter(forge_hashmap_t* map) {
    forge_hashmap_iter_t iter;
    iter.map = map;
    iter.index = -1;  /* Will be incremented on first next() call */
    return iter;
}

int hashmap_iter_next(forge_hashmap_iter_t* iter) {
    forge_hashmap_t* map = iter->map;

    while (++iter->index < map->capacity) {
        forge_hashmap_entry_t* entry = &map->entries[iter->index];
        if (entry->key != NULL && entry->key != TOMBSTONE) {
            return 1;
        }
    }

    return 0;
}

const char* hashmap_iter_key(forge_hashmap_iter_t* iter) {
    return iter->map->entries[iter->index].key;
}

void* hashmap_iter_value(forge_hashmap_iter_t* iter) {
    return iter->map->entries[iter->index].value;
}

