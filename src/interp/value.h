/*
 * FORGE Language Toolchain
 * value.h - Runtime value representation
 *
 * All FORGE values at runtime are represented by forge_value_t.
 * This is a tagged union that can hold any FORGE type.
 */

#ifndef FORGE_VALUE_H
#define FORGE_VALUE_H

#include "common.h"

/* ─────────────────────────────────────────────────────────────────────────────
 * Value Kind Enum
 * ───────────────────────────────────────────────────────────────────────────── */

typedef enum {
    VAL_INT,            /* Signed integer (int, i8, i16, i32, i64) */
    VAL_UINT,           /* Unsigned integer (uint, u8, u16, u32, u64) */
    VAL_FLOAT,          /* 64-bit float (float) */
    VAL_FLOAT32,        /* 32-bit float (f32) */
    VAL_BOOL,           /* Boolean (bool) */
    VAL_STR,            /* String (str) */
    VAL_BYTE,           /* Single byte (byte) */
    VAL_RECORD,         /* Record instance */
    VAL_ARRAY,          /* Array (fixed or dynamic) */
    VAL_MAP,            /* Map/dictionary */
    VAL_OPTIONAL,       /* Optional value (?T) */
    VAL_VOID,           /* Void (no value) */
    VAL_NONE,           /* None (absent optional) */
} forge_val_kind_t;

/* ─────────────────────────────────────────────────────────────────────────────
 * Value Structure
 * ───────────────────────────────────────────────────────────────────────────── */

/* Note: forge_value_t is forward-declared in common.h */

struct forge_value {
    forge_val_kind_t kind;
    union {
        /* Numeric types */
        long long           i;          /* VAL_INT, VAL_BYTE */
        unsigned long long  u;          /* VAL_UINT */
        double              f;          /* VAL_FLOAT */
        float               f32;        /* VAL_FLOAT32 */
        int                 b;          /* VAL_BOOL (0 or 1) */
        
        /* String */
        struct {
            char*   data;               /* Null-terminated string data */
            int     len;                /* Length excluding null terminator */
            int     owned;              /* 1 = heap-allocated (must free), 0 = literal */
        } str;
        
        /* Record instance */
        struct {
            forge_value_t*  fields;     /* Heap-allocated array of field values */
            int             count;      /* Number of fields */
            const char**    names;      /* Interned field name pointers */
        } record;
        
        /* Array (fixed or dynamic) */
        struct {
            forge_value_t*  elems;      /* Array elements */
            int             len;        /* Current length */
            int             cap;        /* Capacity (for dynamic arrays) */
            int             heap;       /* 1 = heap-allocated, must free */
        } array;
        
        /* Map (opaque pointer to forge_hashmap_t) */
        void*               map_ptr;
        
        /* Optional value */
        struct {
            int             present;    /* 0 = none, 1 = some */
            forge_value_t*  inner;      /* Heap-allocated inner value if present */
        } optional;
    } as;  /* Named union for C99 compatibility */
};

/* ─────────────────────────────────────────────────────────────────────────────
 * Value Constructors
 * ───────────────────────────────────────────────────────────────────────────── */

/* Primitive constructors */
forge_value_t val_int(long long i);
forge_value_t val_uint(unsigned long long u);
forge_value_t val_float(double f);
forge_value_t val_float32(float f);
forge_value_t val_bool(int b);
forge_value_t val_byte(unsigned char b);

/* String constructors */
forge_value_t val_str_lit(const char* s);           /* Not owned (literal) */
forge_value_t val_str_own(char* s, int len);        /* Owned (heap) */
forge_value_t val_str_copy(const char* s, int len); /* Copy to new heap string */

/* Void and optional */
forge_value_t val_void(void);
forge_value_t val_none(void);
forge_value_t val_some(forge_value_t inner);

/* Record constructor */
forge_value_t val_record(int field_count, const char** names, forge_value_t* fields);

/* Array constructors */
forge_value_t val_array(int len, int cap);          /* Empty array with capacity */
forge_value_t val_array_from(forge_value_t* elems, int len); /* Copy from array */

/* ─────────────────────────────────────────────────────────────────────────────
 * Value Operations
 * ───────────────────────────────────────────────────────────────────────────── */

/* Free heap resources (strings, records, arrays, optionals) */
void val_free(forge_value_t* v);

/* Deep copy a value (allocates new heap memory as needed) */
forge_value_t val_copy(forge_value_t v);

/* Check equality of two values */
int val_equal(forge_value_t a, forge_value_t b);

/* Convert value to string representation (caller must free result) */
char* val_to_str(forge_value_t v);

/* Get the kind name as a string (for debugging) */
const char* val_kind_name(forge_val_kind_t kind);

/* Check if value is "truthy" for conditionals */
int val_is_truthy(forge_value_t v);

/* ─────────────────────────────────────────────────────────────────────────────
 * Array Operations
 * ───────────────────────────────────────────────────────────────────────────── */

/* Append element to dynamic array (may resize) */
void val_array_push(forge_value_t* arr, forge_value_t elem);

/* Get element at index (returns VAL_NONE if out of bounds) */
forge_value_t val_array_get(forge_value_t* arr, int index);

/* Set element at index */
void val_array_set(forge_value_t* arr, int index, forge_value_t elem);

/* Get array length */
int val_array_len(forge_value_t* arr);

#endif /* FORGE_VALUE_H */

