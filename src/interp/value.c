/*
 * FORGE Language Toolchain
 * value.c - Runtime value implementation
 */

#include "interp/value.h"
#include <string.h>
#include <stdio.h>

/* ─────────────────────────────────────────────────────────────────────────────
 * Primitive Constructors
 * ───────────────────────────────────────────────────────────────────────────── */

forge_value_t val_int(long long i) {
    forge_value_t v;
    v.kind = VAL_INT;
    v.as.i = i;
    return v;
}

forge_value_t val_uint(unsigned long long u) {
    forge_value_t v;
    v.kind = VAL_UINT;
    v.as.u = u;
    return v;
}

forge_value_t val_float(double f) {
    forge_value_t v;
    v.kind = VAL_FLOAT;
    v.as.f = f;
    return v;
}

forge_value_t val_float32(float f) {
    forge_value_t v;
    v.kind = VAL_FLOAT32;
    v.as.f32 = f;
    return v;
}

forge_value_t val_bool(int b) {
    forge_value_t v;
    v.kind = VAL_BOOL;
    v.as.b = (b != 0) ? 1 : 0;
    return v;
}

forge_value_t val_byte(unsigned char b) {
    forge_value_t v;
    v.kind = VAL_BYTE;
    v.as.i = b;
    return v;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * String Constructors
 * ───────────────────────────────────────────────────────────────────────────── */

forge_value_t val_str_lit(const char* s) {
    forge_value_t v;
    v.kind = VAL_STR;
    v.as.str.data = (char*)s;  /* Cast away const - we promise not to modify */
    v.as.str.len = (int)strlen(s);
    v.as.str.owned = 0;
    return v;
}

forge_value_t val_str_own(char* s, int len) {
    forge_value_t v;
    v.kind = VAL_STR;
    v.as.str.data = s;
    v.as.str.len = len;
    v.as.str.owned = 1;
    return v;
}

forge_value_t val_str_copy(const char* s, int len) {
    forge_value_t v;
    v.kind = VAL_STR;
    v.as.str.data = forge_malloc(len + 1);
    memcpy(v.as.str.data, s, len);
    v.as.str.data[len] = '\0';
    v.as.str.len = len;
    v.as.str.owned = 1;
    return v;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Void and Optional Constructors
 * ───────────────────────────────────────────────────────────────────────────── */

forge_value_t val_void(void) {
    forge_value_t v;
    v.kind = VAL_VOID;
    return v;
}

forge_value_t val_none(void) {
    forge_value_t v;
    v.kind = VAL_NONE;
    return v;
}

forge_value_t val_some(forge_value_t inner) {
    forge_value_t v;
    v.kind = VAL_OPTIONAL;
    v.as.optional.present = 1;
    v.as.optional.inner = forge_malloc(sizeof(forge_value_t));
    *v.as.optional.inner = val_copy(inner);
    return v;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Record Constructor
 * ───────────────────────────────────────────────────────────────────────────── */

forge_value_t val_record(int field_count, const char** names, forge_value_t* fields) {
    forge_value_t v;
    v.kind = VAL_RECORD;
    v.as.record.count = field_count;
    
    /* Allocate and copy field names (pointers only - names are interned) */
    v.as.record.names = forge_malloc(field_count * sizeof(const char*));
    memcpy(v.as.record.names, names, field_count * sizeof(const char*));
    
    /* Allocate and deep-copy field values */
    v.as.record.fields = forge_malloc(field_count * sizeof(forge_value_t));
    for (int i = 0; i < field_count; i++) {
        v.as.record.fields[i] = val_copy(fields[i]);
    }
    
    return v;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Array Constructors
 * ───────────────────────────────────────────────────────────────────────────── */

forge_value_t val_array(int len, int cap) {
    forge_value_t v;
    v.kind = VAL_ARRAY;
    v.as.array.len = len;
    v.as.array.cap = (cap > len) ? cap : len;
    v.as.array.heap = 1;
    v.as.array.elems = forge_calloc(v.as.array.cap, sizeof(forge_value_t));
    return v;
}

forge_value_t val_array_from(forge_value_t* elems, int len) {
    forge_value_t v = val_array(len, len);
    for (int i = 0; i < len; i++) {
        v.as.array.elems[i] = val_copy(elems[i]);
    }
    return v;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Value Free
 * ───────────────────────────────────────────────────────────────────────────── */

void val_free(forge_value_t* v) {
    if (!v) return;

    switch (v->kind) {
        case VAL_STR:
            if (v->as.str.owned && v->as.str.data) {
                forge_free(v->as.str.data);
                v->as.str.data = NULL;
            }
            break;

        case VAL_RECORD:
            if (v->as.record.fields) {
                for (int i = 0; i < v->as.record.count; i++) {
                    val_free(&v->as.record.fields[i]);
                }
                forge_free(v->as.record.fields);
                v->as.record.fields = NULL;
            }
            if (v->as.record.names) {
                forge_free(v->as.record.names);
                v->as.record.names = NULL;
            }
            break;

        case VAL_ARRAY:
            if (v->as.array.heap && v->as.array.elems) {
                for (int i = 0; i < v->as.array.len; i++) {
                    val_free(&v->as.array.elems[i]);
                }
                forge_free(v->as.array.elems);
                v->as.array.elems = NULL;
            }
            break;

        case VAL_OPTIONAL:
            if (v->as.optional.present && v->as.optional.inner) {
                val_free(v->as.optional.inner);
                forge_free(v->as.optional.inner);
                v->as.optional.inner = NULL;
            }
            break;

        default:
            /* No heap resources for primitives */
            break;
    }

    v->kind = VAL_NONE;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Value Copy (Deep)
 * ───────────────────────────────────────────────────────────────────────────── */

forge_value_t val_copy(forge_value_t v) {
    forge_value_t copy;
    copy.kind = v.kind;

    switch (v.kind) {
        case VAL_INT:
        case VAL_BYTE:
            copy.as.i = v.as.i;
            break;

        case VAL_UINT:
            copy.as.u = v.as.u;
            break;

        case VAL_FLOAT:
            copy.as.f = v.as.f;
            break;

        case VAL_FLOAT32:
            copy.as.f32 = v.as.f32;
            break;

        case VAL_BOOL:
            copy.as.b = v.as.b;
            break;

        case VAL_STR:
            if (v.as.str.owned) {
                /* Deep copy owned strings */
                copy = val_str_copy(v.as.str.data, v.as.str.len);
            } else {
                /* Share literal strings */
                copy.as.str = v.as.str;
            }
            break;

        case VAL_RECORD:
            copy = val_record(v.as.record.count, v.as.record.names, v.as.record.fields);
            break;

        case VAL_ARRAY:
            copy = val_array_from(v.as.array.elems, v.as.array.len);
            break;

        case VAL_OPTIONAL:
            if (v.as.optional.present) {
                copy = val_some(*v.as.optional.inner);
            } else {
                copy = val_none();
                copy.kind = VAL_OPTIONAL;
                copy.as.optional.present = 0;
                copy.as.optional.inner = NULL;
            }
            break;

        case VAL_VOID:
        case VAL_NONE:
        default:
            /* No data to copy */
            break;
    }

    return copy;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Value Equality
 * ───────────────────────────────────────────────────────────────────────────── */

int val_equal(forge_value_t a, forge_value_t b) {
    if (a.kind != b.kind) return 0;

    switch (a.kind) {
        case VAL_INT:
        case VAL_BYTE:
            return a.as.i == b.as.i;

        case VAL_UINT:
            return a.as.u == b.as.u;

        case VAL_FLOAT:
            return a.as.f == b.as.f;

        case VAL_FLOAT32:
            return a.as.f32 == b.as.f32;

        case VAL_BOOL:
            return a.as.b == b.as.b;

        case VAL_STR:
            if (a.as.str.len != b.as.str.len) return 0;
            return memcmp(a.as.str.data, b.as.str.data, a.as.str.len) == 0;

        case VAL_RECORD:
            if (a.as.record.count != b.as.record.count) return 0;
            for (int i = 0; i < a.as.record.count; i++) {
                if (!val_equal(a.as.record.fields[i], b.as.record.fields[i])) {
                    return 0;
                }
            }
            return 1;

        case VAL_ARRAY:
            if (a.as.array.len != b.as.array.len) return 0;
            for (int i = 0; i < a.as.array.len; i++) {
                if (!val_equal(a.as.array.elems[i], b.as.array.elems[i])) {
                    return 0;
                }
            }
            return 1;

        case VAL_OPTIONAL:
            if (a.as.optional.present != b.as.optional.present) return 0;
            if (!a.as.optional.present) return 1;
            return val_equal(*a.as.optional.inner, *b.as.optional.inner);

        case VAL_VOID:
        case VAL_NONE:
            return 1;

        default:
            return 0;
    }
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Value Kind Name
 * ───────────────────────────────────────────────────────────────────────────── */

const char* val_kind_name(forge_val_kind_t kind) {
    switch (kind) {
        case VAL_INT:       return "int";
        case VAL_UINT:      return "uint";
        case VAL_FLOAT:     return "float";
        case VAL_FLOAT32:   return "f32";
        case VAL_BOOL:      return "bool";
        case VAL_STR:       return "str";
        case VAL_BYTE:      return "byte";
        case VAL_RECORD:    return "record";
        case VAL_ARRAY:     return "array";
        case VAL_MAP:       return "map";
        case VAL_OPTIONAL:  return "optional";
        case VAL_VOID:      return "void";
        case VAL_NONE:      return "none";
        default:            return "unknown";
    }
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Value Truthiness
 * ───────────────────────────────────────────────────────────────────────────── */

int val_is_truthy(forge_value_t v) {
    switch (v.kind) {
        case VAL_BOOL:
            return v.as.b != 0;
        case VAL_INT:
        case VAL_BYTE:
            return v.as.i != 0;
        case VAL_UINT:
            return v.as.u != 0;
        case VAL_FLOAT:
            return v.as.f != 0.0;
        case VAL_FLOAT32:
            return v.as.f32 != 0.0f;
        case VAL_STR:
            return v.as.str.len > 0;
        case VAL_ARRAY:
            return v.as.array.len > 0;
        case VAL_OPTIONAL:
            return v.as.optional.present;
        case VAL_NONE:
        case VAL_VOID:
            return 0;
        default:
            return 1;
    }
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Value to String
 * ───────────────────────────────────────────────────────────────────────────── */

char* val_to_str(forge_value_t v) {
    char* buf;
    int len;

    switch (v.kind) {
        case VAL_INT:
        case VAL_BYTE:
            buf = forge_malloc(32);
            snprintf(buf, 32, "%lld", v.as.i);
            return buf;

        case VAL_UINT:
            buf = forge_malloc(32);
            snprintf(buf, 32, "%llu", v.as.u);
            return buf;

        case VAL_FLOAT:
            buf = forge_malloc(32);
            snprintf(buf, 32, "%g", v.as.f);
            return buf;

        case VAL_FLOAT32:
            buf = forge_malloc(32);
            snprintf(buf, 32, "%g", (double)v.as.f32);
            return buf;

        case VAL_BOOL:
            buf = forge_malloc(8);
            strcpy(buf, v.as.b ? "true" : "false");
            return buf;

        case VAL_STR:
            buf = forge_malloc(v.as.str.len + 1);
            memcpy(buf, v.as.str.data, v.as.str.len);
            buf[v.as.str.len] = '\0';
            return buf;

        case VAL_VOID:
            buf = forge_malloc(8);
            strcpy(buf, "void");
            return buf;

        case VAL_NONE:
            buf = forge_malloc(8);
            strcpy(buf, "none");
            return buf;

        case VAL_OPTIONAL:
            if (v.as.optional.present) {
                char* inner = val_to_str(*v.as.optional.inner);
                len = (int)strlen(inner);
                buf = forge_malloc(len + 8);
                snprintf(buf, len + 8, "some(%s)", inner);
                forge_free(inner);
                return buf;
            } else {
                buf = forge_malloc(8);
                strcpy(buf, "none");
                return buf;
            }

        case VAL_ARRAY: {
            /* Estimate size: "[" + elements + ", " separators + "]" */
            int cap = 64;
            buf = forge_malloc(cap);
            int pos = 0;
            buf[pos++] = '[';

            for (int i = 0; i < v.as.array.len; i++) {
                char* elem = val_to_str(v.as.array.elems[i]);
                int elem_len = (int)strlen(elem);

                /* Resize if needed */
                while (pos + elem_len + 4 > cap) {
                    cap *= 2;
                    buf = forge_realloc(buf, cap);
                }

                if (i > 0) {
                    buf[pos++] = ',';
                    buf[pos++] = ' ';
                }
                memcpy(buf + pos, elem, elem_len);
                pos += elem_len;
                forge_free(elem);
            }
            buf[pos++] = ']';
            buf[pos] = '\0';
            return buf;
        }

        case VAL_RECORD: {
            int cap = 64;
            buf = forge_malloc(cap);
            int pos = 0;
            buf[pos++] = '{';

            for (int i = 0; i < v.as.record.count; i++) {
                const char* name = v.as.record.names[i];
                int name_len = (int)strlen(name);
                char* val = val_to_str(v.as.record.fields[i]);
                int val_len = (int)strlen(val);

                while (pos + name_len + val_len + 6 > cap) {
                    cap *= 2;
                    buf = forge_realloc(buf, cap);
                }

                if (i > 0) {
                    buf[pos++] = ',';
                    buf[pos++] = ' ';
                }
                memcpy(buf + pos, name, name_len);
                pos += name_len;
                buf[pos++] = ':';
                buf[pos++] = ' ';
                memcpy(buf + pos, val, val_len);
                pos += val_len;
                forge_free(val);
            }
            buf[pos++] = '}';
            buf[pos] = '\0';
            return buf;
        }

        case VAL_MAP:
            buf = forge_malloc(16);
            strcpy(buf, "<map>");
            return buf;

        default:
            buf = forge_malloc(16);
            strcpy(buf, "<unknown>");
            return buf;
    }
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Array Operations
 * ───────────────────────────────────────────────────────────────────────────── */

void val_array_push(forge_value_t* arr, forge_value_t elem) {
    if (arr->kind != VAL_ARRAY) return;

    /* Grow if needed */
    if (arr->as.array.len >= arr->as.array.cap) {
        int new_cap = (arr->as.array.cap == 0) ? 8 : arr->as.array.cap * 2;
        arr->as.array.elems = forge_realloc(arr->as.array.elems,
                                             new_cap * sizeof(forge_value_t));
        arr->as.array.cap = new_cap;
    }

    arr->as.array.elems[arr->as.array.len++] = val_copy(elem);
}

forge_value_t val_array_get(forge_value_t* arr, int index) {
    if (arr->kind != VAL_ARRAY) return val_none();
    if (index < 0 || index >= arr->as.array.len) return val_none();
    return val_copy(arr->as.array.elems[index]);
}

void val_array_set(forge_value_t* arr, int index, forge_value_t elem) {
    if (arr->kind != VAL_ARRAY) return;
    if (index < 0 || index >= arr->as.array.len) return;

    val_free(&arr->as.array.elems[index]);
    arr->as.array.elems[index] = val_copy(elem);
}

int val_array_len(forge_value_t* arr) {
    if (arr->kind != VAL_ARRAY) return 0;
    return arr->as.array.len;
}

