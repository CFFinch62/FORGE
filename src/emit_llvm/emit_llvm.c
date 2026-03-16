/*
 * FORGE Language Toolchain
 * emit_llvm.c - LLVM IR emitter implementation
 *
 * Emits LLVM IR text format that can be processed by LLVM tools.
 * See: https://llvm.org/docs/LangRef.html
 */

#include "emit_llvm.h"
#include "lexer/lexer.h"  /* For TOK_* constants */
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * Forward Declarations
 * ═══════════════════════════════════════════════════════════════════════════ */

static void emit_declaration(forge_llvm_emitter_t* e, forge_node_t* node);
static void emit_proc_decl(forge_llvm_emitter_t* e, forge_node_t* node);
static void emit_record_decl(forge_llvm_emitter_t* e, forge_node_t* node);
static void emit_string_constants(forge_llvm_emitter_t* e);
static void emit_runtime_declarations(forge_llvm_emitter_t* e);

/* ═══════════════════════════════════════════════════════════════════════════
 * Emitter Lifecycle
 * ═══════════════════════════════════════════════════════════════════════════ */

forge_llvm_emitter_t* llvm_emitter_create(FILE* out, forge_arena_t* arena,
                                           forge_strtable_t* strtable,
                                           const char* module_name,
                                           const char* source_file) {
    forge_llvm_emitter_t* e = ARENA_ALLOC(arena, forge_llvm_emitter_t);
    e->out = out;
    e->arena = arena;
    e->strtable = strtable;
    e->reg_counter = 0;
    e->label_counter = 0;
    e->str_counter = 0;
    e->var_counter = 0;
    e->module_name = module_name;
    e->source_file = source_file;
    e->in_loop = 0;
    e->loop_exit_label = NULL;
    e->loop_cont_label = NULL;
    e->block_terminated = 0;
    e->current_ret_type = NULL;
    e->current_block_id = -1;
    e->var_scope = NULL;

    /* Initialize string constants array */
    e->string_constants.capacity = 64;
    e->string_constants.count = 0;
    e->string_constants.strings = ARENA_ALLOC_ARRAY(arena, const char*, 64);

    return e;
}

void llvm_emitter_destroy(forge_llvm_emitter_t* e) {
    /* Arena handles memory cleanup */
    (void)e;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * SSA Helpers
 * ═══════════════════════════════════════════════════════════════════════════ */

int llvm_next_reg(forge_llvm_emitter_t* e) {
    return e->reg_counter++;
}

int llvm_next_label(forge_llvm_emitter_t* e) {
    return e->label_counter++;
}

/* Emit a label and track current block for correct phi predecessors */
static void llvm_emit_label(forge_llvm_emitter_t* e, int label_id) {
    LLVM_EMITLN(e, "L%d:", label_id);
    e->current_block_id = label_id;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Variable Scope Helpers
 * ═══════════════════════════════════════════════════════════════════════════ */

void llvm_push_scope(forge_llvm_emitter_t* e) {
    llvm_var_scope_t* scope = ARENA_ALLOC(e->arena, llvm_var_scope_t);
    scope->vars = hashmap_create();
    scope->parent = e->var_scope;
    e->var_scope = scope;
}

void llvm_pop_scope(forge_llvm_emitter_t* e) {
    if (e->var_scope) {
        llvm_var_scope_t* old = e->var_scope;
        e->var_scope = old->parent;
        hashmap_destroy(old->vars);
    }
}

const char* llvm_declare_var(forge_llvm_emitter_t* e, const char* name) {
    /* Generate a unique LLVM name for this variable */
    char buf[256];
    snprintf(buf, sizeof(buf), "%s.%d", name, e->var_counter++);
    const char* unique_name = strtable_intern_cstr(e->strtable, buf);

    /* Store in current scope */
    if (e->var_scope) {
        hashmap_set(e->var_scope->vars, name, (void*)unique_name);
    }

    return unique_name;
}

const char* llvm_lookup_var(forge_llvm_emitter_t* e, const char* name) {
    /* Search from current scope up to root */
    llvm_var_scope_t* scope = e->var_scope;
    while (scope) {
        const char* unique_name = (const char*)hashmap_get(scope->vars, name);
        if (unique_name) {
            return unique_name;
        }
        scope = scope->parent;
    }
    /* Not found in any scope - return original name for backward compatibility */
    return name;
}

/* Add a string constant and return its index */
static int add_string_constant(forge_llvm_emitter_t* e, const char* str) {
    /* Check if already exists */
    for (int i = 0; i < e->string_constants.count; i++) {
        if (strcmp(e->string_constants.strings[i], str) == 0) {
            return i;
        }
    }

    /* Add new */
    if (e->string_constants.count >= e->string_constants.capacity) {
        /* Would need to grow - for now just use what we have */
        return e->string_constants.count - 1;
    }

    int idx = e->string_constants.count++;
    e->string_constants.strings[idx] = str;
    return idx;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Type Emission
 * ═══════════════════════════════════════════════════════════════════════════ */

const char* llvm_type_str(forge_llvm_emitter_t* e, forge_type_t* type) {
    if (!type) return "void";

    switch (type->kind) {
        case TY_INT:    return "i64";
        case TY_INT8:   return "i8";
        case TY_INT16:  return "i16";
        case TY_INT32:  return "i32";
        case TY_UINT:   return "i64";  /* LLVM uses i64 for both signed/unsigned */
        case TY_UINT8:  return "i8";
        case TY_UINT16: return "i16";
        case TY_UINT32: return "i32";
        case TY_FLOAT:  return "double";
        case TY_FLOAT32: return "float";
        case TY_BOOL:   return "i1";
        case TY_BYTE:   return "i8";
        case TY_NONE:   return "void";
        case TY_VOID:   return "void";
        case TY_STR:    return "%forge_str_t";

        case TY_RECORD: {
            /* Format: %RecordName_t */
            char* buf = ARENA_ALLOC_ARRAY(e->arena, char, 128);
            snprintf(buf, 128, "%%%s_t", type->as.record.name);
            return buf;
        }

        case TY_FIXED_ARRAY:
        case TY_DYN_ARRAY:
            return "%forge_array_t";

        case TY_MAP:
            return "%forge_map_t*";

        case TY_OPTIONAL: {
            /* Optional is a struct { i1 present, T value } */
            char* buf = ARENA_ALLOC_ARRAY(e->arena, char, 128);
            snprintf(buf, 128, "{ i1, %s }", llvm_type_str(e, type->as.optional.inner));
            return buf;
        }

        default:
            return "i64"; /* fallback */
    }
}

void llvm_emit_type(forge_llvm_emitter_t* e, forge_type_t* type) {
    LLVM_EMIT(e, "%s", llvm_type_str(e, type));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Runtime Declarations
 * ═══════════════════════════════════════════════════════════════════════════ */

static void emit_runtime_declarations(forge_llvm_emitter_t* e) {
    LLVM_EMITLN(e, "; Runtime type declarations");
    /* forge_str_t: { char* data, i32 len, i32 owned } - matches C runtime */
    LLVM_EMITLN(e, "%%forge_str_t = type { i8*, i32, i32 }");
    /* forge_array_t: { void* data, int len, int cap, size_t elem_size } */
    LLVM_EMITLN(e, "%%forge_array_t = type { i8*, i32, i32, i64 }");
    LLVM_EMITLN(e, "%%forge_map_t = type opaque");
    LLVM_NEWLINE(e);

    LLVM_EMITLN(e, "; Runtime function declarations");
    LLVM_EMITLN(e, "declare void @forge_runtime_init(i32, i8**)");
    LLVM_EMITLN(e, "declare void @forge_print(%%forge_str_t)");
    LLVM_EMITLN(e, "declare %%forge_str_t @forge_str_lit(i8*)");
    LLVM_EMITLN(e, "declare %%forge_str_t @forge_str_from_int(i64)");
    LLVM_EMITLN(e, "declare %%forge_str_t @forge_str_from_float(double)");
    LLVM_EMITLN(e, "declare %%forge_str_t @forge_str_from_bool(i1)");
    LLVM_EMITLN(e, "declare %%forge_str_t @forge_str_concat_ptr(%%forge_str_t*, %%forge_str_t*)");
    LLVM_EMITLN(e, "declare i64 @forge_str_len_ptr(%%forge_str_t*)");
    LLVM_EMITLN(e, "declare i1 @forge_str_equal_ptr(%%forge_str_t*, %%forge_str_t*)");
    LLVM_EMITLN(e, "declare i64 @forge_str_to_int(%%forge_str_t)");
    LLVM_EMITLN(e, "declare double @forge_str_to_float(%%forge_str_t)");
    LLVM_EMITLN(e, "declare %%forge_str_t @forge_int_to_str(i64)");
    LLVM_EMITLN(e, "declare %%forge_str_t @forge_float_to_str(double)");
    LLVM_EMITLN(e, "declare i64 @forge_array_len(%%forge_array_t*)");
    LLVM_EMITLN(e, "declare %%forge_array_t @forge_array_create(i64, i32)");
    LLVM_EMITLN(e, "declare void @forge_array_free(%%forge_array_t*)");
    LLVM_EMITLN(e, "declare void @forge_array_push(%%forge_array_t*, ptr)");
    LLVM_EMITLN(e, "declare ptr @forge_array_get(%%forge_array_t*, i32)");
    LLVM_EMITLN(e, "declare i64 @forge_div_check(i64, i64, i8*, i32)");
    LLVM_EMITLN(e, "declare i64 @forge_mod_check(i64, i64, i8*, i32)");
    LLVM_EMITLN(e, "declare void @llvm.memset.p0i8.i64(i8*, i8, i64, i1)");
    LLVM_NEWLINE(e);

    LLVM_EMITLN(e, "; Standard library: forge.io");
    LLVM_EMITLN(e, "declare void @forge_io_print(%%forge_str_t)");
    LLVM_EMITLN(e, "declare void @forge_io_print_raw(%%forge_str_t)");
    LLVM_EMITLN(e, "declare void @forge_io_eprint(%%forge_str_t)");
    LLVM_EMITLN(e, "declare %%forge_str_t @forge_io_read_line()");
    LLVM_EMITLN(e, "declare %%forge_str_t @forge_io_read_line_prompt(%%forge_str_t)");
    LLVM_EMITLN(e, "declare i1 @forge_io_file_exists(%%forge_str_t)");
    LLVM_EMITLN(e, "declare %%forge_str_t @forge_io_read_file(%%forge_str_t)");
    LLVM_EMITLN(e, "declare i1 @forge_io_write_file(%%forge_str_t, %%forge_str_t)");
    LLVM_EMITLN(e, "declare i1 @forge_io_append_file(%%forge_str_t, %%forge_str_t)");
    LLVM_NEWLINE(e);

    LLVM_EMITLN(e, "; Standard library: forge.str (using _ptr wrappers for ABI compatibility)");
    /* forge_str_len already declared above with builtins */
    LLVM_EMITLN(e, "declare i1 @forge_str_contains_ptr(%%forge_str_t*, %%forge_str_t*)");
    LLVM_EMITLN(e, "declare i1 @forge_str_starts_with_ptr(%%forge_str_t*, %%forge_str_t*)");
    LLVM_EMITLN(e, "declare i1 @forge_str_ends_with_ptr(%%forge_str_t*, %%forge_str_t*)");
    LLVM_EMITLN(e, "declare i64 @forge_str_find_ptr(%%forge_str_t*, %%forge_str_t*)");
    LLVM_EMITLN(e, "declare i64 @forge_str_count_ptr(%%forge_str_t*, %%forge_str_t*)");
    LLVM_EMITLN(e, "declare %%forge_str_t @forge_str_upper_ptr(%%forge_str_t*)");
    LLVM_EMITLN(e, "declare %%forge_str_t @forge_str_lower_ptr(%%forge_str_t*)");
    LLVM_EMITLN(e, "declare %%forge_str_t @forge_str_trim_ptr(%%forge_str_t*)");
    LLVM_EMITLN(e, "declare %%forge_str_t @forge_str_trim_left_ptr(%%forge_str_t*)");
    LLVM_EMITLN(e, "declare %%forge_str_t @forge_str_trim_right_ptr(%%forge_str_t*)");
    LLVM_EMITLN(e, "declare %%forge_str_t @forge_str_substr_ptr(%%forge_str_t*, i64, i64)");
    LLVM_EMITLN(e, "declare %%forge_str_t @forge_str_replace_ptr(%%forge_str_t*, %%forge_str_t*, %%forge_str_t*)");
    LLVM_EMITLN(e, "declare %%forge_str_t @forge_str_repeat_ptr(%%forge_str_t*, i64)");
    LLVM_EMITLN(e, "declare %%forge_str_t @forge_str_reverse_ptr(%%forge_str_t*)");
    LLVM_EMITLN(e, "declare %%forge_str_t @forge_str_char_at_ptr(%%forge_str_t*, i64)");
    LLVM_EMITLN(e, "declare %%forge_array_t @forge_str_split_ptr(%%forge_str_t*, %%forge_str_t*)");
    LLVM_EMITLN(e, "declare %%forge_str_t @forge_str_join(%%forge_array_t*, %%forge_str_t)");
    LLVM_NEWLINE(e);

    LLVM_EMITLN(e, "; Standard library: forge.math");
    LLVM_EMITLN(e, "declare double @forge_math_abs(double)");
    LLVM_EMITLN(e, "declare i64 @forge_math_abs_int(i64)");
    LLVM_EMITLN(e, "declare double @forge_math_min(double, double)");
    LLVM_EMITLN(e, "declare double @forge_math_max(double, double)");
    LLVM_EMITLN(e, "declare i64 @forge_math_min_int(i64, i64)");
    LLVM_EMITLN(e, "declare i64 @forge_math_max_int(i64, i64)");
    LLVM_EMITLN(e, "declare double @forge_math_clamp(double, double, double)");
    LLVM_EMITLN(e, "declare double @forge_math_pow(double, double)");
    LLVM_EMITLN(e, "declare double @forge_math_sqrt(double)");
    LLVM_EMITLN(e, "declare double @forge_math_cbrt(double)");
    LLVM_EMITLN(e, "declare double @forge_math_floor(double)");
    LLVM_EMITLN(e, "declare double @forge_math_ceil(double)");
    LLVM_EMITLN(e, "declare double @forge_math_round(double)");
    LLVM_EMITLN(e, "declare double @forge_math_trunc(double)");
    LLVM_EMITLN(e, "declare double @forge_math_sin(double)");
    LLVM_EMITLN(e, "declare double @forge_math_cos(double)");
    LLVM_EMITLN(e, "declare double @forge_math_tan(double)");
    LLVM_EMITLN(e, "declare double @forge_math_atan2(double, double)");
    LLVM_EMITLN(e, "declare double @forge_math_log(double)");
    LLVM_EMITLN(e, "declare double @forge_math_log10(double)");
    LLVM_EMITLN(e, "declare double @forge_math_log2(double)");
    LLVM_EMITLN(e, "declare double @forge_math_exp(double)");
    LLVM_EMITLN(e, "declare i64 @forge_math_random_int(i64, i64)");
    LLVM_EMITLN(e, "declare double @forge_math_random_float()");
    LLVM_EMITLN(e, "declare void @forge_math_seed_random(i64)");
    LLVM_NEWLINE(e);

    LLVM_EMITLN(e, "; Standard library: forge.sys");
    LLVM_EMITLN(e, "declare %%forge_array_t @forge_sys_args()");
    LLVM_EMITLN(e, "declare %%forge_str_t @forge_sys_env_ptr(%%forge_str_t*)");
    LLVM_EMITLN(e, "declare void @forge_sys_exit(i64)");
    LLVM_EMITLN(e, "declare void @forge_sys_halt()");
    LLVM_EMITLN(e, "declare %%forge_str_t @forge_sys_platform()");
    LLVM_EMITLN(e, "declare %%forge_str_t @forge_sys_arch()");
    LLVM_NEWLINE(e);

    LLVM_EMITLN(e, "; Standard library: forge.time");
    LLVM_EMITLN(e, "declare i64 @forge_time_now()");
    LLVM_EMITLN(e, "declare void @forge_time_sleep(i64)");
    LLVM_EMITLN(e, "declare %%forge_str_t @forge_time_timestamp()");
    LLVM_EMITLN(e, "declare i64 @forge_time_elapsed_ms(i64)");
    LLVM_EMITLN(e, "declare i64 @forge_time_start_clock_simple()");
    LLVM_EMITLN(e, "declare i64 @forge_time_lap_simple(i64)");
    LLVM_NEWLINE(e);

    LLVM_EMITLN(e, "; Standard library: forge.buf");
    LLVM_EMITLN(e, "declare i64 @forge_buf_create(i64)");
    LLVM_EMITLN(e, "declare void @forge_buf_free_buf(i64)");
    LLVM_EMITLN(e, "declare void @forge_buf_write_byte(i64, i64)");
    LLVM_EMITLN(e, "declare void @forge_buf_write_str_ptr(i64, %%forge_str_t*)");
    LLVM_EMITLN(e, "declare void @forge_buf_write_int16_le(i64, i64)");
    LLVM_EMITLN(e, "declare void @forge_buf_write_int32_le(i64, i64)");
    LLVM_EMITLN(e, "declare void @forge_buf_write_float32_le(i64, double)");
    LLVM_EMITLN(e, "declare i64 @forge_buf_read_byte(i64)");
    LLVM_EMITLN(e, "declare i64 @forge_buf_read_int16_le(i64)");
    LLVM_EMITLN(e, "declare i64 @forge_buf_read_int32_le(i64)");
    LLVM_EMITLN(e, "declare void @forge_buf_seek(i64, i64)");
    LLVM_EMITLN(e, "declare void @forge_buf_rewind(i64)");
    LLVM_EMITLN(e, "declare i64 @forge_buf_remaining(i64)");
    LLVM_EMITLN(e, "declare i64 @forge_buf_length(i64)");
    LLVM_EMITLN(e, "declare i64 @forge_buf_capacity(i64)");
    LLVM_EMITLN(e, "declare i64 @forge_buf_position(i64)");
    LLVM_EMITLN(e, "declare void @forge_buf_to_str_ptr(%%forge_str_t*, i64)");
    LLVM_EMITLN(e, "declare void @forge_buf_to_hex_ptr(%%forge_str_t*, i64)");
    LLVM_NEWLINE(e);

    LLVM_EMITLN(e, "; Standard library: forge.serial");
    LLVM_EMITLN(e, "declare i64 @forge_serial_open_ptr(%%forge_str_t*, i64)");
    LLVM_EMITLN(e, "declare void @forge_serial_close(i64)");
    LLVM_EMITLN(e, "declare i64 @forge_serial_read_byte(i64)");
    LLVM_EMITLN(e, "declare i64 @forge_serial_bytes_available(i64)");
    LLVM_EMITLN(e, "declare void @forge_serial_read_line_ptr(%%forge_str_t*, i64)");
    LLVM_EMITLN(e, "declare void @forge_serial_write_byte(i64, i64)");
    LLVM_EMITLN(e, "declare void @forge_serial_write_str_ptr(i64, %%forge_str_t*)");
    LLVM_EMITLN(e, "declare void @forge_serial_set_timeout(i64, i64)");
    LLVM_EMITLN(e, "declare void @forge_serial_flush(i64)");
    LLVM_EMITLN(e, "declare i64 @forge_serial_is_open(i64)");
    LLVM_EMITLN(e, "declare i64 @forge_serial_get_baud(i64)");
    LLVM_NEWLINE(e);

    LLVM_EMITLN(e, "; Standard library: forge.nmea");
    LLVM_EMITLN(e, "declare i64 @forge_nmea_validate_ptr(%%forge_str_t*)");
    LLVM_EMITLN(e, "declare void @forge_nmea_checksum_ptr(%%forge_str_t*, %%forge_str_t*)");
    LLVM_EMITLN(e, "declare void @forge_nmea_sentence_type_ptr(%%forge_str_t*, %%forge_str_t*)");
    LLVM_EMITLN(e, "declare void @forge_nmea_get_talker_ptr(%%forge_str_t*, %%forge_str_t*)");
    LLVM_EMITLN(e, "declare i64 @forge_nmea_field_count_ptr(%%forge_str_t*)");
    LLVM_EMITLN(e, "declare void @forge_nmea_get_field_ptr(%%forge_str_t*, %%forge_str_t*, i64)");
    LLVM_EMITLN(e, "declare double @forge_nmea_latitude_ptr(%%forge_str_t*)");
    LLVM_EMITLN(e, "declare double @forge_nmea_longitude_ptr(%%forge_str_t*)");
    LLVM_NEWLINE(e);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Record Type Declarations
 * ═══════════════════════════════════════════════════════════════════════════ */

static void emit_record_decl(forge_llvm_emitter_t* e, forge_node_t* node) {
    if (node->kind != NODE_RECORD_DECL) return;

    LLVM_EMIT(e, "%%%s_t = type { ", node->data.record.name);

    for (int i = 0; i < node->data.record.field_count; i++) {
        if (i > 0) LLVM_EMIT(e, ", ");
        /* fields are NODE_FIELD_DEF nodes */
        forge_node_t* field = node->data.record.fields[i];
        if (field && field->resolved_type) {
            llvm_emit_type(e, field->resolved_type);
        } else {
            LLVM_EMIT(e, "i64"); /* fallback */
        }
    }

    LLVM_EMITLN(e, " }");
}


/* ═══════════════════════════════════════════════════════════════════════════
 * Expression Emission
 * Returns the SSA register number containing the result
 * ═══════════════════════════════════════════════════════════════════════════ */

int llvm_emit_expr(forge_llvm_emitter_t* e, forge_node_t* node) {
    if (!node) return -1;

    int reg;

    switch (node->kind) {
        case NODE_INT_LIT: {
            /* Integer literal - just use the value directly, no register needed */
            /* We return -1 and the caller handles embedding the constant */
            return -1; /* Handled inline */
        }

        case NODE_FLOAT_LIT: {
            return -1; /* Handled inline */
        }

        case NODE_BOOL_LIT: {
            return -1; /* Handled inline */
        }

        case NODE_STR_LIT: {
            /* String literals need to be created via runtime */
            const char* str = node->data.str_val;
            int str_idx = add_string_constant(e, str);
            int ptr_reg = llvm_next_reg(e);
            reg = llvm_next_reg(e);

            /* Get pointer to string constant */
            LLVM_EMITLN(e, "  %%%d = getelementptr [%zu x i8], [%zu x i8]* @.str.%d, i64 0, i64 0",
                        ptr_reg, strlen(str) + 1, strlen(str) + 1, str_idx);
            /* Call forge_str_lit */
            LLVM_EMITLN(e, "  %%%d = call %%forge_str_t @forge_str_lit(i8* %%%d)",
                        reg, ptr_reg);
            return reg;
        }

        case NODE_IDENT: {
            /* Load variable from stack slot */
            const char* llvm_name = llvm_lookup_var(e, node->data.name);
            reg = llvm_next_reg(e);
            LLVM_EMITLN(e, "  %%%d = load %s, %s* %%%s",
                        reg,
                        llvm_type_str(e, node->resolved_type),
                        llvm_type_str(e, node->resolved_type),
                        llvm_name);
            return reg;
        }

        case NODE_BINARY_OP: {
            forge_node_t* left = node->data.binop.left;
            forge_node_t* right = node->data.binop.right;
            int op = node->data.binop.op;

            int left_reg = llvm_emit_expr(e, left);
            int right_reg = llvm_emit_expr(e, right);
            reg = llvm_next_reg(e);

            /* Handle inline literals */
            char left_val[64], right_val[64];
            if (left_reg < 0) {
                if (left->kind == NODE_INT_LIT)
                    snprintf(left_val, sizeof(left_val), "%lld", (long long)left->data.int_val);
                else if (left->kind == NODE_FLOAT_LIT)
                    snprintf(left_val, sizeof(left_val), "%e", left->data.float_val);
                else if (left->kind == NODE_BOOL_LIT)
                    snprintf(left_val, sizeof(left_val), "%d", left->data.bool_val ? 1 : 0);
            } else {
                snprintf(left_val, sizeof(left_val), "%%%d", left_reg);
            }

            if (right_reg < 0) {
                if (right->kind == NODE_INT_LIT)
                    snprintf(right_val, sizeof(right_val), "%lld", (long long)right->data.int_val);
                else if (right->kind == NODE_FLOAT_LIT)
                    snprintf(right_val, sizeof(right_val), "%e", right->data.float_val);
                else if (right->kind == NODE_BOOL_LIT)
                    snprintf(right_val, sizeof(right_val), "%d", right->data.bool_val ? 1 : 0);
            } else {
                snprintf(right_val, sizeof(right_val), "%%%d", right_reg);
            }

            /* Determine if int or float or string operation */
            int is_float = (left->resolved_type && left->resolved_type->kind == TY_FLOAT) ||
                          (left->resolved_type && left->resolved_type->kind == TY_FLOAT32);
            int is_string = (left->resolved_type && left->resolved_type->kind == TY_STR);

            /* Emit instruction based on operator (op is token type) */
            switch (op) {
                case TOK_PLUS:
                    if (is_string) {
                        /* String concatenation - use _ptr wrapper for ABI compatibility */
                        /* NOTE: We need to allocate temps first, then the result register */
                        int tmp_left = reg; /* Reuse reg (already allocated) for first temp */
                        int tmp_right = llvm_next_reg(e);
                        LLVM_EMITLN(e, "  %%%d = alloca %%forge_str_t", tmp_left);
                        LLVM_EMITLN(e, "  %%%d = alloca %%forge_str_t", tmp_right);
                        LLVM_EMITLN(e, "  store %%forge_str_t %s, %%forge_str_t* %%%d", left_val, tmp_left);
                        LLVM_EMITLN(e, "  store %%forge_str_t %s, %%forge_str_t* %%%d", right_val, tmp_right);
                        reg = llvm_next_reg(e);
                        LLVM_EMITLN(e, "  %%%d = call %%forge_str_t @forge_str_concat_ptr(%%forge_str_t* %%%d, %%forge_str_t* %%%d)",
                                   reg, tmp_left, tmp_right);
                    } else {
                        LLVM_EMITLN(e, "  %%%d = %s %s %s, %s", reg,
                                   is_float ? "fadd" : "add",
                                   is_float ? "double" : "i64",
                                   left_val, right_val);
                    }
                    break;
                case TOK_MINUS:
                    LLVM_EMITLN(e, "  %%%d = %s %s %s, %s", reg,
                               is_float ? "fsub" : "sub",
                               is_float ? "double" : "i64",
                               left_val, right_val);
                    break;
                case TOK_STAR:
                    LLVM_EMITLN(e, "  %%%d = %s %s %s, %s", reg,
                               is_float ? "fmul" : "mul",
                               is_float ? "double" : "i64",
                               left_val, right_val);
                    break;
                case TOK_SLASH:
                    if (is_float) {
                        LLVM_EMITLN(e, "  %%%d = fdiv double %s, %s", reg, left_val, right_val);
                    } else {
                        /* Checked integer division - use reg for GEP, allocate new for result */
                        int src_idx = add_string_constant(e, e->source_file ? e->source_file : "unknown");
                        int src_ptr = reg; /* reuse pre-allocated reg for GEP */
                        LLVM_EMITLN(e, "  %%%d = getelementptr [%zu x i8], [%zu x i8]* @.str.%d, i64 0, i64 0",
                                   src_ptr, strlen(e->source_file ? e->source_file : "unknown") + 1,
                                   strlen(e->source_file ? e->source_file : "unknown") + 1, src_idx);
                        reg = llvm_next_reg(e);
                        LLVM_EMITLN(e, "  %%%d = call i64 @forge_div_check(i64 %s, i64 %s, i8* %%%d, i32 %d)",
                                   reg, left_val, right_val, src_ptr, node->line);
                    }
                    break;
                case TOK_PERCENT: {
                    /* Checked integer modulo - use reg for GEP, allocate new for result */
                    int src_idx2 = add_string_constant(e, e->source_file ? e->source_file : "unknown");
                    int src_ptr2 = reg; /* reuse pre-allocated reg for GEP */
                    LLVM_EMITLN(e, "  %%%d = getelementptr [%zu x i8], [%zu x i8]* @.str.%d, i64 0, i64 0",
                               src_ptr2, strlen(e->source_file ? e->source_file : "unknown") + 1,
                               strlen(e->source_file ? e->source_file : "unknown") + 1, src_idx2);
                    reg = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = call i64 @forge_mod_check(i64 %s, i64 %s, i8* %%%d, i32 %d)",
                               reg, left_val, right_val, src_ptr2, node->line);
                    break;
                }
                case TOK_EQ:
                    if (is_string) {
                        /* String equality - use _ptr wrapper for ABI compatibility */
                        int tmp_left = reg; /* Reuse reg for first temp */
                        int tmp_right = llvm_next_reg(e);
                        LLVM_EMITLN(e, "  %%%d = alloca %%forge_str_t", tmp_left);
                        LLVM_EMITLN(e, "  %%%d = alloca %%forge_str_t", tmp_right);
                        LLVM_EMITLN(e, "  store %%forge_str_t %s, %%forge_str_t* %%%d", left_val, tmp_left);
                        LLVM_EMITLN(e, "  store %%forge_str_t %s, %%forge_str_t* %%%d", right_val, tmp_right);
                        reg = llvm_next_reg(e);
                        LLVM_EMITLN(e, "  %%%d = call i1 @forge_str_equal_ptr(%%forge_str_t* %%%d, %%forge_str_t* %%%d)",
                                   reg, tmp_left, tmp_right);
                    } else {
                        LLVM_EMITLN(e, "  %%%d = %s %s %s, %s", reg,
                                   is_float ? "fcmp oeq" : "icmp eq",
                                   is_float ? "double" : "i64",
                                   left_val, right_val);
                    }
                    break;
                case TOK_NEQ:
                    if (is_string) {
                        /* String inequality - use _ptr wrapper for ABI compatibility */
                        int tmp_left = reg; /* Reuse reg for first temp */
                        int tmp_right = llvm_next_reg(e);
                        LLVM_EMITLN(e, "  %%%d = alloca %%forge_str_t", tmp_left);
                        LLVM_EMITLN(e, "  %%%d = alloca %%forge_str_t", tmp_right);
                        LLVM_EMITLN(e, "  store %%forge_str_t %s, %%forge_str_t* %%%d", left_val, tmp_left);
                        LLVM_EMITLN(e, "  store %%forge_str_t %s, %%forge_str_t* %%%d", right_val, tmp_right);
                        int eq_reg = llvm_next_reg(e);
                        LLVM_EMITLN(e, "  %%%d = call i1 @forge_str_equal_ptr(%%forge_str_t* %%%d, %%forge_str_t* %%%d)",
                                   eq_reg, tmp_left, tmp_right);
                        reg = llvm_next_reg(e);
                        LLVM_EMITLN(e, "  %%%d = xor i1 %%%d, 1", reg, eq_reg);
                    } else {
                        LLVM_EMITLN(e, "  %%%d = %s %s %s, %s", reg,
                                   is_float ? "fcmp one" : "icmp ne",
                                   is_float ? "double" : "i64",
                                   left_val, right_val);
                    }
                    break;
                case TOK_LT:
                    LLVM_EMITLN(e, "  %%%d = %s %s %s, %s", reg,
                               is_float ? "fcmp olt" : "icmp slt",
                               is_float ? "double" : "i64",
                               left_val, right_val);
                    break;
                case TOK_LEQ:
                    LLVM_EMITLN(e, "  %%%d = %s %s %s, %s", reg,
                               is_float ? "fcmp ole" : "icmp sle",
                               is_float ? "double" : "i64",
                               left_val, right_val);
                    break;
                case TOK_GT:
                    LLVM_EMITLN(e, "  %%%d = %s %s %s, %s", reg,
                               is_float ? "fcmp ogt" : "icmp sgt",
                               is_float ? "double" : "i64",
                               left_val, right_val);
                    break;
                case TOK_GEQ:
                    LLVM_EMITLN(e, "  %%%d = %s %s %s, %s", reg,
                               is_float ? "fcmp oge" : "icmp sge",
                               is_float ? "double" : "i64",
                               left_val, right_val);
                    break;
                case TOK_AND:
                    LLVM_EMITLN(e, "  %%%d = and i1 %s, %s", reg, left_val, right_val);
                    break;
                case TOK_OR:
                    LLVM_EMITLN(e, "  %%%d = or i1 %s, %s", reg, left_val, right_val);
                    break;
            }
            return reg;
        }

        case NODE_UNARY_OP: {
            int op = node->data.unop.op;
            forge_node_t* operand = node->data.unop.operand;
            int operand_reg = llvm_emit_expr(e, operand);
            reg = llvm_next_reg(e);

            if (op == TOK_MINUS) {
                /* Negate: 0 - value */
                forge_type_t* operand_type = operand->resolved_type;
                int is_float = operand_type && (operand_type->kind == TY_FLOAT || operand_type->kind == TY_FLOAT32);

                if (operand_reg < 0) {
                    /* Inline literal */
                    if (operand->kind == NODE_INT_LIT) {
                        LLVM_EMITLN(e, "  %%%d = sub i64 0, %lld", reg,
                                   (long long)operand->data.int_val);
                    } else if (operand->kind == NODE_FLOAT_LIT) {
                        LLVM_EMITLN(e, "  %%%d = fsub double 0.0, %e", reg,
                                   operand->data.float_val);
                    }
                } else {
                    if (is_float) {
                        LLVM_EMITLN(e, "  %%%d = fsub double 0.0, %%%d", reg, operand_reg);
                    } else {
                        LLVM_EMITLN(e, "  %%%d = sub i64 0, %%%d", reg, operand_reg);
                    }
                }
            } else if (op == TOK_NOT) {
                if (operand_reg < 0) {
                    /* Inline boolean literal */
                    if (operand->kind == NODE_BOOL_LIT) {
                        LLVM_EMITLN(e, "  %%%d = xor i1 %d, 1", reg,
                                   operand->data.bool_val ? 1 : 0);
                    }
                } else {
                    LLVM_EMITLN(e, "  %%%d = xor i1 %%%d, 1", reg, operand_reg);
                }
            }
            return reg;
        }

        case NODE_CAST: {
            /* Type conversion: int(expr), float(expr), str(expr), etc. */
            forge_node_t* target_type = node->data.cast.target_type;
            forge_node_t* expr = node->data.cast.expr;
            int expr_reg = llvm_emit_expr(e, expr);

            forge_type_t* from_type = expr->resolved_type;
            forge_type_t* to_type = node->resolved_type;

            if (!to_type || !target_type) {
                return expr_reg;
            }

            /* Determine target type kind from the type node name */
            const char* type_name = NULL;
            if (target_type->kind == NODE_TYPE_PRIM) {
                type_name = target_type->data.name;
            }

            reg = llvm_next_reg(e);

            /* Handle conversion to str */
            if (type_name && strcmp(type_name, "str") == 0) {
                if (from_type && from_type->kind == TY_INT) {
                    if (expr_reg < 0 && expr->kind == NODE_INT_LIT) {
                        LLVM_EMITLN(e, "  %%%d = call %%forge_str_t @forge_str_from_int(i64 %lld)",
                                   reg, (long long)expr->data.int_val);
                    } else {
                        LLVM_EMITLN(e, "  %%%d = call %%forge_str_t @forge_str_from_int(i64 %%%d)",
                                   reg, expr_reg);
                    }
                } else if (from_type && (from_type->kind == TY_FLOAT || from_type->kind == TY_FLOAT32)) {
                    if (expr_reg < 0 && expr->kind == NODE_FLOAT_LIT) {
                        LLVM_EMITLN(e, "  %%%d = call %%forge_str_t @forge_str_from_float(double %e)",
                                   reg, expr->data.float_val);
                    } else {
                        LLVM_EMITLN(e, "  %%%d = call %%forge_str_t @forge_str_from_float(double %%%d)",
                                   reg, expr_reg);
                    }
                } else if (from_type && from_type->kind == TY_BOOL) {
                    if (expr_reg < 0 && expr->kind == NODE_BOOL_LIT) {
                        LLVM_EMITLN(e, "  %%%d = call %%forge_str_t @forge_str_from_bool(i1 %d)",
                                   reg, expr->data.bool_val ? 1 : 0);
                    } else {
                        LLVM_EMITLN(e, "  %%%d = call %%forge_str_t @forge_str_from_bool(i1 %%%d)",
                                   reg, expr_reg);
                    }
                } else if (from_type && from_type->kind == TY_STR) {
                    /* Already a string */
                    return expr_reg;
                } else {
                    /* Fallback: try int conversion */
                    if (expr_reg >= 0) {
                        LLVM_EMITLN(e, "  %%%d = call %%forge_str_t @forge_str_from_int(i64 %%%d)",
                                   reg, expr_reg);
                    } else {
                        LLVM_EMITLN(e, "  %%%d = call %%forge_str_t @forge_str_lit(i8* null)", reg);
                    }
                }
                return reg;
            }

            /* Handle conversion to int */
            if (type_name && strcmp(type_name, "int") == 0) {
                if (from_type && (from_type->kind == TY_FLOAT || from_type->kind == TY_FLOAT32)) {
                    LLVM_EMITLN(e, "  %%%d = fptosi double %%%d to i64", reg, expr_reg);
                } else if (from_type && from_type->kind == TY_BOOL) {
                    LLVM_EMITLN(e, "  %%%d = zext i1 %%%d to i64", reg, expr_reg);
                } else {
                    /* Already int or compatible */
                    return expr_reg;
                }
                return reg;
            }

            /* Handle conversion to float */
            if (type_name && strcmp(type_name, "float") == 0) {
                if (from_type && from_type->kind == TY_INT) {
                    if (expr_reg < 0 && expr->kind == NODE_INT_LIT) {
                        LLVM_EMITLN(e, "  %%%d = sitofp i64 %lld to double",
                                   reg, (long long)expr->data.int_val);
                    } else {
                        LLVM_EMITLN(e, "  %%%d = sitofp i64 %%%d to double", reg, expr_reg);
                    }
                } else {
                    /* Already float or compatible */
                    return expr_reg;
                }
                return reg;
            }

            /* Handle conversion to bool */
            if (type_name && strcmp(type_name, "bool") == 0) {
                if (from_type && from_type->kind == TY_INT) {
                    LLVM_EMITLN(e, "  %%%d = icmp ne i64 %%%d, 0", reg, expr_reg);
                } else {
                    return expr_reg;
                }
                return reg;
            }

            /* Default: return the expression as-is */
            return expr_reg;
        }

        case NODE_CALL: {
            /* Function call - callee can be an identifier or stdlib field access */
            forge_node_t* callee = node->data.call.callee;
            const char* func_name = NULL;
            char stdlib_name[128];
            int is_stdlib_call = 0;

            if (callee && callee->kind == NODE_IDENT) {
                func_name = callee->data.name;
            } else if (callee && callee->kind == NODE_FIELD_ACCESS) {
                /* Check for forge.*.func() pattern */
                forge_node_t* inner = callee->data.field_access.object;
                if (inner && inner->kind == NODE_FIELD_ACCESS) {
                    forge_node_t* root = inner->data.field_access.object;
                    if (root && root->kind == NODE_IDENT &&
                        strcmp(root->data.name, "forge") == 0) {
                        /* Build function name: forge_module_func */
                        snprintf(stdlib_name, sizeof(stdlib_name),
                                 "forge_%s_%s",
                                 inner->data.field_access.field_name,
                                 callee->data.field_access.field_name);
                        func_name = stdlib_name;
                        is_stdlib_call = 1;
                    }
                }
            }

            if (!func_name) {
                /* TODO: Handle other callee types */
                return -1;
            }
            int arg_count = node->data.call.arg_count;

            /* Handle builtin print specially */
            if (strcmp(func_name, "print") == 0 && arg_count >= 1) {
                int arg_reg = llvm_emit_expr(e, node->data.call.args[0]);
                forge_type_t* arg_type = node->data.call.args[0]->resolved_type;

                /* Convert to string if needed */
                int str_reg;
                if (arg_type && arg_type->kind == TY_STR) {
                    str_reg = arg_reg;
                } else if (arg_type && arg_type->kind == TY_INT) {
                    str_reg = llvm_next_reg(e);
                    if (arg_reg < 0) {
                        LLVM_EMITLN(e, "  %%%d = call %%forge_str_t @forge_str_from_int(i64 %lld)",
                                   str_reg, (long long)node->data.call.args[0]->data.int_val);
                    } else {
                        LLVM_EMITLN(e, "  %%%d = call %%forge_str_t @forge_str_from_int(i64 %%%d)",
                                   str_reg, arg_reg);
                    }
                } else if (arg_type && (arg_type->kind == TY_FLOAT || arg_type->kind == TY_FLOAT32)) {
                    str_reg = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = call %%forge_str_t @forge_str_from_float(double %%%d)",
                               str_reg, arg_reg);
                } else if (arg_type && arg_type->kind == TY_BOOL) {
                    str_reg = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = call %%forge_str_t @forge_str_from_bool(i1 %%%d)",
                               str_reg, arg_reg);
                } else {
                    str_reg = arg_reg;
                }

                LLVM_EMITLN(e, "  call void @forge_print(%%forge_str_t %%%d)", str_reg);
                return -1;
            }

            /* Handle len() builtin */
            if (strcmp(func_name, "len") == 0 && arg_count >= 1) {
                forge_type_t* arg_type = node->data.call.args[0]->resolved_type;
                int arg_reg = llvm_emit_expr(e, node->data.call.args[0]);

                if (arg_type && arg_type->kind == TY_STR) {
                    /* String length - use _ptr wrapper for ABI compatibility */
                    int temp_ptr = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = alloca %%forge_str_t", temp_ptr);
                    LLVM_EMITLN(e, "  store %%forge_str_t %%%d, %%forge_str_t* %%%d", arg_reg, temp_ptr);
                    reg = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = call i64 @forge_str_len_ptr(%%forge_str_t* %%%d)", reg, temp_ptr);
                    return reg;
                } else if (arg_type && (arg_type->kind == TY_FIXED_ARRAY || arg_type->kind == TY_DYN_ARRAY)) {
                    /* For arrays, store to temp and pass pointer to forge_array_len */
                    int temp_ptr = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = alloca %%forge_array_t", temp_ptr);
                    LLVM_EMITLN(e, "  store %%forge_array_t %%%d, %%forge_array_t* %%%d", arg_reg, temp_ptr);
                    int result_reg = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = call i64 @forge_array_len(%%forge_array_t* %%%d)", result_reg, temp_ptr);
                    return result_reg;
                } else {
                    /* Fallback: string - use _ptr wrapper for ABI compatibility */
                    int temp_ptr = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = alloca %%forge_str_t", temp_ptr);
                    LLVM_EMITLN(e, "  store %%forge_str_t %%%d, %%forge_str_t* %%%d", arg_reg, temp_ptr);
                    reg = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = call i64 @forge_str_len_ptr(%%forge_str_t* %%%d)", reg, temp_ptr);
                    return reg;
                }
            }

            /* Handle str() builtin - type conversion to string */
            if (strcmp(func_name, "str") == 0 && arg_count >= 1) {
                forge_type_t* arg_type = node->data.call.args[0]->resolved_type;
                forge_node_t* arg_node = node->data.call.args[0];
                int arg_reg = llvm_emit_expr(e, arg_node);
                reg = llvm_next_reg(e);

                if (arg_type && arg_type->kind == TY_INT) {
                    if (arg_reg < 0 && arg_node->kind == NODE_INT_LIT) {
                        LLVM_EMITLN(e, "  %%%d = call %%forge_str_t @forge_str_from_int(i64 %lld)",
                                   reg, (long long)arg_node->data.int_val);
                    } else {
                        LLVM_EMITLN(e, "  %%%d = call %%forge_str_t @forge_str_from_int(i64 %%%d)", reg, arg_reg);
                    }
                } else if (arg_type && (arg_type->kind == TY_FLOAT || arg_type->kind == TY_FLOAT32)) {
                    if (arg_reg < 0 && arg_node->kind == NODE_FLOAT_LIT) {
                        LLVM_EMITLN(e, "  %%%d = call %%forge_str_t @forge_str_from_float(double %e)",
                                   reg, arg_node->data.float_val);
                    } else {
                        LLVM_EMITLN(e, "  %%%d = call %%forge_str_t @forge_str_from_float(double %%%d)", reg, arg_reg);
                    }
                } else if (arg_type && arg_type->kind == TY_BOOL) {
                    if (arg_reg < 0 && arg_node->kind == NODE_BOOL_LIT) {
                        LLVM_EMITLN(e, "  %%%d = call %%forge_str_t @forge_str_from_bool(i1 %d)",
                                   reg, arg_node->data.bool_val ? 1 : 0);
                    } else {
                        LLVM_EMITLN(e, "  %%%d = call %%forge_str_t @forge_str_from_bool(i1 %%%d)", reg, arg_reg);
                    }
                } else if (arg_type && arg_type->kind == TY_STR) {
                    /* Already a string, just return it */
                    return arg_reg;
                } else {
                    /* Fallback: treat as int - with literal handling */
                    if (arg_reg < 0 && arg_node->kind == NODE_INT_LIT) {
                        LLVM_EMITLN(e, "  %%%d = call %%forge_str_t @forge_str_from_int(i64 %lld)",
                                   reg, (long long)arg_node->data.int_val);
                    } else if (arg_reg >= 0) {
                        LLVM_EMITLN(e, "  %%%d = call %%forge_str_t @forge_str_from_int(i64 %%%d)", reg, arg_reg);
                    } else {
                        /* Unknown literal type - try to get a default */
                        LLVM_EMITLN(e, "  %%%d = call %%forge_str_t @forge_str_lit(i8* null)", reg);
                    }
                }
                return reg;
            }

            /* Handle forge.str.* functions - use _ptr wrappers for ABI compatibility */
            if (is_stdlib_call && strncmp(func_name, "forge_str_", 10) == 0) {
                const char* str_func = func_name + 10; /* Skip "forge_str_" prefix */

                /* Functions that take string pointers instead of values */
                /* len: 1 str arg -> i64 */
                /* contains, starts_with, ends_with, find, count: 2 str args */
                /* upper, lower, trim, trim_left, trim_right, repeat, reverse, char_at: 1 str arg */
                /* substr: 1 str + 2 int args */
                /* replace, split: multiple str args */
                /* join: array ptr + str (already handled differently) */

                if (strcmp(str_func, "len") == 0) {
                    /* forge.str.len - one string arg, returns i64 */
                    int arg0_reg = llvm_emit_expr(e, node->data.call.args[0]);
                    int tmp0 = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = alloca %%forge_str_t", tmp0);
                    LLVM_EMITLN(e, "  store %%forge_str_t %%%d, %%forge_str_t* %%%d", arg0_reg, tmp0);
                    reg = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = call i64 @forge_str_len_ptr(%%forge_str_t* %%%d)", reg, tmp0);
                    return reg;
                }

                if (strcmp(str_func, "contains") == 0 ||
                    strcmp(str_func, "starts_with") == 0 ||
                    strcmp(str_func, "ends_with") == 0 ||
                    strcmp(str_func, "find") == 0 ||
                    strcmp(str_func, "count") == 0) {
                    /* Two string args - store both to temps and pass pointers */
                    int arg0_reg = llvm_emit_expr(e, node->data.call.args[0]);
                    int arg1_reg = llvm_emit_expr(e, node->data.call.args[1]);
                    int tmp0 = llvm_next_reg(e);
                    int tmp1 = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = alloca %%forge_str_t", tmp0);
                    LLVM_EMITLN(e, "  %%%d = alloca %%forge_str_t", tmp1);
                    LLVM_EMITLN(e, "  store %%forge_str_t %%%d, %%forge_str_t* %%%d", arg0_reg, tmp0);
                    LLVM_EMITLN(e, "  store %%forge_str_t %%%d, %%forge_str_t* %%%d", arg1_reg, tmp1);
                    reg = llvm_next_reg(e);
                    const char* ret_type = (strcmp(str_func, "find") == 0 || strcmp(str_func, "count") == 0) ? "i64" : "i1";
                    LLVM_EMITLN(e, "  %%%d = call %s @%s_ptr(%%forge_str_t* %%%d, %%forge_str_t* %%%d)",
                               reg, ret_type, func_name, tmp0, tmp1);
                    return reg;
                }

                if (strcmp(str_func, "upper") == 0 ||
                    strcmp(str_func, "lower") == 0 ||
                    strcmp(str_func, "trim") == 0 ||
                    strcmp(str_func, "trim_left") == 0 ||
                    strcmp(str_func, "trim_right") == 0 ||
                    strcmp(str_func, "reverse") == 0) {
                    /* One string arg - store to temp and pass pointer */
                    int arg0_reg = llvm_emit_expr(e, node->data.call.args[0]);
                    int tmp0 = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = alloca %%forge_str_t", tmp0);
                    LLVM_EMITLN(e, "  store %%forge_str_t %%%d, %%forge_str_t* %%%d", arg0_reg, tmp0);
                    reg = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = call %%forge_str_t @%s_ptr(%%forge_str_t* %%%d)",
                               reg, func_name, tmp0);
                    return reg;
                }

                if (strcmp(str_func, "repeat") == 0 ||
                    strcmp(str_func, "char_at") == 0) {
                    /* str + int arg */
                    int arg0_reg = llvm_emit_expr(e, node->data.call.args[0]);
                    int arg1_reg = llvm_emit_expr(e, node->data.call.args[1]);
                    int tmp0 = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = alloca %%forge_str_t", tmp0);
                    LLVM_EMITLN(e, "  store %%forge_str_t %%%d, %%forge_str_t* %%%d", arg0_reg, tmp0);
                    reg = llvm_next_reg(e);
                    if (arg1_reg < 0) {
                        LLVM_EMITLN(e, "  %%%d = call %%forge_str_t @%s_ptr(%%forge_str_t* %%%d, i64 %lld)",
                                   reg, func_name, tmp0, (long long)node->data.call.args[1]->data.int_val);
                    } else {
                        LLVM_EMITLN(e, "  %%%d = call %%forge_str_t @%s_ptr(%%forge_str_t* %%%d, i64 %%%d)",
                                   reg, func_name, tmp0, arg1_reg);
                    }
                    return reg;
                }

                if (strcmp(str_func, "substr") == 0) {
                    /* str + 2 int args */
                    int arg0_reg = llvm_emit_expr(e, node->data.call.args[0]);
                    int arg1_reg = llvm_emit_expr(e, node->data.call.args[1]);
                    int arg2_reg = llvm_emit_expr(e, node->data.call.args[2]);
                    int tmp0 = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = alloca %%forge_str_t", tmp0);
                    LLVM_EMITLN(e, "  store %%forge_str_t %%%d, %%forge_str_t* %%%d", arg0_reg, tmp0);
                    reg = llvm_next_reg(e);
                    /* Handle literal args */
                    char arg1_str[64], arg2_str[64];
                    if (arg1_reg < 0) {
                        snprintf(arg1_str, sizeof(arg1_str), "%lld", (long long)node->data.call.args[1]->data.int_val);
                    } else {
                        snprintf(arg1_str, sizeof(arg1_str), "%%%d", arg1_reg);
                    }
                    if (arg2_reg < 0) {
                        snprintf(arg2_str, sizeof(arg2_str), "%lld", (long long)node->data.call.args[2]->data.int_val);
                    } else {
                        snprintf(arg2_str, sizeof(arg2_str), "%%%d", arg2_reg);
                    }
                    LLVM_EMITLN(e, "  %%%d = call %%forge_str_t @%s_ptr(%%forge_str_t* %%%d, i64 %s, i64 %s)",
                               reg, func_name, tmp0, arg1_str, arg2_str);
                    return reg;
                }

                if (strcmp(str_func, "replace") == 0) {
                    /* 3 string args */
                    int arg0_reg = llvm_emit_expr(e, node->data.call.args[0]);
                    int arg1_reg = llvm_emit_expr(e, node->data.call.args[1]);
                    int arg2_reg = llvm_emit_expr(e, node->data.call.args[2]);
                    int tmp0 = llvm_next_reg(e);
                    int tmp1 = llvm_next_reg(e);
                    int tmp2 = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = alloca %%forge_str_t", tmp0);
                    LLVM_EMITLN(e, "  %%%d = alloca %%forge_str_t", tmp1);
                    LLVM_EMITLN(e, "  %%%d = alloca %%forge_str_t", tmp2);
                    LLVM_EMITLN(e, "  store %%forge_str_t %%%d, %%forge_str_t* %%%d", arg0_reg, tmp0);
                    LLVM_EMITLN(e, "  store %%forge_str_t %%%d, %%forge_str_t* %%%d", arg1_reg, tmp1);
                    LLVM_EMITLN(e, "  store %%forge_str_t %%%d, %%forge_str_t* %%%d", arg2_reg, tmp2);
                    reg = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = call %%forge_str_t @%s_ptr(%%forge_str_t* %%%d, %%forge_str_t* %%%d, %%forge_str_t* %%%d)",
                               reg, func_name, tmp0, tmp1, tmp2);
                    return reg;
                }

                if (strcmp(str_func, "split") == 0) {
                    /* 2 string args, returns array */
                    int arg0_reg = llvm_emit_expr(e, node->data.call.args[0]);
                    int arg1_reg = llvm_emit_expr(e, node->data.call.args[1]);
                    int tmp0 = llvm_next_reg(e);
                    int tmp1 = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = alloca %%forge_str_t", tmp0);
                    LLVM_EMITLN(e, "  %%%d = alloca %%forge_str_t", tmp1);
                    LLVM_EMITLN(e, "  store %%forge_str_t %%%d, %%forge_str_t* %%%d", arg0_reg, tmp0);
                    LLVM_EMITLN(e, "  store %%forge_str_t %%%d, %%forge_str_t* %%%d", arg1_reg, tmp1);
                    reg = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = call %%forge_array_t @%s_ptr(%%forge_str_t* %%%d, %%forge_str_t* %%%d)",
                               reg, func_name, tmp0, tmp1);
                    return reg;
                }

                if (strcmp(str_func, "join") == 0) {
                    /* array ptr + str (join already takes array by pointer) */
                    int arg0_reg = llvm_emit_expr(e, node->data.call.args[0]);
                    int arg1_reg = llvm_emit_expr(e, node->data.call.args[1]);
                    /* arg0 is already an array, need to pass its address */
                    int arr_ptr = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = alloca %%forge_array_t", arr_ptr);
                    LLVM_EMITLN(e, "  store %%forge_array_t %%%d, %%forge_array_t* %%%d", arg0_reg, arr_ptr);
                    reg = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = call %%forge_str_t @forge_str_join(%%forge_array_t* %%%d, %%forge_str_t %%%d)",
                               reg, arr_ptr, arg1_reg);
                    return reg;
                }

                /* len is handled above by the "len" builtin check */
            }

            /* Handle forge.math.* functions - convert int args to double when needed */
            if (is_stdlib_call && strncmp(func_name, "forge_math_", 11) == 0) {
                const char* math_func = func_name + 11; /* Skip "forge_math_" prefix */

                /* Helper: emit arg and get a string representation suitable for double param */
                char double_arg_strs[4][64];

                #define EMIT_DOUBLE_ARG(idx) do { \
                    forge_node_t* arg_node = node->data.call.args[idx]; \
                    forge_type_t* arg_type = arg_node->resolved_type; \
                    if (arg_node->kind == NODE_FLOAT_LIT) { \
                        /* Float literal - emit directly */ \
                        snprintf(double_arg_strs[idx], sizeof(double_arg_strs[idx]), "%e", arg_node->data.float_val); \
                    } else if (arg_node->kind == NODE_INT_LIT) { \
                        /* Int literal - convert to double */ \
                        int conv_reg = llvm_next_reg(e); \
                        LLVM_EMITLN(e, "  %%%d = sitofp i64 %lld to double", conv_reg, (long long)arg_node->data.int_val); \
                        snprintf(double_arg_strs[idx], sizeof(double_arg_strs[idx]), "%%%d", conv_reg); \
                    } else { \
                        int arg_reg = llvm_emit_expr(e, arg_node); \
                        if (arg_type && arg_type->kind == TY_INT) { \
                            int conv_reg = llvm_next_reg(e); \
                            LLVM_EMITLN(e, "  %%%d = sitofp i64 %%%d to double", conv_reg, arg_reg); \
                            snprintf(double_arg_strs[idx], sizeof(double_arg_strs[idx]), "%%%d", conv_reg); \
                        } else { \
                            snprintf(double_arg_strs[idx], sizeof(double_arg_strs[idx]), "%%%d", arg_reg); \
                        } \
                    } \
                } while(0)

                /* Functions taking 1 double, returning double */
                if (strcmp(math_func, "abs") == 0 ||
                    strcmp(math_func, "sqrt") == 0 ||
                    strcmp(math_func, "cbrt") == 0 ||
                    strcmp(math_func, "floor") == 0 ||
                    strcmp(math_func, "ceil") == 0 ||
                    strcmp(math_func, "round") == 0 ||
                    strcmp(math_func, "trunc") == 0 ||
                    strcmp(math_func, "sin") == 0 ||
                    strcmp(math_func, "cos") == 0 ||
                    strcmp(math_func, "tan") == 0 ||
                    strcmp(math_func, "log") == 0 ||
                    strcmp(math_func, "log10") == 0 ||
                    strcmp(math_func, "log2") == 0 ||
                    strcmp(math_func, "exp") == 0) {
                    EMIT_DOUBLE_ARG(0);
                    reg = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = call double @%s(double %s)",
                               reg, func_name, double_arg_strs[0]);
                    return reg;
                }

                /* Functions taking 2 doubles, returning double */
                if (strcmp(math_func, "min") == 0 ||
                    strcmp(math_func, "max") == 0 ||
                    strcmp(math_func, "pow") == 0 ||
                    strcmp(math_func, "atan2") == 0) {
                    EMIT_DOUBLE_ARG(0);
                    EMIT_DOUBLE_ARG(1);
                    reg = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = call double @%s(double %s, double %s)",
                               reg, func_name, double_arg_strs[0], double_arg_strs[1]);
                    return reg;
                }

                /* clamp: 3 doubles -> double */
                if (strcmp(math_func, "clamp") == 0) {
                    EMIT_DOUBLE_ARG(0);
                    EMIT_DOUBLE_ARG(1);
                    EMIT_DOUBLE_ARG(2);
                    reg = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = call double @%s(double %s, double %s, double %s)",
                               reg, func_name, double_arg_strs[0], double_arg_strs[1], double_arg_strs[2]);
                    return reg;
                }

                /* Integer functions (no conversion needed) */
                if (strcmp(math_func, "abs_int") == 0) {
                    int arg_reg = llvm_emit_expr(e, node->data.call.args[0]);
                    reg = llvm_next_reg(e);
                    if (arg_reg < 0) {
                        LLVM_EMITLN(e, "  %%%d = call i64 @%s(i64 %lld)",
                                   reg, func_name, (long long)node->data.call.args[0]->data.int_val);
                    } else {
                        LLVM_EMITLN(e, "  %%%d = call i64 @%s(i64 %%%d)", reg, func_name, arg_reg);
                    }
                    return reg;
                }

                if (strcmp(math_func, "min_int") == 0 ||
                    strcmp(math_func, "max_int") == 0 ||
                    strcmp(math_func, "random_int") == 0) {
                    int arg0_reg = llvm_emit_expr(e, node->data.call.args[0]);
                    int arg1_reg = llvm_emit_expr(e, node->data.call.args[1]);
                    reg = llvm_next_reg(e);
                    char arg0_str[32], arg1_str[32];
                    if (arg0_reg < 0) snprintf(arg0_str, sizeof(arg0_str), "%lld", (long long)node->data.call.args[0]->data.int_val);
                    else snprintf(arg0_str, sizeof(arg0_str), "%%%d", arg0_reg);
                    if (arg1_reg < 0) snprintf(arg1_str, sizeof(arg1_str), "%lld", (long long)node->data.call.args[1]->data.int_val);
                    else snprintf(arg1_str, sizeof(arg1_str), "%%%d", arg1_reg);
                    LLVM_EMITLN(e, "  %%%d = call i64 @%s(i64 %s, i64 %s)", reg, func_name, arg0_str, arg1_str);
                    return reg;
                }

                /* random_float: no args, returns double */
                if (strcmp(math_func, "random_float") == 0) {
                    reg = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = call double @%s()", reg, func_name);
                    return reg;
                }

                /* seed_random: 1 int arg, returns void */
                if (strcmp(math_func, "seed_random") == 0) {
                    int arg_reg = llvm_emit_expr(e, node->data.call.args[0]);
                    if (arg_reg < 0) {
                        LLVM_EMITLN(e, "  call void @%s(i64 %lld)",
                                   func_name, (long long)node->data.call.args[0]->data.int_val);
                    } else {
                        LLVM_EMITLN(e, "  call void @%s(i64 %%%d)", func_name, arg_reg);
                    }
                    return -1;
                }

                #undef EMIT_DOUBLE_ARG
            }

            /* Handle forge.sys.* functions */
            if (is_stdlib_call && strncmp(func_name, "forge_sys_", 10) == 0) {
                const char* sys_func = func_name + 10; /* Skip "forge_sys_" prefix */

                /* args: no args, returns array */
                if (strcmp(sys_func, "args") == 0) {
                    reg = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = call %%forge_array_t @forge_sys_args()", reg);
                    return reg;
                }

                /* env: 1 string arg, returns string - use _ptr wrapper */
                if (strcmp(sys_func, "env") == 0) {
                    int arg0_reg = llvm_emit_expr(e, node->data.call.args[0]);
                    int tmp0 = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = alloca %%forge_str_t", tmp0);
                    LLVM_EMITLN(e, "  store %%forge_str_t %%%d, %%forge_str_t* %%%d", arg0_reg, tmp0);
                    reg = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = call %%forge_str_t @forge_sys_env_ptr(%%forge_str_t* %%%d)",
                               reg, tmp0);
                    return reg;
                }

                /* exit: 1 int arg, returns void */
                if (strcmp(sys_func, "exit") == 0) {
                    int arg_reg = llvm_emit_expr(e, node->data.call.args[0]);
                    if (arg_reg < 0) {
                        LLVM_EMITLN(e, "  call void @forge_sys_exit(i64 %lld)",
                                   (long long)node->data.call.args[0]->data.int_val);
                    } else {
                        LLVM_EMITLN(e, "  call void @forge_sys_exit(i64 %%%d)", arg_reg);
                    }
                    return -1;
                }

                /* halt: no args, returns void */
                if (strcmp(sys_func, "halt") == 0) {
                    LLVM_EMITLN(e, "  call void @forge_sys_halt()");
                    return -1;
                }

                /* platform: no args, returns string */
                if (strcmp(sys_func, "platform") == 0) {
                    reg = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = call %%forge_str_t @forge_sys_platform()", reg);
                    return reg;
                }

                /* arch: no args, returns string */
                if (strcmp(sys_func, "arch") == 0) {
                    reg = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = call %%forge_str_t @forge_sys_arch()", reg);
                    return reg;
                }
            }

            /* Handle forge.time.* functions */
            if (is_stdlib_call && strncmp(func_name, "forge_time_", 11) == 0) {
                const char* time_func = func_name + 11; /* Skip "forge_time_" prefix */

                /* now: no args, returns i64 (uint) */
                if (strcmp(time_func, "now") == 0) {
                    reg = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = call i64 @forge_time_now()", reg);
                    return reg;
                }

                /* sleep: 1 int arg, returns void */
                if (strcmp(time_func, "sleep") == 0) {
                    int arg_reg = llvm_emit_expr(e, node->data.call.args[0]);
                    if (arg_reg < 0) {
                        LLVM_EMITLN(e, "  call void @forge_time_sleep(i64 %lld)",
                                   (long long)node->data.call.args[0]->data.int_val);
                    } else {
                        LLVM_EMITLN(e, "  call void @forge_time_sleep(i64 %%%d)", arg_reg);
                    }
                    return -1;
                }

                /* timestamp: no args, returns string */
                if (strcmp(time_func, "timestamp") == 0) {
                    reg = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = call %%forge_str_t @forge_time_timestamp()", reg);
                    return reg;
                }

                /* elapsed_ms: 1 int arg, returns i64 */
                if (strcmp(time_func, "elapsed_ms") == 0) {
                    int arg_reg = llvm_emit_expr(e, node->data.call.args[0]);
                    reg = llvm_next_reg(e);
                    if (arg_reg < 0) {
                        LLVM_EMITLN(e, "  %%%d = call i64 @forge_time_elapsed_ms(i64 %lld)",
                                   reg, (long long)node->data.call.args[0]->data.int_val);
                    } else {
                        LLVM_EMITLN(e, "  %%%d = call i64 @forge_time_elapsed_ms(i64 %%%d)",
                                   reg, arg_reg);
                    }
                    return reg;
                }

                /* start_clock: use simple version that returns i64 */
                if (strcmp(time_func, "start_clock") == 0) {
                    reg = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = call i64 @forge_time_start_clock_simple()", reg);
                    return reg;
                }

                /* lap: use simple version that takes i64 and returns i64 */
                if (strcmp(time_func, "lap") == 0) {
                    int arg_reg = llvm_emit_expr(e, node->data.call.args[0]);
                    reg = llvm_next_reg(e);
                    if (arg_reg < 0) {
                        LLVM_EMITLN(e, "  %%%d = call i64 @forge_time_lap_simple(i64 %lld)",
                                   reg, (long long)node->data.call.args[0]->data.int_val);
                    } else {
                        LLVM_EMITLN(e, "  %%%d = call i64 @forge_time_lap_simple(i64 %%%d)",
                                   reg, arg_reg);
                    }
                    return reg;
                }
            }

            /* Handle forge.buf.* functions */
            if (is_stdlib_call && strncmp(func_name, "forge_buf_", 10) == 0) {
                const char* buf_func = func_name + 10; /* Skip "forge_buf_" prefix */

                /* create: 1 int arg, returns int (handle) */
                if (strcmp(buf_func, "create") == 0) {
                    int arg_reg = llvm_emit_expr(e, node->data.call.args[0]);
                    reg = llvm_next_reg(e);
                    if (arg_reg < 0) {
                        LLVM_EMITLN(e, "  %%%d = call i64 @forge_buf_create(i64 %lld)",
                                   reg, (long long)node->data.call.args[0]->data.int_val);
                    } else {
                        LLVM_EMITLN(e, "  %%%d = call i64 @forge_buf_create(i64 %%%d)", reg, arg_reg);
                    }
                    return reg;
                }

                /* free_buf: 1 int arg, returns void */
                if (strcmp(buf_func, "free_buf") == 0) {
                    int arg_reg = llvm_emit_expr(e, node->data.call.args[0]);
                    if (arg_reg < 0) {
                        LLVM_EMITLN(e, "  call void @forge_buf_free_buf(i64 %lld)",
                                   (long long)node->data.call.args[0]->data.int_val);
                    } else {
                        LLVM_EMITLN(e, "  call void @forge_buf_free_buf(i64 %%%d)", arg_reg);
                    }
                    return -1;
                }

                /* write_byte: 2 int args, returns void */
                if (strcmp(buf_func, "write_byte") == 0) {
                    int h_reg = llvm_emit_expr(e, node->data.call.args[0]);
                    int v_reg = llvm_emit_expr(e, node->data.call.args[1]);
                    LLVM_EMIT(e, "  call void @forge_buf_write_byte(");
                    if (h_reg < 0) {
                        LLVM_EMIT(e, "i64 %lld", (long long)node->data.call.args[0]->data.int_val);
                    } else {
                        LLVM_EMIT(e, "i64 %%%d", h_reg);
                    }
                    LLVM_EMIT(e, ", ");
                    if (v_reg < 0) {
                        LLVM_EMIT(e, "i64 %lld", (long long)node->data.call.args[1]->data.int_val);
                    } else {
                        LLVM_EMIT(e, "i64 %%%d", v_reg);
                    }
                    LLVM_EMITLN(e, ")");
                    return -1;
                }

                /* write_str: handle + string, use ptr wrapper */
                if (strcmp(buf_func, "write_str") == 0) {
                    int h_reg = llvm_emit_expr(e, node->data.call.args[0]);
                    int s_reg = llvm_emit_expr(e, node->data.call.args[1]);
                    int tmp = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = alloca %%forge_str_t", tmp);
                    int tmp2 = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = getelementptr inbounds %%forge_str_t, %%forge_str_t* %%%d, i32 0, i32 0", tmp2, tmp);
                    int tmp3 = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = extractvalue %%forge_str_t %%%d, 0", tmp3, s_reg);
                    LLVM_EMITLN(e, "  store i8* %%%d, i8** %%%d", tmp3, tmp2);
                    int tmp4 = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = getelementptr inbounds %%forge_str_t, %%forge_str_t* %%%d, i32 0, i32 1", tmp4, tmp);
                    int tmp5 = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = extractvalue %%forge_str_t %%%d, 1", tmp5, s_reg);
                    LLVM_EMITLN(e, "  store i32 %%%d, i32* %%%d", tmp5, tmp4);
                    int tmp6 = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = getelementptr inbounds %%forge_str_t, %%forge_str_t* %%%d, i32 0, i32 2", tmp6, tmp);
                    int tmp7 = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = extractvalue %%forge_str_t %%%d, 2", tmp7, s_reg);
                    LLVM_EMITLN(e, "  store i32 %%%d, i32* %%%d", tmp7, tmp6);
                    LLVM_EMIT(e, "  call void @forge_buf_write_str_ptr(");
                    if (h_reg < 0) {
                        LLVM_EMIT(e, "i64 %lld", (long long)node->data.call.args[0]->data.int_val);
                    } else {
                        LLVM_EMIT(e, "i64 %%%d", h_reg);
                    }
                    LLVM_EMITLN(e, ", %%forge_str_t* %%%d)", tmp);
                    return -1;
                }

                /* write_int16_le, write_int32_le: 2 int args */
                if (strcmp(buf_func, "write_int16_le") == 0 || strcmp(buf_func, "write_int32_le") == 0) {
                    int h_reg = llvm_emit_expr(e, node->data.call.args[0]);
                    int v_reg = llvm_emit_expr(e, node->data.call.args[1]);
                    LLVM_EMIT(e, "  call void @forge_buf_%s(", buf_func);
                    if (h_reg < 0) {
                        LLVM_EMIT(e, "i64 %lld", (long long)node->data.call.args[0]->data.int_val);
                    } else {
                        LLVM_EMIT(e, "i64 %%%d", h_reg);
                    }
                    LLVM_EMIT(e, ", ");
                    if (v_reg < 0) {
                        LLVM_EMIT(e, "i64 %lld", (long long)node->data.call.args[1]->data.int_val);
                    } else {
                        LLVM_EMIT(e, "i64 %%%d", v_reg);
                    }
                    LLVM_EMITLN(e, ")");
                    return -1;
                }

                /* write_float32_le: handle + double */
                if (strcmp(buf_func, "write_float32_le") == 0) {
                    int h_reg = llvm_emit_expr(e, node->data.call.args[0]);
                    int v_reg = llvm_emit_expr(e, node->data.call.args[1]);
                    LLVM_EMIT(e, "  call void @forge_buf_write_float32_le(");
                    if (h_reg < 0) {
                        LLVM_EMIT(e, "i64 %lld", (long long)node->data.call.args[0]->data.int_val);
                    } else {
                        LLVM_EMIT(e, "i64 %%%d", h_reg);
                    }
                    LLVM_EMIT(e, ", ");
                    if (v_reg < 0) {
                        /* Float literal */
                        LLVM_EMIT(e, "double %e", node->data.call.args[1]->data.float_val);
                    } else {
                        LLVM_EMIT(e, "double %%%d", v_reg);
                    }
                    LLVM_EMITLN(e, ")");
                    return -1;
                }

                /* read_byte, read_int16_le, read_int32_le: 1 int arg, returns int */
                if (strcmp(buf_func, "read_byte") == 0 ||
                    strcmp(buf_func, "read_int16_le") == 0 ||
                    strcmp(buf_func, "read_int32_le") == 0) {
                    int h_reg = llvm_emit_expr(e, node->data.call.args[0]);
                    reg = llvm_next_reg(e);
                    LLVM_EMIT(e, "  %%%d = call i64 @forge_buf_%s(", reg, buf_func);
                    if (h_reg < 0) {
                        LLVM_EMIT(e, "i64 %lld", (long long)node->data.call.args[0]->data.int_val);
                    } else {
                        LLVM_EMIT(e, "i64 %%%d", h_reg);
                    }
                    LLVM_EMITLN(e, ")");
                    return reg;
                }

                /* seek: 2 int args, void */
                if (strcmp(buf_func, "seek") == 0) {
                    int h_reg = llvm_emit_expr(e, node->data.call.args[0]);
                    int p_reg = llvm_emit_expr(e, node->data.call.args[1]);
                    LLVM_EMIT(e, "  call void @forge_buf_seek(");
                    if (h_reg < 0) {
                        LLVM_EMIT(e, "i64 %lld", (long long)node->data.call.args[0]->data.int_val);
                    } else {
                        LLVM_EMIT(e, "i64 %%%d", h_reg);
                    }
                    LLVM_EMIT(e, ", ");
                    if (p_reg < 0) {
                        LLVM_EMIT(e, "i64 %lld", (long long)node->data.call.args[1]->data.int_val);
                    } else {
                        LLVM_EMIT(e, "i64 %%%d", p_reg);
                    }
                    LLVM_EMITLN(e, ")");
                    return -1;
                }

                /* rewind: 1 int arg, void */
                if (strcmp(buf_func, "rewind") == 0) {
                    int h_reg = llvm_emit_expr(e, node->data.call.args[0]);
                    LLVM_EMIT(e, "  call void @forge_buf_rewind(");
                    if (h_reg < 0) {
                        LLVM_EMIT(e, "i64 %lld", (long long)node->data.call.args[0]->data.int_val);
                    } else {
                        LLVM_EMIT(e, "i64 %%%d", h_reg);
                    }
                    LLVM_EMITLN(e, ")");
                    return -1;
                }

                /* remaining, length, capacity, position: 1 int arg, returns int */
                if (strcmp(buf_func, "remaining") == 0 ||
                    strcmp(buf_func, "length") == 0 ||
                    strcmp(buf_func, "capacity") == 0 ||
                    strcmp(buf_func, "position") == 0) {
                    int h_reg = llvm_emit_expr(e, node->data.call.args[0]);
                    reg = llvm_next_reg(e);
                    LLVM_EMIT(e, "  %%%d = call i64 @forge_buf_%s(", reg, buf_func);
                    if (h_reg < 0) {
                        LLVM_EMIT(e, "i64 %lld", (long long)node->data.call.args[0]->data.int_val);
                    } else {
                        LLVM_EMIT(e, "i64 %%%d", h_reg);
                    }
                    LLVM_EMITLN(e, ")");
                    return reg;
                }

                /* to_str, to_hex: 1 int arg, returns string via ptr wrapper */
                if (strcmp(buf_func, "to_str") == 0 || strcmp(buf_func, "to_hex") == 0) {
                    int h_reg = llvm_emit_expr(e, node->data.call.args[0]);
                    int tmp = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = alloca %%forge_str_t", tmp);
                    LLVM_EMIT(e, "  call void @forge_buf_%s_ptr(%%forge_str_t* %%%d, ", buf_func, tmp);
                    if (h_reg < 0) {
                        LLVM_EMIT(e, "i64 %lld", (long long)node->data.call.args[0]->data.int_val);
                    } else {
                        LLVM_EMIT(e, "i64 %%%d", h_reg);
                    }
                    LLVM_EMITLN(e, ")");
                    reg = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = load %%forge_str_t, %%forge_str_t* %%%d", reg, tmp);
                    return reg;
                }
            }

            /* Handle forge.serial.* functions */
            if (is_stdlib_call && strncmp(func_name, "forge_serial_", 13) == 0) {
                const char* serial_func = func_name + 13; /* Skip "forge_serial_" prefix */

                /* open: path str + baud int, returns int handle */
                if (strcmp(serial_func, "open") == 0) {
                    int path_reg = llvm_emit_expr(e, node->data.call.args[0]);
                    int baud_reg = llvm_emit_expr(e, node->data.call.args[1]);
                    /* Store string in alloca for ptr wrapper */
                    int tmp = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = alloca %%forge_str_t", tmp);
                    /* Store each field */
                    int tmp2 = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = getelementptr inbounds %%forge_str_t, %%forge_str_t* %%%d, i32 0, i32 0", tmp2, tmp);
                    int tmp3 = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = extractvalue %%forge_str_t %%%d, 0", tmp3, path_reg);
                    LLVM_EMITLN(e, "  store i8* %%%d, i8** %%%d", tmp3, tmp2);
                    int tmp4 = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = getelementptr inbounds %%forge_str_t, %%forge_str_t* %%%d, i32 0, i32 1", tmp4, tmp);
                    int tmp5 = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = extractvalue %%forge_str_t %%%d, 1", tmp5, path_reg);
                    LLVM_EMITLN(e, "  store i32 %%%d, i32* %%%d", tmp5, tmp4);
                    int tmp6 = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = getelementptr inbounds %%forge_str_t, %%forge_str_t* %%%d, i32 0, i32 2", tmp6, tmp);
                    int tmp7 = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = extractvalue %%forge_str_t %%%d, 2", tmp7, path_reg);
                    LLVM_EMITLN(e, "  store i32 %%%d, i32* %%%d", tmp7, tmp6);
                    reg = llvm_next_reg(e);
                    LLVM_EMIT(e, "  %%%d = call i64 @forge_serial_open_ptr(%%forge_str_t* %%%d, ", reg, tmp);
                    if (baud_reg < 0) {
                        LLVM_EMIT(e, "i64 %lld", (long long)node->data.call.args[1]->data.int_val);
                    } else {
                        LLVM_EMIT(e, "i64 %%%d", baud_reg);
                    }
                    LLVM_EMITLN(e, ")");
                    return reg;
                }

                /* close: 1 int arg, void */
                if (strcmp(serial_func, "close") == 0) {
                    int h_reg = llvm_emit_expr(e, node->data.call.args[0]);
                    LLVM_EMIT(e, "  call void @forge_serial_close(");
                    if (h_reg < 0) {
                        LLVM_EMIT(e, "i64 %lld", (long long)node->data.call.args[0]->data.int_val);
                    } else {
                        LLVM_EMIT(e, "i64 %%%d", h_reg);
                    }
                    LLVM_EMITLN(e, ")");
                    return -1;
                }

                /* read_byte, bytes_available, is_open, get_baud: 1 int arg, returns int */
                if (strcmp(serial_func, "read_byte") == 0 ||
                    strcmp(serial_func, "bytes_available") == 0 ||
                    strcmp(serial_func, "is_open") == 0 ||
                    strcmp(serial_func, "get_baud") == 0) {
                    int h_reg = llvm_emit_expr(e, node->data.call.args[0]);
                    reg = llvm_next_reg(e);
                    LLVM_EMIT(e, "  %%%d = call i64 @forge_serial_%s(", reg, serial_func);
                    if (h_reg < 0) {
                        LLVM_EMIT(e, "i64 %lld", (long long)node->data.call.args[0]->data.int_val);
                    } else {
                        LLVM_EMIT(e, "i64 %%%d", h_reg);
                    }
                    LLVM_EMITLN(e, ")");
                    return reg;
                }

                /* read_line: 1 int arg, returns string via ptr wrapper */
                if (strcmp(serial_func, "read_line") == 0) {
                    int h_reg = llvm_emit_expr(e, node->data.call.args[0]);
                    int tmp = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = alloca %%forge_str_t", tmp);
                    LLVM_EMIT(e, "  call void @forge_serial_read_line_ptr(%%forge_str_t* %%%d, ", tmp);
                    if (h_reg < 0) {
                        LLVM_EMIT(e, "i64 %lld", (long long)node->data.call.args[0]->data.int_val);
                    } else {
                        LLVM_EMIT(e, "i64 %%%d", h_reg);
                    }
                    LLVM_EMITLN(e, ")");
                    reg = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = load %%forge_str_t, %%forge_str_t* %%%d", reg, tmp);
                    return reg;
                }

                /* write_byte: 2 int args, void */
                if (strcmp(serial_func, "write_byte") == 0) {
                    int h_reg = llvm_emit_expr(e, node->data.call.args[0]);
                    int v_reg = llvm_emit_expr(e, node->data.call.args[1]);
                    LLVM_EMIT(e, "  call void @forge_serial_write_byte(");
                    if (h_reg < 0) {
                        LLVM_EMIT(e, "i64 %lld", (long long)node->data.call.args[0]->data.int_val);
                    } else {
                        LLVM_EMIT(e, "i64 %%%d", h_reg);
                    }
                    LLVM_EMIT(e, ", ");
                    if (v_reg < 0) {
                        LLVM_EMIT(e, "i64 %lld", (long long)node->data.call.args[1]->data.int_val);
                    } else {
                        LLVM_EMIT(e, "i64 %%%d", v_reg);
                    }
                    LLVM_EMITLN(e, ")");
                    return -1;
                }

                /* write_str: handle + string, void */
                if (strcmp(serial_func, "write_str") == 0) {
                    int h_reg = llvm_emit_expr(e, node->data.call.args[0]);
                    int s_reg = llvm_emit_expr(e, node->data.call.args[1]);
                    /* Store string in alloca for ptr wrapper */
                    int tmp = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = alloca %%forge_str_t", tmp);
                    int tmp2 = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = getelementptr inbounds %%forge_str_t, %%forge_str_t* %%%d, i32 0, i32 0", tmp2, tmp);
                    int tmp3 = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = extractvalue %%forge_str_t %%%d, 0", tmp3, s_reg);
                    LLVM_EMITLN(e, "  store i8* %%%d, i8** %%%d", tmp3, tmp2);
                    int tmp4 = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = getelementptr inbounds %%forge_str_t, %%forge_str_t* %%%d, i32 0, i32 1", tmp4, tmp);
                    int tmp5 = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = extractvalue %%forge_str_t %%%d, 1", tmp5, s_reg);
                    LLVM_EMITLN(e, "  store i32 %%%d, i32* %%%d", tmp5, tmp4);
                    int tmp6 = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = getelementptr inbounds %%forge_str_t, %%forge_str_t* %%%d, i32 0, i32 2", tmp6, tmp);
                    int tmp7 = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = extractvalue %%forge_str_t %%%d, 2", tmp7, s_reg);
                    LLVM_EMITLN(e, "  store i32 %%%d, i32* %%%d", tmp7, tmp6);
                    LLVM_EMIT(e, "  call void @forge_serial_write_str_ptr(");
                    if (h_reg < 0) {
                        LLVM_EMIT(e, "i64 %lld", (long long)node->data.call.args[0]->data.int_val);
                    } else {
                        LLVM_EMIT(e, "i64 %%%d", h_reg);
                    }
                    LLVM_EMITLN(e, ", %%forge_str_t* %%%d)", tmp);
                    return -1;
                }

                /* set_timeout: 2 int args, void */
                if (strcmp(serial_func, "set_timeout") == 0) {
                    int h_reg = llvm_emit_expr(e, node->data.call.args[0]);
                    int ms_reg = llvm_emit_expr(e, node->data.call.args[1]);
                    LLVM_EMIT(e, "  call void @forge_serial_set_timeout(");
                    if (h_reg < 0) {
                        LLVM_EMIT(e, "i64 %lld", (long long)node->data.call.args[0]->data.int_val);
                    } else {
                        LLVM_EMIT(e, "i64 %%%d", h_reg);
                    }
                    LLVM_EMIT(e, ", ");
                    if (ms_reg < 0) {
                        LLVM_EMIT(e, "i64 %lld", (long long)node->data.call.args[1]->data.int_val);
                    } else {
                        LLVM_EMIT(e, "i64 %%%d", ms_reg);
                    }
                    LLVM_EMITLN(e, ")");
                    return -1;
                }

                /* flush: 1 int arg, void */
                if (strcmp(serial_func, "flush") == 0) {
                    int h_reg = llvm_emit_expr(e, node->data.call.args[0]);
                    LLVM_EMIT(e, "  call void @forge_serial_flush(");
                    if (h_reg < 0) {
                        LLVM_EMIT(e, "i64 %lld", (long long)node->data.call.args[0]->data.int_val);
                    } else {
                        LLVM_EMIT(e, "i64 %%%d", h_reg);
                    }
                    LLVM_EMITLN(e, ")");
                    return -1;
                }
            }

            /* Handle forge.nmea.* functions */
            if (is_stdlib_call && strncmp(func_name, "forge_nmea_", 11) == 0) {
                const char* nmea_func = func_name + 11; /* Skip "forge_nmea_" prefix */

                /* Helper to copy string to alloca for ptr passing */
                #define NMEA_ALLOCA_STR(str_reg, tmp_var) do { \
                    int tmp_var = llvm_next_reg(e); \
                    LLVM_EMITLN(e, "  %%%d = alloca %%forge_str_t", tmp_var); \
                    int tmp2 = llvm_next_reg(e); \
                    LLVM_EMITLN(e, "  %%%d = getelementptr inbounds %%forge_str_t, %%forge_str_t* %%%d, i32 0, i32 0", tmp2, tmp_var); \
                    int tmp3 = llvm_next_reg(e); \
                    LLVM_EMITLN(e, "  %%%d = extractvalue %%forge_str_t %%%d, 0", tmp3, str_reg); \
                    LLVM_EMITLN(e, "  store i8* %%%d, i8** %%%d", tmp3, tmp2); \
                    int tmp4 = llvm_next_reg(e); \
                    LLVM_EMITLN(e, "  %%%d = getelementptr inbounds %%forge_str_t, %%forge_str_t* %%%d, i32 0, i32 1", tmp4, tmp_var); \
                    int tmp5 = llvm_next_reg(e); \
                    LLVM_EMITLN(e, "  %%%d = extractvalue %%forge_str_t %%%d, 1", tmp5, str_reg); \
                    LLVM_EMITLN(e, "  store i32 %%%d, i32* %%%d", tmp5, tmp4); \
                    int tmp6 = llvm_next_reg(e); \
                    LLVM_EMITLN(e, "  %%%d = getelementptr inbounds %%forge_str_t, %%forge_str_t* %%%d, i32 0, i32 2", tmp6, tmp_var); \
                    int tmp7 = llvm_next_reg(e); \
                    LLVM_EMITLN(e, "  %%%d = extractvalue %%forge_str_t %%%d, 2", tmp7, str_reg); \
                    LLVM_EMITLN(e, "  store i32 %%%d, i32* %%%d", tmp7, tmp6); \
                } while(0)

                /* validate: 1 str arg, returns int */
                if (strcmp(nmea_func, "validate") == 0) {
                    int s_reg = llvm_emit_expr(e, node->data.call.args[0]);
                    int tmp = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = alloca %%forge_str_t", tmp);
                    int tmp2 = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = getelementptr inbounds %%forge_str_t, %%forge_str_t* %%%d, i32 0, i32 0", tmp2, tmp);
                    int tmp3 = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = extractvalue %%forge_str_t %%%d, 0", tmp3, s_reg);
                    LLVM_EMITLN(e, "  store i8* %%%d, i8** %%%d", tmp3, tmp2);
                    int tmp4 = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = getelementptr inbounds %%forge_str_t, %%forge_str_t* %%%d, i32 0, i32 1", tmp4, tmp);
                    int tmp5 = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = extractvalue %%forge_str_t %%%d, 1", tmp5, s_reg);
                    LLVM_EMITLN(e, "  store i32 %%%d, i32* %%%d", tmp5, tmp4);
                    int tmp6 = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = getelementptr inbounds %%forge_str_t, %%forge_str_t* %%%d, i32 0, i32 2", tmp6, tmp);
                    int tmp7 = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = extractvalue %%forge_str_t %%%d, 2", tmp7, s_reg);
                    LLVM_EMITLN(e, "  store i32 %%%d, i32* %%%d", tmp7, tmp6);
                    reg = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = call i64 @forge_nmea_validate_ptr(%%forge_str_t* %%%d)", reg, tmp);
                    return reg;
                }

                /* field_count: 1 str arg, returns int */
                if (strcmp(nmea_func, "field_count") == 0) {
                    int s_reg = llvm_emit_expr(e, node->data.call.args[0]);
                    int tmp = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = alloca %%forge_str_t", tmp);
                    int tmp2 = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = getelementptr inbounds %%forge_str_t, %%forge_str_t* %%%d, i32 0, i32 0", tmp2, tmp);
                    int tmp3 = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = extractvalue %%forge_str_t %%%d, 0", tmp3, s_reg);
                    LLVM_EMITLN(e, "  store i8* %%%d, i8** %%%d", tmp3, tmp2);
                    int tmp4 = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = getelementptr inbounds %%forge_str_t, %%forge_str_t* %%%d, i32 0, i32 1", tmp4, tmp);
                    int tmp5 = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = extractvalue %%forge_str_t %%%d, 1", tmp5, s_reg);
                    LLVM_EMITLN(e, "  store i32 %%%d, i32* %%%d", tmp5, tmp4);
                    int tmp6 = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = getelementptr inbounds %%forge_str_t, %%forge_str_t* %%%d, i32 0, i32 2", tmp6, tmp);
                    int tmp7 = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = extractvalue %%forge_str_t %%%d, 2", tmp7, s_reg);
                    LLVM_EMITLN(e, "  store i32 %%%d, i32* %%%d", tmp7, tmp6);
                    reg = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = call i64 @forge_nmea_field_count_ptr(%%forge_str_t* %%%d)", reg, tmp);
                    return reg;
                }

                /* checksum, sentence_type, get_talker: 1 str arg, returns str */
                if (strcmp(nmea_func, "checksum") == 0 ||
                    strcmp(nmea_func, "sentence_type") == 0 ||
                    strcmp(nmea_func, "get_talker") == 0) {
                    int s_reg = llvm_emit_expr(e, node->data.call.args[0]);
                    /* Alloca for output */
                    int out_tmp = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = alloca %%forge_str_t", out_tmp);
                    /* Alloca for input */
                    int in_tmp = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = alloca %%forge_str_t", in_tmp);
                    int tmp2 = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = getelementptr inbounds %%forge_str_t, %%forge_str_t* %%%d, i32 0, i32 0", tmp2, in_tmp);
                    int tmp3 = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = extractvalue %%forge_str_t %%%d, 0", tmp3, s_reg);
                    LLVM_EMITLN(e, "  store i8* %%%d, i8** %%%d", tmp3, tmp2);
                    int tmp4 = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = getelementptr inbounds %%forge_str_t, %%forge_str_t* %%%d, i32 0, i32 1", tmp4, in_tmp);
                    int tmp5 = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = extractvalue %%forge_str_t %%%d, 1", tmp5, s_reg);
                    LLVM_EMITLN(e, "  store i32 %%%d, i32* %%%d", tmp5, tmp4);
                    int tmp6 = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = getelementptr inbounds %%forge_str_t, %%forge_str_t* %%%d, i32 0, i32 2", tmp6, in_tmp);
                    int tmp7 = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = extractvalue %%forge_str_t %%%d, 2", tmp7, s_reg);
                    LLVM_EMITLN(e, "  store i32 %%%d, i32* %%%d", tmp7, tmp6);
                    /* Call */
                    LLVM_EMITLN(e, "  call void @forge_nmea_%s_ptr(%%forge_str_t* %%%d, %%forge_str_t* %%%d)", nmea_func, out_tmp, in_tmp);
                    reg = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = load %%forge_str_t, %%forge_str_t* %%%d", reg, out_tmp);
                    return reg;
                }

                /* get_field: 1 str + 1 int arg, returns str */
                if (strcmp(nmea_func, "get_field") == 0) {
                    int s_reg = llvm_emit_expr(e, node->data.call.args[0]);
                    int idx_reg = llvm_emit_expr(e, node->data.call.args[1]);
                    /* Alloca for output */
                    int out_tmp = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = alloca %%forge_str_t", out_tmp);
                    /* Alloca for input string */
                    int in_tmp = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = alloca %%forge_str_t", in_tmp);
                    int tmp2 = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = getelementptr inbounds %%forge_str_t, %%forge_str_t* %%%d, i32 0, i32 0", tmp2, in_tmp);
                    int tmp3 = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = extractvalue %%forge_str_t %%%d, 0", tmp3, s_reg);
                    LLVM_EMITLN(e, "  store i8* %%%d, i8** %%%d", tmp3, tmp2);
                    int tmp4 = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = getelementptr inbounds %%forge_str_t, %%forge_str_t* %%%d, i32 0, i32 1", tmp4, in_tmp);
                    int tmp5 = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = extractvalue %%forge_str_t %%%d, 1", tmp5, s_reg);
                    LLVM_EMITLN(e, "  store i32 %%%d, i32* %%%d", tmp5, tmp4);
                    int tmp6 = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = getelementptr inbounds %%forge_str_t, %%forge_str_t* %%%d, i32 0, i32 2", tmp6, in_tmp);
                    int tmp7 = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = extractvalue %%forge_str_t %%%d, 2", tmp7, s_reg);
                    LLVM_EMITLN(e, "  store i32 %%%d, i32* %%%d", tmp7, tmp6);
                    /* Call */
                    LLVM_EMIT(e, "  call void @forge_nmea_get_field_ptr(%%forge_str_t* %%%d, %%forge_str_t* %%%d, ", out_tmp, in_tmp);
                    if (idx_reg < 0) {
                        LLVM_EMIT(e, "i64 %lld", (long long)node->data.call.args[1]->data.int_val);
                    } else {
                        LLVM_EMIT(e, "i64 %%%d", idx_reg);
                    }
                    LLVM_EMITLN(e, ")");
                    reg = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = load %%forge_str_t, %%forge_str_t* %%%d", reg, out_tmp);
                    return reg;
                }

                /* latitude, longitude: 1 str arg, returns double */
                if (strcmp(nmea_func, "latitude") == 0 || strcmp(nmea_func, "longitude") == 0) {
                    int s_reg = llvm_emit_expr(e, node->data.call.args[0]);
                    /* Alloca for input */
                    int in_tmp = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = alloca %%forge_str_t", in_tmp);
                    int tmp2 = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = getelementptr inbounds %%forge_str_t, %%forge_str_t* %%%d, i32 0, i32 0", tmp2, in_tmp);
                    int tmp3 = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = extractvalue %%forge_str_t %%%d, 0", tmp3, s_reg);
                    LLVM_EMITLN(e, "  store i8* %%%d, i8** %%%d", tmp3, tmp2);
                    int tmp4 = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = getelementptr inbounds %%forge_str_t, %%forge_str_t* %%%d, i32 0, i32 1", tmp4, in_tmp);
                    int tmp5 = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = extractvalue %%forge_str_t %%%d, 1", tmp5, s_reg);
                    LLVM_EMITLN(e, "  store i32 %%%d, i32* %%%d", tmp5, tmp4);
                    int tmp6 = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = getelementptr inbounds %%forge_str_t, %%forge_str_t* %%%d, i32 0, i32 2", tmp6, in_tmp);
                    int tmp7 = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = extractvalue %%forge_str_t %%%d, 2", tmp7, s_reg);
                    LLVM_EMITLN(e, "  store i32 %%%d, i32* %%%d", tmp7, tmp6);
                    reg = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = call double @forge_nmea_%s_ptr(%%forge_str_t* %%%d)", reg, nmea_func, in_tmp);
                    return reg;
                }

                #undef NMEA_ALLOCA_STR
            }

            /* Handle append() builtin */
            if (func_name && strcmp(func_name, "append") == 0 && arg_count == 2) {
                /* append(arr, val) -> push val onto arr, return arr */
                int arr_reg = llvm_emit_expr(e, node->data.call.args[0]);
                int val_reg = llvm_emit_expr(e, node->data.call.args[1]);

                /* Determine element type */
                forge_type_t* elem_type = NULL;
                forge_type_t* arr_type = node->data.call.args[0]->resolved_type;
                if (arr_type) {
                    if (arr_type->kind == TY_DYN_ARRAY)
                        elem_type = arr_type->as.dyn_array.elem_type;
                    else if (arr_type->kind == TY_FIXED_ARRAY)
                        elem_type = arr_type->as.fixed_array.elem_type;
                }
                const char* elem_type_str = llvm_type_str(e, elem_type);

                /* Store array to alloca so we can pass pointer */
                int arr_ptr = llvm_next_reg(e);
                LLVM_EMITLN(e, "  %%%d = alloca %%forge_array_t", arr_ptr);
                LLVM_EMITLN(e, "  store %%forge_array_t %%%d, %%forge_array_t* %%%d", arr_reg, arr_ptr);

                /* Store value to alloca so we can pass pointer */
                int val_ptr = llvm_next_reg(e);
                LLVM_EMITLN(e, "  %%%d = alloca %s", val_ptr, elem_type_str);
                if (val_reg < 0 && node->data.call.args[1]->kind == NODE_INT_LIT) {
                    LLVM_EMITLN(e, "  store i64 %lld, i64* %%%d",
                               (long long)node->data.call.args[1]->data.int_val, val_ptr);
                } else if (val_reg < 0 && node->data.call.args[1]->kind == NODE_FLOAT_LIT) {
                    LLVM_EMITLN(e, "  store double %e, double* %%%d",
                               node->data.call.args[1]->data.float_val, val_ptr);
                } else if (val_reg < 0 && node->data.call.args[1]->kind == NODE_BOOL_LIT) {
                    LLVM_EMITLN(e, "  store i1 %d, i1* %%%d",
                               node->data.call.args[1]->data.bool_val ? 1 : 0, val_ptr);
                } else {
                    LLVM_EMITLN(e, "  store %s %%%d, %s* %%%d", elem_type_str, val_reg, elem_type_str, val_ptr);
                }

                /* Call forge_array_push */
                LLVM_EMITLN(e, "  call void @forge_array_push(%%forge_array_t* %%%d, ptr %%%d)", arr_ptr, val_ptr);

                /* Load updated array back */
                reg = llvm_next_reg(e);
                LLVM_EMITLN(e, "  %%%d = load %%forge_array_t, %%forge_array_t* %%%d", reg, arr_ptr);
                return reg;
            }

            /* Generic function call */
            int* arg_regs = ARENA_ALLOC_ARRAY(e->arena, int, arg_count);
            for (int i = 0; i < arg_count; i++) {
                arg_regs[i] = llvm_emit_expr(e, node->data.call.args[i]);
            }

            if (node->resolved_type && node->resolved_type->kind != TY_VOID) {
                reg = llvm_next_reg(e);
                LLVM_EMIT(e, "  %%%d = call %s @%s(",
                         reg, llvm_type_str(e, node->resolved_type), func_name);
            } else {
                LLVM_EMIT(e, "  call void @%s(", func_name);
                reg = -1;
            }

            for (int i = 0; i < arg_count; i++) {
                if (i > 0) LLVM_EMIT(e, ", ");
                forge_type_t* arg_type = node->data.call.args[i]->resolved_type;
                if (arg_regs[i] < 0) {
                    /* Inline literal */
                    if (node->data.call.args[i]->kind == NODE_INT_LIT) {
                        LLVM_EMIT(e, "i64 %lld", (long long)node->data.call.args[i]->data.int_val);
                    }
                } else {
                    LLVM_EMIT(e, "%s %%%d", llvm_type_str(e, arg_type), arg_regs[i]);
                }
            }
            LLVM_EMITLN(e, ")");
            return reg;
        }

        case NODE_ARRAY_LITERAL: {
            /* Array literal - allocate and fill */
            int count = node->data.array_lit.count;
            forge_type_t* elem_type = NULL;
            if (node->resolved_type && (node->resolved_type->kind == TY_FIXED_ARRAY ||
                                        node->resolved_type->kind == TY_DYN_ARRAY)) {
                elem_type = node->resolved_type->kind == TY_FIXED_ARRAY ?
                            node->resolved_type->as.fixed_array.elem_type :
                            node->resolved_type->as.dyn_array.elem_type;
            }
            const char* elem_type_str = llvm_type_str(e, elem_type);

            /* Compute elem_size for forge_array_create */
            int elem_size = 8; /* default for pointers/i64 */
            if (elem_type) {
                switch (elem_type->kind) {
                    case TY_INT: elem_size = 8; break;
                    case TY_FLOAT: elem_size = 8; break;
                    case TY_BOOL: elem_size = 1; break;
                    case TY_STR: elem_size = 16; break; /* forge_str_t: { i8*, i32, i32 } = 16 bytes */
                    default: elem_size = 8; break;
                }
            }

            if (count == 0) {
                /* Empty array - use heap allocation via forge_array_create
                 * so that subsequent forge_array_push can realloc safely */
                int arr_ptr = llvm_next_reg(e);
                LLVM_EMITLN(e, "  %%%d = alloca %%forge_array_t", arr_ptr);
                int created = llvm_next_reg(e);
                LLVM_EMITLN(e, "  %%%d = call %%forge_array_t @forge_array_create(i64 %d, i32 0)",
                           created, elem_size);
                LLVM_EMITLN(e, "  store %%forge_array_t %%%d, %%forge_array_t* %%%d", created, arr_ptr);
                return arr_ptr;
            }

            /* Allocate array struct */
            int arr_ptr = llvm_next_reg(e);
            LLVM_EMITLN(e, "  %%%d = alloca %%forge_array_t", arr_ptr);

            /* Allocate data buffer (stack for simplicity) */
            int data_ptr = llvm_next_reg(e);
            LLVM_EMITLN(e, "  %%%d = alloca %s, i64 %d", data_ptr, elem_type_str, count);

            /* Store each element */
            for (int i = 0; i < count; i++) {
                forge_node_t* elem = node->data.array_lit.elements[i];
                int elem_reg = llvm_emit_expr(e, elem);
                int elem_ptr = llvm_next_reg(e);
                LLVM_EMITLN(e, "  %%%d = getelementptr %s, %s* %%%d, i64 %d",
                           elem_ptr, elem_type_str, elem_type_str, data_ptr, i);
                /* Handle literal elements that return -1 */
                if (elem_reg < 0) {
                    if (elem->kind == NODE_INT_LIT) {
                        LLVM_EMITLN(e, "  store i64 %lld, i64* %%%d",
                                   (long long)elem->data.int_val, elem_ptr);
                    } else if (elem->kind == NODE_FLOAT_LIT) {
                        LLVM_EMITLN(e, "  store double %e, double* %%%d",
                                   elem->data.float_val, elem_ptr);
                    } else if (elem->kind == NODE_BOOL_LIT) {
                        LLVM_EMITLN(e, "  store i1 %d, i1* %%%d",
                                   elem->data.bool_val ? 1 : 0, elem_ptr);
                    }
                } else {
                    LLVM_EMITLN(e, "  store %s %%%d, %s* %%%d",
                               elem_type_str, elem_reg, elem_type_str, elem_ptr);
                }
            }

            /* Set up forge_array_t struct fields */
            /* data pointer (field 0) */
            int data_field = llvm_next_reg(e);
            LLVM_EMITLN(e, "  %%%d = getelementptr %%forge_array_t, %%forge_array_t* %%%d, i32 0, i32 0",
                       data_field, arr_ptr);
            int data_cast = llvm_next_reg(e);
            LLVM_EMITLN(e, "  %%%d = bitcast %s* %%%d to i8*", data_cast, elem_type_str, data_ptr);
            LLVM_EMITLN(e, "  store i8* %%%d, i8** %%%d", data_cast, data_field);

            /* length (field 1) - i32 */
            int len_field = llvm_next_reg(e);
            LLVM_EMITLN(e, "  %%%d = getelementptr %%forge_array_t, %%forge_array_t* %%%d, i32 0, i32 1",
                       len_field, arr_ptr);
            LLVM_EMITLN(e, "  store i32 %d, i32* %%%d", count, len_field);

            /* capacity (field 2) - i32 */
            int cap_field = llvm_next_reg(e);
            LLVM_EMITLN(e, "  %%%d = getelementptr %%forge_array_t, %%forge_array_t* %%%d, i32 0, i32 2",
                       cap_field, arr_ptr);
            LLVM_EMITLN(e, "  store i32 %d, i32* %%%d", count, cap_field);

            /* elem_size (field 3) - i64 (size_t) */
            int elem_size_field = llvm_next_reg(e);
            LLVM_EMITLN(e, "  %%%d = getelementptr %%forge_array_t, %%forge_array_t* %%%d, i32 0, i32 3",
                       elem_size_field, arr_ptr);
            LLVM_EMITLN(e, "  store i64 %d, i64* %%%d", elem_size, elem_size_field);

            return arr_ptr;
        }

        case NODE_INDEX: {
            /* Array/string indexing: obj[idx] */
            int obj_reg = llvm_emit_expr(e, node->data.index.object);
            forge_node_t* idx_node = node->data.index.index;
            int idx_reg = llvm_emit_expr(e, idx_node);

            forge_type_t* obj_type = node->data.index.object->resolved_type;
            if (obj_type && (obj_type->kind == TY_FIXED_ARRAY || obj_type->kind == TY_DYN_ARRAY)) {
                forge_type_t* elem_type = obj_type->kind == TY_FIXED_ARRAY ?
                                          obj_type->as.fixed_array.elem_type :
                                          obj_type->as.dyn_array.elem_type;
                const char* elem_type_str = llvm_type_str(e, elem_type);

                /* obj_reg is a loaded value - need to store to temp to get pointer */
                int arr_ptr = llvm_next_reg(e);
                LLVM_EMITLN(e, "  %%%d = alloca %%forge_array_t", arr_ptr);
                LLVM_EMITLN(e, "  store %%forge_array_t %%%d, %%forge_array_t* %%%d", obj_reg, arr_ptr);

                /* Get data pointer from array struct */
                int data_field = llvm_next_reg(e);
                LLVM_EMITLN(e, "  %%%d = getelementptr %%forge_array_t, %%forge_array_t* %%%d, i32 0, i32 0",
                           data_field, arr_ptr);
                int data_ptr = llvm_next_reg(e);
                LLVM_EMITLN(e, "  %%%d = load i8*, i8** %%%d", data_ptr, data_field);

                /* Cast to proper element pointer and index */
                int typed_ptr = llvm_next_reg(e);
                LLVM_EMITLN(e, "  %%%d = bitcast i8* %%%d to %s*", typed_ptr, data_ptr, elem_type_str);
                int elem_ptr = llvm_next_reg(e);
                /* Handle literal index */
                if (idx_reg < 0 && idx_node->kind == NODE_INT_LIT) {
                    LLVM_EMITLN(e, "  %%%d = getelementptr %s, %s* %%%d, i64 %lld",
                               elem_ptr, elem_type_str, elem_type_str, typed_ptr,
                               (long long)idx_node->data.int_val);
                } else {
                    LLVM_EMITLN(e, "  %%%d = getelementptr %s, %s* %%%d, i64 %%%d",
                               elem_ptr, elem_type_str, elem_type_str, typed_ptr, idx_reg);
                }
                int result = llvm_next_reg(e);
                LLVM_EMITLN(e, "  %%%d = load %s, %s* %%%d", result, elem_type_str, elem_type_str, elem_ptr);
                return result;
            }
            return -1;
        }

        case NODE_FIELD_ACCESS: {
            /* Check for forge.math constants (PI, E, TAU) */
            forge_node_t* inner = node->data.field_access.object;
            const char* field_name = node->data.field_access.field_name;

            if (inner && inner->kind == NODE_FIELD_ACCESS) {
                forge_node_t* root = inner->data.field_access.object;
                const char* mod = inner->data.field_access.field_name;
                if (root && root->kind == NODE_IDENT &&
                    strcmp(root->data.name, "forge") == 0 &&
                    strcmp(mod, "math") == 0) {
                    /* Handle forge.math.PI, forge.math.E, forge.math.TAU */
                    int reg = llvm_next_reg(e);
                    if (strcmp(field_name, "PI") == 0) {
                        LLVM_EMITLN(e, "  %%%d = fadd double 0.0, 0x400921FB54442D18", reg); /* PI */
                        return reg;
                    } else if (strcmp(field_name, "E") == 0) {
                        LLVM_EMITLN(e, "  %%%d = fadd double 0.0, 0x4005BF0A8B145769", reg); /* E */
                        return reg;
                    } else if (strcmp(field_name, "TAU") == 0) {
                        LLVM_EMITLN(e, "  %%%d = fadd double 0.0, 0x401921FB54442D18", reg); /* TAU */
                        return reg;
                    }
                }
            }

            /* Record field access: obj.field */
            int obj_reg = llvm_emit_expr(e, inner);

            forge_type_t* obj_type = inner->resolved_type;
            if (obj_type && obj_type->kind == TY_RECORD) {
                /* Format type name with _t suffix */
                char rec_type_buf[128];
                snprintf(rec_type_buf, sizeof(rec_type_buf), "%s_t", obj_type->as.record.name);

                /* obj_reg is a loaded value, need to store to temp to get pointer */
                int obj_ptr = llvm_next_reg(e);
                LLVM_EMITLN(e, "  %%%d = alloca %%%s", obj_ptr, rec_type_buf);
                LLVM_EMITLN(e, "  store %%%s %%%d, %%%s* %%%d", rec_type_buf, obj_reg, rec_type_buf, obj_ptr);

                /* Find field index */
                int field_idx = -1;
                forge_type_t* field_type = NULL;
                for (int i = 0; i < obj_type->as.record.field_count; i++) {
                    if (strcmp(obj_type->as.record.field_names[i], field_name) == 0) {
                        field_idx = i;
                        field_type = obj_type->as.record.field_types[i];
                        break;
                    }
                }

                if (field_idx >= 0) {
                    const char* field_type_str = llvm_type_str(e, field_type);
                    int field_ptr = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = getelementptr %%%s, %%%s* %%%d, i32 0, i32 %d",
                               field_ptr, rec_type_buf, rec_type_buf, obj_ptr, field_idx);
                    int result = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = load %s, %s* %%%d", result, field_type_str, field_type_str, field_ptr);
                    return result;
                }
            }
            return -1;
        }

        case NODE_RECORD_LITERAL: {
            /* Record literal: TypeName { field1: val1, field2: val2 } */
            const char* type_name = node->data.record_lit.type_name;
            int field_count = node->data.record_lit.field_count;

            /* Format type name as %<name>_t */
            char rec_type_buf[128];
            snprintf(rec_type_buf, sizeof(rec_type_buf), "%s_t", type_name);

            /* Allocate record struct */
            int rec_ptr = llvm_next_reg(e);
            LLVM_EMITLN(e, "  %%%d = alloca %%%s", rec_ptr, rec_type_buf);

            /* Store each field value */
            for (int i = 0; i < field_count; i++) {
                const char* fname = node->data.record_lit.fields[i].name;
                forge_node_t* fval = node->data.record_lit.fields[i].value;
                int val_reg = llvm_emit_expr(e, fval);

                forge_type_t* ftype = fval->resolved_type;
                const char* ftype_str = llvm_type_str(e, ftype);

                /* Find field index in type */
                int field_idx = i;  /* Assume fields are in order for simplicity */

                int field_ptr = llvm_next_reg(e);
                LLVM_EMITLN(e, "  %%%d = getelementptr %%%s, %%%s* %%%d, i32 0, i32 %d",
                           field_ptr, rec_type_buf, rec_type_buf, rec_ptr, field_idx);
                /* Handle literal field values */
                if (val_reg < 0 && fval->kind == NODE_INT_LIT) {
                    LLVM_EMITLN(e, "  store i64 %lld, i64* %%%d",
                               (long long)fval->data.int_val, field_ptr);
                } else if (val_reg < 0 && fval->kind == NODE_FLOAT_LIT) {
                    LLVM_EMITLN(e, "  store double %e, double* %%%d",
                               fval->data.float_val, field_ptr);
                } else if (val_reg < 0 && fval->kind == NODE_BOOL_LIT) {
                    LLVM_EMITLN(e, "  store i1 %d, i1* %%%d",
                               fval->data.bool_val ? 1 : 0, field_ptr);
                } else {
                    LLVM_EMITLN(e, "  store %s %%%d, %s* %%%d", ftype_str, val_reg, ftype_str, field_ptr);
                }
                (void)fname;  /* Suppress unused warning */
            }

            return rec_ptr;
        }

        case NODE_SOME: {
            /* some(expr) - wrap value in optional */
            forge_node_t* val_node = node->data.some.expr;
            int val_reg = llvm_emit_expr(e, val_node);
            forge_type_t* val_type = val_node->resolved_type;
            const char* val_type_str = llvm_type_str(e, val_type);

            /* Optionals are represented as { i1 has_value, T value } */
            /* Allocate optional struct on stack */
            int opt_ptr = llvm_next_reg(e);
            LLVM_EMITLN(e, "  %%%d = alloca { i1, %s }", opt_ptr, val_type_str);

            /* Set has_value = true */
            int flag_ptr = llvm_next_reg(e);
            LLVM_EMITLN(e, "  %%%d = getelementptr { i1, %s }, { i1, %s }* %%%d, i32 0, i32 0",
                       flag_ptr, val_type_str, val_type_str, opt_ptr);
            LLVM_EMITLN(e, "  store i1 true, i1* %%%d", flag_ptr);

            /* Set value - handle literals */
            int val_ptr = llvm_next_reg(e);
            LLVM_EMITLN(e, "  %%%d = getelementptr { i1, %s }, { i1, %s }* %%%d, i32 0, i32 1",
                       val_ptr, val_type_str, val_type_str, opt_ptr);
            if (val_reg < 0 && val_node->kind == NODE_INT_LIT) {
                LLVM_EMITLN(e, "  store i64 %lld, i64* %%%d",
                           (long long)val_node->data.int_val, val_ptr);
            } else if (val_reg < 0 && val_node->kind == NODE_FLOAT_LIT) {
                LLVM_EMITLN(e, "  store double %e, double* %%%d",
                           val_node->data.float_val, val_ptr);
            } else if (val_reg < 0 && val_node->kind == NODE_BOOL_LIT) {
                LLVM_EMITLN(e, "  store i1 %d, i1* %%%d",
                           val_node->data.bool_val ? 1 : 0, val_ptr);
            } else {
                LLVM_EMITLN(e, "  store %s %%%d, %s* %%%d", val_type_str, val_reg, val_type_str, val_ptr);
            }

            return opt_ptr;
        }

        case NODE_NONE_LIT: {
            /* none - create optional with has_value = false */
            /* We need to get the inner type from the resolved_type or the proc return type */
            forge_type_t* inner_type = NULL;
            if (node->resolved_type && node->resolved_type->kind == TY_OPTIONAL) {
                inner_type = node->resolved_type->as.optional.inner;
            }
            /* Fallback: use current procedure's return type if it's optional */
            if ((!inner_type || inner_type->kind == TY_VOID) &&
                e->current_ret_type && e->current_ret_type->kind == TY_OPTIONAL) {
                inner_type = e->current_ret_type->as.optional.inner;
            }
            /* Last resort: default to i64 */
            if (!inner_type || inner_type->kind == TY_VOID) {
                /* Use i64 as a safe default for none literals */
            }
            const char* inner_str = (!inner_type || inner_type->kind == TY_VOID) ? "i64" : llvm_type_str(e, inner_type);

            /* Allocate optional struct */
            int opt_ptr = llvm_next_reg(e);
            LLVM_EMITLN(e, "  %%%d = alloca { i1, %s }", opt_ptr, inner_str);

            /* Set has_value = false */
            int flag_ptr = llvm_next_reg(e);
            LLVM_EMITLN(e, "  %%%d = getelementptr { i1, %s }, { i1, %s }* %%%d, i32 0, i32 0",
                       flag_ptr, inner_str, inner_str, opt_ptr);
            LLVM_EMITLN(e, "  store i1 false, i1* %%%d", flag_ptr);

            return opt_ptr;
        }

        case NODE_IS_SOME:
        case NODE_IS_NONE: {
            /* x is some / x is none - check if optional has value */
            int opt_val = llvm_emit_expr(e, node->data.is_check.expr);
            forge_type_t* inner_type = NULL;
            if (node->data.is_check.expr->resolved_type &&
                node->data.is_check.expr->resolved_type->kind == TY_OPTIONAL) {
                inner_type = node->data.is_check.expr->resolved_type->as.optional.inner;
            }
            const char* inner_str = llvm_type_str(e, inner_type);

            /* opt_val is a loaded value, store to temp to get pointer */
            int opt_ptr = llvm_next_reg(e);
            LLVM_EMITLN(e, "  %%%d = alloca { i1, %s }", opt_ptr, inner_str);
            LLVM_EMITLN(e, "  store { i1, %s } %%%d, { i1, %s }* %%%d",
                       inner_str, opt_val, inner_str, opt_ptr);

            /* Get has_value flag */
            int flag_ptr = llvm_next_reg(e);
            LLVM_EMITLN(e, "  %%%d = getelementptr { i1, %s }, { i1, %s }* %%%d, i32 0, i32 0",
                       flag_ptr, inner_str, inner_str, opt_ptr);
            int has_val = llvm_next_reg(e);
            LLVM_EMITLN(e, "  %%%d = load i1, i1* %%%d", has_val, flag_ptr);

            if (node->kind == NODE_IS_NONE) {
                /* Invert for is none */
                int result = llvm_next_reg(e);
                LLVM_EMITLN(e, "  %%%d = xor i1 %%%d, true", result, has_val);
                return result;
            }
            return has_val;
        }

        case NODE_OR_ELSE: {
            /* expr or_else fallback - return value if some, else fallback */
            int opt_val = llvm_emit_expr(e, node->data.or_else.optional_expr);
            forge_type_t* inner_type = NULL;
            if (node->data.or_else.optional_expr->resolved_type &&
                node->data.or_else.optional_expr->resolved_type->kind == TY_OPTIONAL) {
                inner_type = node->data.or_else.optional_expr->resolved_type->as.optional.inner;
            }
            const char* inner_str = llvm_type_str(e, inner_type);

            /* opt_val is a loaded value, store to temp to get pointer */
            int opt_ptr = llvm_next_reg(e);
            LLVM_EMITLN(e, "  %%%d = alloca { i1, %s }", opt_ptr, inner_str);
            LLVM_EMITLN(e, "  store { i1, %s } %%%d, { i1, %s }* %%%d",
                       inner_str, opt_val, inner_str, opt_ptr);

            /* Check has_value */
            int flag_ptr = llvm_next_reg(e);
            LLVM_EMITLN(e, "  %%%d = getelementptr { i1, %s }, { i1, %s }* %%%d, i32 0, i32 0",
                       flag_ptr, inner_str, inner_str, opt_ptr);
            int has_val = llvm_next_reg(e);
            LLVM_EMITLN(e, "  %%%d = load i1, i1* %%%d", has_val, flag_ptr);

            int some_label = llvm_next_label(e);
            int none_label = llvm_next_label(e);
            int end_label = llvm_next_label(e);

            LLVM_EMITLN(e, "  br i1 %%%d, label %%L%d, label %%L%d", has_val, some_label, none_label);

            /* Some case - get value */
            llvm_emit_label(e, some_label);
            int val_ptr = llvm_next_reg(e);
            LLVM_EMITLN(e, "  %%%d = getelementptr { i1, %s }, { i1, %s }* %%%d, i32 0, i32 1",
                       val_ptr, inner_str, inner_str, opt_ptr);
            int some_val = llvm_next_reg(e);
            LLVM_EMITLN(e, "  %%%d = load %s, %s* %%%d", some_val, inner_str, inner_str, val_ptr);
            LLVM_EMITLN(e, "  br label %%L%d", end_label);

            /* None case - get fallback */
            llvm_emit_label(e, none_label);
            forge_node_t* fallback_node = node->data.or_else.fallback;
            int fallback_reg = llvm_emit_expr(e, fallback_node);
            /* Handle literal fallbacks */
            if (fallback_reg < 0 && fallback_node->kind == NODE_INT_LIT) {
                fallback_reg = llvm_next_reg(e);
                LLVM_EMITLN(e, "  %%%d = add i64 0, %lld",
                           fallback_reg, (long long)fallback_node->data.int_val);
            } else if (fallback_reg < 0 && fallback_node->kind == NODE_FLOAT_LIT) {
                fallback_reg = llvm_next_reg(e);
                LLVM_EMITLN(e, "  %%%d = fadd double 0.0, %e",
                           fallback_reg, fallback_node->data.float_val);
            } else if (fallback_reg < 0 && fallback_node->kind == NODE_BOOL_LIT) {
                fallback_reg = llvm_next_reg(e);
                LLVM_EMITLN(e, "  %%%d = or i1 false, %s",
                           fallback_reg, fallback_node->data.bool_val ? "true" : "false");
            }
            /* Track the actual block we're in after emitting fallback
             * (may differ from none_label if fallback creates new blocks) */
            int fallback_block = e->current_block_id;
            LLVM_EMITLN(e, "  br label %%L%d", end_label);

            /* Merge with phi */
            llvm_emit_label(e, end_label);
            int result = llvm_next_reg(e);
            LLVM_EMITLN(e, "  %%%d = phi %s [ %%%d, %%L%d ], [ %%%d, %%L%d ]",
                       result, inner_str, some_val, some_label, fallback_reg, fallback_block);

            return result;
        }

        default:
            /* TODO: More expression types */
            return -1;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Statement Emission
 * ═══════════════════════════════════════════════════════════════════════════ */

int llvm_emit_stmt(forge_llvm_emitter_t* e, forge_node_t* node) {
    if (!node) return -1;

    switch (node->kind) {
        case NODE_VAR_DECL:
        case NODE_CONST_DECL: {
            const char* name = node->data.var_decl.name;
            forge_type_t* type = node->resolved_type;

            /* Get unique LLVM name for this variable (handles shadowing) */
            const char* llvm_name = llvm_declare_var(e, name);

            /* Allocate stack space for variable */
            LLVM_EMITLN(e, "  %%%s = alloca %s", llvm_name, llvm_type_str(e, type));

            /* Initialize if there's an init expression */
            if (node->data.var_decl.init_expr) {
                forge_node_t* init_expr = node->data.var_decl.init_expr;
                int init_reg = llvm_emit_expr(e, init_expr);

                /* For array/struct/some/none literals, the init_reg is a pointer - need to load value */
                if (init_reg >= 0 && (init_expr->kind == NODE_ARRAY_LITERAL ||
                                       init_expr->kind == NODE_RECORD_LITERAL ||
                                       init_expr->kind == NODE_SOME ||
                                       init_expr->kind == NODE_NONE_LIT)) {
                    int loaded = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = load %s, %s* %%%d",
                               loaded, llvm_type_str(e, type), llvm_type_str(e, type), init_reg);
                    LLVM_EMITLN(e, "  store %s %%%d, %s* %%%s",
                               llvm_type_str(e, type), loaded,
                               llvm_type_str(e, type), llvm_name);
                } else if (init_reg >= 0) {
                    LLVM_EMITLN(e, "  store %s %%%d, %s* %%%s",
                               llvm_type_str(e, type), init_reg,
                               llvm_type_str(e, type), llvm_name);
                } else if (init_expr->kind == NODE_INT_LIT) {
                    LLVM_EMITLN(e, "  store i64 %lld, i64* %%%s",
                               (long long)init_expr->data.int_val, llvm_name);
                } else if (init_expr->kind == NODE_FLOAT_LIT) {
                    LLVM_EMITLN(e, "  store double %e, double* %%%s",
                               init_expr->data.float_val, llvm_name);
                } else if (init_expr->kind == NODE_BOOL_LIT) {
                    LLVM_EMITLN(e, "  store i1 %d, i1* %%%s",
                               init_expr->data.bool_val ? 1 : 0, llvm_name);
                }
            }
            return -1;
        }

        case NODE_ASSIGN: {
            forge_node_t* target = node->data.assign.target;
            forge_node_t* value = node->data.assign.value;

            int val_reg = llvm_emit_expr(e, value);

            if (target->kind == NODE_IDENT) {
                const char* name = target->data.name;
                const char* llvm_name = llvm_lookup_var(e, name);
                forge_type_t* type = target->resolved_type;

                if (val_reg >= 0) {
                    /* If value is an alloca (record/array/optional), load first */
                    if (value->kind == NODE_RECORD_LITERAL ||
                        value->kind == NODE_ARRAY_LITERAL ||
                        value->kind == NODE_SOME ||
                        value->kind == NODE_NONE_LIT) {
                        int loaded = llvm_next_reg(e);
                        LLVM_EMITLN(e, "  %%%d = load %s, %s* %%%d",
                                   loaded, llvm_type_str(e, type),
                                   llvm_type_str(e, type), val_reg);
                        LLVM_EMITLN(e, "  store %s %%%d, %s* %%%s",
                                   llvm_type_str(e, type), loaded,
                                   llvm_type_str(e, type), llvm_name);
                    } else {
                        LLVM_EMITLN(e, "  store %s %%%d, %s* %%%s",
                                   llvm_type_str(e, type), val_reg,
                                   llvm_type_str(e, type), llvm_name);
                    }
                } else if (value->kind == NODE_INT_LIT) {
                    LLVM_EMITLN(e, "  store i64 %lld, i64* %%%s",
                               (long long)value->data.int_val, llvm_name);
                } else if (value->kind == NODE_BOOL_LIT) {
                    LLVM_EMITLN(e, "  store i1 %d, i1* %%%s",
                               value->data.bool_val ? 1 : 0, llvm_name);
                } else if (value->kind == NODE_FLOAT_LIT) {
                    LLVM_EMITLN(e, "  store double %a, double* %%%s",
                               value->data.float_val, llvm_name);
                }
            } else if (target->kind == NODE_INDEX) {
                /* Array element assignment: arr[idx] = value */
                forge_node_t* arr_obj = target->data.index.object;
                forge_node_t* idx_node = target->data.index.index;

                int arr_reg = llvm_emit_expr(e, arr_obj);
                int idx_reg = llvm_emit_expr(e, idx_node);

                forge_type_t* arr_type = arr_obj->resolved_type;
                if (arr_type && (arr_type->kind == TY_FIXED_ARRAY || arr_type->kind == TY_DYN_ARRAY)) {
                    forge_type_t* elem_type = arr_type->kind == TY_FIXED_ARRAY ?
                                              arr_type->as.fixed_array.elem_type :
                                              arr_type->as.dyn_array.elem_type;
                    const char* elem_type_str = llvm_type_str(e, elem_type);

                    /* Store array to temp to get pointer */
                    int arr_ptr = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = alloca %%forge_array_t", arr_ptr);
                    LLVM_EMITLN(e, "  store %%forge_array_t %%%d, %%forge_array_t* %%%d", arr_reg, arr_ptr);

                    /* Get data pointer from array struct */
                    int data_field = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = getelementptr %%forge_array_t, %%forge_array_t* %%%d, i32 0, i32 0",
                               data_field, arr_ptr);
                    int data_ptr = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = load i8*, i8** %%%d", data_ptr, data_field);

                    /* Cast to proper element pointer and index */
                    int typed_ptr = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = bitcast i8* %%%d to %s*", typed_ptr, data_ptr, elem_type_str);
                    int elem_ptr = llvm_next_reg(e);
                    if (idx_reg < 0 && idx_node->kind == NODE_INT_LIT) {
                        LLVM_EMITLN(e, "  %%%d = getelementptr %s, %s* %%%d, i64 %lld",
                                   elem_ptr, elem_type_str, elem_type_str, typed_ptr,
                                   (long long)idx_node->data.int_val);
                    } else {
                        LLVM_EMITLN(e, "  %%%d = getelementptr %s, %s* %%%d, i64 %%%d",
                                   elem_ptr, elem_type_str, elem_type_str, typed_ptr, idx_reg);
                    }

                    /* Store value */
                    if (val_reg >= 0) {
                        LLVM_EMITLN(e, "  store %s %%%d, %s* %%%d",
                                   elem_type_str, val_reg, elem_type_str, elem_ptr);
                    } else if (value->kind == NODE_INT_LIT) {
                        LLVM_EMITLN(e, "  store i64 %lld, i64* %%%d",
                                   (long long)value->data.int_val, elem_ptr);
                    } else if (value->kind == NODE_BOOL_LIT) {
                        LLVM_EMITLN(e, "  store i1 %d, i1* %%%d",
                                   value->data.bool_val ? 1 : 0, elem_ptr);
                    } else if (value->kind == NODE_FLOAT_LIT) {
                        LLVM_EMITLN(e, "  store double %a, double* %%%d",
                                   value->data.float_val, elem_ptr);
                    }
                }
            }
            return -1;
        }

        case NODE_EXPR_STMT:
            llvm_emit_expr(e, node->data.expr_stmt.expr);
            return -1;

        case NODE_IF: {
            forge_node_t* cond = node->data.if_stmt.condition;
            int cond_reg = llvm_emit_expr(e, cond);
            int then_label = llvm_next_label(e);
            int end_label = llvm_next_label(e);

            /* Pre-allocate labels for elif chains */
            int elif_count = node->data.if_stmt.elif_count;
            int* elif_labels = NULL;
            int else_label = -1;

            if (elif_count > 0 || node->data.if_stmt.else_body) {
                if (elif_count > 0) {
                    elif_labels = (int*)malloc(sizeof(int) * elif_count * 2);
                    for (int i = 0; i < elif_count; i++) {
                        elif_labels[i * 2] = llvm_next_label(e);      /* condition check label */
                        elif_labels[i * 2 + 1] = llvm_next_label(e);  /* body label */
                    }
                }
                if (node->data.if_stmt.else_body) {
                    else_label = llvm_next_label(e);
                }
            }

            /* Determine first fallthrough target */
            int first_fallthrough;
            if (elif_count > 0) {
                first_fallthrough = elif_labels[0];  /* first elif condition */
            } else if (else_label >= 0) {
                first_fallthrough = else_label;
            } else {
                first_fallthrough = end_label;
            }

            /* Branch based on condition */
            if (cond_reg < 0 && cond->kind == NODE_BOOL_LIT) {
                LLVM_EMITLN(e, "  br i1 %d, label %%L%d, label %%L%d",
                           cond->data.bool_val ? 1 : 0, then_label, first_fallthrough);
            } else {
                LLVM_EMITLN(e, "  br i1 %%%d, label %%L%d, label %%L%d",
                           cond_reg, then_label, first_fallthrough);
            }

            /* Then block */
            llvm_emit_label(e, then_label);
            e->block_terminated = 0;
            llvm_emit_stmt(e, node->data.if_stmt.then_body);
            if (!e->block_terminated) {
                LLVM_EMITLN(e, "  br label %%L%d", end_label);
            }

            /* Elif chains */
            for (int i = 0; i < elif_count; i++) {
                /* Emit condition check */
                llvm_emit_label(e, elif_labels[i * 2]);
                e->block_terminated = 0;
                int elif_cond_reg = llvm_emit_expr(e, node->data.if_stmt.elif_conditions[i]);

                /* Determine next fallthrough */
                int next_fallthrough;
                if (i + 1 < elif_count) {
                    next_fallthrough = elif_labels[(i + 1) * 2];
                } else if (else_label >= 0) {
                    next_fallthrough = else_label;
                } else {
                    next_fallthrough = end_label;
                }

                LLVM_EMITLN(e, "  br i1 %%%d, label %%L%d, label %%L%d",
                           elif_cond_reg, elif_labels[i * 2 + 1], next_fallthrough);

                /* Emit body */
                llvm_emit_label(e, elif_labels[i * 2 + 1]);
                e->block_terminated = 0;
                llvm_emit_stmt(e, node->data.if_stmt.elif_bodies[i]);
                if (!e->block_terminated) {
                    LLVM_EMITLN(e, "  br label %%L%d", end_label);
                }
            }

            /* Else block */
            if (else_label >= 0) {
                llvm_emit_label(e, else_label);
                e->block_terminated = 0;
                llvm_emit_stmt(e, node->data.if_stmt.else_body);
                if (!e->block_terminated) {
                    LLVM_EMITLN(e, "  br label %%L%d", end_label);
                }
            }

            if (elif_labels) free(elif_labels);

            /* End label */
            llvm_emit_label(e, end_label);
            e->block_terminated = 0;
            return -1;
        }

        case NODE_WHILE: {
            int cond_label = llvm_next_label(e);
            int body_label = llvm_next_label(e);
            int end_label = llvm_next_label(e);

            /* Save loop context */
            int prev_in_loop = e->in_loop;
            char exit_buf[32], cont_buf[32];
            snprintf(exit_buf, sizeof(exit_buf), "L%d", end_label);
            snprintf(cont_buf, sizeof(cont_buf), "L%d", cond_label);
            const char* prev_exit = e->loop_exit_label;
            const char* prev_cont = e->loop_cont_label;
            e->in_loop = 1;
            e->loop_exit_label = exit_buf;
            e->loop_cont_label = cont_buf;

            /* Jump to condition check */
            LLVM_EMITLN(e, "  br label %%L%d", cond_label);

            /* Condition block */
            llvm_emit_label(e, cond_label);
            forge_node_t* cond = node->data.while_stmt.condition;
            int cond_reg = llvm_emit_expr(e, cond);
            if (cond_reg < 0 && cond->kind == NODE_BOOL_LIT) {
                LLVM_EMITLN(e, "  br i1 %d, label %%L%d, label %%L%d",
                           cond->data.bool_val ? 1 : 0, body_label, end_label);
            } else {
                LLVM_EMITLN(e, "  br i1 %%%d, label %%L%d, label %%L%d",
                           cond_reg, body_label, end_label);
            }

            /* Body block */
            llvm_emit_label(e, body_label);
            e->block_terminated = 0;
            llvm_emit_stmt(e, node->data.while_stmt.body);
            if (!e->block_terminated) {
                LLVM_EMITLN(e, "  br label %%L%d", cond_label);
            }

            /* End label */
            llvm_emit_label(e, end_label);
            e->block_terminated = 0;

            /* Restore loop context */
            e->in_loop = prev_in_loop;
            e->loop_exit_label = prev_exit;
            e->loop_cont_label = prev_cont;
            return -1;
        }

        case NODE_BLOCK: {
            /* Push a new scope for the block */
            llvm_push_scope(e);
            for (int i = 0; i < node->data.block.count; i++) {
                llvm_emit_stmt(e, node->data.block.stmts[i]);
            }
            /* Pop the block scope */
            llvm_pop_scope(e);
            return -1;
        }

        case NODE_RETURN: {
            if (node->data.return_stmt.value) {
                forge_node_t* ret_val = node->data.return_stmt.value;
                int ret_reg = llvm_emit_expr(e, ret_val);
                forge_type_t* ret_type = ret_val->resolved_type;
                /* For none/some, prefer the procedure's declared return type */
                if ((ret_val->kind == NODE_NONE_LIT || ret_val->kind == NODE_SOME) &&
                    e->current_ret_type) {
                    ret_type = e->current_ret_type;
                }
                if (ret_reg >= 0) {
                    /* If expression returns a pointer (alloca) but we need a value, load it */
                    if (ret_val->kind == NODE_RECORD_LITERAL ||
                        ret_val->kind == NODE_ARRAY_LITERAL ||
                        ret_val->kind == NODE_SOME ||
                        ret_val->kind == NODE_NONE_LIT) {
                        int loaded = llvm_next_reg(e);
                        LLVM_EMITLN(e, "  %%%d = load %s, %s* %%%d",
                                   loaded, llvm_type_str(e, ret_type),
                                   llvm_type_str(e, ret_type), ret_reg);
                        LLVM_EMITLN(e, "  ret %s %%%d", llvm_type_str(e, ret_type), loaded);
                    } else {
                        LLVM_EMITLN(e, "  ret %s %%%d", llvm_type_str(e, ret_type), ret_reg);
                    }
                } else if (ret_val->kind == NODE_INT_LIT) {
                    LLVM_EMITLN(e, "  ret i64 %lld",
                               (long long)ret_val->data.int_val);
                } else if (ret_val->kind == NODE_FLOAT_LIT) {
                    LLVM_EMITLN(e, "  ret double %e", ret_val->data.float_val);
                } else if (ret_val->kind == NODE_BOOL_LIT) {
                    LLVM_EMITLN(e, "  ret i1 %d", ret_val->data.bool_val ? 1 : 0);
                }
            } else {
                LLVM_EMITLN(e, "  ret void");
            }
            e->block_terminated = 1;
            return -1;
        }

        case NODE_BREAK:
            if (e->loop_exit_label) {
                LLVM_EMITLN(e, "  br label %%%s", e->loop_exit_label);
                e->block_terminated = 1;
            }
            return -1;

        case NODE_CONTINUE:
            if (e->loop_cont_label) {
                LLVM_EMITLN(e, "  br label %%%s", e->loop_cont_label);
                e->block_terminated = 1;
            }
            return -1;

        case NODE_FOR: {
            /* For loop: for x in range/array { body } */
            const char* var_name = node->data.for_stmt.var_name;
            forge_node_t* iterable = node->data.for_stmt.iterable;

            /* Generate labels */
            int init_label = llvm_next_label(e);
            int cond_label = llvm_next_label(e);
            int body_label = llvm_next_label(e);
            int incr_label = llvm_next_label(e);
            int end_label = llvm_next_label(e);

            /* Save loop context */
            int prev_in_loop = e->in_loop;
            char exit_buf[32], cont_buf[32];
            snprintf(exit_buf, sizeof(exit_buf), "L%d", end_label);
            snprintf(cont_buf, sizeof(cont_buf), "L%d", incr_label);
            const char* prev_exit = e->loop_exit_label;
            const char* prev_cont = e->loop_cont_label;
            e->in_loop = 1;
            e->loop_exit_label = exit_buf;
            e->loop_cont_label = cont_buf;

            /* Push scope for for-loop variable */
            llvm_push_scope(e);
            const char* llvm_var = llvm_declare_var(e, var_name);

            if (iterable && iterable->kind == NODE_RANGE) {
                /* Range iteration: for x in start..end */
                forge_node_t* start_node = iterable->data.range.start;
                forge_node_t* end_node = iterable->data.range.end;
                int start_reg = llvm_emit_expr(e, start_node);
                int end_reg = llvm_emit_expr(e, end_node);
                int inclusive = iterable->data.range.inclusive;

                /* Allocate loop variable */
                LLVM_EMITLN(e, "  %%%s = alloca i64", llvm_var);
                if (start_reg >= 0) {
                    LLVM_EMITLN(e, "  store i64 %%%d, i64* %%%s", start_reg, llvm_var);
                } else if (start_node->kind == NODE_INT_LIT) {
                    LLVM_EMITLN(e, "  store i64 %lld, i64* %%%s",
                               (long long)start_node->data.int_val, llvm_var);
                }

                LLVM_EMITLN(e, "  %%for_end_%d = alloca i64", init_label);
                if (end_reg >= 0) {
                    LLVM_EMITLN(e, "  store i64 %%%d, i64* %%for_end_%d", end_reg, init_label);
                } else if (end_node->kind == NODE_INT_LIT) {
                    LLVM_EMITLN(e, "  store i64 %lld, i64* %%for_end_%d",
                               (long long)end_node->data.int_val, init_label);
                }

                /* Jump to condition */
                LLVM_EMITLN(e, "  br label %%L%d", cond_label);

                /* Condition: x < end (or x <= end for inclusive) */
                llvm_emit_label(e, cond_label);
                int cur_val = llvm_next_reg(e);
                LLVM_EMITLN(e, "  %%%d = load i64, i64* %%%s", cur_val, llvm_var);
                int end_val = llvm_next_reg(e);
                LLVM_EMITLN(e, "  %%%d = load i64, i64* %%for_end_%d", end_val, init_label);
                int cmp_reg = llvm_next_reg(e);
                if (inclusive) {
                    LLVM_EMITLN(e, "  %%%d = icmp sle i64 %%%d, %%%d", cmp_reg, cur_val, end_val);
                } else {
                    LLVM_EMITLN(e, "  %%%d = icmp slt i64 %%%d, %%%d", cmp_reg, cur_val, end_val);
                }
                LLVM_EMITLN(e, "  br i1 %%%d, label %%L%d, label %%L%d", cmp_reg, body_label, end_label);

                /* Body */
                llvm_emit_label(e, body_label);
                e->block_terminated = 0;
                llvm_emit_stmt(e, node->data.for_stmt.body);
                if (!e->block_terminated) {
                    LLVM_EMITLN(e, "  br label %%L%d", incr_label);
                }

                /* Increment */
                llvm_emit_label(e, incr_label);
                e->block_terminated = 0;
                int load_reg = llvm_next_reg(e);
                LLVM_EMITLN(e, "  %%%d = load i64, i64* %%%s", load_reg, llvm_var);
                int inc_reg = llvm_next_reg(e);
                LLVM_EMITLN(e, "  %%%d = add i64 %%%d, 1", inc_reg, load_reg);
                LLVM_EMITLN(e, "  store i64 %%%d, i64* %%%s", inc_reg, llvm_var);
                LLVM_EMITLN(e, "  br label %%L%d", cond_label);

            } else if (iterable && iterable->kind == NODE_ARRAY_LITERAL) {
                /* Array literal iteration - inline the values */
                /* For simplicity, unroll the loop for small arrays */
                int count = iterable->data.array_lit.count;
                LLVM_EMITLN(e, "  %%%s = alloca i64", llvm_var);

                for (int i = 0; i < count; i++) {
                    int elem_reg = llvm_emit_expr(e, iterable->data.array_lit.elements[i]);
                    LLVM_EMITLN(e, "  store i64 %%%d, i64* %%%s", elem_reg, llvm_var);
                    llvm_emit_stmt(e, node->data.for_stmt.body);
                }
                /* Skip to end - no actual loop structure needed for unrolled */
                LLVM_EMITLN(e, "  br label %%L%d", end_label);

            } else {
                /* Array variable iteration - use runtime array access */
                int arr_val = llvm_emit_expr(e, iterable);

                /* Determine element type */
                forge_type_t* iter_type = iterable->resolved_type;
                forge_type_t* elem_type = NULL;
                if (iter_type && (iter_type->kind == TY_FIXED_ARRAY || iter_type->kind == TY_DYN_ARRAY)) {
                    elem_type = iter_type->kind == TY_FIXED_ARRAY ?
                                iter_type->as.fixed_array.elem_type :
                                iter_type->as.dyn_array.elem_type;
                }
                const char* elem_type_str = llvm_type_str(e, elem_type);

                /* Store array value to temp to get pointer */
                int arr_ptr = llvm_next_reg(e);
                LLVM_EMITLN(e, "  %%%d = alloca %%forge_array_t", arr_ptr);
                LLVM_EMITLN(e, "  store %%forge_array_t %%%d, %%forge_array_t* %%%d", arr_val, arr_ptr);

                /* Allocate index and loop variable */
                LLVM_EMITLN(e, "  %%for_idx_%d = alloca i64", init_label);
                LLVM_EMITLN(e, "  store i64 0, i64* %%for_idx_%d", init_label);
                LLVM_EMITLN(e, "  %%%s = alloca %s", llvm_var, elem_type_str);

                /* Get array length - extract from struct field 1 (i32) */
                int len_ptr = llvm_next_reg(e);
                LLVM_EMITLN(e, "  %%%d = getelementptr %%forge_array_t, %%forge_array_t* %%%d, i32 0, i32 1",
                           len_ptr, arr_ptr);
                int len_i32 = llvm_next_reg(e);
                LLVM_EMITLN(e, "  %%%d = load i32, i32* %%%d", len_i32, len_ptr);
                int len_reg = llvm_next_reg(e);
                LLVM_EMITLN(e, "  %%%d = sext i32 %%%d to i64", len_reg, len_i32);

                /* Jump to condition */
                LLVM_EMITLN(e, "  br label %%L%d", cond_label);

                /* Condition: idx < len */
                llvm_emit_label(e, cond_label);
                int idx_val = llvm_next_reg(e);
                LLVM_EMITLN(e, "  %%%d = load i64, i64* %%for_idx_%d", idx_val, init_label);
                int cmp_reg = llvm_next_reg(e);
                LLVM_EMITLN(e, "  %%%d = icmp slt i64 %%%d, %%%d", cmp_reg, idx_val, len_reg);
                LLVM_EMITLN(e, "  br i1 %%%d, label %%L%d, label %%L%d", cmp_reg, body_label, end_label);

                /* Body */
                llvm_emit_label(e, body_label);
                e->block_terminated = 0;
                /* Load element from array */
                int data_ptr = llvm_next_reg(e);
                LLVM_EMITLN(e, "  %%%d = getelementptr %%forge_array_t, %%forge_array_t* %%%d, i32 0, i32 0",
                           data_ptr, arr_ptr);
                int data_reg = llvm_next_reg(e);
                LLVM_EMITLN(e, "  %%%d = load i8*, i8** %%%d", data_reg, data_ptr);
                int typed_data = llvm_next_reg(e);
                LLVM_EMITLN(e, "  %%%d = bitcast i8* %%%d to %s*", typed_data, data_reg, elem_type_str);
                int elem_ptr = llvm_next_reg(e);
                LLVM_EMITLN(e, "  %%%d = getelementptr %s, %s* %%%d, i64 %%%d",
                           elem_ptr, elem_type_str, elem_type_str, typed_data, idx_val);
                int elem_reg = llvm_next_reg(e);
                LLVM_EMITLN(e, "  %%%d = load %s, %s* %%%d", elem_reg, elem_type_str, elem_type_str, elem_ptr);
                LLVM_EMITLN(e, "  store %s %%%d, %s* %%%s", elem_type_str, elem_reg, elem_type_str, llvm_var);

                llvm_emit_stmt(e, node->data.for_stmt.body);
                if (!e->block_terminated) {
                    LLVM_EMITLN(e, "  br label %%L%d", incr_label);
                }

                /* Increment index */
                llvm_emit_label(e, incr_label);
                e->block_terminated = 0;
                int load_idx = llvm_next_reg(e);
                LLVM_EMITLN(e, "  %%%d = load i64, i64* %%for_idx_%d", load_idx, init_label);
                int inc_idx = llvm_next_reg(e);
                LLVM_EMITLN(e, "  %%%d = add i64 %%%d, 1", inc_idx, load_idx);
                LLVM_EMITLN(e, "  store i64 %%%d, i64* %%for_idx_%d", inc_idx, init_label);
                LLVM_EMITLN(e, "  br label %%L%d", cond_label);
            }

            /* End label */
            llvm_emit_label(e, end_label);
            e->block_terminated = 0;

            /* Restore loop context */
            e->in_loop = prev_in_loop;
            e->loop_exit_label = prev_exit;
            e->loop_cont_label = prev_cont;

            /* Pop the for-loop scope */
            llvm_pop_scope(e);
            return -1;
        }

        case NODE_COMPOUND_ASSIGN: {
            /* target op= value  →  target = target op value */
            forge_node_t* target = node->data.compound_assign.target;
            forge_node_t* value = node->data.compound_assign.value;
            int op = node->data.compound_assign.op;

            /* Emit the value */
            int val_reg = llvm_emit_expr(e, value);

            if (target->kind == NODE_IDENT) {
                const char* name = target->data.name;
                const char* llvm_name = llvm_lookup_var(e, name);
                forge_type_t* type = target->resolved_type;
                int is_float = (type && type->kind == TY_FLOAT);

                /* Load current value */
                int cur_reg = llvm_next_reg(e);
                LLVM_EMITLN(e, "  %%%d = load %s, %s* %%%s",
                           cur_reg, llvm_type_str(e, type),
                           llvm_type_str(e, type), llvm_name);

                /* Prepare value operand string */
                char val_str[64];
                if (val_reg >= 0) {
                    snprintf(val_str, sizeof(val_str), "%%%d", val_reg);
                } else if (value->kind == NODE_INT_LIT) {
                    snprintf(val_str, sizeof(val_str), "%lld", (long long)value->data.int_val);
                } else if (value->kind == NODE_FLOAT_LIT) {
                    snprintf(val_str, sizeof(val_str), "%a", value->data.float_val);
                } else {
                    snprintf(val_str, sizeof(val_str), "0");
                }

                /* Compute new value */
                int new_reg = llvm_next_reg(e);
                switch (op) {
                    case TOK_PLUS_EQ:
                        LLVM_EMITLN(e, "  %%%d = %s %s %%%d, %s", new_reg,
                                   is_float ? "fadd" : "add",
                                   is_float ? "double" : "i64", cur_reg, val_str);
                        break;
                    case TOK_MINUS_EQ:
                        LLVM_EMITLN(e, "  %%%d = %s %s %%%d, %s", new_reg,
                                   is_float ? "fsub" : "sub",
                                   is_float ? "double" : "i64", cur_reg, val_str);
                        break;
                    case TOK_STAR_EQ:
                        LLVM_EMITLN(e, "  %%%d = %s %s %%%d, %s", new_reg,
                                   is_float ? "fmul" : "mul",
                                   is_float ? "double" : "i64", cur_reg, val_str);
                        break;
                    case TOK_SLASH_EQ:
                        if (is_float) {
                            LLVM_EMITLN(e, "  %%%d = fdiv double %%%d, %s", new_reg, cur_reg, val_str);
                        } else {
                            int src_idx = add_string_constant(e, e->source_file ? e->source_file : "unknown");
                            /* Use new_reg for GEP, allocate fresh for result */
                            int src_ptr = new_reg;
                            LLVM_EMITLN(e, "  %%%d = getelementptr [%zu x i8], [%zu x i8]* @.str.%d, i64 0, i64 0",
                                       src_ptr, strlen(e->source_file ? e->source_file : "unknown") + 1,
                                       strlen(e->source_file ? e->source_file : "unknown") + 1, src_idx);
                            new_reg = llvm_next_reg(e);
                            LLVM_EMITLN(e, "  %%%d = call i64 @forge_div_check(i64 %%%d, i64 %s, i8* %%%d, i32 %d)",
                                       new_reg, cur_reg, val_str, src_ptr, node->line);
                        }
                        break;
                    case TOK_PERCENT_EQ: {
                        int src_idx = add_string_constant(e, e->source_file ? e->source_file : "unknown");
                        /* Use new_reg for GEP, allocate fresh for result */
                        int src_ptr = new_reg;
                        LLVM_EMITLN(e, "  %%%d = getelementptr [%zu x i8], [%zu x i8]* @.str.%d, i64 0, i64 0",
                                   src_ptr, strlen(e->source_file ? e->source_file : "unknown") + 1,
                                   strlen(e->source_file ? e->source_file : "unknown") + 1, src_idx);
                        new_reg = llvm_next_reg(e);
                        LLVM_EMITLN(e, "  %%%d = call i64 @forge_mod_check(i64 %%%d, i64 %s, i8* %%%d, i32 %d)",
                                   new_reg, cur_reg, val_str, src_ptr, node->line);
                        break;
                    }
                    default:
                        LLVM_EMITLN(e, "  %%%d = add i64 %%%d, %s", new_reg, cur_reg, val_str);
                        break;
                }

                /* Store result back */
                LLVM_EMITLN(e, "  store %s %%%d, %s* %%%s",
                           llvm_type_str(e, type), new_reg,
                           llvm_type_str(e, type), llvm_name);
            } else if (target->kind == NODE_INDEX) {
                /* Compound assign to array element: arr[i] op= value */
                forge_node_t* arr_obj = target->data.index.object;
                forge_node_t* idx_node = target->data.index.index;

                int arr_reg = llvm_emit_expr(e, arr_obj);
                int idx_reg = llvm_emit_expr(e, idx_node);

                forge_type_t* arr_type = arr_obj->resolved_type;
                if (arr_type && (arr_type->kind == TY_FIXED_ARRAY || arr_type->kind == TY_DYN_ARRAY)) {
                    forge_type_t* elem_type = arr_type->kind == TY_FIXED_ARRAY ?
                                              arr_type->as.fixed_array.elem_type :
                                              arr_type->as.dyn_array.elem_type;
                    const char* elem_type_str = llvm_type_str(e, elem_type);
                    int is_float = (elem_type && elem_type->kind == TY_FLOAT);

                    /* Get element pointer */
                    int arr_ptr = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = alloca %%forge_array_t", arr_ptr);
                    LLVM_EMITLN(e, "  store %%forge_array_t %%%d, %%forge_array_t* %%%d", arr_reg, arr_ptr);
                    int data_field = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = getelementptr %%forge_array_t, %%forge_array_t* %%%d, i32 0, i32 0",
                               data_field, arr_ptr);
                    int data_ptr = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = load i8*, i8** %%%d", data_ptr, data_field);
                    int typed_ptr = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = bitcast i8* %%%d to %s*", typed_ptr, data_ptr, elem_type_str);
                    int elem_ptr = llvm_next_reg(e);
                    if (idx_reg < 0 && idx_node->kind == NODE_INT_LIT) {
                        LLVM_EMITLN(e, "  %%%d = getelementptr %s, %s* %%%d, i64 %lld",
                                   elem_ptr, elem_type_str, elem_type_str, typed_ptr,
                                   (long long)idx_node->data.int_val);
                    } else {
                        LLVM_EMITLN(e, "  %%%d = getelementptr %s, %s* %%%d, i64 %%%d",
                                   elem_ptr, elem_type_str, elem_type_str, typed_ptr, idx_reg);
                    }

                    /* Load current value */
                    int cur_val = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = load %s, %s* %%%d", cur_val, elem_type_str, elem_type_str, elem_ptr);

                    /* Prepare value operand */
                    char val_str[64];
                    if (val_reg >= 0) snprintf(val_str, sizeof(val_str), "%%%d", val_reg);
                    else if (value->kind == NODE_INT_LIT) snprintf(val_str, sizeof(val_str), "%lld", (long long)value->data.int_val);
                    else snprintf(val_str, sizeof(val_str), "0");

                    /* Compute */
                    int new_val = llvm_next_reg(e);
                    switch (op) {
                        case TOK_PLUS_EQ:
                            LLVM_EMITLN(e, "  %%%d = %s %s %%%d, %s", new_val,
                                       is_float ? "fadd" : "add", elem_type_str, cur_val, val_str);
                            break;
                        case TOK_MINUS_EQ:
                            LLVM_EMITLN(e, "  %%%d = %s %s %%%d, %s", new_val,
                                       is_float ? "fsub" : "sub", elem_type_str, cur_val, val_str);
                            break;
                        case TOK_STAR_EQ:
                            LLVM_EMITLN(e, "  %%%d = %s %s %%%d, %s", new_val,
                                       is_float ? "fmul" : "mul", elem_type_str, cur_val, val_str);
                            break;
                        default:
                            LLVM_EMITLN(e, "  %%%d = add %s %%%d, %s", new_val, elem_type_str, cur_val, val_str);
                            break;
                    }

                    /* Store back */
                    LLVM_EMITLN(e, "  store %s %%%d, %s* %%%d", elem_type_str, new_val, elem_type_str, elem_ptr);
                }
            }
            return -1;
        }

        case NODE_WITH_ALLOC: {
            /* with alloc(Type, Size) as name: -> scoped forge_array_t */
            const char* var_name = node->data.with_alloc.var_name;
            forge_type_t* elem_type = NULL;
            if (node->data.with_alloc.type_expr && node->data.with_alloc.type_expr->resolved_type) {
                elem_type = node->data.with_alloc.type_expr->resolved_type;
            }

            /* Determine element size */
            int elem_size = 8; /* default i64 */
            if (elem_type) {
                if (elem_type->kind == TY_INT) elem_size = 8;
                else if (elem_type->kind == TY_FLOAT) elem_size = 8;
                else if (elem_type->kind == TY_BOOL) elem_size = 1;
            }

            /* Evaluate size expression */
            int size_reg = -1;
            if (node->data.with_alloc.size_expr) {
                size_reg = llvm_emit_expr(e, node->data.with_alloc.size_expr);
            }

            /* Create array: forge_array_create(elem_size, initial_cap) */
            char size_str[64];
            if (size_reg >= 0) {
                /* Truncate i64 to i32 for the capacity parameter */
                int trunc_reg = llvm_next_reg(e);
                LLVM_EMITLN(e, "  %%%d = trunc i64 %%%d to i32", trunc_reg, size_reg);
                snprintf(size_str, sizeof(size_str), "%%%d", trunc_reg);
            } else if (node->data.with_alloc.size_expr && node->data.with_alloc.size_expr->kind == NODE_INT_LIT) {
                snprintf(size_str, sizeof(size_str), "%lld", (long long)node->data.with_alloc.size_expr->data.int_val);
            } else {
                snprintf(size_str, sizeof(size_str), "0");
            }

            int arr_reg = llvm_next_reg(e);
            LLVM_EMITLN(e, "  %%%d = call %%forge_array_t @forge_array_create(i64 %d, i32 %s)",
                       arr_reg, elem_size, size_str);

            /* Allocate stack slot and store */
            llvm_push_scope(e);
            const char* llvm_name = llvm_declare_var(e, var_name);
            LLVM_EMITLN(e, "  %%%s = alloca %%forge_array_t", llvm_name);
            LLVM_EMITLN(e, "  store %%forge_array_t %%%d, %%forge_array_t* %%%s", arr_reg, llvm_name);

            /* Set length = size and zero-initialize data */
            if (node->data.with_alloc.size_expr) {
                /* Set len field */
                int len_ptr = llvm_next_reg(e);
                LLVM_EMITLN(e, "  %%%d = getelementptr %%forge_array_t, %%forge_array_t* %%%s, i32 0, i32 1",
                           len_ptr, llvm_name);
                if (size_reg >= 0) {
                    int trunc_len = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = trunc i64 %%%d to i32", trunc_len, size_reg);
                    LLVM_EMITLN(e, "  store i32 %%%d, i32* %%%d", trunc_len, len_ptr);
                } else {
                    LLVM_EMITLN(e, "  store i32 %s, i32* %%%d", size_str, len_ptr);
                }

                /* Zero-initialize data: memset(data, 0, size * elem_size) */
                int data_field = llvm_next_reg(e);
                LLVM_EMITLN(e, "  %%%d = getelementptr %%forge_array_t, %%forge_array_t* %%%s, i32 0, i32 0",
                           data_field, llvm_name);
                int data_ptr = llvm_next_reg(e);
                LLVM_EMITLN(e, "  %%%d = load i8*, i8** %%%d", data_ptr, data_field);

                if (size_reg >= 0) {
                    int byte_count = llvm_next_reg(e);
                    LLVM_EMITLN(e, "  %%%d = mul i64 %%%d, %d", byte_count, size_reg, elem_size);
                    LLVM_EMITLN(e, "  call void @llvm.memset.p0i8.i64(i8* %%%d, i8 0, i64 %%%d, i1 false)",
                               data_ptr, byte_count);
                } else {
                    long long total_bytes = node->data.with_alloc.size_expr->data.int_val * elem_size;
                    LLVM_EMITLN(e, "  call void @llvm.memset.p0i8.i64(i8* %%%d, i8 0, i64 %lld, i1 false)",
                               data_ptr, total_bytes);
                }
            }

            /* Emit body */
            if (node->data.with_alloc.body && node->data.with_alloc.body->kind == NODE_BLOCK) {
                for (int i = 0; i < node->data.with_alloc.body->data.block.count; i++) {
                    llvm_emit_stmt(e, node->data.with_alloc.body->data.block.stmts[i]);
                }
            }

            /* Free the array */
            LLVM_EMITLN(e, "  call void @forge_array_free(%%forge_array_t* %%%s)", llvm_name);

            llvm_pop_scope(e);
            return -1;
        }

        default:
            return -1;
    }
}



/* ═══════════════════════════════════════════════════════════════════════════
 * Procedure Emission
 * ═══════════════════════════════════════════════════════════════════════════ */

static void emit_proc_decl(forge_llvm_emitter_t* e, forge_node_t* node) {
    if (node->kind != NODE_PROC_DECL) return;

    const char* name = node->data.proc.name;
    /* return_type is a type expression node, get resolved_type from proc node */
    forge_type_t* ret_type = node->data.proc.return_type ? node->data.proc.return_type->resolved_type : NULL;
    int param_count = node->data.proc.param_count;

    /* Rename Forge's main to forge_main to avoid conflict with C main */
    const char* llvm_func_name = (strcmp(name, "main") == 0) ? "forge_main" : name;

    /* Reset register and var counter for each function */
    e->reg_counter = 0;
    e->var_counter = 0;
    e->current_ret_type = ret_type;

    /* Push a new scope for this function */
    llvm_push_scope(e);

    /* Function signature */
    LLVM_EMIT(e, "define %s @%s(", llvm_type_str(e, ret_type), llvm_func_name);

    for (int i = 0; i < param_count; i++) {
        if (i > 0) LLVM_EMIT(e, ", ");
        /* params[i].type_expr is a type expression node */
        forge_type_t* param_type = node->data.proc.params[i].type_expr ?
                                   node->data.proc.params[i].type_expr->resolved_type : NULL;
        LLVM_EMIT(e, "%s %%%s.arg", llvm_type_str(e, param_type),
                 node->data.proc.params[i].name);
    }

    LLVM_EMITLN(e, ") {");
    LLVM_EMITLN(e, "entry:");

    /* Allocate stack space for parameters and copy them */
    for (int i = 0; i < param_count; i++) {
        const char* param_name = node->data.proc.params[i].name;
        forge_type_t* param_type = node->data.proc.params[i].type_expr ?
                                   node->data.proc.params[i].type_expr->resolved_type : NULL;

        /* Register parameter in scope and get unique name */
        const char* llvm_name = llvm_declare_var(e, param_name);

        LLVM_EMITLN(e, "  %%%s = alloca %s", llvm_name, llvm_type_str(e, param_type));
        LLVM_EMITLN(e, "  store %s %%%s.arg, %s* %%%s",
                   llvm_type_str(e, param_type), param_name,
                   llvm_type_str(e, param_type), llvm_name);
    }

    /* Emit function body */
    if (node->data.proc.body) {
        llvm_emit_stmt(e, node->data.proc.body);
    }

    /* Add implicit return if needed */
    if (!ret_type || ret_type->kind == TY_VOID) {
        LLVM_EMITLN(e, "  ret void");
    }

    LLVM_EMITLN(e, "}");
    LLVM_NEWLINE(e);

    /* Pop the function scope */
    llvm_pop_scope(e);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * String Constants Emission
 * ═══════════════════════════════════════════════════════════════════════════ */

static void emit_string_constants(forge_llvm_emitter_t* e) {
    for (int i = 0; i < e->string_constants.count; i++) {
        const char* str = e->string_constants.strings[i];
        size_t len = strlen(str);

        LLVM_EMIT(e, "@.str.%d = private unnamed_addr constant [%zu x i8] c\"",
                 i, len + 1);

        /* Emit string with escaping */
        for (size_t j = 0; j < len; j++) {
            unsigned char c = (unsigned char)str[j];
            if (c == '\\' || c == '"' || c < 32 || c > 126) {
                LLVM_EMIT(e, "\\%02X", c);
            } else {
                LLVM_EMIT(e, "%c", c);
            }
        }

        LLVM_EMITLN(e, "\\00\"");  /* Null terminator */
    }
    LLVM_NEWLINE(e);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Top-Level Declaration Emission
 * ═══════════════════════════════════════════════════════════════════════════ */

static void emit_declaration(forge_llvm_emitter_t* e, forge_node_t* node) {
    switch (node->kind) {
        case NODE_PROC_DECL:
            emit_proc_decl(e, node);
            break;
        case NODE_RECORD_DECL:
            emit_record_decl(e, node);
            break;
        default:
            /* Global variables, etc. - TODO */
            break;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Program Emission (Main Entry Point)
 * ═══════════════════════════════════════════════════════════════════════════ */

void llvm_emitter_emit_program(forge_llvm_emitter_t* e, forge_node_t* program) {
    if (!program || program->kind != NODE_PROGRAM) return;

    /* Module header */
    LLVM_EMITLN(e, "; FORGE LLVM IR Output");
    LLVM_EMITLN(e, "; Source: %s", e->source_file ? e->source_file : "unknown");
    LLVM_EMITLN(e, "; Target: x86_64-pc-linux-gnu");
    LLVM_NEWLINE(e);

    /* Runtime declarations */
    emit_runtime_declarations(e);

    /* First pass: collect string constants and emit record types */
    for (int i = 0; i < program->data.program.decl_count; i++) {
        forge_node_t* decl = program->data.program.decls[i];
        if (decl->kind == NODE_RECORD_DECL) {
            emit_record_decl(e, decl);
        }
    }
    LLVM_NEWLINE(e);

    /* Second pass: emit procedures */
    int has_main = 0;
    for (int i = 0; i < program->data.program.decl_count; i++) {
        forge_node_t* decl = program->data.program.decls[i];
        if (decl->kind == NODE_PROC_DECL) {
            /* Check if this is the main procedure */
            if (strcmp(decl->data.proc.name, "main") == 0) {
                has_main = 1;
            }
            emit_proc_decl(e, decl);
        }
    }

    /* Emit C main entry point if there's a Forge main */
    if (has_main) {
        LLVM_EMITLN(e, "; Entry point - C main that calls Forge main");
        LLVM_EMITLN(e, "define i32 @main(i32 %%argc, i8** %%argv) {");
        LLVM_EMITLN(e, "entry:");
        LLVM_EMITLN(e, "  call void @forge_runtime_init(i32 %%argc, i8** %%argv)");
        LLVM_EMITLN(e, "  call void @forge_main()");
        LLVM_EMITLN(e, "  ret i32 0");
        LLVM_EMITLN(e, "}");
        LLVM_NEWLINE(e);
    }

    /* Emit string constants at the end */
    emit_string_constants(e);
}