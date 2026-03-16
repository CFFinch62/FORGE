/*
 * FORGE Language Toolchain
 * dynarray.h - Generic growable dynamic array
 *
 * A simple, type-generic dynamic array implemented via macros.
 * Usage:
 *   DYNARRAY_DEFINE(int_array, int);
 *   int_array arr = {0};
 *   int_array_push(&arr, 42);
 *   int val = int_array_get(&arr, 0);
 *   int_array_free(&arr);
 */

#ifndef FORGE_DYNARRAY_H
#define FORGE_DYNARRAY_H

#include "common.h"

/* ─────────────────────────────────────────────────────────────────────────────
 * Generic Dynamic Array Macro
 * ───────────────────────────────────────────────────────────────────────────── */

#define DYNARRAY_INITIAL_CAPACITY 16

/*
 * Define a dynamic array type and its operations.
 * NAME: the name prefix for the type and functions
 * TYPE: the element type
 *
 * This creates:
 *   - struct NAME { TYPE* data; int len; int cap; }
 *   - void NAME_init(NAME* arr)
 *   - void NAME_push(NAME* arr, TYPE elem)
 *   - TYPE NAME_get(NAME* arr, int index)
 *   - TYPE* NAME_get_ptr(NAME* arr, int index)
 *   - void NAME_set(NAME* arr, int index, TYPE elem)
 *   - TYPE NAME_pop(NAME* arr)
 *   - void NAME_clear(NAME* arr)
 *   - void NAME_free(NAME* arr)
 */
#define DYNARRAY_DEFINE(NAME, TYPE) \
    typedef struct { \
        TYPE* data; \
        int len; \
        int cap; \
    } NAME; \
    \
    static inline void NAME##_init(NAME* arr) { \
        arr->data = NULL; \
        arr->len = 0; \
        arr->cap = 0; \
    } \
    \
    static inline void NAME##_ensure_capacity(NAME* arr, int needed) { \
        if (arr->cap >= needed) return; \
        int new_cap = arr->cap == 0 ? DYNARRAY_INITIAL_CAPACITY : arr->cap; \
        while (new_cap < needed) new_cap *= 2; \
        arr->data = (TYPE*)forge_realloc(arr->data, new_cap * sizeof(TYPE)); \
        arr->cap = new_cap; \
    } \
    \
    static inline void NAME##_push(NAME* arr, TYPE elem) { \
        NAME##_ensure_capacity(arr, arr->len + 1); \
        arr->data[arr->len++] = elem; \
    } \
    \
    static inline TYPE NAME##_get(NAME* arr, int index) { \
        assert(index >= 0 && index < arr->len); \
        return arr->data[index]; \
    } \
    \
    static inline TYPE* NAME##_get_ptr(NAME* arr, int index) { \
        assert(index >= 0 && index < arr->len); \
        return &arr->data[index]; \
    } \
    \
    static inline void NAME##_set(NAME* arr, int index, TYPE elem) { \
        assert(index >= 0 && index < arr->len); \
        arr->data[index] = elem; \
    } \
    \
    static inline TYPE NAME##_pop(NAME* arr) { \
        assert(arr->len > 0); \
        return arr->data[--arr->len]; \
    } \
    \
    static inline void NAME##_clear(NAME* arr) { \
        arr->len = 0; \
    } \
    \
    static inline void NAME##_free(NAME* arr) { \
        if (arr->data) forge_free(arr->data); \
        arr->data = NULL; \
        arr->len = 0; \
        arr->cap = 0; \
    } \
    \
    static inline int NAME##_len(NAME* arr) { \
        return arr->len; \
    } \
    \
    static inline bool NAME##_empty(NAME* arr) { \
        return arr->len == 0; \
    }

/* ─────────────────────────────────────────────────────────────────────────────
 * Pre-defined Common Array Types
 * ───────────────────────────────────────────────────────────────────────────── */

/* Integer array */
DYNARRAY_DEFINE(forge_int_array, int)

/* Pointer array (for generic usage) */
DYNARRAY_DEFINE(forge_ptr_array, void*)

#endif /* FORGE_DYNARRAY_H */

