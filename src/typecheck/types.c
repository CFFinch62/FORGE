/*
 * FORGE Language Toolchain
 * types.c - Type representation implementation
 */

#include "typecheck/types.h"
#include <stdio.h>
#include <string.h>

/* ─────────────────────────────────────────────────────────────────────────────
 * Type Construction
 * ───────────────────────────────────────────────────────────────────────────── */

forge_type_t* type_prim(forge_arena_t* a, forge_type_kind_t kind) {
    forge_type_t* t = ARENA_ALLOC(a, forge_type_t);
    t->kind = kind;
    return t;
}

forge_type_t* type_optional(forge_arena_t* a, forge_type_t* inner) {
    forge_type_t* t = ARENA_ALLOC(a, forge_type_t);
    t->kind = TY_OPTIONAL;
    t->as.optional.inner = inner;
    return t;
}

forge_type_t* type_fixed_array(forge_arena_t* a, forge_type_t* elem, int size) {
    forge_type_t* t = ARENA_ALLOC(a, forge_type_t);
    t->kind = TY_FIXED_ARRAY;
    t->as.fixed_array.elem_type = elem;
    t->as.fixed_array.size = size;
    return t;
}

forge_type_t* type_dyn_array(forge_arena_t* a, forge_type_t* elem) {
    forge_type_t* t = ARENA_ALLOC(a, forge_type_t);
    t->kind = TY_DYN_ARRAY;
    t->as.dyn_array.elem_type = elem;
    return t;
}

forge_type_t* type_map(forge_arena_t* a, forge_type_t* key, forge_type_t* val) {
    forge_type_t* t = ARENA_ALLOC(a, forge_type_t);
    t->kind = TY_MAP;
    t->as.map.key_type = key;
    t->as.map.val_type = val;
    return t;
}

forge_type_t* type_record(forge_arena_t* a, const char* name,
                          const char** field_names, forge_type_t** field_types,
                          int field_count) {
    forge_type_t* t = ARENA_ALLOC(a, forge_type_t);
    t->kind = TY_RECORD;
    t->as.record.name = name;
    t->as.record.field_names = field_names;
    t->as.record.field_types = field_types;
    t->as.record.field_count = field_count;
    return t;
}

forge_type_t* type_alias(forge_arena_t* a, const char* name, forge_type_t* target) {
    forge_type_t* t = ARENA_ALLOC(a, forge_type_t);
    t->kind = TY_ALIAS;
    t->as.alias.name = name;
    t->as.alias.target = target;
    return t;
}

forge_type_t* type_unresolved(forge_arena_t* a, const char* name) {
    forge_type_t* t = ARENA_ALLOC(a, forge_type_t);
    t->kind = TY_UNRESOLVED;
    t->as.unresolved.name = name;
    return t;
}

forge_type_t* type_error(forge_arena_t* a) {
    forge_type_t* t = ARENA_ALLOC(a, forge_type_t);
    t->kind = TY_ERROR;
    return t;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Helper: Resolve aliases to their underlying type
 * ───────────────────────────────────────────────────────────────────────────── */

static forge_type_t* type_resolve(forge_type_t* t) {
    while (t && t->kind == TY_ALIAS) {
        t = t->as.alias.target;
    }
    return t;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Type Predicates
 * ───────────────────────────────────────────────────────────────────────────── */

int type_equal(forge_type_t* a, forge_type_t* b) {
    if (!a || !b) return 0;

    /* Resolve aliases */
    a = type_resolve(a);
    b = type_resolve(b);

    if (a->kind != b->kind) return 0;

    switch (a->kind) {
        /* Primitives: kind equality is sufficient */
        case TY_INT: case TY_INT8: case TY_INT16: case TY_INT32:
        case TY_UINT: case TY_UINT8: case TY_UINT16: case TY_UINT32:
        case TY_FLOAT: case TY_FLOAT32:
        case TY_BOOL: case TY_STR: case TY_BYTE: case TY_VOID:
        case TY_NONE: case TY_ERROR: case TY_ANY:
            return 1;

        case TY_OPTIONAL:
            return type_equal(a->as.optional.inner, b->as.optional.inner);

        case TY_FIXED_ARRAY:
            return a->as.fixed_array.size == b->as.fixed_array.size &&
                   type_equal(a->as.fixed_array.elem_type, b->as.fixed_array.elem_type);

        case TY_DYN_ARRAY:
            return type_equal(a->as.dyn_array.elem_type, b->as.dyn_array.elem_type);

        case TY_MAP:
            return type_equal(a->as.map.key_type, b->as.map.key_type) &&
                   type_equal(a->as.map.val_type, b->as.map.val_type);

        case TY_RECORD:
            /* Records use nominal equality - same name means same type */
            return a->as.record.name == b->as.record.name ||
                   (a->as.record.name && b->as.record.name &&
                    strcmp(a->as.record.name, b->as.record.name) == 0);

        case TY_ALIAS:
            /* Should have been resolved above */
            return 0;

        case TY_UNRESOLVED:
            /* Unresolved types are never equal */
            return 0;
    }

    return 0;
}

int type_is_integer(forge_type_t* t) {
    if (!t) return 0;
    t = type_resolve(t);
    switch (t->kind) {
        case TY_INT: case TY_INT8: case TY_INT16: case TY_INT32:
        case TY_UINT: case TY_UINT8: case TY_UINT16: case TY_UINT32:
        case TY_BYTE:
            return 1;
        default:
            return 0;
    }
}

int type_is_numeric(forge_type_t* t) {
    if (!t) return 0;
    t = type_resolve(t);
    return type_is_integer(t) ||
           t->kind == TY_FLOAT ||
           t->kind == TY_FLOAT32;
}

int type_is_assignable(forge_type_t* target, forge_type_t* source) {
    if (!target || !source) return 0;

    target = type_resolve(target);
    source = type_resolve(source);

    /* TY_ANY accepts any type (for builtin functions) */
    if (target->kind == TY_ANY) return 1;

    /* A source of TY_ANY is assignable to any target (e.g. return value of append) */
    if (source->kind == TY_ANY) return 1;

    /* Same type: always assignable */
    if (type_equal(target, source)) return 1;

    /* 'none' is assignable to any optional type */
    if (source->kind == TY_NONE && target->kind == TY_OPTIONAL) {
        return 1;
    }

    /* T is assignable to ?T (implicit wrapping) */
    if (target->kind == TY_OPTIONAL) {
        return type_equal(target->as.optional.inner, source);
    }

    return 0;
}

int type_is_error(forge_type_t* t) {
    if (!t) return 0;
    return type_resolve(t)->kind == TY_ERROR;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Type Names
 * ───────────────────────────────────────────────────────────────────────────── */

const char* type_kind_name(forge_type_kind_t kind) {
    switch (kind) {
        case TY_INT:        return "int";
        case TY_INT8:       return "int8";
        case TY_INT16:      return "int16";
        case TY_INT32:      return "int32";
        case TY_UINT:       return "uint";
        case TY_UINT8:      return "uint8";
        case TY_UINT16:     return "uint16";
        case TY_UINT32:     return "uint32";
        case TY_FLOAT:      return "float";
        case TY_FLOAT32:    return "float32";
        case TY_BOOL:       return "bool";
        case TY_STR:        return "str";
        case TY_BYTE:       return "byte";
        case TY_VOID:       return "void";
        case TY_OPTIONAL:   return "optional";
        case TY_FIXED_ARRAY: return "array";
        case TY_DYN_ARRAY:  return "array";
        case TY_MAP:        return "map";
        case TY_RECORD:     return "record";
        case TY_ALIAS:      return "alias";
        case TY_NONE:       return "none";
        case TY_UNRESOLVED: return "unresolved";
        case TY_ERROR:      return "error";
        case TY_ANY:        return "any";
    }
    return "unknown";
}

char* type_to_str(forge_type_t* t) {
    if (!t) {
        char* s = forge_malloc(5);
        strcpy(s, "null");
        return s;
    }

    char buf[256];

    switch (t->kind) {
        case TY_INT: case TY_INT8: case TY_INT16: case TY_INT32:
        case TY_UINT: case TY_UINT8: case TY_UINT16: case TY_UINT32:
        case TY_FLOAT: case TY_FLOAT32:
        case TY_BOOL: case TY_STR: case TY_BYTE: case TY_VOID:
        case TY_NONE: case TY_ERROR: case TY_ANY:
            snprintf(buf, sizeof(buf), "%s", type_kind_name(t->kind));
            break;

        case TY_OPTIONAL: {
            char* inner = type_to_str(t->as.optional.inner);
            snprintf(buf, sizeof(buf), "?%s", inner);
            forge_free(inner);
            break;
        }

        case TY_FIXED_ARRAY: {
            char* elem = type_to_str(t->as.fixed_array.elem_type);
            snprintf(buf, sizeof(buf), "[%d]%s", t->as.fixed_array.size, elem);
            forge_free(elem);
            break;
        }

        case TY_DYN_ARRAY: {
            char* elem = type_to_str(t->as.dyn_array.elem_type);
            snprintf(buf, sizeof(buf), "[]%s", elem);
            forge_free(elem);
            break;
        }

        case TY_MAP: {
            char* key = type_to_str(t->as.map.key_type);
            char* val = type_to_str(t->as.map.val_type);
            snprintf(buf, sizeof(buf), "map[%s]%s", key, val);
            forge_free(key);
            forge_free(val);
            break;
        }

        case TY_RECORD:
            snprintf(buf, sizeof(buf), "%s", t->as.record.name ? t->as.record.name : "record");
            break;

        case TY_ALIAS:
            snprintf(buf, sizeof(buf), "%s", t->as.alias.name ? t->as.alias.name : "alias");
            break;

        case TY_UNRESOLVED:
            snprintf(buf, sizeof(buf), "<%s>", t->as.unresolved.name ? t->as.unresolved.name : "?");
            break;
    }

    size_t len = strlen(buf) + 1;
    char* result = forge_malloc(len);
    memcpy(result, buf, len);
    return result;
}

