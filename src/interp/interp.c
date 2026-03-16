/*
 * FORGE Language Toolchain
 * interp.c - Tree-walking interpreter implementation
 */

#define _GNU_SOURCE              /* For memmem */
#define _POSIX_C_SOURCE 200809L  /* For getline, ssize_t */

#include "interp/interp.h"
#include "lexer/lexer.h"
#include "../runtime/forge_runtime.h"  /* For serial functions */
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <unistd.h>  /* access(), F_OK */
#include <limits.h>  /* INT64_MIN */

/* Forward declarations */
static forge_value_t interp_call_module_proc(forge_interp_t* interp,
                                              forge_module_t* module,
                                              const char* name,
                                              forge_value_t* args, int arg_count);

/* Forward declarations for stdlib dispatchers */
static int try_stdlib_io(forge_interp_t* interp, const char* proc_name,
                         forge_value_t* args, int arg_count, forge_value_t* result);
static int try_stdlib_str(forge_interp_t* interp, const char* proc_name,
                          forge_value_t* args, int arg_count, forge_value_t* result);
static int try_stdlib_math(forge_interp_t* interp, const char* proc_name,
                           forge_value_t* args, int arg_count, forge_value_t* result);
static int try_stdlib_sys(forge_interp_t* interp, const char* proc_name,
                          forge_value_t* args, int arg_count, forge_value_t* result);
static int try_stdlib_time(forge_interp_t* interp, const char* proc_name,
                           forge_value_t* args, int arg_count, forge_value_t* result);
static int try_stdlib_buf(forge_interp_t* interp, const char* proc_name,
                          forge_value_t* args, int arg_count, forge_value_t* result);
static int try_stdlib_serial(forge_interp_t* interp, const char* proc_name,
                             forge_value_t* args, int arg_count, forge_value_t* result);
static int try_stdlib_nmea(forge_interp_t* interp, const char* proc_name,
                           forge_value_t* args, int arg_count, forge_value_t* result);

/* ─────────────────────────────────────────────────────────────────────────────
 * Error Handling
 * ───────────────────────────────────────────────────────────────────────────── */

void interp_error(forge_interp_t* interp, int line, int col, const char* fmt, ...) {
    interp->had_error = 1;

    va_list args;
    va_start(args, fmt);

    /* Format: "Runtime error: <message>" */
    int offset = snprintf(interp->error_msg, sizeof(interp->error_msg),
                          "Runtime error: ");
    offset += vsnprintf(interp->error_msg + offset, sizeof(interp->error_msg) - offset,
                        fmt, args);
    va_end(args);

    fprintf(stderr, "%s\n", interp->error_msg);

    /* Print location info */
    if (line > 0) {
        const char* filename = NULL;
        if (interp->current_module && interp->current_module->filepath) {
            filename = interp->current_module->filepath;
        } else if (interp->current_filename) {
            filename = interp->current_filename;
        }
        if (filename) {
            fprintf(stderr, "  at %s:%d:%d\n", filename, line, col);
        } else {
            fprintf(stderr, "  at line %d, col %d\n", line, col);
        }
    }

    interp_print_stack(interp);
}

void interp_print_stack(forge_interp_t* interp) {
    if (interp->call_depth == 0) return;

    for (int i = interp->call_depth - 1; i >= 0; i--) {
        const char* proc_name = interp->call_stack[i].proc_name;
        const char* filename = interp->call_stack[i].filename;
        int line = interp->call_stack[i].line;

        if (!proc_name) proc_name = "<anonymous>";

        if (filename) {
            fprintf(stderr, "  in proc %s() at %s:%d\n", proc_name, filename, line);
        } else {
            fprintf(stderr, "  in proc %s() at line %d\n", proc_name, line);
        }
    }
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Interpreter Lifecycle
 * ───────────────────────────────────────────────────────────────────────────── */

forge_interp_t* interp_create(forge_arena_t* arena, forge_strtable_t* strtable) {
    forge_interp_t* interp = forge_malloc(sizeof(forge_interp_t));

    interp->arena = arena;
    interp->strtable = strtable;
    interp->modules = hashmap_create();
    interp->current_module = NULL;
    interp->main_module = NULL;
    interp->procedures = hashmap_create();
    interp->records = hashmap_create();
    interp->channels = hashmap_create();
    interp->globals = env_create(NULL);
    interp->current_filename = NULL;
    interp->call_depth = 0;
    interp->had_error = 0;
    interp->error_msg[0] = '\0';

    return interp;
}

/* Helper: destroy a module and its resources */
static void module_destroy(forge_module_t* module) {
    if (!module) return;
    hashmap_destroy(module->procedures);
    hashmap_destroy(module->records);
    env_destroy(module->env);
    forge_free(module);
}

void interp_destroy(forge_interp_t* interp) {
    if (!interp) return;

    /* Destroy all loaded modules */
    forge_hashmap_iter_t iter = hashmap_iter(interp->modules);
    while (hashmap_iter_next(&iter)) {
        module_destroy((forge_module_t*)hashmap_iter_value(&iter));
    }
    hashmap_destroy(interp->modules);

    /* Destroy all channels */
    iter = hashmap_iter(interp->channels);
    while (hashmap_iter_next(&iter)) {
        forge_free(hashmap_iter_value(&iter));
    }
    hashmap_destroy(interp->channels);

    hashmap_destroy(interp->procedures);
    hashmap_destroy(interp->records);
    env_destroy(interp->globals);
    forge_free(interp);
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Helper: Type checking for operations
 * ───────────────────────────────────────────────────────────────────────────── */

static int is_numeric(forge_value_t v) {
    return v.kind == VAL_INT || v.kind == VAL_FLOAT ||
           v.kind == VAL_UINT || v.kind == VAL_FLOAT32 || v.kind == VAL_BYTE;
}

static double as_float(forge_value_t v) {
    switch (v.kind) {
        case VAL_INT:
        case VAL_BYTE:     return (double)v.as.i;
        case VAL_UINT:     return (double)v.as.u;
        case VAL_FLOAT:    return v.as.f;
        case VAL_FLOAT32:  return (double)v.as.f32;
        default:           return 0.0;
    }
}

static int both_int(forge_value_t a, forge_value_t b) {
    return (a.kind == VAL_INT || a.kind == VAL_BYTE || a.kind == VAL_UINT) &&
           (b.kind == VAL_INT || b.kind == VAL_BYTE || b.kind == VAL_UINT);
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Binary Operators
 * ───────────────────────────────────────────────────────────────────────────── */

static forge_value_t eval_binary_arith(forge_interp_t* interp, int op,
                                        forge_value_t left, forge_value_t right,
                                        int line, int col) {
    if (!is_numeric(left) || !is_numeric(right)) {
        interp_error(interp, line, col, "Arithmetic requires numeric operands");
        return val_none();
    }
    
    /* Integer arithmetic if both are integers */
    if (both_int(left, right)) {
        long long a = (left.kind == VAL_UINT) ? (long long)left.as.u : left.as.i;
        long long b = (right.kind == VAL_UINT) ? (long long)right.as.u : right.as.i;
        
        switch (op) {
            case TOK_PLUS:    return val_int(a + b);
            case TOK_MINUS:   return val_int(a - b);
            case TOK_STAR:    return val_int(a * b);
            case TOK_SLASH:
                if (b == 0) {
                    interp_error(interp, line, col, "Division by zero");
                    return val_none();
                }
                return val_int(a / b);
            case TOK_PERCENT:
                if (b == 0) {
                    interp_error(interp, line, col, "Modulo by zero");
                    return val_none();
                }
                return val_int(a % b);
            default:
                interp_error(interp, line, col, "Unknown arithmetic operator");
                return val_none();
        }
    }
    
    /* Float arithmetic */
    double a = as_float(left);
    double b = as_float(right);
    
    switch (op) {
        case TOK_PLUS:    return val_float(a + b);
        case TOK_MINUS:   return val_float(a - b);
        case TOK_STAR:    return val_float(a * b);
        case TOK_SLASH:
            if (b == 0.0) {
                interp_error(interp, line, col, "Division by zero");
                return val_none();
            }
            return val_float(a / b);
        case TOK_PERCENT:
            return val_float(fmod(a, b));
        default:
            interp_error(interp, line, col, "Unknown arithmetic operator");
            return val_none();
    }
}

static forge_value_t eval_binary_compare(forge_interp_t* interp, int op,
                                          forge_value_t left, forge_value_t right,
                                          int line, int col) {
    /* Equality works for any types */
    if (op == TOK_EQ) {
        return val_bool(val_equal(left, right));
    }
    if (op == TOK_NEQ) {
        return val_bool(!val_equal(left, right));
    }

    /* Ordering requires numeric types */
    if (!is_numeric(left) || !is_numeric(right)) {
        interp_error(interp, line, col, "Comparison requires numeric operands");
        return val_none();
    }

    double a = as_float(left);
    double b = as_float(right);

    switch (op) {
        case TOK_LT:   return val_bool(a < b);
        case TOK_GT:   return val_bool(a > b);
        case TOK_LEQ:  return val_bool(a <= b);
        case TOK_GEQ:  return val_bool(a >= b);
        default:
            interp_error(interp, line, col, "Unknown comparison operator");
            return val_none();
    }
}

static forge_value_t eval_binary_bitwise(forge_interp_t* interp, int op,
                                          forge_value_t left, forge_value_t right,
                                          int line, int col) {
    if (!both_int(left, right)) {
        interp_error(interp, line, col, "Bitwise operators require integer operands");
        return val_none();
    }

    long long a = (left.kind == VAL_UINT) ? (long long)left.as.u : left.as.i;
    long long b = (right.kind == VAL_UINT) ? (long long)right.as.u : right.as.i;

    switch (op) {
        case TOK_AMP:     return val_int(a & b);
        case TOK_PIPE:    return val_int(a | b);
        case TOK_CARET:   return val_int(a ^ b);
        case TOK_LSHIFT:  return val_int(a << b);
        case TOK_RSHIFT:  return val_int(a >> b);
        default:
            interp_error(interp, line, col, "Unknown bitwise operator");
            return val_none();
    }
}

static forge_value_t eval_string_concat(forge_interp_t* interp,
                                         forge_value_t left, forge_value_t right,
                                         int line, int col) {
    (void)interp; (void)line; (void)col;

    /* Convert both to strings and concatenate */
    char* left_str = val_to_str(left);
    char* right_str = val_to_str(right);

    int left_len = strlen(left_str);
    int right_len = strlen(right_str);
    int total_len = left_len + right_len;

    char* result = forge_malloc(total_len + 1);
    memcpy(result, left_str, left_len);
    memcpy(result + left_len, right_str, right_len);
    result[total_len] = '\0';

    forge_free(left_str);
    forge_free(right_str);

    /* Create owned string value */
    forge_value_t v = val_str_copy(result, total_len);
    forge_free(result);
    return v;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Unary Operators
 * ───────────────────────────────────────────────────────────────────────────── */

static forge_value_t eval_unary(forge_interp_t* interp, int op,
                                 forge_value_t operand, int line, int col) {
    switch (op) {
        case TOK_MINUS:
            if (!is_numeric(operand)) {
                interp_error(interp, line, col, "Negation requires numeric operand");
                return val_none();
            }
            if (operand.kind == VAL_INT || operand.kind == VAL_BYTE) {
                return val_int(-operand.as.i);
            }
            if (operand.kind == VAL_UINT) {
                return val_int(-(long long)operand.as.u);
            }
            if (operand.kind == VAL_FLOAT32) {
                return val_float32(-operand.as.f32);
            }
            return val_float(-operand.as.f);

        case TOK_NOT:
            return val_bool(!val_is_truthy(operand));

        case TOK_TILDE:
            if (!both_int(operand, operand)) {
                interp_error(interp, line, col, "Bitwise NOT requires integer operand");
                return val_none();
            }
            return val_int(~operand.as.i);

        default:
            interp_error(interp, line, col, "Unknown unary operator");
            return val_none();
    }
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Expression Evaluation
 * ───────────────────────────────────────────────────────────────────────────── */

forge_value_t interp_eval_expr(forge_interp_t* interp, forge_env_t* env,
                                forge_node_t* expr) {
    if (!expr) {
        return val_none();
    }

    if (interp->had_error) {
        return val_none();
    }

    switch (expr->kind) {
        /* === Literals === */

        case NODE_INT_LIT:
            return val_int(expr->data.int_val);

        case NODE_FLOAT_LIT:
            return val_float(expr->data.float_val);

        case NODE_STR_LIT:
            return val_str_lit(expr->data.str_val);

        case NODE_BOOL_LIT:
            return val_bool(expr->data.bool_val);

        case NODE_NONE_LIT:
            return val_none();

        /* === Identifier === */

        case NODE_IDENT: {
            forge_value_t val = env_get(env, expr->data.name);
            if (val.kind == VAL_NONE && !env_has(env, expr->data.name)) {
                interp_error(interp, expr->line, expr->column,
                             "Undefined variable '%s'", expr->data.name);
            }
            return val;
        }

        /* === Qualified Identifier (module.name) === */

        case NODE_QUALIFIED_IDENT: {
            const char* mod_name = expr->data.qualified.module_name;
            const char* sym_name = expr->data.qualified.symbol_name;

            forge_module_t* module = interp_get_module(interp, mod_name);
            if (!module) {
                interp_error(interp, expr->line, expr->column,
                             "Unknown module '%s'", mod_name);
                return val_none();
            }

            /* Try to get as a variable from module's environment */
            if (env_has(module->env, sym_name)) {
                return env_get(module->env, sym_name);
            }

            /* It might be a procedure or type - return none for now */
            /* Actual procedure calls are handled in NODE_CALL */
            interp_error(interp, expr->line, expr->column,
                         "Undefined symbol '%s' in module '%s'", sym_name, mod_name);
            return val_none();
        }

        /* === Binary Operations === */

        case NODE_BINARY_OP: {
            int op = expr->data.binop.op;

            /* Short-circuit evaluation for AND/OR */
            if (op == TOK_AND) {
                forge_value_t left = interp_eval_expr(interp, env, expr->data.binop.left);
                if (!val_is_truthy(left)) {
                    val_free(&left);
                    return val_bool(0);
                }
                val_free(&left);
                forge_value_t right = interp_eval_expr(interp, env, expr->data.binop.right);
                return val_bool(val_is_truthy(right));
            }

            if (op == TOK_OR) {
                forge_value_t left = interp_eval_expr(interp, env, expr->data.binop.left);
                if (val_is_truthy(left)) {
                    val_free(&left);
                    return val_bool(1);
                }
                val_free(&left);
                forge_value_t right = interp_eval_expr(interp, env, expr->data.binop.right);
                return val_bool(val_is_truthy(right));
            }

            /* Evaluate both operands */
            forge_value_t left = interp_eval_expr(interp, env, expr->data.binop.left);
            if (interp->had_error) return val_none();

            forge_value_t right = interp_eval_expr(interp, env, expr->data.binop.right);
            if (interp->had_error) {
                val_free(&left);
                return val_none();
            }

            forge_value_t result;

            /* String concatenation */
            if (op == TOK_PLUS && left.kind == VAL_STR) {
                result = eval_string_concat(interp, left, right,
                                            expr->line, expr->column);
            }
            /* Arithmetic */
            else if (op == TOK_PLUS || op == TOK_MINUS || op == TOK_STAR ||
                     op == TOK_SLASH || op == TOK_PERCENT) {
                result = eval_binary_arith(interp, op, left, right,
                                           expr->line, expr->column);
            }
            /* Comparison */
            else if (op == TOK_EQ || op == TOK_NEQ || op == TOK_LT ||
                     op == TOK_GT || op == TOK_LEQ || op == TOK_GEQ) {
                result = eval_binary_compare(interp, op, left, right,
                                             expr->line, expr->column);
            }
            /* Bitwise */
            else if (op == TOK_AMP || op == TOK_PIPE || op == TOK_CARET ||
                     op == TOK_LSHIFT || op == TOK_RSHIFT) {
                result = eval_binary_bitwise(interp, op, left, right,
                                             expr->line, expr->column);
            }
            else {
                interp_error(interp, expr->line, expr->column,
                             "Unknown binary operator");
                result = val_none();
            }

            val_free(&left);
            val_free(&right);
            return result;
        }

        /* === Unary Operations === */

        case NODE_UNARY_OP: {
            forge_value_t operand = interp_eval_expr(interp, env, expr->data.unop.operand);
            if (interp->had_error) return val_none();

            forge_value_t result = eval_unary(interp, expr->data.unop.op, operand,
                                               expr->line, expr->column);
            val_free(&operand);
            return result;
        }

        /* === Array Literal === */

        case NODE_ARRAY_LITERAL: {
            int count = expr->data.array_lit.count;
            forge_value_t arr = val_array(0, count > 0 ? count : 4);

            for (int i = 0; i < count; i++) {
                forge_value_t elem = interp_eval_expr(interp, env, expr->data.array_lit.elements[i]);
                if (interp->had_error) {
                    val_free(&arr);
                    return val_none();
                }
                val_array_push(&arr, elem);
                val_free(&elem);
            }

            return arr;
        }

        /* === Index Access === */

        case NODE_INDEX: {
            /* Optimization: if the object is a simple identifier, use env_get_ptr
             * to avoid deep-copying the entire array/string just to read one element.
             * This is critical for performance with large arrays (e.g., 2M-element sieve). */
            forge_node_t* idx_obj = expr->data.index.object;

            if (idx_obj->kind == NODE_IDENT) {
                const char* var_name = idx_obj->data.name;
                forge_value_t* obj_ptr = env_get_ptr(env, var_name);
                if (!obj_ptr) {
                    interp_error(interp, expr->line, expr->column,
                                 "Undefined variable '%s'", var_name);
                    return val_none();
                }

                forge_value_t idx = interp_eval_expr(interp, env, expr->data.index.index);
                if (interp->had_error) return val_none();

                forge_value_t result;

                if (obj_ptr->kind == VAL_ARRAY) {
                    if (idx.kind != VAL_INT && idx.kind != VAL_UINT && idx.kind != VAL_BYTE) {
                        interp_error(interp, expr->line, expr->column,
                                     "Array index must be integer");
                        result = val_none();
                    } else {
                        int index = (idx.kind == VAL_UINT) ? (int)idx.as.u : (int)idx.as.i;
                        result = val_array_get(obj_ptr, index);
                        if (result.kind == VAL_NONE) {
                            interp_error(interp, expr->line, expr->column,
                                         "Array index %d out of bounds", index);
                        }
                    }
                } else if (obj_ptr->kind == VAL_STR) {
                    if (idx.kind != VAL_INT && idx.kind != VAL_UINT && idx.kind != VAL_BYTE) {
                        interp_error(interp, expr->line, expr->column,
                                     "String index must be integer");
                        result = val_none();
                    } else {
                        int index = (idx.kind == VAL_UINT) ? (int)idx.as.u : (int)idx.as.i;
                        if (index < 0 || index >= obj_ptr->as.str.len) {
                            interp_error(interp, expr->line, expr->column,
                                         "String index %d out of bounds", index);
                            result = val_none();
                        } else {
                            result = val_str_copy(&obj_ptr->as.str.data[index], 1);
                        }
                    }
                } else {
                    interp_error(interp, expr->line, expr->column,
                                 "Cannot index into %s", val_kind_name(obj_ptr->kind));
                    result = val_none();
                }

                val_free(&idx);
                return result;
            }

            /* Fallback: evaluate the object expression (may produce a copy) */
            forge_value_t obj = interp_eval_expr(interp, env, expr->data.index.object);
            if (interp->had_error) return val_none();

            forge_value_t idx = interp_eval_expr(interp, env, expr->data.index.index);
            if (interp->had_error) {
                val_free(&obj);
                return val_none();
            }

            forge_value_t result;

            if (obj.kind == VAL_ARRAY) {
                if (idx.kind != VAL_INT && idx.kind != VAL_UINT && idx.kind != VAL_BYTE) {
                    interp_error(interp, expr->line, expr->column,
                                 "Array index must be integer");
                    result = val_none();
                } else {
                    int index = (idx.kind == VAL_UINT) ? (int)idx.as.u : (int)idx.as.i;
                    result = val_array_get(&obj, index);
                    if (result.kind == VAL_NONE) {
                        interp_error(interp, expr->line, expr->column,
                                     "Array index %d out of bounds", index);
                    }
                }
            }
            else if (obj.kind == VAL_STR) {
                if (idx.kind != VAL_INT && idx.kind != VAL_UINT && idx.kind != VAL_BYTE) {
                    interp_error(interp, expr->line, expr->column,
                                 "String index must be integer");
                    result = val_none();
                } else {
                    int index = (idx.kind == VAL_UINT) ? (int)idx.as.u : (int)idx.as.i;
                    if (index < 0 || index >= obj.as.str.len) {
                        interp_error(interp, expr->line, expr->column,
                                     "String index %d out of bounds", index);
                        result = val_none();
                    } else {
                        /* Return single character as string */
                        result = val_str_copy(&obj.as.str.data[index], 1);
                    }
                }
            }
            else {
                interp_error(interp, expr->line, expr->column,
                             "Cannot index into %s", val_kind_name(obj.kind));
                result = val_none();
            }

            val_free(&obj);
            val_free(&idx);
            return result;
        }

        /* === Record Literal === */

        case NODE_RECORD_LITERAL: {
            int count = expr->data.record_lit.field_count;

            /* Build arrays for field names and values */
            const char** names = forge_malloc(count * sizeof(const char*));
            forge_value_t* values = forge_malloc(count * sizeof(forge_value_t));

            for (int i = 0; i < count; i++) {
                names[i] = expr->data.record_lit.fields[i].name;
                values[i] = interp_eval_expr(interp, env, expr->data.record_lit.fields[i].value);
                if (interp->had_error) {
                    /* Clean up already evaluated values */
                    for (int j = 0; j < i; j++) {
                        val_free(&values[j]);
                    }
                    forge_free(names);
                    forge_free(values);
                    return val_none();
                }
            }

            forge_value_t result = val_record(count, names, values);

            /* val_record copies everything, so free our temp values */
            for (int i = 0; i < count; i++) {
                val_free(&values[i]);
            }
            forge_free(names);
            forge_free(values);

            return result;
        }

        /* === Field Access === */

        case NODE_FIELD_ACCESS: {
            const char* field_name = expr->data.field_access.field_name;
            forge_node_t* inner = expr->data.field_access.object;

            /* Check for forge.math constants (PI, E, TAU) */
            if (inner && inner->kind == NODE_FIELD_ACCESS) {
                forge_node_t* root = inner->data.field_access.object;
                const char* mod = inner->data.field_access.field_name;
                if (root && root->kind == NODE_IDENT &&
                    strcmp(root->data.name, "forge") == 0 &&
                    strcmp(mod, "math") == 0) {
                    if (strcmp(field_name, "PI") == 0) {
                        return val_float(3.14159265358979323846);
                    } else if (strcmp(field_name, "E") == 0) {
                        return val_float(2.71828182845904523536);
                    } else if (strcmp(field_name, "TAU") == 0) {
                        return val_float(6.28318530717958647692);
                    }
                }
            }

            forge_value_t obj = interp_eval_expr(interp, env, inner);
            if (interp->had_error) return val_none();

            /* .value on optional — unwrap the inner value */
            if (strcmp(field_name, "value") == 0 &&
                obj.kind == VAL_OPTIONAL) {
                if (!obj.as.optional.present) {
                    interp_error(interp, expr->line, expr->column,
                                 "Cannot access .value on none optional");
                    val_free(&obj);
                    return val_none();
                }
                forge_value_t result = val_copy(*obj.as.optional.inner);
                val_free(&obj);
                return result;
            }

            if (obj.kind != VAL_RECORD) {
                interp_error(interp, expr->line, expr->column,
                             "Cannot access field '%s' on non-record type",
                             field_name);
                val_free(&obj);
                return val_none();
            }

            /* Look up field in record */
            for (int i = 0; i < obj.as.record.count; i++) {
                if (strcmp(obj.as.record.names[i], field_name) == 0) {
                    forge_value_t result = val_copy(obj.as.record.fields[i]);
                    val_free(&obj);
                    return result;
                }
            }

            interp_error(interp, expr->line, expr->column,
                         "Record has no field '%s'", field_name);
            val_free(&obj);
            return val_none();
        }

        /* === Optional Operations === */

        case NODE_SOME: {
            /* some(expr) - wrap value in optional */
            forge_value_t inner = interp_eval_expr(interp, env, expr->data.some.expr);
            if (interp->had_error) return val_none();

            forge_value_t result = val_some(inner);
            val_free(&inner);
            return result;
        }

        case NODE_IS_SOME: {
            /* expr is some - check if optional has value */
            forge_value_t val = interp_eval_expr(interp, env, expr->data.is_check.expr);
            if (interp->had_error) return val_none();

            int is_some = (val.kind == VAL_OPTIONAL && val.as.optional.present);
            val_free(&val);
            return val_bool(is_some);
        }

        case NODE_IS_NONE: {
            /* expr is none - check if optional is empty */
            forge_value_t val = interp_eval_expr(interp, env, expr->data.is_check.expr);
            if (interp->had_error) return val_none();

            int is_none = (val.kind == VAL_NONE ||
                          (val.kind == VAL_OPTIONAL && !val.as.optional.present));
            val_free(&val);
            return val_bool(is_none);
        }

        case NODE_OR_ELSE: {
            /* expr or_else default - unwrap optional or use default */
            forge_value_t val = interp_eval_expr(interp, env, expr->data.or_else.optional_expr);
            if (interp->had_error) return val_none();

            /* If it's a some() optional, unwrap it */
            if (val.kind == VAL_OPTIONAL && val.as.optional.present) {
                forge_value_t result = val_copy(*val.as.optional.inner);
                val_free(&val);
                return result;
            }

            /* If it's none or VAL_NONE, use the default */
            if (val.kind == VAL_NONE ||
                (val.kind == VAL_OPTIONAL && !val.as.optional.present)) {
                val_free(&val);
                return interp_eval_expr(interp, env, expr->data.or_else.fallback);
            }

            /* Otherwise return the value as-is */
            return val;
        }

        /* === Call Expression === */

        case NODE_CALL: {
            forge_node_t* callee = expr->data.call.callee;

            /* Evaluate arguments */
            int arg_count = expr->data.call.arg_count;
            forge_value_t* args = NULL;
            if (arg_count > 0) {
                args = forge_malloc(arg_count * sizeof(forge_value_t));
                for (int i = 0; i < arg_count; i++) {
                    args[i] = interp_eval_expr(interp, env, expr->data.call.args[i]);
                    if (interp->had_error) {
                        for (int j = 0; j < i; j++) {
                            val_free(&args[j]);
                        }
                        forge_free(args);
                        return val_none();
                    }
                }
            }

            forge_value_t result;

            if (callee->kind == NODE_IDENT) {
                /* Simple procedure call by name */
                result = interp_call_proc(interp, env, callee->data.name, args, arg_count);
            }
            else if (callee->kind == NODE_QUALIFIED_IDENT) {
                /* Module-qualified procedure call: module.proc() */
                const char* mod_name = callee->data.qualified.module_name;
                const char* proc_name = callee->data.qualified.symbol_name;

                forge_module_t* module = interp_get_module(interp, mod_name);
                if (!module) {
                    interp_error(interp, expr->line, expr->column,
                                 "Unknown module '%s'", mod_name);
                    for (int i = 0; i < arg_count; i++) val_free(&args[i]);
                    forge_free(args);
                    return val_none();
                }

                /* Check if it's a stdlib module - dispatch to builtins */
                if (module->is_stdlib) {
                    forge_value_t stdlib_result;
                    int handled = 0;
                    if (strcmp(mod_name, "forge.io") == 0) {
                        handled = try_stdlib_io(interp, proc_name, args, arg_count, &stdlib_result);
                    } else if (strcmp(mod_name, "forge.str") == 0) {
                        handled = try_stdlib_str(interp, proc_name, args, arg_count, &stdlib_result);
                    } else if (strcmp(mod_name, "forge.math") == 0) {
                        handled = try_stdlib_math(interp, proc_name, args, arg_count, &stdlib_result);
                    } else if (strcmp(mod_name, "forge.sys") == 0) {
                        handled = try_stdlib_sys(interp, proc_name, args, arg_count, &stdlib_result);
                    } else if (strcmp(mod_name, "forge.time") == 0) {
                        handled = try_stdlib_time(interp, proc_name, args, arg_count, &stdlib_result);
                    } else if (strcmp(mod_name, "forge.buf") == 0) {
                        handled = try_stdlib_buf(interp, proc_name, args, arg_count, &stdlib_result);
                    } else if (strcmp(mod_name, "forge.serial") == 0) {
                        handled = try_stdlib_serial(interp, proc_name, args, arg_count, &stdlib_result);
                    } else if (strcmp(mod_name, "forge.nmea") == 0) {
                        handled = try_stdlib_nmea(interp, proc_name, args, arg_count, &stdlib_result);
                    }
                    if (handled) {
                        result = stdlib_result;
                    } else {
                        interp_error(interp, expr->line, expr->column,
                                     "Undefined procedure '%s' in stdlib module '%s'",
                                     proc_name, mod_name);
                        for (int i = 0; i < arg_count; i++) val_free(&args[i]);
                        forge_free(args);
                        return val_none();
                    }
                } else {
                    /* Look up procedure in module */
                    forge_node_t* proc = interp_module_get_proc(module, proc_name);
                    if (!proc) {
                        interp_error(interp, expr->line, expr->column,
                                     "Undefined procedure '%s' in module '%s'",
                                     proc_name, mod_name);
                        for (int i = 0; i < arg_count; i++) val_free(&args[i]);
                        forge_free(args);
                        return val_none();
                    }

                    /* Call the procedure in the module's environment */
                    result = interp_call_module_proc(interp, module, proc_name, args, arg_count);
                }
            }
            else if (callee->kind == NODE_FIELD_ACCESS) {
                /* Handle forge.io.func() pattern (chained field access) */
                /* Pattern: NODE_FIELD_ACCESS(NODE_FIELD_ACCESS(ident, sub), func) */
                forge_node_t* inner = callee->data.field_access.object;
                const char* func_name = callee->data.field_access.field_name;

                /* Check for forge.*.func() pattern */
                if (inner->kind == NODE_FIELD_ACCESS) {
                    forge_node_t* root = inner->data.field_access.object;
                    const char* sub_name = inner->data.field_access.field_name;

                    if (root->kind == NODE_IDENT &&
                        strcmp(root->data.name, "forge") == 0) {
                        /* Construct module name: forge.{sub_name} */
                        char module_name[64];
                        snprintf(module_name, sizeof(module_name), "forge.%s", sub_name);

                        forge_module_t* module = interp_get_module(interp, module_name);
                        if (module && module->is_stdlib) {
                            forge_value_t stdlib_result;
                            int handled = 0;
                            if (strcmp(sub_name, "io") == 0) {
                                handled = try_stdlib_io(interp, func_name, args, arg_count, &stdlib_result);
                            } else if (strcmp(sub_name, "str") == 0) {
                                handled = try_stdlib_str(interp, func_name, args, arg_count, &stdlib_result);
                            } else if (strcmp(sub_name, "math") == 0) {
                                handled = try_stdlib_math(interp, func_name, args, arg_count, &stdlib_result);
                            } else if (strcmp(sub_name, "sys") == 0) {
                                handled = try_stdlib_sys(interp, func_name, args, arg_count, &stdlib_result);
                            } else if (strcmp(sub_name, "time") == 0) {
                                handled = try_stdlib_time(interp, func_name, args, arg_count, &stdlib_result);
                            } else if (strcmp(sub_name, "buf") == 0) {
                                handled = try_stdlib_buf(interp, func_name, args, arg_count, &stdlib_result);
                            } else if (strcmp(sub_name, "serial") == 0) {
                                handled = try_stdlib_serial(interp, func_name, args, arg_count, &stdlib_result);
                            } else if (strcmp(sub_name, "nmea") == 0) {
                                handled = try_stdlib_nmea(interp, func_name, args, arg_count, &stdlib_result);
                            }
                            if (handled) {
                                result = stdlib_result;
                            } else {
                                interp_error(interp, expr->line, expr->column,
                                             "Undefined procedure '%s' in stdlib module '%s'",
                                             func_name, module_name);
                                for (int i = 0; i < arg_count; i++) val_free(&args[i]);
                                forge_free(args);
                                return val_none();
                            }
                        } else {
                            interp_error(interp, expr->line, expr->column,
                                         "Module '%s' not imported", module_name);
                            for (int i = 0; i < arg_count; i++) val_free(&args[i]);
                            forge_free(args);
                            return val_none();
                        }
                    } else {
                        interp_error(interp, expr->line, expr->column,
                                     "Cannot call field access as function");
                        for (int i = 0; i < arg_count; i++) val_free(&args[i]);
                        forge_free(args);
                        return val_none();
                    }
                } else {
                    interp_error(interp, expr->line, expr->column,
                                 "Cannot call field access as function");
                    for (int i = 0; i < arg_count; i++) val_free(&args[i]);
                    forge_free(args);
                    return val_none();
                }
            }
            else {
                interp_error(interp, expr->line, expr->column,
                             "Cannot call non-identifier callee (not yet supported)");
                for (int i = 0; i < arg_count; i++) val_free(&args[i]);
                forge_free(args);
                return val_none();
            }

            /* Free arguments */
            for (int i = 0; i < arg_count; i++) {
                val_free(&args[i]);
            }
            forge_free(args);

            return result;
        }

        /* === Cast Expression === */

        case NODE_CAST: {
            forge_node_t* target_type = expr->data.cast.target_type;
            forge_node_t* inner = expr->data.cast.expr;
            forge_value_t val = interp_eval_expr(interp, env, inner);
            if (interp->had_error) return val_none();

            /* Get target type name */
            const char* type_name = NULL;
            if (target_type->kind == NODE_TYPE_PRIM) {
                type_name = target_type->data.name;
            } else if (target_type->kind == NODE_TYPE_NAMED) {
                type_name = target_type->data.name;
            }

            if (!type_name) {
                interp_error(interp, expr->line, expr->column,
                             "Invalid cast target type");
                val_free(&val);
                return val_none();
            }

            forge_value_t result;

            /* int() - convert to integer */
            if (strcmp(type_name, "int") == 0) {
                switch (val.kind) {
                    case VAL_INT: result = val; break;
                    case VAL_FLOAT: result = val_int((int64_t)val.as.f); val_free(&val); break;
                    case VAL_BOOL: result = val_int(val.as.b ? 1 : 0); val_free(&val); break;
                    case VAL_STR: {
                        int64_t parsed = strtoll(val.as.str.data, NULL, 10);
                        result = val_int(parsed);
                        val_free(&val);
                        break;
                    }
                    default:
                        interp_error(interp, expr->line, expr->column,
                                     "Cannot convert %s to int", val_kind_name(val.kind));
                        val_free(&val);
                        return val_none();
                }
            }
            /* float() - convert to float */
            else if (strcmp(type_name, "float") == 0) {
                switch (val.kind) {
                    case VAL_FLOAT: result = val; break;
                    case VAL_INT: result = val_float((double)val.as.i); val_free(&val); break;
                    case VAL_BOOL: result = val_float(val.as.b ? 1.0 : 0.0); val_free(&val); break;
                    case VAL_STR: {
                        double parsed = strtod(val.as.str.data, NULL);
                        result = val_float(parsed);
                        val_free(&val);
                        break;
                    }
                    default:
                        interp_error(interp, expr->line, expr->column,
                                     "Cannot convert %s to float", val_kind_name(val.kind));
                        val_free(&val);
                        return val_none();
                }
            }
            /* str() - convert to string */
            else if (strcmp(type_name, "str") == 0) {
                char* s = val_to_str(val);
                result = val_str_copy(s, strlen(s));
                forge_free(s);
                val_free(&val);
            }
            /* bool() - convert to bool */
            else if (strcmp(type_name, "bool") == 0) {
                int truthy = val_is_truthy(val);
                val_free(&val);
                result = val_bool(truthy);
            }
            /* byte/uint8() */
            else if (strcmp(type_name, "byte") == 0 || strcmp(type_name, "uint8") == 0) {
                switch (val.kind) {
                    case VAL_INT: result = val_byte((uint8_t)val.as.i); val_free(&val); break;
                    case VAL_BYTE: result = val; break;
                    default:
                        interp_error(interp, expr->line, expr->column,
                                     "Cannot convert %s to byte", val_kind_name(val.kind));
                        val_free(&val);
                        return val_none();
                }
            }
            else {
                interp_error(interp, expr->line, expr->column,
                             "Unknown cast target type '%s'", type_name);
                val_free(&val);
                return val_none();
            }

            return result;
        }

        default:
            interp_error(interp, expr->line, expr->column,
                         "Cannot evaluate node kind %s", ast_node_kind_name(expr->kind));
            return val_none();
    }
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Statement Execution
 * ───────────────────────────────────────────────────────────────────────────── */

/* Helper: Create a normal result with no value */
static forge_result_t result_normal(void) {
    forge_result_t r = { FLOW_NORMAL, val_void() };
    return r;
}

/* Helper: Create a result with flow control */
static forge_result_t result_flow(forge_flow_t flow, forge_value_t val) {
    forge_result_t r = { flow, val };
    return r;
}

forge_result_t interp_exec_block(forge_interp_t* interp, forge_env_t* env,
                                  forge_node_t* block) {
    if (!block || block->kind != NODE_BLOCK) {
        interp_error(interp, 0, 0, "Expected block node");
        return result_normal();
    }

    int count = block->data.block.count;
    forge_node_t** stmts = block->data.block.stmts;

    for (int i = 0; i < count; i++) {
        if (interp->had_error) return result_normal();

        forge_result_t res = interp_exec_stmt(interp, env, stmts[i]);

        /* Propagate control flow */
        if (res.flow != FLOW_NORMAL) {
            return res;
        }

        /* Free the result value if not used */
        val_free(&res.value);
    }

    return result_normal();
}

forge_result_t interp_exec_stmt(forge_interp_t* interp, forge_env_t* env,
                                 forge_node_t* stmt) {
    if (!stmt) return result_normal();
    if (interp->had_error) return result_normal();

    switch (stmt->kind) {

        /* === Variable/Constant Declaration === */

        case NODE_VAR_DECL:
        case NODE_CONST_DECL: {
            const char* name = stmt->data.var_decl.name;
            forge_node_t* init = stmt->data.var_decl.init_expr;

            forge_value_t val;
            if (init) {
                val = interp_eval_expr(interp, env, init);
                if (interp->had_error) return result_normal();
            } else {
                /* Uninitialized variable - default to none */
                val = val_none();
            }

            env_define(env, name, val);
            val_free(&val);
            return result_normal();
        }

        /* === Assignment === */

        case NODE_ASSIGN: {
            forge_node_t* target = stmt->data.assign.target;
            forge_node_t* value_node = stmt->data.assign.value;

            forge_value_t new_val = interp_eval_expr(interp, env, value_node);
            if (interp->had_error) return result_normal();

            /* Handle different assignment targets */
            if (target->kind == NODE_IDENT) {
                /* Simple variable assignment */
                const char* name = target->data.name;
                if (!env_update(env, name, new_val)) {
                    interp_error(interp, target->line, target->column,
                                 "Cannot assign to undefined variable '%s'", name);
                }
                val_free(&new_val);
            } else if (target->kind == NODE_INDEX) {
                /* Array/string index assignment: arr[i] = val */
                /* We need to modify the array in place, so get a pointer to it */
                forge_node_t* obj = target->data.index.object;

                if (obj->kind != NODE_IDENT) {
                    interp_error(interp, target->line, target->column,
                                 "Index assignment target must be a variable");
                    val_free(&new_val);
                    return result_normal();
                }

                const char* var_name = obj->data.name;
                forge_value_t* container_ptr = env_get_ptr(env, var_name);

                if (!container_ptr) {
                    interp_error(interp, target->line, target->column,
                                 "Undefined variable '%s'", var_name);
                    val_free(&new_val);
                    return result_normal();
                }

                if (container_ptr->kind != VAL_ARRAY) {
                    interp_error(interp, target->line, target->column,
                                 "Cannot index-assign to non-array type");
                    val_free(&new_val);
                    return result_normal();
                }

                forge_value_t index = interp_eval_expr(interp, env, target->data.index.index);
                if (interp->had_error) { val_free(&new_val); return result_normal(); }

                if (index.kind != VAL_INT) {
                    interp_error(interp, target->line, target->column,
                                 "Array index must be integer");
                    val_free(&index);
                    val_free(&new_val);
                    return result_normal();
                }

                int idx = (int)index.as.i;
                if (idx < 0 || idx >= container_ptr->as.array.len) {
                    interp_error(interp, target->line, target->column,
                                 "Array index %d out of bounds (length %d)",
                                 idx, container_ptr->as.array.len);
                    val_free(&index);
                    val_free(&new_val);
                    return result_normal();
                }

                /* Set the element in the original array */
                val_free(&container_ptr->as.array.elems[idx]);
                container_ptr->as.array.elems[idx] = new_val;

                val_free(&index);
            } else if (target->kind == NODE_FIELD_ACCESS) {
                /* Record field assignment: rec.field = val */
                /* For now, simplified: would need reference semantics */
                interp_error(interp, target->line, target->column,
                             "Field assignment not yet fully supported");
                val_free(&new_val);
            } else {
                interp_error(interp, target->line, target->column,
                             "Invalid assignment target");
                val_free(&new_val);
            }

            return result_normal();
        }

        /* === Compound Assignment (x += y, etc.) === */

        case NODE_COMPOUND_ASSIGN: {
            forge_node_t* target = stmt->data.compound_assign.target;
            forge_node_t* value_node = stmt->data.compound_assign.value;
            int op = stmt->data.compound_assign.op;

            /* Only support simple identifier targets for now */
            if (target->kind != NODE_IDENT) {
                interp_error(interp, target->line, target->column,
                             "Compound assignment requires simple variable target");
                return result_normal();
            }

            const char* name = target->data.name;
            forge_value_t current = env_get(env, name);
            if (current.kind == VAL_NONE && !env_has(env, name)) {
                interp_error(interp, target->line, target->column,
                             "Undefined variable '%s'", name);
                return result_normal();
            }

            forge_value_t rhs = interp_eval_expr(interp, env, value_node);
            if (interp->had_error) { val_free(&current); return result_normal(); }

            /* Map compound operator to binary operator */
            int binop;
            switch (op) {
                case TOK_PLUS_EQ:  binop = TOK_PLUS; break;
                case TOK_MINUS_EQ: binop = TOK_MINUS; break;
                case TOK_STAR_EQ:  binop = TOK_STAR; break;
                case TOK_SLASH_EQ: binop = TOK_SLASH; break;
                case TOK_PERCENT_EQ: binop = TOK_PERCENT; break;
                default:
                    interp_error(interp, stmt->line, stmt->column,
                                 "Unknown compound assignment operator");
                    val_free(&current);
                    val_free(&rhs);
                    return result_normal();
            }

            /* Perform the binary operation */
            forge_value_t new_val = eval_binary_arith(interp, binop, current, rhs,
                                                       stmt->line, stmt->column);
            val_free(&current);
            val_free(&rhs);

            if (interp->had_error) return result_normal();

            env_update(env, name, new_val);
            val_free(&new_val);

            return result_normal();
        }

        /* === Expression Statement === */

        case NODE_EXPR_STMT: {
            forge_value_t val = interp_eval_expr(interp, env, stmt->data.expr_stmt.expr);
            val_free(&val);
            return result_normal();
        }

        /* === Block Statement === */

        case NODE_BLOCK: {
            /* Create new scope for block */
            forge_env_t* block_env = env_create(env);
            forge_result_t res = interp_exec_block(interp, block_env, stmt);
            env_destroy(block_env);
            return res;
        }

        /* === If Statement === */

        case NODE_IF: {
            forge_value_t cond = interp_eval_expr(interp, env, stmt->data.if_stmt.condition);
            if (interp->had_error) return result_normal();

            int cond_true = val_is_truthy(cond);
            val_free(&cond);

            if (cond_true) {
                forge_env_t* then_env = env_create(env);
                forge_result_t res = interp_exec_block(interp, then_env, stmt->data.if_stmt.then_body);
                env_destroy(then_env);
                return res;
            }

            /* Check elif branches */
            for (int i = 0; i < stmt->data.if_stmt.elif_count; i++) {
                forge_value_t elif_cond = interp_eval_expr(interp, env,
                                                           stmt->data.if_stmt.elif_conditions[i]);
                if (interp->had_error) return result_normal();

                int elif_true = val_is_truthy(elif_cond);
                val_free(&elif_cond);

                if (elif_true) {
                    forge_env_t* elif_env = env_create(env);
                    forge_result_t res = interp_exec_block(interp, elif_env,
                                                           stmt->data.if_stmt.elif_bodies[i]);
                    env_destroy(elif_env);
                    return res;
                }
            }

            /* Check else branch */
            if (stmt->data.if_stmt.else_body) {
                forge_env_t* else_env = env_create(env);
                forge_result_t res = interp_exec_block(interp, else_env, stmt->data.if_stmt.else_body);
                env_destroy(else_env);
                return res;
            }

            return result_normal();
        }

        /* === While Loop === */

        case NODE_WHILE: {
            while (!interp->had_error) {
                forge_value_t cond = interp_eval_expr(interp, env, stmt->data.while_stmt.condition);
                if (interp->had_error) return result_normal();

                int cond_true = val_is_truthy(cond);
                val_free(&cond);

                if (!cond_true) break;

                forge_env_t* loop_env = env_create(env);
                forge_result_t res = interp_exec_block(interp, loop_env, stmt->data.while_stmt.body);
                env_destroy(loop_env);

                if (res.flow == FLOW_BREAK) {
                    val_free(&res.value);
                    break;
                }
                if (res.flow == FLOW_CONTINUE) {
                    val_free(&res.value);
                    continue;
                }
                if (res.flow == FLOW_RETURN) {
                    return res;
                }
                val_free(&res.value);
            }
            return result_normal();
        }

        /* === For Loop === */

        case NODE_FOR: {
            const char* var_name = stmt->data.for_stmt.var_name;
            forge_node_t* iterable = stmt->data.for_stmt.iterable;

            /* Handle range iteration - check node kind first, don't try to evaluate range */
            if (iterable->kind == NODE_RANGE) {
                forge_value_t start = interp_eval_expr(interp, env, iterable->data.range.start);
                if (interp->had_error) return result_normal();
                forge_value_t end = interp_eval_expr(interp, env, iterable->data.range.end);
                if (interp->had_error) { val_free(&start); return result_normal(); }

                if (start.kind != VAL_INT || end.kind != VAL_INT) {
                    interp_error(interp, iterable->line, iterable->column,
                                 "Range bounds must be integers");
                    val_free(&start);
                    val_free(&end);
                    return result_normal();
                }

                long long s = start.as.i;
                long long e = end.as.i;
                int inclusive = iterable->data.range.inclusive;
                val_free(&start);
                val_free(&end);

                for (long long i = s; inclusive ? (i <= e) : (i < e); i++) {
                    if (interp->had_error) break;

                    forge_env_t* loop_env = env_create(env);
                    env_define(loop_env, var_name, val_int(i));

                    forge_result_t res = interp_exec_block(interp, loop_env, stmt->data.for_stmt.body);
                    env_destroy(loop_env);

                    if (res.flow == FLOW_BREAK) {
                        val_free(&res.value);
                        break;
                    }
                    if (res.flow == FLOW_CONTINUE) {
                        val_free(&res.value);
                        continue;
                    }
                    if (res.flow == FLOW_RETURN) {
                        return res;
                    }
                    val_free(&res.value);
                }
                return result_normal();
            }

            /* For non-range iterables, evaluate the expression */
            forge_value_t iter_val = interp_eval_expr(interp, env, iterable);
            if (interp->had_error) return result_normal();

            /* Handle array iteration */
            if (iter_val.kind == VAL_ARRAY) {
                for (int i = 0; i < iter_val.as.array.len; i++) {
                    if (interp->had_error) break;

                    forge_env_t* loop_env = env_create(env);
                    forge_value_t elem = val_copy(iter_val.as.array.elems[i]);
                    env_define(loop_env, var_name, elem);
                    val_free(&elem);

                    forge_result_t res = interp_exec_block(interp, loop_env, stmt->data.for_stmt.body);
                    env_destroy(loop_env);

                    if (res.flow == FLOW_BREAK) {
                        val_free(&res.value);
                        break;
                    }
                    if (res.flow == FLOW_CONTINUE) {
                        val_free(&res.value);
                        continue;
                    }
                    if (res.flow == FLOW_RETURN) {
                        val_free(&iter_val);
                        return res;
                    }
                    val_free(&res.value);
                }
                val_free(&iter_val);
            } else {
                interp_error(interp, iterable->line, iterable->column,
                             "Cannot iterate over non-iterable type");
                val_free(&iter_val);
                return result_normal();
            }

            return result_normal();
        }

        /* === Infinite Loop === */

        case NODE_LOOP: {
            while (!interp->had_error) {
                forge_env_t* loop_env = env_create(env);
                forge_result_t res = interp_exec_block(interp, loop_env, stmt->data.loop_stmt.body);
                env_destroy(loop_env);

                if (res.flow == FLOW_BREAK) {
                    val_free(&res.value);
                    break;
                }
                if (res.flow == FLOW_CONTINUE) {
                    val_free(&res.value);
                    continue;
                }
                if (res.flow == FLOW_RETURN) {
                    return res;
                }
                val_free(&res.value);
            }
            return result_normal();
        }

        /* === Return Statement === */

        case NODE_RETURN: {
            forge_value_t val;
            if (stmt->data.return_stmt.value) {
                val = interp_eval_expr(interp, env, stmt->data.return_stmt.value);
                if (interp->had_error) return result_normal();
            } else {
                val = val_void();
            }
            return result_flow(FLOW_RETURN, val);
        }

        /* === Break Statement === */

        case NODE_BREAK: {
            return result_flow(FLOW_BREAK, val_void());
        }

        /* === Continue Statement === */

        case NODE_CONTINUE: {
            return result_flow(FLOW_CONTINUE, val_void());
        }

        /* === Emit Statement === */

        case NODE_EMIT: {
            const char* channel_name = stmt->data.emit_stmt.channel_name;
            forge_node_t* payload_expr = stmt->data.emit_stmt.payload;

            forge_value_t payload;
            forge_value_t* payload_ptr = NULL;

            if (payload_expr) {
                payload = interp_eval_expr(interp, env, payload_expr);
                if (interp->had_error) return result_normal();
                payload_ptr = &payload;
            }

            interp_emit(interp, env, channel_name, payload_ptr);

            if (payload_ptr) {
                val_free(&payload);
            }

            return result_normal();
        }

        /* === Assert Statement === */

        case NODE_ASSERT: {
            forge_value_t cond = interp_eval_expr(interp, env, stmt->data.panic_assert.expr);
            if (interp->had_error) return result_normal();

            if (!val_is_truthy(cond)) {
                interp_error(interp, stmt->line, stmt->column, "Assertion failed");
            }
            val_free(&cond);
            return result_normal();
        }

        /* === Panic Statement === */

        case NODE_PANIC: {
            forge_value_t msg = interp_eval_expr(interp, env, stmt->data.panic_assert.expr);
            if (interp->had_error) return result_normal();

            char* str = val_to_str(msg);
            interp_error(interp, stmt->line, stmt->column, "Panic: %s", str);
            forge_free(str);
            val_free(&msg);
            return result_normal();
        }

        /* === Free Statement === */

        case NODE_FREE: {
            /* For now, just evaluate and discard - actual memory management TBD */
            forge_value_t val = interp_eval_expr(interp, env, stmt->data.free_stmt.expr);
            val_free(&val);
            return result_normal();
        }

        /* === With Allocation === */

        case NODE_WITH_ALLOC: {
            const char* var_name = stmt->data.with_alloc.var_name;

            /* Evaluate size expression if present */
            int count = 0;
            if (stmt->data.with_alloc.size_expr) {
                forge_value_t size_val = interp_eval_expr(interp, env, stmt->data.with_alloc.size_expr);
                if (interp->had_error) return result_normal();
                count = (int)size_val.as.i;
                val_free(&size_val);
            }

            /* Create an array of the requested size (zero-initialized) */
            forge_value_t alloc_val;
            alloc_val.kind = VAL_ARRAY;
            alloc_val.as.array.elems = NULL;
            alloc_val.as.array.len = count;
            alloc_val.as.array.cap = count;
            if (count > 0) {
                alloc_val.as.array.elems = forge_malloc(count * sizeof(forge_value_t));
                for (int i = 0; i < count; i++) {
                    alloc_val.as.array.elems[i] = val_int(0);
                }
            }

            /* Bind the variable in a new scope */
            forge_env_t* inner_env = env_create(env);
            env_define(inner_env, var_name, alloc_val);

            /* Execute the body */
            forge_result_t res = interp_exec_block(interp, inner_env, stmt->data.with_alloc.body);

            /* Free the allocated value */
            forge_value_t to_free = env_get(inner_env, var_name);
            val_free(&to_free);

            env_destroy(inner_env);
            return res;
        }

        /* === Procedure Declaration === */

        case NODE_PROC_DECL: {
            const char* name = stmt->data.proc.name;
            /* Store the procedure node in the procedures hashmap */
            hashmap_set(interp->procedures, name, stmt);
            return result_normal();
        }

        /* === Record Declaration === */

        case NODE_RECORD_DECL: {
            const char* name = stmt->data.record.name;
            /* Store the record definition in the records hashmap */
            hashmap_set(interp->records, name, stmt);
            return result_normal();
        }

        default:
            interp_error(interp, stmt->line, stmt->column,
                         "Unknown statement type: %d", stmt->kind);
            return result_normal();
    }
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Procedure Calls
 * ───────────────────────────────────────────────────────────────────────────── */

/* Call a procedure from a specific module */
static forge_value_t interp_call_module_proc(forge_interp_t* interp,
                                              forge_module_t* module,
                                              const char* name,
                                              forge_value_t* args, int arg_count) {
    /* Look up the procedure in the module */
    forge_node_t* proc = (forge_node_t*)hashmap_get(module->procedures, name);
    if (!proc) {
        interp_error(interp, 0, 0, "Undefined procedure '%s' in module '%s'",
                     name, module->name);
        return val_none();
    }

    /* Check argument count */
    int param_count = proc->data.proc.param_count;
    if (arg_count != param_count) {
        interp_error(interp, proc->line, proc->column,
                     "Procedure '%s.%s' expects %d arguments, got %d",
                     module->name, name, param_count, arg_count);
        return val_none();
    }

    /* Check call depth */
    if (interp->call_depth >= INTERP_MAX_CALL_DEPTH) {
        interp_error(interp, 0, 0, "Maximum call depth exceeded (stack overflow)");
        return val_none();
    }

    /* Push call frame */
    interp->call_stack[interp->call_depth].proc_name = name;
    interp->call_stack[interp->call_depth].filename = module->filepath;
    interp->call_stack[interp->call_depth].line = proc->line;
    interp->call_depth++;

    /* Create new environment with module's env as parent */
    forge_env_t* proc_env = env_create(module->env);

    /* Bind parameters to argument values */
    forge_param_t* params = proc->data.proc.params;
    for (int i = 0; i < param_count; i++) {
        forge_value_t arg_copy = val_copy(args[i]);
        env_define(proc_env, params[i].name, arg_copy);
        val_free(&arg_copy);
    }

    /* Save and set current module context */
    forge_module_t* prev_module = interp->current_module;
    interp->current_module = module;

    /* Execute the procedure body */
    forge_result_t result = interp_exec_block(interp, proc_env, proc->data.proc.body);

    /* Restore previous module context */
    interp->current_module = prev_module;

    /* Clean up environment */
    env_destroy(proc_env);

    /* Pop call frame */
    interp->call_depth--;

    /* Handle return value */
    if (result.flow == FLOW_RETURN) {
        return result.value;
    }

    /* If no explicit return, return void */
    val_free(&result.value);
    return val_void();
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Builtin Functions
 * ───────────────────────────────────────────────────────────────────────────── */

/* print(args...) - print values to stdout with spaces, then newline */
static forge_value_t builtin_print(forge_value_t* args, int arg_count) {
    for (int i = 0; i < arg_count; i++) {
        char* str = val_to_str(args[i]);
        printf("%s", str);
        forge_free(str);
        if (i < arg_count - 1) printf(" ");
    }
    printf("\n");
    return val_void();
}

/* str(val) - convert any value to string */
static forge_value_t builtin_str(forge_interp_t* interp, forge_value_t* args, int arg_count) {
    if (arg_count != 1) {
        interp_error(interp, 0, 0, "str() takes exactly 1 argument");
        return val_none();
    }
    char* s = val_to_str(args[0]);
    forge_value_t result = val_str_copy(s, strlen(s));
    forge_free(s);
    return result;
}

/* len(arr|str) - return length of array or string */
static forge_value_t builtin_len(forge_interp_t* interp, forge_value_t* args, int arg_count) {
    if (arg_count != 1) {
        interp_error(interp, 0, 0, "len() takes exactly 1 argument");
        return val_none();
    }
    if (args[0].kind == VAL_ARRAY) {
        return val_int(args[0].as.array.len);
    }
    if (args[0].kind == VAL_STR) {
        return val_int(args[0].as.str.len);
    }
    interp_error(interp, 0, 0, "len() requires array or string argument");
    return val_none();
}

/* append(arr, val) - append value to array (returns new array with appended element) */
static forge_value_t builtin_append(forge_interp_t* interp, forge_value_t* args, int arg_count) {
    if (arg_count != 2) {
        interp_error(interp, 0, 0, "append() takes exactly 2 arguments");
        return val_none();
    }
    if (args[0].kind != VAL_ARRAY) {
        interp_error(interp, 0, 0, "append() requires array as first argument");
        return val_none();
    }

    /* Create new array with extra space */
    int old_len = args[0].as.array.len;
    int new_len = old_len + 1;
    forge_value_t* new_elems = forge_malloc(new_len * sizeof(forge_value_t));

    /* Copy old elements */
    for (int i = 0; i < old_len; i++) {
        new_elems[i] = val_copy(args[0].as.array.elems[i]);
    }
    /* Add new element */
    new_elems[old_len] = val_copy(args[1]);

    /* Use val_array_from to properly construct the result */
    forge_value_t result = val_array_from(new_elems, new_len);

    /* Free the temporary array (val_array_from copies) */
    for (int i = 0; i < new_len; i++) {
        val_free(&new_elems[i]);
    }
    forge_free(new_elems);

    return result;
}

/* type(val) - return type name as string */
static forge_value_t builtin_type(forge_interp_t* interp, forge_value_t* args, int arg_count) {
    if (arg_count != 1) {
        interp_error(interp, 0, 0, "type() takes exactly 1 argument");
        return val_none();
    }
    const char* type_name = val_kind_name(args[0].kind);
    return val_str_copy(type_name, strlen(type_name));
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Standard Library Builtins (forge.* namespace)
 * ───────────────────────────────────────────────────────────────────────────── */

/* forge.io.print - print with newline */
static forge_value_t stdlib_io_print(forge_value_t* args, int arg_count) {
    for (int i = 0; i < arg_count; i++) {
        char* str = val_to_str(args[i]);
        printf("%s", str);
        forge_free(str);
        if (i < arg_count - 1) printf(" ");
    }
    printf("\n");
    return val_void();
}

/* forge.io.print_raw - print without newline */
static forge_value_t stdlib_io_print_raw(forge_value_t* args, int arg_count) {
    for (int i = 0; i < arg_count; i++) {
        char* str = val_to_str(args[i]);
        printf("%s", str);
        forge_free(str);
        if (i < arg_count - 1) printf(" ");
    }
    fflush(stdout);
    return val_void();
}

/* forge.io.eprint - print to stderr */
static forge_value_t stdlib_io_eprint(forge_value_t* args, int arg_count) {
    for (int i = 0; i < arg_count; i++) {
        char* str = val_to_str(args[i]);
        fprintf(stderr, "%s", str);
        forge_free(str);
        if (i < arg_count - 1) fprintf(stderr, " ");
    }
    fprintf(stderr, "\n");
    return val_void();
}

/* forge.io.read_line - read line from stdin */
static forge_value_t stdlib_io_read_line(forge_interp_t* interp,
                                          forge_value_t* args, int arg_count) {
    (void)args;
    if (arg_count != 0) {
        interp_error(interp, 0, 0, "forge.io.read_line() takes no arguments");
        return val_none();
    }

    char* line = NULL;
    size_t cap = 0;
    ssize_t len = getline(&line, &cap, stdin);

    if (len < 0) {
        free(line);
        return val_str_lit("");
    }

    /* Strip trailing newline */
    if (len > 0 && line[len - 1] == '\n') {
        line[len - 1] = '\0';
        len--;
    }

    forge_value_t result = val_str_copy(line, (int)len);
    free(line);
    return result;
}

/* forge.io.read_line_prompt - print prompt then read line */
static forge_value_t stdlib_io_read_line_prompt(forge_interp_t* interp,
                                                 forge_value_t* args, int arg_count) {
    if (arg_count != 1) {
        interp_error(interp, 0, 0, "forge.io.read_line_prompt() takes 1 argument");
        return val_none();
    }
    if (args[0].kind != VAL_STR) {
        interp_error(interp, 0, 0, "forge.io.read_line_prompt() argument must be string");
        return val_none();
    }

    /* Print prompt */
    fwrite(args[0].as.str.data, 1, args[0].as.str.len, stdout);
    fflush(stdout);

    /* Read line */
    char* line = NULL;
    size_t cap = 0;
    ssize_t len = getline(&line, &cap, stdin);

    if (len < 0) {
        free(line);
        return val_str_lit("");
    }

    if (len > 0 && line[len - 1] == '\n') {
        line[len - 1] = '\0';
        len--;
    }

    forge_value_t result = val_str_copy(line, (int)len);
    free(line);
    return result;
}

/* forge.io.file_exists - check if file exists */
static forge_value_t stdlib_io_file_exists(forge_interp_t* interp,
                                            forge_value_t* args, int arg_count) {
    if (arg_count != 1 || args[0].kind != VAL_STR) {
        interp_error(interp, 0, 0, "forge.io.file_exists() requires string argument");
        return val_none();
    }

    /* Need null-terminated string for access() */
    char* path = forge_malloc(args[0].as.str.len + 1);
    memcpy(path, args[0].as.str.data, args[0].as.str.len);
    path[args[0].as.str.len] = '\0';

    int exists = (access(path, F_OK) == 0);
    forge_free(path);

    return val_bool(exists);
}

/* forge.io.read_file - read entire file contents */
static forge_value_t stdlib_io_read_file(forge_interp_t* interp,
                                          forge_value_t* args, int arg_count) {
    if (arg_count != 1 || args[0].kind != VAL_STR) {
        interp_error(interp, 0, 0, "forge.io.read_file() requires string argument");
        return val_none();
    }

    /* Null-terminate path */
    char* path = forge_malloc(args[0].as.str.len + 1);
    memcpy(path, args[0].as.str.data, args[0].as.str.len);
    path[args[0].as.str.len] = '\0';

    FILE* f = fopen(path, "rb");
    forge_free(path);

    if (!f) {
        /* Return empty optional for now (TODO: Result record) */
        return val_none();
    }

    /* Get file size */
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* buf = forge_malloc(size + 1);
    fread(buf, 1, size, f);
    buf[size] = '\0';
    fclose(f);

    forge_value_t result = val_str_copy(buf, (int)size);
    forge_free(buf);
    return result;
}

/* forge.io.write_file - write string to file */
static forge_value_t stdlib_io_write_file(forge_interp_t* interp,
                                           forge_value_t* args, int arg_count) {
    if (arg_count != 2 || args[0].kind != VAL_STR || args[1].kind != VAL_STR) {
        interp_error(interp, 0, 0, "forge.io.write_file(path, content) requires 2 strings");
        return val_none();
    }

    char* path = forge_malloc(args[0].as.str.len + 1);
    memcpy(path, args[0].as.str.data, args[0].as.str.len);
    path[args[0].as.str.len] = '\0';

    FILE* f = fopen(path, "wb");
    forge_free(path);

    if (!f) {
        return val_bool(0);  /* Failure */
    }

    fwrite(args[1].as.str.data, 1, args[1].as.str.len, f);
    fclose(f);
    return val_bool(1);  /* Success */
}

/* forge.io.append_file - append string to file */
static forge_value_t stdlib_io_append_file(forge_interp_t* interp,
                                            forge_value_t* args, int arg_count) {
    if (arg_count != 2 || args[0].kind != VAL_STR || args[1].kind != VAL_STR) {
        interp_error(interp, 0, 0, "forge.io.append_file(path, content) requires 2 strings");
        return val_none();
    }

    char* path = forge_malloc(args[0].as.str.len + 1);
    memcpy(path, args[0].as.str.data, args[0].as.str.len);
    path[args[0].as.str.len] = '\0';

    FILE* f = fopen(path, "ab");
    forge_free(path);

    if (!f) {
        return val_bool(0);
    }

    fwrite(args[1].as.str.data, 1, args[1].as.str.len, f);
    fclose(f);
    return val_bool(1);
}

/* Try to dispatch a stdlib (forge.io) function call */
static int try_stdlib_io(forge_interp_t* interp, const char* proc_name,
                         forge_value_t* args, int arg_count, forge_value_t* result) {
    if (strcmp(proc_name, "print") == 0) {
        *result = stdlib_io_print(args, arg_count);
        return 1;
    }
    if (strcmp(proc_name, "print_raw") == 0) {
        *result = stdlib_io_print_raw(args, arg_count);
        return 1;
    }
    if (strcmp(proc_name, "eprint") == 0) {
        *result = stdlib_io_eprint(args, arg_count);
        return 1;
    }
    if (strcmp(proc_name, "read_line") == 0) {
        *result = stdlib_io_read_line(interp, args, arg_count);
        return 1;
    }
    if (strcmp(proc_name, "read_line_prompt") == 0) {
        *result = stdlib_io_read_line_prompt(interp, args, arg_count);
        return 1;
    }
    if (strcmp(proc_name, "file_exists") == 0) {
        *result = stdlib_io_file_exists(interp, args, arg_count);
        return 1;
    }
    if (strcmp(proc_name, "read_file") == 0) {
        *result = stdlib_io_read_file(interp, args, arg_count);
        return 1;
    }
    if (strcmp(proc_name, "write_file") == 0) {
        *result = stdlib_io_write_file(interp, args, arg_count);
        return 1;
    }
    if (strcmp(proc_name, "append_file") == 0) {
        *result = stdlib_io_append_file(interp, args, arg_count);
        return 1;
    }
    return 0;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Standard Library: forge.str
 * ───────────────────────────────────────────────────────────────────────────── */

static forge_value_t stdlib_str_len(forge_value_t* args, int arg_count) {
    if (arg_count < 1 || args[0].kind != VAL_STR) return val_int(0);
    return val_int((int64_t)args[0].as.str.len);
}

static forge_value_t stdlib_str_contains(forge_value_t* args, int arg_count) {
    if (arg_count < 2 || args[0].kind != VAL_STR || args[1].kind != VAL_STR)
        return val_bool(0);
    /* Need null-terminated strings for strstr */
    if (args[1].as.str.len == 0) return val_bool(1);
    return val_bool(memmem(args[0].as.str.data, args[0].as.str.len,
                           args[1].as.str.data, args[1].as.str.len) != NULL);
}

static forge_value_t stdlib_str_starts_with(forge_value_t* args, int arg_count) {
    if (arg_count < 2 || args[0].kind != VAL_STR || args[1].kind != VAL_STR)
        return val_bool(0);
    int prefix_len = args[1].as.str.len;
    if (prefix_len > args[0].as.str.len) return val_bool(0);
    return val_bool(memcmp(args[0].as.str.data, args[1].as.str.data, prefix_len) == 0);
}

static forge_value_t stdlib_str_ends_with(forge_value_t* args, int arg_count) {
    if (arg_count < 2 || args[0].kind != VAL_STR || args[1].kind != VAL_STR)
        return val_bool(0);
    int s_len = args[0].as.str.len;
    int suffix_len = args[1].as.str.len;
    if (suffix_len > s_len) return val_bool(0);
    return val_bool(memcmp(args[0].as.str.data + s_len - suffix_len,
                           args[1].as.str.data, suffix_len) == 0);
}

static forge_value_t stdlib_str_find(forge_value_t* args, int arg_count) {
    if (arg_count < 2 || args[0].kind != VAL_STR || args[1].kind != VAL_STR)
        return val_int(-1);
    if (args[1].as.str.len == 0) return val_int(0);
    void* found = memmem(args[0].as.str.data, args[0].as.str.len,
                         args[1].as.str.data, args[1].as.str.len);
    if (!found) return val_int(-1);
    return val_int((int64_t)((char*)found - args[0].as.str.data));
}

static forge_value_t stdlib_str_count(forge_value_t* args, int arg_count) {
    if (arg_count < 2 || args[0].kind != VAL_STR || args[1].kind != VAL_STR)
        return val_int(0);
    const char* s = args[0].as.str.data;
    int s_len = args[0].as.str.len;
    const char* substr = args[1].as.str.data;
    int substr_len = args[1].as.str.len;
    if (substr_len == 0) return val_int(0);

    int64_t count = 0;
    const char* pos = s;
    const char* end = s + s_len;
    while (pos + substr_len <= end) {
        void* found = memmem(pos, end - pos, substr, substr_len);
        if (!found) break;
        count++;
        pos = (char*)found + substr_len;
    }
    return val_int(count);
}

static forge_value_t stdlib_str_upper(forge_interp_t* interp, forge_value_t* args, int arg_count) {
    (void)interp;
    if (arg_count < 1 || args[0].kind != VAL_STR) return val_str_lit("");
    const char* s = args[0].as.str.data;
    int len = args[0].as.str.len;
    char* buf = forge_malloc(len + 1);
    for (int i = 0; i < len; i++) {
        unsigned char c = (unsigned char)s[i];
        buf[i] = (c >= 'a' && c <= 'z') ? (c - 'a' + 'A') : c;
    }
    buf[len] = '\0';
    return val_str_own(buf, len);
}

static forge_value_t stdlib_str_lower(forge_interp_t* interp, forge_value_t* args, int arg_count) {
    (void)interp;
    if (arg_count < 1 || args[0].kind != VAL_STR) return val_str_lit("");
    const char* s = args[0].as.str.data;
    int len = args[0].as.str.len;
    char* buf = forge_malloc(len + 1);
    for (int i = 0; i < len; i++) {
        unsigned char c = (unsigned char)s[i];
        buf[i] = (c >= 'A' && c <= 'Z') ? (c - 'A' + 'a') : c;
    }
    buf[len] = '\0';
    return val_str_own(buf, len);
}

static int is_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static forge_value_t stdlib_str_trim(forge_interp_t* interp, forge_value_t* args, int arg_count) {
    (void)interp;
    if (arg_count < 1 || args[0].kind != VAL_STR) return val_str_lit("");
    const char* s = args[0].as.str.data;
    int len = args[0].as.str.len;
    int start = 0, end = len;
    while (start < end && is_ws(s[start])) start++;
    while (end > start && is_ws(s[end - 1])) end--;

    int new_len = end - start;
    char* buf = forge_malloc(new_len + 1);
    memcpy(buf, s + start, new_len);
    buf[new_len] = '\0';
    return val_str_own(buf, new_len);
}

static forge_value_t stdlib_str_trim_left(forge_interp_t* interp, forge_value_t* args, int arg_count) {
    (void)interp;
    if (arg_count < 1 || args[0].kind != VAL_STR) return val_str_lit("");
    const char* s = args[0].as.str.data;
    int len = args[0].as.str.len;
    int start = 0;
    while (start < len && is_ws(s[start])) start++;
    int new_len = len - start;
    return val_str_copy(s + start, new_len);
}

static forge_value_t stdlib_str_trim_right(forge_interp_t* interp, forge_value_t* args, int arg_count) {
    (void)interp;
    if (arg_count < 1 || args[0].kind != VAL_STR) return val_str_lit("");
    const char* s = args[0].as.str.data;
    int len = args[0].as.str.len;
    while (len > 0 && is_ws(s[len - 1])) len--;
    return val_str_copy(s, len);
}

static forge_value_t stdlib_str_substr(forge_interp_t* interp, forge_value_t* args, int arg_count) {
    (void)interp;
    if (arg_count < 3 || args[0].kind != VAL_STR ||
        args[1].kind != VAL_INT || args[2].kind != VAL_INT)
        return val_str_lit("");

    const char* s = args[0].as.str.data;
    int64_t start = args[1].as.i;
    int64_t length = args[2].as.i;
    int s_len = args[0].as.str.len;

    if (start < 0) start = 0;
    if (start >= s_len) return val_str_lit("");
    if (length < 0 || start + length > s_len) length = s_len - start;

    return val_str_copy(s + start, (int)length);
}

static forge_value_t stdlib_str_replace(forge_interp_t* interp, forge_value_t* args, int arg_count) {
    (void)interp;
    if (arg_count < 3 || args[0].kind != VAL_STR ||
        args[1].kind != VAL_STR || args[2].kind != VAL_STR)
        return val_copy(args[0]);

    const char* s = args[0].as.str.data;
    int s_len = args[0].as.str.len;
    const char* old_str = args[1].as.str.data;
    int old_len = args[1].as.str.len;
    const char* new_str = args[2].as.str.data;
    int new_len = args[2].as.str.len;

    if (old_len == 0) return val_copy(args[0]);

    /* Count occurrences using memmem */
    int count = 0;
    const char* pos = s;
    const char* end = s + s_len;
    while (pos + old_len <= end) {
        void* found = memmem(pos, end - pos, old_str, old_len);
        if (!found) break;
        count++;
        pos = (char*)found + old_len;
    }
    if (count == 0) return val_copy(args[0]);

    /* Build result */
    int result_len = s_len + count * (new_len - old_len);
    char* buf = forge_malloc(result_len + 1);
    char* out = buf;

    pos = s;
    const char* prev = s;
    while (pos + old_len <= end) {
        void* found = memmem(pos, end - pos, old_str, old_len);
        if (!found) break;
        int chunk = (char*)found - prev;
        memcpy(out, prev, chunk);
        out += chunk;
        memcpy(out, new_str, new_len);
        out += new_len;
        prev = (char*)found + old_len;
        pos = prev;
    }
    /* Copy remaining */
    int remain = end - prev;
    memcpy(out, prev, remain);
    out[remain] = '\0';

    return val_str_own(buf, result_len);
}

static forge_value_t stdlib_str_repeat(forge_interp_t* interp, forge_value_t* args, int arg_count) {
    (void)interp;
    if (arg_count < 2 || args[0].kind != VAL_STR || args[1].kind != VAL_INT)
        return val_str_lit("");

    const char* s = args[0].as.str.data;
    int s_len = args[0].as.str.len;
    int64_t n = args[1].as.i;
    if (n <= 0) return val_str_lit("");

    int result_len = s_len * (int)n;
    char* buf = forge_malloc(result_len + 1);

    for (int64_t i = 0; i < n; i++) {
        memcpy(buf + i * s_len, s, s_len);
    }
    buf[result_len] = '\0';

    return val_str_own(buf, result_len);
}

static forge_value_t stdlib_str_reverse(forge_interp_t* interp, forge_value_t* args, int arg_count) {
    (void)interp;
    if (arg_count < 1 || args[0].kind != VAL_STR) return val_str_lit("");

    const char* s = args[0].as.str.data;
    int len = args[0].as.str.len;
    char* buf = forge_malloc(len + 1);

    for (int i = 0; i < len; i++) {
        buf[i] = s[len - 1 - i];
    }
    buf[len] = '\0';

    return val_str_own(buf, len);
}

static forge_value_t stdlib_str_char_at(forge_interp_t* interp, forge_value_t* args, int arg_count) {
    (void)interp;
    if (arg_count < 2 || args[0].kind != VAL_STR || args[1].kind != VAL_INT)
        return val_str_lit("");

    const char* s = args[0].as.str.data;
    int64_t idx = args[1].as.i;
    int len = args[0].as.str.len;

    if (idx < 0 || idx >= len) return val_str_lit("");

    char buf[2] = { s[idx], '\0' };
    return val_str_copy(buf, 1);
}

static forge_value_t stdlib_str_split(forge_interp_t* interp, forge_value_t* args, int arg_count) {
    (void)interp;
    if (arg_count < 2 || args[0].kind != VAL_STR || args[1].kind != VAL_STR)
        return val_array(0, 4);

    const char* s = args[0].as.str.data;
    int s_len = args[0].as.str.len;
    const char* delim = args[1].as.str.data;
    int delim_len = args[1].as.str.len;

    forge_value_t result = val_array(0, 4);

    if (delim_len == 0) {
        /* Split into individual characters */
        for (int i = 0; i < s_len; i++) {
            char buf[2] = { s[i], '\0' };
            forge_value_t ch = val_str_copy(buf, 1);
            val_array_push(&result, ch);
        }
        return result;
    }

    const char* start = s;
    const char* end = s + s_len;
    while (start <= end) {
        void* found = memmem(start, end - start, delim, delim_len);
        if (!found) break;
        int part_len = (char*)found - start;
        forge_value_t part = val_str_copy(start, part_len);
        val_array_push(&result, part);
        start = (char*)found + delim_len;
    }

    /* Add remaining part */
    int remain_len = end - start;
    forge_value_t part = val_str_copy(start, remain_len);
    val_array_push(&result, part);

    return result;
}

static forge_value_t stdlib_str_join(forge_interp_t* interp, forge_value_t* args, int arg_count) {
    (void)interp;
    if (arg_count < 2 || args[0].kind != VAL_ARRAY || args[1].kind != VAL_STR)
        return val_str_lit("");

    forge_value_t* arr = args[0].as.array.elems;
    int arr_len = args[0].as.array.len;
    const char* sep = args[1].as.str.data;
    int sep_len = args[1].as.str.len;

    if (arr_len == 0) return val_str_lit("");

    /* Calculate total length */
    int total = 0;
    for (int i = 0; i < arr_len; i++) {
        if (arr[i].kind == VAL_STR) {
            total += arr[i].as.str.len;
        }
        if (i > 0) total += sep_len;
    }

    char* buf = forge_malloc(total + 1);
    char* out = buf;

    for (int i = 0; i < arr_len; i++) {
        if (i > 0) {
            memcpy(out, sep, sep_len);
            out += sep_len;
        }
        if (arr[i].kind == VAL_STR) {
            int len = arr[i].as.str.len;
            memcpy(out, arr[i].as.str.data, len);
            out += len;
        }
    }
    *out = '\0';

    return val_str_own(buf, total);
}

/* Try to dispatch a stdlib (forge.str) function call */
static int try_stdlib_str(forge_interp_t* interp, const char* proc_name,
                          forge_value_t* args, int arg_count, forge_value_t* result) {
    if (strcmp(proc_name, "len") == 0) {
        *result = stdlib_str_len(args, arg_count);
        return 1;
    }
    if (strcmp(proc_name, "contains") == 0) {
        *result = stdlib_str_contains(args, arg_count);
        return 1;
    }
    if (strcmp(proc_name, "starts_with") == 0) {
        *result = stdlib_str_starts_with(args, arg_count);
        return 1;
    }
    if (strcmp(proc_name, "ends_with") == 0) {
        *result = stdlib_str_ends_with(args, arg_count);
        return 1;
    }
    if (strcmp(proc_name, "find") == 0) {
        *result = stdlib_str_find(args, arg_count);
        return 1;
    }
    if (strcmp(proc_name, "count") == 0) {
        *result = stdlib_str_count(args, arg_count);
        return 1;
    }
    if (strcmp(proc_name, "upper") == 0) {
        *result = stdlib_str_upper(interp, args, arg_count);
        return 1;
    }
    if (strcmp(proc_name, "lower") == 0) {
        *result = stdlib_str_lower(interp, args, arg_count);
        return 1;
    }
    if (strcmp(proc_name, "trim") == 0) {
        *result = stdlib_str_trim(interp, args, arg_count);
        return 1;
    }
    if (strcmp(proc_name, "trim_left") == 0) {
        *result = stdlib_str_trim_left(interp, args, arg_count);
        return 1;
    }
    if (strcmp(proc_name, "trim_right") == 0) {
        *result = stdlib_str_trim_right(interp, args, arg_count);
        return 1;
    }
    if (strcmp(proc_name, "substr") == 0) {
        *result = stdlib_str_substr(interp, args, arg_count);
        return 1;
    }
    if (strcmp(proc_name, "replace") == 0) {
        *result = stdlib_str_replace(interp, args, arg_count);
        return 1;
    }
    if (strcmp(proc_name, "repeat") == 0) {
        *result = stdlib_str_repeat(interp, args, arg_count);
        return 1;
    }
    if (strcmp(proc_name, "reverse") == 0) {
        *result = stdlib_str_reverse(interp, args, arg_count);
        return 1;
    }
    if (strcmp(proc_name, "char_at") == 0) {
        *result = stdlib_str_char_at(interp, args, arg_count);
        return 1;
    }
    if (strcmp(proc_name, "split") == 0) {
        *result = stdlib_str_split(interp, args, arg_count);
        return 1;
    }
    if (strcmp(proc_name, "join") == 0) {
        *result = stdlib_str_join(interp, args, arg_count);
        return 1;
    }
    return 0;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Standard Library: forge.math
 * ───────────────────────────────────────────────────────────────────────────── */

#include <math.h>
#include <time.h>

/* Track whether random has been seeded in interpreter */
static int g_interp_random_seeded = 0;

static void interp_ensure_random_seeded(void) {
    if (!g_interp_random_seeded) {
        srand((unsigned int)time(NULL));
        g_interp_random_seeded = 1;
    }
}

/* Try to dispatch a stdlib (forge.math) function call */
static int try_stdlib_math(forge_interp_t* interp, const char* proc_name,
                           forge_value_t* args, int arg_count, forge_value_t* result) {
    (void)interp;

    /* Constants (accessed as functions for now) */
    if (strcmp(proc_name, "PI") == 0) {
        *result = val_float(3.14159265358979323846);
        return 1;
    }
    if (strcmp(proc_name, "E") == 0) {
        *result = val_float(2.71828182845904523536);
        return 1;
    }
    if (strcmp(proc_name, "TAU") == 0) {
        *result = val_float(6.28318530717958647692);
        return 1;
    }

    /* Absolute value */
    if (strcmp(proc_name, "abs") == 0) {
        if (arg_count < 1) return 0;
        if (args[0].kind == VAL_INT)
            *result = val_int(args[0].as.i < 0 ? -args[0].as.i : args[0].as.i);
        else if (args[0].kind == VAL_FLOAT)
            *result = val_float(fabs(args[0].as.f));
        else
            *result = val_float(0.0);
        return 1;
    }
    if (strcmp(proc_name, "abs_int") == 0) {
        if (arg_count < 1 || args[0].kind != VAL_INT) {
            *result = val_int(0);
            return 1;
        }
        *result = val_int(args[0].as.i < 0 ? -args[0].as.i : args[0].as.i);
        return 1;
    }

    /* Min/Max */
    if (strcmp(proc_name, "min") == 0) {
        if (arg_count < 2) { *result = val_float(0.0); return 1; }
        double a = args[0].kind == VAL_FLOAT ? args[0].as.f : (double)args[0].as.i;
        double b = args[1].kind == VAL_FLOAT ? args[1].as.f : (double)args[1].as.i;
        *result = val_float(a < b ? a : b);
        return 1;
    }
    if (strcmp(proc_name, "max") == 0) {
        if (arg_count < 2) { *result = val_float(0.0); return 1; }
        double a = args[0].kind == VAL_FLOAT ? args[0].as.f : (double)args[0].as.i;
        double b = args[1].kind == VAL_FLOAT ? args[1].as.f : (double)args[1].as.i;
        *result = val_float(a > b ? a : b);
        return 1;
    }
    if (strcmp(proc_name, "min_int") == 0) {
        if (arg_count < 2 || args[0].kind != VAL_INT || args[1].kind != VAL_INT) {
            *result = val_int(0);
            return 1;
        }
        *result = val_int(args[0].as.i < args[1].as.i ? args[0].as.i : args[1].as.i);
        return 1;
    }
    if (strcmp(proc_name, "max_int") == 0) {
        if (arg_count < 2 || args[0].kind != VAL_INT || args[1].kind != VAL_INT) {
            *result = val_int(0);
            return 1;
        }
        *result = val_int(args[0].as.i > args[1].as.i ? args[0].as.i : args[1].as.i);
        return 1;
    }

    /* Clamp */
    if (strcmp(proc_name, "clamp") == 0) {
        if (arg_count < 3) { *result = val_float(0.0); return 1; }
        double val = args[0].kind == VAL_FLOAT ? args[0].as.f : (double)args[0].as.i;
        double lo = args[1].kind == VAL_FLOAT ? args[1].as.f : (double)args[1].as.i;
        double hi = args[2].kind == VAL_FLOAT ? args[2].as.f : (double)args[2].as.i;
        if (val < lo) val = lo;
        if (val > hi) val = hi;
        *result = val_float(val);
        return 1;
    }

    /* Power and roots */
    if (strcmp(proc_name, "pow") == 0) {
        if (arg_count < 2) { *result = val_float(0.0); return 1; }
        double base = args[0].kind == VAL_FLOAT ? args[0].as.f : (double)args[0].as.i;
        double exp = args[1].kind == VAL_FLOAT ? args[1].as.f : (double)args[1].as.i;
        *result = val_float(pow(base, exp));
        return 1;
    }
    if (strcmp(proc_name, "sqrt") == 0) {
        if (arg_count < 1) { *result = val_float(0.0); return 1; }
        double x = args[0].kind == VAL_FLOAT ? args[0].as.f : (double)args[0].as.i;
        *result = val_float(sqrt(x));
        return 1;
    }
    if (strcmp(proc_name, "cbrt") == 0) {
        if (arg_count < 1) { *result = val_float(0.0); return 1; }
        double x = args[0].kind == VAL_FLOAT ? args[0].as.f : (double)args[0].as.i;
        *result = val_float(cbrt(x));
        return 1;
    }

    /* Rounding */
    if (strcmp(proc_name, "floor") == 0) {
        if (arg_count < 1) { *result = val_float(0.0); return 1; }
        double x = args[0].kind == VAL_FLOAT ? args[0].as.f : (double)args[0].as.i;
        *result = val_float(floor(x));
        return 1;
    }
    if (strcmp(proc_name, "ceil") == 0) {
        if (arg_count < 1) { *result = val_float(0.0); return 1; }
        double x = args[0].kind == VAL_FLOAT ? args[0].as.f : (double)args[0].as.i;
        *result = val_float(ceil(x));
        return 1;
    }
    if (strcmp(proc_name, "round") == 0) {
        if (arg_count < 1) { *result = val_float(0.0); return 1; }
        double x = args[0].kind == VAL_FLOAT ? args[0].as.f : (double)args[0].as.i;
        *result = val_float(round(x));
        return 1;
    }
    if (strcmp(proc_name, "trunc") == 0) {
        if (arg_count < 1) { *result = val_float(0.0); return 1; }
        double x = args[0].kind == VAL_FLOAT ? args[0].as.f : (double)args[0].as.i;
        *result = val_float(trunc(x));
        return 1;
    }

    /* Trigonometry */
    if (strcmp(proc_name, "sin") == 0) {
        if (arg_count < 1) { *result = val_float(0.0); return 1; }
        double x = args[0].kind == VAL_FLOAT ? args[0].as.f : (double)args[0].as.i;
        *result = val_float(sin(x));
        return 1;
    }
    if (strcmp(proc_name, "cos") == 0) {
        if (arg_count < 1) { *result = val_float(0.0); return 1; }
        double x = args[0].kind == VAL_FLOAT ? args[0].as.f : (double)args[0].as.i;
        *result = val_float(cos(x));
        return 1;
    }
    if (strcmp(proc_name, "tan") == 0) {
        if (arg_count < 1) { *result = val_float(0.0); return 1; }
        double x = args[0].kind == VAL_FLOAT ? args[0].as.f : (double)args[0].as.i;
        *result = val_float(tan(x));
        return 1;
    }
    if (strcmp(proc_name, "atan2") == 0) {
        if (arg_count < 2) { *result = val_float(0.0); return 1; }
        double y = args[0].kind == VAL_FLOAT ? args[0].as.f : (double)args[0].as.i;
        double x = args[1].kind == VAL_FLOAT ? args[1].as.f : (double)args[1].as.i;
        *result = val_float(atan2(y, x));
        return 1;
    }

    /* Logarithms and exponentials */
    if (strcmp(proc_name, "log") == 0) {
        if (arg_count < 1) { *result = val_float(0.0); return 1; }
        double x = args[0].kind == VAL_FLOAT ? args[0].as.f : (double)args[0].as.i;
        *result = val_float(log(x));
        return 1;
    }
    if (strcmp(proc_name, "log10") == 0) {
        if (arg_count < 1) { *result = val_float(0.0); return 1; }
        double x = args[0].kind == VAL_FLOAT ? args[0].as.f : (double)args[0].as.i;
        *result = val_float(log10(x));
        return 1;
    }
    if (strcmp(proc_name, "log2") == 0) {
        if (arg_count < 1) { *result = val_float(0.0); return 1; }
        double x = args[0].kind == VAL_FLOAT ? args[0].as.f : (double)args[0].as.i;
        *result = val_float(log2(x));
        return 1;
    }
    if (strcmp(proc_name, "exp") == 0) {
        if (arg_count < 1) { *result = val_float(0.0); return 1; }
        double x = args[0].kind == VAL_FLOAT ? args[0].as.f : (double)args[0].as.i;
        *result = val_float(exp(x));
        return 1;
    }

    /* Random numbers */
    if (strcmp(proc_name, "random_int") == 0) {
        if (arg_count < 2 || args[0].kind != VAL_INT || args[1].kind != VAL_INT) {
            *result = val_int(0);
            return 1;
        }
        interp_ensure_random_seeded();
        int64_t lo = args[0].as.i;
        int64_t hi = args[1].as.i;
        if (hi <= lo) {
            *result = val_int(lo);
            return 1;
        }
        int64_t range = hi - lo;
        *result = val_int(lo + (rand() % range));
        return 1;
    }
    if (strcmp(proc_name, "random_float") == 0) {
        interp_ensure_random_seeded();
        *result = val_float((double)rand() / ((double)RAND_MAX + 1.0));
        return 1;
    }
    if (strcmp(proc_name, "seed_random") == 0) {
        if (arg_count < 1 || args[0].kind != VAL_INT) {
            *result = val_void();
            return 1;
        }
        srand((unsigned int)args[0].as.i);
        g_interp_random_seeded = 1;
        *result = val_void();
        return 1;
    }

    return 0;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Standard Library: forge.sys
 * ───────────────────────────────────────────────────────────────────────────── */

/* Global storage for command-line args (set by interpreter init) */
static int g_interp_argc = 0;
static char** g_interp_argv = NULL;

void interp_set_args(int argc, char** argv) {
    g_interp_argc = argc;
    g_interp_argv = argv;
}

static forge_value_t stdlib_sys_args(void) {
    /* Create array of strings from command-line args */
    forge_value_t arr = val_array(g_interp_argc, g_interp_argc > 0 ? g_interp_argc : 4);
    for (int i = 0; i < g_interp_argc; i++) {
        forge_value_t arg = val_str_copy(g_interp_argv[i], strlen(g_interp_argv[i]));
        val_array_push(&arr, arg);
    }
    return arr;
}

static forge_value_t stdlib_sys_env(forge_value_t* args, int arg_count) {
    if (arg_count < 1 || args[0].kind != VAL_STR) {
        return val_str_lit("");
    }
    /* Need null-terminated string for getenv */
    char* name = forge_malloc(args[0].as.str.len + 1);
    memcpy(name, args[0].as.str.data, args[0].as.str.len);
    name[args[0].as.str.len] = '\0';

    const char* val = getenv(name);
    forge_free(name);

    if (val == NULL) {
        return val_str_lit("");
    }
    return val_str_copy(val, strlen(val));
}

static forge_value_t stdlib_sys_exit(forge_value_t* args, int arg_count) {
    int code = 0;
    if (arg_count >= 1 && args[0].kind == VAL_INT) {
        code = (int)args[0].as.i;
    }
    exit(code);
    return val_void();  /* Never reached */
}

static forge_value_t stdlib_sys_halt(void) {
    exit(0);
    return val_void();  /* Never reached */
}

static forge_value_t stdlib_sys_platform(void) {
#if defined(__linux__)
    return val_str_lit("linux");
#elif defined(__APPLE__) && defined(__MACH__)
    return val_str_lit("macos");
#elif defined(_WIN32) || defined(_WIN64)
    return val_str_lit("windows");
#else
    return val_str_lit("embedded");
#endif
}

static forge_value_t stdlib_sys_arch(void) {
#if defined(__x86_64__) || defined(_M_X64)
    return val_str_lit("x86_64");
#elif defined(__aarch64__) || defined(_M_ARM64)
    return val_str_lit("arm64");
#elif defined(__arm__) || defined(_M_ARM)
    return val_str_lit("arm");
#elif defined(__riscv) && (__riscv_xlen == 64)
    return val_str_lit("riscv64");
#elif defined(__riscv) && (__riscv_xlen == 32)
    return val_str_lit("riscv32");
#elif defined(__i386__) || defined(_M_IX86)
    return val_str_lit("x86");
#else
    return val_str_lit("unknown");
#endif
}

/* Try to dispatch a stdlib (forge.sys) function call */
static int try_stdlib_sys(forge_interp_t* interp, const char* proc_name,
                          forge_value_t* args, int arg_count, forge_value_t* result) {
    (void)interp;

    if (strcmp(proc_name, "args") == 0) {
        *result = stdlib_sys_args();
        return 1;
    }
    if (strcmp(proc_name, "env") == 0) {
        *result = stdlib_sys_env(args, arg_count);
        return 1;
    }
    if (strcmp(proc_name, "exit") == 0) {
        *result = stdlib_sys_exit(args, arg_count);
        return 1;
    }
    if (strcmp(proc_name, "halt") == 0) {
        *result = stdlib_sys_halt();
        return 1;
    }
    if (strcmp(proc_name, "platform") == 0) {
        *result = stdlib_sys_platform();
        return 1;
    }
    if (strcmp(proc_name, "arch") == 0) {
        *result = stdlib_sys_arch();
        return 1;
    }

    return 0;
}

/* ───────────────────────────────────────────────────────────────────────────────
 * Standard Library: forge.time
 * ─────────────────────────────────────────────────────────────────────────────── */

#include <sys/time.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

static uint64_t interp_time_now(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000ULL + (uint64_t)tv.tv_usec / 1000ULL;
}

static void interp_time_sleep(uint64_t ms) {
#ifdef _WIN32
    Sleep((DWORD)ms);
#else
    usleep((useconds_t)(ms * 1000));
#endif
}

static forge_value_t interp_time_timestamp(void) {
    time_t now = time(NULL);
    struct tm* tm_info = gmtime(&now);

    /* ISO 8601 format: YYYY-MM-DDTHH:MM:SSZ */
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", tm_info);

    return val_str_copy(buf, strlen(buf));
}

/* Clock state stored as int (start_ms in high 32 bits, lap_ms in low 32 bits)
 * Actually, we'll store clocks differently - as a record or just track start time
 * For simplicity, we'll use the record approach with two fields packed into a value
 */

/* Simple approach: start_clock returns the start time, lap takes start time + stores lap time in a var */
/* Actually, let's use a simpler model: just track timing with now() and elapsed_ms() */

/* Try to dispatch a stdlib (forge.time) function call */
static int try_stdlib_time(forge_interp_t* interp, const char* proc_name,
                           forge_value_t* args, int arg_count, forge_value_t* result) {
    (void)interp;

    if (strcmp(proc_name, "now") == 0) {
        *result = val_int((int64_t)interp_time_now());
        return 1;
    }

    if (strcmp(proc_name, "sleep") == 0) {
        if (arg_count < 1 || args[0].kind != VAL_INT) {
            *result = val_void();
            return 1;
        }
        interp_time_sleep((uint64_t)args[0].as.i);
        *result = val_void();
        return 1;
    }

    if (strcmp(proc_name, "timestamp") == 0) {
        *result = interp_time_timestamp();
        return 1;
    }

    if (strcmp(proc_name, "elapsed_ms") == 0) {
        if (arg_count < 1 || args[0].kind != VAL_INT) {
            *result = val_int(0);
            return 1;
        }
        uint64_t start = (uint64_t)args[0].as.i;
        uint64_t now = interp_time_now();
        *result = val_int((int64_t)(now - start));
        return 1;
    }

    if (strcmp(proc_name, "start_clock") == 0) {
        /* Return current time as the clock start value */
        *result = val_int((int64_t)interp_time_now());
        return 1;
    }

    if (strcmp(proc_name, "lap") == 0) {
        /* For lap, we need a mutable clock - for now, just return elapsed since arg */
        if (arg_count < 1 || args[0].kind != VAL_INT) {
            *result = val_int(0);
            return 1;
        }
        uint64_t start = (uint64_t)args[0].as.i;
        uint64_t now = interp_time_now();
        *result = val_int((int64_t)(now - start));
        return 1;
    }

    return 0;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * forge.buf Internal Buffer Pool for Interpreter
 * ───────────────────────────────────────────────────────────────────────────── */

#define INTERP_BUF_POOL_SIZE 256

typedef struct {
    uint8_t* data;
    int64_t  length;    /* bytes written */
    int64_t  capacity;
    int64_t  position;  /* read position */
    int      in_use;
} interp_buf_t;

static interp_buf_t g_interp_buf_pool[INTERP_BUF_POOL_SIZE];
static int g_interp_buf_pool_init = 0;

static void interp_buf_pool_init(void) {
    if (!g_interp_buf_pool_init) {
        for (int i = 0; i < INTERP_BUF_POOL_SIZE; i++) {
            g_interp_buf_pool[i].data = NULL;
            g_interp_buf_pool[i].length = 0;
            g_interp_buf_pool[i].capacity = 0;
            g_interp_buf_pool[i].position = 0;
            g_interp_buf_pool[i].in_use = 0;
        }
        g_interp_buf_pool_init = 1;
    }
}

static int64_t interp_buf_create(int64_t capacity) {
    interp_buf_pool_init();
    for (int i = 0; i < INTERP_BUF_POOL_SIZE; i++) {
        if (!g_interp_buf_pool[i].in_use) {
            g_interp_buf_pool[i].data = (uint8_t*)forge_malloc((size_t)capacity);
            g_interp_buf_pool[i].length = 0;
            g_interp_buf_pool[i].capacity = capacity;
            g_interp_buf_pool[i].position = 0;
            g_interp_buf_pool[i].in_use = 1;
            return (int64_t)i;
        }
    }
    return -1;
}

static void interp_buf_free(int64_t handle) {
    if (handle < 0 || handle >= INTERP_BUF_POOL_SIZE) return;
    if (!g_interp_buf_pool[handle].in_use) return;
    forge_free(g_interp_buf_pool[handle].data);
    g_interp_buf_pool[handle].data = NULL;
    g_interp_buf_pool[handle].length = 0;
    g_interp_buf_pool[handle].capacity = 0;
    g_interp_buf_pool[handle].position = 0;
    g_interp_buf_pool[handle].in_use = 0;
}

static void interp_buf_ensure_capacity(interp_buf_t* buf, int64_t needed) {
    if (buf->length + needed > buf->capacity) {
        int64_t new_cap = buf->capacity * 2;
        if (new_cap < buf->length + needed) new_cap = buf->length + needed;
        uint8_t* new_data = (uint8_t*)forge_malloc((size_t)new_cap);
        if (new_data) {
            memcpy(new_data, buf->data, (size_t)buf->length);
            forge_free(buf->data);
            buf->data = new_data;
            buf->capacity = new_cap;
        }
    }
}

/* Try to dispatch a stdlib (forge.buf) function call */
static int try_stdlib_buf(forge_interp_t* interp, const char* proc_name,
                          forge_value_t* args, int arg_count, forge_value_t* result) {
    (void)interp;

    if (strcmp(proc_name, "create") == 0) {
        int64_t cap = (arg_count > 0 && args[0].kind == VAL_INT) ? args[0].as.i : 256;
        *result = val_int(interp_buf_create(cap));
        return 1;
    }

    if (strcmp(proc_name, "free_buf") == 0) {
        if (arg_count > 0 && args[0].kind == VAL_INT) {
            interp_buf_free(args[0].as.i);
        }
        *result = val_void();
        return 1;
    }

    if (strcmp(proc_name, "write_byte") == 0) {
        if (arg_count >= 2 && args[0].kind == VAL_INT && args[1].kind == VAL_INT) {
            int64_t h = args[0].as.i;
            if (h >= 0 && h < INTERP_BUF_POOL_SIZE && g_interp_buf_pool[h].in_use) {
                interp_buf_t* buf = &g_interp_buf_pool[h];
                interp_buf_ensure_capacity(buf, 1);
                buf->data[buf->length++] = (uint8_t)(args[1].as.i & 0xFF);
            }
        }
        *result = val_void();
        return 1;
    }

    if (strcmp(proc_name, "write_str") == 0) {
        if (arg_count >= 2 && args[0].kind == VAL_INT && args[1].kind == VAL_STR) {
            int64_t h = args[0].as.i;
            if (h >= 0 && h < INTERP_BUF_POOL_SIZE && g_interp_buf_pool[h].in_use) {
                interp_buf_t* buf = &g_interp_buf_pool[h];
                int len = args[1].as.str.len;
                interp_buf_ensure_capacity(buf, len);
                memcpy(buf->data + buf->length, args[1].as.str.data, (size_t)len);
                buf->length += len;
            }
        }
        *result = val_void();
        return 1;
    }

    if (strcmp(proc_name, "write_int16_le") == 0) {
        if (arg_count >= 2 && args[0].kind == VAL_INT && args[1].kind == VAL_INT) {
            int64_t h = args[0].as.i;
            if (h >= 0 && h < INTERP_BUF_POOL_SIZE && g_interp_buf_pool[h].in_use) {
                interp_buf_t* buf = &g_interp_buf_pool[h];
                interp_buf_ensure_capacity(buf, 2);
                int16_t v = (int16_t)args[1].as.i;
                buf->data[buf->length++] = (uint8_t)(v & 0xFF);
                buf->data[buf->length++] = (uint8_t)((v >> 8) & 0xFF);
            }
        }
        *result = val_void();
        return 1;
    }

    if (strcmp(proc_name, "write_int32_le") == 0) {
        if (arg_count >= 2 && args[0].kind == VAL_INT && args[1].kind == VAL_INT) {
            int64_t h = args[0].as.i;
            if (h >= 0 && h < INTERP_BUF_POOL_SIZE && g_interp_buf_pool[h].in_use) {
                interp_buf_t* buf = &g_interp_buf_pool[h];
                interp_buf_ensure_capacity(buf, 4);
                int32_t v = (int32_t)args[1].as.i;
                buf->data[buf->length++] = (uint8_t)(v & 0xFF);
                buf->data[buf->length++] = (uint8_t)((v >> 8) & 0xFF);
                buf->data[buf->length++] = (uint8_t)((v >> 16) & 0xFF);
                buf->data[buf->length++] = (uint8_t)((v >> 24) & 0xFF);
            }
        }
        *result = val_void();
        return 1;
    }

    if (strcmp(proc_name, "write_float32_le") == 0) {
        if (arg_count >= 2 && args[0].kind == VAL_INT &&
            (args[1].kind == VAL_FLOAT || args[1].kind == VAL_INT)) {
            int64_t h = args[0].as.i;
            if (h >= 0 && h < INTERP_BUF_POOL_SIZE && g_interp_buf_pool[h].in_use) {
                interp_buf_t* buf = &g_interp_buf_pool[h];
                interp_buf_ensure_capacity(buf, 4);
                double val = (args[1].kind == VAL_FLOAT) ? args[1].as.f : (double)args[1].as.i;
                float f = (float)val;
                uint32_t bits;
                memcpy(&bits, &f, sizeof(bits));
                buf->data[buf->length++] = (uint8_t)(bits & 0xFF);
                buf->data[buf->length++] = (uint8_t)((bits >> 8) & 0xFF);
                buf->data[buf->length++] = (uint8_t)((bits >> 16) & 0xFF);
                buf->data[buf->length++] = (uint8_t)((bits >> 24) & 0xFF);
            }
        }
        *result = val_void();
        return 1;
    }

    if (strcmp(proc_name, "read_byte") == 0) {
        if (arg_count > 0 && args[0].kind == VAL_INT) {
            int64_t h = args[0].as.i;
            if (h >= 0 && h < INTERP_BUF_POOL_SIZE && g_interp_buf_pool[h].in_use) {
                interp_buf_t* buf = &g_interp_buf_pool[h];
                if (buf->position < buf->length) {
                    *result = val_int((int64_t)buf->data[buf->position++]);
                    return 1;
                }
            }
        }
        *result = val_int(-1);  /* none sentinel */
        return 1;
    }

    if (strcmp(proc_name, "read_int16_le") == 0) {
        if (arg_count > 0 && args[0].kind == VAL_INT) {
            int64_t h = args[0].as.i;
            if (h >= 0 && h < INTERP_BUF_POOL_SIZE && g_interp_buf_pool[h].in_use) {
                interp_buf_t* buf = &g_interp_buf_pool[h];
                if (buf->position + 2 <= buf->length) {
                    int16_t v = (int16_t)(buf->data[buf->position] |
                                         (buf->data[buf->position + 1] << 8));
                    buf->position += 2;
                    *result = val_int((int64_t)v);
                    return 1;
                }
            }
        }
        *result = val_int(INT64_MIN);  /* none sentinel */
        return 1;
    }

    if (strcmp(proc_name, "read_int32_le") == 0) {
        if (arg_count > 0 && args[0].kind == VAL_INT) {
            int64_t h = args[0].as.i;
            if (h >= 0 && h < INTERP_BUF_POOL_SIZE && g_interp_buf_pool[h].in_use) {
                interp_buf_t* buf = &g_interp_buf_pool[h];
                if (buf->position + 4 <= buf->length) {
                    int32_t v = (int32_t)(buf->data[buf->position] |
                                         (buf->data[buf->position + 1] << 8) |
                                         (buf->data[buf->position + 2] << 16) |
                                         (buf->data[buf->position + 3] << 24));
                    buf->position += 4;
                    *result = val_int((int64_t)v);
                    return 1;
                }
            }
        }
        *result = val_int(INT64_MIN);  /* none sentinel */
        return 1;
    }

    if (strcmp(proc_name, "seek") == 0) {
        if (arg_count >= 2 && args[0].kind == VAL_INT && args[1].kind == VAL_INT) {
            int64_t h = args[0].as.i;
            if (h >= 0 && h < INTERP_BUF_POOL_SIZE && g_interp_buf_pool[h].in_use) {
                interp_buf_t* buf = &g_interp_buf_pool[h];
                int64_t pos = args[1].as.i;
                if (pos < 0) pos = 0;
                if (pos > buf->length) pos = buf->length;
                buf->position = pos;
            }
        }
        *result = val_void();
        return 1;
    }

    if (strcmp(proc_name, "rewind") == 0) {
        if (arg_count > 0 && args[0].kind == VAL_INT) {
            int64_t h = args[0].as.i;
            if (h >= 0 && h < INTERP_BUF_POOL_SIZE && g_interp_buf_pool[h].in_use) {
                g_interp_buf_pool[h].position = 0;
            }
        }
        *result = val_void();
        return 1;
    }

    if (strcmp(proc_name, "remaining") == 0) {
        if (arg_count > 0 && args[0].kind == VAL_INT) {
            int64_t h = args[0].as.i;
            if (h >= 0 && h < INTERP_BUF_POOL_SIZE && g_interp_buf_pool[h].in_use) {
                interp_buf_t* buf = &g_interp_buf_pool[h];
                *result = val_int(buf->length - buf->position);
                return 1;
            }
        }
        *result = val_int(0);
        return 1;
    }

    if (strcmp(proc_name, "length") == 0) {
        if (arg_count > 0 && args[0].kind == VAL_INT) {
            int64_t h = args[0].as.i;
            if (h >= 0 && h < INTERP_BUF_POOL_SIZE && g_interp_buf_pool[h].in_use) {
                *result = val_int(g_interp_buf_pool[h].length);
                return 1;
            }
        }
        *result = val_int(0);
        return 1;
    }

    if (strcmp(proc_name, "capacity") == 0) {
        if (arg_count > 0 && args[0].kind == VAL_INT) {
            int64_t h = args[0].as.i;
            if (h >= 0 && h < INTERP_BUF_POOL_SIZE && g_interp_buf_pool[h].in_use) {
                *result = val_int(g_interp_buf_pool[h].capacity);
                return 1;
            }
        }
        *result = val_int(0);
        return 1;
    }

    if (strcmp(proc_name, "position") == 0) {
        if (arg_count > 0 && args[0].kind == VAL_INT) {
            int64_t h = args[0].as.i;
            if (h >= 0 && h < INTERP_BUF_POOL_SIZE && g_interp_buf_pool[h].in_use) {
                *result = val_int(g_interp_buf_pool[h].position);
                return 1;
            }
        }
        *result = val_int(0);
        return 1;
    }

    if (strcmp(proc_name, "to_str") == 0) {
        if (arg_count > 0 && args[0].kind == VAL_INT) {
            int64_t h = args[0].as.i;
            if (h >= 0 && h < INTERP_BUF_POOL_SIZE && g_interp_buf_pool[h].in_use) {
                interp_buf_t* buf = &g_interp_buf_pool[h];
                char* copy = (char*)forge_malloc((size_t)(buf->length + 1));
                memcpy(copy, buf->data, (size_t)buf->length);
                copy[buf->length] = '\0';
                *result = val_str_own(copy, (int)buf->length);
                return 1;
            }
        }
        *result = val_str_lit("");
        return 1;
    }

    if (strcmp(proc_name, "to_hex") == 0) {
        if (arg_count > 0 && args[0].kind == VAL_INT) {
            int64_t h = args[0].as.i;
            if (h >= 0 && h < INTERP_BUF_POOL_SIZE && g_interp_buf_pool[h].in_use) {
                interp_buf_t* buf = &g_interp_buf_pool[h];
                if (buf->length == 0) {
                    *result = val_str_lit("");
                    return 1;
                }
                int64_t hex_len = buf->length * 3 - 1;
                char* hex = (char*)forge_malloc((size_t)(hex_len + 1));
                static const char hex_chars[] = "0123456789ABCDEF";
                char* p = hex;
                for (int64_t i = 0; i < buf->length; i++) {
                    if (i > 0) *p++ = ' ';
                    uint8_t b = buf->data[i];
                    *p++ = hex_chars[(b >> 4) & 0xF];
                    *p++ = hex_chars[b & 0xF];
                }
                *p = '\0';
                *result = val_str_own(hex, (int)hex_len);
                return 1;
            }
        }
        *result = val_str_lit("");
        return 1;
    }

    return 0;
}

/* Try to dispatch a stdlib (forge.serial) function call */
static int try_stdlib_serial(forge_interp_t* interp, const char* proc_name,
                             forge_value_t* args, int arg_count, forge_value_t* result) {
    (void)interp;

    /* Use runtime serial functions directly */

    if (strcmp(proc_name, "open") == 0) {
        if (arg_count >= 2 && args[0].kind == VAL_STR && args[1].kind == VAL_INT) {
            forge_str_t path;
            path.data = (char*)args[0].as.str.data;
            path.len = args[0].as.str.len;
            path.owned = 0;  /* Don't free - we don't own this */
            int64_t handle = forge_serial_open(path, args[1].as.i);
            *result = val_int(handle);
            return 1;
        }
        *result = val_int(-1);
        return 1;
    }

    if (strcmp(proc_name, "close") == 0) {
        if (arg_count > 0 && args[0].kind == VAL_INT) {
            forge_serial_close(args[0].as.i);
        }
        *result = val_void();
        return 1;
    }

    if (strcmp(proc_name, "read_byte") == 0) {
        if (arg_count > 0 && args[0].kind == VAL_INT) {
            int64_t byte = forge_serial_read_byte(args[0].as.i);
            *result = val_int(byte);
            return 1;
        }
        *result = val_int(-1);
        return 1;
    }

    if (strcmp(proc_name, "bytes_available") == 0) {
        if (arg_count > 0 && args[0].kind == VAL_INT) {
            int64_t count = forge_serial_bytes_available(args[0].as.i);
            *result = val_int(count);
            return 1;
        }
        *result = val_int(0);
        return 1;
    }

    if (strcmp(proc_name, "read_line") == 0) {
        if (arg_count > 0 && args[0].kind == VAL_INT) {
            forge_str_t line = forge_serial_read_line(args[0].as.i);
            /* Convert runtime string to interpreter string */
            char* copy = (char*)forge_malloc((size_t)(line.len + 1));
            memcpy(copy, line.data, (size_t)line.len);
            copy[line.len] = '\0';
            *result = val_str_own(copy, line.len);
            return 1;
        }
        *result = val_str_lit("");
        return 1;
    }

    if (strcmp(proc_name, "write_byte") == 0) {
        if (arg_count >= 2 && args[0].kind == VAL_INT && args[1].kind == VAL_INT) {
            forge_serial_write_byte(args[0].as.i, args[1].as.i);
        }
        *result = val_void();
        return 1;
    }

    if (strcmp(proc_name, "write_str") == 0) {
        if (arg_count >= 2 && args[0].kind == VAL_INT && args[1].kind == VAL_STR) {
            forge_str_t s;
            s.data = (char*)args[1].as.str.data;
            s.len = args[1].as.str.len;
            s.owned = 0;  /* Don't free - we don't own this */
            forge_serial_write_str(args[0].as.i, s);
        }
        *result = val_void();
        return 1;
    }

    if (strcmp(proc_name, "set_timeout") == 0) {
        if (arg_count >= 2 && args[0].kind == VAL_INT && args[1].kind == VAL_INT) {
            forge_serial_set_timeout(args[0].as.i, args[1].as.i);
        }
        *result = val_void();
        return 1;
    }

    if (strcmp(proc_name, "flush") == 0) {
        if (arg_count > 0 && args[0].kind == VAL_INT) {
            forge_serial_flush(args[0].as.i);
        }
        *result = val_void();
        return 1;
    }

    if (strcmp(proc_name, "is_open") == 0) {
        if (arg_count > 0 && args[0].kind == VAL_INT) {
            int64_t open = forge_serial_is_open(args[0].as.i);
            *result = val_int(open);
            return 1;
        }
        *result = val_int(0);
        return 1;
    }

    if (strcmp(proc_name, "get_baud") == 0) {
        if (arg_count > 0 && args[0].kind == VAL_INT) {
            int64_t baud = forge_serial_get_baud(args[0].as.i);
            *result = val_int(baud);
            return 1;
        }
        *result = val_int(0);
        return 1;
    }

    return 0;
}

/* Helper: convert interpreter string to runtime string */
static forge_str_t interp_str_to_runtime(forge_value_t* v) {
    forge_str_t s;
    s.data = (char*)v->as.str.data;
    s.len = v->as.str.len;
    s.owned = 0;  /* Don't free - interpreter owns this */
    return s;
}

/* Helper: convert runtime string to interpreter string (with copy) */
static forge_value_t runtime_str_to_interp(forge_str_t s) {
    char* copy = (char*)forge_malloc((size_t)(s.len + 1));
    memcpy(copy, s.data, (size_t)s.len);
    copy[s.len] = '\0';
    /* Free runtime string if owned */
    if (s.owned) {
        free((void*)s.data);
    }
    return val_str_own(copy, s.len);
}

/* Try to dispatch a stdlib (forge.nmea) function call */
static int try_stdlib_nmea(forge_interp_t* interp, const char* proc_name,
                           forge_value_t* args, int arg_count, forge_value_t* result) {
    (void)interp;

    if (strcmp(proc_name, "validate") == 0) {
        if (arg_count > 0 && args[0].kind == VAL_STR) {
            forge_str_t sentence = interp_str_to_runtime(&args[0]);
            int64_t valid = forge_nmea_validate(sentence);
            *result = val_int(valid);
            return 1;
        }
        *result = val_int(0);
        return 1;
    }

    if (strcmp(proc_name, "checksum") == 0) {
        if (arg_count > 0 && args[0].kind == VAL_STR) {
            forge_str_t sentence = interp_str_to_runtime(&args[0]);
            forge_str_t cs = forge_nmea_checksum(sentence);
            *result = runtime_str_to_interp(cs);
            return 1;
        }
        *result = val_str_lit("");
        return 1;
    }

    if (strcmp(proc_name, "sentence_type") == 0) {
        if (arg_count > 0 && args[0].kind == VAL_STR) {
            forge_str_t sentence = interp_str_to_runtime(&args[0]);
            forge_str_t type = forge_nmea_sentence_type(sentence);
            *result = runtime_str_to_interp(type);
            return 1;
        }
        *result = val_str_lit("");
        return 1;
    }

    if (strcmp(proc_name, "get_talker") == 0) {
        if (arg_count > 0 && args[0].kind == VAL_STR) {
            forge_str_t sentence = interp_str_to_runtime(&args[0]);
            forge_str_t talker = forge_nmea_get_talker(sentence);
            *result = runtime_str_to_interp(talker);
            return 1;
        }
        *result = val_str_lit("");
        return 1;
    }

    if (strcmp(proc_name, "field_count") == 0) {
        if (arg_count > 0 && args[0].kind == VAL_STR) {
            forge_str_t sentence = interp_str_to_runtime(&args[0]);
            int64_t count = forge_nmea_field_count(sentence);
            *result = val_int(count);
            return 1;
        }
        *result = val_int(0);
        return 1;
    }

    if (strcmp(proc_name, "get_field") == 0) {
        if (arg_count >= 2 && args[0].kind == VAL_STR && args[1].kind == VAL_INT) {
            forge_str_t sentence = interp_str_to_runtime(&args[0]);
            forge_str_t field = forge_nmea_get_field(sentence, args[1].as.i);
            *result = runtime_str_to_interp(field);
            return 1;
        }
        *result = val_str_lit("");
        return 1;
    }

    if (strcmp(proc_name, "latitude") == 0) {
        if (arg_count > 0 && args[0].kind == VAL_STR) {
            forge_str_t sentence = interp_str_to_runtime(&args[0]);
            double lat = forge_nmea_latitude(sentence);
            *result = val_float(lat);
            return 1;
        }
        *result = val_float(0.0);
        return 1;
    }

    if (strcmp(proc_name, "longitude") == 0) {
        if (arg_count > 0 && args[0].kind == VAL_STR) {
            forge_str_t sentence = interp_str_to_runtime(&args[0]);
            double lon = forge_nmea_longitude(sentence);
            *result = val_float(lon);
            return 1;
        }
        *result = val_float(0.0);
        return 1;
    }

    return 0;
}

/* Try to dispatch a builtin function. Returns 1 if handled, 0 if not a builtin */
static int try_builtin(forge_interp_t* interp, const char* name,
                       forge_value_t* args, int arg_count, forge_value_t* result) {

    /* print - variadic */
    if (strcmp(name, "print") == 0) {
        *result = builtin_print(args, arg_count);
        return 1;
    }

    /* str - convert to string */
    if (strcmp(name, "str") == 0) {
        *result = builtin_str(interp, args, arg_count);
        return 1;
    }

    /* len - array/string length */
    if (strcmp(name, "len") == 0) {
        *result = builtin_len(interp, args, arg_count);
        return 1;
    }

    /* append - array append */
    if (strcmp(name, "append") == 0) {
        *result = builtin_append(interp, args, arg_count);
        return 1;
    }

    /* type - get type name */
    if (strcmp(name, "type") == 0) {
        *result = builtin_type(interp, args, arg_count);
        return 1;
    }

    /* Not a builtin */
    return 0;
}

forge_value_t interp_call_proc(forge_interp_t* interp, forge_env_t* env,
                                const char* name, forge_value_t* args, int arg_count) {
    (void)env;

    /* Check for builtin functions first */
    forge_value_t builtin_result;
    if (try_builtin(interp, name, args, arg_count, &builtin_result)) {
        return builtin_result;
    }

    /* Look up user-defined procedure */
    forge_node_t* proc = (forge_node_t*)hashmap_get(interp->procedures, name);
    if (!proc) {
        interp_error(interp, 0, 0, "Undefined procedure '%s'", name);
        return val_none();
    }

    /* Check argument count */
    int param_count = proc->data.proc.param_count;
    if (arg_count != param_count) {
        interp_error(interp, proc->line, proc->column,
                     "Procedure '%s' expects %d arguments, got %d",
                     name, param_count, arg_count);
        return val_none();
    }

    /* Check call depth */
    if (interp->call_depth >= INTERP_MAX_CALL_DEPTH) {
        interp_error(interp, 0, 0, "Maximum call depth exceeded (stack overflow)");
        return val_none();
    }

    /* Push call frame */
    interp->call_stack[interp->call_depth].proc_name = name;
    interp->call_stack[interp->call_depth].filename = NULL; /* TODO: track filename */
    interp->call_stack[interp->call_depth].line = proc->line;
    interp->call_depth++;

    /* Create new environment for procedure body */
    forge_env_t* proc_env = env_create(interp->globals);

    /* Bind parameters to argument values */
    forge_param_t* params = proc->data.proc.params;
    for (int i = 0; i < param_count; i++) {
        forge_value_t arg_copy = val_copy(args[i]);
        env_define(proc_env, params[i].name, arg_copy);
        val_free(&arg_copy);
    }

    /* Execute the procedure body */
    forge_result_t result = interp_exec_block(interp, proc_env, proc->data.proc.body);

    /* Clean up environment */
    env_destroy(proc_env);

    /* Pop call frame */
    interp->call_depth--;

    /* Handle return value */
    if (result.flow == FLOW_RETURN) {
        return result.value;
    }

    /* If no explicit return, return void */
    val_free(&result.value);
    return val_void();
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Program Execution
 * ───────────────────────────────────────────────────────────────────────────── */

void interp_run(forge_interp_t* interp, forge_node_t* program) {
    if (!program || program->kind != NODE_PROGRAM) {
        interp_error(interp, 0, 0, "Expected program node");
        return;
    }

    /* Phase 0: Process imports */
    int import_count = program->data.program.import_count;
    forge_node_t** imports = program->data.program.imports;

    for (int i = 0; i < import_count && !interp->had_error; i++) {
        forge_node_t* import = imports[i];
        if (import->kind != NODE_IMPORT) continue;

        const char* module_path = import->data.import.module_path;
        const char* alias = import->data.import.alias;

        /* Check if module already loaded */
        const char* module_name = alias ? alias : module_path;
        if (hashmap_has(interp->modules, module_name)) {
            continue;  /* Already loaded */
        }

        /* Check for stdlib modules (forge.* namespace) */
        if (strncmp(module_path, "forge.", 6) == 0) {
            /* Stdlib modules are synthetic - just register them */
            forge_module_t* stdlib_mod = forge_malloc(sizeof(forge_module_t));
            stdlib_mod->name = module_path;
            stdlib_mod->filepath = "<stdlib>";
            stdlib_mod->program = NULL;
            stdlib_mod->env = env_create(NULL);
            stdlib_mod->procedures = hashmap_create();
            stdlib_mod->records = hashmap_create();
            stdlib_mod->initialized = 1;  /* Always initialized */
            stdlib_mod->is_stdlib = 1;    /* Mark as stdlib */
            hashmap_set(interp->modules, module_name, stdlib_mod);
            continue;
        }

        /* For now, we just register that this module should exist.
         * Actual file loading requires parser integration.
         * Module can be pre-loaded via interp_load_module() before interp_run(). */
        if (!hashmap_has(interp->modules, module_path)) {
            /* Module not pre-loaded - emit warning but continue */
            /* In a full implementation, we would load the file here */
            interp_error(interp, import->line, import->column,
                         "Module '%s' not found (pre-load modules before interp_run)",
                         module_path);
        }
    }

    if (interp->had_error) return;

    /* Phase 1: Register all declarations */
    int decl_count = program->data.program.decl_count;
    forge_node_t** decls = program->data.program.decls;

    for (int i = 0; i < decl_count && !interp->had_error; i++) {
        forge_node_t* decl = decls[i];

        switch (decl->kind) {
            case NODE_PROC_DECL:
                hashmap_set(interp->procedures, decl->data.proc.name, decl);
                break;
            case NODE_RECORD_DECL:
                hashmap_set(interp->records, decl->data.record.name, decl);
                break;
            case NODE_VAR_DECL:
            case NODE_CONST_DECL: {
                /* Global variable - evaluate and define */
                forge_result_t res = interp_exec_stmt(interp, interp->globals, decl);
                val_free(&res.value);
                break;
            }
            case NODE_CHANNEL_DECL:
                interp_register_channel(interp, decl);
                break;
            case NODE_ON_HANDLER:
                interp_register_handler(interp, decl, NULL);  /* NULL = main module */
                break;
            case NODE_TYPE_ALIAS:
                /* Type aliases are handled at type-check time, not runtime */
                break;
            default:
                interp_error(interp, decl->line, decl->column,
                             "Unexpected declaration type: %s",
                             ast_node_kind_name(decl->kind));
                break;
        }
    }

    if (interp->had_error) return;

    /* Phase 2: Call main() if it exists */
    forge_node_t* main_proc = (forge_node_t*)hashmap_get(interp->procedures, "main");
    if (main_proc) {
        interp_call_proc(interp, interp->globals, "main", NULL, 0);
    }
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Module Loading
 * ───────────────────────────────────────────────────────────────────────────── */

forge_module_t* interp_load_module(forge_interp_t* interp, const char* name,
                                    const char* filepath, forge_node_t* program) {
    if (!program || program->kind != NODE_PROGRAM) {
        interp_error(interp, 0, 0, "Expected program node for module '%s'", name);
        return NULL;
    }

    /* Check if module already loaded */
    if (hashmap_has(interp->modules, name)) {
        return (forge_module_t*)hashmap_get(interp->modules, name);
    }

    /* Create module structure */
    forge_module_t* module = forge_malloc(sizeof(forge_module_t));
    module->name = name;
    module->filepath = filepath;
    module->program = program;
    module->env = env_create(NULL);  /* Module's own global env */
    module->procedures = hashmap_create();
    module->records = hashmap_create();
    module->initialized = 0;

    /* Register the module before processing (to handle circular refs) */
    hashmap_set(interp->modules, name, module);

    /* Save current module context */
    forge_module_t* prev_module = interp->current_module;
    interp->current_module = module;

    /* Process all declarations in the module */
    int decl_count = program->data.program.decl_count;
    forge_node_t** decls = program->data.program.decls;

    for (int i = 0; i < decl_count && !interp->had_error; i++) {
        forge_node_t* decl = decls[i];

        switch (decl->kind) {
            case NODE_PROC_DECL:
                hashmap_set(module->procedures, decl->data.proc.name, decl);
                break;
            case NODE_RECORD_DECL:
                hashmap_set(module->records, decl->data.record.name, decl);
                break;
            case NODE_VAR_DECL:
            case NODE_CONST_DECL: {
                /* Module-level variable - evaluate and define in module env */
                forge_result_t res = interp_exec_stmt(interp, module->env, decl);
                val_free(&res.value);
                break;
            }
            case NODE_CHANNEL_DECL:
                interp_register_channel(interp, decl);
                break;
            case NODE_ON_HANDLER:
                interp_register_handler(interp, decl, module);
                break;
            case NODE_TYPE_ALIAS:
                /* Type aliases are handled at type-check time */
                break;
            default:
                /* Ignore other node types */
                break;
        }
    }

    /* Restore previous module context */
    interp->current_module = prev_module;

    if (interp->had_error) {
        /* Note: module is already in hashmap, leave it there but mark error */
        return NULL;
    }

    return module;
}

forge_module_t* interp_get_module(forge_interp_t* interp, const char* name) {
    return (forge_module_t*)hashmap_get(interp->modules, name);
}

void interp_init_module(forge_interp_t* interp, forge_module_t* module) {
    if (!module || module->initialized) return;

    module->initialized = 1;

    /* Look for init() procedure */
    forge_node_t* init_proc = (forge_node_t*)hashmap_get(module->procedures, "init");
    if (init_proc) {
        /* Save and set module context */
        forge_module_t* prev = interp->current_module;
        interp->current_module = module;

        /* Call init with no arguments */
        interp_call_proc(interp, module->env, "init", NULL, 0);

        interp->current_module = prev;
    }
}

forge_node_t* interp_module_get_proc(forge_module_t* module, const char* name) {
    if (!module) return NULL;
    return (forge_node_t*)hashmap_get(module->procedures, name);
}

forge_node_t* interp_module_get_record(forge_module_t* module, const char* name) {
    if (!module) return NULL;
    return (forge_node_t*)hashmap_get(module->records, name);
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Channel System
 * ───────────────────────────────────────────────────────────────────────────── */

void interp_register_channel(forge_interp_t* interp, forge_node_t* decl) {
    if (!decl || decl->kind != NODE_CHANNEL_DECL) return;

    const char* name = decl->data.channel.name;

    /* Check if channel already registered */
    if (hashmap_has(interp->channels, name)) {
        interp_error(interp, decl->line, decl->column,
                     "Channel '%s' already declared", name);
        return;
    }

    /* Create channel structure */
    forge_channel_t* channel = forge_malloc(sizeof(forge_channel_t));
    channel->name = name;
    channel->decl = decl;
    channel->handler_count = 0;

    hashmap_set(interp->channels, name, channel);
}

void interp_register_handler(forge_interp_t* interp, forge_node_t* handler,
                              forge_module_t* module) {
    if (!handler || handler->kind != NODE_ON_HANDLER) return;

    const char* channel_name = handler->data.on_handler.channel_name;

    /* Look up the channel */
    forge_channel_t* channel = (forge_channel_t*)hashmap_get(interp->channels, channel_name);
    if (!channel) {
        interp_error(interp, handler->line, handler->column,
                     "Unknown channel '%s'", channel_name);
        return;
    }

    /* Check handler limit */
    if (channel->handler_count >= INTERP_MAX_HANDLERS) {
        interp_error(interp, handler->line, handler->column,
                     "Too many handlers for channel '%s' (max %d)",
                     channel_name, INTERP_MAX_HANDLERS);
        return;
    }

    /* Add handler */
    channel->handlers[channel->handler_count].handler = handler;
    channel->handlers[channel->handler_count].module = module;
    channel->handler_count++;
}

void interp_emit(forge_interp_t* interp, forge_env_t* env,
                  const char* channel_name, forge_value_t* payload) {
    /* Look up the channel */
    forge_channel_t* channel = (forge_channel_t*)hashmap_get(interp->channels, channel_name);
    if (!channel) {
        interp_error(interp, 0, 0, "Unknown channel '%s'", channel_name);
        return;
    }

    /* Call all handlers synchronously */
    for (int i = 0; i < channel->handler_count && !interp->had_error; i++) {
        forge_handler_entry_t* entry = &channel->handlers[i];
        forge_node_t* handler = entry->handler;
        forge_module_t* handler_module = entry->module;

        /* Determine which environment to use */
        forge_env_t* handler_env;
        if (handler_module) {
            handler_env = env_create(handler_module->env);
        } else {
            handler_env = env_create(env);
        }

        /* Bind payload to parameter if specified */
        const char* param_name = handler->data.on_handler.param_name;
        if (param_name && payload) {
            forge_value_t payload_copy = val_copy(*payload);
            env_define(handler_env, param_name, payload_copy);
            val_free(&payload_copy);
        }

        /* Save module context */
        forge_module_t* prev_module = interp->current_module;
        if (handler_module) {
            interp->current_module = handler_module;
        }

        /* Execute the handler body */
        forge_result_t result = interp_exec_block(interp, handler_env,
                                                   handler->data.on_handler.body);
        val_free(&result.value);

        /* Restore module context */
        interp->current_module = prev_module;

        /* Clean up handler environment */
        env_destroy(handler_env);
    }
}

