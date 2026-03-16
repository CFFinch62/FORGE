/*
 * FORGE Language - Production CLI
 *
 * Usage: forge <command> [options] [file.fg]
 *
 * Commands:
 *   run       Run a FORGE program (interpreter)
 *   build     Compile to native binary
 *   check     Type-check only (no execution)
 *   repl      Interactive FORGE shell
 *   doc       Generate documentation
 *   emit      Emit C code to stdout (low-level)
 *   emit-llvm Emit LLVM IR to stdout (low-level)
 *
 * Phase 7 Implementation - Full CLI
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>     /* access(), getcwd() */

#include "common.h"
#include "util/memory.h"
#include "util/strtable.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "typecheck/checker.h"
#include "interp/interp.h"
#include "emit_c/emit_c.h"
#include "emit_llvm/emit_llvm.h"

/* ─────────────────────────────────────────────────────────────────────────────
 * CLI Argument Parsing (Task 7.1)
 * ───────────────────────────────────────────────────────────────────────────── */

typedef enum {
    CMD_NONE = 0,
    CMD_RUN,
    CMD_BUILD,
    CMD_CHECK,
    CMD_REPL,
    CMD_DOC,
    CMD_EMIT,
    CMD_EMIT_LLVM,
    CMD_FMT,
    CMD_VERSION,
    CMD_HELP
} forge_command_t;

typedef struct {
    forge_command_t command;
    const char*     input_file;
    const char*     output_file;    /* --out, -o */
    const char*     target;         /* --target c|llvm */
    int             opt_level;      /* --opt, -O (0, 1, 2, 3) */
    int             debug;          /* --debug, -g */
    int             verbose;        /* --verbose, -v */
    int             quiet;          /* --quiet, -q */
    int             trace;          /* --trace (run mode) */
    int             repl_after;     /* --repl (run mode: drop into REPL after main) */
    int             bounds_check;   /* --bounds-check (build mode) */
    int             async_channels; /* --async-channels (build mode) */
    int             strict;         /* --strict (warnings as errors) */
    int             no_color;       /* --no-color (disable colored output) */
    const char*     cc;             /* --cc (compiler path) */
    const char*     runtime_path;   /* --runtime (runtime path) */
    const char*     arch;           /* --arch (cross-compile target arch) */
    const char*     os;             /* --os (cross-compile target OS) */
} forge_cli_args_t;

static void cli_args_init(forge_cli_args_t* args) {
    memset(args, 0, sizeof(*args));
    args->command = CMD_NONE;
    args->target = "c";             /* Default to C backend */
    args->opt_level = 0;
    args->cc = "gcc";               /* Default compiler */
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Help and Version
 * ───────────────────────────────────────────────────────────────────────────── */

static void print_version(void) {
    printf("forge %s\n", FORGE_VERSION_STRING);
    printf("FORGE Language Toolchain\n");
    printf("Copyright (c) 2024-2026\n");
}

static void print_usage(const char* prog) {
    printf("FORGE Language Toolchain v%s\n\n", FORGE_VERSION_STRING);
    printf("Usage: %s <command> [options] [file.fg]\n\n", prog);
    printf("Commands:\n");
    printf("  run <file.fg>       Run a FORGE program (interpreter)\n");
    printf("  build <file.fg>     Compile to native binary\n");
    printf("  check <file.fg>     Type-check only (no execution)\n");
    printf("  fmt <file.fg>       Format source code in place\n");
    printf("  repl                Interactive FORGE shell\n");
    printf("  doc <file.fg>       Generate documentation\n");
    printf("  emit <file.fg>      Emit C code to stdout\n");
    printf("  emit-llvm <file.fg> Emit LLVM IR to stdout\n");
    printf("\n");
    printf("Options:\n");
    printf("  -o, --out <file>    Output file (build mode)\n");
    printf("  --target <c|llvm>   Compilation target (default: c)\n");
    printf("  -O, --opt <level>   Optimization level 0-3 (default: 0)\n");
    printf("  -g, --debug         Include debug symbols\n");
    printf("  --trace             Enable execution tracing (run mode)\n");
    printf("  --repl              Drop into REPL after main() (run mode)\n");
    printf("  --bounds-check      Enable array bounds checking (build mode)\n");
    printf("  --async-channels    Enable async channel delivery (build mode)\n");
    printf("  --strict            Treat warnings as errors\n");
    printf("  --arch <arch>       Cross-compile target architecture\n");
    printf("  --os <os>           Cross-compile target OS\n");
    printf("  --cc <compiler>     C compiler to use (default: gcc)\n");
    printf("  --runtime <path>    Path to runtime directory\n");
    printf("  --no-color          Disable colored output\n");
    printf("  -v, --verbose       Verbose output\n");
    printf("  -q, --quiet         Suppress non-error output\n");
    printf("  --version           Show version\n");
    printf("  --help              Show this help\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s run hello.fg             Run hello.fg\n", prog);
    printf("  %s run hello.fg --repl      Run then drop into REPL\n", prog);
    printf("  %s build hello.fg -o hello  Compile to ./hello\n", prog);
    printf("  %s build hello.fg --strict  Compile with warnings as errors\n", prog);
    printf("  %s fmt src/*.fg             Format source files\n", prog);
    printf("  %s check src/*.fg           Type-check files\n", prog);
    printf("  %s repl                     Start REPL\n", prog);
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Argument Parser
 * ───────────────────────────────────────────────────────────────────────────── */

static int parse_args(int argc, char** argv, forge_cli_args_t* args) {
    if (argc < 2) {
        print_usage(argv[0]);
        return -1;
    }

    cli_args_init(args);
    int i = 1;

    /* Check for global flags before command */
    if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-V") == 0) {
        args->command = CMD_VERSION;
        return 0;
    }
    if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
        args->command = CMD_HELP;
        return 0;
    }

    /* Parse command */
    const char* cmd = argv[i++];
    if (strcmp(cmd, "run") == 0) {
        args->command = CMD_RUN;
    } else if (strcmp(cmd, "build") == 0) {
        args->command = CMD_BUILD;
    } else if (strcmp(cmd, "check") == 0) {
        args->command = CMD_CHECK;
    } else if (strcmp(cmd, "fmt") == 0) {
        args->command = CMD_FMT;
    } else if (strcmp(cmd, "repl") == 0) {
        args->command = CMD_REPL;
    } else if (strcmp(cmd, "doc") == 0) {
        args->command = CMD_DOC;
    } else if (strcmp(cmd, "emit") == 0) {
        args->command = CMD_EMIT;
    } else if (strcmp(cmd, "emit-llvm") == 0) {
        args->command = CMD_EMIT_LLVM;
    } else {
        fprintf(stderr, "forge: Unknown command '%s'\n", cmd);
        fprintf(stderr, "Run 'forge --help' for usage.\n");
        return -1;
    }

    /* Parse options and positional arguments */
    while (i < argc) {
        const char* arg = argv[i];

        if (arg[0] == '-') {
            /* Option */
            if (strcmp(arg, "-o") == 0 || strcmp(arg, "--out") == 0) {
                if (++i >= argc) {
                    fprintf(stderr, "forge: %s requires an argument\n", arg);
                    return -1;
                }
                args->output_file = argv[i];
            } else if (strcmp(arg, "--target") == 0) {
                if (++i >= argc) {
                    fprintf(stderr, "forge: --target requires 'c' or 'llvm'\n");
                    return -1;
                }
                args->target = argv[i];
                if (strcmp(args->target, "c") != 0 && strcmp(args->target, "llvm") != 0) {
                    fprintf(stderr, "forge: Invalid target '%s' (use 'c' or 'llvm')\n", args->target);
                    return -1;
                }
            } else if (strcmp(arg, "-O") == 0 || strcmp(arg, "--opt") == 0) {
                if (++i >= argc) {
                    fprintf(stderr, "forge: %s requires level 0-3\n", arg);
                    return -1;
                }
                args->opt_level = atoi(argv[i]);
                if (args->opt_level < 0 || args->opt_level > 3) {
                    fprintf(stderr, "forge: Invalid optimization level %d\n", args->opt_level);
                    return -1;
                }
            } else if (strcmp(arg, "-O0") == 0) {
                args->opt_level = 0;
            } else if (strcmp(arg, "-O1") == 0) {
                args->opt_level = 1;
            } else if (strcmp(arg, "-O2") == 0) {
                args->opt_level = 2;
            } else if (strcmp(arg, "-O3") == 0) {
                args->opt_level = 3;
            } else if (strcmp(arg, "-g") == 0 || strcmp(arg, "--debug") == 0) {
                args->debug = 1;
            } else if (strcmp(arg, "-v") == 0 || strcmp(arg, "--verbose") == 0) {
                args->verbose = 1;
            } else if (strcmp(arg, "-q") == 0 || strcmp(arg, "--quiet") == 0) {
                args->quiet = 1;
            } else if (strcmp(arg, "--trace") == 0) {
                args->trace = 1;
            } else if (strcmp(arg, "--repl") == 0) {
                args->repl_after = 1;
            } else if (strcmp(arg, "--bounds-check") == 0) {
                args->bounds_check = 1;
            } else if (strcmp(arg, "--async-channels") == 0) {
                args->async_channels = 1;
            } else if (strcmp(arg, "--strict") == 0) {
                args->strict = 1;
            } else if (strcmp(arg, "--no-color") == 0) {
                args->no_color = 1;
            } else if (strcmp(arg, "--arch") == 0) {
                if (++i >= argc) {
                    fprintf(stderr, "forge: --arch requires an architecture\n");
                    return -1;
                }
                args->arch = argv[i];
            } else if (strcmp(arg, "--os") == 0) {
                if (++i >= argc) {
                    fprintf(stderr, "forge: --os requires an OS name\n");
                    return -1;
                }
                args->os = argv[i];
            } else if (strcmp(arg, "--cc") == 0) {
                if (++i >= argc) {
                    fprintf(stderr, "forge: --cc requires a compiler path\n");
                    return -1;
                }
                args->cc = argv[i];
            } else if (strcmp(arg, "--runtime") == 0) {
                if (++i >= argc) {
                    fprintf(stderr, "forge: --runtime requires a path\n");
                    return -1;
                }
                args->runtime_path = argv[i];
            } else if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
                args->command = CMD_HELP;
                return 0;
            } else if (strcmp(arg, "--version") == 0) {
                args->command = CMD_VERSION;
                return 0;
            } else {
                fprintf(stderr, "forge: Unknown option '%s'\n", arg);
                return -1;
            }
        } else {
            /* Positional argument (input file) */
            if (args->input_file == NULL) {
                args->input_file = arg;
            } else {
                fprintf(stderr, "forge: Unexpected argument '%s'\n", arg);
                return -1;
            }
        }
        i++;
    }

    /* Validate required arguments */
    if (args->command != CMD_REPL && args->command != CMD_HELP &&
        args->command != CMD_VERSION && args->input_file == NULL) {
        fprintf(stderr, "forge: No input file specified\n");
        return -1;
    }

    return 0;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * File Reading
 * ───────────────────────────────────────────────────────────────────────────── */

static char* read_file(const char* path, int* out_len) {
    FILE* file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "forge: Cannot open file '%s': %s\n", path, strerror(errno));
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char* buffer = malloc(size + 1);
    if (!buffer) {
        fprintf(stderr, "forge: Out of memory reading '%s'\n", path);
        fclose(file);
        return NULL;
    }

    size_t nread = fread(buffer, 1, size, file);
    buffer[nread] = '\0';
    fclose(file);

    if (out_len) *out_len = (int)nread;
    return buffer;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Runtime Path Detection
 * ───────────────────────────────────────────────────────────────────────────── */

static int find_runtime_path(const forge_cli_args_t* args, char* out_path, size_t out_size) {
    /* User override */
    if (args->runtime_path) {
        snprintf(out_path, out_size, "%s", args->runtime_path);
        if (access(out_path, R_OK) == 0) return 0;
        fprintf(stderr, "forge: Runtime path '%s' not found\n", args->runtime_path);
        return -1;
    }

    /* Check relative to executable (../runtime) */
    /* Try current directory runtime/ */
    snprintf(out_path, out_size, "runtime");
    if (access(out_path, R_OK) == 0) return 0;

    /* Check common install locations */
    const char* paths[] = {
        "/usr/local/share/forge/runtime",
        "/usr/share/forge/runtime",
        NULL
    };
    for (int i = 0; paths[i]; i++) {
        snprintf(out_path, out_size, "%s", paths[i]);
        if (access(out_path, R_OK) == 0) return 0;
    }

    fprintf(stderr, "forge: Cannot find runtime directory\n");
    return -1;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Forward declarations for REPL
 * ───────────────────────────────────────────────────────────────────────────── */

static int repl_execute(forge_interp_t* interp, forge_arena_t* arena,
                        forge_strtable_t* strtable, const char* input);
static void repl_loop(forge_interp_t* interp, forge_arena_t* arena,
                      forge_strtable_t* strtable);

/* ─────────────────────────────────────────────────────────────────────────────
 * Command: run
 * ───────────────────────────────────────────────────────────────────────────── */

static int cmd_run(const forge_cli_args_t* args) {
    int source_len = 0;
    char* source = read_file(args->input_file, &source_len);
    if (!source) return 1;

    forge_strtable_t* strtable = strtable_create();
    forge_arena_t* arena = arena_create(65536);
    int exit_code = 0;

    /* Lex */
    forge_lexer_t* lexer = lexer_create(source, source_len, args->input_file, strtable);
    lexer_tokenize(lexer);
    if (lexer->had_error) {
        exit_code = 1;
        goto cleanup_lexer;
    }

    /* Parse */
    forge_parser_t* parser = parser_create(lexer->tokens.data, lexer->tokens.len,
                                           arena, strtable, args->input_file);
    forge_node_t* program = parser_parse(parser);
    if (parser_had_error(parser)) {
        exit_code = 1;
        goto cleanup_parser;
    }

    /* Type Check */
    forge_checker_t* checker = checker_create(arena, strtable, args->input_file);
    if (checker_check(checker, program) != 0) {
        exit_code = 1;
        goto cleanup_checker;
    }

    /* Interpret */
    {
        forge_interp_t* interp = interp_create(arena, strtable);
        interp->current_filename = args->input_file;
        interp_run(interp, program);
        if (interp->had_error) exit_code = 1;

        /* If --repl flag, drop into REPL after main() returns */
        if (args->repl_after && !interp->had_error) {
            printf("\n--- main() finished, entering REPL ---\n");
            printf("Variables and procedures from the program are available.\n\n");
            repl_loop(interp, arena, strtable);
        }

        interp_destroy(interp);
    }

cleanup_checker:
    checker_destroy(checker);
cleanup_parser:
    parser_destroy(parser);
cleanup_lexer:
    lexer_destroy(lexer);
    arena_destroy(arena);
    strtable_destroy(strtable);
    free(source);
    return exit_code;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Command: check
 * ───────────────────────────────────────────────────────────────────────────── */

static int cmd_check(const forge_cli_args_t* args) {
    int source_len = 0;
    char* source = read_file(args->input_file, &source_len);
    if (!source) return 1;

    forge_strtable_t* strtable = strtable_create();
    forge_arena_t* arena = arena_create(65536);
    int exit_code = 0;

    /* Lex */
    forge_lexer_t* lexer = lexer_create(source, source_len, args->input_file, strtable);
    lexer_tokenize(lexer);
    if (lexer->had_error) {
        exit_code = 1;
        goto cleanup_lexer;
    }

    /* Parse */
    forge_parser_t* parser = parser_create(lexer->tokens.data, lexer->tokens.len,
                                           arena, strtable, args->input_file);
    forge_node_t* program = parser_parse(parser);
    if (parser_had_error(parser)) {
        exit_code = 1;
        goto cleanup_parser;
    }

    /* Type Check */
    forge_checker_t* checker = checker_create(arena, strtable, args->input_file);
    if (checker_check(checker, program) != 0) {
        exit_code = 1;
        goto cleanup_checker;
    }

    if (!args->quiet) {
        printf("✓ %s: No type errors\n", args->input_file);
    }

cleanup_checker:
    checker_destroy(checker);
cleanup_parser:
    parser_destroy(parser);
cleanup_lexer:
    lexer_destroy(lexer);
    arena_destroy(arena);
    strtable_destroy(strtable);
    free(source);
    return exit_code;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Command: fmt (format source code)
 *
 * Formats FORGE source to canonical style:
 * - 4-space indentation
 * - One blank line between top-level declarations
 * - Two blank lines between procedures
 * - Spaces around binary operators
 * - No trailing whitespace
 * - Consistent spacing inside record literals and argument lists
 * ───────────────────────────────────────────────────────────────────────────── */

/* Helper: write indentation */
static void fmt_indent(FILE* out, int level) {
    for (int i = 0; i < level * 4; i++) {
        fputc(' ', out);
    }
}

/* Helper: format a type expression */
static void fmt_type(FILE* out, forge_node_t* type) {
    if (!type) return;
    switch (type->kind) {
        case NODE_TYPE_PRIM:
        case NODE_TYPE_NAMED:
            fprintf(out, "%s", type->data.name);
            break;
        case NODE_TYPE_FIXED_ARRAY:
            fprintf(out, "[");
            fmt_type(out, type->data.type_fixed_array.elem_type);
            fprintf(out, "; %d]", type->data.type_fixed_array.size);
            break;
        case NODE_TYPE_DYN_ARRAY:
            fprintf(out, "[]");
            fmt_type(out, type->data.type_dyn_array.elem_type);
            break;
        case NODE_TYPE_OPTIONAL:
            fprintf(out, "?");
            fmt_type(out, type->data.type_optional.inner_type);
            break;
        case NODE_TYPE_REF:
            fprintf(out, "ref ");
            fmt_type(out, type->data.type_ref.inner_type);
            break;
        case NODE_TYPE_MAP:
            fprintf(out, "map[");
            fmt_type(out, type->data.type_map.key_type);
            fprintf(out, ", ");
            fmt_type(out, type->data.type_map.val_type);
            fprintf(out, "]");
            break;
        default:
            fprintf(out, "?");
            break;
    }
}

/* Forward declare */
static void fmt_expr(FILE* out, forge_node_t* expr);
static void fmt_stmt(FILE* out, forge_node_t* stmt, int indent);
static void fmt_block(FILE* out, forge_node_t* block, int indent);

/* Format an expression */
static void fmt_expr(FILE* out, forge_node_t* expr) {
    if (!expr) return;
    switch (expr->kind) {
        case NODE_INT_LIT:
            fprintf(out, "%lld", (long long)expr->data.int_val);
            break;
        case NODE_FLOAT_LIT:
            fprintf(out, "%g", expr->data.float_val);
            break;
        case NODE_BOOL_LIT:
            fprintf(out, "%s", expr->data.bool_val ? "true" : "false");
            break;
        case NODE_STR_LIT:
            fprintf(out, "\"%s\"", expr->data.str_val);
            break;
        case NODE_NONE_LIT:
            fprintf(out, "none");
            break;
        case NODE_IDENT:
            fprintf(out, "%s", expr->data.name);
            break;
        case NODE_QUALIFIED_IDENT:
            fprintf(out, "%s.%s", expr->data.qualified.module_name,
                    expr->data.qualified.symbol_name);
            break;
        case NODE_BINARY_OP: {
            const char* op = "";
            switch (expr->data.binop.op) {
                case TOK_PLUS: op = " + "; break;
                case TOK_MINUS: op = " - "; break;
                case TOK_STAR: op = " * "; break;
                case TOK_SLASH: op = " / "; break;
                case TOK_PERCENT: op = " % "; break;
                case TOK_EQ: op = " == "; break;
                case TOK_NEQ: op = " != "; break;
                case TOK_LT: op = " < "; break;
                case TOK_LEQ: op = " <= "; break;
                case TOK_GT: op = " > "; break;
                case TOK_GEQ: op = " >= "; break;
                case TOK_AND: op = " and "; break;
                case TOK_OR: op = " or "; break;
                default: op = " ?? "; break;
            }
            fmt_expr(out, expr->data.binop.left);
            fprintf(out, "%s", op);
            fmt_expr(out, expr->data.binop.right);
            break;
        }
        case NODE_UNARY_OP: {
            switch (expr->data.unop.op) {
                case TOK_MINUS: fprintf(out, "-"); break;
                case TOK_NOT: fprintf(out, "not "); break;
                default: fprintf(out, "?"); break;
            }
            fmt_expr(out, expr->data.unop.operand);
            break;
        }
        case NODE_CALL: {
            fmt_expr(out, expr->data.call.callee);
            fprintf(out, "(");
            for (int i = 0; i < expr->data.call.arg_count; i++) {
                if (i > 0) fprintf(out, ", ");
                fmt_expr(out, expr->data.call.args[i]);
            }
            fprintf(out, ")");
            break;
        }
        case NODE_INDEX:
            fmt_expr(out, expr->data.index.object);
            fprintf(out, "[");
            fmt_expr(out, expr->data.index.index);
            fprintf(out, "]");
            break;
        case NODE_FIELD_ACCESS:
            fmt_expr(out, expr->data.field_access.object);
            fprintf(out, ".%s", expr->data.field_access.field_name);
            break;
        case NODE_RECORD_LITERAL: {
            fprintf(out, "%s { ", expr->data.record_lit.type_name);
            for (int i = 0; i < expr->data.record_lit.field_count; i++) {
                if (i > 0) fprintf(out, ", ");
                fprintf(out, "%s: ", expr->data.record_lit.fields[i].name);
                fmt_expr(out, expr->data.record_lit.fields[i].value);
            }
            fprintf(out, " }");
            break;
        }
        case NODE_ARRAY_LITERAL: {
            fprintf(out, "[");
            for (int i = 0; i < expr->data.array_lit.count; i++) {
                if (i > 0) fprintf(out, ", ");
                fmt_expr(out, expr->data.array_lit.elements[i]);
            }
            fprintf(out, "]");
            break;
        }
        default:
            fprintf(out, "/* ? */");
            break;
    }
}

/* Format a statement */
static void fmt_stmt(FILE* out, forge_node_t* stmt, int indent) {
    if (!stmt) return;
    fmt_indent(out, indent);
    switch (stmt->kind) {
        case NODE_VAR_DECL:
            fprintf(out, "var %s", stmt->data.var_decl.name);
            if (stmt->data.var_decl.type_expr) {
                fprintf(out, ": ");
                fmt_type(out, stmt->data.var_decl.type_expr);
            }
            if (stmt->data.var_decl.init_expr) {
                fprintf(out, " = ");
                fmt_expr(out, stmt->data.var_decl.init_expr);
            }
            fprintf(out, "\n");
            break;
        case NODE_CONST_DECL:
            fprintf(out, "const %s", stmt->data.var_decl.name);
            if (stmt->data.var_decl.type_expr) {
                fprintf(out, ": ");
                fmt_type(out, stmt->data.var_decl.type_expr);
            }
            if (stmt->data.var_decl.init_expr) {
                fprintf(out, " = ");
                fmt_expr(out, stmt->data.var_decl.init_expr);
            }
            fprintf(out, "\n");
            break;
        case NODE_ASSIGN:
            fmt_expr(out, stmt->data.assign.target);
            fprintf(out, " = ");
            fmt_expr(out, stmt->data.assign.value);
            fprintf(out, "\n");
            break;
        case NODE_IF: {
            fprintf(out, "if ");
            fmt_expr(out, stmt->data.if_stmt.condition);
            fprintf(out, ":\n");
            fmt_block(out, stmt->data.if_stmt.then_body, indent + 1);
            /* Handle elif chains */
            for (int i = 0; i < stmt->data.if_stmt.elif_count; i++) {
                fmt_indent(out, indent);
                fprintf(out, "elif ");
                fmt_expr(out, stmt->data.if_stmt.elif_conditions[i]);
                fprintf(out, ":\n");
                fmt_block(out, stmt->data.if_stmt.elif_bodies[i], indent + 1);
            }
            if (stmt->data.if_stmt.else_body) {
                fmt_indent(out, indent);
                fprintf(out, "else:\n");
                fmt_block(out, stmt->data.if_stmt.else_body, indent + 1);
            }
            break;
        }
        case NODE_WHILE:
            fprintf(out, "while ");
            fmt_expr(out, stmt->data.while_stmt.condition);
            fprintf(out, ":\n");
            fmt_block(out, stmt->data.while_stmt.body, indent + 1);
            break;
        case NODE_FOR:
            fprintf(out, "for %s in ", stmt->data.for_stmt.var_name);
            fmt_expr(out, stmt->data.for_stmt.iterable);
            fprintf(out, ":\n");
            fmt_block(out, stmt->data.for_stmt.body, indent + 1);
            break;
        case NODE_RETURN:
            fprintf(out, "return");
            if (stmt->data.return_stmt.value) {
                fprintf(out, " ");
                fmt_expr(out, stmt->data.return_stmt.value);
            }
            fprintf(out, "\n");
            break;
        case NODE_BREAK:
            fprintf(out, "break\n");
            break;
        case NODE_CONTINUE:
            fprintf(out, "continue\n");
            break;
        case NODE_EXPR_STMT:
            fmt_expr(out, stmt->data.expr_stmt.expr);
            fprintf(out, "\n");
            break;
        case NODE_EMIT:
            fprintf(out, "emit %s(", stmt->data.emit_stmt.channel_name);
            fmt_expr(out, stmt->data.emit_stmt.payload);
            fprintf(out, ")\n");
            break;
        default:
            fprintf(out, "# ?\n");
            break;
    }
}

/* Format a block of statements */
static void fmt_block(FILE* out, forge_node_t* block, int indent) {
    if (!block || block->kind != NODE_BLOCK) {
        fmt_indent(out, indent);
        fprintf(out, "pass\n");
        return;
    }
    int count = block->data.block.count;
    if (count == 0) {
        fmt_indent(out, indent);
        fprintf(out, "pass\n");
        return;
    }
    for (int i = 0; i < count; i++) {
        fmt_stmt(out, block->data.block.stmts[i], indent);
    }
}

/* Format a procedure */
static void fmt_proc(FILE* out, forge_node_t* proc) {
    fprintf(out, "proc %s(", proc->data.proc.name);
    for (int i = 0; i < proc->data.proc.param_count; i++) {
        if (i > 0) fprintf(out, ", ");
        forge_param_t* param = &proc->data.proc.params[i];
        if (param->is_ref) fprintf(out, "ref ");
        fprintf(out, "%s: ", param->name);
        fmt_type(out, param->type_expr);
    }
    fprintf(out, ")");
    if (proc->data.proc.return_type) {
        fprintf(out, " -> ");
        fmt_type(out, proc->data.proc.return_type);
    }
    fprintf(out, ":\n");
    fmt_block(out, proc->data.proc.body, 1);
}

/* Format a record */
static void fmt_record(FILE* out, forge_node_t* rec) {
    fprintf(out, "record %s:\n", rec->data.record.name);
    for (int i = 0; i < rec->data.record.field_count; i++) {
        forge_node_t* field = rec->data.record.fields[i];
        fmt_indent(out, 1);
        fprintf(out, "%s: ", field->data.field_def.name);
        fmt_type(out, field->data.field_def.type_expr);
        fprintf(out, "\n");
    }
}

static int cmd_fmt(const forge_cli_args_t* args) {
    int source_len = 0;
    char* source = read_file(args->input_file, &source_len);
    if (!source) return 1;

    forge_strtable_t* strtable = strtable_create();
    forge_arena_t* arena = arena_create(65536);
    int exit_code = 0;

    /* Lex */
    forge_lexer_t* lexer = lexer_create(source, source_len, args->input_file, strtable);
    lexer_tokenize(lexer);
    if (lexer->had_error) {
        exit_code = 1;
        goto cleanup_lexer;
    }

    /* Parse */
    forge_parser_t* parser = parser_create(lexer->tokens.data, lexer->tokens.len,
                                           arena, strtable, args->input_file);
    forge_node_t* program = parser_parse(parser);
    if (parser_had_error(parser)) {
        exit_code = 1;
        goto cleanup_parser;
    }

    /* Format to a temp file, then overwrite the original */
    {
        char temp_path[4096];
        snprintf(temp_path, sizeof(temp_path), "%s.fmt.tmp", args->input_file);
        FILE* out = fopen(temp_path, "w");
        if (!out) {
            fprintf(stderr, "forge fmt: Cannot create temp file '%s'\n", temp_path);
            exit_code = 1;
            goto cleanup_parser;
        }

        /* Format imports */
        for (int i = 0; i < program->data.program.import_count; i++) {
            forge_node_t* imp = program->data.program.imports[i];
            fprintf(out, "import %s", imp->data.import.module_path);
            if (imp->data.import.alias) {
                fprintf(out, " as %s", imp->data.import.alias);
            }
            fprintf(out, "\n");
        }
        if (program->data.program.import_count > 0) {
            fprintf(out, "\n");
        }

        /* Format declarations with proper spacing */
        int prev_was_proc = 0;
        for (int i = 0; i < program->data.program.decl_count; i++) {
            forge_node_t* decl = program->data.program.decls[i];

            /* Add blank lines between declarations */
            if (i > 0) {
                if (prev_was_proc || decl->kind == NODE_PROC_DECL) {
                    fprintf(out, "\n\n");  /* Two blank lines around procs */
                } else {
                    fprintf(out, "\n");    /* One blank line otherwise */
                }
            }

            switch (decl->kind) {
                case NODE_PROC_DECL:
                    fmt_proc(out, decl);
                    prev_was_proc = 1;
                    break;
                case NODE_RECORD_DECL:
                    fmt_record(out, decl);
                    prev_was_proc = 0;
                    break;
                case NODE_VAR_DECL:
                case NODE_CONST_DECL:
                    fmt_stmt(out, decl, 0);
                    prev_was_proc = 0;
                    break;
                case NODE_CHANNEL_DECL:
                    fprintf(out, "channel %s: ", decl->data.channel.name);
                    fmt_type(out, decl->data.channel.payload_type);
                    fprintf(out, "\n");
                    prev_was_proc = 0;
                    break;
                default:
                    prev_was_proc = 0;
                    break;
            }
        }
        fprintf(out, "\n");  /* Final newline */
        fclose(out);

        /* Replace original with formatted */
        if (rename(temp_path, args->input_file) != 0) {
            fprintf(stderr, "forge fmt: Cannot rename '%s' to '%s'\n",
                    temp_path, args->input_file);
            unlink(temp_path);
            exit_code = 1;
            goto cleanup_parser;
        }

        if (!args->quiet) {
            printf("✓ Formatted %s\n", args->input_file);
        }
    }

cleanup_parser:
    parser_destroy(parser);
cleanup_lexer:
    lexer_destroy(lexer);
    arena_destroy(arena);
    strtable_destroy(strtable);
    free(source);
    return exit_code;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Command: emit (C code to stdout)
 * ───────────────────────────────────────────────────────────────────────────── */

static int cmd_emit(const forge_cli_args_t* args) {
    int source_len = 0;
    char* source = read_file(args->input_file, &source_len);
    if (!source) return 1;

    forge_strtable_t* strtable = strtable_create();
    forge_arena_t* arena = arena_create(65536);
    int exit_code = 0;

    /* Lex */
    forge_lexer_t* lexer = lexer_create(source, source_len, args->input_file, strtable);
    lexer_tokenize(lexer);
    if (lexer->had_error) {
        exit_code = 1;
        goto cleanup_lexer;
    }

    /* Parse */
    forge_parser_t* parser = parser_create(lexer->tokens.data, lexer->tokens.len,
                                           arena, strtable, args->input_file);
    forge_node_t* program = parser_parse(parser);
    if (parser_had_error(parser)) {
        exit_code = 1;
        goto cleanup_parser;
    }

    /* Type Check */
    forge_checker_t* checker = checker_create(arena, strtable, args->input_file);
    if (checker_check(checker, program) != 0) {
        exit_code = 1;
        goto cleanup_checker;
    }

    /* Emit C */
    {
        forge_emitter_t* emitter = emitter_create(stdout, arena, strtable,
                                                  "main", args->input_file);
        emitter_emit_program(emitter, program);
        emitter_destroy(emitter);
    }

cleanup_checker:
    checker_destroy(checker);
cleanup_parser:
    parser_destroy(parser);
cleanup_lexer:
    lexer_destroy(lexer);
    arena_destroy(arena);
    strtable_destroy(strtable);
    free(source);
    return exit_code;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Command: emit-llvm (LLVM IR to stdout)
 * ───────────────────────────────────────────────────────────────────────────── */

static int cmd_emit_llvm(const forge_cli_args_t* args) {
    int source_len = 0;
    char* source = read_file(args->input_file, &source_len);
    if (!source) return 1;

    forge_strtable_t* strtable = strtable_create();
    forge_arena_t* arena = arena_create(65536);
    int exit_code = 0;

    /* Lex */
    forge_lexer_t* lexer = lexer_create(source, source_len, args->input_file, strtable);
    lexer_tokenize(lexer);
    if (lexer->had_error) {
        exit_code = 1;
        goto cleanup_lexer;
    }

    /* Parse */
    forge_parser_t* parser = parser_create(lexer->tokens.data, lexer->tokens.len,
                                           arena, strtable, args->input_file);
    forge_node_t* program = parser_parse(parser);
    if (parser_had_error(parser)) {
        exit_code = 1;
        goto cleanup_parser;
    }

    /* Type Check */
    forge_checker_t* checker = checker_create(arena, strtable, args->input_file);
    if (checker_check(checker, program) != 0) {
        exit_code = 1;
        goto cleanup_checker;
    }

    /* Emit LLVM IR */
    {
        forge_llvm_emitter_t* llvm = llvm_emitter_create(stdout, arena, strtable,
                                                         "main", args->input_file);
        llvm_emitter_emit_program(llvm, program);
        llvm_emitter_destroy(llvm);
    }

cleanup_checker:
    checker_destroy(checker);
cleanup_parser:
    parser_destroy(parser);
cleanup_lexer:
    lexer_destroy(lexer);
    arena_destroy(arena);
    strtable_destroy(strtable);
    free(source);
    return exit_code;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Command: build (compile to native binary)
 * ───────────────────────────────────────────────────────────────────────────── */

static int cmd_build(const forge_cli_args_t* args) {
    char runtime_path[1024];
    if (find_runtime_path(args, runtime_path, sizeof(runtime_path)) != 0) {
        return 1;
    }

    int source_len = 0;
    char* source = read_file(args->input_file, &source_len);
    if (!source) return 1;

    forge_strtable_t* strtable = strtable_create();
    forge_arena_t* arena = arena_create(65536);
    int exit_code = 0;

    /* Lex */
    forge_lexer_t* lexer = lexer_create(source, source_len, args->input_file, strtable);
    lexer_tokenize(lexer);
    if (lexer->had_error) {
        exit_code = 1;
        goto cleanup_lexer;
    }

    /* Parse */
    forge_parser_t* parser = parser_create(lexer->tokens.data, lexer->tokens.len,
                                           arena, strtable, args->input_file);
    forge_node_t* program = parser_parse(parser);
    if (parser_had_error(parser)) {
        exit_code = 1;
        goto cleanup_parser;
    }

    /* Type Check */
    forge_checker_t* checker = checker_create(arena, strtable, args->input_file);
    if (checker_check(checker, program) != 0) {
        exit_code = 1;
        goto cleanup_checker;
    }

    /* Determine output file name */
    char output_name[1024];
    if (args->output_file) {
        snprintf(output_name, sizeof(output_name), "%s", args->output_file);
    } else {
        /* Default: input file without .fg extension */
        const char* base = args->input_file;
        const char* last_slash = strrchr(base, '/');
        if (last_slash) base = last_slash + 1;

        size_t len = strlen(base);
        if (len > 3 && strcmp(base + len - 3, ".fg") == 0) {
            snprintf(output_name, sizeof(output_name), "%.*s", (int)(len - 3), base);
        } else {
            snprintf(output_name, sizeof(output_name), "%s.out", base);
        }
    }

    /* Build via C backend */
    if (strcmp(args->target, "c") == 0) {
        /* Create temp file for C code */
        char c_file[1024];
        snprintf(c_file, sizeof(c_file), "/tmp/forge_%d.c", (int)getpid());

        FILE* c_out = fopen(c_file, "w");
        if (!c_out) {
            fprintf(stderr, "forge: Cannot create temp file '%s'\n", c_file);
            exit_code = 1;
            goto cleanup_checker;
        }

        /* Emit C */
        forge_emitter_t* emitter = emitter_create(c_out, arena, strtable,
                                                  "main", args->input_file);
        emitter_emit_program(emitter, program);
        emitter_destroy(emitter);
        fclose(c_out);

        /* Invoke compiler */
        char cmd[4096];
        snprintf(cmd, sizeof(cmd),
            "%s -std=c99 -O%d %s -I%s %s/forge_runtime.c %s -o %s -lm",
            args->cc,
            args->opt_level,
            args->debug ? "-g" : "",
            runtime_path,
            runtime_path,
            c_file,
            output_name);

        if (args->verbose) {
            printf("$ %s\n", cmd);
        }

        int result = system(cmd);
        unlink(c_file);  /* Remove temp file */

        if (result != 0) {
            fprintf(stderr, "forge: Compilation failed\n");
            exit_code = 1;
            goto cleanup_checker;
        }

        if (!args->quiet) {
            printf("✓ Built: %s\n", output_name);
        }
    }
    /* Build via LLVM backend */
    else if (strcmp(args->target, "llvm") == 0) {
        /* Create temp file for LLVM IR */
        char ll_file[1024], s_file[1024];
        snprintf(ll_file, sizeof(ll_file), "/tmp/forge_%d.ll", (int)getpid());
        snprintf(s_file, sizeof(s_file), "/tmp/forge_%d.s", (int)getpid());

        FILE* ll_out = fopen(ll_file, "w");
        if (!ll_out) {
            fprintf(stderr, "forge: Cannot create temp file '%s'\n", ll_file);
            exit_code = 1;
            goto cleanup_checker;
        }

        /* Emit LLVM IR */
        forge_llvm_emitter_t* llvm = llvm_emitter_create(ll_out, arena, strtable,
                                                         "main", args->input_file);
        llvm_emitter_emit_program(llvm, program);
        llvm_emitter_destroy(llvm);
        fclose(ll_out);

        /* Compile LLVM IR to assembly */
        char cmd[4096];
        snprintf(cmd, sizeof(cmd), "llc-16 --relocation-model=pic -O%d %s -o %s",
                 args->opt_level, ll_file, s_file);

        if (args->verbose) {
            printf("$ %s\n", cmd);
        }

        if (system(cmd) != 0) {
            fprintf(stderr, "forge: LLVM compilation failed (llc-16)\n");
            unlink(ll_file);
            exit_code = 1;
            goto cleanup_checker;
        }

        /* Link with runtime */
        snprintf(cmd, sizeof(cmd),
            "%s -O%d %s -I%s %s %s/forge_runtime.c -o %s -lm",
            args->cc,
            args->opt_level,
            args->debug ? "-g" : "",
            runtime_path,
            s_file,
            runtime_path,
            output_name);

        if (args->verbose) {
            printf("$ %s\n", cmd);
        }

        int result = system(cmd);
        unlink(ll_file);
        unlink(s_file);

        if (result != 0) {
            fprintf(stderr, "forge: Linking failed\n");
            exit_code = 1;
            goto cleanup_checker;
        }

        if (!args->quiet) {
            printf("✓ Built (LLVM): %s\n", output_name);
        }
    }

cleanup_checker:
    checker_destroy(checker);
cleanup_parser:
    parser_destroy(parser);
cleanup_lexer:
    lexer_destroy(lexer);
    arena_destroy(arena);
    strtable_destroy(strtable);
    free(source);
    return exit_code;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Command: repl (interactive shell)
 * ───────────────────────────────────────────────────────────────────────────── */

static char* trim_str(char* s) {
    while (*s && (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r')) s++;
    if (*s == '\0') return s;
    char* end = s + strlen(s) - 1;
    while (end > s && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) end--;
    end[1] = '\0';
    return s;
}

/*
 * REPL: Execute a single REPL entry (statement, expression, or declaration).
 * Returns 1 if the entry was processed, 0 on error.
 */
static int repl_execute(forge_interp_t* interp, forge_arena_t* arena,
                        forge_strtable_t* strtable, const char* input) {
    /* Determine if this is a top-level declaration or a statement/expression */
    const char* trimmed = input;
    while (*trimmed == ' ' || *trimmed == '\t') trimmed++;

    int is_decl = (strncmp(trimmed, "proc ", 5) == 0 ||
                   strncmp(trimmed, "record ", 7) == 0 ||
                   strncmp(trimmed, "const ", 6) == 0 ||
                   strncmp(trimmed, "var ", 4) == 0 ||
                   strncmp(trimmed, "channel ", 8) == 0 ||
                   strncmp(trimmed, "type ", 5) == 0 ||
                   strncmp(trimmed, "import ", 7) == 0 ||
                   strncmp(trimmed, "on ", 3) == 0);

    /* Build the source to parse */
    char source[70000];
    if (is_decl) {
        /* Top-level declaration: parse as-is in a minimal program */
        snprintf(source, sizeof(source), "%s\n", input);
    } else {
        /* Statement/expression: wrap in a __repl_eval__ procedure */
        snprintf(source, sizeof(source),
                 "proc __repl_eval__() -> void:\n"
                 "    %s\n",
                 input);
    }

    /* Lex */
    forge_lexer_t* lexer = lexer_create(source, strlen(source), "<repl>", strtable);
    lexer_tokenize(lexer);

    if (lexer->had_error) {
        lexer_destroy(lexer);
        return 0;
    }

    /* Parse */
    forge_parser_t* parser = parser_create(lexer->tokens.data, lexer->tokens.len,
                                           arena, strtable, "<repl>");
    forge_node_t* program = parser_parse(parser);

    if (parser_had_error(parser)) {
        parser_destroy(parser);
        lexer_destroy(lexer);
        return 0;
    }

    /* Skip type checking in REPL - rely on interpreter's runtime checking.
     * A persistent type checker would need to share state with the interpreter,
     * which is complex. For now, we accept runtime errors instead. */

    /* Execute based on what we parsed */
    interp->had_error = 0;  /* Clear any previous error */

    if (is_decl) {
        /* Register declarations in the interpreter */
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
                    interp_register_handler(interp, decl, NULL);
                    break;
                case NODE_TYPE_ALIAS:
                    /* Type aliases handled at type-check time */
                    break;
                default:
                    break;
            }
        }

        /* Handle imports */
        int import_count = program->data.program.import_count;
        forge_node_t** imports = program->data.program.imports;
        for (int i = 0; i < import_count; i++) {
            forge_node_t* imp = imports[i];
            if (imp->kind == NODE_IMPORT) {
                /* For now, just mark stdlib modules as available */
                const char* mod = imp->data.import.module_path;
                if (strncmp(mod, "forge.", 6) == 0) {
                    /* Stdlib module - already available */
                }
            }
        }
    } else {
        /* Execute the __repl_eval__ procedure */
        /* First, register it */
        int decl_count = program->data.program.decl_count;
        forge_node_t** decls = program->data.program.decls;
        forge_node_t* eval_proc = NULL;

        for (int i = 0; i < decl_count; i++) {
            if (decls[i]->kind == NODE_PROC_DECL &&
                strcmp(decls[i]->data.proc.name, "__repl_eval__") == 0) {
                eval_proc = decls[i];
                break;
            }
        }

        if (eval_proc) {
            /* Execute the procedure body directly in the global environment */
            forge_node_t* body = eval_proc->data.proc.body;
            if (body && body->kind == NODE_BLOCK) {
                int stmt_count = body->data.block.count;
                forge_node_t** stmts = body->data.block.stmts;

                for (int i = 0; i < stmt_count && !interp->had_error; i++) {
                    forge_node_t* stmt = stmts[i];

                    /* Check if this is an expression statement */
                    if (stmt->kind == NODE_EXPR_STMT) {
                        forge_value_t result = interp_eval_expr(interp, interp->globals,
                                                                 stmt->data.expr_stmt.expr);
                        if (!interp->had_error && result.kind != VAL_VOID &&
                            result.kind != VAL_NONE) {
                            /* Print expression result */
                            char* str_repr = val_to_str(result);
                            if (str_repr) {
                                printf("%s\n", str_repr);
                                forge_free(str_repr);
                            }
                        }
                        val_free(&result);
                    } else {
                        /* Execute statement */
                        forge_result_t res = interp_exec_stmt(interp, interp->globals, stmt);
                        val_free(&res.value);
                    }
                }
            }
        }
    }

    parser_destroy(parser);
    lexer_destroy(lexer);

    return !interp->had_error;
}

/*
 * REPL loop: Runs the interactive read-eval-print loop.
 * Can be called with an existing interpreter (e.g., after running main()).
 */
static void repl_loop(forge_interp_t* interp, forge_arena_t* arena,
                      forge_strtable_t* strtable) {
    char line[4096];
    char buffer[65536];
    size_t buffer_len = 0;
    int in_block = 0;

    while (1) {
        /* Print prompt */
        printf(in_block ? "... " : ">>> ");
        fflush(stdout);

        /* Read line */
        if (!fgets(line, sizeof(line), stdin)) {
            printf("\n");
            break;
        }

        /* Remove trailing newline for trimming */
        size_t line_len = strlen(line);
        if (line_len > 0 && line[line_len - 1] == '\n') {
            line[line_len - 1] = '\0';
            line_len--;
        }

        /* Trim for command checking */
        char* trimmed = trim_str(line);

        /* Check for REPL commands (only when not in a block) */
        if (!in_block) {
            if (strcmp(trimmed, "exit") == 0 || strcmp(trimmed, "quit") == 0) {
                break;
            }
            if (strcmp(trimmed, "help") == 0) {
                printf("REPL Commands:\n");
                printf("  exit, quit    Exit the REPL\n");
                printf("  help          Show this help\n");
                printf("\n");
                printf("Enter FORGE code directly. Variables and procedures persist.\n");
                printf("Multi-line input: end a line with ':' to start a block,\n");
                printf("then enter an empty line to execute.\n");
                continue;
            }
            /* Empty line outside block - skip */
            if (trimmed[0] == '\0') {
                continue;
            }
        }

        /* Empty line in block mode ends the block */
        if (in_block && trimmed[0] == '\0') {
            in_block = 0;
            buffer[buffer_len] = '\0';

            /* Execute the buffered code */
            if (buffer_len > 0) {
                repl_execute(interp, arena, strtable, buffer);
            }

            buffer_len = 0;
            continue;
        }

        /* Append line to buffer (with newline for multi-line) */
        if (buffer_len + line_len + 2 >= sizeof(buffer)) {
            fprintf(stderr, "repl: Input too long\n");
            buffer_len = 0;
            in_block = 0;
            continue;
        }

        if (buffer_len > 0) {
            buffer[buffer_len++] = '\n';
        }
        memcpy(buffer + buffer_len, line, line_len);
        buffer_len += line_len;
        buffer[buffer_len] = '\0';

        /* Check if entering a block (line ends with ':') */
        size_t len = strlen(trimmed);
        if (len > 0 && trimmed[len - 1] == ':') {
            in_block = 1;
            continue;
        }

        if (in_block) continue;

        /* Single-line input - execute immediately */
        repl_execute(interp, arena, strtable, buffer);
        buffer_len = 0;
    }
}

static int cmd_repl(const forge_cli_args_t* args) {
    (void)args;

    printf("FORGE %s REPL — type 'exit' or Ctrl-D to quit, 'help' for commands\n",
           FORGE_VERSION_STRING);

    /* Create persistent interpreter state */
    forge_strtable_t* strtable = strtable_create();
    forge_arena_t* arena = arena_create(1024 * 1024);  /* 1MB arena */
    forge_interp_t* interp = interp_create(arena, strtable);
    interp->current_filename = "<repl>";

    /* Run the REPL loop */
    repl_loop(interp, arena, strtable);

    /* Cleanup */
    interp_destroy(interp);
    arena_destroy(arena);
    strtable_destroy(strtable);

    return 0;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Command: doc (generate documentation)
 * ───────────────────────────────────────────────────────────────────────────── */

/* Helper to get type name from a type expression node */
static const char* get_type_name(forge_node_t* type_expr) {
    if (!type_expr) return "?";
    switch (type_expr->kind) {
        case NODE_TYPE_PRIM:
        case NODE_TYPE_NAMED:
            return type_expr->data.name ? type_expr->data.name : "?";
        case NODE_TYPE_OPTIONAL:
            return "?T";  /* Simplified for docs */
        case NODE_TYPE_FIXED_ARRAY:
        case NODE_TYPE_DYN_ARRAY:
            return "[]T";
        case NODE_TYPE_MAP:
            return "map[K,V]";
        case NODE_TYPE_REF:
            return "ref T";
        default:
            return "?";
    }
}

static int cmd_doc(const forge_cli_args_t* args) {
    int source_len = 0;
    char* source = read_file(args->input_file, &source_len);
    if (!source) return 1;

    forge_strtable_t* strtable = strtable_create();
    forge_arena_t* arena = arena_create(65536);
    int exit_code = 0;

    /* Lex */
    forge_lexer_t* lexer = lexer_create(source, source_len, args->input_file, strtable);
    lexer_tokenize(lexer);
    if (lexer->had_error) {
        exit_code = 1;
        goto cleanup_lexer;
    }

    /* Parse */
    forge_parser_t* parser = parser_create(lexer->tokens.data, lexer->tokens.len,
                                           arena, strtable, args->input_file);
    forge_node_t* program = parser_parse(parser);
    if (parser_had_error(parser)) {
        exit_code = 1;
        goto cleanup_parser;
    }

    /* Generate documentation */
    printf("# %s\n\n", args->input_file);
    printf("*Generated documentation for FORGE source file.*\n\n");

    if (program && program->kind == NODE_PROGRAM) {
        int decl_count = program->data.program.decl_count;
        forge_node_t** decls = program->data.program.decls;

        /* List records */
        int found_record = 0;
        for (int i = 0; i < decl_count; i++) {
            forge_node_t* decl = decls[i];
            if (decl && decl->kind == NODE_RECORD_DECL) {
                if (!found_record) {
                    printf("## Records\n\n");
                    found_record = 1;
                }
                printf("### `%s`\n\n", decl->data.record.name);
                printf("```forge\nrecord %s:\n", decl->data.record.name);
                for (int j = 0; j < decl->data.record.field_count; j++) {
                    forge_node_t* field = decl->data.record.fields[j];
                    if (field && field->kind == NODE_FIELD_DEF) {
                        printf("    %s: %s\n",
                               field->data.field_def.name,
                               get_type_name(field->data.field_def.type_expr));
                    }
                }
                printf("```\n\n");
            }
        }

        /* List procedures */
        int found_proc = 0;
        for (int i = 0; i < decl_count; i++) {
            forge_node_t* decl = decls[i];
            if (decl && decl->kind == NODE_PROC_DECL) {
                /* Skip internal procs */
                const char* name = decl->data.proc.name;
                if (name && name[0] == '_' && name[1] == '_') continue;
                if (name && strcmp(name, "main") == 0) continue;

                if (!found_proc) {
                    printf("## Procedures\n\n");
                    found_proc = 1;
                }

                printf("### `%s`\n\n", name);
                printf("```forge\nproc %s(", name);

                /* Parameters (forge_param_t array, not nodes) */
                int param_count = decl->data.proc.param_count;
                forge_param_t* params = decl->data.proc.params;
                for (int j = 0; j < param_count; j++) {
                    if (j > 0) printf(", ");
                    printf("%s: %s",
                           params[j].name,
                           get_type_name(params[j].type_expr));
                }
                printf(")");

                /* Return type */
                if (decl->data.proc.return_type) {
                    printf(" -> %s", get_type_name(decl->data.proc.return_type));
                }
                printf("\n```\n\n");
            }
        }

        if (!found_record && !found_proc) {
            printf("*No exported declarations found.*\n");
        }
    }

cleanup_parser:
    parser_destroy(parser);
cleanup_lexer:
    lexer_destroy(lexer);
    arena_destroy(arena);
    strtable_destroy(strtable);
    free(source);
    return exit_code;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Main Entry Point
 * ───────────────────────────────────────────────────────────────────────────── */

int main(int argc, char** argv) {
    forge_cli_args_t args;

    if (parse_args(argc, argv, &args) != 0) {
        return 1;
    }

    switch (args.command) {
        case CMD_VERSION:
            print_version();
            return 0;

        case CMD_HELP:
            print_usage(argv[0]);
            return 0;

        case CMD_RUN:
            return cmd_run(&args);

        case CMD_BUILD:
            return cmd_build(&args);

        case CMD_CHECK:
            return cmd_check(&args);

        case CMD_EMIT:
            return cmd_emit(&args);

        case CMD_EMIT_LLVM:
            return cmd_emit_llvm(&args);

        case CMD_REPL:
            return cmd_repl(&args);

        case CMD_DOC:
            return cmd_doc(&args);

        case CMD_FMT:
            return cmd_fmt(&args);

        case CMD_NONE:
        default:
            print_usage(argv[0]);
            return 1;
    }
}
