/*
 * FORGE Runtime Library Implementation
 */

#define _DEFAULT_SOURCE          /* For usleep() */
#define _POSIX_C_SOURCE 200809L  /* For getline, ssize_t */

#include "forge_runtime.h"
#include <stdarg.h>
#include <unistd.h>  /* access(), F_OK */
#include <math.h>    /* math functions */
#include <time.h>    /* for random seeding */
#include <limits.h>  /* INT64_MIN */

/* Global argc/argv for runtime access */
static int g_argc = 0;
static char** g_argv = NULL;

/* ═══════════════════════════════════════════════════════════════════════════
 * Runtime Initialization
 * ═══════════════════════════════════════════════════════════════════════════ */

void forge_runtime_init(int argc, char** argv) {
    g_argc = argc;
    g_argv = argv;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * String Operations
 * ═══════════════════════════════════════════════════════════════════════════ */

forge_str_t forge_str_lit(const char* s) {
    forge_str_t str;
    str.data = (char*)s;  /* Cast away const — we won't modify or free it */
    str.len = s ? (int)strlen(s) : 0;
    str.owned = 0;
    return str;
}

forge_str_t forge_str_dup(const char* s, int len) {
    forge_str_t str;
    str.data = malloc(len + 1);
    if (str.data) {
        memcpy(str.data, s, len);
        str.data[len] = '\0';
    }
    str.len = len;
    str.owned = 1;
    return str;
}

forge_str_t forge_str_concat(forge_str_t a, forge_str_t b) {
    forge_str_t result;
    result.len = a.len + b.len;
    result.data = malloc(result.len + 1);
    result.owned = 1;

    if (result.data) {
        memcpy(result.data, a.data, a.len);
        memcpy(result.data + a.len, b.data, b.len);
        result.data[result.len] = '\0';
    }
    return result;
}

/* Pointer-based wrapper for LLVM ABI compatibility */
forge_str_t forge_str_concat_ptr(forge_str_t* a, forge_str_t* b) {
    return forge_str_concat(*a, *b);
}

void forge_str_free(forge_str_t* s) {
    if (s && s->owned && s->data) {
        free(s->data);
        s->data = NULL;
        s->len = 0;
        s->owned = 0;
    }
}

forge_str_t forge_str_from_int(int64_t i) {
    char buf[32];
    int len = snprintf(buf, sizeof(buf), "%lld", (long long)i);
    return forge_str_dup(buf, len);
}

forge_str_t forge_str_from_float(double f) {
    char buf[64];
    int len = snprintf(buf, sizeof(buf), "%g", f);
    return forge_str_dup(buf, len);
}

forge_str_t forge_str_from_bool(int b) {
    return forge_str_lit(b ? "true" : "false");
}

int64_t forge_str_to_int(forge_str_t s) {
    /* Need to null-terminate for strtoll */
    char buf[64];
    int len = s.len < 63 ? s.len : 63;
    memcpy(buf, s.data, len);
    buf[len] = '\0';
    return strtoll(buf, NULL, 10);
}

double forge_str_to_float(forge_str_t s) {
    /* Need to null-terminate for strtod */
    char buf[64];
    int len = s.len < 63 ? s.len : 63;
    memcpy(buf, s.data, len);
    buf[len] = '\0';
    return strtod(buf, NULL);
}

int64_t forge_str_len(forge_str_t s) {
    return s.len;
}

/* Pointer-based wrapper for LLVM ABI compatibility */
int64_t forge_str_len_ptr(forge_str_t* s) {
    return s->len;
}

int forge_str_equal(forge_str_t a, forge_str_t b) {
    if (a.len != b.len) return 0;
    return memcmp(a.data, b.data, a.len) == 0;
}

/* Pointer-based wrapper for LLVM ABI compatibility */
int forge_str_equal_ptr(forge_str_t* a, forge_str_t* b) {
    return forge_str_equal(*a, *b);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Dynamic Array Operations
 * ═══════════════════════════════════════════════════════════════════════════ */

forge_array_t forge_array_create(size_t elem_size, int initial_cap) {
    forge_array_t a;
    a.elem_size = elem_size;
    a.len = 0;
    a.cap = initial_cap > 0 ? initial_cap : 8;
    a.data = malloc(a.cap * elem_size);
    return a;
}

void forge_array_push(forge_array_t* a, const void* elem) {
    if (a->len >= a->cap) {
        a->cap = a->cap * 2;
        a->data = realloc(a->data, a->cap * a->elem_size);
    }
    memcpy((char*)a->data + a->len * a->elem_size, elem, a->elem_size);
    a->len++;
}

void* forge_array_get(forge_array_t* a, int index) {
    if (index < 0 || index >= a->len) {
        FORGE_PANIC("Array index out of bounds");
    }
    return (char*)a->data + index * a->elem_size;
}

void forge_array_set(forge_array_t* a, int index, const void* elem) {
    if (index < 0 || index >= a->len) {
        FORGE_PANIC("Array index out of bounds");
    }
    memcpy((char*)a->data + index * a->elem_size, elem, a->elem_size);
}

int forge_array_len(forge_array_t* a) {
    return a->len;
}

void forge_array_free(forge_array_t* a) {
    if (a && a->data) {
        free(a->data);
        a->data = NULL;
        a->len = 0;
        a->cap = 0;
    }
}

forge_array_t forge_array_from_ints(int64_t* vals, int count) {
    forge_array_t arr = forge_array_create(sizeof(int64_t), count > 0 ? count : 1);
    for (int i = 0; i < count; i++) {
        forge_array_push(&arr, &vals[i]);
    }
    return arr;
}

forge_array_t forge_array_from_floats(double* vals, int count) {
    forge_array_t arr = forge_array_create(sizeof(double), count > 0 ? count : 1);
    for (int i = 0; i < count; i++) {
        forge_array_push(&arr, &vals[i]);
    }
    return arr;
}

forge_array_t forge_array_from_bools(int* vals, int count) {
    forge_array_t arr = forge_array_create(sizeof(int), count > 0 ? count : 1);
    for (int i = 0; i < count; i++) {
        forge_array_push(&arr, &vals[i]);
    }
    return arr;
}

forge_array_t forge_array_from_strs(forge_str_t* vals, int count) {
    forge_array_t arr = forge_array_create(sizeof(forge_str_t), count > 0 ? count : 1);
    for (int i = 0; i < count; i++) {
        forge_array_push(&arr, &vals[i]);
    }
    return arr;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Map Operations
 * ═══════════════════════════════════════════════════════════════════════════ */

#define MAP_INITIAL_CAP 16
#define MAP_LOAD_FACTOR 0.75

typedef struct {
    void*    key;
    void*    val;
    int      occupied;
    uint32_t hash;
} forge_map_entry_t;

struct forge_map {
    forge_map_entry_t* entries;
    int                cap;
    int                len;
    size_t             key_size;
    size_t             val_size;
    int              (*key_equal)(const void*, const void*);
    uint32_t         (*key_hash)(const void*);
};

forge_map_t* forge_map_create(size_t key_size, size_t val_size,
                               int (*key_equal)(const void*, const void*),
                               uint32_t (*key_hash)(const void*)) {
    forge_map_t* m = malloc(sizeof(forge_map_t));
    if (!m) return NULL;

    m->cap = MAP_INITIAL_CAP;
    m->len = 0;
    m->key_size = key_size;
    m->val_size = val_size;
    m->key_equal = key_equal;
    m->key_hash = key_hash;
    m->entries = calloc(m->cap, sizeof(forge_map_entry_t));

    return m;
}

static void forge_map_resize(forge_map_t* m) {
    int old_cap = m->cap;
    forge_map_entry_t* old_entries = m->entries;

    m->cap = old_cap * 2;
    m->entries = calloc(m->cap, sizeof(forge_map_entry_t));
    m->len = 0;

    for (int i = 0; i < old_cap; i++) {
        if (old_entries[i].occupied) {
            forge_map_set(m, old_entries[i].key, old_entries[i].val);
            free(old_entries[i].key);
            free(old_entries[i].val);
        }
    }
    free(old_entries);
}

void forge_map_set(forge_map_t* m, const void* key, const void* val) {
    if ((double)m->len / m->cap >= MAP_LOAD_FACTOR) {
        forge_map_resize(m);
    }

    uint32_t hash = m->key_hash(key);
    int idx = hash % m->cap;

    while (m->entries[idx].occupied) {
        if (m->entries[idx].hash == hash &&
            m->key_equal(m->entries[idx].key, key)) {
            /* Update existing */
            memcpy(m->entries[idx].val, val, m->val_size);
            return;
        }
        idx = (idx + 1) % m->cap;
    }

    /* Insert new */
    m->entries[idx].key = malloc(m->key_size);
    m->entries[idx].val = malloc(m->val_size);
    memcpy(m->entries[idx].key, key, m->key_size);
    memcpy(m->entries[idx].val, val, m->val_size);
    m->entries[idx].occupied = 1;
    m->entries[idx].hash = hash;
    m->len++;
}

int forge_map_get(forge_map_t* m, const void* key, void* val_out) {
    uint32_t hash = m->key_hash(key);
    int idx = hash % m->cap;
    int start = idx;

    while (m->entries[idx].occupied) {
        if (m->entries[idx].hash == hash &&
            m->key_equal(m->entries[idx].key, key)) {
            if (val_out) {
                memcpy(val_out, m->entries[idx].val, m->val_size);
            }
            return 1;
        }
        idx = (idx + 1) % m->cap;
        if (idx == start) break;
    }
    return 0;
}

int forge_map_has(forge_map_t* m, const void* key) {
    return forge_map_get(m, key, NULL);
}

void forge_map_delete(forge_map_t* m, const void* key) {
    uint32_t hash = m->key_hash(key);
    int idx = hash % m->cap;
    int start = idx;

    while (m->entries[idx].occupied) {
        if (m->entries[idx].hash == hash &&
            m->key_equal(m->entries[idx].key, key)) {
            free(m->entries[idx].key);
            free(m->entries[idx].val);
            m->entries[idx].occupied = 0;
            m->len--;
            return;
        }
        idx = (idx + 1) % m->cap;
        if (idx == start) break;
    }
}

int forge_map_len(forge_map_t* m) {
    return m->len;
}

void forge_map_free(forge_map_t* m) {
    if (!m) return;
    for (int i = 0; i < m->cap; i++) {
        if (m->entries[i].occupied) {
            free(m->entries[i].key);
            free(m->entries[i].val);
        }
    }
    free(m->entries);
    free(m);
}

/* Common key helpers */
int forge_str_key_equal(const void* a, const void* b) {
    const forge_str_t* sa = a;
    const forge_str_t* sb = b;
    return forge_str_equal(*sa, *sb);
}

uint32_t forge_str_key_hash(const void* a) {
    const forge_str_t* s = a;
    /* FNV-1a hash */
    uint32_t hash = 2166136261u;
    for (int i = 0; i < s->len; i++) {
        hash ^= (unsigned char)s->data[i];
        hash *= 16777619u;
    }
    return hash;
}

int forge_int_key_equal(const void* a, const void* b) {
    return *(const int64_t*)a == *(const int64_t*)b;
}

uint32_t forge_int_key_hash(const void* a) {
    int64_t val = *(const int64_t*)a;
    /* Simple integer hash */
    return (uint32_t)(val ^ (val >> 32));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Error Handling
 * ═══════════════════════════════════════════════════════════════════════════ */

void forge_panic(const char* msg, const char* file, int line) {
    fprintf(stderr, "FORGE panic at %s:%d: %s\n", file, line, msg);
    exit(1);
}

void forge_assert(int cond, const char* msg, const char* file, int line) {
    if (!cond) {
        forge_panic(msg, file, line);
    }
}

int64_t forge_div_check(int64_t a, int64_t b, const char* file, int line) {
    if (b == 0) {
        fprintf(stderr, "Runtime error: Division by zero\n  at %s:%d\n", file, line);
        exit(1);
    }
    return a / b;
}

int64_t forge_mod_check(int64_t a, int64_t b, const char* file, int line) {
    if (b == 0) {
        fprintf(stderr, "Runtime error: Division by zero\n  at %s:%d\n", file, line);
        exit(1);
    }
    return a % b;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * I/O Builtins
 * ═══════════════════════════════════════════════════════════════════════════ */

void forge_print(forge_str_t s) {
    if (s.data && s.len > 0) {
        fwrite(s.data, 1, s.len, stdout);
    }
    putchar('\n');
}

void forge_eprint(forge_str_t s) {
    if (s.data && s.len > 0) {
        fwrite(s.data, 1, s.len, stderr);
    }
    fputc('\n', stderr);
}

forge_str_t forge_read_line(void) {
    char* line = NULL;
    size_t cap = 0;
    ssize_t len = getline(&line, &cap, stdin);

    if (len < 0) {
        free(line);
        return forge_str_lit("");
    }

    /* Strip trailing newline */
    if (len > 0 && line[len - 1] == '\n') {
        line[len - 1] = '\0';
        len--;
    }

    forge_str_t result;
    result.data = line;
    result.len = (int)len;
    result.owned = 1;
    return result;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Standard Library: forge.io
 * ═══════════════════════════════════════════════════════════════════════════ */

void forge_io_print(forge_str_t s) {
    if (s.data && s.len > 0) {
        fwrite(s.data, 1, s.len, stdout);
    }
    putchar('\n');
}

void forge_io_print_raw(forge_str_t s) {
    if (s.data && s.len > 0) {
        fwrite(s.data, 1, s.len, stdout);
    }
    fflush(stdout);
}

void forge_io_eprint(forge_str_t s) {
    if (s.data && s.len > 0) {
        fwrite(s.data, 1, s.len, stderr);
    }
    fputc('\n', stderr);
}

forge_str_t forge_io_read_line(void) {
    return forge_read_line();  /* Reuse existing implementation */
}

forge_str_t forge_io_read_line_prompt(forge_str_t prompt) {
    if (prompt.data && prompt.len > 0) {
        fwrite(prompt.data, 1, prompt.len, stdout);
        fflush(stdout);
    }
    return forge_read_line();
}

int forge_io_file_exists(forge_str_t path) {
    /* Need null-terminated path for access() */
    char* path_z = malloc(path.len + 1);
    memcpy(path_z, path.data, path.len);
    path_z[path.len] = '\0';

    int exists = (access(path_z, F_OK) == 0);
    free(path_z);
    return exists;
}

forge_str_t forge_io_read_file(forge_str_t path) {
    char* path_z = malloc(path.len + 1);
    memcpy(path_z, path.data, path.len);
    path_z[path.len] = '\0';

    FILE* f = fopen(path_z, "rb");
    free(path_z);

    if (!f) {
        return forge_str_lit("");
    }

    /* Get file size */
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* buf = malloc(size + 1);
    fread(buf, 1, size, f);
    buf[size] = '\0';
    fclose(f);

    forge_str_t result;
    result.data = buf;
    result.len = (int)size;
    result.owned = 1;
    return result;
}

int forge_io_write_file(forge_str_t path, forge_str_t content) {
    char* path_z = malloc(path.len + 1);
    memcpy(path_z, path.data, path.len);
    path_z[path.len] = '\0';

    FILE* f = fopen(path_z, "wb");
    free(path_z);

    if (!f) return 0;

    fwrite(content.data, 1, content.len, f);
    fclose(f);
    return 1;
}

int forge_io_append_file(forge_str_t path, forge_str_t content) {
    char* path_z = malloc(path.len + 1);
    memcpy(path_z, path.data, path.len);
    path_z[path.len] = '\0';

    FILE* f = fopen(path_z, "ab");
    free(path_z);

    if (!f) return 0;

    fwrite(content.data, 1, content.len, f);
    fclose(f);
    return 1;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Standard Library: forge.str
 * ═══════════════════════════════════════════════════════════════════════════ */

int64_t forge_str_len_fn(forge_str_t s) {
    return (int64_t)s.len;
}

int forge_str_contains(forge_str_t s, forge_str_t substr) {
    if (substr.len == 0) return 1;
    if (substr.len > s.len) return 0;

    for (int i = 0; i <= s.len - substr.len; i++) {
        if (memcmp(s.data + i, substr.data, substr.len) == 0) {
            return 1;
        }
    }
    return 0;
}

int forge_str_starts_with(forge_str_t s, forge_str_t prefix) {
    if (prefix.len > s.len) return 0;
    return memcmp(s.data, prefix.data, prefix.len) == 0;
}

int forge_str_ends_with(forge_str_t s, forge_str_t suffix) {
    if (suffix.len > s.len) return 0;
    return memcmp(s.data + s.len - suffix.len, suffix.data, suffix.len) == 0;
}

int64_t forge_str_find(forge_str_t s, forge_str_t substr) {
    if (substr.len == 0) return 0;
    if (substr.len > s.len) return -1;

    for (int i = 0; i <= s.len - substr.len; i++) {
        if (memcmp(s.data + i, substr.data, substr.len) == 0) {
            return (int64_t)i;
        }
    }
    return -1;
}

int64_t forge_str_count(forge_str_t s, forge_str_t substr) {
    if (substr.len == 0) return 0;
    if (substr.len > s.len) return 0;

    int64_t count = 0;
    for (int i = 0; i <= s.len - substr.len; i++) {
        if (memcmp(s.data + i, substr.data, substr.len) == 0) {
            count++;
            i += substr.len - 1;  /* Skip past this occurrence */
        }
    }
    return count;
}

forge_str_t forge_str_upper(forge_str_t s) {
    char* buf = malloc(s.len + 1);
    for (int i = 0; i < s.len; i++) {
        unsigned char c = (unsigned char)s.data[i];
        buf[i] = (c >= 'a' && c <= 'z') ? (c - 'a' + 'A') : c;
    }
    buf[s.len] = '\0';

    forge_str_t result;
    result.data = buf;
    result.len = s.len;
    result.owned = 1;
    return result;
}

forge_str_t forge_str_lower(forge_str_t s) {
    char* buf = malloc(s.len + 1);
    for (int i = 0; i < s.len; i++) {
        unsigned char c = (unsigned char)s.data[i];
        buf[i] = (c >= 'A' && c <= 'Z') ? (c - 'A' + 'a') : c;
    }
    buf[s.len] = '\0';

    forge_str_t result;
    result.data = buf;
    result.len = s.len;
    result.owned = 1;
    return result;
}

static int is_whitespace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

forge_str_t forge_str_trim(forge_str_t s) {
    int start = 0;
    int end = s.len;

    while (start < end && is_whitespace(s.data[start])) start++;
    while (end > start && is_whitespace(s.data[end - 1])) end--;

    int new_len = end - start;
    char* buf = malloc(new_len + 1);
    memcpy(buf, s.data + start, new_len);
    buf[new_len] = '\0';

    forge_str_t result;
    result.data = buf;
    result.len = new_len;
    result.owned = 1;
    return result;
}

forge_str_t forge_str_trim_left(forge_str_t s) {
    int start = 0;
    while (start < s.len && is_whitespace(s.data[start])) start++;

    int new_len = s.len - start;
    char* buf = malloc(new_len + 1);
    memcpy(buf, s.data + start, new_len);
    buf[new_len] = '\0';

    forge_str_t result;
    result.data = buf;
    result.len = new_len;
    result.owned = 1;
    return result;
}

forge_str_t forge_str_trim_right(forge_str_t s) {
    int end = s.len;
    while (end > 0 && is_whitespace(s.data[end - 1])) end--;

    char* buf = malloc(end + 1);
    memcpy(buf, s.data, end);
    buf[end] = '\0';

    forge_str_t result;
    result.data = buf;
    result.len = end;
    result.owned = 1;
    return result;
}

forge_str_t forge_str_replace(forge_str_t s, forge_str_t old_str, forge_str_t new_str) {
    if (old_str.len == 0) return forge_str_dup(s.data, s.len);

    /* Count occurrences to calculate result size */
    int count = 0;
    for (int i = 0; i <= s.len - old_str.len; i++) {
        if (memcmp(s.data + i, old_str.data, old_str.len) == 0) {
            count++;
            i += old_str.len - 1;
        }
    }

    if (count == 0) return forge_str_dup(s.data, s.len);

    int new_len = s.len + count * (new_str.len - old_str.len);
    char* buf = malloc(new_len + 1);
    char* out = buf;

    for (int i = 0; i < s.len; ) {
        if (i <= s.len - old_str.len &&
            memcmp(s.data + i, old_str.data, old_str.len) == 0) {
            memcpy(out, new_str.data, new_str.len);
            out += new_str.len;
            i += old_str.len;
        } else {
            *out++ = s.data[i++];
        }
    }
    *out = '\0';

    forge_str_t result;
    result.data = buf;
    result.len = new_len;
    result.owned = 1;
    return result;
}

forge_str_t forge_str_substr(forge_str_t s, int64_t start, int64_t length) {
    if (start < 0) start = 0;
    if (start >= s.len) {
        return forge_str_lit("");
    }
    if (length < 0 || start + length > s.len) {
        length = s.len - start;
    }

    char* buf = malloc(length + 1);
    memcpy(buf, s.data + start, length);
    buf[length] = '\0';

    forge_str_t result;
    result.data = buf;
    result.len = (int)length;
    result.owned = 1;
    return result;
}

forge_array_t forge_str_split(forge_str_t s, forge_str_t delim) {
    forge_array_t arr = forge_array_create(sizeof(forge_str_t), 4);

    if (delim.len == 0) {
        /* Split into individual characters */
        for (int i = 0; i < s.len; i++) {
            forge_str_t ch = forge_str_substr(s, i, 1);
            forge_array_push(&arr, &ch);
        }
        return arr;
    }

    int start = 0;
    for (int i = 0; i <= s.len - delim.len; i++) {
        if (memcmp(s.data + i, delim.data, delim.len) == 0) {
            forge_str_t part = forge_str_substr(s, start, i - start);
            forge_array_push(&arr, &part);
            i += delim.len - 1;
            start = i + 1;
        }
    }

    /* Add remaining part */
    forge_str_t part = forge_str_substr(s, start, s.len - start);
    forge_array_push(&arr, &part);

    return arr;
}

forge_str_t forge_str_join(forge_array_t* arr, forge_str_t sep) {
    if (arr->len == 0) return forge_str_lit("");

    /* Calculate total length */
    int64_t total = 0;
    for (int64_t i = 0; i < arr->len; i++) {
        forge_str_t* elem = (forge_str_t*)((char*)arr->data + i * arr->elem_size);
        total += elem->len;
        if (i > 0) total += sep.len;
    }

    char* buf = malloc(total + 1);
    char* out = buf;

    for (int64_t i = 0; i < arr->len; i++) {
        if (i > 0) {
            memcpy(out, sep.data, sep.len);
            out += sep.len;
        }
        forge_str_t* elem = (forge_str_t*)((char*)arr->data + i * arr->elem_size);
        memcpy(out, elem->data, elem->len);
        out += elem->len;
    }
    *out = '\0';

    forge_str_t result;
    result.data = buf;
    result.len = (int)total;
    result.owned = 1;
    return result;
}

forge_str_t forge_str_repeat(forge_str_t s, int64_t n) {
    if (n <= 0) return forge_str_lit("");

    int64_t new_len = s.len * n;
    char* buf = malloc(new_len + 1);

    for (int64_t i = 0; i < n; i++) {
        memcpy(buf + i * s.len, s.data, s.len);
    }
    buf[new_len] = '\0';

    forge_str_t result;
    result.data = buf;
    result.len = (int)new_len;
    result.owned = 1;
    return result;
}

forge_str_t forge_str_reverse(forge_str_t s) {
    char* buf = malloc(s.len + 1);
    for (int i = 0; i < s.len; i++) {
        buf[i] = s.data[s.len - 1 - i];
    }
    buf[s.len] = '\0';

    forge_str_t result;
    result.data = buf;
    result.len = s.len;
    result.owned = 1;
    return result;
}

forge_str_t forge_str_char_at(forge_str_t s, int64_t index) {
    if (index < 0 || index >= s.len) {
        return forge_str_lit("");
    }
    return forge_str_substr(s, index, 1);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * LLVM ABI-Compatible Wrappers
 * These take forge_str_t by pointer to avoid struct-passing ABI issues
 * ═══════════════════════════════════════════════════════════════════════════ */

int forge_str_contains_ptr(forge_str_t* s, forge_str_t* substr) {
    return forge_str_contains(*s, *substr);
}

int forge_str_starts_with_ptr(forge_str_t* s, forge_str_t* prefix) {
    return forge_str_starts_with(*s, *prefix);
}

int forge_str_ends_with_ptr(forge_str_t* s, forge_str_t* suffix) {
    return forge_str_ends_with(*s, *suffix);
}

int64_t forge_str_find_ptr(forge_str_t* s, forge_str_t* substr) {
    return forge_str_find(*s, *substr);
}

int64_t forge_str_count_ptr(forge_str_t* s, forge_str_t* substr) {
    return forge_str_count(*s, *substr);
}

forge_str_t forge_str_upper_ptr(forge_str_t* s) {
    return forge_str_upper(*s);
}

forge_str_t forge_str_lower_ptr(forge_str_t* s) {
    return forge_str_lower(*s);
}

forge_str_t forge_str_trim_ptr(forge_str_t* s) {
    return forge_str_trim(*s);
}

forge_str_t forge_str_trim_left_ptr(forge_str_t* s) {
    return forge_str_trim_left(*s);
}

forge_str_t forge_str_trim_right_ptr(forge_str_t* s) {
    return forge_str_trim_right(*s);
}

forge_str_t forge_str_substr_ptr(forge_str_t* s, int64_t start, int64_t length) {
    return forge_str_substr(*s, start, length);
}

forge_str_t forge_str_replace_ptr(forge_str_t* s, forge_str_t* old_str, forge_str_t* new_str) {
    return forge_str_replace(*s, *old_str, *new_str);
}

forge_str_t forge_str_repeat_ptr(forge_str_t* s, int64_t n) {
    return forge_str_repeat(*s, n);
}

forge_str_t forge_str_reverse_ptr(forge_str_t* s) {
    return forge_str_reverse(*s);
}

forge_str_t forge_str_char_at_ptr(forge_str_t* s, int64_t index) {
    return forge_str_char_at(*s, index);
}

forge_array_t forge_str_split_ptr(forge_str_t* s, forge_str_t* delim) {
    return forge_str_split(*s, *delim);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Standard Library: forge.math
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Track whether random has been seeded */
static int g_random_seeded = 0;

/* Internal helper to ensure random is seeded */
static void ensure_random_seeded(void) {
    if (!g_random_seeded) {
        srand((unsigned int)time(NULL));
        g_random_seeded = 1;
    }
}

/* Absolute value */
double forge_math_abs(double x) {
    return fabs(x);
}

int64_t forge_math_abs_int(int64_t x) {
    return x < 0 ? -x : x;
}

/* Min/Max */
double forge_math_min(double a, double b) {
    return a < b ? a : b;
}

double forge_math_max(double a, double b) {
    return a > b ? a : b;
}

int64_t forge_math_min_int(int64_t a, int64_t b) {
    return a < b ? a : b;
}

int64_t forge_math_max_int(int64_t a, int64_t b) {
    return a > b ? a : b;
}

/* Clamp */
double forge_math_clamp(double val, double lo, double hi) {
    if (val < lo) return lo;
    if (val > hi) return hi;
    return val;
}

/* Power and roots */
double forge_math_pow(double base, double exp) {
    return pow(base, exp);
}

double forge_math_sqrt(double x) {
    return sqrt(x);
}

double forge_math_cbrt(double x) {
    return cbrt(x);
}

/* Rounding */
double forge_math_floor(double x) {
    return floor(x);
}

double forge_math_ceil(double x) {
    return ceil(x);
}

double forge_math_round(double x) {
    return round(x);
}

double forge_math_trunc(double x) {
    return trunc(x);
}

/* Trigonometry */
double forge_math_sin(double x) {
    return sin(x);
}

double forge_math_cos(double x) {
    return cos(x);
}

double forge_math_tan(double x) {
    return tan(x);
}

double forge_math_atan2(double y, double x) {
    return atan2(y, x);
}

/* Logarithms and exponentials */
double forge_math_log(double x) {
    return log(x);
}

double forge_math_log10(double x) {
    return log10(x);
}

double forge_math_log2(double x) {
    return log2(x);
}

double forge_math_exp(double x) {
    return exp(x);
}

/* Random numbers */
int64_t forge_math_random_int(int64_t lo, int64_t hi) {
    ensure_random_seeded();
    if (hi <= lo) return lo;
    /* Generate random in range [lo, hi) */
    int64_t range = hi - lo;
    return lo + (rand() % range);
}

double forge_math_random_float(void) {
    ensure_random_seeded();
    return (double)rand() / ((double)RAND_MAX + 1.0);
}

void forge_math_seed_random(uint64_t seed) {
    srand((unsigned int)seed);
    g_random_seeded = 1;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Standard Library: forge.sys
 * ═══════════════════════════════════════════════════════════════════════════ */

forge_array_t forge_sys_args(void) {
    /* Skip argv[0] (the binary name) to match interpreter behavior,
     * which only returns user-supplied arguments */
    int user_argc = g_argc > 1 ? g_argc - 1 : 0;
    forge_array_t arr;
    arr.elem_size = sizeof(forge_str_t);
    arr.cap = user_argc > 0 ? user_argc : 1;
    arr.len = user_argc;
    arr.data = malloc(arr.cap * arr.elem_size);

    for (int i = 0; i < user_argc; i++) {
        forge_str_t s = forge_str_lit(g_argv[i + 1]);
        memcpy((char*)arr.data + i * arr.elem_size, &s, sizeof(forge_str_t));
    }

    return arr;
}

forge_str_t forge_sys_env(forge_str_t name) {
    /* Need null-terminated string for getenv */
    char* name_cstr = malloc(name.len + 1);
    memcpy(name_cstr, name.data, name.len);
    name_cstr[name.len] = '\0';

    const char* val = getenv(name_cstr);
    free(name_cstr);

    if (val == NULL) {
        /* Return empty string if not found */
        return forge_str_lit("");
    }
    return forge_str_lit(val);
}

forge_str_t forge_sys_env_ptr(forge_str_t* name) {
    return forge_sys_env(*name);
}

void forge_sys_exit(int64_t code) {
    exit((int)code);
}

void forge_sys_halt(void) {
    exit(0);
}

forge_str_t forge_sys_platform(void) {
#if defined(__linux__)
    return forge_str_lit("linux");
#elif defined(__APPLE__) && defined(__MACH__)
    return forge_str_lit("macos");
#elif defined(_WIN32) || defined(_WIN64)
    return forge_str_lit("windows");
#else
    return forge_str_lit("embedded");
#endif
}

forge_str_t forge_sys_arch(void) {
#if defined(__x86_64__) || defined(_M_X64)
    return forge_str_lit("x86_64");
#elif defined(__aarch64__) || defined(_M_ARM64)
    return forge_str_lit("arm64");
#elif defined(__arm__) || defined(_M_ARM)
    return forge_str_lit("arm");
#elif defined(__riscv) && (__riscv_xlen == 64)
    return forge_str_lit("riscv64");
#elif defined(__riscv) && (__riscv_xlen == 32)
    return forge_str_lit("riscv32");
#elif defined(__i386__) || defined(_M_IX86)
    return forge_str_lit("x86");
#else
    return forge_str_lit("unknown");
#endif
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Standard Library: forge.time
 * ═══════════════════════════════════════════════════════════════════════════ */

#include <sys/time.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

uint64_t forge_time_now(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000ULL + (uint64_t)tv.tv_usec / 1000ULL;
}

void forge_time_sleep(uint64_t ms) {
#ifdef _WIN32
    Sleep((DWORD)ms);
#else
    usleep((useconds_t)(ms * 1000));
#endif
}

forge_str_t forge_time_timestamp(void) {
    time_t now = time(NULL);
    struct tm* tm_info = gmtime(&now);

    /* ISO 8601 format: YYYY-MM-DDTHH:MM:SSZ */
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", tm_info);

    return forge_str_dup(buf, (int)strlen(buf));
}

uint64_t forge_time_elapsed_ms(uint64_t start) {
    uint64_t now_ms = forge_time_now();
    return now_ms - start;
}

/* Note: For simplicity, start_clock() just returns the current time as an int.
 * This matches how the interpreter and LLVM backends handle it.
 * A more advanced implementation would use forge_clock_t records. */
uint64_t forge_time_start_clock_simple(void) {
    return forge_time_now();
}

uint64_t forge_time_lap_simple(uint64_t start) {
    return forge_time_elapsed_ms(start);
}

/* Keep the full struct-based versions for potential future use */
forge_clock_t forge_time_start_clock(void) {
    forge_clock_t clock;
    clock.start_ms = forge_time_now();
    clock.lap_ms = clock.start_ms;
    return clock;
}

uint64_t forge_time_lap(forge_clock_t* clock) {
    uint64_t now_ms = forge_time_now();
    uint64_t elapsed = now_ms - clock->lap_ms;
    clock->lap_ms = now_ms;
    return elapsed;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Standard Library: forge.buf — Byte Buffer Operations
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Internal buffer structure */
typedef struct {
    uint8_t* data;
    int64_t  length;    /* bytes written */
    int64_t  capacity;
    int64_t  position;  /* read position */
    int      in_use;    /* 1 if allocated, 0 if free */
} forge_buf_t;

/* Simple buffer pool - up to 256 concurrent buffers */
#define FORGE_BUF_POOL_SIZE 256
static forge_buf_t g_buf_pool[FORGE_BUF_POOL_SIZE];
static int g_buf_pool_initialized = 0;

static void forge_buf_pool_init(void) {
    if (!g_buf_pool_initialized) {
        for (int i = 0; i < FORGE_BUF_POOL_SIZE; i++) {
            g_buf_pool[i].data = NULL;
            g_buf_pool[i].length = 0;
            g_buf_pool[i].capacity = 0;
            g_buf_pool[i].position = 0;
            g_buf_pool[i].in_use = 0;
        }
        g_buf_pool_initialized = 1;
    }
}

int64_t forge_buf_create(int64_t capacity) {
    forge_buf_pool_init();

    /* Find a free slot */
    for (int i = 0; i < FORGE_BUF_POOL_SIZE; i++) {
        if (!g_buf_pool[i].in_use) {
            g_buf_pool[i].data = (uint8_t*)malloc((size_t)capacity);
            if (!g_buf_pool[i].data) {
                fprintf(stderr, "forge.buf.create: allocation failed\n");
                return -1;
            }
            g_buf_pool[i].length = 0;
            g_buf_pool[i].capacity = capacity;
            g_buf_pool[i].position = 0;
            g_buf_pool[i].in_use = 1;
            return (int64_t)i;
        }
    }

    fprintf(stderr, "forge.buf.create: buffer pool exhausted\n");
    return -1;
}

void forge_buf_free_buf(int64_t handle) {
    if (handle < 0 || handle >= FORGE_BUF_POOL_SIZE) return;
    if (!g_buf_pool[handle].in_use) return;

    free(g_buf_pool[handle].data);
    g_buf_pool[handle].data = NULL;
    g_buf_pool[handle].length = 0;
    g_buf_pool[handle].capacity = 0;
    g_buf_pool[handle].position = 0;
    g_buf_pool[handle].in_use = 0;
}

/* Helper to ensure capacity */
static void forge_buf_ensure_capacity(forge_buf_t* buf, int64_t needed) {
    if (buf->length + needed > buf->capacity) {
        int64_t new_cap = buf->capacity * 2;
        if (new_cap < buf->length + needed) {
            new_cap = buf->length + needed;
        }
        uint8_t* new_data = (uint8_t*)realloc(buf->data, (size_t)new_cap);
        if (new_data) {
            buf->data = new_data;
            buf->capacity = new_cap;
        }
    }
}

void forge_buf_write_byte(int64_t handle, int64_t byte_val) {
    if (handle < 0 || handle >= FORGE_BUF_POOL_SIZE) return;
    if (!g_buf_pool[handle].in_use) return;

    forge_buf_t* buf = &g_buf_pool[handle];
    forge_buf_ensure_capacity(buf, 1);
    buf->data[buf->length++] = (uint8_t)(byte_val & 0xFF);
}

void forge_buf_write_bytes(int64_t handle, uint8_t* data, int64_t len) {
    if (handle < 0 || handle >= FORGE_BUF_POOL_SIZE) return;
    if (!g_buf_pool[handle].in_use) return;
    if (!data || len <= 0) return;

    forge_buf_t* buf = &g_buf_pool[handle];
    forge_buf_ensure_capacity(buf, len);
    memcpy(buf->data + buf->length, data, (size_t)len);
    buf->length += len;
}

void forge_buf_write_str(int64_t handle, forge_str_t s) {
    forge_buf_write_bytes(handle, (uint8_t*)s.data, s.len);
}

void forge_buf_write_str_ptr(int64_t handle, forge_str_t* s) {
    if (s) forge_buf_write_str(handle, *s);
}

void forge_buf_write_int16_le(int64_t handle, int64_t val) {
    if (handle < 0 || handle >= FORGE_BUF_POOL_SIZE) return;
    if (!g_buf_pool[handle].in_use) return;

    forge_buf_t* buf = &g_buf_pool[handle];
    forge_buf_ensure_capacity(buf, 2);
    int16_t v = (int16_t)val;
    buf->data[buf->length++] = (uint8_t)(v & 0xFF);
    buf->data[buf->length++] = (uint8_t)((v >> 8) & 0xFF);
}

void forge_buf_write_int32_le(int64_t handle, int64_t val) {
    if (handle < 0 || handle >= FORGE_BUF_POOL_SIZE) return;
    if (!g_buf_pool[handle].in_use) return;

    forge_buf_t* buf = &g_buf_pool[handle];
    forge_buf_ensure_capacity(buf, 4);
    int32_t v = (int32_t)val;
    buf->data[buf->length++] = (uint8_t)(v & 0xFF);
    buf->data[buf->length++] = (uint8_t)((v >> 8) & 0xFF);
    buf->data[buf->length++] = (uint8_t)((v >> 16) & 0xFF);
    buf->data[buf->length++] = (uint8_t)((v >> 24) & 0xFF);
}


void forge_buf_write_float32_le(int64_t handle, double val) {
    if (handle < 0 || handle >= FORGE_BUF_POOL_SIZE) return;
    if (!g_buf_pool[handle].in_use) return;

    forge_buf_t* buf = &g_buf_pool[handle];
    forge_buf_ensure_capacity(buf, 4);
    float f = (float)val;
    uint32_t bits;
    memcpy(&bits, &f, sizeof(bits));
    buf->data[buf->length++] = (uint8_t)(bits & 0xFF);
    buf->data[buf->length++] = (uint8_t)((bits >> 8) & 0xFF);
    buf->data[buf->length++] = (uint8_t)((bits >> 16) & 0xFF);
    buf->data[buf->length++] = (uint8_t)((bits >> 24) & 0xFF);
}

/* Read operations - return sentinel values for "none" */
int64_t forge_buf_read_byte(int64_t handle) {
    if (handle < 0 || handle >= FORGE_BUF_POOL_SIZE) return -1;
    if (!g_buf_pool[handle].in_use) return -1;

    forge_buf_t* buf = &g_buf_pool[handle];
    if (buf->position >= buf->length) return -1;

    return (int64_t)buf->data[buf->position++];
}

int64_t forge_buf_read_int16_le(int64_t handle) {
    if (handle < 0 || handle >= FORGE_BUF_POOL_SIZE) return INT64_MIN;
    if (!g_buf_pool[handle].in_use) return INT64_MIN;

    forge_buf_t* buf = &g_buf_pool[handle];
    if (buf->position + 2 > buf->length) return INT64_MIN;

    int16_t val = (int16_t)(buf->data[buf->position] |
                           (buf->data[buf->position + 1] << 8));
    buf->position += 2;
    return (int64_t)val;
}

int64_t forge_buf_read_int32_le(int64_t handle) {
    if (handle < 0 || handle >= FORGE_BUF_POOL_SIZE) return INT64_MIN;
    if (!g_buf_pool[handle].in_use) return INT64_MIN;

    forge_buf_t* buf = &g_buf_pool[handle];
    if (buf->position + 4 > buf->length) return INT64_MIN;

    int32_t val = (int32_t)(buf->data[buf->position] |
                           (buf->data[buf->position + 1] << 8) |
                           (buf->data[buf->position + 2] << 16) |
                           (buf->data[buf->position + 3] << 24));
    buf->position += 4;
    return (int64_t)val;
}

/* Position operations */
void forge_buf_seek(int64_t handle, int64_t pos) {
    if (handle < 0 || handle >= FORGE_BUF_POOL_SIZE) return;
    if (!g_buf_pool[handle].in_use) return;

    forge_buf_t* buf = &g_buf_pool[handle];
    if (pos < 0) pos = 0;
    if (pos > buf->length) pos = buf->length;
    buf->position = pos;
}

void forge_buf_rewind(int64_t handle) {
    forge_buf_seek(handle, 0);
}

int64_t forge_buf_remaining(int64_t handle) {
    if (handle < 0 || handle >= FORGE_BUF_POOL_SIZE) return 0;
    if (!g_buf_pool[handle].in_use) return 0;

    forge_buf_t* buf = &g_buf_pool[handle];
    return buf->length - buf->position;
}

int64_t forge_buf_length(int64_t handle) {
    if (handle < 0 || handle >= FORGE_BUF_POOL_SIZE) return 0;
    if (!g_buf_pool[handle].in_use) return 0;
    return g_buf_pool[handle].length;
}

int64_t forge_buf_capacity(int64_t handle) {
    if (handle < 0 || handle >= FORGE_BUF_POOL_SIZE) return 0;
    if (!g_buf_pool[handle].in_use) return 0;
    return g_buf_pool[handle].capacity;
}

int64_t forge_buf_position(int64_t handle) {
    if (handle < 0 || handle >= FORGE_BUF_POOL_SIZE) return 0;
    if (!g_buf_pool[handle].in_use) return 0;
    return g_buf_pool[handle].position;
}

/* Conversion operations */
forge_str_t forge_buf_to_str(int64_t handle) {
    if (handle < 0 || handle >= FORGE_BUF_POOL_SIZE) {
        return forge_str_lit("");
    }
    if (!g_buf_pool[handle].in_use) {
        return forge_str_lit("");
    }

    forge_buf_t* buf = &g_buf_pool[handle];
    return forge_str_dup((const char*)buf->data, (int)buf->length);
}

void forge_buf_to_str_ptr(forge_str_t* out, int64_t handle) {
    if (out) *out = forge_buf_to_str(handle);
}

forge_str_t forge_buf_to_hex(int64_t handle) {
    if (handle < 0 || handle >= FORGE_BUF_POOL_SIZE) {
        return forge_str_lit("");
    }
    if (!g_buf_pool[handle].in_use) {
        return forge_str_lit("");
    }

    forge_buf_t* buf = &g_buf_pool[handle];
    if (buf->length == 0) {
        return forge_str_lit("");
    }

    /* Each byte becomes "XX " (3 chars), minus trailing space */
    int64_t hex_len = buf->length * 3 - 1;
    char* hex = (char*)malloc((size_t)(hex_len + 1));
    if (!hex) return forge_str_lit("");

    static const char hex_chars[] = "0123456789ABCDEF";
    char* p = hex;
    for (int64_t i = 0; i < buf->length; i++) {
        if (i > 0) *p++ = ' ';
        uint8_t b = buf->data[i];
        *p++ = hex_chars[(b >> 4) & 0xF];
        *p++ = hex_chars[b & 0xF];
    }
    *p = '\0';

    forge_str_t result = forge_str_dup(hex, (int)hex_len);
    free(hex);
    return result;
}

void forge_buf_to_hex_ptr(forge_str_t* out, int64_t handle) {
    if (out) *out = forge_buf_to_hex(handle);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * forge.serial — Serial Port I/O
 * ═══════════════════════════════════════════════════════════════════════════ */

#include <termios.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>

typedef struct {
    int      fd;           /* File descriptor */
    int64_t  baud;         /* Baud rate */
    int      is_open;      /* Port is open */
    int      timeout_ms;   /* Read timeout in ms */
    struct termios orig;   /* Original terminal settings */
} forge_serial_t;

static forge_serial_t g_serial_pool[FORGE_SERIAL_POOL_SIZE];
static int g_serial_pool_init = 0;

static void serial_pool_init(void) {
    if (g_serial_pool_init) return;
    for (int i = 0; i < FORGE_SERIAL_POOL_SIZE; i++) {
        g_serial_pool[i].fd = -1;
        g_serial_pool[i].baud = 0;
        g_serial_pool[i].is_open = 0;
        g_serial_pool[i].timeout_ms = 0;
    }
    g_serial_pool_init = 1;
}

static speed_t baud_to_speed(int64_t baud) {
    switch (baud) {
        case 300:    return B300;
        case 600:    return B600;
        case 1200:   return B1200;
        case 2400:   return B2400;
        case 4800:   return B4800;
        case 9600:   return B9600;
        case 19200:  return B19200;
        case 38400:  return B38400;
        case 57600:  return B57600;
        case 115200: return B115200;
        case 230400: return B230400;
        default:     return B9600;  /* Default to 9600 */
    }
}

int64_t forge_serial_open(forge_str_t path, int64_t baud) {
    serial_pool_init();

    /* Find free slot */
    int slot = -1;
    for (int i = 0; i < FORGE_SERIAL_POOL_SIZE; i++) {
        if (!g_serial_pool[i].is_open) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        fprintf(stderr, "forge.serial.open: serial port pool exhausted\n");
        return -1;
    }

    /* Null-terminate path */
    char* path_str = (char*)malloc((size_t)path.len + 1);
    memcpy(path_str, path.data, (size_t)path.len);
    path_str[path.len] = '\0';

    /* Open port */
    int fd = open(path_str, O_RDWR | O_NOCTTY | O_NONBLOCK);
    free(path_str);

    if (fd < 0) {
        return -1;
    }

    /* Save original settings */
    struct termios tty;
    if (tcgetattr(fd, &g_serial_pool[slot].orig) != 0) {
        close(fd);
        return -1;
    }

    /* Configure port */
    memset(&tty, 0, sizeof(tty));
    tty.c_cflag = CS8 | CREAD | CLOCAL;  /* 8N1, enable receiver, ignore modem control */
    tty.c_iflag = IGNPAR;                 /* Ignore parity errors */
    tty.c_oflag = 0;
    tty.c_lflag = 0;                      /* Non-canonical mode */
    tty.c_cc[VMIN] = 0;                   /* Non-blocking read */
    tty.c_cc[VTIME] = 0;

    speed_t speed = baud_to_speed(baud);
    cfsetispeed(&tty, speed);
    cfsetospeed(&tty, speed);

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        close(fd);
        return -1;
    }

    /* Clear non-blocking for normal operation */
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);

    /* Store in pool */
    g_serial_pool[slot].fd = fd;
    g_serial_pool[slot].baud = baud;
    g_serial_pool[slot].is_open = 1;
    g_serial_pool[slot].timeout_ms = 1000;  /* Default 1 second timeout */

    return (int64_t)slot;
}

int64_t forge_serial_open_ptr(forge_str_t* path, int64_t baud) {
    if (!path) return -1;
    return forge_serial_open(*path, baud);
}

void forge_serial_close(int64_t handle) {
    if (handle < 0 || handle >= FORGE_SERIAL_POOL_SIZE) return;
    if (!g_serial_pool[handle].is_open) return;

    /* Restore original settings */
    tcsetattr(g_serial_pool[handle].fd, TCSANOW, &g_serial_pool[handle].orig);

    close(g_serial_pool[handle].fd);
    g_serial_pool[handle].fd = -1;
    g_serial_pool[handle].baud = 0;
    g_serial_pool[handle].is_open = 0;
}

int64_t forge_serial_read_byte(int64_t handle) {
    if (handle < 0 || handle >= FORGE_SERIAL_POOL_SIZE) return -1;
    if (!g_serial_pool[handle].is_open) return -1;

    /* Check if data is available */
    int bytes_avail = 0;
    if (ioctl(g_serial_pool[handle].fd, FIONREAD, &bytes_avail) < 0) {
        return -1;
    }
    if (bytes_avail == 0) {
        return -1;  /* No data available */
    }

    unsigned char byte;
    ssize_t n = read(g_serial_pool[handle].fd, &byte, 1);
    if (n == 1) {
        return (int64_t)byte;
    }
    return -1;
}

int64_t forge_serial_bytes_available(int64_t handle) {
    if (handle < 0 || handle >= FORGE_SERIAL_POOL_SIZE) return 0;
    if (!g_serial_pool[handle].is_open) return 0;

    int bytes_avail = 0;
    if (ioctl(g_serial_pool[handle].fd, FIONREAD, &bytes_avail) < 0) {
        return 0;
    }
    return (int64_t)bytes_avail;
}

forge_str_t forge_serial_read_line(int64_t handle) {
    if (handle < 0 || handle >= FORGE_SERIAL_POOL_SIZE) {
        return forge_str_lit("");
    }
    if (!g_serial_pool[handle].is_open) {
        return forge_str_lit("");
    }

    /* Read until newline */
    char* buf = (char*)malloc(1024);
    int pos = 0;
    int capacity = 1024;

    /* Set up timeout using select */
    int fd = g_serial_pool[handle].fd;
    int timeout_ms = g_serial_pool[handle].timeout_ms;

    while (1) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(fd, &readfds);

        struct timeval tv;
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;

        int ret = select(fd + 1, &readfds, NULL, NULL, &tv);
        if (ret <= 0) {
            break;  /* Timeout or error */
        }

        unsigned char byte;
        ssize_t n = read(fd, &byte, 1);
        if (n != 1) break;

        if (byte == '\n') {
            break;  /* End of line */
        }
        if (byte == '\r') {
            continue;  /* Skip CR */
        }

        /* Add to buffer */
        if (pos >= capacity - 1) {
            capacity *= 2;
            buf = (char*)realloc(buf, (size_t)capacity);
        }
        buf[pos++] = (char)byte;
    }

    buf[pos] = '\0';
    forge_str_t result = forge_str_dup(buf, pos);
    free(buf);
    return result;
}

void forge_serial_read_line_ptr(forge_str_t* out, int64_t handle) {
    if (out) *out = forge_serial_read_line(handle);
}

void forge_serial_write_byte(int64_t handle, int64_t byte_val) {
    if (handle < 0 || handle >= FORGE_SERIAL_POOL_SIZE) return;
    if (!g_serial_pool[handle].is_open) return;

    unsigned char byte = (unsigned char)(byte_val & 0xFF);
    write(g_serial_pool[handle].fd, &byte, 1);
}

void forge_serial_write_str(int64_t handle, forge_str_t s) {
    if (handle < 0 || handle >= FORGE_SERIAL_POOL_SIZE) return;
    if (!g_serial_pool[handle].is_open) return;

    if (s.len > 0 && s.data) {
        write(g_serial_pool[handle].fd, s.data, (size_t)s.len);
    }
}

void forge_serial_write_str_ptr(int64_t handle, forge_str_t* s) {
    if (s) forge_serial_write_str(handle, *s);
}

void forge_serial_set_timeout(int64_t handle, int64_t ms) {
    if (handle < 0 || handle >= FORGE_SERIAL_POOL_SIZE) return;
    if (!g_serial_pool[handle].is_open) return;

    g_serial_pool[handle].timeout_ms = (int)ms;
}

void forge_serial_flush(int64_t handle) {
    if (handle < 0 || handle >= FORGE_SERIAL_POOL_SIZE) return;
    if (!g_serial_pool[handle].is_open) return;

    /* Wait for output to be transmitted (don't discard it!) */
    tcdrain(g_serial_pool[handle].fd);
}

int64_t forge_serial_is_open(int64_t handle) {
    if (handle < 0 || handle >= FORGE_SERIAL_POOL_SIZE) return 0;
    return g_serial_pool[handle].is_open ? 1 : 0;
}

int64_t forge_serial_get_baud(int64_t handle) {
    if (handle < 0 || handle >= FORGE_SERIAL_POOL_SIZE) return 0;
    if (!g_serial_pool[handle].is_open) return 0;
    return g_serial_pool[handle].baud;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * forge.nmea — NMEA 0183 Sentence Parsing
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Helper: calculate XOR checksum of NMEA data between $ and * */
static uint8_t nmea_calc_checksum(const char* data, int len) {
    uint8_t cs = 0;
    int start = 0;
    int end = len;

    /* Find start (skip $) */
    if (len > 0 && data[0] == '$') {
        start = 1;
    }

    /* Find end (stop at *) */
    for (int i = start; i < len; i++) {
        if (data[i] == '*') {
            end = i;
            break;
        }
    }

    /* XOR all characters between $ and * */
    for (int i = start; i < end; i++) {
        cs ^= (uint8_t)data[i];
    }

    return cs;
}

/* Helper: parse hex digit */
static int hex_digit_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

int64_t forge_nmea_validate(forge_str_t sentence) {
    if (sentence.len < 4) return 0;  /* Minimum: $X*CC */

    const char* data = sentence.data;
    int len = sentence.len;

    /* Find * and extract provided checksum */
    int star_pos = -1;
    for (int i = len - 1; i >= 0; i--) {
        if (data[i] == '*') {
            star_pos = i;
            break;
        }
    }

    if (star_pos < 0 || star_pos + 2 >= len) {
        return 0;  /* No checksum found */
    }

    /* Parse provided checksum */
    int hi = hex_digit_value(data[star_pos + 1]);
    int lo = hex_digit_value(data[star_pos + 2]);
    if (hi < 0 || lo < 0) return 0;

    uint8_t provided = (uint8_t)((hi << 4) | lo);
    uint8_t calculated = nmea_calc_checksum(data, len);

    return (provided == calculated) ? 1 : 0;
}

int64_t forge_nmea_validate_ptr(forge_str_t* sentence) {
    if (!sentence) return 0;
    return forge_nmea_validate(*sentence);
}

forge_str_t forge_nmea_checksum(forge_str_t sentence) {
    uint8_t cs = nmea_calc_checksum(sentence.data, sentence.len);

    char buf[3];
    static const char hex[] = "0123456789ABCDEF";
    buf[0] = hex[(cs >> 4) & 0x0F];
    buf[1] = hex[cs & 0x0F];
    buf[2] = '\0';

    return forge_str_dup(buf, 2);
}

void forge_nmea_checksum_ptr(forge_str_t* out, forge_str_t* sentence) {
    if (out && sentence) {
        *out = forge_nmea_checksum(*sentence);
    } else if (out) {
        *out = forge_str_lit("");
    }
}

forge_str_t forge_nmea_sentence_type(forge_str_t sentence) {
    if (sentence.len < 2) return forge_str_lit("");

    const char* data = sentence.data;
    int start = (data[0] == '$') ? 1 : 0;

    /* Find first comma */
    int end = start;
    while (end < sentence.len && data[end] != ',' && data[end] != '*') {
        end++;
    }

    if (end <= start) return forge_str_lit("");

    return forge_str_dup(data + start, end - start);
}

void forge_nmea_sentence_type_ptr(forge_str_t* out, forge_str_t* sentence) {
    if (out && sentence) {
        *out = forge_nmea_sentence_type(*sentence);
    } else if (out) {
        *out = forge_str_lit("");
    }
}

forge_str_t forge_nmea_get_talker(forge_str_t sentence) {
    if (sentence.len < 3) return forge_str_lit("");

    const char* data = sentence.data;
    int start = (data[0] == '$') ? 1 : 0;

    /* Talker ID is typically 2 characters */
    if (start + 2 > sentence.len) return forge_str_lit("");

    return forge_str_dup(data + start, 2);
}

void forge_nmea_get_talker_ptr(forge_str_t* out, forge_str_t* sentence) {
    if (out && sentence) {
        *out = forge_nmea_get_talker(*sentence);
    } else if (out) {
        *out = forge_str_lit("");
    }
}

int64_t forge_nmea_field_count(forge_str_t sentence) {
    if (sentence.len == 0) return 0;

    const char* data = sentence.data;
    int count = 1;  /* At least one field (the sentence type) */

    for (int i = 0; i < sentence.len; i++) {
        if (data[i] == ',') {
            count++;
        } else if (data[i] == '*') {
            break;  /* Stop at checksum delimiter */
        }
    }

    return (int64_t)count;
}

int64_t forge_nmea_field_count_ptr(forge_str_t* sentence) {
    if (!sentence) return 0;
    return forge_nmea_field_count(*sentence);
}

forge_str_t forge_nmea_get_field(forge_str_t sentence, int64_t index) {
    if (sentence.len == 0 || index < 0) return forge_str_lit("");

    const char* data = sentence.data;
    int current_field = 0;
    int field_start = (data[0] == '$') ? 1 : 0;

    for (int i = field_start; i <= sentence.len; i++) {
        char c = (i < sentence.len) ? data[i] : '*';

        if (c == ',' || c == '*' || i == sentence.len) {
            if (current_field == index) {
                /* Found the field */
                int field_len = i - field_start;
                if (field_len <= 0) return forge_str_lit("");
                return forge_str_dup(data + field_start, field_len);
            }
            current_field++;
            field_start = i + 1;

            if (c == '*') break;  /* No more fields after checksum */
        }
    }

    return forge_str_lit("");  /* Field not found */
}

void forge_nmea_get_field_ptr(forge_str_t* out, forge_str_t* sentence, int64_t index) {
    if (out && sentence) {
        *out = forge_nmea_get_field(*sentence, index);
    } else if (out) {
        *out = forge_str_lit("");
    }
}

/* Helper: convert NMEA coordinate (DDDMM.MMMM) to decimal degrees */
static double nmea_coord_to_degrees(const char* coord, int len) {
    if (len < 4) return 0.0;

    /* Find decimal point */
    int dot_pos = -1;
    for (int i = 0; i < len; i++) {
        if (coord[i] == '.') {
            dot_pos = i;
            break;
        }
    }

    if (dot_pos < 2) return 0.0;

    /* Parse degrees (everything before the last 2 digits before decimal) */
    int deg_end = dot_pos - 2;
    double degrees = 0.0;
    for (int i = 0; i < deg_end; i++) {
        if (coord[i] >= '0' && coord[i] <= '9') {
            degrees = degrees * 10.0 + (coord[i] - '0');
        }
    }

    /* Parse minutes (2 digits before decimal + fractional part) */
    char min_str[32];
    int min_len = 0;
    for (int i = deg_end; i < len && min_len < 31; i++) {
        min_str[min_len++] = coord[i];
    }
    min_str[min_len] = '\0';

    double minutes = atof(min_str);

    return degrees + (minutes / 60.0);
}

double forge_nmea_latitude(forge_str_t sentence) {
    /* Get sentence type to determine field positions */
    forge_str_t type = forge_nmea_sentence_type(sentence);

    int lat_field = -1;
    int ns_field = -1;

    /* Check for GGA or RMC */
    if (type.len >= 3) {
        const char* t = type.data + (type.len >= 5 ? 2 : 0);  /* Skip talker ID */
        if ((t[0] == 'G' && t[1] == 'G' && t[2] == 'A') ||
            (type.len >= 5 && type.data[2] == 'G' && type.data[3] == 'G' && type.data[4] == 'A')) {
            /* GGA: field 2 = lat, field 3 = N/S */
            lat_field = 2;
            ns_field = 3;
        } else if ((t[0] == 'R' && t[1] == 'M' && t[2] == 'C') ||
                   (type.len >= 5 && type.data[2] == 'R' && type.data[3] == 'M' && type.data[4] == 'C')) {
            /* RMC: field 3 = lat, field 4 = N/S */
            lat_field = 3;
            ns_field = 4;
        }
    }

    if (type.owned) free((void*)type.data);

    if (lat_field < 0) return 0.0;

    /* Get latitude field */
    forge_str_t lat_str = forge_nmea_get_field(sentence, lat_field);
    forge_str_t ns_str = forge_nmea_get_field(sentence, ns_field);

    double lat = nmea_coord_to_degrees(lat_str.data, lat_str.len);

    /* Apply N/S sign */
    if (ns_str.len > 0 && (ns_str.data[0] == 'S' || ns_str.data[0] == 's')) {
        lat = -lat;
    }

    if (lat_str.owned) free((void*)lat_str.data);
    if (ns_str.owned) free((void*)ns_str.data);

    return lat;
}

double forge_nmea_latitude_ptr(forge_str_t* sentence) {
    if (!sentence) return 0.0;
    return forge_nmea_latitude(*sentence);
}

double forge_nmea_longitude(forge_str_t sentence) {
    /* Get sentence type to determine field positions */
    forge_str_t type = forge_nmea_sentence_type(sentence);

    int lon_field = -1;
    int ew_field = -1;

    /* Check for GGA or RMC */
    if (type.len >= 3) {
        const char* t = type.data + (type.len >= 5 ? 2 : 0);  /* Skip talker ID */
        if ((t[0] == 'G' && t[1] == 'G' && t[2] == 'A') ||
            (type.len >= 5 && type.data[2] == 'G' && type.data[3] == 'G' && type.data[4] == 'A')) {
            /* GGA: field 4 = lon, field 5 = E/W */
            lon_field = 4;
            ew_field = 5;
        } else if ((t[0] == 'R' && t[1] == 'M' && t[2] == 'C') ||
                   (type.len >= 5 && type.data[2] == 'R' && type.data[3] == 'M' && type.data[4] == 'C')) {
            /* RMC: field 5 = lon, field 6 = E/W */
            lon_field = 5;
            ew_field = 6;
        }
    }

    if (type.owned) free((void*)type.data);

    if (lon_field < 0) return 0.0;

    /* Get longitude field */
    forge_str_t lon_str = forge_nmea_get_field(sentence, lon_field);
    forge_str_t ew_str = forge_nmea_get_field(sentence, ew_field);

    double lon = nmea_coord_to_degrees(lon_str.data, lon_str.len);

    /* Apply E/W sign */
    if (ew_str.len > 0 && (ew_str.data[0] == 'W' || ew_str.data[0] == 'w')) {
        lon = -lon;
    }

    if (lon_str.owned) free((void*)lon_str.data);
    if (ew_str.owned) free((void*)ew_str.data);

    return lon;
}

double forge_nmea_longitude_ptr(forge_str_t* sentence) {
    if (!sentence) return 0.0;
    return forge_nmea_longitude(*sentence);
}
