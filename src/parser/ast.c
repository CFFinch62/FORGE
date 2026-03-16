/*
 * FORGE Language Toolchain
 * ast.c - AST node constructors and utilities
 */

#include "parser/ast.h"
#include "lexer/lexer.h"  /* For token type constants in op_name() */

/* ─────────────────────────────────────────────────────────────────────────────
 * Internal Helper
 * ───────────────────────────────────────────────────────────────────────────── */

static forge_node_t* node_create(forge_arena_t* a, forge_node_kind_t kind,
                                  int line, int col) {
    forge_node_t* node = ARENA_ALLOC(a, forge_node_t);
    node->kind = kind;
    node->line = line;
    node->column = col;
    node->resolved_type = NULL;
    return node;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Literal Constructors
 * ───────────────────────────────────────────────────────────────────────────── */

forge_node_t* ast_int_lit(forge_arena_t* a, long long val, int line, int col) {
    forge_node_t* node = node_create(a, NODE_INT_LIT, line, col);
    node->data.int_val = val;
    return node;
}

forge_node_t* ast_float_lit(forge_arena_t* a, double val, int line, int col) {
    forge_node_t* node = node_create(a, NODE_FLOAT_LIT, line, col);
    node->data.float_val = val;
    return node;
}

forge_node_t* ast_str_lit(forge_arena_t* a, const char* val, int line, int col) {
    forge_node_t* node = node_create(a, NODE_STR_LIT, line, col);
    node->data.str_val = val;
    return node;
}

forge_node_t* ast_bool_lit(forge_arena_t* a, int val, int line, int col) {
    forge_node_t* node = node_create(a, NODE_BOOL_LIT, line, col);
    node->data.bool_val = val;
    return node;
}

forge_node_t* ast_none_lit(forge_arena_t* a, int line, int col) {
    return node_create(a, NODE_NONE_LIT, line, col);
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Identifier Constructors
 * ───────────────────────────────────────────────────────────────────────────── */

forge_node_t* ast_ident(forge_arena_t* a, const char* name, int line, int col) {
    forge_node_t* node = node_create(a, NODE_IDENT, line, col);
    node->data.name = name;
    return node;
}

forge_node_t* ast_qualified_ident(forge_arena_t* a, const char* module,
                                   const char* symbol, int line, int col) {
    forge_node_t* node = node_create(a, NODE_QUALIFIED_IDENT, line, col);
    node->data.qualified.module_name = module;
    node->data.qualified.symbol_name = symbol;
    return node;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Operator Constructors
 * ───────────────────────────────────────────────────────────────────────────── */

forge_node_t* ast_binary_op(forge_arena_t* a, int op,
                             forge_node_t* left, forge_node_t* right,
                             int line, int col) {
    forge_node_t* node = node_create(a, NODE_BINARY_OP, line, col);
    node->data.binop.op = op;
    node->data.binop.left = left;
    node->data.binop.right = right;
    return node;
}

forge_node_t* ast_unary_op(forge_arena_t* a, int op,
                            forge_node_t* operand, int line, int col) {
    forge_node_t* node = node_create(a, NODE_UNARY_OP, line, col);
    node->data.unop.op = op;
    node->data.unop.operand = operand;
    return node;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Expression Constructors
 * ───────────────────────────────────────────────────────────────────────────── */

forge_node_t* ast_call(forge_arena_t* a, forge_node_t* callee,
                        forge_node_t** args, int arg_count, int line, int col) {
    forge_node_t* node = node_create(a, NODE_CALL, line, col);
    node->data.call.callee = callee;
    node->data.call.args = args;
    node->data.call.arg_count = arg_count;
    return node;
}

forge_node_t* ast_field_access(forge_arena_t* a, forge_node_t* object,
                                const char* field, int line, int col) {
    forge_node_t* node = node_create(a, NODE_FIELD_ACCESS, line, col);
    node->data.field_access.object = object;
    node->data.field_access.field_name = field;
    return node;
}

forge_node_t* ast_index(forge_arena_t* a, forge_node_t* object,
                         forge_node_t* index, int line, int col) {
    forge_node_t* node = node_create(a, NODE_INDEX, line, col);
    node->data.index.object = object;
    node->data.index.index = index;
    return node;
}

forge_node_t* ast_cast(forge_arena_t* a, forge_node_t* target_type,
                        forge_node_t* expr, int line, int col) {
    forge_node_t* node = node_create(a, NODE_CAST, line, col);
    node->data.cast.target_type = target_type;
    node->data.cast.expr = expr;
    return node;
}

forge_node_t* ast_range(forge_arena_t* a, forge_node_t* start,
                         forge_node_t* end, int inclusive, int line, int col) {
    forge_node_t* node = node_create(a, NODE_RANGE, line, col);
    node->data.range.start = start;
    node->data.range.end = end;
    node->data.range.inclusive = inclusive;
    return node;
}

forge_node_t* ast_array_lit(forge_arena_t* a, forge_node_t** elements, int count,
                             int line, int col) {
    forge_node_t* node = node_create(a, NODE_ARRAY_LITERAL, line, col);
    node->data.array_lit.elements = elements;
    node->data.array_lit.count = count;
    return node;
}

forge_node_t* ast_record_lit(forge_arena_t* a, const char* type_name,
                              forge_field_init_t* fields, int field_count,
                              int line, int col) {
    forge_node_t* node = node_create(a, NODE_RECORD_LITERAL, line, col);
    node->data.record_lit.type_name = type_name;
    node->data.record_lit.fields = fields;
    node->data.record_lit.field_count = field_count;
    return node;
}

forge_node_t* ast_some(forge_arena_t* a, forge_node_t* expr, int line, int col) {
    forge_node_t* node = node_create(a, NODE_SOME, line, col);
    node->data.some.expr = expr;
    return node;
}

forge_node_t* ast_or_else(forge_arena_t* a, forge_node_t* optional_expr,
                           forge_node_t* fallback, int line, int col) {
    forge_node_t* node = node_create(a, NODE_OR_ELSE, line, col);
    node->data.or_else.optional_expr = optional_expr;
    node->data.or_else.fallback = fallback;
    return node;
}

forge_node_t* ast_is_some(forge_arena_t* a, forge_node_t* expr, int line, int col) {
    forge_node_t* node = node_create(a, NODE_IS_SOME, line, col);
    node->data.is_check.expr = expr;
    return node;
}

forge_node_t* ast_is_none(forge_arena_t* a, forge_node_t* expr, int line, int col) {
    forge_node_t* node = node_create(a, NODE_IS_NONE, line, col);
    node->data.is_check.expr = expr;
    return node;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Statement Constructors
 * ───────────────────────────────────────────────────────────────────────────── */

forge_node_t* ast_block(forge_arena_t* a, forge_node_t** stmts, int count,
                         int line, int col) {
    forge_node_t* node = node_create(a, NODE_BLOCK, line, col);
    node->data.block.stmts = stmts;
    node->data.block.count = count;
    return node;
}

forge_node_t* ast_assign(forge_arena_t* a, forge_node_t* target,
                          forge_node_t* value, int line, int col) {
    forge_node_t* node = node_create(a, NODE_ASSIGN, line, col);
    node->data.assign.target = target;
    node->data.assign.value = value;
    return node;
}

forge_node_t* ast_compound_assign(forge_arena_t* a, int op, forge_node_t* target,
                                   forge_node_t* value, int line, int col) {
    forge_node_t* node = node_create(a, NODE_COMPOUND_ASSIGN, line, col);
    node->data.compound_assign.op = op;
    node->data.compound_assign.target = target;
    node->data.compound_assign.value = value;
    return node;
}

forge_node_t* ast_if(forge_arena_t* a, forge_node_t* cond, forge_node_t* then_body,
                      forge_node_t** elif_conds, forge_node_t** elif_bodies, int elif_count,
                      forge_node_t* else_body, int line, int col) {
    forge_node_t* node = node_create(a, NODE_IF, line, col);
    node->data.if_stmt.condition = cond;
    node->data.if_stmt.then_body = then_body;
    node->data.if_stmt.elif_conditions = elif_conds;
    node->data.if_stmt.elif_bodies = elif_bodies;
    node->data.if_stmt.elif_count = elif_count;
    node->data.if_stmt.else_body = else_body;
    return node;
}

forge_node_t* ast_while(forge_arena_t* a, forge_node_t* cond, forge_node_t* body,
                         int line, int col) {
    forge_node_t* node = node_create(a, NODE_WHILE, line, col);
    node->data.while_stmt.condition = cond;
    node->data.while_stmt.body = body;
    return node;
}

forge_node_t* ast_for(forge_arena_t* a, const char* var, forge_node_t* iterable,
                       forge_node_t* body, int line, int col) {
    forge_node_t* node = node_create(a, NODE_FOR, line, col);
    node->data.for_stmt.var_name = var;
    node->data.for_stmt.iterable = iterable;
    node->data.for_stmt.body = body;
    return node;
}

forge_node_t* ast_loop(forge_arena_t* a, forge_node_t* body, int line, int col) {
    forge_node_t* node = node_create(a, NODE_LOOP, line, col);
    node->data.loop_stmt.body = body;
    return node;
}

forge_node_t* ast_return(forge_arena_t* a, forge_node_t* value, int line, int col) {
    forge_node_t* node = node_create(a, NODE_RETURN, line, col);
    node->data.return_stmt.value = value;
    return node;
}

forge_node_t* ast_break(forge_arena_t* a, int line, int col) {
    forge_node_t* node = node_create(a, NODE_BREAK, line, col);
    return node;
}

forge_node_t* ast_continue(forge_arena_t* a, int line, int col) {
    forge_node_t* node = node_create(a, NODE_CONTINUE, line, col);
    return node;
}

forge_node_t* ast_emit(forge_arena_t* a, const char* channel_name, forge_node_t* payload,
                        int line, int col) {
    forge_node_t* node = node_create(a, NODE_EMIT, line, col);
    node->data.emit_stmt.channel_name = channel_name;
    node->data.emit_stmt.payload = payload;
    return node;
}

forge_node_t* ast_free_stmt(forge_arena_t* a, forge_node_t* expr, int line, int col) {
    forge_node_t* node = node_create(a, NODE_FREE, line, col);
    node->data.free_stmt.expr = expr;
    return node;
}

forge_node_t* ast_with_alloc(forge_arena_t* a, const char* var_name,
                              forge_node_t* type_expr, forge_node_t* size_expr,
                              forge_node_t* body, int line, int col) {
    forge_node_t* node = node_create(a, NODE_WITH_ALLOC, line, col);
    node->data.with_alloc.var_name = var_name;
    node->data.with_alloc.type_expr = type_expr;
    node->data.with_alloc.size_expr = size_expr;
    node->data.with_alloc.body = body;
    return node;
}

forge_node_t* ast_expr_stmt(forge_arena_t* a, forge_node_t* expr, int line, int col) {
    forge_node_t* node = node_create(a, NODE_EXPR_STMT, line, col);
    node->data.expr_stmt.expr = expr;
    return node;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Declaration Constructors
 * ───────────────────────────────────────────────────────────────────────────── */

forge_node_t* ast_var_decl(forge_arena_t* a, const char* name, forge_node_t* type_expr,
                            forge_node_t* init, int exported, int line, int col) {
    forge_node_t* node = node_create(a, NODE_VAR_DECL, line, col);
    node->data.var_decl.name = name;
    node->data.var_decl.type_expr = type_expr;
    node->data.var_decl.init_expr = init;
    node->data.var_decl.exported = exported;
    return node;
}

forge_node_t* ast_const_decl(forge_arena_t* a, const char* name, forge_node_t* type_expr,
                              forge_node_t* init, int exported, int line, int col) {
    forge_node_t* node = node_create(a, NODE_CONST_DECL, line, col);
    node->data.var_decl.name = name;
    node->data.var_decl.type_expr = type_expr;
    node->data.var_decl.init_expr = init;
    node->data.var_decl.exported = exported;
    return node;
}

forge_node_t* ast_proc_decl(forge_arena_t* a, const char* name,
                             forge_param_t* params, int param_count,
                             forge_node_t* return_type, forge_node_t* body,
                             int exported, int line, int col) {
    forge_node_t* node = node_create(a, NODE_PROC_DECL, line, col);
    node->data.proc.name = name;
    node->data.proc.params = params;
    node->data.proc.param_count = param_count;
    node->data.proc.return_type = return_type;
    node->data.proc.body = body;
    node->data.proc.exported = exported;
    return node;
}

forge_node_t* ast_program(forge_arena_t* a, forge_node_t** imports, int import_count,
                           forge_node_t** decls, int decl_count) {
    forge_node_t* node = node_create(a, NODE_PROGRAM, 1, 1);
    node->data.program.imports = imports;
    node->data.program.import_count = import_count;
    node->data.program.decls = decls;
    node->data.program.decl_count = decl_count;
    return node;
}

forge_node_t* ast_import(forge_arena_t* a, const char* module_path, const char* alias,
                          int line, int col) {
    forge_node_t* node = node_create(a, NODE_IMPORT, line, col);
    node->data.import.module_path = module_path;
    node->data.import.alias = alias;
    return node;
}

forge_node_t* ast_record_decl(forge_arena_t* a, const char* name,
                               forge_node_t** fields, int field_count,
                               int exported, int line, int col) {
    forge_node_t* node = node_create(a, NODE_RECORD_DECL, line, col);
    node->data.record.name = name;
    node->data.record.fields = fields;
    node->data.record.field_count = field_count;
    node->data.record.exported = exported;
    return node;
}

forge_node_t* ast_field_def(forge_arena_t* a, const char* name, forge_node_t* type_expr,
                             int line, int col) {
    forge_node_t* node = node_create(a, NODE_FIELD_DEF, line, col);
    node->data.field_def.name = name;
    node->data.field_def.type_expr = type_expr;
    return node;
}

forge_node_t* ast_channel_decl(forge_arena_t* a, const char* name, forge_node_t* payload_type,
                                int exported, int line, int col) {
    forge_node_t* node = node_create(a, NODE_CHANNEL_DECL, line, col);
    node->data.channel.name = name;
    node->data.channel.payload_type = payload_type;
    node->data.channel.exported = exported;
    return node;
}

forge_node_t* ast_on_handler(forge_arena_t* a, const char* channel_name, const char* param_name,
                              forge_node_t* body, int line, int col) {
    forge_node_t* node = node_create(a, NODE_ON_HANDLER, line, col);
    node->data.on_handler.channel_name = channel_name;
    node->data.on_handler.param_name = param_name;
    node->data.on_handler.body = body;
    return node;
}

forge_node_t* ast_type_alias(forge_arena_t* a, const char* name, forge_node_t* type_expr,
                              int exported, int line, int col) {
    forge_node_t* node = node_create(a, NODE_TYPE_ALIAS, line, col);
    node->data.type_alias.name = name;
    node->data.type_alias.type_expr = type_expr;
    node->data.type_alias.exported = exported;
    return node;
}

/* ───────────────────────────────────────────────────────────────────────────
 * Type Expression Constructors
 * ─────────────────────────────────────────────────────────────────────────── */

forge_node_t* ast_type_prim(forge_arena_t* a, const char* name, int line, int col) {
    forge_node_t* node = node_create(a, NODE_TYPE_PRIM, line, col);
    node->data.name = name;
    return node;
}

forge_node_t* ast_type_named(forge_arena_t* a, const char* name, int line, int col) {
    forge_node_t* node = node_create(a, NODE_TYPE_NAMED, line, col);
    node->data.name = name;
    return node;
}

forge_node_t* ast_type_optional(forge_arena_t* a, forge_node_t* inner, int line, int col) {
    forge_node_t* node = node_create(a, NODE_TYPE_OPTIONAL, line, col);
    node->data.type_optional.inner_type = inner;
    return node;
}

forge_node_t* ast_type_fixed_array(forge_arena_t* a, forge_node_t* elem, int size,
                                    int line, int col) {
    forge_node_t* node = node_create(a, NODE_TYPE_FIXED_ARRAY, line, col);
    node->data.type_fixed_array.elem_type = elem;
    node->data.type_fixed_array.size = size;
    return node;
}

forge_node_t* ast_type_dyn_array(forge_arena_t* a, forge_node_t* elem, int line, int col) {
    forge_node_t* node = node_create(a, NODE_TYPE_DYN_ARRAY, line, col);
    node->data.type_dyn_array.elem_type = elem;
    return node;
}

forge_node_t* ast_type_map(forge_arena_t* a, forge_node_t* key, forge_node_t* val,
                            int line, int col) {
    forge_node_t* node = node_create(a, NODE_TYPE_MAP, line, col);
    node->data.type_map.key_type = key;
    node->data.type_map.val_type = val;
    return node;
}

forge_node_t* ast_type_ref(forge_arena_t* a, forge_node_t* inner, int line, int col) {
    forge_node_t* node = node_create(a, NODE_TYPE_REF, line, col);
    node->data.type_ref.inner_type = inner;
    return node;
}

/* ───────────────────────────────────────────────────────────────────────────
 * Utility Functions
 * ─────────────────────────────────────────────────────────────────────────── */

const char* ast_node_kind_name(forge_node_kind_t kind) {
    switch (kind) {
        case NODE_PROGRAM:         return "PROGRAM";
        case NODE_IMPORT:          return "IMPORT";
        case NODE_PROC_DECL:       return "PROC_DECL";
        case NODE_RECORD_DECL:     return "RECORD_DECL";
        case NODE_CHANNEL_DECL:    return "CHANNEL_DECL";
        case NODE_ON_HANDLER:      return "ON_HANDLER";
        case NODE_VAR_DECL:        return "VAR_DECL";
        case NODE_CONST_DECL:      return "CONST_DECL";
        case NODE_TYPE_ALIAS:      return "TYPE_ALIAS";
        case NODE_FIELD_DEF:       return "FIELD_DEF";
        case NODE_PARAM:           return "PARAM";
        case NODE_BLOCK:           return "BLOCK";
        case NODE_ASSIGN:          return "ASSIGN";
        case NODE_COMPOUND_ASSIGN: return "COMPOUND_ASSIGN";
        case NODE_IF:              return "IF";
        case NODE_WHILE:           return "WHILE";
        case NODE_FOR:             return "FOR";
        case NODE_LOOP:            return "LOOP";
        case NODE_RETURN:          return "RETURN";
        case NODE_BREAK:           return "BREAK";
        case NODE_CONTINUE:        return "CONTINUE";
        case NODE_EMIT:            return "EMIT";
        case NODE_WITH_ALLOC:      return "WITH_ALLOC";
        case NODE_FREE:            return "FREE";
        case NODE_PANIC:           return "PANIC";
        case NODE_ASSERT:          return "ASSERT";
        case NODE_EXPR_STMT:       return "EXPR_STMT";
        case NODE_INT_LIT:         return "INT_LIT";
        case NODE_FLOAT_LIT:       return "FLOAT_LIT";
        case NODE_STR_LIT:         return "STR_LIT";
        case NODE_BOOL_LIT:        return "BOOL_LIT";
        case NODE_NONE_LIT:        return "NONE_LIT";
        case NODE_IDENT:           return "IDENT";
        case NODE_QUALIFIED_IDENT: return "QUALIFIED_IDENT";
        case NODE_BINARY_OP:       return "BINARY_OP";
        case NODE_UNARY_OP:        return "UNARY_OP";
        case NODE_CALL:            return "CALL";
        case NODE_FIELD_ACCESS:    return "FIELD_ACCESS";
        case NODE_INDEX:           return "INDEX";
        case NODE_RECORD_LITERAL:  return "RECORD_LITERAL";
        case NODE_ARRAY_LITERAL:   return "ARRAY_LITERAL";
        case NODE_CAST:            return "CAST";
        case NODE_SOME:            return "SOME";
        case NODE_OR_ELSE:         return "OR_ELSE";
        case NODE_IS_SOME:         return "IS_SOME";
        case NODE_IS_NONE:         return "IS_NONE";
        case NODE_RANGE:           return "RANGE";
        case NODE_TYPE_PRIM:       return "TYPE_PRIM";
        case NODE_TYPE_OPTIONAL:   return "TYPE_OPTIONAL";
        case NODE_TYPE_FIXED_ARRAY:return "TYPE_FIXED_ARRAY";
        case NODE_TYPE_DYN_ARRAY:  return "TYPE_DYN_ARRAY";
        case NODE_TYPE_MAP:        return "TYPE_MAP";
        case NODE_TYPE_NAMED:      return "TYPE_NAMED";
        case NODE_TYPE_REF:        return "TYPE_REF";
        default:                   return "UNKNOWN";
    }
}

static void print_indent(int depth) {
    for (int i = 0; i < depth; i++) {
        printf("  ");
    }
}

const char* ast_op_name(int op) {
    switch (op) {
        case TOK_PLUS:      return "+";
        case TOK_MINUS:     return "-";
        case TOK_STAR:      return "*";
        case TOK_SLASH:     return "/";
        case TOK_PERCENT:   return "%";
        case TOK_EQ:        return "==";
        case TOK_NEQ:       return "!=";
        case TOK_LT:        return "<";
        case TOK_GT:        return ">";
        case TOK_LEQ:       return "<=";
        case TOK_GEQ:       return ">=";
        case TOK_AND:       return "and";
        case TOK_OR:        return "or";
        case TOK_NOT:       return "not";
        case TOK_AMP:       return "&";
        case TOK_PIPE:      return "|";
        case TOK_CARET:     return "^";
        case TOK_TILDE:     return "~";
        case TOK_LSHIFT:    return "<<";
        case TOK_RSHIFT:    return ">>";
        case TOK_PLUS_EQ:   return "+=";
        case TOK_MINUS_EQ:  return "-=";
        case TOK_STAR_EQ:   return "*=";
        case TOK_SLASH_EQ:  return "/=";
        case TOK_PERCENT_EQ:return "%=";
        default:            return "?";
    }
}

void ast_print(forge_node_t* node, int depth) {
    if (node == NULL) {
        print_indent(depth);
        printf("(null)\n");
        return;
    }

    print_indent(depth);
    printf("%s", ast_node_kind_name(node->kind));

    switch (node->kind) {
        /* === Literals === */
        case NODE_INT_LIT:
            printf(" %lld\n", node->data.int_val);
            break;
        case NODE_FLOAT_LIT:
            printf(" %g\n", node->data.float_val);
            break;
        case NODE_STR_LIT:
            printf(" \"%s\"\n", node->data.str_val);
            break;
        case NODE_BOOL_LIT:
            printf(" %s\n", node->data.bool_val ? "true" : "false");
            break;
        case NODE_NONE_LIT:
            printf("\n");
            break;

        /* === Identifiers === */
        case NODE_IDENT:
        case NODE_TYPE_PRIM:
        case NODE_TYPE_NAMED:
            printf(" %s\n", node->data.name);
            break;
        case NODE_QUALIFIED_IDENT:
            printf(" %s.%s\n",
                   node->data.qualified.module_name,
                   node->data.qualified.symbol_name);
            break;

        /* === Operators === */
        case NODE_BINARY_OP:
            printf(" %s\n", ast_op_name(node->data.binop.op));
            ast_print(node->data.binop.left, depth + 1);
            ast_print(node->data.binop.right, depth + 1);
            break;
        case NODE_UNARY_OP:
            printf(" %s\n", ast_op_name(node->data.unop.op));
            ast_print(node->data.unop.operand, depth + 1);
            break;

        /* === Function calls and access === */
        case NODE_CALL:
            printf(" (args=%d)\n", node->data.call.arg_count);
            print_indent(depth + 1);
            printf("callee:\n");
            ast_print(node->data.call.callee, depth + 2);
            for (int i = 0; i < node->data.call.arg_count; i++) {
                print_indent(depth + 1);
                printf("arg[%d]:\n", i);
                ast_print(node->data.call.args[i], depth + 2);
            }
            break;
        case NODE_FIELD_ACCESS:
            printf(" .%s\n", node->data.field_access.field_name);
            ast_print(node->data.field_access.object, depth + 1);
            break;
        case NODE_INDEX:
            printf("\n");
            print_indent(depth + 1);
            printf("object:\n");
            ast_print(node->data.index.object, depth + 2);
            print_indent(depth + 1);
            printf("index:\n");
            ast_print(node->data.index.index, depth + 2);
            break;

        /* === Literals (compound) === */
        case NODE_ARRAY_LITERAL:
            printf(" (count=%d)\n", node->data.array_lit.count);
            for (int i = 0; i < node->data.array_lit.count; i++) {
                ast_print(node->data.array_lit.elements[i], depth + 1);
            }
            break;
        case NODE_RANGE:
            printf(" (inclusive=%d)\n", node->data.range.inclusive);
            print_indent(depth + 1);
            printf("start:\n");
            ast_print(node->data.range.start, depth + 2);
            print_indent(depth + 1);
            printf("end:\n");
            ast_print(node->data.range.end, depth + 2);
            break;
        case NODE_CAST:
            printf("\n");
            print_indent(depth + 1);
            printf("to_type:\n");
            ast_print(node->data.cast.target_type, depth + 2);
            print_indent(depth + 1);
            printf("expr:\n");
            ast_print(node->data.cast.expr, depth + 2);
            break;

        /* === Blocks and statements === */
        case NODE_BLOCK:
            printf(" (%d stmts)\n", node->data.block.count);
            for (int i = 0; i < node->data.block.count; i++) {
                ast_print(node->data.block.stmts[i], depth + 1);
            }
            break;
        case NODE_ASSIGN:
            printf("\n");
            print_indent(depth + 1);
            printf("target:\n");
            ast_print(node->data.assign.target, depth + 2);
            print_indent(depth + 1);
            printf("value:\n");
            ast_print(node->data.assign.value, depth + 2);
            break;
        case NODE_COMPOUND_ASSIGN:
            printf(" (op=%d)\n", node->data.compound_assign.op);
            print_indent(depth + 1);
            printf("target:\n");
            ast_print(node->data.compound_assign.target, depth + 2);
            print_indent(depth + 1);
            printf("value:\n");
            ast_print(node->data.compound_assign.value, depth + 2);
            break;
        case NODE_IF:
            printf(" (elif_count=%d, has_else=%d)\n",
                   node->data.if_stmt.elif_count,
                   node->data.if_stmt.else_body != NULL);
            print_indent(depth + 1);
            printf("condition:\n");
            ast_print(node->data.if_stmt.condition, depth + 2);
            print_indent(depth + 1);
            printf("then:\n");
            ast_print(node->data.if_stmt.then_body, depth + 2);
            for (int i = 0; i < node->data.if_stmt.elif_count; i++) {
                print_indent(depth + 1);
                printf("elif[%d] cond:\n", i);
                ast_print(node->data.if_stmt.elif_conditions[i], depth + 2);
                print_indent(depth + 1);
                printf("elif[%d] body:\n", i);
                ast_print(node->data.if_stmt.elif_bodies[i], depth + 2);
            }
            if (node->data.if_stmt.else_body) {
                print_indent(depth + 1);
                printf("else:\n");
                ast_print(node->data.if_stmt.else_body, depth + 2);
            }
            break;
        case NODE_WHILE:
            printf("\n");
            print_indent(depth + 1);
            printf("condition:\n");
            ast_print(node->data.while_stmt.condition, depth + 2);
            print_indent(depth + 1);
            printf("body:\n");
            ast_print(node->data.while_stmt.body, depth + 2);
            break;
        case NODE_FOR:
            printf(" %s\n", node->data.for_stmt.var_name);
            print_indent(depth + 1);
            printf("iterable:\n");
            ast_print(node->data.for_stmt.iterable, depth + 2);
            print_indent(depth + 1);
            printf("body:\n");
            ast_print(node->data.for_stmt.body, depth + 2);
            break;
        case NODE_LOOP:
            printf("\n");
            ast_print(node->data.loop_stmt.body, depth + 1);
            break;
        case NODE_RETURN:
            printf("\n");
            if (node->data.return_stmt.value) {
                ast_print(node->data.return_stmt.value, depth + 1);
            }
            break;
        case NODE_BREAK:
        case NODE_CONTINUE:
            printf("\n");
            break;
        case NODE_EMIT:
            printf(" %s\n", node->data.emit_stmt.channel_name);
            if (node->data.emit_stmt.payload) {
                print_indent(depth + 1);
                printf("payload:\n");
                ast_print(node->data.emit_stmt.payload, depth + 2);
            }
            break;
        case NODE_EXPR_STMT:
            printf("\n");
            ast_print(node->data.expr_stmt.expr, depth + 1);
            break;

        /* === Declarations === */
        case NODE_PROC_DECL:
            printf(" %s%s (%d params)\n",
                   node->data.proc.exported ? "export " : "",
                   node->data.proc.name,
                   node->data.proc.param_count);
            for (int i = 0; i < node->data.proc.param_count; i++) {
                print_indent(depth + 1);
                forge_param_t* param = &node->data.proc.params[i];
                printf("param[%d]: %s%s",
                       i,
                       param->is_ref ? "ref " : "",
                       param->name);
                if (param->type_expr) {
                    printf(" : ");
                }
                printf("\n");
                if (param->type_expr) {
                    ast_print(param->type_expr, depth + 2);
                }
            }
            if (node->data.proc.return_type) {
                print_indent(depth + 1);
                printf("returns:\n");
                ast_print(node->data.proc.return_type, depth + 2);
            }
            if (node->data.proc.body) {
                print_indent(depth + 1);
                printf("body:\n");
                ast_print(node->data.proc.body, depth + 2);
            }
            break;
        /* NODE_PARAM is not an AST node - params are stored inline in forge_param_t */
        case NODE_VAR_DECL:
        case NODE_CONST_DECL:
            printf(" %s\n", node->data.var_decl.name);
            if (node->data.var_decl.type_expr) {
                print_indent(depth + 1);
                printf("type:\n");
                ast_print(node->data.var_decl.type_expr, depth + 2);
            }
            if (node->data.var_decl.init_expr) {
                print_indent(depth + 1);
                printf("init:\n");
                ast_print(node->data.var_decl.init_expr, depth + 2);
            }
            break;
        case NODE_RECORD_DECL:
            printf(" %s%s (%d fields)\n",
                   node->data.record.exported ? "export " : "",
                   node->data.record.name,
                   node->data.record.field_count);
            for (int i = 0; i < node->data.record.field_count; i++) {
                ast_print(node->data.record.fields[i], depth + 1);
            }
            break;
        case NODE_FIELD_DEF:
            printf(" %s\n", node->data.field_def.name);
            if (node->data.field_def.type_expr) {
                ast_print(node->data.field_def.type_expr, depth + 1);
            }
            break;
        case NODE_CHANNEL_DECL:
            printf(" %s%s\n",
                   node->data.channel.exported ? "export " : "",
                   node->data.channel.name);
            if (node->data.channel.payload_type) {
                print_indent(depth + 1);
                printf("payload:\n");
                ast_print(node->data.channel.payload_type, depth + 2);
            }
            break;
        case NODE_IMPORT:
            printf(" %s", node->data.import.module_path);
            if (node->data.import.alias) {
                printf(" as %s", node->data.import.alias);
            }
            printf("\n");
            break;
        case NODE_PROGRAM:
            printf(" (imports=%d, decls=%d)\n",
                   node->data.program.import_count,
                   node->data.program.decl_count);
            for (int i = 0; i < node->data.program.import_count; i++) {
                ast_print(node->data.program.imports[i], depth + 1);
            }
            for (int i = 0; i < node->data.program.decl_count; i++) {
                ast_print(node->data.program.decls[i], depth + 1);
            }
            break;

        /* === Type expressions === */
        case NODE_TYPE_OPTIONAL:
            printf("\n");
            ast_print(node->data.type_optional.inner_type, depth + 1);
            break;
        case NODE_TYPE_FIXED_ARRAY:
            printf(" (size=%d)\n", node->data.type_fixed_array.size);
            ast_print(node->data.type_fixed_array.elem_type, depth + 1);
            break;
        case NODE_TYPE_DYN_ARRAY:
            printf("\n");
            ast_print(node->data.type_dyn_array.elem_type, depth + 1);
            break;
        case NODE_TYPE_MAP:
            printf("\n");
            print_indent(depth + 1);
            printf("key:\n");
            ast_print(node->data.type_map.key_type, depth + 2);
            print_indent(depth + 1);
            printf("val:\n");
            ast_print(node->data.type_map.val_type, depth + 2);
            break;
        case NODE_TYPE_REF:
            printf("\n");
            ast_print(node->data.type_ref.inner_type, depth + 1);
            break;

        default:
            printf(" (unhandled)\n");
            break;
    }
}

