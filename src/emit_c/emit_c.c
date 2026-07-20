/*
 * FORGE Language Toolchain
 * emit_c.c - C code emitter implementation
 */

#include "emit_c/emit_c.h"
#include "lexer/lexer.h"
#include <stdarg.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * Emitter Lifecycle
 * ═══════════════════════════════════════════════════════════════════════════ */

forge_emitter_t* emitter_create(FILE* out, forge_arena_t* arena,
                                 forge_strtable_t* strtable,
                                 const char* module_name,
                                 const char* source_file) {
    forge_emitter_t* e = ARENA_ALLOC(arena, forge_emitter_t);
    e->out = out;
    e->indent = 0;
    e->arena = arena;
    e->strtable = strtable;
    e->tmp_counter = 0;
    e->label_counter = 0;
    e->module_name = module_name;
    e->source_file = source_file;
    e->in_loop = 0;
    return e;
}

void emitter_destroy(forge_emitter_t* e) {
    /* Nothing to free - arena owns all memory */
    FORGE_UNUSED(e);
}

void emit_indent(forge_emitter_t* e) {
    for (int i = 0; i < e->indent; i++) {
        fputc(' ', e->out);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Type Emission
 * ═══════════════════════════════════════════════════════════════════════════ */

void emit_type(forge_emitter_t* e, forge_type_t* type) {
    if (!type) {
        EMIT(e, "void");
        return;
    }
    
    switch (type->kind) {
        case TY_INT:    EMIT(e, "int64_t"); break;
        case TY_INT8:   EMIT(e, "int8_t"); break;
        case TY_INT16:  EMIT(e, "int16_t"); break;
        case TY_INT32:  EMIT(e, "int32_t"); break;
        case TY_UINT:   EMIT(e, "uint64_t"); break;
        case TY_UINT8:  EMIT(e, "uint8_t"); break;
        case TY_UINT16: EMIT(e, "uint16_t"); break;
        case TY_UINT32: EMIT(e, "uint32_t"); break;
        case TY_FLOAT:  EMIT(e, "double"); break;
        case TY_FLOAT32:EMIT(e, "float"); break;
        case TY_BOOL:   EMIT(e, "int"); break;
        case TY_BYTE:   EMIT(e, "uint8_t"); break;
        case TY_STR:    EMIT(e, "forge_str_t"); break;
        case TY_NONE:   EMIT(e, "void"); break;
        case TY_VOID:   EMIT(e, "void"); break;
        
        case TY_OPTIONAL:
            /* Use named typedefs for type compatibility across functions */
            if (type->as.optional.inner) {
                switch (type->as.optional.inner->kind) {
                    case TY_INT:    EMIT(e, "forge_opt_int_t"); break;
                    case TY_FLOAT:  EMIT(e, "forge_opt_float_t"); break;
                    case TY_BOOL:   EMIT(e, "forge_opt_bool_t"); break;
                    case TY_STR:    EMIT(e, "forge_opt_str_t"); break;
                    case TY_RECORD:
                        EMIT(e, "forge_opt_%s_t", type->as.optional.inner->as.record.name);
                        break;
                    default:
                        /* Fallback to anonymous struct for unusual types */
                        EMIT(e, "FORGE_OPTIONAL(");
                        emit_type(e, type->as.optional.inner);
                        EMIT(e, ")");
                        break;
                }
            } else {
                EMIT(e, "FORGE_OPTIONAL(int64_t)");
            }
            break;
            
        case TY_FIXED_ARRAY:
        case TY_DYN_ARRAY:
            EMIT(e, "forge_array_t");
            break;
            
        case TY_MAP:
            EMIT(e, "forge_map_t*");
            break;
            
        case TY_RECORD:
            EMIT(e, "%s_t", type->as.record.name);
            break;

        case TY_ALIAS:
            /* Follow alias to its target type */
            emit_type(e, type->as.alias.target);
            break;

        case TY_UNRESOLVED:
            /* Should be resolved by now */
            EMIT(e, "/* UNRESOLVED: %s */ void", type->as.unresolved.name);
            break;

        case TY_ERROR:
            EMIT(e, "/* ERROR TYPE */ void");
            break;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Forward Declarations
 * ═══════════════════════════════════════════════════════════════════════════ */

static void emit_decl(forge_emitter_t* e, forge_node_t* node);
static void emit_block(forge_emitter_t* e, forge_node_t* block);

/* ═══════════════════════════════════════════════════════════════════════════
 * Expression Emission
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Helper to emit escaped string for C */
static void emit_escaped_string(forge_emitter_t* e, const char* str) {
    fputc('"', e->out);
    for (const char* p = str; *p; p++) {
        switch (*p) {
            case '\n': fputs("\\n", e->out); break;
            case '\t': fputs("\\t", e->out); break;
            case '\r': fputs("\\r", e->out); break;
            case '\\': fputs("\\\\", e->out); break;
            case '"':  fputs("\\\"", e->out); break;
            default:   fputc(*p, e->out); break;
        }
    }
    fputc('"', e->out);
}

/* Format a double as a C floating-point literal, guaranteeing the text
 * contains a '.' or exponent marker. "%g" alone prints whole-number
 * values like 1.0 or 16.0 as "1"/"16", which C then parses as *integer*
 * literals — turning constants like `1.0 / 16.0` into integer division
 * (result 0) instead of the intended float division. */
static void emit_float_literal(forge_emitter_t* e, double val) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%.17g", val);
    int needs_suffix = 1;
    for (const char* p = buf; *p; p++) {
        if (*p == '.' || *p == 'e' || *p == 'E' ||
            *p == 'n' || *p == 'N' /* nan/inf */) {
            needs_suffix = 0;
            break;
        }
    }
    EMIT(e, "%s%s", buf, needs_suffix ? ".0" : "");
}

static void emit_literal(forge_emitter_t* e, forge_node_t* node) {
    switch (node->kind) {
        case NODE_INT_LIT:
            EMIT(e, "%lldLL", node->data.int_val);
            break;
        case NODE_FLOAT_LIT:
            emit_float_literal(e, node->data.float_val);
            break;
        case NODE_STR_LIT:
            EMIT(e, "forge_str_lit(");
            emit_escaped_string(e, node->data.str_val);
            EMIT(e, ")");
            break;
        case NODE_BOOL_LIT:
            EMIT(e, "%d", node->data.bool_val ? 1 : 0);
            break;
        case NODE_NONE_LIT:
            /* none -> (OptType){ .present = 0 } using named typedef */
            if (node->resolved_type && node->resolved_type->kind == TY_OPTIONAL) {
                EMIT(e, "((");
                emit_type(e, node->resolved_type);
                EMIT(e, "){ .present = 0 })");
            } else {
                EMIT(e, "((forge_opt_int_t){ .present = 0 })");
            }
            break;
        default:
            EMIT(e, "/* unknown literal */");
            break;
    }
}

static void emit_binop(forge_emitter_t* e, forge_node_t* node);
static void emit_unop(forge_emitter_t* e, forge_node_t* node);
static void emit_call(forge_emitter_t* e, forge_node_t* node);
static void emit_field_access(forge_emitter_t* e, forge_node_t* node);
static void emit_index(forge_emitter_t* e, forge_node_t* node);

void emit_expr(forge_emitter_t* e, forge_node_t* node) {
    if (!node) {
        EMIT(e, "/* null expr */");
        return;
    }
    
    switch (node->kind) {
        case NODE_INT_LIT:
        case NODE_FLOAT_LIT:
        case NODE_STR_LIT:
        case NODE_BOOL_LIT:
        case NODE_NONE_LIT:
            emit_literal(e, node);
            break;
            
        case NODE_IDENT:
            EMIT(e, "%s", node->data.name);
            break;

        case NODE_BINARY_OP:
            emit_binop(e, node);
            break;

        case NODE_UNARY_OP:
            emit_unop(e, node);
            break;

        case NODE_CALL:
            emit_call(e, node);
            break;

        case NODE_FIELD_ACCESS:
            emit_field_access(e, node);
            break;

        case NODE_INDEX:
            emit_index(e, node);
            break;

        case NODE_ARRAY_LITERAL: {
            /* Array literal: [a, b, c] -> forge_array_from_XXX((T[]){a,b,c}, count) */
            int count = node->data.array_lit.count;
            forge_type_t* elem_type = NULL;

            /* Get element type from resolved type */
            if (node->resolved_type) {
                if (node->resolved_type->kind == TY_DYN_ARRAY) {
                    elem_type = node->resolved_type->as.dyn_array.elem_type;
                } else if (node->resolved_type->kind == TY_FIXED_ARRAY) {
                    elem_type = node->resolved_type->as.fixed_array.elem_type;
                }
            }

            if (count == 0) {
                /* Empty array */
                EMIT(e, "forge_array_create(sizeof(int64_t), 1)");
            } else if (elem_type && elem_type->kind == TY_INT) {
                EMIT(e, "forge_array_from_ints((int64_t[]){");
                for (int i = 0; i < count; i++) {
                    if (i > 0) EMIT(e, ", ");
                    emit_expr(e, node->data.array_lit.elements[i]);
                }
                EMIT(e, "}, %d)", count);
            } else if (elem_type && elem_type->kind == TY_FLOAT) {
                EMIT(e, "forge_array_from_floats((double[]){");
                for (int i = 0; i < count; i++) {
                    if (i > 0) EMIT(e, ", ");
                    emit_expr(e, node->data.array_lit.elements[i]);
                }
                EMIT(e, "}, %d)", count);
            } else if (elem_type && elem_type->kind == TY_BOOL) {
                EMIT(e, "forge_array_from_bools((int[]){");
                for (int i = 0; i < count; i++) {
                    if (i > 0) EMIT(e, ", ");
                    emit_expr(e, node->data.array_lit.elements[i]);
                }
                EMIT(e, "}, %d)", count);
            } else if (elem_type && elem_type->kind == TY_STR) {
                EMIT(e, "forge_array_from_strs((forge_str_t[]){");
                for (int i = 0; i < count; i++) {
                    if (i > 0) EMIT(e, ", ");
                    emit_expr(e, node->data.array_lit.elements[i]);
                }
                EMIT(e, "}, %d)", count);
            } else if (elem_type && (elem_type->kind == TY_DYN_ARRAY || elem_type->kind == TY_FIXED_ARRAY)) {
                EMIT(e, "forge_array_from_arrays((forge_array_t[]){");
                for (int i = 0; i < count; i++) {
                    if (i > 0) EMIT(e, ", ");
                    emit_expr(e, node->data.array_lit.elements[i]);
                }
                EMIT(e, "}, %d)", count);
            } else {
                /* Fallback - assume int */
                EMIT(e, "forge_array_from_ints((int64_t[]){");
                for (int i = 0; i < count; i++) {
                    if (i > 0) EMIT(e, ", ");
                    emit_expr(e, node->data.array_lit.elements[i]);
                }
                EMIT(e, "}, %d)", count);
            }
            break;
        }

        case NODE_RECORD_LITERAL:
            EMIT(e, "((%s_t){", node->data.record_lit.type_name);
            for (int i = 0; i < node->data.record_lit.field_count; i++) {
                if (i > 0) EMIT(e, ", ");
                EMIT(e, ".%s = ", node->data.record_lit.fields[i].name);
                emit_expr(e, node->data.record_lit.fields[i].value);
            }
            EMIT(e, "})");
            break;

        case NODE_SOME:
            /* some(expr) -> (OptType){ .present = 1, .value = (expr) } */
            EMIT(e, "((");
            if (node->resolved_type && node->resolved_type->kind == TY_OPTIONAL) {
                emit_type(e, node->resolved_type);
            } else {
                EMIT(e, "forge_opt_int_t");
            }
            EMIT(e, "){ .present = 1, .value = (");
            emit_expr(e, node->data.some.expr);
            EMIT(e, ") })");
            break;

        case NODE_OR_ELSE:
            EMIT(e, "(");
            emit_expr(e, node->data.or_else.optional_expr);
            EMIT(e, ".present ? ");
            emit_expr(e, node->data.or_else.optional_expr);
            EMIT(e, ".value : ");
            emit_expr(e, node->data.or_else.fallback);
            EMIT(e, ")");
            break;

        case NODE_IS_SOME:
            EMIT(e, "(");
            emit_expr(e, node->data.is_check.expr);
            EMIT(e, ".present != 0)");
            break;

        case NODE_IS_NONE:
            EMIT(e, "(");
            emit_expr(e, node->data.is_check.expr);
            EMIT(e, ".present == 0)");
            break;

        case NODE_CAST: {
            forge_type_t* target_type = node->resolved_type;
            forge_node_t* inner = node->data.cast.expr;
            forge_type_t* source_type = inner->resolved_type;

            /* Special handling for conversions to string */
            if (target_type && target_type->kind == TY_STR) {
                if (source_type) {
                    switch (source_type->kind) {
                        case TY_INT:
                        case TY_INT8: case TY_INT16: case TY_INT32:
                        case TY_UINT: case TY_UINT8: case TY_UINT16: case TY_UINT32:
                            EMIT(e, "forge_str_from_int(");
                            emit_expr(e, inner);
                            EMIT(e, ")");
                            break;
                        case TY_FLOAT: case TY_FLOAT32:
                            EMIT(e, "forge_str_from_float(");
                            emit_expr(e, inner);
                            EMIT(e, ")");
                            break;
                        case TY_BOOL:
                            EMIT(e, "forge_str_from_bool(");
                            emit_expr(e, inner);
                            EMIT(e, ")");
                            break;
                        case TY_STR:
                            /* str -> str is a no-op */
                            emit_expr(e, inner);
                            break;
                        case TY_BYTE:
                            /* byte -> str builds a real length-1 string
                             * (a raw cast to forge_str_t is invalid C since
                             * forge_str_t is a struct, not a scalar). */
                            EMIT(e, "forge_str_from_char(");
                            emit_expr(e, inner);
                            EMIT(e, ")");
                            break;
                        default:
                            /* Fallback: try to cast */
                            EMIT(e, "((forge_str_t)(");
                            emit_expr(e, inner);
                            EMIT(e, "))");
                            break;
                    }
                } else {
                    /* No source type info, just emit */
                    emit_expr(e, inner);
                }
            }
            /* Special handling for int casts */
            else if (target_type && (target_type->kind == TY_INT ||
                     target_type->kind == TY_INT8 || target_type->kind == TY_INT16 ||
                     target_type->kind == TY_INT32)) {
                if (source_type && source_type->kind == TY_STR) {
                    EMIT(e, "forge_str_to_int(");
                    emit_expr(e, inner);
                    EMIT(e, ")");
                } else if (source_type && source_type->kind == TY_BYTE) {
                    /* A byte here represents a digit character (the only
                     * source of TY_BYTE is indexing a str); parse its
                     * decimal digit value rather than widening the raw
                     * ASCII code point, matching the interpreter's
                     * strtoll-on-length-1-string behavior. */
                    EMIT(e, "forge_char_to_digit(");
                    emit_expr(e, inner);
                    EMIT(e, ")");
                } else {
                    EMIT(e, "((int64_t)(");
                    emit_expr(e, inner);
                    EMIT(e, "))");
                }
            }
            /* Special handling for float casts */
            else if (target_type && (target_type->kind == TY_FLOAT || target_type->kind == TY_FLOAT32)) {
                if (source_type && source_type->kind == TY_STR) {
                    EMIT(e, "forge_str_to_float(");
                    emit_expr(e, inner);
                    EMIT(e, ")");
                } else {
                    EMIT(e, "((double)(");
                    emit_expr(e, inner);
                    EMIT(e, "))");
                }
            }
            /* Default: standard C cast */
            else {
                EMIT(e, "((");
                emit_type(e, target_type);
                EMIT(e, ")(");
                emit_expr(e, inner);
                EMIT(e, "))");
            }
            break;
        }

        default:
            EMIT(e, "/* TODO: expr kind %d */", node->kind);
            break;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Binary and Unary Operators
 * ═══════════════════════════════════════════════════════════════════════════ */

static void emit_binop(forge_emitter_t* e, forge_node_t* node) {
    int op = node->data.binop.op;
    forge_node_t* left = node->data.binop.left;
    forge_node_t* right = node->data.binop.right;

    /* Special case: string concatenation */
    if (op == TOK_PLUS && left->resolved_type && left->resolved_type->kind == TY_STR) {
        EMIT(e, "forge_str_concat(");
        emit_expr(e, left);
        EMIT(e, ", ");
        emit_expr(e, right);
        EMIT(e, ")");
        return;
    }

    /* Special case: string comparison */
    if ((op == TOK_EQ || op == TOK_NEQ) && left->resolved_type && left->resolved_type->kind == TY_STR) {
        if (op == TOK_EQ) {
            EMIT(e, "forge_str_equal(");
        } else {
            EMIT(e, "(!forge_str_equal(");
        }
        emit_expr(e, left);
        EMIT(e, ", ");
        emit_expr(e, right);
        if (op == TOK_EQ) {
            EMIT(e, ")");
        } else {
            EMIT(e, "))");
        }
        return;
    }

    /* Integer division/modulo: emit runtime-checked versions */
    int is_int_div = (op == TOK_SLASH || op == TOK_PERCENT) &&
                     left->resolved_type && type_is_integer(left->resolved_type);
    if (is_int_div) {
        EMIT(e, "%s(", op == TOK_SLASH ? "forge_div_check" : "forge_mod_check");
        emit_expr(e, left);
        EMIT(e, ", ");
        emit_expr(e, right);
        EMIT(e, ", \"%s\", %d)", e->source_file ? e->source_file : "unknown", node->line);
        return;
    }

    /* Standard binary operators */
    EMIT(e, "(");
    emit_expr(e, left);

    switch (op) {
        case TOK_PLUS:    EMIT(e, " + ");  break;
        case TOK_MINUS:   EMIT(e, " - ");  break;
        case TOK_STAR:    EMIT(e, " * ");  break;
        case TOK_SLASH:   EMIT(e, " / ");  break;
        case TOK_PERCENT: EMIT(e, " %% "); break;
        case TOK_EQ:      EMIT(e, " == "); break;
        case TOK_NEQ:     EMIT(e, " != "); break;
        case TOK_LT:      EMIT(e, " < ");  break;
        case TOK_GT:      EMIT(e, " > ");  break;
        case TOK_LEQ:     EMIT(e, " <= "); break;
        case TOK_GEQ:     EMIT(e, " >= "); break;
        case TOK_AND:     EMIT(e, " && "); break;
        case TOK_OR:      EMIT(e, " || "); break;
        case TOK_AMP:     EMIT(e, " & ");  break;
        case TOK_PIPE:    EMIT(e, " | ");  break;
        case TOK_CARET:   EMIT(e, " ^ ");  break;
        case TOK_LSHIFT:  EMIT(e, " << "); break;
        case TOK_RSHIFT:  EMIT(e, " >> "); break;
        default:          EMIT(e, " /* unknown op %d */ ", op); break;
    }

    emit_expr(e, right);
    EMIT(e, ")");
}

static void emit_unop(forge_emitter_t* e, forge_node_t* node) {
    int op = node->data.unop.op;
    forge_node_t* operand = node->data.unop.operand;

    switch (op) {
        case TOK_MINUS: EMIT(e, "(-"); break;
        case TOK_NOT:   EMIT(e, "(!");  break;
        case TOK_TILDE: EMIT(e, "(~"); break;
        default:        EMIT(e, "(/* unknown unop %d */", op); break;
    }

    emit_expr(e, operand);
    EMIT(e, ")");
}

/* Helper to check if a callee is a stdlib call (forge.*.func) */
static int is_stdlib_call(forge_node_t* callee, const char** module_name, const char** func_name) {
    if (callee->kind != NODE_FIELD_ACCESS) return 0;

    forge_node_t* inner = callee->data.field_access.object;
    if (inner->kind != NODE_FIELD_ACCESS) return 0;

    forge_node_t* root = inner->data.field_access.object;
    if (root->kind != NODE_IDENT || strcmp(root->data.name, "forge") != 0) return 0;

    *module_name = inner->data.field_access.field_name;
    *func_name = callee->data.field_access.field_name;
    return 1;
}

static void emit_call(forge_emitter_t* e, forge_node_t* node) {
    forge_node_t* callee = node->data.call.callee;

    /* Check for stdlib calls (forge.*.func) */
    const char* module_name = NULL;
    const char* func_name = NULL;
    if (is_stdlib_call(callee, &module_name, &func_name)) {
        /* Special handling for forge.time functions that return ints (simple versions) */
        if (strcmp(module_name, "time") == 0 && strcmp(func_name, "start_clock") == 0) {
            EMIT(e, "forge_time_start_clock_simple()");
            return;
        }
        if (strcmp(module_name, "time") == 0 && strcmp(func_name, "lap") == 0) {
            EMIT(e, "forge_time_lap_simple(");
            emit_expr(e, node->data.call.args[0]);
            EMIT(e, ")");
            return;
        }

        /* Map forge.io.* to forge_io_* */
        EMIT(e, "forge_%s_%s(", module_name, func_name);
        for (int i = 0; i < node->data.call.arg_count; i++) {
            if (i > 0) EMIT(e, ", ");
            /* forge_str_join takes array by pointer */
            if (strcmp(module_name, "str") == 0 && strcmp(func_name, "join") == 0 && i == 0) {
                EMIT(e, "&");
            }
            emit_expr(e, node->data.call.args[i]);
        }
        EMIT(e, ")");
        return;
    }

    /* Check for builtin functions */
    if (callee->kind == NODE_IDENT) {
        const char* name = callee->data.name;

        /* swap(a, b) builtin — emit an inline block-scoped swap */
        if (strcmp(name, "swap") == 0 && node->data.call.arg_count == 2) {
            forge_node_t* a = node->data.call.args[0];
            forge_node_t* b = node->data.call.args[1];
            forge_type_t* t = a->resolved_type;
            EMIT(e, "do { ");
            emit_type(e, t);
            EMIT(e, " _forge_swap_tmp = ");
            emit_expr(e, a);
            EMIT(e, "; ");
            emit_expr(e, a);
            EMIT(e, " = ");
            emit_expr(e, b);
            EMIT(e, "; ");
            emit_expr(e, b);
            EMIT(e, " = _forge_swap_tmp; } while(0)");
            return;
        }

        /* print builtin */
        if (strcmp(name, "print") == 0) {
            EMIT(e, "forge_print(");
            if (node->data.call.arg_count > 0) {
                forge_node_t* arg = node->data.call.args[0];
                /* Auto-convert to string if needed */
                if (arg->resolved_type && arg->resolved_type->kind != TY_STR) {
                    switch (arg->resolved_type->kind) {
                        case TY_INT:
                        case TY_INT8: case TY_INT16: case TY_INT32:
                        case TY_UINT: case TY_UINT8: case TY_UINT16: case TY_UINT32:
                            EMIT(e, "forge_str_from_int(");
                            emit_expr(e, arg);
                            EMIT(e, ")");
                            break;
                        case TY_FLOAT: case TY_FLOAT32:
                            EMIT(e, "forge_str_from_float(");
                            emit_expr(e, arg);
                            EMIT(e, ")");
                            break;
                        case TY_BOOL:
                            EMIT(e, "forge_str_from_bool(");
                            emit_expr(e, arg);
                            EMIT(e, ")");
                            break;
                        default:
                            emit_expr(e, arg);
                            break;
                    }
                } else {
                    emit_expr(e, arg);
                }
            }
            EMIT(e, ")");
            return;
        }

        /* str() conversion */
        if (strcmp(name, "str") == 0 && node->data.call.arg_count > 0) {
            forge_node_t* arg = node->data.call.args[0];
            if (arg->resolved_type) {
                switch (arg->resolved_type->kind) {
                    case TY_INT:
                    case TY_INT8: case TY_INT16: case TY_INT32:
                    case TY_UINT: case TY_UINT8: case TY_UINT16: case TY_UINT32:
                        EMIT(e, "forge_str_from_int(");
                        emit_expr(e, arg);
                        EMIT(e, ")");
                        return;
                    case TY_FLOAT: case TY_FLOAT32:
                        EMIT(e, "forge_str_from_float(");
                        emit_expr(e, arg);
                        EMIT(e, ")");
                        return;
                    case TY_BOOL:
                        EMIT(e, "forge_str_from_bool(");
                        emit_expr(e, arg);
                        EMIT(e, ")");
                        return;
                    default:
                        break;
                }
            }
        }

        /* len() builtin */
        if (strcmp(name, "len") == 0 && node->data.call.arg_count > 0) {
            forge_node_t* arg = node->data.call.args[0];
            if (arg->resolved_type) {
                if (arg->resolved_type->kind == TY_STR) {
                    EMIT(e, "(");
                    emit_expr(e, arg);
                    EMIT(e, ".len)");
                    return;
                } else if (arg->resolved_type->kind == TY_DYN_ARRAY ||
                           arg->resolved_type->kind == TY_FIXED_ARRAY) {
                    EMIT(e, "forge_array_len(&");
                    emit_expr(e, arg);
                    EMIT(e, ")");
                    return;
                }
            }
        }

        /* append() builtin */
        if (strcmp(name, "append") == 0 && node->data.call.arg_count == 2) {
            /* Use compound literal to ensure the value is an lvalue for &:
             * (forge_array_push(&arr, &(Type){value}), arr) */
            forge_type_t* elem_type = NULL;
            forge_node_t* arr_arg = node->data.call.args[0];
            if (arr_arg->resolved_type) {
                if (arr_arg->resolved_type->kind == TY_DYN_ARRAY) {
                    elem_type = arr_arg->resolved_type->as.dyn_array.elem_type;
                } else if (arr_arg->resolved_type->kind == TY_FIXED_ARRAY) {
                    elem_type = arr_arg->resolved_type->as.fixed_array.elem_type;
                }
            }
            EMIT(e, "(forge_array_push(&");
            emit_expr(e, node->data.call.args[0]);
            EMIT(e, ", &(");
            emit_type(e, elem_type);
            EMIT(e, "){");
            emit_expr(e, node->data.call.args[1]);
            EMIT(e, "}), ");
            emit_expr(e, node->data.call.args[0]);
            EMIT(e, ")");
            return;
        }

        /* Regular function call with forge_ prefix */
        EMIT(e, "forge_%s(", name);
    } else {
        /* Complex callee (e.g., method call) */
        emit_expr(e, callee);
        EMIT(e, "(");
    }

    /* Emit arguments */
    for (int i = 0; i < node->data.call.arg_count; i++) {
        if (i > 0) EMIT(e, ", ");
        emit_expr(e, node->data.call.args[i]);
    }
    EMIT(e, ")");
}

static void emit_field_access(forge_emitter_t* e, forge_node_t* node) {
    /* Check for forge.math constants (PI, E, TAU) */
    forge_node_t* inner = node->data.field_access.object;
    const char* field = node->data.field_access.field_name;

    if (inner && inner->kind == NODE_FIELD_ACCESS) {
        forge_node_t* root = inner->data.field_access.object;
        const char* mod = inner->data.field_access.field_name;
        if (root && root->kind == NODE_IDENT &&
            strcmp(root->data.name, "forge") == 0 &&
            strcmp(mod, "math") == 0) {
            /* Handle forge.math.PI, forge.math.E, forge.math.TAU */
            if (strcmp(field, "PI") == 0) {
                EMIT(e, "FORGE_MATH_PI");
                return;
            } else if (strcmp(field, "E") == 0) {
                EMIT(e, "FORGE_MATH_E");
                return;
            } else if (strcmp(field, "TAU") == 0) {
                EMIT(e, "FORGE_MATH_TAU");
                return;
            }
        }
    }

    emit_expr(e, inner);
    EMIT(e, ".%s", field);
}

static void emit_index(forge_emitter_t* e, forge_node_t* node) {
    forge_node_t* object = node->data.index.object;
    forge_node_t* idx = node->data.index.index;

    if (object->resolved_type) {
        if (object->resolved_type->kind == TY_STR) {
            /* String indexing returns char as int */
            EMIT(e, "((int)(");
            emit_expr(e, object);
            EMIT(e, ".data[");
            emit_expr(e, idx);
            EMIT(e, "]))");
            return;
        } else if (object->resolved_type->kind == TY_DYN_ARRAY ||
                   object->resolved_type->kind == TY_FIXED_ARRAY) {
            /* Array indexing */
            EMIT(e, "(*(");
            emit_type(e, object->resolved_type->as.dyn_array.elem_type);
            EMIT(e, "*)forge_array_get(&");
            emit_expr(e, object);
            EMIT(e, ", ");
            emit_expr(e, idx);
            EMIT(e, "))");
            return;
        }
    }

    /* Fallback */
    emit_expr(e, object);
    EMIT(e, "[");
    emit_expr(e, idx);
    EMIT(e, "]");
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Statement Emission
 * ═══════════════════════════════════════════════════════════════════════════ */

static void emit_block(forge_emitter_t* e, forge_node_t* block) {
    if (!block || block->kind != NODE_BLOCK) return;

    EMITLN(e, "{");
    PUSH_INDENT(e);

    for (int i = 0; i < block->data.block.count; i++) {
        emit_stmt(e, block->data.block.stmts[i]);
    }

    POP_INDENT(e);
    INDENT(e);
    EMITLN(e, "}");
}

void emit_stmt(forge_emitter_t* e, forge_node_t* node) {
    if (!node) return;

    switch (node->kind) {
        case NODE_VAR_DECL:
        case NODE_CONST_DECL:
            INDENT(e);
            emit_type(e, node->resolved_type);
            EMIT(e, " %s", node->data.var_decl.name);
            if (node->data.var_decl.init_expr) {
                forge_node_t* init = node->data.var_decl.init_expr;
                /* Special handling for optional initialization to avoid compound literal issues */
                if (node->resolved_type && node->resolved_type->kind == TY_OPTIONAL) {
                    if (init->kind == NODE_SOME) {
                        EMIT(e, " = FORGE_SOME_VAL(");
                        emit_expr(e, init->data.some.expr);
                        EMIT(e, ")");
                    } else if (init->kind == NODE_NONE_LIT) {
                        EMIT(e, " = FORGE_NONE_VAL");
                    } else {
                        /* Other expression (variable, function call, etc.) */
                        EMIT(e, " = ");
                        emit_expr(e, init);
                    }
                } else {
                    EMIT(e, " = ");
                    emit_expr(e, init);
                }
            }
            EMITLN(e, ";");
            break;

        case NODE_ASSIGN:
            INDENT(e);
            emit_expr(e, node->data.assign.target);
            EMIT(e, " = ");
            emit_expr(e, node->data.assign.value);
            EMITLN(e, ";");
            break;

        case NODE_COMPOUND_ASSIGN: {
            INDENT(e);
            emit_expr(e, node->data.compound_assign.target);
            int op = node->data.compound_assign.op;
            switch (op) {
                case TOK_PLUS_EQ:    EMIT(e, " += "); break;
                case TOK_MINUS_EQ:   EMIT(e, " -= "); break;
                case TOK_STAR_EQ:    EMIT(e, " *= "); break;
                case TOK_SLASH_EQ:   EMIT(e, " /= "); break;
                case TOK_PERCENT_EQ: EMIT(e, " %%= "); break;
                case TOK_AMP_EQ:     EMIT(e, " &= ");  break;
                case TOK_PIPE_EQ:    EMIT(e, " |= ");  break;
                case TOK_CARET_EQ:   EMIT(e, " ^= ");  break;
                case TOK_LSHIFT_EQ:  EMIT(e, " <<= "); break;
                case TOK_RSHIFT_EQ:  EMIT(e, " >>= "); break;
                default:             EMIT(e, " /* op %d */ ", op); break;
            }
            emit_expr(e, node->data.compound_assign.value);
            EMITLN(e, ";");
            break;
        }

        case NODE_IF:
            INDENT(e);
            EMIT(e, "if (");
            emit_expr(e, node->data.if_stmt.condition);
            EMIT(e, ") ");
            emit_block(e, node->data.if_stmt.then_body);

            /* Handle elif chain */
            for (int i = 0; i < node->data.if_stmt.elif_count; i++) {
                INDENT(e);
                EMIT(e, "else if (");
                emit_expr(e, node->data.if_stmt.elif_conditions[i]);
                EMIT(e, ") ");
                emit_block(e, node->data.if_stmt.elif_bodies[i]);
            }

            if (node->data.if_stmt.else_body) {
                INDENT(e);
                EMIT(e, "else ");
                emit_block(e, node->data.if_stmt.else_body);
            }
            break;

        case NODE_WHILE:
            INDENT(e);
            EMIT(e, "while (");
            emit_expr(e, node->data.while_stmt.condition);
            EMIT(e, ") ");
            e->in_loop++;
            emit_block(e, node->data.while_stmt.body);
            e->in_loop--;
            break;

        case NODE_FOR:
            INDENT(e);
            /* For over range */
            if (node->data.for_stmt.iterable->kind == NODE_RANGE) {
                forge_node_t* range = node->data.for_stmt.iterable;
                EMIT(e, "for (int64_t %s = ", node->data.for_stmt.var_name);
                emit_expr(e, range->data.range.start);
                EMIT(e, "; %s %s ", node->data.for_stmt.var_name,
                     range->data.range.inclusive ? "<=" : "<");
                emit_expr(e, range->data.range.end);
                EMIT(e, "; %s++) ", node->data.for_stmt.var_name);
            } else {
                /* For over array - generate index-based loop */
                int idx = e->tmp_counter++;
                EMIT(e, "for (int _idx%d = 0; _idx%d < forge_array_len(&", idx, idx);
                emit_expr(e, node->data.for_stmt.iterable);
                EMITLN(e, "); _idx%d++) {", idx);
                PUSH_INDENT(e);
                INDENT(e);

                /* Get element type from iterable's resolved type */
                forge_type_t* elem_type = NULL;
                forge_node_t* iterable = node->data.for_stmt.iterable;
                if (iterable->resolved_type) {
                    if (iterable->resolved_type->kind == TY_DYN_ARRAY) {
                        elem_type = iterable->resolved_type->as.dyn_array.elem_type;
                    } else if (iterable->resolved_type->kind == TY_FIXED_ARRAY) {
                        elem_type = iterable->resolved_type->as.fixed_array.elem_type;
                    }
                }

                emit_type(e, elem_type);
                EMIT(e, " %s = *(", node->data.for_stmt.var_name);
                emit_type(e, elem_type);
                EMIT(e, "*)forge_array_get(&");
                emit_expr(e, node->data.for_stmt.iterable);
                EMITLN(e, ", _idx%d);", idx);

                /* Emit body statements directly (not as block) */
                if (node->data.for_stmt.body && node->data.for_stmt.body->kind == NODE_BLOCK) {
                    e->in_loop++;
                    for (int i = 0; i < node->data.for_stmt.body->data.block.count; i++) {
                        emit_stmt(e, node->data.for_stmt.body->data.block.stmts[i]);
                    }
                    e->in_loop--;
                }

                POP_INDENT(e);
                INDENT(e);
                EMITLN(e, "}");
                return;
            }
            e->in_loop++;
            emit_block(e, node->data.for_stmt.body);
            e->in_loop--;
            break;

        case NODE_LOOP:
            INDENT(e);
            EMIT(e, "while (1) ");
            e->in_loop++;
            emit_block(e, node->data.loop_stmt.body);
            e->in_loop--;
            break;

        case NODE_RETURN:
            INDENT(e);
            if (node->data.return_stmt.value) {
                EMIT(e, "return ");
                emit_expr(e, node->data.return_stmt.value);
                EMITLN(e, ";");
            } else {
                EMITLN(e, "return;");
            }
            break;

        case NODE_BREAK:
            INDENT(e);
            EMITLN(e, "break;");
            break;

        case NODE_CONTINUE:
            INDENT(e);
            EMITLN(e, "continue;");
            break;

        case NODE_EMIT:
            INDENT(e);
            EMIT(e, "forge_emit_%s(", node->data.emit_stmt.channel_name);
            if (node->data.emit_stmt.payload) {
                emit_expr(e, node->data.emit_stmt.payload);
            }
            EMITLN(e, ");");
            break;

        case NODE_BLOCK:
            emit_block(e, node);
            break;

        case NODE_CALL:
            /* Expression statement (function call) */
            INDENT(e);
            emit_expr(e, node);
            EMITLN(e, ";");
            break;

        case NODE_EXPR_STMT:
            /* Expression statement wrapper */
            INDENT(e);
            emit_expr(e, node->data.expr_stmt.expr);
            EMITLN(e, ";");
            break;

        case NODE_WITH_ALLOC: {
            /* with alloc(Type, Size) as name: -> scoped forge_array_t */
            forge_type_t* elem_type = NULL;
            if (node->data.with_alloc.type_expr && node->data.with_alloc.type_expr->resolved_type) {
                elem_type = node->data.with_alloc.type_expr->resolved_type;
            }

            INDENT(e);
            EMITLN(e, "{");
            PUSH_INDENT(e);

            /* Declare the array variable */
            INDENT(e);
            EMIT(e, "forge_array_t %s = forge_array_create(sizeof(", node->data.with_alloc.var_name);
            emit_type(e, elem_type);
            EMIT(e, "), ");
            if (node->data.with_alloc.size_expr) {
                emit_expr(e, node->data.with_alloc.size_expr);
            } else {
                EMIT(e, "0");
            }
            EMITLN(e, ");");

            /* Set length and zero-initialize */
            if (node->data.with_alloc.size_expr) {
                INDENT(e);
                EMIT(e, "%s.len = ", node->data.with_alloc.var_name);
                emit_expr(e, node->data.with_alloc.size_expr);
                EMITLN(e, ";");
                INDENT(e);
                EMIT(e, "memset(%s.data, 0, ", node->data.with_alloc.var_name);
                emit_expr(e, node->data.with_alloc.size_expr);
                EMIT(e, " * sizeof(");
                emit_type(e, elem_type);
                EMITLN(e, "));");
            }

            /* Emit body statements */
            if (node->data.with_alloc.body && node->data.with_alloc.body->kind == NODE_BLOCK) {
                for (int i = 0; i < node->data.with_alloc.body->data.block.count; i++) {
                    emit_stmt(e, node->data.with_alloc.body->data.block.stmts[i]);
                }
            }

            /* Free the array */
            INDENT(e);
            EMITLN(e, "forge_array_free(&%s);", node->data.with_alloc.var_name);

            POP_INDENT(e);
            INDENT(e);
            EMITLN(e, "}");
            break;
        }

        default:
            INDENT(e);
            EMITLN(e, "/* TODO: stmt kind %d */", node->kind);
            break;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Declaration Emission
 * ═══════════════════════════════════════════════════════════════════════════ */

static void emit_record_decl(forge_emitter_t* e, forge_node_t* node) {
    /* Use the named struct to match forward declaration */
    EMITLN(e, "struct %s_s {", node->data.record.name);
    PUSH_INDENT(e);

    for (int i = 0; i < node->data.record.field_count; i++) {
        forge_node_t* field = node->data.record.fields[i];
        INDENT(e);
        /* Get the field's resolved type from the record type */
        if (node->resolved_type && node->resolved_type->kind == TY_RECORD) {
            emit_type(e, node->resolved_type->as.record.field_types[i]);
        } else {
            EMIT(e, "/* unknown type */");
        }
        EMITLN(e, " %s;", field->data.field_def.name);
    }

    POP_INDENT(e);
    EMITLN(e, "};");
    /* Also emit a named optional typedef for this record type */
    EMITLN(e, "typedef struct { int present; %s_t value; } forge_opt_%s_t;",
           node->data.record.name, node->data.record.name);
    NEWLINE(e);
}

static void emit_proc_decl(forge_emitter_t* e, forge_node_t* node) {
    /* Function signature */
    if (!node->data.proc.exported) {
        EMIT(e, "static ");
    }

    /* Return type - use the resolved type of the return type expression */
    forge_node_t* ret_type_node = node->data.proc.return_type;
    if (ret_type_node && ret_type_node->resolved_type) {
        emit_type(e, ret_type_node->resolved_type);
    } else {
        EMIT(e, "void");
    }

    /* Function name with forge_ prefix (unless it's main) */
    if (strcmp(node->data.proc.name, "main") == 0) {
        EMIT(e, " forge_main(");
    } else {
        EMIT(e, " forge_%s(", node->data.proc.name);
    }

    /* Parameters */
    if (node->data.proc.param_count == 0) {
        EMIT(e, "void");
    } else {
        for (int i = 0; i < node->data.proc.param_count; i++) {
            if (i > 0) EMIT(e, ", ");
            forge_param_t* param = &node->data.proc.params[i];

            /* Get param type from the param's type expression's resolved type */
            if (param->type_expr && param->type_expr->resolved_type) {
                emit_type(e, param->type_expr->resolved_type);
            } else {
                EMIT(e, "/* unknown type */");
            }

            if (param->is_ref) {
                EMIT(e, "*");
            }
            EMIT(e, " %s", param->name);
        }
    }
    EMIT(e, ") ");

    /* Body */
    emit_block(e, node->data.proc.body);
    NEWLINE(e);
}

static void emit_decl(forge_emitter_t* e, forge_node_t* node) {
    switch (node->kind) {
        case NODE_RECORD_DECL:
            emit_record_decl(e, node);
            break;
        case NODE_PROC_DECL:
            emit_proc_decl(e, node);
            break;
        case NODE_VAR_DECL:
        case NODE_CONST_DECL:
            /* Global variable */
            emit_type(e, node->resolved_type);
            EMIT(e, " %s", node->data.var_decl.name);
            if (node->data.var_decl.init_expr) {
                EMIT(e, " = ");
                emit_expr(e, node->data.var_decl.init_expr);
            }
            EMITLN(e, ";");
            break;
        case NODE_CHANNEL_DECL:
            /* Generate channel infrastructure */
            EMITLN(e, "/* Channel: %s */", node->data.channel.name);
            EMIT(e, "typedef void (*%s_handler_t)(", node->data.channel.name);
            if (node->data.channel.payload_type && node->resolved_type) {
                emit_type(e, node->resolved_type);
            } else {
                EMIT(e, "void");
            }
            EMITLN(e, ");");
            EMITLN(e, "static %s_handler_t %s_handlers[64];",
                   node->data.channel.name, node->data.channel.name);
            EMITLN(e, "static int %s_handler_count = 0;",
                   node->data.channel.name);
            NEWLINE(e);
            EMIT(e, "void forge_register_%s(%s_handler_t h) ",
                 node->data.channel.name, node->data.channel.name);
            EMITLN(e, "{ %s_handlers[%s_handler_count++] = h; }",
                   node->data.channel.name, node->data.channel.name);
            NEWLINE(e);
            EMIT(e, "void forge_emit_%s(", node->data.channel.name);
            if (node->data.channel.payload_type && node->resolved_type) {
                emit_type(e, node->resolved_type);
                EMIT(e, " payload");
            }
            EMITLN(e, ") {");
            EMITLN(e, "    for (int i = 0; i < %s_handler_count; i++) {",
                   node->data.channel.name);
            if (node->data.channel.payload_type) {
                EMITLN(e, "        %s_handlers[i](payload);",
                       node->data.channel.name);
            } else {
                EMITLN(e, "        %s_handlers[i]();",
                       node->data.channel.name);
            }
            EMITLN(e, "    }");
            EMITLN(e, "}");
            NEWLINE(e);
            break;
        case NODE_ON_HANDLER: {
            /* Generate handler function */
            static int handler_num = 0;
            EMIT(e, "static void _handler_%s_%d(",
                 node->data.on_handler.channel_name, handler_num);
            if (node->data.on_handler.param_name) {
                /* TODO: get payload type from channel */
                EMIT(e, "/* payload */ int64_t %s",
                     node->data.on_handler.param_name);
            }
            EMIT(e, ") ");
            emit_block(e, node->data.on_handler.body);
            NEWLINE(e);
            handler_num++;
            break;
        }
        default:
            EMITLN(e, "/* TODO: decl kind %d */", node->kind);
            break;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Program Emission
 * ═══════════════════════════════════════════════════════════════════════════ */

void emitter_emit_program(forge_emitter_t* e, forge_node_t* program) {
    if (!program || program->kind != NODE_PROGRAM) return;

    /* Header */
    EMITLN(e, "/* Generated by FORGE compiler v%s */", FORGE_VERSION_STRING);
    EMITLN(e, "/* Source: %s */", e->source_file ? e->source_file : "unknown");
    NEWLINE(e);
    EMITLN(e, "#include \"forge_runtime.h\"");
    NEWLINE(e);

    /* Forward declarations for records */
    for (int i = 0; i < program->data.program.decl_count; i++) {
        forge_node_t* decl = program->data.program.decls[i];
        if (decl->kind == NODE_RECORD_DECL) {
            EMITLN(e, "typedef struct %s_s %s_t;",
                   decl->data.record.name, decl->data.record.name);
        }
    }
    NEWLINE(e);

    /* Emit all declarations */
    for (int i = 0; i < program->data.program.decl_count; i++) {
        emit_decl(e, program->data.program.decls[i]);
    }

    /* Generate main entry point */
    EMITLN(e, "/* Entry point */");
    EMITLN(e, "int main(int argc, char** argv) {");
    EMITLN(e, "    forge_runtime_init(argc, argv);");
    EMITLN(e, "    forge_main();");
    EMITLN(e, "    return 0;");
    EMITLN(e, "}");
}

