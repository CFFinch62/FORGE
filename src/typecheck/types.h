/*
 * FORGE Language Toolchain
 * types.h - Type representation for the type checker
 *
 * This module defines the compile-time type system. Each type is represented
 * by a forge_type_t struct allocated in the arena. Types are compared
 * structurally (except for records, which use nominal identity).
 */

#ifndef FORGE_TYPES_H
#define FORGE_TYPES_H

#include "common.h"
#include "util/memory.h"

/* ─────────────────────────────────────────────────────────────────────────────
 * Type Kinds
 * ───────────────────────────────────────────────────────────────────────────── */

typedef enum {
    /* Primitive types */
    TY_INT,         /* Default integer (64-bit signed) */
    TY_INT8,
    TY_INT16,
    TY_INT32,
    TY_UINT,        /* Default unsigned (64-bit) */
    TY_UINT8,
    TY_UINT16,
    TY_UINT32,
    TY_FLOAT,       /* Default float (64-bit double) */
    TY_FLOAT32,
    TY_BOOL,
    TY_STR,
    TY_BYTE,
    TY_VOID,

    /* Composite types */
    TY_OPTIONAL,    /* ?T - optional wrapping inner type */
    TY_FIXED_ARRAY, /* [N]T - fixed-size array */
    TY_DYN_ARRAY,   /* []T - dynamic array */
    TY_MAP,         /* map[K]V */
    TY_RECORD,      /* Named record type */
    TY_ALIAS,       /* Type alias */

    /* Special types */
    TY_NONE,        /* Type of the 'none' literal - compatible with any ?T */
    TY_UNRESOLVED,  /* Placeholder before resolution (for forward refs) */
    TY_ERROR,       /* Error sentinel - propagates through type checking */
    TY_ANY,         /* Any type - for builtin functions that accept anything */
} forge_type_kind_t;

/* ─────────────────────────────────────────────────────────────────────────────
 * Type Structure
 * ───────────────────────────────────────────────────────────────────────────── */

/* Note: forge_type_t is forward-declared in common.h */
struct forge_type {
    forge_type_kind_t kind;
    union {
        struct {
            forge_type_t* inner;        /* TY_OPTIONAL: inner type */
        } optional;

        struct {
            forge_type_t* elem_type;
            int           size;         /* TY_FIXED_ARRAY: array size */
        } fixed_array;

        struct {
            forge_type_t* elem_type;    /* TY_DYN_ARRAY: element type */
        } dyn_array;

        struct {
            forge_type_t* key_type;
            forge_type_t* val_type;     /* TY_MAP: key and value types */
        } map;

        struct {
            const char*     name;       /* Interned record type name */
            const char**    field_names;
            forge_type_t**  field_types;
            int             field_count;
        } record;

        struct {
            const char*   name;         /* Alias name */
            forge_type_t* target;       /* TY_ALIAS: target type */
        } alias;

        struct {
            const char* name;           /* TY_UNRESOLVED: type name to resolve */
        } unresolved;
    } as;  /* Named union for C99 compatibility */
};

/* ─────────────────────────────────────────────────────────────────────────────
 * Type Construction
 * ───────────────────────────────────────────────────────────────────────────── */

/* Create a primitive type (TY_INT, TY_BOOL, TY_STR, etc.) */
forge_type_t* type_prim(forge_arena_t* a, forge_type_kind_t kind);

/* Create an optional type ?T */
forge_type_t* type_optional(forge_arena_t* a, forge_type_t* inner);

/* Create a fixed-size array [N]T */
forge_type_t* type_fixed_array(forge_arena_t* a, forge_type_t* elem, int size);

/* Create a dynamic array []T */
forge_type_t* type_dyn_array(forge_arena_t* a, forge_type_t* elem);

/* Create a map type map[K]V */
forge_type_t* type_map(forge_arena_t* a, forge_type_t* key, forge_type_t* val);

/* Create a record type */
forge_type_t* type_record(forge_arena_t* a, const char* name,
                          const char** field_names, forge_type_t** field_types,
                          int field_count);

/* Create a type alias */
forge_type_t* type_alias(forge_arena_t* a, const char* name, forge_type_t* target);

/* Create an unresolved type (for forward references) */
forge_type_t* type_unresolved(forge_arena_t* a, const char* name);

/* Create the error sentinel type */
forge_type_t* type_error(forge_arena_t* a);

/* ─────────────────────────────────────────────────────────────────────────────
 * Type Predicates
 * ───────────────────────────────────────────────────────────────────────────── */

/* Structural equality (follows aliases) */
int type_equal(forge_type_t* a, forge_type_t* b);

/* Is this an integer type (int, int8, int16, int32, uint, uint8, etc.)? */
int type_is_integer(forge_type_t* t);

/* Is this a numeric type (integer or float)? */
int type_is_numeric(forge_type_t* t);

/* Can source be assigned to target? (same type, or source=none and target=?T) */
int type_is_assignable(forge_type_t* target, forge_type_t* source);

/* Is this the error sentinel? */
int type_is_error(forge_type_t* t);

/* ─────────────────────────────────────────────────────────────────────────────
 * Type Names (for error messages)
 * ───────────────────────────────────────────────────────────────────────────── */

/* Get a human-readable type name. Allocates; caller must free. */
char* type_to_str(forge_type_t* t);

/* Get the kind name (e.g., "int", "bool", "array") - static string, no free */
const char* type_kind_name(forge_type_kind_t kind);

#endif /* FORGE_TYPES_H */

