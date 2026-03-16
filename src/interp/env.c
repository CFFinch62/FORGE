/*
 * FORGE Language Toolchain
 * env.c - Environment (scope) implementation
 */

#include "interp/env.h"
#include <stdio.h>

/* ─────────────────────────────────────────────────────────────────────────────
 * Environment Lifecycle
 * ───────────────────────────────────────────────────────────────────────────── */

forge_env_t* env_create(forge_env_t* parent) {
    forge_env_t* env = forge_malloc(sizeof(forge_env_t));
    env->bindings = hashmap_create();
    env->parent = parent;
    return env;
}

void env_destroy(forge_env_t* env) {
    if (!env) return;
    
    /* Free all values in this scope */
    forge_hashmap_iter_t iter = hashmap_iter(env->bindings);
    while (hashmap_iter_next(&iter)) {
        forge_value_t* val = (forge_value_t*)hashmap_iter_value(&iter);
        if (val) {
            val_free(val);
            forge_free(val);
        }
    }
    
    hashmap_destroy(env->bindings);
    forge_free(env);
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Variable Binding
 * ───────────────────────────────────────────────────────────────────────────── */

void env_define(forge_env_t* env, const char* name, forge_value_t val) {
    /* Check if already exists in this scope - free old value */
    forge_value_t* existing = (forge_value_t*)hashmap_get(env->bindings, name);
    if (existing) {
        val_free(existing);
        *existing = val_copy(val);
    } else {
        /* Allocate new value on heap and store pointer */
        forge_value_t* heap_val = forge_malloc(sizeof(forge_value_t));
        *heap_val = val_copy(val);
        hashmap_set(env->bindings, name, heap_val);
    }
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Variable Lookup
 * ───────────────────────────────────────────────────────────────────────────── */

forge_value_t env_get(forge_env_t* env, const char* name) {
    forge_value_t result;
    if (env_get_opt(env, name, &result)) {
        return result;
    }
    return val_none();
}

int env_get_opt(forge_env_t* env, const char* name, forge_value_t* out) {
    forge_env_t* current = env;
    
    while (current) {
        forge_value_t* val = (forge_value_t*)hashmap_get(current->bindings, name);
        if (val) {
            *out = val_copy(*val);
            return 1;
        }
        current = current->parent;
    }
    
    return 0;
}

int env_has(forge_env_t* env, const char* name) {
    forge_env_t* current = env;
    
    while (current) {
        if (hashmap_has(current->bindings, name)) {
            return 1;
        }
        current = current->parent;
    }
    
    return 0;
}

int env_has_local(forge_env_t* env, const char* name) {
    return hashmap_has(env->bindings, name);
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Variable Update
 * ───────────────────────────────────────────────────────────────────────────── */

int env_update(forge_env_t* env, const char* name, forge_value_t val) {
    forge_env_t* current = env;
    
    while (current) {
        forge_value_t* existing = (forge_value_t*)hashmap_get(current->bindings, name);
        if (existing) {
            val_free(existing);
            *existing = val_copy(val);
            return 1;
        }
        current = current->parent;
    }
    
    return 0;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Direct Access
 * ───────────────────────────────────────────────────────────────────────────── */

forge_value_t* env_get_ptr(forge_env_t* env, const char* name) {
    forge_env_t* current = env;
    
    while (current) {
        forge_value_t* val = (forge_value_t*)hashmap_get(current->bindings, name);
        if (val) {
            return val;
        }
        current = current->parent;
    }
    
    return NULL;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Debugging
 * ───────────────────────────────────────────────────────────────────────────── */

void env_print(forge_env_t* env, int include_parents) {
    int depth = 0;
    forge_env_t* current = env;
    
    while (current) {
        printf("=== Scope %d (%d bindings) ===\n", depth, hashmap_count(current->bindings));
        
        forge_hashmap_iter_t iter = hashmap_iter(current->bindings);
        while (hashmap_iter_next(&iter)) {
            const char* name = hashmap_iter_key(&iter);
            forge_value_t* val = (forge_value_t*)hashmap_iter_value(&iter);
            char* str = val_to_str(*val);
            printf("  %s: %s (%s)\n", name, str, val_kind_name(val->kind));
            forge_free(str);
        }
        
        if (!include_parents) break;
        current = current->parent;
        depth++;
    }
}

int env_count(forge_env_t* env) {
    return hashmap_count(env->bindings);
}

