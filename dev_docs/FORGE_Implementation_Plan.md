# FORGE Implementation Plan

**Document:** Development Roadmap and Technical Implementation Guide  
**Version:** 0.1  
**Status:** Active  
**Companion:** `FORGE_Language_Spec.md` v0.1  
**Author:** Fragillidae Software  
**Implementation Language:** C (C99 standard)

---

> This document directs the development effort for the FORGE toolchain. It defines phases, milestones, deliverables, task breakdowns, file structures, coding standards, and testing strategy. Read it alongside the Language Specification. Where this document conflicts with the spec, the spec takes precedence on language semantics; this document takes precedence on build order and implementation decisions.

---

## Table of Contents

1. [Project Structure](#1-project-structure)
2. [Development Philosophy](#2-development-philosophy)
3. [Phase Overview](#3-phase-overview)
4. [Phase 1 — Lexer](#4-phase-1--lexer)
5. [Phase 2 — Parser and AST](#5-phase-2--parser-and-ast)
6. [Phase 3 — Tree-Walking Interpreter](#6-phase-3--tree-walking-interpreter)
7. [Phase 4 — Type Checker](#7-phase-4--type-checker)
8. [Phase 5a — C Emitter](#8-phase-5a--c-emitter)
9. [Phase 5b — LLVM IR Emitter](#9-phase-5b--llvm-ir-emitter)
10. [Phase 6 — Standard Library](#10-phase-6--standard-library)
11. [Phase 7 — Toolchain CLI](#11-phase-7--toolchain-cli)
12. [Phase 8 — Tooling and Ecosystem](#12-phase-8--tooling-and-ecosystem)
13. [Testing Strategy](#13-testing-strategy)
14. [Coding Standards](#14-coding-standards)
15. [Milestone Summary](#15-milestone-summary)
16. [Risk Register](#16-risk-register)
17. [Appendix A — File Tree](#appendix-a--file-tree)
18. [Appendix B — Key Data Structures Cheatsheet](#appendix-b--key-data-structures-cheatsheet)
19. [Appendix C — Test File Naming Convention](#appendix-c--test-file-naming-convention)

---

## 1. Project Structure

### 1.1 Repository Layout

```
forge/
├── README.md
├── LICENSE
├── Makefile
├── .gitignore
│
├── src/                        # Toolchain source (C)
│   ├── main.c                  # CLI entry point
│   ├── common.h                # Shared types, macros, forward decls
│   ├── memory.h / memory.c     # Arena allocator, general alloc utilities
│   │
│   ├── lexer/
│   │   ├── lexer.h
│   │   └── lexer.c
│   │
│   ├── parser/
│   │   ├── ast.h               # AST node types and structures
│   │   ├── ast.c               # AST construction helpers
│   │   ├── parser.h
│   │   └── parser.c
│   │
│   ├── typecheck/
│   │   ├── types.h             # Type representation
│   │   ├── types.c
│   │   ├── checker.h
│   │   └── checker.c
│   │
│   ├── interp/
│   │   ├── value.h             # forge_value_t and operations
│   │   ├── value.c
│   │   ├── env.h               # Environment / scope stack
│   │   ├── env.c
│   │   ├── channel.h           # Channel registry
│   │   ├── channel.c
│   │   ├── interp.h
│   │   └── interp.c
│   │
│   ├── emit_c/
│   │   ├── emit_c.h
│   │   └── emit_c.c
│   │
│   ├── emit_llvm/              # Phase 5b — added later
│   │   ├── emit_llvm.h
│   │   └── emit_llvm.c
│   │
│   └── util/
│       ├── strtable.h          # String interning table
│       ├── strtable.c
│       ├── hashmap.h           # Generic open-addressing hash map
│       ├── hashmap.c
│       ├── dynarray.h          # Generic growable array
│       ├── dynarray.c
│       ├── error.h             # Error reporting and formatting
│       └── error.c
│
├── runtime/                    # C runtime support (compiled with emitted code)
│   ├── forge_runtime.h
│   └── forge_runtime.c
│
├── stdlib/                     # FORGE standard library (.fg source)
│   ├── forge/
│   │   ├── io.fg
│   │   ├── str.fg
│   │   ├── math.fg
│   │   ├── sys.fg
│   │   ├── time.fg
│   │   ├── buf.fg
│   │   ├── serial.fg
│   │   └── nmea.fg
│   └── bootstrap/              # C implementations for stdlib builtins
│       ├── io_builtin.c
│       ├── str_builtin.c
│       ├── math_builtin.c
│       ├── sys_builtin.c
│       ├── time_builtin.c
│       ├── buf_builtin.c
│       ├── serial_builtin.c
│       └── nmea_builtin.c
│
├── tests/
│   ├── unit/                   # C unit tests for each component
│   │   ├── test_lexer.c
│   │   ├── test_parser.c
│   │   ├── test_typecheck.c
│   │   ├── test_interp.c
│   │   └── test_emit_c.c
│   ├── forge/                  # FORGE source test programs
│   │   ├── 01_lexer/
│   │   ├── 02_parser/
│   │   ├── 03_interp/
│   │   ├── 04_typecheck/
│   │   ├── 05_emit_c/
│   │   └── programs/           # Full programs for integration testing
│   └── runner.sh               # Test runner script
│
├── examples/                   # Example FORGE programs
│   ├── hello.fg
│   ├── fibonacci.fg
│   ├── sensor_pipeline.fg
│   ├── nmea_monitor.fg
│   └── event_demo.fg
│
└── docs/
    ├── FORGE_Language_Spec.md
    ├── FORGE_Implementation_Plan.md  ← this document
    └── CHANGELOG.md
```

### 1.2 Build System

Use a single `Makefile`. No CMake, no autotools. Keep the build simple.

```makefile
CC      = gcc
CFLAGS  = -std=c99 -Wall -Wextra -Wpedantic -g
RELEASE = -std=c99 -O2 -DNDEBUG

SRC     = $(wildcard src/**/*.c) $(wildcard src/*.c)
OBJ     = $(SRC:.c=.o)
TARGET  = forge

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

release:
	$(CC) $(RELEASE) -o $(TARGET) $(SRC)

test:
	bash tests/runner.sh

clean:
	rm -f $(OBJ) $(TARGET)
```

---

## 2. Development Philosophy

### 2.1 Build It Working Before Building It Right

The priority order for each phase is:

1. **Correct** — it does what the spec says
2. **Clear** — the code is readable and maintainable
3. **Complete** — handles all edge cases
4. **Fast** — optimize only after 1–3 are satisfied

Do not prematurely optimize the lexer, parser, or interpreter. These are not performance-critical until you are processing tens of thousands of lines of FORGE source. Educational programs are small.

### 2.2 One Phase Must Be Solid Before the Next Begins

Each phase has a defined exit gate — a set of tests that must pass before moving on. Do not start Phase 2 (parser) until Phase 1 (lexer) passes all its tests. The phases build on each other; bugs that slip through early compound badly in later phases.

### 2.3 Test Programs Drive Everything

For every language feature you implement, write a FORGE test program that exercises it. The test programs in `tests/forge/` are the ground truth for what the toolchain must handle. Write the test first, then implement until it passes. This is lightweight test-driven development without a heavy framework.

### 2.4 The Interpreter Is the Reference Implementation

Until the type checker and C emitter are complete, `forge run` is the canonical way to validate that FORGE programs behave correctly. The C emitter must produce programs that behave identically to `forge run` on all valid inputs. When there is a discrepancy, the interpreter wins.

### 2.5 Prefer Explicit Over Clever in the Toolchain Code

The FORGE toolchain is also an educational artifact — you may show it to students or use it to teach compiler construction. Write C that a third-year CS student could read. Avoid macro metaprogramming, function pointer tables with no comments, and overly terse variable names.

---

## 3. Phase Overview

```
Phase 1  ──  Lexer                       ▓▓▓▓▓░░░░░░░░░░░░░░░░░░░░░░░░
Phase 2  ──  Parser + AST                ░░░░░▓▓▓▓▓▓░░░░░░░░░░░░░░░░░░
Phase 3  ──  Tree-Walking Interpreter    ░░░░░░░░░░░▓▓▓▓▓▓▓░░░░░░░░░░░
Phase 4  ──  Type Checker               ░░░░░░░░░░░░░░░░░░▓▓▓▓▓░░░░░░
Phase 5a ──  C Emitter                  ░░░░░░░░░░░░░░░░░░░░░░░▓▓▓▓▓░
Phase 5b ──  LLVM IR Emitter            ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░▓  (optional/later)
Phase 6  ──  Standard Library           ░░░░░░░░░░▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓  (runs parallel to 3–5)
Phase 7  ──  Toolchain CLI              ░░░░░░░░░░░░░░░░░░░░░░░░░▓▓▓▓
Phase 8  ──  Tooling + Ecosystem        ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░▓  (post-v1)
```

| Phase | Deliverable | Entry Gate | Exit Gate |
|-------|-------------|------------|-----------|
| 1 | Lexer | — | All lexer unit tests pass |
| 2 | Parser + AST | Phase 1 complete | All parser unit tests pass; can pretty-print AST |
| 3 | Interpreter | Phase 2 complete | All interpreter test programs run correctly |
| 4 | Type Checker | Phase 3 complete | Type errors caught; typed programs run correctly |
| 5a | C Emitter | Phase 4 complete | All interpreter test programs produce identical output when compiled |
| 5b | LLVM Emitter | Phase 5a complete | Same parity tests pass via LLVM path |
| 6 | Standard Library | Phase 3 started | All stdlib modules functional in both interp and compiled modes |
| 7 | Toolchain CLI | Phase 5a complete | `forge run`, `forge build`, `forge check`, `forge fmt`, `forge repl` all functional |
| 8 | Tooling | Phase 7 complete | Editor integration, doc generator, packaging |

---

## 4. Phase 1 — Lexer

### 4.1 Goal

Produce a correct, complete tokenizer for all FORGE source. The lexer reads a UTF-8 source buffer and emits a flat array of `forge_token_t`. It is the only component that touches raw source bytes.

### 4.2 Files

```
src/lexer/lexer.h
src/lexer/lexer.c
src/util/strtable.h       # needed for string interning
src/util/strtable.c
tests/unit/test_lexer.c
tests/forge/01_lexer/
```

### 4.3 Data Structures

```c
/* lexer.h */

typedef enum {
    /* Literals */
    TOK_INT_LIT, TOK_FLOAT_LIT, TOK_STR_LIT, TOK_RAW_STR_LIT,
    /* Bool handled as keywords */
    /* Keywords — one per reserved word */
    TOK_AND, TOK_AS, TOK_BOOL, TOK_BREAK, TOK_BYTE,
    TOK_CHANNEL, TOK_CONST, TOK_CONTINUE, TOK_ELIF, TOK_ELSE,
    TOK_EMIT, TOK_EXPORT, TOK_FALSE, TOK_FLOAT_KW, TOK_FLOAT32_KW,
    TOK_FOR, TOK_FREE, TOK_IF, TOK_IMPORT, TOK_IN,
    TOK_INT_KW, TOK_INT8_KW, TOK_INT16_KW, TOK_INT32_KW,
    TOK_IS, TOK_LOOP, TOK_MAP, TOK_NONE, TOK_NOT,
    TOK_ON, TOK_OR, TOK_OR_ELSE, TOK_PANIC, TOK_PROC,
    TOK_RANGE, TOK_RECORD, TOK_REF, TOK_RETURN,
    TOK_SOME, TOK_STR_KW, TOK_TRUE, TOK_TYPE,
    TOK_UINT_KW, TOK_UINT8_KW, TOK_UINT16_KW, TOK_UINT32_KW,
    TOK_VAR, TOK_VOID, TOK_WHILE, TOK_WITH,
    /* Identifiers */
    TOK_IDENT,
    /* Operators */
    TOK_PLUS, TOK_MINUS, TOK_STAR, TOK_SLASH, TOK_PERCENT,
    TOK_EQ, TOK_NEQ, TOK_LT, TOK_GT, TOK_LEQ, TOK_GEQ,
    TOK_AMP, TOK_PIPE, TOK_CARET, TOK_TILDE,
    TOK_LSHIFT, TOK_RSHIFT,
    TOK_ASSIGN,
    TOK_PLUS_EQ, TOK_MINUS_EQ, TOK_STAR_EQ, TOK_SLASH_EQ,
    TOK_PERCENT_EQ, TOK_AMP_EQ, TOK_PIPE_EQ, TOK_CARET_EQ,
    TOK_LSHIFT_EQ, TOK_RSHIFT_EQ,
    TOK_ARROW,           /* -> */
    TOK_DOTDOT,          /* .. */
    TOK_DOTDOT_EQ,       /* ..= */
    TOK_DOT, TOK_COMMA, TOK_COLON, TOK_QUESTION,
    TOK_LPAREN, TOK_RPAREN,
    TOK_LBRACKET, TOK_RBRACKET,
    TOK_LBRACE, TOK_RBRACE,
    TOK_BACKSLASH,       /* line continuation */
    /* Structural */
    TOK_NEWLINE,
    TOK_INDENT,
    TOK_DEDENT,
    TOK_EOF,
    TOK_ERROR
} forge_token_type_t;

typedef struct {
    forge_token_type_t  type;
    const char*         start;      /* pointer into source buffer (not owned) */
    int                 length;     /* byte length in source */
    int                 line;
    int                 column;
    union {
        long long       int_val;    /* TOK_INT_LIT */
        double          float_val;  /* TOK_FLOAT_LIT */
        const char*     str_val;    /* TOK_STR_LIT, TOK_IDENT — interned */
    };
} forge_token_t;

typedef struct {
    const char*     source;         /* full source buffer */
    int             source_len;
    const char*     filename;
    int             pos;            /* current position in source */
    int             line;
    int             column;
    /* Indentation tracking */
    int             indent_stack[256];
    int             indent_depth;
    int             pending_dedents; /* may need to emit multiple DEDENTs */
    int             at_line_start;
    /* Token output */
    forge_token_t*  tokens;         /* dynamic array of tokens */
    int             token_count;
    int             token_cap;
    /* String table for interning */
    forge_strtable_t* strtable;
    /* Error state */
    int             had_error;
} forge_lexer_t;

/* Public API */
forge_lexer_t* lexer_create(const char* source, int len, const char* filename,
                             forge_strtable_t* strtable);
void           lexer_tokenize(forge_lexer_t* lex);
void           lexer_destroy(forge_lexer_t* lex);
void           lexer_print_tokens(forge_lexer_t* lex);   /* debug */
const char*    token_type_name(forge_token_type_t type); /* debug */
```

### 4.4 Implementation Tasks

#### Task 1.1 — Utility: String Intern Table

Before writing the lexer, build `strtable.h/c`. This is a hash-based set of deduplicated strings. Every identifier and string literal is interned — stored once, referenced by pointer. Pointer comparison then replaces string comparison throughout the toolchain.

```c
forge_strtable_t* strtable_create(void);
const char*       strtable_intern(forge_strtable_t* t, const char* s, int len);
void              strtable_destroy(forge_strtable_t* t);
```

Implementation: open-addressing hash table, FNV-1a hash, linear probing, load factor ≤ 0.75, doubles capacity on resize.

#### Task 1.2 — Utility: Dynamic Array for Tokens

Build `dynarray.h` — a simple typed growable array. Use it for the token output array and reuse it throughout the project.

```c
/* Use a macro-based generic or just a concrete forge_token_t array */
/* Simplest approach for C99: concrete token array with realloc */
void tokens_push(forge_lexer_t* lex, forge_token_t tok);
```

#### Task 1.3 — Core Lexer: Single-Character Tokens

Implement the main `lexer_tokenize` loop. Start with single-character tokens: `( ) [ ] { } , . : ? ~ \ `. Test after each group.

#### Task 1.4 — Core Lexer: Multi-Character Operators

Add two-character operators: `->`, `..`, `..=`, `==`, `!=`, `<=`, `>=`, `<<`, `>>`, `+=`, `-=`, `*=`, `/=`, `%=`, `&=`, `|=`, `^=`, `<<=`, `>>=`.

Implement as a peek-ahead: consume the first character, peek at the next, decide.

#### Task 1.5 — Core Lexer: Integer Literals

Handle decimal, hex (`0x`), binary (`0b`), octal (`0o`), and underscore separators. Store parsed value in `tok.int_val`.

```c
/* Pseudocode */
if (c == '0' && peek() == 'x') { scan_hex_literal(); }
else if (c == '0' && peek() == 'b') { scan_binary_literal(); }
else if (c == '0' && peek() == 'o') { scan_octal_literal(); }
else { scan_decimal_literal(); }
/* Check for '.' to distinguish int from float */
```

#### Task 1.6 — Core Lexer: Float Literals

After scanning digits, check for `.` followed by a digit (not `..`). Handle exponent notation `e`/`E`.

Edge case: `1..10` is `INT_LIT(1)`, `DOTDOT`, `INT_LIT(10)` — not `FLOAT_LIT(1.)`.

#### Task 1.7 — Core Lexer: String Literals

Quoted strings: scan until closing `"`, processing escape sequences. Produce an interned, escape-resolved `str_val`.

Raw strings: scan until closing `` ` ``, no escape processing.

Error on unterminated string (EOF before closing delimiter).

#### Task 1.8 — Core Lexer: Identifiers and Keywords

Scan identifier: `[a-zA-Z][a-zA-Z0-9_]*`. Intern the string. Look it up in the keyword table. Emit the appropriate keyword token or `TOK_IDENT`.

```c
/* Keyword lookup — sorted array + binary search, or hash map */
static const struct { const char* word; forge_token_type_t type; } keywords[] = {
    { "and",     TOK_AND },
    { "as",      TOK_AS },
    /* ... all keywords sorted alphabetically ... */
};
```

#### Task 1.9 — Indentation Tracking

This is the most complex part of the lexer. Implement INDENT/DEDENT emission.

**Algorithm:**

```
At the start of each new logical line (after all pending DEDENTs are emitted):
  1. Count leading spaces. If a tab is found: lexer_error("tabs not allowed").
  2. If this is a blank line or comment-only line: skip (do not change indent).
  3. Compare spaces to indent_stack[indent_depth]:
     a. If greater: push to stack, emit INDENT.
     b. If equal: no action.
     c. If less: pop stack and emit DEDENT until stack matches.
        If no stack level matches: lexer_error("inconsistent dedent").
  4. At EOF: emit all remaining DEDENTs.
```

Key edge case: multiple DEDENTs may need to be emitted for a single line. Buffer them in `pending_dedents` and drain before emitting the line's first real token.

#### Task 1.10 — Comments and Whitespace

Skip `#` through end-of-line. Skip spaces and tabs *within* a line (non-indentation whitespace). Track line and column numbers accurately for error reporting.

#### Task 1.11 — Line Continuation

If the last non-whitespace character before a newline is `\`, suppress the NEWLINE token and continue the logical line.

Also suppress NEWLINE inside unmatched `(`, `[`, `{`. Maintain a nesting counter.

#### Task 1.12 — Error Recovery

The lexer should not stop at the first error. Record the error, emit `TOK_ERROR`, and continue scanning from the next character. This allows multiple errors to be reported in one run.

### 4.5 Exit Criteria

All tests in `tests/unit/test_lexer.c` and `tests/forge/01_lexer/` pass.

Minimum test coverage:
- [ ] All single-character tokens
- [ ] All multi-character operators
- [ ] Integer literals in all 4 bases with separators
- [ ] Float literals with and without exponents
- [ ] String literals with all escape sequences
- [ ] Raw string literals
- [ ] All 40+ keywords correctly distinguished from identifiers
- [ ] Correct INDENT/DEDENT for simple blocks
- [ ] Correct INDENT/DEDENT for multiply-nested blocks
- [ ] Correct multiple DEDENTs on a single line
- [ ] Correct handling of blank and comment-only lines within blocks
- [ ] Line continuation with `\`
- [ ] Implicit continuation inside `()`, `[]`, `{}`
- [ ] Tab character produces error
- [ ] Inconsistent dedent produces error
- [ ] Unterminated string produces error

---

## 5. Phase 2 — Parser and AST

### 5.1 Goal

Consume the token stream from Phase 1 and produce an in-memory Abstract Syntax Tree representing the complete structure of a FORGE source file. The parser does **not** type-check. It only verifies syntactic correctness.

### 5.2 Files

```
src/parser/ast.h
src/parser/ast.c
src/parser/parser.h
src/parser/parser.c
tests/unit/test_parser.c
tests/forge/02_parser/
```

### 5.3 AST Node Design

The AST uses a tagged union. Every node is a `forge_node_t*` (heap-allocated via the arena allocator).

```c
/* ast.h — excerpt showing key node types */

typedef enum {
    /* Top-level */
    NODE_PROGRAM,
    /* Declarations */
    NODE_IMPORT, NODE_PROC_DECL, NODE_RECORD_DECL,
    NODE_CHANNEL_DECL, NODE_ON_HANDLER,
    NODE_VAR_DECL, NODE_CONST_DECL, NODE_TYPE_ALIAS,
    /* Statements */
    NODE_BLOCK, NODE_ASSIGN,
    NODE_IF, NODE_WHILE, NODE_FOR, NODE_LOOP,
    NODE_RETURN, NODE_BREAK, NODE_CONTINUE,
    NODE_EMIT, NODE_WITH_ALLOC,
    NODE_PANIC, NODE_ASSERT, NODE_EXPR_STMT,
    /* Expressions */
    NODE_INT_LIT, NODE_FLOAT_LIT, NODE_STR_LIT,
    NODE_BOOL_LIT, NODE_NONE_LIT,
    NODE_IDENT, NODE_QUALIFIED_IDENT,
    NODE_BINARY_OP, NODE_UNARY_OP,
    NODE_CALL, NODE_FIELD_ACCESS, NODE_INDEX,
    NODE_RECORD_LITERAL, NODE_ARRAY_LITERAL,
    NODE_CAST, NODE_SOME, NODE_OR_ELSE,
    NODE_IS_CHECK, NODE_RANGE,
    /* Types (as AST nodes for type expressions) */
    NODE_TYPE_PRIM, NODE_TYPE_OPTIONAL,
    NODE_TYPE_FIXED_ARRAY, NODE_TYPE_DYN_ARRAY,
    NODE_TYPE_MAP, NODE_TYPE_NAMED,
} forge_node_kind_t;
```

Full union definition: refer to Spec Section 17.3. Reproduce it verbatim in `ast.h`.

#### Arena Allocator

All AST nodes are allocated from an arena. This avoids hundreds of small `malloc` calls and makes cleanup trivial (free the arena, everything goes).

```c
/* memory.h */
typedef struct forge_arena forge_arena_t;

forge_arena_t* arena_create(size_t initial_capacity);
void*          arena_alloc(forge_arena_t* a, size_t size);
void           arena_destroy(forge_arena_t* a);

/* Convenience macro */
#define ARENA_ALLOC(arena, type)  ((type*)arena_alloc((arena), sizeof(type)))
```

Implementation: singly-linked list of fixed-size blocks (e.g. 64KB each). Bump-pointer allocation within each block. Zero overhead for individual nodes.

### 5.4 Parser Design

Recursive descent. One parsing function per grammar rule. The parser owns a cursor into the token array.

```c
/* parser.h */

typedef struct {
    forge_token_t*   tokens;
    int              count;
    int              pos;           /* current token cursor */
    forge_arena_t*   arena;
    forge_strtable_t* strtable;
    const char*      filename;
    int              had_error;
} forge_parser_t;

forge_parser_t* parser_create(forge_token_t* tokens, int count,
                               forge_arena_t* arena,
                               forge_strtable_t* strtable,
                               const char* filename);
forge_node_t*   parser_parse(forge_parser_t* p);    /* returns NODE_PROGRAM */
void            parser_destroy(forge_parser_t* p);
void            ast_print(forge_node_t* node, int depth);  /* debug */
```

**Parser helper macros:**

```c
#define CURRENT(p)    ((p)->tokens[(p)->pos])
#define PEEK(p, n)    ((p)->tokens[(p)->pos + (n)])
#define ADVANCE(p)    ((p)->tokens[(p)->pos++])
#define CHECK(p, t)   (CURRENT(p).type == (t))
#define MATCH(p, t)   (CHECK(p, t) ? (ADVANCE(p), 1) : 0)
#define EXPECT(p, t)  (CHECK(p, t) ? ADVANCE(p) : parser_error(p, "expected " #t))
```

### 5.5 Implementation Tasks

#### Task 2.1 — Arena Allocator

Implement `memory.h/c`. Test it in isolation: allocate 10,000 nodes, verify pointers are valid, verify arena_destroy releases all memory cleanly (use valgrind).

#### Task 2.2 — AST Node Constructors

Write a constructor function for each node kind. These are thin wrappers over `arena_alloc` that set the kind and zero the union.

```c
forge_node_t* ast_int_lit(forge_arena_t* a, long long val, int line, int col);
forge_node_t* ast_binary_op(forge_arena_t* a, int op,
                             forge_node_t* left, forge_node_t* right,
                             int line, int col);
/* ... one per node kind ... */
```

#### Task 2.3 — Parser: Top-Level Structure

Parse the program as a sequence of import declarations followed by top-level declarations. A top-level declaration may be optionally prefixed by `export`.

```c
forge_node_t* parse_program(forge_parser_t* p);
forge_node_t* parse_import(forge_parser_t* p);
forge_node_t* parse_top_level_decl(forge_parser_t* p, int exported);
```

#### Task 2.4 — Parser: Procedure Declarations

```c
forge_node_t* parse_proc_decl(forge_parser_t* p, int exported);
forge_node_t* parse_param_list(forge_parser_t* p);
forge_node_t* parse_type(forge_parser_t* p);
forge_node_t* parse_block(forge_parser_t* p);
```

Parse parameter list, return type annotation, and body block. Collect params into a small array (stack-allocated, then copied to arena).

#### Task 2.5 — Parser: Record and Channel Declarations

```c
forge_node_t* parse_record_decl(forge_parser_t* p, int exported);
forge_node_t* parse_channel_decl(forge_parser_t* p, int exported);
forge_node_t* parse_on_handler(forge_parser_t* p);
```

Record: parse field list inside INDENT/DEDENT. Channel: parse name and payload type on one line. On-handler: parse channel reference, optional `as` variable, and block.

#### Task 2.6 — Parser: Statements

Implement each statement parser. The key is the dispatch: look at the current token and decide which statement type follows.

```c
forge_node_t* parse_stmt(forge_parser_t* p);
forge_node_t* parse_var_decl(forge_parser_t* p);
forge_node_t* parse_assign_or_call(forge_parser_t* p);  /* ambiguous — resolve by lookahead */
forge_node_t* parse_if(forge_parser_t* p);
forge_node_t* parse_while(forge_parser_t* p);
forge_node_t* parse_for(forge_parser_t* p);
forge_node_t* parse_loop(forge_parser_t* p);
forge_node_t* parse_return(forge_parser_t* p);
forge_node_t* parse_emit(forge_parser_t* p);
forge_node_t* parse_with(forge_parser_t* p);
forge_node_t* parse_panic_assert(forge_parser_t* p);
```

**Ambiguity: assignment vs expression statement.** Both start with an identifier. Resolve by scanning forward: if the token after the expression is an assignment operator (`=`, `+=`, etc.), it's an assignment. Otherwise it's an expression statement (must be a call).

#### Task 2.7 — Parser: Expressions

Implement the expression parser as a Pratt parser or a set of precedence-climbing functions. The precedence table is in Spec Appendix B.

```c
forge_node_t* parse_expr(forge_parser_t* p);
forge_node_t* parse_or_expr(forge_parser_t* p);
forge_node_t* parse_and_expr(forge_parser_t* p);
forge_node_t* parse_not_expr(forge_parser_t* p);
forge_node_t* parse_cmp_expr(forge_parser_t* p);
forge_node_t* parse_bitor_expr(forge_parser_t* p);
forge_node_t* parse_bitxor_expr(forge_parser_t* p);
forge_node_t* parse_bitand_expr(forge_parser_t* p);
forge_node_t* parse_shift_expr(forge_parser_t* p);
forge_node_t* parse_add_expr(forge_parser_t* p);
forge_node_t* parse_mul_expr(forge_parser_t* p);
forge_node_t* parse_unary_expr(forge_parser_t* p);
forge_node_t* parse_postfix_expr(forge_parser_t* p);
forge_node_t* parse_primary(forge_parser_t* p);
```

#### Task 2.8 — Parser: Type Expressions

```c
forge_node_t* parse_type(forge_parser_t* p);
```

Handles primitive types, `?T`, `[T; N]`, `[]T`, `map[K, V]`, and named types (identifiers). Called from variable declarations, procedure parameter and return types, channel declarations, and cast expressions.

#### Task 2.9 — AST Pretty-Printer

Implement `ast_print` — walks the AST and prints a human-readable indented tree. This is your primary debugging tool for Phase 2.

```
PROGRAM
  PROC_DECL "main" -> void
    BLOCK
      VAR_DECL "x" : int
        INT_LIT 42
      IF
        BINARY_OP >
          IDENT "x"
          INT_LIT 0
        BLOCK
          EXPR_STMT
            CALL "print"
              STR_LIT "positive"
```

#### Task 2.10 — Error Recovery in the Parser

Implement synchronization: when a parse error occurs, consume tokens until a synchronization point is reached (a `NEWLINE` or a keyword that starts a new top-level declaration). Report the error but continue parsing to find more errors.

### 5.6 Exit Criteria

- [ ] `ast_print` produces correct trees for all test programs in `tests/forge/02_parser/`
- [ ] All valid grammar constructs parse without error
- [ ] Invalid constructs produce clear error messages with line and column
- [ ] Parser correctly handles multiple errors in one file
- [ ] AST nodes correctly record source positions

---

## 6. Phase 3 — Tree-Walking Interpreter

### 6.1 Goal

Execute FORGE programs by walking the AST directly. This produces `forge run`. No compilation, no intermediate representation. Developer experience is the priority: clear error messages, full stack traces, runtime type information.

### 6.2 Files

```
src/interp/value.h / value.c        # Runtime value type
src/interp/env.h / env.c            # Scope/environment stack
src/interp/channel.h / channel.c    # Channel registry
src/interp/interp.h / interp.c      # Main evaluator
src/util/hashmap.h / hashmap.c      # Generic hash map (used by env and channel)
tests/unit/test_interp.c
tests/forge/03_interp/
```

### 6.3 Value Representation

```c
/* value.h */

typedef enum {
    VAL_INT, VAL_UINT, VAL_FLOAT, VAL_FLOAT32,
    VAL_BOOL, VAL_STR, VAL_BYTE,
    VAL_RECORD,
    VAL_ARRAY_FIXED, VAL_ARRAY_DYN,
    VAL_MAP,
    VAL_OPTIONAL,
    VAL_VOID,
    VAL_NONE
} forge_val_kind_t;

typedef struct forge_value forge_value_t;

struct forge_value {
    forge_val_kind_t kind;
    union {
        long long           i;          /* VAL_INT, VAL_BYTE */
        unsigned long long  u;          /* VAL_UINT */
        double              f;          /* VAL_FLOAT */
        float               f32;        /* VAL_FLOAT32 */
        int                 b;          /* VAL_BOOL */
        struct {
            char*   data;               /* heap-allocated, null-terminated */
            int     len;                /* byte length excluding null */
            int     owned;             /* 1 = must free, 0 = interned/literal */
        } str;
        struct {
            forge_value_t*  fields;     /* heap array */
            int             count;
            const char**    names;      /* field name pointers (interned) */
        } record;
        struct {
            forge_value_t*  elems;      /* heap or stack array */
            int             len;
            int             cap;
            int             heap;       /* 1 = heap-allocated, must free */
        } array;
        void*               map_ptr;    /* opaque handle to forge_map_t */
        struct {
            int             present;    /* 0 = none, 1 = some */
            forge_value_t*  inner;      /* heap-allocated if present */
        } optional;
    };
};

/* Value operations */
forge_value_t val_int(long long i);
forge_value_t val_float(double f);
forge_value_t val_bool(int b);
forge_value_t val_str_lit(const char* s);       /* not owned */
forge_value_t val_str_heap(char* s, int len);   /* owned */
forge_value_t val_void(void);
forge_value_t val_none(void);
forge_value_t val_some(forge_value_t inner);

void          val_free(forge_value_t* v);        /* free heap resources */
forge_value_t val_copy(forge_value_t v);         /* deep copy */
int           val_equal(forge_value_t a, forge_value_t b);
char*         val_to_str(forge_value_t v);       /* allocates; caller frees */
```

### 6.4 Environment Stack

```c
/* env.h */

#define ENV_MAX_BINDINGS 256

typedef struct forge_env forge_env_t;

struct forge_env {
    const char*     names[ENV_MAX_BINDINGS];
    forge_value_t   values[ENV_MAX_BINDINGS];
    int             count;
    forge_env_t*    parent;
};

forge_env_t*  env_create(forge_env_t* parent);
void          env_destroy(forge_env_t* env);
void          env_set(forge_env_t* env, const char* name, forge_value_t val);
forge_value_t env_get(forge_env_t* env, const char* name);   /* panics if not found */
int           env_get_opt(forge_env_t* env, const char* name, forge_value_t* out);
void          env_update(forge_env_t* env, const char* name, forge_value_t val);
    /* env_update searches up the chain; env_set only writes to current frame */
```

### 6.5 Channel Registry

```c
/* channel.h */

#define CHANNEL_MAX_HANDLERS 64

typedef struct {
    const char*         name;           /* interned channel name */
    forge_node_t*       handlers[CHANNEL_MAX_HANDLERS];
    forge_env_t*        handler_envs[CHANNEL_MAX_HANDLERS]; /* closure env for each */
    int                 handler_count;
} forge_channel_t;

typedef struct {
    forge_channel_t*    channels;       /* dynamic array */
    int                 count;
    int                 cap;
} forge_channel_registry_t;

forge_channel_registry_t* channel_registry_create(void);
void   channel_registry_destroy(forge_channel_registry_t* reg);
void   channel_register(forge_channel_registry_t* reg,
                        const char* name,
                        forge_node_t* handler_node,
                        forge_env_t* env);
void   channel_emit(forge_channel_registry_t* reg,
                    struct forge_interp* interp,
                    const char* name,
                    forge_value_t* payload);     /* NULL for void channels */
```

### 6.6 Interpreter Structure

```c
/* interp.h */

typedef enum {
    FLOW_NORMAL,
    FLOW_RETURN,
    FLOW_BREAK,
    FLOW_CONTINUE,
} forge_flow_t;

typedef struct {
    forge_flow_t    flow;
    forge_value_t   return_val;
} forge_result_t;

typedef struct forge_interp {
    forge_arena_t*              arena;
    forge_strtable_t*           strtable;
    forge_channel_registry_t*   channels;
    /* Module table: module name -> NODE_PROGRAM */
    forge_hashmap_t*            modules;
    /* Call stack for error reporting */
    struct {
        const char*     proc_name;
        const char*     filename;
        int             line;
    }                           call_stack[256];
    int                         call_depth;
    /* Error state */
    int                         had_error;
    char                        error_msg[1024];
} forge_interp_t;

forge_interp_t* interp_create(forge_arena_t* arena, forge_strtable_t* strtable);
void            interp_load_module(forge_interp_t* interp,
                                   const char* name,
                                   forge_node_t* program);
void            interp_run(forge_interp_t* interp);
void            interp_destroy(forge_interp_t* interp);
```

### 6.7 Implementation Tasks

#### Task 3.1 — Utility: Generic Hash Map

Implement `hashmap.h/c` — an open-addressing hash map with string keys and `void*` values. FNV-1a hash, linear probing, 0.75 load factor, doubles on resize. This is used by the environment, module table, and channel registry.

#### Task 3.2 — Value Layer

Implement `value.h/c` fully. Test every operation:
- Arithmetic on int/float (including mixed — should fail type check, but interp can be lenient in v1)
- String concatenation and conversion
- Record construction and field access
- Optional wrapping/unwrapping
- `val_copy` produces truly independent copies
- `val_free` does not double-free

#### Task 3.3 — Environment Layer

Implement `env.h/c`. Test scope creation, lookup chain, shadowing, and destruction. Ensure `env_update` correctly walks up the chain to modify an outer-scope variable.

#### Task 3.4 — Interpreter: Expressions

Implement `interp_eval_expr(interp, env, node) -> forge_value_t`. Handle each expression node kind. Start with literals and work outward.

```
Literals         → return val_int / val_float / val_str / val_bool / val_none
IDENT            → env_get(env, name)
QUALIFIED_IDENT  → look up module, then exported name in that module's env
BINARY_OP        → eval left, eval right, apply operator
UNARY_OP         → eval operand, apply operator
CALL             → eval args, push call frame, execute proc body, pop frame
FIELD_ACCESS     → eval record expr, return named field
INDEX            → eval array/map, eval index, return element
RECORD_LITERAL   → eval each field expression, construct VAL_RECORD
ARRAY_LITERAL    → eval each element, construct VAL_ARRAY_DYN
CAST             → eval inner, convert type
SOME             → eval inner, wrap in optional
OR_ELSE          → eval optional; if some return .inner, else eval default
IS_CHECK         → eval optional; return bool
```

#### Task 3.5 — Interpreter: Statements

Implement `interp_exec_stmt(interp, env, node) -> forge_result_t`. Return a `forge_result_t` carrying the flow type (NORMAL, RETURN, BREAK, CONTINUE) so that loops and procedures can handle early exits correctly.

Key statements:
- `VAR_DECL`: eval init expr (or zero-value), bind in current env
- `ASSIGN`: eval RHS, update binding via `env_update`
- `IF`: eval condition (must be VAL_BOOL), execute appropriate branch
- `WHILE`: loop while condition true; break on FLOW_BREAK result
- `FOR`: range variant vs collection variant
- `LOOP`: loop until FLOW_BREAK
- `RETURN`: set FLOW_RETURN + return_val, propagate up
- `EMIT`: look up channel in registry, call `channel_emit`

#### Task 3.6 — Interpreter: Procedures

When calling a procedure:
1. Create a new environment with the module's top-level env as parent
2. Bind each parameter name to its argument value
3. Execute the body block
4. Return the return value (or VAL_VOID)
5. Pop the call stack frame

Handle `ref` parameters: bind a pointer to the caller's value; on procedure return, write back the (possibly modified) value. In the interpreter, simulate this by updating the caller's environment binding after the call.

#### Task 3.7 — Interpreter: Module Loading

A module is loaded by:
1. Parsing its source file into an AST
2. Executing all module-level `var` declarations (init time)
3. Registering all `on` handlers in the channel registry
4. Calling `init()` if defined

Build a dependency resolver: parse import declarations, load dependencies first (detect cycles — error on circular imports).

#### Task 3.8 — Interpreter: Channel Dispatch

Implement `channel_emit`:

```c
void channel_emit(forge_channel_registry_t* reg,
                  forge_interp_t* interp,
                  const char* name,
                  forge_value_t* payload) {
    forge_channel_t* ch = channel_find(reg, name);
    if (!ch) {
        /* No handlers registered — silently do nothing */
        return;
    }
    for (int i = 0; i < ch->handler_count; i++) {
        forge_node_t* handler = ch->handlers[i];
        forge_env_t* handler_env = env_create(ch->handler_envs[i]);
        if (payload && handler->on_handler.as_name) {
            env_set(handler_env, handler->on_handler.as_name, val_copy(*payload));
        }
        interp_exec_block(interp, handler_env, handler->on_handler.body);
        env_destroy(handler_env);
    }
}
```

#### Task 3.9 — Runtime Error Reporting

All runtime errors must report: error message, file name, line number, and a call stack trace. Implement `interp_runtime_error` which formats and prints this information and sets `had_error`.

```
Runtime error: division by zero
  in proc divide() at sensors.fg:47
  in proc main() at main.fg:12
```

#### Task 3.10 — Builtin Functions

The interpreter needs a small set of builtin procedures (not in FORGE source):

```
print(s: str)           print to stdout
str(val)                convert to string
len(arr)                array/string length
append(arr, val)        dynamic array append
alloc(type, count)      heap allocation
free(val)               heap release
has_key(map, key)       map key check
delete_key(map, key)    map key removal
get(map, key)           safe map access -> ?T
map_keys(map)           returns []K
some(val)               wrap optional
assert(cond, msg)       runtime assert
panic(msg)              abort with message
range(start, stop)      (handled by for loop, not a real call)
```

Implement these as a lookup table checked before the normal procedure lookup.

#### Task 3.11 — Minimal `forge run` CLI

Implement a minimal command-line interface to satisfy the Phase 3 exit criteria. This is **not** the full CLI from Phase 7 — it is the minimum viable `forge run` that connects the lexer, parser, and interpreter.

**File:** `src/cli/main.c`

**Pipeline:**
```
read file → lex → parse → interpret
```

**Usage:**
```
forge run <file.fg>
```

**Implementation:**
1. Read source file into memory
2. Create lexer, tokenize source
3. Create parser, parse tokens to AST
4. Create interpreter, run AST
5. Print any errors with file:line:col format
6. Exit 0 on success, 1 on error

**Notes:**
- This is intentionally simple — no `--help`, no other subcommands
- Error formatting uses existing `interp_error` infrastructure
- The full CLI polish (multiple subcommands, nice help text, etc.) is Phase 7

### 6.8 Exit Criteria

The following test programs must run correctly under `forge run`:

- [ ] Hello world
- [ ] Fibonacci (recursive and iterative)
- [ ] Bubble sort on a fixed array
- [ ] Factorial with recursion depth ≥ 20
- [ ] Record creation, field access, mutation via `ref`
- [ ] Dynamic array: append, iterate, free
- [ ] Map: insert, lookup, has_key, delete, iterate
- [ ] Optional: some/none, is checks, or_else
- [ ] Channel: emit on same module, emit across modules
- [ ] Multi-module program: 3+ modules with import chain
- [ ] Module init order is correct
- [ ] Runtime error produces correct file/line/stack trace
- [ ] Division by zero caught at runtime
- [ ] Array out-of-bounds caught at runtime
---

## 7. Phase 4 — Type Checker

### 7.1 Goal

Add a type checking pass that walks the AST before execution or code emission. Catch type errors at analysis time rather than runtime. Annotate every expression node with its resolved type so the C emitter can use that information without re-deriving it.

### 7.2 Files

```
src/typecheck/types.h / types.c     # Type representation
src/typecheck/checker.h / checker.c # Type checker pass
tests/unit/test_typecheck.c
tests/forge/04_typecheck/
```

### 7.3 Type Representation

```c
/* types.h */

typedef enum {
    TY_INT, TY_INT8, TY_INT16, TY_INT32,
    TY_UINT, TY_UINT8, TY_UINT16, TY_UINT32,
    TY_FLOAT, TY_FLOAT32,
    TY_BOOL, TY_STR, TY_BYTE, TY_VOID,
    TY_OPTIONAL,
    TY_FIXED_ARRAY,
    TY_DYN_ARRAY,
    TY_MAP,
    TY_RECORD,
    TY_ALIAS,
    TY_NONE,        /* the type of 'none' literal — compatible with any ?T */
    TY_UNRESOLVED,  /* placeholder before resolution */
} forge_type_kind_t;

typedef struct forge_type forge_type_t;

struct forge_type {
    forge_type_kind_t   kind;
    union {
        struct {
            forge_type_t*   inner;          /* TY_OPTIONAL: inner type */
        } optional;
        struct {
            forge_type_t*   elem_type;
            int             size;           /* TY_FIXED_ARRAY */
        } fixed_array;
        struct {
            forge_type_t*   elem_type;      /* TY_DYN_ARRAY */
        } dyn_array;
        struct {
            forge_type_t*   key_type;
            forge_type_t*   val_type;       /* TY_MAP */
        } map;
        struct {
            const char*     name;           /* interned record type name */
            const char**    field_names;
            forge_type_t**  field_types;
            int             field_count;
        } record;
        struct {
            const char*     name;
            forge_type_t*   target;         /* TY_ALIAS */
        } alias;
    };
};

/* Type construction */
forge_type_t* type_prim(forge_arena_t* a, forge_type_kind_t kind);
forge_type_t* type_optional(forge_arena_t* a, forge_type_t* inner);
forge_type_t* type_fixed_array(forge_arena_t* a, forge_type_t* elem, int size);
forge_type_t* type_dyn_array(forge_arena_t* a, forge_type_t* elem);
forge_type_t* type_map(forge_arena_t* a, forge_type_t* key, forge_type_t* val);
forge_type_t* type_record(forge_arena_t* a, const char* name,
                           const char** fnames, forge_type_t** ftypes, int n);

/* Type predicates */
int  type_equal(forge_type_t* a, forge_type_t* b);
int  type_is_integer(forge_type_t* t);
int  type_is_numeric(forge_type_t* t);
int  type_is_assignable(forge_type_t* target, forge_type_t* source);
    /* assignable: same type, OR source is TY_NONE and target is TY_OPTIONAL */

/* Type names for error messages */
const char* type_name(forge_type_t* t);     /* allocates; caller frees */
```

### 7.4 Type Checker Architecture

The checker is a two-pass process:

**Pass 1 — Declaration collection:** Scan all top-level declarations (proc, record, channel, const, type alias) without checking their bodies. Build a symbol table for the module. This allows forward references within a module.

**Pass 2 — Body checking:** Check all procedure bodies, on-handler bodies, and variable initializers using the symbol table built in Pass 1.

```c
/* checker.h */

typedef struct {
    /* Symbol tables: name -> forge_type_t* */
    forge_hashmap_t*    types;          /* record and alias declarations */
    forge_hashmap_t*    procs;          /* procedure signatures */
    forge_hashmap_t*    channels;       /* channel payload types */
    forge_hashmap_t*    consts;         /* constant types */
    /* Module imports */
    forge_hashmap_t*    imports;        /* alias -> module checker */
    /* Current proc return type (for return statement checking) */
    forge_type_t*       current_return_type;
    /* Arena for type allocation */
    forge_arena_t*      arena;
    forge_strtable_t*   strtable;
    const char*         filename;
    int                 had_error;
    int                 error_count;
} forge_checker_t;

forge_checker_t* checker_create(forge_arena_t* arena, forge_strtable_t* strtable,
                                 const char* filename);
int  checker_check(forge_checker_t* checker, forge_node_t* program);
void checker_destroy(forge_checker_t* checker);
```

### 7.5 Implementation Tasks

#### Task 4.1 — Type Construction and Equality

Implement all type constructors and `type_equal`. Test that structural equality works for nested types (e.g., `?[int; 10]` == `?[int; 10]` but not `?[int; 10]` == `?[int; 5]`).

#### Task 4.2 — Symbol Table Population (Pass 1)

Walk the top-level declarations. For each:
- `record`: build a `forge_type_t` with field names and types; insert into `checker->types`
- `proc`: build a signature descriptor (param types + return type); insert into `checker->procs`
- `channel`: store payload type in `checker->channels`
- `const`: evaluate the constant expression type; insert into `checker->consts`
- `type`: resolve the alias target; insert into `checker->types`

Report duplicate declarations.

#### Task 4.3 — Expression Type Inference

Implement `checker_type_of(checker, env, node) -> forge_type_t*`. This is the core of the type checker. It returns the type of an expression node and annotates `node->resolved_type`.

Key rules:
- Literals: trivial
- `IDENT`: look up in local env, then module symbols
- `BINARY_OP`: check operand types are compatible; determine result type
- `CALL`: check argument count and types match proc signature; return proc return type
- `FIELD_ACCESS`: check left is a record type; return field type
- `INDEX`: check is array or map; return element type
- `CAST`: return the target type (validity of the cast is checked separately)
- `SOME(x)`: return `?T` where `T` is the type of `x`
- `OR_ELSE`: check left is `?T`, right is `T`; return `T`
- `IS_CHECK`: return `bool`

#### Task 4.4 — Statement Type Checking

Walk statements. Key checks:
- `VAR_DECL`: declared type must match initializer type (if present)
- `ASSIGN`: target type must match RHS type; target must be a mutable lvalue
- `IF/WHILE`: condition must be `bool`
- `RETURN`: return type must match current procedure's declared return type
- `EMIT`: payload type must match channel's declared payload type
- `FOR` over range: loop variable inferred as `int`
- `FOR` over collection: loop variable type inferred from element type

#### Task 4.5 — Procedure Body Checking

For each `PROC_DECL`:
1. Create a local env with parameter bindings
2. Set `current_return_type`
3. Check the body block
4. Verify all code paths return a value (for non-void procs)

The return-path analysis is a simple recursive walk: a block "always returns" if it contains a `RETURN` statement, or an `IF` where all branches always return.

#### Task 4.6 — Import Resolution

When checking a module that has imports, the checker needs access to the symbol tables of imported modules. Implement a module checker registry: a map of module name → `forge_checker_t*`. Load and check imported modules before the importing module.

#### Task 4.7 — Warning System

Implement checker warnings (non-fatal) for:
- Unused variables (scan for uses after declaration)
- Unused imports
- Shadowed variables
- Missing return on non-void proc (may be error in `--strict` mode)
- Ignoring return value of a proc that returns non-void

### 7.6 Exit Criteria

All programs in `tests/forge/04_typecheck/` must produce the correct outcome:

- [ ] Valid programs: checker passes with zero errors
- [ ] Type mismatch: checker reports error with line/column
- [ ] Wrong arg count: caught
- [ ] Wrong arg types: caught
- [ ] Return type mismatch: caught
- [ ] Emit payload type mismatch: caught
- [ ] Undefined variable: caught
- [ ] Undefined procedure: caught
- [ ] Unknown record field: caught
- [ ] Circular import: caught
- [ ] Unused import: warning produced
- [ ] All previously passing interpreter tests still pass after type checker is inserted in the pipeline

---

## 8. Phase 5a — C Emitter

### 8.1 Goal

Produce a compilable C source file from the type-annotated AST. The emitted C, when compiled with gcc or clang and linked against `forge_runtime.c`, must produce a binary that behaves identically to `forge run` on all valid FORGE programs.

### 8.2 Files

```
src/emit_c/emit_c.h
src/emit_c/emit_c.c
runtime/forge_runtime.h
runtime/forge_runtime.c
tests/forge/05_emit_c/
```

### 8.3 Runtime Library Design

`forge_runtime.h/c` provides the C-level types and functions that emitted code uses. It must be compiled once and linked with every FORGE program.

**Key runtime types:**

```c
/* forge_runtime.h */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ── Dynamic array ────────────────────────────────── */
typedef struct {
    void*   data;
    int     len;
    int     cap;
    size_t  elem_size;
} forge_array_t;

forge_array_t forge_array_create(size_t elem_size, int initial_cap);
void          forge_array_push(forge_array_t* a, void* elem);
void*         forge_array_get(forge_array_t* a, int index);
void          forge_array_free(forge_array_t* a);

/* ── String ───────────────────────────────────────── */
typedef struct {
    char*   data;
    int     len;
    int     owned;
} forge_str_t;

forge_str_t  forge_str_lit(const char* s);      /* wraps literal — not owned */
forge_str_t  forge_str_concat(forge_str_t a, forge_str_t b);
void         forge_str_free(forge_str_t* s);
forge_str_t  forge_str_from_int(int64_t i);
forge_str_t  forge_str_from_float(double f);
forge_str_t  forge_str_from_bool(int b);

/* ── Optional ─────────────────────────────────────── */
/* Implemented per-type via macro to preserve type safety */
#define FORGE_OPTIONAL(T)  struct { int present; T value; }

/* ── Map ──────────────────────────────────────────── */
typedef struct forge_map forge_map_t;
forge_map_t* forge_map_create(size_t key_size, size_t val_size,
                               int (*key_equal)(void*, void*),
                               uint32_t (*key_hash)(void*));
void         forge_map_set(forge_map_t* m, void* key, void* val);
int          forge_map_get(forge_map_t* m, void* key, void* val_out);
int          forge_map_has(forge_map_t* m, void* key);
void         forge_map_delete(forge_map_t* m, void* key);
void         forge_map_free(forge_map_t* m);

/* ── Error / panic ────────────────────────────────── */
void forge_panic(const char* msg, const char* file, int line);
void forge_assert(int cond, const char* msg, const char* file, int line);

#define FORGE_PANIC(msg)        forge_panic(msg, __FILE__, __LINE__)
#define FORGE_ASSERT(c, msg)    forge_assert(c, msg, __FILE__, __LINE__)

/* ── I/O builtins ─────────────────────────────────── */
void forge_print(forge_str_t s);
void forge_eprint(forge_str_t s);
forge_str_t forge_read_line(void);
```

### 8.4 Emitter Design

The C emitter does a single linear pass over the annotated AST and writes to a `FILE*` (the output `.c` file). Maintain an indentation counter for readable C output.

```c
/* emit_c.h */

typedef struct {
    FILE*               out;
    int                 indent;
    forge_arena_t*      arena;
    forge_strtable_t*   strtable;
    int                 tmp_counter;    /* for unique temp variable names */
    int                 label_counter;  /* for unique label names */
    const char*         module_name;
} forge_emitter_t;

forge_emitter_t* emitter_create(FILE* out, forge_arena_t* arena,
                                 forge_strtable_t* strtable,
                                 const char* module_name);
void  emitter_emit_program(forge_emitter_t* e, forge_node_t* program);
void  emitter_destroy(forge_emitter_t* e);
```

**Helper macros:**

```c
#define EMIT(e, ...)        fprintf((e)->out, __VA_ARGS__)
#define EMITLN(e, ...)      (fprintf((e)->out, __VA_ARGS__), fputc('\n', (e)->out))
#define INDENT(e)           emit_indent(e)
#define PUSH_INDENT(e)      ((e)->indent += 4)
#define POP_INDENT(e)       ((e)->indent -= 4)
```

### 8.5 Implementation Tasks

#### Task 5a.1 — Runtime Library

Implement `forge_runtime.c` completely. This is pure C with no FORGE toolchain dependencies. Test it directly as a C library before connecting to the emitter.

Priority order:
1. `forge_str_t` operations (most heavily used)
2. `forge_array_t` (dynamic arrays)
3. `forge_panic` / `forge_assert`
4. `forge_print` / `forge_read_line`
5. `forge_map_t`

#### Task 5a.2 — Emit: Header and Includes

The generated `.c` file begins with:

```c
/* Generated by FORGE compiler v0.1 */
/* Source: main.fg */
#include "forge_runtime.h"
#include "sensors.h"     /* one include per imported module */
/* ... other imports ... */
```

Each module is also emitted with a corresponding `.h` file declaring exported symbols.

#### Task 5a.3 — Emit: Record Types

```forge
record Sensor:
    id:    int
    value: float
```

Emits:

```c
typedef struct {
    int64_t id;
    double  value;
} Sensor_t;
```

Naming convention: `RecordName_t`. Avoid collisions with FORGE's own runtime types by using this suffix consistently.

#### Task 5a.4 — Emit: Procedure Declarations

```forge
proc add(a: int, b: int) -> int:
    return a + b
```

Emits:

```c
static int64_t forge_add(int64_t a, int64_t b) {
    return a + b;
}
```

Naming convention: `forge_` prefix on all emitted procedures (avoids collision with C stdlib and runtime symbols). Exported procedures omit `static`.

`ref` parameters emit as pointers:

```c
static void forge_swap(int64_t* a, int64_t* b) {
    int64_t tmp = *a;
    *a = *b;
    *b = tmp;
}
```

#### Task 5a.5 — Emit: Expressions

Map FORGE expression nodes to C expressions. Most are straightforward:

| FORGE | C |
|-------|---|
| `a + b` | `(a + b)` |
| `a and b` | `(a && b)` |
| `not a` | `(!a)` |
| `str(x)` | `forge_str_from_int(x)` |
| `a + b` (str) | `forge_str_concat(a, b)` |
| `arr[i]` | `*(ElemType*)forge_array_get(&arr, i)` |
| `rec.field` | `rec.field` |
| `some(x)` | `(OptType){ .present=1, .value=(x) }` |
| `x or_else d` | `(x.present ? x.value : (d))` |
| `x is some` | `(x.present != 0)` |

#### Task 5a.6 — Emit: Control Flow

`if/elif/else` → C `if/else if/else`  
`while` → C `while`  
`for` over range → C `for` loop with int counter  
`for` over array → C `for` with index counter, array_get  
`loop` → `while(1)` with `break` on FLOW_BREAK  
`break/continue` → direct C `break`/`continue`  
`return` → C `return`  

#### Task 5a.7 — Emit: Channel System

Channels require the most careful emitting. For each module, the emitter generates:

1. An array of function pointers for each channel's handlers
2. A registration function `module_register_handlers()` that populates the arrays
3. A dispatch function `module_emit_channelname(payload)` that calls each registered handler

```c
/* Generated channel infrastructure for channel: depth_reading */
typedef void (*depth_reading_handler_t)(double payload);
static depth_reading_handler_t depth_reading_handlers[64];
static int depth_reading_handler_count = 0;

void sensors_register_depth_reading(depth_reading_handler_t h) {
    depth_reading_handlers[depth_reading_handler_count++] = h;
}

void sensors_emit_depth_reading(double payload) {
    for (int i = 0; i < depth_reading_handler_count; i++) {
        depth_reading_handlers[i](payload);
    }
}
```

#### Task 5a.8 — Emit: Module Init and Main

Each module gets a `module_init()` function. The generated `main()` calls all module inits in dependency order, then calls `forge_main()` (the user's `main`).

```c
/* Generated main.c entry point */
int main(int argc, char** argv) {
    forge_runtime_init(argc, argv);
    sensors_module_init();
    display_module_init();
    main_module_init();
    forge_main();
    return 0;
}
```

#### Task 5a.9 — Parity Testing

For every test program in `tests/forge/03_interp/` (interpreter tests):
1. Run `forge run program.fg` and capture output
2. Run `forge build program.fg && ./program` and capture output
3. Assert outputs are identical

This is automated in `tests/runner.sh`.

### 8.6 Exit Criteria

- [ ] All interpreter test programs produce identical output when compiled via C
- [ ] Generated C compiles without warnings under `gcc -Wall -Wextra`
- [ ] Generated C compiles under `clang` as well as `gcc`
- [ ] Multi-module programs compile and link correctly
- [ ] Channel dispatch works identically in compiled mode
- [ ] `ref` parameters correctly modify caller values

---

## 9. Phase 5b — LLVM IR Emitter

### 9.1 Goal

Provide an alternative compilation backend that emits LLVM IR (`.ll` files) instead of C. This enables LLVM's optimization passes and opens the door to cross-compilation targets beyond what `gcc` covers easily (e.g., WebAssembly, RISC-V).

### 9.2 Status

**Phase 5b is optional and deferred until after Phase 5a is complete and stable.** The C emitter is the primary compilation path for v1.0. LLVM is a Phase 2.0 target.

### 9.3 Scope for Future Implementation

When implemented:
- Emit LLVM IR text format (`.ll`) — human-readable, no need for the LLVM C API
- Use `opt` for optimization passes, `llc` for native code generation
- Same parity test suite as Phase 5a
- Activated by `forge build --target llvm`

### 9.4 Preliminary Design Notes

LLVM IR is SSA (Static Single Assignment) form. Key mapping:
- FORGE procedures → LLVM `define` functions
- FORGE records → LLVM `%struct` types
- FORGE `int` → `i64`, `float` → `double`, `bool` → `i1`, `byte` → `i8`
- Dynamic arrays / maps → calls into LLVM-compatible runtime (same runtime, different ABI)

---

## 10. Phase 6 — Standard Library

### 10.1 Goal

Implement all stdlib modules defined in Spec Section 14. The stdlib has two layers:
1. **FORGE source** (`stdlib/forge/*.fg`) — the public API as FORGE procedures
2. **C bootstrap** (`stdlib/bootstrap/*_builtin.c`) — low-level implementations called by the FORGE layer

### 10.2 Implementation Strategy

Pure-FORGE implementations are preferred. Use C bootstrap only when the operation genuinely requires OS system calls or C library access (file I/O, serial ports, time, math).

**Example: `forge.str.length` is pure FORGE:**
```forge
# str.fg
export proc length(s: str) -> int:
    return _str_byte_length(s)   # _str_byte_length is a builtin
```

**`forge.serial.open` requires C:**
```c
/* serial_builtin.c */
forge_value_t forge_serial_open(forge_value_t path, forge_value_t baud) {
    /* calls open(), tcsetattr(), etc. */
}
```

### 10.3 Implementation Order

Build stdlib modules in this order, as interpreter tests need them:

| Priority | Module | Dependency |
|----------|--------|------------|
| 1 | `forge.io` | Needed from day one (print, read_line) |
| 2 | `forge.str` | Needed by almost every program |
| 3 | `forge.sys` | Needed for args, exit |
| 4 | `forge.math` | Needed for numeric programs |
| 5 | `forge.time` | Needed for any timed program |
| 6 | `forge.buf` | Needed before serial/nmea |
| 7 | `forge.serial` | Marine/embedded domain |
| 8 | `forge.nmea` | Marine domain; depends on serial and str |

### 10.4 Builtin Registration

The interpreter maintains a table of builtin procedures. When the interpreter encounters a call to a module procedure, it first checks the builtin table before looking up the FORGE source.

```c
typedef forge_value_t (*forge_builtin_fn_t)(forge_value_t* args, int arg_count,
                                             forge_interp_t* interp);

typedef struct {
    const char*         qualified_name;   /* e.g. "forge.io.print" */
    forge_builtin_fn_t  fn;
} forge_builtin_t;

static forge_builtin_t builtins[] = {
    { "forge.io.print",       builtin_io_print },
    { "forge.io.read_line",   builtin_io_read_line },
    { "forge.str.length",     builtin_str_length },
    /* ... */
    { NULL, NULL }
};
```

### 10.5 Exit Criteria

- [ ] `forge.io`: print, eprint, read_line, read_file, write_file, append_file, file_exists
- [ ] `forge.str`: all 16 functions from spec
- [ ] `forge.math`: all arithmetic, trig, log/exp, random functions
- [ ] `forge.sys`: args, env, exit, halt, platform, arch
- [ ] `forge.time`: now, sleep, timestamp, elapsed_ms, clock
- [ ] `forge.buf`: full Buffer record and all operations
- [ ] `forge.serial`: open, close, read/write operations, timeout
- [ ] `forge.nmea`: checksum validation, field extraction, GGA/RMC parsing, sentence building
- [ ] All stdlib functions work in both `forge run` and `forge build` modes

---

## 11. Phase 7 — Toolchain CLI

### 11.1 Goal

Implement `forge` as a polished, production-quality command-line tool. All subcommands must work correctly. Error messages must be clear, consistent, and actionable.

### 11.2 Files

```
src/main.c      # CLI entry point and dispatcher
```

### 11.3 Implementation Tasks

#### Task 7.1 — Argument Parsing

Implement a simple argument parser in `main.c`. No external library. Parse subcommand, positional arguments, and flags into a `forge_cli_args_t` struct.

```c
typedef struct {
    const char*     subcommand;     /* "run", "build", "check", "fmt", "repl", "doc" */
    const char*     input_file;
    const char*     output_file;    /* --out */
    const char*     target;         /* --target c|llvm */
    int             opt;            /* --opt */
    int             debug;          /* --debug */
    int             strict;         /* --strict */
    int             bounds_check;   /* --bounds-check */
    int             async_channels; /* --async-channels */
    int             trace;          /* --trace (run mode) */
    int             repl;           /* --repl (run mode) */
    const char*     cc;             /* --cc */
    const char*     arch;           /* --arch */
    const char*     os_target;      /* --os */
    const char**    include_paths;  /* -I paths */
    int             include_count;
} forge_cli_args_t;
```

#### Task 7.2 — `forge run`

Pipeline: read file → lex → parse → (type check) → interpret.

Print errors with format: `filename:line:column: error: message`

Exit code 0 on success, 1 on error.

#### Task 7.3 — `forge build`

Pipeline: read file(s) → lex → parse → type check → emit C → invoke gcc/clang.

The gcc/clang invocation:
```c
snprintf(cmd, sizeof(cmd),
    "%s -std=c99 -O%d %s -I%s %s forge_runtime.c -o %s",
    cc,             /* "gcc" or "clang" or value of --cc */
    opt ? 2 : 0,
    debug ? "-g" : "",
    runtime_include_path,
    emitted_c_file,
    output_file);
system(cmd);
```

#### Task 7.4 — `forge check`

Run the pipeline through the type checker only. Report errors and exit. No output other than errors. Exit 0 if clean.

#### Task 7.5 — `forge fmt`

Implement a source formatter. Walk the AST and re-emit FORGE source in canonical style. This is simpler than it sounds — the AST captures all structure; the formatter just walks it with consistent spacing rules.

Write to a temp file, compare to original. If different, overwrite (or report in `--check` mode).

#### Task 7.6 — `forge repl`

Implement the REPL as a persistent interpreter state:

```c
forge_interp_t* interp = interp_create(...);
char line[4096];

printf("FORGE %s REPL — 'exit' to quit\n", FORGE_VERSION);
while (1) {
    printf(">>> ");
    fflush(stdout);
    if (!fgets(line, sizeof(line), stdin)) break;
    if (strcmp(trim(line), "exit") == 0) break;

    /* Handle multi-line input (detect trailing ':') */
    /* Lex, parse as stmt or decl, execute, print result */
    forge_value_t result = repl_eval(interp, line);
    if (result.kind != VAL_VOID) {
        char* s = val_to_str(result);
        printf("%s\n", s);
        free(s);
    }
}
```

Handle multi-line input: if a line ends with `:` (opening a block), continue reading with `... ` prompt until a blank line or a dedented line closes the block.

#### Task 7.7 — `forge doc`

Walk the AST of each exported declaration. Collect the `#` comment lines immediately preceding it. Emit markdown documentation.

```markdown
## proc read(s: Sensor) -> float

Reads the current value from the sensor.
Returns -1.0 if the sensor is inactive.

**Parameters:**
- `s: Sensor` — the sensor to read

**Returns:** `float`
```

#### Task 7.8 — Error Message Quality

All error messages must follow this format:

```
filename.fg:12:5: error: type mismatch: expected 'int', got 'float'
filename.fg:12:5: note: variable 'x' declared as 'int' here
```

Rules:
- Always include filename, line, column
- `error:` prefix for fatal errors
- `warning:` prefix for non-fatal issues
- `note:` prefix for supplementary context
- Point to the exact token that caused the error
- For type errors, show both the expected and actual types
- For undeclared names, suggest similar names if any exist (edit distance ≤ 2)

### 11.9 Exit Criteria

- [ ] `forge run hello.fg` works end-to-end
- [ ] `forge build hello.fg && ./hello` produces identical output
- [ ] `forge check` exits 0 on valid programs, 1 on invalid
- [ ] `forge fmt` is idempotent (`forge fmt file.fg; forge fmt file.fg` produces no changes)
- [ ] `forge repl` handles single-line expressions, declarations, and multi-line blocks
- [ ] `forge doc` produces valid markdown for all stdlib modules
- [ ] Error messages include file, line, column for all error types

---

## 12. Phase 8 — Tooling and Ecosystem

**Status: Post-v1.0. Planned for next development cycle.**

### 12.1 Language Server Protocol (LSP)

A `forge-lsp` binary implementing the Language Server Protocol would provide:
- Go-to-definition
- Hover type information
- Error squiggles as you type
- Auto-completion for module exports and record fields

This enables editor integration for VS Code, Neovim, and any other LSP-compatible editor.

### 12.2 Syntax Highlighting

Provide TextMate grammar files (`.tmLanguage`) covering:
- Keywords
- String literals and escape sequences
- Comments
- Numeric literals
- Record names and procedure names
- Channel-related keywords (`emit`, `on`, `channel`)

Targets: VS Code extension, Neovim via Tree-sitter grammar, Sublime Text.

### 12.3 Tree-sitter Grammar

A formal Tree-sitter grammar for FORGE enables:
- Fast, incremental parsing in editors
- Syntax highlighting via Tree-sitter parsers (Neovim, Helix, GitHub)
- AST-based code navigation tools

### 12.4 Package Manager (`forge pkg`)

A minimal package manager:
- `forge pkg init` — create a new FORGE project with a `forge.toml` manifest
- `forge pkg add module_name` — add a dependency
- `forge pkg build` — build the project using manifest
- `forge pkg publish` — publish to a central registry (future)

`forge.toml` format:
```toml
[package]
name = "depth_monitor"
version = "0.1.0"
author = "Fragillidae Software"

[dependencies]
forge_nmea_utils = "1.0.0"
```

### 12.5 IDE Integration (Suite 2)

Add a FORGE IDE to the IDE Suite 2 collection following established naming conventions:
- Name: **SMELT** *(a forge-related tool — a type of small furnace; also fits the marine theme — smelt is a fish)*
- Alternative: **BELLOWS** *(what drives a forge's fire — evocative)*
- Icon: follows the suite's punctuation-imagery approach

---

This has been superceded by crearting a plugin for the existing MyCode IDE.

## 13. Testing Strategy

### 13.1 Three-Layer Test Architecture

```
Layer 1 — Unit Tests (C)          tests/unit/
Layer 2 — FORGE Source Tests      tests/forge/
Layer 3 — Integration Tests       tests/forge/programs/
```

Each layer is independent. Layer 1 runs fastest and is run most often. Layer 3 is slowest and run on commit.

### 13.2 Layer 1 — C Unit Tests

Test individual C functions in isolation. Written in plain C using a minimal test framework:

```c
/* tests/unit/test_framework.h */
#define TEST(name) static void test_##name(void)
#define RUN(name)  do { test_##name(); tests_run++; } while(0)
#define ASSERT(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL [%s:%d]: %s\n", __FILE__, __LINE__, msg); \
        tests_failed++; \
    } \
} while(0)
#define ASSERT_EQ(a, b) ASSERT((a)==(b), #a " == " #b)
#define ASSERT_STR_EQ(a, b) ASSERT(strcmp((a),(b))==0, #a " == " #b)
```

No external test framework (no CMocka, no Check, no Unity). The test framework is 20 lines.

### 13.3 Layer 2 — FORGE Source Tests

Each FORGE test file has a matching `.expected` file containing the expected stdout output. The test runner executes `forge run test.fg` and diffs the output.

```
tests/forge/03_interp/
├── arithmetic.fg            arithmetic.expected
├── strings.fg               strings.expected
├── records.fg               records.expected
├── arrays_fixed.fg          arrays_fixed.expected
├── arrays_dynamic.fg        arrays_dynamic.expected
├── maps.fg                  maps.expected
├── optionals.fg             optionals.expected
├── loops_while.fg           loops_while.expected
├── loops_for.fg             loops_for.expected
├── loops_loop.fg            loops_loop.expected
├── procs_basic.fg           procs_basic.expected
├── procs_ref.fg             procs_ref.expected
├── procs_recursion.fg       procs_recursion.expected
├── channels_basic.fg        channels_basic.expected
├── channels_cross_module.fg channels_cross_module.expected
├── error_divide_by_zero.fg  error_divide_by_zero.expected  (expected stderr)
└── error_oob.fg             error_oob.expected
```

### 13.4 Layer 3 — Integration / Program Tests

Full FORGE programs that exercise multiple features together. For each:

```
tests/forge/programs/
├── fibonacci/
│   ├── main.fg
│   └── expected.txt
├── bubble_sort/
│   ├── main.fg
│   └── expected.txt
├── nmea_parser/
│   ├── main.fg
│   ├── parser.fg
│   ├── sample_input.txt
│   └── expected.txt
├── sensor_pipeline/
│   ├── main.fg
│   ├── sensors.fg
│   ├── display.fg
│   ├── logger.fg
│   └── expected.txt
└── event_counter/
    ├── main.fg
    └── expected.txt
```

### 13.5 Parity Tests (Interp vs Compiled)

For every Layer 2 and Layer 3 test, the test runner also:
1. Compiles with `forge build`
2. Runs the compiled binary
3. Asserts output is identical to interpreted output

This ensures behavioral parity between the two execution paths.

### 13.6 Error Tests

Tests for invalid FORGE programs that must produce specific error messages:

```
tests/forge/errors/
├── type_mismatch.fg            type_mismatch.expected_err
├── undefined_variable.fg       undefined_variable.expected_err
├── wrong_arg_count.fg          wrong_arg_count.expected_err
├── circular_import/            (multi-file test)
├── bad_indent.fg               bad_indent.expected_err
└── tab_in_source.fg            tab_in_source.expected_err
```

The `.expected_err` file contains patterns (not exact strings) for the expected error output. The test runner uses `grep` for pattern matching.

### 13.7 Test Runner Script

```bash
#!/usr/bin/env bash
# tests/runner.sh

PASS=0
FAIL=0
FORGE=./forge

run_test() {
    local fg_file="$1"
    local expected="$2"
    local actual
    actual=$($FORGE run "$fg_file" 2>&1)
    if diff -q <(echo "$actual") "$expected" > /dev/null 2>&1; then
        PASS=$((PASS + 1))
    else
        FAIL=$((FAIL + 1))
        echo "FAIL: $fg_file"
        diff <(echo "$actual") "$expected"
    fi
}

# Run all layer 2 tests
for fg in tests/forge/03_interp/*.fg; do
    expected="${fg%.fg}.expected"
    [ -f "$expected" ] && run_test "$fg" "$expected"
done

# ... similar loops for other layers ...

echo ""
echo "Results: $PASS passed, $FAIL failed"
[ $FAIL -eq 0 ] && exit 0 || exit 1
```

---

## 14. Coding Standards

### 14.1 C Standard

C99 strict. Compile with `-std=c99 -Wall -Wextra -Wpedantic`. Zero warnings in production code.

### 14.2 Naming Conventions

| Item | Convention | Example |
|------|-----------|---------|
| Types | `lower_snake_case_t` | `forge_token_t`, `forge_value_t` |
| Enums | `UPPER_SNAKE_CASE` | `TOK_INDENT`, `VAL_INT` |
| Functions | `module_verb_noun` | `lexer_tokenize`, `env_get`, `val_copy` |
| Macros | `UPPER_SNAKE_CASE` | `ARENA_ALLOC`, `EMIT`, `ASSERT_EQ` |
| Constants | `UPPER_SNAKE_CASE` | `MAX_HANDLERS`, `FORGE_VERSION` |
| Local variables | `lower_snake_case` | `token_count`, `current_indent` |
| File-private | `static` qualifier always | `static int count_spaces(...)` |

### 14.3 Header Guards

```c
#ifndef FORGE_LEXER_H
#define FORGE_LEXER_H

/* ... */

#endif /* FORGE_LEXER_H */
```

### 14.4 Function Length

Keep functions under 60 lines. If a function exceeds this, break it into helpers. The recursive descent parser functions are an exception — they may be longer if the grammar rule is complex, but must be clearly structured.

### 14.5 Error Handling in C Code

Toolchain internal errors (bugs, not user errors) use `assert()` from `<assert.h>`. User-facing errors use the `error.h` reporting functions. Never use `printf` directly for errors in library code.

```c
/* Internal toolchain invariant */
assert(node != NULL && "ast_eval called with NULL node");

/* User-facing error */
error_report(checker->filename, node->line, node->column,
             "type mismatch: expected '%s', got '%s'",
             type_name(expected), type_name(actual));
```

### 14.6 Memory Discipline

- All AST nodes allocated from arena — never `free()` individually
- Runtime values (`forge_value_t`) track ownership via the `owned` flag on strings and the heap flag on arrays
- Every `malloc` in non-arena code has a corresponding `free` path
- Use `valgrind --leak-check=full` before marking any phase complete

### 14.7 Comments

Write comments that explain *why*, not *what*. The code shows what. The comment explains intent, invariants, and tradeoffs.

```c
/* Bad */
i++;    /* increment i */

/* Good */
/* Skip the closing delimiter character before continuing to the next token */
i++;
```

Document every public function in the header with a brief description, parameter semantics, and return value.

```c
/**
 * Intern a string into the table. If an identical string already exists,
 * returns the existing pointer. The returned pointer is valid for the
 * lifetime of the strtable.
 *
 * @param t    The intern table
 * @param s    The string bytes (need not be null-terminated)
 * @param len  Byte length of s
 * @return     Interned null-terminated string pointer (not owned by caller)
 */
const char* strtable_intern(forge_strtable_t* t, const char* s, int len);
```

### 14.8 Version Control Practice

- One commit per completed task (e.g., "Phase 1 Task 1.3: single-character tokens")
- Commit message format: `Phase N Task N.M: brief description`
- Do not commit code that fails existing tests
- Tag each phase completion: `v0.1-phase1`, `v0.1-phase2`, etc.

---

## 15. Milestone Summary

| Milestone | Description | Key Deliverable |
|-----------|-------------|-----------------|
| **M1** | Phase 1 complete | `forge` tokenizes any FORGE source correctly |
| **M2** | Phase 2 complete | `forge` parses and pretty-prints AST |
| **M3** | Phase 3 complete | `forge run` executes FORGE programs |
| **M3.5** | stdlib core complete | `forge.io`, `forge.str`, `forge.sys` working |
| **M4** | Phase 4 complete | `forge check` catches all type errors |
| **M5** | Phase 5a complete | `forge build` produces native binaries via C |
| **M5.5** | stdlib full complete | All 8 stdlib modules working in both modes |
| **M6** | Phase 7 complete | All CLI subcommands working; REPL functional |
| **M7 — v1.0** | Integration tested | All test programs pass; parity verified |
| **M8** | Phase 8 begins | Editor integration, LSP, packaging |

---

## 16. Risk Register

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|-----------|
| Indentation lexing edge cases (blank lines in blocks, mixed DEDENT levels) | High | High | Extensive lexer tests before moving to parser; test with pathological indent patterns |
| Interpreter and compiler behavioral divergence | Medium | High | Parity test suite run after every compiler change; interpreter is the reference |
| C emitter produces code with undefined behavior on edge cases | Medium | Medium | Compile emitted code with `-fsanitize=undefined,address` in test mode |
| FORGE standard library platform differences (serial ports on Windows vs Linux) | High | Medium | Abstract platform differences behind `forge.sys.platform()` guards; document limitations |
| Grammar ambiguity (assignment vs expression statement) | Low | High | Already identified and resolved via lookahead; test exhaustively |
| Memory leaks in interpreter value management | Medium | Medium | Valgrind on all interpreter tests before phase completion |
| LLVM IR emitter complexity underestimated | High | Low | Phase 5b is explicitly deferred; C emitter is the v1 target |
| String interning interaction with string mutation | Low | High | Strings are immutable; all operations produce new strings; document this invariant prominently |
| Serial port / NMEA library available only on Linux/macOS | Medium | Low | Stub implementations for Windows; primary target is Linux (marine systems) |

---

## Appendix A — File Tree

Complete repository file tree at v1.0 completion. See [Section 1.1](#11-repository-layout).

---

## Appendix B — Key Data Structures Cheatsheet

```
forge_strtable_t    String intern table (hash set of char*)
forge_arena_t       Arena allocator (bump-pointer, linked blocks)
forge_token_t       Lexer token (type, source ref, literal value, position)
forge_lexer_t       Lexer state (source buffer, position, indent stack, output tokens)
forge_node_t        AST node (tagged union, all node kinds)
forge_parser_t      Parser state (token array cursor, arena, error state)
forge_type_t        Type descriptor (tagged union of all FORGE types)
forge_checker_t     Type checker state (symbol tables, import map)
forge_value_t       Runtime value (tagged union, all value kinds)
forge_env_t         Scope frame (name→value bindings, parent pointer)
forge_channel_t     Channel descriptor (name, handler array)
forge_channel_registry_t  All channels in the running program
forge_interp_t      Interpreter state (modules, channels, call stack)
forge_emitter_t     C emitter state (output FILE*, indent level, counters)
forge_cli_args_t    Parsed CLI arguments
forge_array_t       Runtime dynamic array (data, len, cap, elem_size)
forge_str_t         Runtime string (data, len, owned flag)
forge_map_t         Runtime hash map (opaque)
```

---

## Appendix C — Test File Naming Convention

```
##_descriptive_name.fg        FORGE source test
##_descriptive_name.expected  Expected stdout (for run/build tests)
##_descriptive_name.expected_err  Expected stderr pattern (for error tests)

##  = two-digit sequence number within the phase directory
      Allows sorting and easy identification of test order

Examples:
  01_hello.fg
  02_arithmetic.fg
  03_string_concat.fg
  10_records_basic.fg
  11_records_nested.fg
  20_channels_emit.fg
  21_channels_cross_module.fg
```

Error test `.expected_err` files use grep-compatible patterns, one per line. All patterns must match for the test to pass:

```
# type_mismatch.expected_err
type_mismatch.fg:[0-9]*:[0-9]*: error:
expected.*int.*got.*float
```

---

*FORGE Implementation Plan v0.1 — Fragillidae Software — Confidential*  
*Companion document: FORGE_Language_Spec.md v0.1*
