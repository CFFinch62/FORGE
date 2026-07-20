/*
 * FORGE Runtime Library
 *
 * This header provides the types and functions used by FORGE-generated C code.
 * Compile forge_runtime.c and link it with any compiled FORGE program.
 */

#ifndef FORGE_RUNTIME_H
#define FORGE_RUNTIME_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * String Type
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    char*   data;
    int     len;
    int     owned;      /* 1 if we should free data, 0 for literals */
} forge_str_t;

/* Create string from literal (not owned — don't free) */
forge_str_t forge_str_lit(const char* s);

/* Create owned copy of string */
forge_str_t forge_str_dup(const char* s, int len);

/* Concatenate two strings (returns new owned string) */
forge_str_t forge_str_concat(forge_str_t a, forge_str_t b);
forge_str_t forge_str_concat_ptr(forge_str_t* a, forge_str_t* b); /* LLVM ABI wrapper */

/* Free string if owned */
void forge_str_free(forge_str_t* s);

/* Convert to string */
forge_str_t forge_str_from_int(int64_t i);
forge_str_t forge_str_from_float(double f);
forge_str_t forge_str_from_bool(int b);
forge_str_t forge_str_from_char(uint8_t c);
int64_t forge_char_to_digit(uint8_t c);

/* Convert from string */
int64_t forge_str_to_int(forge_str_t s);
double forge_str_to_float(forge_str_t s);

/* String length */
int64_t forge_str_len(forge_str_t s);
int64_t forge_str_len_ptr(forge_str_t* s); /* LLVM ABI wrapper */

/* String comparison */
int forge_str_equal(forge_str_t a, forge_str_t b);
int forge_str_equal_ptr(forge_str_t* a, forge_str_t* b); /* LLVM ABI wrapper */

/* ═══════════════════════════════════════════════════════════════════════════
 * Dynamic Array Type
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    void*   data;
    int     len;
    int     cap;
    size_t  elem_size;
} forge_array_t;

/* Create array with initial capacity */
forge_array_t forge_array_create(size_t elem_size, int initial_cap);

/* Push element to end (copies elem_size bytes from elem) */
void forge_array_push(forge_array_t* a, const void* elem);

/* Get pointer to element at index (runtime bounds check) */
void* forge_array_get(forge_array_t* a, int index);

/* Set element at index */
void forge_array_set(forge_array_t* a, int index, const void* elem);

/* Get length */
int forge_array_len(forge_array_t* a);

/* Free array */
void forge_array_free(forge_array_t* a);

/* Create array from inline data (for array literal emission) */
forge_array_t forge_array_from_ints(int64_t* vals, int count);
forge_array_t forge_array_from_floats(double* vals, int count);
forge_array_t forge_array_from_bools(int* vals, int count);
forge_array_t forge_array_from_strs(forge_str_t* vals, int count);
forge_array_t forge_array_from_arrays(forge_array_t* vals, int count);

/* ═══════════════════════════════════════════════════════════════════════════
 * Map Type
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct forge_map forge_map_t;

/* Create map with key/value sizes and hash/equal functions */
forge_map_t* forge_map_create(size_t key_size, size_t val_size,
                               int (*key_equal)(const void*, const void*),
                               uint32_t (*key_hash)(const void*));

/* Set key-value pair */
void forge_map_set(forge_map_t* m, const void* key, const void* val);

/* Get value for key (returns 1 if found, 0 if not; copies to val_out) */
int forge_map_get(forge_map_t* m, const void* key, void* val_out);

/* Check if key exists */
int forge_map_has(forge_map_t* m, const void* key);

/* Delete key */
void forge_map_delete(forge_map_t* m, const void* key);

/* Get number of entries */
int forge_map_len(forge_map_t* m);

/* Free map */
void forge_map_free(forge_map_t* m);

/* Common hash/equal functions for string keys */
int forge_str_key_equal(const void* a, const void* b);
uint32_t forge_str_key_hash(const void* a);

/* Common hash/equal functions for int keys */
int forge_int_key_equal(const void* a, const void* b);
uint32_t forge_int_key_hash(const void* a);

/* ═══════════════════════════════════════════════════════════════════════════
 * Optional Type (macro-based for type safety)
 * Using struct tags for compatibility with compound literal initialization
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Named typedefs for optional types - these ensure type compatibility across
 * function boundaries (anonymous structs are treated as distinct types in C) */
typedef struct { int present; int64_t value; } forge_opt_int_t;
typedef struct { int present; double value; } forge_opt_float_t;
typedef struct { int present; int value; } forge_opt_bool_t;
typedef struct { int present; forge_str_t value; } forge_opt_str_t;

/* Generic optional (inline anonymous struct - only for function-local use
 * where cross-function compatibility is not needed) */
#define FORGE_OPTIONAL(T)  struct { int present; T value; }

/* Create some/none values - these create braced initializers */
#define FORGE_SOME_VAL(v)   { .present = 1, .value = (v) }
#define FORGE_NONE_VAL      { .present = 0 }

/* For expression contexts where compound literals are needed.
 * Prefer the named typedef versions (e.g. forge_opt_int_t) for cross-function use. */
#define FORGE_SOME(T, v)   ((FORGE_OPTIONAL(T)){ .present = 1, .value = (v) })
#define FORGE_NONE(T)      ((FORGE_OPTIONAL(T)){ .present = 0 })

/* ═══════════════════════════════════════════════════════════════════════════
 * Error Handling
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Panic with message and location */
void forge_panic(const char* msg, const char* file, int line);

/* Assert condition */
void forge_assert(int cond, const char* msg, const char* file, int line);

#define FORGE_PANIC(msg)        forge_panic(msg, __FILE__, __LINE__)
#define FORGE_ASSERT(c, msg)    forge_assert(c, msg, __FILE__, __LINE__)

/* Safe integer division - panics on division by zero */
int64_t forge_div_check(int64_t a, int64_t b, const char* file, int line);
int64_t forge_mod_check(int64_t a, int64_t b, const char* file, int line);

/* ═══════════════════════════════════════════════════════════════════════════
 * I/O Builtins (legacy)
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Print string to stdout (with newline) */
void forge_print(forge_str_t s);

/* Print string to stderr */
void forge_eprint(forge_str_t s);

/* Read line from stdin (returns owned string) */
forge_str_t forge_read_line(void);

/* ═══════════════════════════════════════════════════════════════════════════
 * Standard Library: forge.io
 * ═══════════════════════════════════════════════════════════════════════════ */

/* forge.io.print - print with newline (variadic - takes single string for now) */
void forge_io_print(forge_str_t s);

/* forge.io.print_raw - print without newline */
void forge_io_print_raw(forge_str_t s);

/* forge.io.eprint - print to stderr */
void forge_io_eprint(forge_str_t s);

/* forge.io.read_line - read line from stdin */
forge_str_t forge_io_read_line(void);

/* forge.io.read_line_prompt - print prompt then read line */
forge_str_t forge_io_read_line_prompt(forge_str_t prompt);

/* forge.io.file_exists - check if file exists */
int forge_io_file_exists(forge_str_t path);

/* forge.io.read_file - read entire file contents */
forge_str_t forge_io_read_file(forge_str_t path);

/* forge.io.write_file - write string to file */
int forge_io_write_file(forge_str_t path, forge_str_t content);

/* forge.io.append_file - append string to file */
int forge_io_append_file(forge_str_t path, forge_str_t content);

/* ═══════════════════════════════════════════════════════════════════════════
 * Standard Library: forge.str
 * ═══════════════════════════════════════════════════════════════════════════ */

/* forge.str.len - string length (same as len builtin) */
int64_t forge_str_len_fn(forge_str_t s);

/* forge.str.contains - check if string contains substring */
int forge_str_contains(forge_str_t s, forge_str_t substr);

/* forge.str.starts_with - check if string starts with prefix */
int forge_str_starts_with(forge_str_t s, forge_str_t prefix);

/* forge.str.ends_with - check if string ends with suffix */
int forge_str_ends_with(forge_str_t s, forge_str_t suffix);

/* forge.str.find - find substring, returns -1 if not found */
int64_t forge_str_find(forge_str_t s, forge_str_t substr);

/* forge.str.count - count occurrences of substring */
int64_t forge_str_count(forge_str_t s, forge_str_t substr);

/* forge.str.upper - convert to uppercase */
forge_str_t forge_str_upper(forge_str_t s);

/* forge.str.lower - convert to lowercase */
forge_str_t forge_str_lower(forge_str_t s);

/* forge.str.trim - remove leading/trailing whitespace */
forge_str_t forge_str_trim(forge_str_t s);

/* forge.str.trim_left - remove leading whitespace */
forge_str_t forge_str_trim_left(forge_str_t s);

/* forge.str.trim_right - remove trailing whitespace */
forge_str_t forge_str_trim_right(forge_str_t s);

/* forge.str.replace - replace all occurrences */
forge_str_t forge_str_replace(forge_str_t s, forge_str_t old_str, forge_str_t new_str);

/* forge.str.substr - extract substring */
forge_str_t forge_str_substr(forge_str_t s, int64_t start, int64_t length);

/* forge.str.split - split string by delimiter, returns array of strings */
forge_array_t forge_str_split(forge_str_t s, forge_str_t delim);

/* forge.str.join - join array of strings with separator */
forge_str_t forge_str_join(forge_array_t* arr, forge_str_t sep);

/* forge.str.repeat - repeat string n times */
forge_str_t forge_str_repeat(forge_str_t s, int64_t n);

/* forge.str.reverse - reverse string */
forge_str_t forge_str_reverse(forge_str_t s);

/* forge.str.char_at - get character at index as string */
forge_str_t forge_str_char_at(forge_str_t s, int64_t index);

/* ═══════════════════════════════════════════════════════════════════════════
 * LLVM ABI-Compatible Wrappers (take forge_str_t by pointer)
 * ═══════════════════════════════════════════════════════════════════════════ */

int forge_str_contains_ptr(forge_str_t* s, forge_str_t* substr);
int forge_str_starts_with_ptr(forge_str_t* s, forge_str_t* prefix);
int forge_str_ends_with_ptr(forge_str_t* s, forge_str_t* suffix);
int64_t forge_str_find_ptr(forge_str_t* s, forge_str_t* substr);
int64_t forge_str_count_ptr(forge_str_t* s, forge_str_t* substr);
forge_str_t forge_str_upper_ptr(forge_str_t* s);
forge_str_t forge_str_lower_ptr(forge_str_t* s);
forge_str_t forge_str_trim_ptr(forge_str_t* s);
forge_str_t forge_str_trim_left_ptr(forge_str_t* s);
forge_str_t forge_str_trim_right_ptr(forge_str_t* s);
forge_str_t forge_str_substr_ptr(forge_str_t* s, int64_t start, int64_t length);
forge_str_t forge_str_replace_ptr(forge_str_t* s, forge_str_t* old_str, forge_str_t* new_str);
forge_str_t forge_str_repeat_ptr(forge_str_t* s, int64_t n);
forge_str_t forge_str_reverse_ptr(forge_str_t* s);
forge_str_t forge_str_char_at_ptr(forge_str_t* s, int64_t index);
forge_array_t forge_str_split_ptr(forge_str_t* s, forge_str_t* delim);

/* ═══════════════════════════════════════════════════════════════════════════
 * Standard Library: forge.math
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Constants */
#define FORGE_MATH_PI  3.14159265358979323846
#define FORGE_MATH_E   2.71828182845904523536
#define FORGE_MATH_TAU 6.28318530717958647692

/* Absolute value */
double forge_math_abs(double x);
int64_t forge_math_abs_int(int64_t x);

/* Min/Max */
double forge_math_min(double a, double b);
double forge_math_max(double a, double b);
int64_t forge_math_min_int(int64_t a, int64_t b);
int64_t forge_math_max_int(int64_t a, int64_t b);

/* Clamp */
double forge_math_clamp(double val, double lo, double hi);

/* Power and roots */
double forge_math_pow(double base, double exp);
double forge_math_sqrt(double x);
double forge_math_cbrt(double x);

/* Rounding */
double forge_math_floor(double x);
double forge_math_ceil(double x);
double forge_math_round(double x);
double forge_math_trunc(double x);

/* Trigonometry */
double forge_math_sin(double x);
double forge_math_cos(double x);
double forge_math_tan(double x);
double forge_math_atan2(double y, double x);

/* Logarithms and exponentials */
double forge_math_log(double x);
double forge_math_log10(double x);
double forge_math_log2(double x);
double forge_math_exp(double x);

/* Random numbers */
int64_t forge_math_random_int(int64_t lo, int64_t hi);
double forge_math_random_float(void);
void forge_math_seed_random(uint64_t seed);

/* ═══════════════════════════════════════════════════════════════════════════
 * Standard Library: forge.sys
 * ═══════════════════════════════════════════════════════════════════════════ */

/* forge.sys.args - get command-line arguments as array of strings */
forge_array_t forge_sys_args(void);

/* forge.sys.env - get environment variable value (returns empty string if not found) */
forge_str_t forge_sys_env(forge_str_t name);
forge_str_t forge_sys_env_ptr(forge_str_t* name);  /* LLVM ABI wrapper */

/* forge.sys.exit - terminate process with exit code */
void forge_sys_exit(int64_t code);

/* forge.sys.halt - terminate process with code 0 */
void forge_sys_halt(void);

/* forge.sys.platform - returns "linux", "windows", "macos", "embedded" */
forge_str_t forge_sys_platform(void);

/* forge.sys.arch - returns "x86_64", "arm64", "riscv32", etc. */
forge_str_t forge_sys_arch(void);

/* ═══════════════════════════════════════════════════════════════════════════
 * Standard Library: forge.time
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Clock record for timing */
typedef struct {
    uint64_t start_ms;
    uint64_t lap_ms;
} forge_clock_t;

/* Get current time in milliseconds since epoch */
uint64_t forge_time_now(void);

/* Sleep for specified milliseconds */
void forge_time_sleep(uint64_t ms);

/* Get ISO 8601 formatted timestamp string */
forge_str_t forge_time_timestamp(void);

/* Get milliseconds elapsed since start time */
uint64_t forge_time_elapsed_ms(uint64_t start);

/* Create a new clock (starts timing) - struct version */
forge_clock_t forge_time_start_clock(void);

/* Get milliseconds since last lap (or start), resets lap timer - struct version */
uint64_t forge_time_lap(forge_clock_t* clock);

/* Simple int-based versions for code generators */
uint64_t forge_time_start_clock_simple(void);
uint64_t forge_time_lap_simple(uint64_t start);

/* ═══════════════════════════════════════════════════════════════════════════
 * Standard Library: forge.buf — Byte Buffer Operations
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Buffer is managed via opaque handles (int64_t) internally.
 * This avoids complex struct-by-ref passing in the emitters.
 */

/* Create a new buffer with given capacity, returns handle */
int64_t forge_buf_create(int64_t capacity);

/* Free a buffer by handle (note: named free_buf to avoid conflict with stdlib free) */
void forge_buf_free_buf(int64_t handle);

/* Write operations */
void forge_buf_write_byte(int64_t handle, int64_t byte_val);
void forge_buf_write_bytes(int64_t handle, uint8_t* data, int64_t len);
void forge_buf_write_str(int64_t handle, forge_str_t s);
void forge_buf_write_str_ptr(int64_t handle, forge_str_t* s);  /* ABI wrapper */
void forge_buf_write_int16_le(int64_t handle, int64_t val);
void forge_buf_write_int32_le(int64_t handle, int64_t val);
void forge_buf_write_float32_le(int64_t handle, double val);

/* Read operations - return -1 for none (optionals) */
int64_t forge_buf_read_byte(int64_t handle);  /* Returns -1 if no data */
int64_t forge_buf_read_int16_le(int64_t handle);  /* Returns INT64_MIN if no data */
int64_t forge_buf_read_int32_le(int64_t handle);  /* Returns INT64_MIN if no data */

/* Position operations */
void forge_buf_seek(int64_t handle, int64_t pos);
void forge_buf_rewind(int64_t handle);
int64_t forge_buf_remaining(int64_t handle);
int64_t forge_buf_length(int64_t handle);
int64_t forge_buf_capacity(int64_t handle);
int64_t forge_buf_position(int64_t handle);

/* Conversion operations */
forge_str_t forge_buf_to_str(int64_t handle);
void forge_buf_to_str_ptr(forge_str_t* out, int64_t handle);  /* ABI wrapper */
forge_str_t forge_buf_to_hex(int64_t handle);
void forge_buf_to_hex_ptr(forge_str_t* out, int64_t handle);  /* ABI wrapper */

/* ═══════════════════════════════════════════════════════════════════════════
 * forge.serial — Serial Port I/O
 * ═══════════════════════════════════════════════════════════════════════════ */

#define FORGE_SERIAL_POOL_SIZE 16  /* Max open serial ports */

/* Open a serial port, returns handle (-1 on failure) */
int64_t forge_serial_open(forge_str_t path, int64_t baud);
int64_t forge_serial_open_ptr(forge_str_t* path, int64_t baud);  /* ABI wrapper */

/* Close a serial port */
void forge_serial_close(int64_t handle);

/* Read operations */
int64_t forge_serial_read_byte(int64_t handle);      /* Returns byte or -1 if none available */
int64_t forge_serial_bytes_available(int64_t handle);

/* Read a line (blocking until \n, strips \r\n) */
forge_str_t forge_serial_read_line(int64_t handle);
void forge_serial_read_line_ptr(forge_str_t* out, int64_t handle);  /* ABI wrapper */

/* Write operations */
void forge_serial_write_byte(int64_t handle, int64_t byte_val);
void forge_serial_write_str(int64_t handle, forge_str_t s);
void forge_serial_write_str_ptr(int64_t handle, forge_str_t* s);  /* ABI wrapper */

/* Configuration */
void forge_serial_set_timeout(int64_t handle, int64_t ms);
void forge_serial_flush(int64_t handle);

/* Query port state */
int64_t forge_serial_is_open(int64_t handle);
int64_t forge_serial_get_baud(int64_t handle);

/* ═══════════════════════════════════════════════════════════════════════════
 * forge.nmea — NMEA 0183 Sentence Parsing
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Validate NMEA checksum (returns 1 if valid, 0 if invalid) */
int64_t forge_nmea_validate(forge_str_t sentence);
int64_t forge_nmea_validate_ptr(forge_str_t* sentence);

/* Calculate checksum and return as 2-char hex string */
forge_str_t forge_nmea_checksum(forge_str_t sentence);
void forge_nmea_checksum_ptr(forge_str_t* out, forge_str_t* sentence);

/* Get sentence type (e.g., "GPGGA", "GPRMC") */
forge_str_t forge_nmea_sentence_type(forge_str_t sentence);
void forge_nmea_sentence_type_ptr(forge_str_t* out, forge_str_t* sentence);

/* Get talker ID (e.g., "GP", "GN", "GL") */
forge_str_t forge_nmea_get_talker(forge_str_t sentence);
void forge_nmea_get_talker_ptr(forge_str_t* out, forge_str_t* sentence);

/* Count comma-separated fields */
int64_t forge_nmea_field_count(forge_str_t sentence);
int64_t forge_nmea_field_count_ptr(forge_str_t* sentence);

/* Get field by index (0-based, field 0 is the sentence type) */
forge_str_t forge_nmea_get_field(forge_str_t sentence, int64_t index);
void forge_nmea_get_field_ptr(forge_str_t* out, forge_str_t* sentence, int64_t index);

/* Parse latitude from GGA/RMC sentence (returns decimal degrees, negative for S) */
double forge_nmea_latitude(forge_str_t sentence);
double forge_nmea_latitude_ptr(forge_str_t* sentence);

/* Parse longitude from GGA/RMC sentence (returns decimal degrees, negative for W) */
double forge_nmea_longitude(forge_str_t sentence);
double forge_nmea_longitude_ptr(forge_str_t* sentence);

/* ═══════════════════════════════════════════════════════════════════════════
 * Runtime Initialization
 * ═══════════════════════════════════════════════════════════════════════════ */

void forge_runtime_init(int argc, char** argv);

#endif /* FORGE_RUNTIME_H */

