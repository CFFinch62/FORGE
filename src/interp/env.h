/*
 * FORGE Language Toolchain
 * env.h - Environment (scope) management
 *
 * An environment represents a lexical scope with name→value bindings.
 * Environments form a chain via parent pointers for nested scopes.
 */

#ifndef FORGE_ENV_H
#define FORGE_ENV_H

#include "common.h"
#include "interp/value.h"
#include "util/hashmap.h"

/* ─────────────────────────────────────────────────────────────────────────────
 * Environment Structure
 * ───────────────────────────────────────────────────────────────────────────── */

/* Note: forge_env_t is forward-declared in common.h */

struct forge_env {
    forge_hashmap_t*    bindings;   /* name (const char*) → forge_value_t* */
    forge_env_t*        parent;     /* Enclosing scope (NULL for global) */
};

/* ─────────────────────────────────────────────────────────────────────────────
 * Environment Lifecycle
 * ───────────────────────────────────────────────────────────────────────────── */

/*
 * Create a new environment with the given parent scope.
 * Pass NULL for the global/top-level environment.
 */
forge_env_t* env_create(forge_env_t* parent);

/*
 * Destroy an environment and free all its bindings.
 * Does NOT destroy the parent environment.
 * Frees all values stored in this environment.
 */
void env_destroy(forge_env_t* env);

/* ─────────────────────────────────────────────────────────────────────────────
 * Variable Binding
 * ───────────────────────────────────────────────────────────────────────────── */

/*
 * Define a new variable in the current scope.
 * The value is copied into the environment.
 * If the name already exists in THIS scope, it is overwritten.
 * Does NOT search parent scopes.
 */
void env_define(forge_env_t* env, const char* name, forge_value_t val);

/*
 * Alias for env_define (matches spec naming).
 */
#define env_set(env, name, val) env_define(env, name, val)

/* ─────────────────────────────────────────────────────────────────────────────
 * Variable Lookup
 * ───────────────────────────────────────────────────────────────────────────── */

/*
 * Look up a variable by name.
 * Searches current scope first, then walks up the parent chain.
 * Returns a COPY of the value if found.
 * Returns VAL_NONE if not found.
 */
forge_value_t env_get(forge_env_t* env, const char* name);

/*
 * Look up a variable, returning success/failure.
 * If found, copies the value to *out and returns 1.
 * If not found, returns 0 and *out is unchanged.
 */
int env_get_opt(forge_env_t* env, const char* name, forge_value_t* out);

/*
 * Check if a variable exists in this scope or any parent.
 */
int env_has(forge_env_t* env, const char* name);

/*
 * Check if a variable exists in THIS scope only (not parents).
 */
int env_has_local(forge_env_t* env, const char* name);

/* ─────────────────────────────────────────────────────────────────────────────
 * Variable Update
 * ───────────────────────────────────────────────────────────────────────────── */

/*
 * Update an existing variable.
 * Searches current scope first, then walks up the parent chain.
 * When found, frees the old value and stores a copy of the new value.
 * Returns 1 if variable was found and updated, 0 if not found.
 */
int env_update(forge_env_t* env, const char* name, forge_value_t val);

/* ─────────────────────────────────────────────────────────────────────────────
 * Direct Access (for ref parameters and assignments)
 * ───────────────────────────────────────────────────────────────────────────── */

/*
 * Get a direct pointer to a value in the environment.
 * Returns NULL if not found.
 * WARNING: The pointer may be invalidated by further env_define calls.
 */
forge_value_t* env_get_ptr(forge_env_t* env, const char* name);

/* ─────────────────────────────────────────────────────────────────────────────
 * Debugging
 * ───────────────────────────────────────────────────────────────────────────── */

/*
 * Print the environment for debugging.
 * Shows all bindings in current scope and optionally parent scopes.
 */
void env_print(forge_env_t* env, int include_parents);

/*
 * Get the number of bindings in this scope (not including parents).
 */
int env_count(forge_env_t* env);

#endif /* FORGE_ENV_H */

