# FORGE VS Code Extension — Part 2
## Language Server Protocol (LSP) Implementation

**Document:** Development Plan  
**Version:** 0.1  
**Status:** Active — Begin after FORGE Toolchain Phase 4 (Type Checker) complete  
**Companion Documents:**  
  `FORGE_Language_Spec.md` v0.1  
  `FORGE_Implementation_Plan.md` v0.1  
  `FORGE_VSCode_Extension_Part1.md` v0.1  
**Author:** Fragillidae Software  
**Prerequisites:**  
  Part 1 extension published  
  FORGE toolchain Phase 4 (type checker) complete  
  FORGE toolchain Phase 3 (interpreter) complete  

---

> Part 2 transforms the FORGE extension from a syntax highlighter into a full development environment. It adds the Language Server — a separate process that runs the FORGE frontend on demand and communicates with VS Code via the Language Server Protocol. Unlike Part 1 (pure JSON), Part 2 requires TypeScript for the VS Code client side and C (or a thin Node.js wrapper) for the server side.

---

## Table of Contents

1. [Overview and Goals](#1-overview-and-goals)
2. [LSP Architecture](#2-lsp-architecture)
3. [Repository Structure](#3-repository-structure)
4. [Technology Decisions](#4-technology-decisions)
5. [Phase 2A — Server Foundation](#5-phase-2a--server-foundation)
6. [Phase 2B — Core LSP Features](#6-phase-2b--core-lsp-features)
7. [Phase 2C — Advanced LSP Features](#7-phase-2c--advanced-lsp-features)
8. [Phase 2D — Extension Client](#8-phase-2d--extension-client)
9. [Phase 2E — Integration and Polish](#9-phase-2e--integration-and-polish)
10. [LSP Feature Reference](#10-lsp-feature-reference)
11. [Testing Strategy](#11-testing-strategy)
12. [Build and Release Process](#12-build-and-release-process)
13. [Task Checklist](#13-task-checklist)
14. [Appendix A — LSP Message Reference](#appendix-a--lsp-message-reference)
15. [Appendix B — Server Stdio Protocol](#appendix-b--server-stdio-protocol)
16. [Appendix C — Capability Declaration Reference](#appendix-c--capability-declaration-reference)

---

## 1. Overview and Goals

### 1.1 What Part 2 Delivers

| Feature | LSP Method | Priority |
|---------|------------|----------|
| Inline error diagnostics | `textDocument/publishDiagnostics` | P1 — highest value |
| Hover type information | `textDocument/hover` | P1 |
| Go-to-definition | `textDocument/definition` | P1 |
| Auto-complete | `textDocument/completion` | P1 |
| Document symbols (outline) | `textDocument/documentSymbol` | P2 |
| Find all references | `textDocument/references` | P2 |
| Rename symbol | `textDocument/rename` | P2 |
| Signature help | `textDocument/signatureHelp` | P2 |
| Semantic highlighting | `textDocument/semanticTokens` | P2 |
| Code formatting | `textDocument/formatting` | P3 |
| Code actions (quick fixes) | `textDocument/codeAction` | P3 |
| Workspace symbols | `workspace/symbol` | P3 |
| Inlay hints | `textDocument/inlayHint` | P3 |

**P1 features ship in the first LSP release.** P2 in the second. P3 are stretch goals.

### 1.2 User Experience Goals

When Part 2 is complete, a developer writing FORGE code in VS Code should experience:

- Red squiggles on type errors as they type, before saving
- Hover over any identifier to see its type
- `F12` on a procedure call to jump to its definition
- `Ctrl+Space` to get completions for module exports and record fields
- The Problems panel populated with all errors and warnings in the workspace
- An outline panel showing all procedures, records, and channels in the current file

### 1.3 Why LSP Specifically

LSP (Language Server Protocol) was designed by Microsoft to separate language intelligence from editor UI. The server runs as a separate process; editors communicate with it via a standard JSON-RPC protocol over stdin/stdout. This means:

- One server implementation works for VS Code, Neovim (via `nvim-lspconfig`), Helix, Emacs (Eglot), Sublime Text (LSP plugin), and any other LSP-compatible editor
- The server is not tied to the Node.js/TypeScript ecosystem — it can be written in any language
- The TypeScript glue code in the VS Code extension is minimal — mostly just spawning the server process and forwarding messages

---

## 2. LSP Architecture

### 2.1 Component Overview

```
┌─────────────────────────────────────────────────────────────────┐
│  VS Code                                                        │
│                                                                 │
│  ┌──────────────────────┐         ┌──────────────────────────┐  │
│  │   forge-language     │         │   Editor UI              │  │
│  │   Extension          │         │                          │  │
│  │   (TypeScript)       │◄───────►│  Squiggles, hover,       │  │
│  │                      │         │  completions, outline... │  │
│  │   LSP Client         │         └──────────────────────────┘  │
│  └──────────┬───────────┘                                       │
│             │  JSON-RPC over stdin/stdout                       │
└─────────────┼───────────────────────────────────────────────────┘
              │
              ▼
┌─────────────────────────────────────────────────────────────────┐
│  forge-lsp (separate process)                                   │
│                                                                 │
│  JSON-RPC message handler                                       │
│       │                                                         │
│       ▼                                                         │
│  Document store (in-memory source buffers)                      │
│       │                                                         │
│       ▼                                                         │
│  FORGE Frontend  ──►  Lexer ──► Parser ──► Type Checker        │
│       │                                                         │
│       ▼                                                         │
│  Symbol index (procs, records, channels, across all files)      │
│       │                                                         │
│       ▼                                                         │
│  LSP feature handlers (hover, completion, definition, ...)      │
└─────────────────────────────────────────────────────────────────┘
```

### 2.2 Communication Protocol

The client (VS Code extension) spawns `forge-lsp` as a child process. All communication is via JSON-RPC 2.0 messages over the process's stdin/stdout, using the LSP base protocol framing:

```
Content-Length: <byte-length>\r\n
\r\n
<JSON body>
```

Example request (client → server):
```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "textDocument/hover",
  "params": {
    "textDocument": { "uri": "file:///path/to/sensors.fg" },
    "position": { "line": 12, "character": 8 }
  }
}
```

Example response (server → client):
```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "result": {
    "contents": {
      "kind": "markdown",
      "value": "**proc** `read(s: Sensor) -> float`\n\nReads the current sensor value."
    },
    "range": {
      "start": { "line": 12, "character": 4 },
      "end":   { "line": 12, "character": 8 }
    }
  }
}
```

### 2.3 Document Synchronization

VS Code sends the server a copy of the document content when:
- A FORGE file is opened (`textDocument/didOpen`)
- The user types and changes are made (`textDocument/didChange`)
- A file is saved (`textDocument/didSave`)
- A file is closed (`textDocument/didClose`)

The server maintains an in-memory document store — it does not read from the filesystem for analysis. This ensures the user sees diagnostics based on what they have typed, not what is saved on disk.

### 2.4 Incremental vs Full Sync

For v1 of the LSP, use **full document sync** (`TextDocumentSyncKind.Full`). On every change, VS Code sends the complete document content. The server re-analyzes from scratch.

Full sync is simpler to implement. For small-to-medium FORGE files (educational programs, marine tools), the performance is perfectly acceptable. Incremental sync (sending only diffs) is a future optimization.

---

## 3. Repository Structure

The LSP is developed in the same repository as the Part 1 extension, in a new `server/` subdirectory.

```
forge-vscode/
├── package.json                    # Updated: adds LSP client dependency
├── tsconfig.json                   # TypeScript config for client
│
├── client/                         # VS Code extension (TypeScript)
│   ├── src/
│   │   ├── extension.ts            # Extension entry point
│   │   └── client.ts               # LSP client setup
│   └── package.json
│
├── server/                         # Language server
│   ├── forge-lsp.c                 # Main server (C) — or Node.js wrapper
│   ├── forge-lsp.h
│   ├── jsonrpc.c / jsonrpc.h       # JSON-RPC framing layer
│   ├── lsp_handlers.c              # One function per LSP method
│   ├── lsp_handlers.h
│   ├── document_store.c            # In-memory source buffer management
│   ├── document_store.h
│   ├── symbol_index.c              # Cross-file symbol lookup
│   ├── symbol_index.h
│   └── Makefile
│
├── syntaxes/                       # Unchanged from Part 1
├── language-configuration.json    # Unchanged from Part 1
├── snippets/                       # Unchanged from Part 1
└── icons/                          # Unchanged from Part 1
```

### 3.1 Relationship to the FORGE Toolchain

The language server reuses the FORGE lexer, parser, and type checker from the main toolchain repository. There are two options:

**Option A — Embed as source:** Copy `src/lexer/`, `src/parser/`, `src/typecheck/` into the `server/` directory. Simpler, but requires keeping them in sync with the toolchain manually.

**Option B — Link as library:** Build the toolchain frontend as a static library (`libforge_frontend.a`) and link it into `forge-lsp`. More complex to set up, but changes to the toolchain automatically flow into the LSP.

**Recommendation:** Start with Option A (copy). Once the toolchain is stable at v1.0, migrate to Option B.

---

## 4. Technology Decisions

### 4.1 Server Language: C

The language server is implemented in C, consistent with the rest of the FORGE toolchain. This means:

- The same lexer, parser, and type checker code runs in both the toolchain and the LSP
- No additional runtime dependencies — the server binary is standalone
- The binary is shipped inside the VS Code extension package

The main complexity this adds is JSON parsing and generation in C. Use a small embedded JSON library — `cJSON` (MIT licensed, single `.c`/`.h` file) is the recommended choice.

### 4.2 Client Language: TypeScript

The VS Code extension client must be TypeScript (VS Code's extension API is TypeScript/JavaScript). However, the client code for Part 2 is very thin — roughly 80–100 lines:

- Spawn the `forge-lsp` binary
- Create an `lsp.LanguageClient` pointed at the process
- Declare which document types the client handles
- Activate on VS Code startup

The `vscode-languageclient` npm package handles all LSP wire protocol details on the client side.

### 4.3 JSON Library: cJSON

`cJSON` is a single-file C JSON parser and generator, MIT licensed, widely used in embedded systems:

```bash
# Download into server/
curl -o server/cjson.h https://raw.githubusercontent.com/DaveGamble/cJSON/master/cJSON.h
curl -o server/cjson.c https://raw.githubusercontent.com/DaveGamble/cJSON/master/cJSON.c
```

Used for:
- Parsing incoming LSP requests
- Generating LSP responses and notifications

### 4.4 Binary Distribution

The compiled `forge-lsp` binary is bundled inside the VS Code extension package (`.vsix`). The extension package includes binaries for:

| Platform | Binary |
|----------|--------|
| Linux x64 | `bin/linux-x64/forge-lsp` |
| Linux arm64 | `bin/linux-arm64/forge-lsp` (for Raspberry Pi / ARM marine systems) |
| macOS x64 | `bin/darwin-x64/forge-lsp` |
| macOS arm64 | `bin/darwin-arm64/forge-lsp` (Apple Silicon) |
| Windows x64 | `bin/win32-x64/forge-lsp.exe` |

The extension client selects the correct binary at runtime based on `process.platform` and `process.arch`. A CI pipeline (GitHub Actions) builds all platform binaries on each release.

For v0.1 LSP development: target Linux x64 and macOS only. Add Windows after core features are working.

---

## 5. Phase 2A — Server Foundation

**Goal:** A `forge-lsp` binary that speaks the LSP wire protocol, handles `initialize` / `initialized` / `shutdown` / `exit`, and responds to `textDocument/didOpen` without crashing.

This phase produces no visible user features but establishes the communication layer that everything else builds on.

### 5.1 Task 2A.1 — JSON-RPC Framing Layer

Implement the LSP base protocol reader/writer in `jsonrpc.c`:

```c
/* jsonrpc.h */

typedef struct {
    char*   method;         /* null for responses */
    int     has_id;
    int     id;             /* request id (for responses) */
    cJSON*  params;         /* owned — caller must cJSON_Delete */
    cJSON*  result;         /* for responses */
    cJSON*  error;          /* for error responses */
} lsp_message_t;

/* Read one complete message from stdin.
   Blocks until Content-Length header + body are available.
   Returns 1 on success, 0 on EOF, -1 on error. */
int  lsp_read_message(lsp_message_t* msg);

/* Write a response to stdout. */
void lsp_send_response(int id, cJSON* result);
void lsp_send_error(int id, int code, const char* message);
void lsp_send_notification(const char* method, cJSON* params);

void lsp_message_free(lsp_message_t* msg);
```

**Reading algorithm:**
```c
int lsp_read_message(lsp_message_t* msg) {
    int content_length = -1;
    char header_line[256];

    /* Read headers until blank line */
    while (fgets(header_line, sizeof(header_line), stdin)) {
        if (strcmp(header_line, "\r\n") == 0) break;
        if (strncmp(header_line, "Content-Length: ", 16) == 0)
            content_length = atoi(header_line + 16);
    }
    if (content_length <= 0) return -1;

    /* Read body */
    char* body = malloc(content_length + 1);
    fread(body, 1, content_length, stdin);
    body[content_length] = '\0';

    /* Parse JSON */
    cJSON* json = cJSON_Parse(body);
    free(body);
    if (!json) return -1;

    /* Extract fields */
    cJSON* method = cJSON_GetObjectItem(json, "method");
    cJSON* id     = cJSON_GetObjectItem(json, "id");
    msg->method   = method ? strdup(method->valuestring) : NULL;
    msg->has_id   = (id != NULL);
    msg->id       = id ? id->valueint : 0;
    msg->params   = cJSON_DetachItemFromObject(json, "params");
    cJSON_Delete(json);
    return 1;
}
```

**Writing algorithm:**
```c
void lsp_send_response(int id, cJSON* result) {
    cJSON* resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "jsonrpc", "2.0");
    cJSON_AddNumberToObject(resp, "id", id);
    cJSON_AddItemToObject(resp, "result", result);  /* result is consumed */
    char* body = cJSON_Print(resp);
    cJSON_Delete(resp);
    fprintf(stdout, "Content-Length: %zu\r\n\r\n%s", strlen(body), body);
    fflush(stdout);
    free(body);
}
```

### 5.2 Task 2A.2 — Document Store

```c
/* document_store.h */

#define MAX_DOCUMENTS 64

typedef struct {
    char*   uri;            /* file URI: "file:///path/to/file.fg" */
    char*   text;           /* full document content (owned) */
    int     version;        /* increments with each change */
} lsp_document_t;

typedef struct {
    lsp_document_t  docs[MAX_DOCUMENTS];
    int             count;
} lsp_document_store_t;

void  docstore_init(lsp_document_store_t* store);
void  docstore_open(lsp_document_store_t* store,
                    const char* uri, const char* text, int version);
void  docstore_update(lsp_document_store_t* store,
                      const char* uri, const char* text, int version);
void  docstore_close(lsp_document_store_t* store, const char* uri);
lsp_document_t* docstore_get(lsp_document_store_t* store, const char* uri);
```

For Part 2 v1, `MAX_DOCUMENTS 64` is sufficient. Educational programs are small; marine tools rarely have more than a dozen modules.

### 5.3 Task 2A.3 — LSP Lifecycle Handlers

Implement the four lifecycle methods:

**`initialize`:** The client sends its capabilities and the server responds with its own capabilities. This is where the server declares what LSP features it supports.

```c
cJSON* handle_initialize(cJSON* params) {
    cJSON* result = cJSON_CreateObject();
    cJSON* caps   = cJSON_CreateObject();

    /* Text document sync: Full (send full content on every change) */
    cJSON_AddNumberToObject(caps, "textDocumentSync", 1);  /* 1 = Full */

    /* Declare supported features (expand as phases complete) */
    cJSON_AddTrueToObject(caps, "hoverProvider");
    cJSON_AddTrueToObject(caps, "definitionProvider");
    /* completionProvider, referencesProvider etc. added in Phase 2B/2C */

    cJSON_AddItemToObject(result, "capabilities", caps);
    cJSON* server_info = cJSON_CreateObject();
    cJSON_AddStringToObject(server_info, "name", "forge-lsp");
    cJSON_AddStringToObject(server_info, "version", "0.1.0");
    cJSON_AddItemToObject(result, "serverInfo", server_info);
    return result;
}
```

**`initialized`:** Client notification that initialization is complete. Server can begin background analysis.

**`shutdown`:** Client requests clean shutdown. Server stops accepting requests, returns null result.

**`exit`:** Client notification to terminate the process. Server calls `exit(0)` (or `exit(1)` if shutdown was not called first).

### 5.4 Task 2A.4 — Main Server Loop

```c
/* forge-lsp.c */

int main(void) {
    lsp_document_store_t store;
    docstore_init(&store);

    /* Redirect stderr to a log file — stdout is used for LSP messages */
    freopen("/tmp/forge-lsp.log", "w", stderr);
    fprintf(stderr, "forge-lsp started\n");

    int running = 1;
    while (running) {
        lsp_message_t msg;
        int r = lsp_read_message(&msg);
        if (r <= 0) break;

        fprintf(stderr, "recv: %s\n", msg.method ? msg.method : "(response)");

        if (!msg.method) {
            /* Response to a server-initiated request — ignore for now */
        } else if (strcmp(msg.method, "initialize") == 0) {
            cJSON* result = handle_initialize(msg.params);
            lsp_send_response(msg.id, result);
        } else if (strcmp(msg.method, "initialized") == 0) {
            /* notification — no response needed */
        } else if (strcmp(msg.method, "textDocument/didOpen") == 0) {
            handle_did_open(&store, msg.params);
        } else if (strcmp(msg.method, "textDocument/didChange") == 0) {
            handle_did_change(&store, msg.params);
        } else if (strcmp(msg.method, "textDocument/didClose") == 0) {
            handle_did_close(&store, msg.params);
        } else if (strcmp(msg.method, "textDocument/hover") == 0) {
            cJSON* result = handle_hover(&store, msg.params);
            lsp_send_response(msg.id, result ? result : cJSON_CreateNull());
        } else if (strcmp(msg.method, "textDocument/definition") == 0) {
            cJSON* result = handle_definition(&store, msg.params);
            lsp_send_response(msg.id, result ? result : cJSON_CreateNull());
        } else if (strcmp(msg.method, "shutdown") == 0) {
            lsp_send_response(msg.id, cJSON_CreateNull());
            running = 0;
        } else if (strcmp(msg.method, "exit") == 0) {
            break;
        } else {
            /* Unknown method — respond with MethodNotFound error */
            lsp_send_error(msg.id, -32601, "Method not found");
        }

        lsp_message_free(&msg);
    }
    return 0;
}
```

### 5.5 Phase 2A Exit Criteria

- [ ] `forge-lsp` starts without crashing
- [ ] Responds to `initialize` with correct capability declaration
- [ ] Accepts `textDocument/didOpen` and stores document in docstore
- [ ] Accepts `textDocument/didChange` and updates docstore
- [ ] Handles `shutdown` + `exit` cleanly
- [ ] VS Code extension client can start and connect to the server
- [ ] No messages appear in VS Code's "Output → FORGE Language Server" panel indicating errors

---

## 6. Phase 2B — Core LSP Features

**Goal:** Implement the P1 features: diagnostics, hover, go-to-definition, and completion. These are the features that most dramatically change the development experience.

### 6.1 Symbol Index

Before implementing individual features, build the symbol index — an in-memory cross-file database of all FORGE declarations.

```c
/* symbol_index.h */

typedef enum {
    SYM_PROC, SYM_RECORD, SYM_CHANNEL, SYM_FIELD,
    SYM_PARAM, SYM_VAR, SYM_CONST, SYM_TYPE_ALIAS
} lsp_sym_kind_t;

typedef struct {
    lsp_sym_kind_t  kind;
    const char*     name;           /* interned */
    const char*     module_name;    /* interned */
    const char*     uri;            /* source file URI */
    int             line;           /* 0-based */
    int             column;         /* 0-based */
    forge_type_t*   type;           /* resolved type (from type checker) */
    const char*     doc_comment;    /* preceding # comment, if any */
    /* For procs: parameter information */
    forge_param_t*  params;
    int             param_count;
    forge_type_t*   return_type;
} lsp_symbol_t;

typedef struct {
    lsp_symbol_t*   symbols;        /* dynamic array */
    int             count;
    int             cap;
} lsp_symbol_index_t;

void          symindex_init(lsp_symbol_index_t* idx);
void          symindex_clear_file(lsp_symbol_index_t* idx, const char* uri);
void          symindex_add(lsp_symbol_index_t* idx, lsp_symbol_t sym);
lsp_symbol_t* symindex_find_by_name(lsp_symbol_index_t* idx,
                                    const char* name, const char* module);
lsp_symbol_t* symindex_find_at_position(lsp_symbol_index_t* idx,
                                        const char* uri, int line, int col);
lsp_symbol_t* symindex_completions(lsp_symbol_index_t* idx,
                                   const char* prefix, const char* module,
                                   int* count);
```

**Population:** After every `didOpen` or `didChange`, re-run the FORGE frontend on the document and walk the resulting AST to populate the symbol index for that file. This is the core analysis pipeline:

```c
void analyse_document(lsp_document_store_t* store,
                      lsp_symbol_index_t* symindex,
                      lsp_diagnostic_list_t* diags,
                      const char* uri) {
    lsp_document_t* doc = docstore_get(store, uri);
    if (!doc) return;

    /* Clear old data for this file */
    symindex_clear_file(symindex, uri);
    diaglist_clear_file(diags, uri);

    /* Run FORGE frontend */
    forge_arena_t*      arena    = arena_create(65536);
    forge_strtable_t*   strtable = strtable_create();
    forge_lexer_t*      lexer    = lexer_create(doc->text, strlen(doc->text),
                                                uri, strtable);
    lexer_tokenize(lexer);
    forge_parser_t*     parser   = parser_create(lexer->tokens, lexer->token_count,
                                                  arena, strtable, uri);
    forge_node_t*       ast      = parser_parse(parser);
    forge_checker_t*    checker  = checker_create(arena, strtable, uri);
    checker_check(checker, ast);

    /* Collect diagnostics from lexer, parser, checker errors */
    collect_diagnostics(lexer, parser, checker, diags, uri);

    /* Populate symbol index from AST */
    populate_symbol_index(ast, checker, symindex, uri);

    /* Send diagnostics to client */
    publish_diagnostics(uri, diags);

    /* Cleanup */
    checker_destroy(checker);
    parser_destroy(parser);
    lexer_destroy(lexer);
    strtable_destroy(strtable);
    arena_destroy(arena);
}
```

### 6.2 Task 2B.1 — Diagnostics (`textDocument/publishDiagnostics`)

Diagnostics are push notifications — the server sends them to the client without being asked, whenever a document changes.

```c
void publish_diagnostics(const char* uri, lsp_diagnostic_list_t* diags) {
    cJSON* params  = cJSON_CreateObject();
    cJSON_AddStringToObject(params, "uri", uri);
    cJSON* diag_arr = cJSON_CreateArray();

    for (int i = 0; i < diags->count; i++) {
        if (strcmp(diags->items[i].uri, uri) != 0) continue;
        lsp_diagnostic_t* d = &diags->items[i];

        cJSON* diag = cJSON_CreateObject();
        cJSON* range = make_lsp_range(d->line, d->column, d->line, d->end_column);
        cJSON_AddItemToObject(diag, "range", range);
        cJSON_AddNumberToObject(diag, "severity",
            d->is_warning ? 2 : 1);  /* 1=error, 2=warning, 3=info, 4=hint */
        cJSON_AddStringToObject(diag, "source", "forge");
        cJSON_AddStringToObject(diag, "message", d->message);
        cJSON_AddItemToArray(diag_arr, diag);
    }

    cJSON_AddItemToObject(params, "diagnostics", diag_arr);
    lsp_send_notification("textDocument/publishDiagnostics", params);
}
```

**Mapping FORGE errors to diagnostics:** Every error reported by the lexer, parser, or type checker has a file, line, column, and message. Map these directly to LSP diagnostic objects. Toolchain errors with `error:` prefix get severity 1 (error); warnings get severity 2 (warning).

### 6.3 Task 2B.2 — Hover (`textDocument/hover`)

The user hovers over an identifier. VS Code sends the cursor position. The server returns a markdown string showing the type and doc comment.

```c
cJSON* handle_hover(lsp_document_store_t* store, cJSON* params) {
    const char* uri  = get_uri(params);
    int line         = get_line(params);
    int col          = get_character(params);

    lsp_symbol_t* sym = symindex_find_at_position(&g_symindex, uri, line, col);
    if (!sym) return NULL;

    /* Build markdown content */
    char buf[2048];
    format_symbol_hover(sym, buf, sizeof(buf));

    cJSON* result    = cJSON_CreateObject();
    cJSON* contents  = cJSON_CreateObject();
    cJSON_AddStringToObject(contents, "kind", "markdown");
    cJSON_AddStringToObject(contents, "value", buf);
    cJSON_AddItemToObject(result, "contents", contents);
    cJSON_AddItemToObject(result, "range", make_lsp_range(line, col, line, col));
    return result;
}
```

**Hover content format for different symbol types:**

```markdown
<!-- Procedure hover -->
**proc** `read(s: Sensor) -> float`

Reads the current value from the sensor.
Returns -1.0 if the sensor is inactive.

*Module:* `sensors`

<!-- Record hover -->
**record** `Sensor`

| Field | Type |
|-------|------|
| `id` | `int` |
| `label` | `str` |
| `value` | `float` |
| `active` | `bool` |

<!-- Variable hover -->
**var** `depth: float`

*Declared at:* `main.fg:14`

<!-- Channel hover -->
**channel** `depth_reading: float`

*Module:* `sensors`
```

### 6.4 Task 2B.3 — Go-to-Definition (`textDocument/definition`)

The user presses `F12` on an identifier. VS Code sends the position. The server returns the location where the symbol was declared.

```c
cJSON* handle_definition(lsp_document_store_t* store, cJSON* params) {
    const char* uri = get_uri(params);
    int line        = get_line(params);
    int col         = get_character(params);

    lsp_symbol_t* sym = symindex_find_at_position(&g_symindex, uri, line, col);
    if (!sym) return NULL;

    /* Return the declaration location */
    cJSON* location  = cJSON_CreateObject();
    cJSON_AddStringToObject(location, "uri", sym->uri);
    cJSON_AddItemToObject(location, "range",
        make_lsp_range(sym->line, sym->column,
                       sym->line, sym->column + strlen(sym->name)));
    return location;
}
```

For cross-module definitions (e.g., `sensors.read` where `read` is in `sensors.fg`), the symbol index stores the declaration URI and position. VS Code will open the target file automatically.

### 6.5 Task 2B.4 — Completion (`textDocument/completion`)

The user types and presses `Ctrl+Space` (or VS Code triggers automatically). The server returns a list of completion items.

**Context detection:** Parse the text to the left of the cursor on the current line to determine completion context:

| Context | Example | Completions |
|---------|---------|-------------|
| After `import ` | `import ` | All known module names |
| After `module.` | `sensors.` | Exported symbols from that module |
| After `record.` | `sensor.` | Field names of that record type |
| After `forge.` | `forge.` | All stdlib submodules |
| After `forge.io.` | `forge.io.` | All exports of `forge.io` |
| Start of statement | `pr` | Keywords + all proc names in scope |
| Type position | `var x: ` | All known type names |

```c
cJSON* handle_completion(lsp_document_store_t* store, cJSON* params) {
    const char* uri = get_uri(params);
    int line        = get_line(params);
    int col         = get_character(params);

    /* Get current line text up to cursor */
    char line_text[4096];
    get_line_text(store, uri, line, col, line_text, sizeof(line_text));

    lsp_completion_context_t ctx = detect_completion_context(line_text);

    cJSON* items = cJSON_CreateArray();

    switch (ctx.kind) {
    case CTX_MODULE_MEMBER:
        add_module_completions(items, ctx.module_name, &g_symindex);
        break;
    case CTX_RECORD_FIELD:
        add_field_completions(items, ctx.record_type);
        break;
    case CTX_IMPORT:
        add_module_name_completions(items, &g_symindex);
        break;
    case CTX_TYPE:
        add_type_completions(items, &g_symindex);
        break;
    case CTX_GENERAL:
    default:
        add_keyword_completions(items);
        add_proc_completions(items, &g_symindex, uri);
        break;
    }

    cJSON* result = cJSON_CreateObject();
    cJSON_AddFalseToObject(result, "isIncomplete");
    cJSON_AddItemToObject(result, "items", items);
    return result;
}
```

**Completion item structure:**

```c
cJSON* make_completion_item(const char* label, int kind,
                             const char* detail, const char* doc) {
    cJSON* item = cJSON_CreateObject();
    cJSON_AddStringToObject(item, "label", label);
    cJSON_AddNumberToObject(item, "kind", kind);
    /* Completion item kinds:
       1=Text, 2=Method, 3=Function, 4=Constructor, 5=Field,
       6=Variable, 7=Class, 8=Interface, 9=Module, 14=Keyword, 15=Snippet */
    if (detail) cJSON_AddStringToObject(item, "detail", detail);
    if (doc) {
        cJSON* docs = cJSON_CreateObject();
        cJSON_AddStringToObject(docs, "kind", "markdown");
        cJSON_AddStringToObject(docs, "value", doc);
        cJSON_AddItemToObject(item, "documentation", docs);
    }
    return item;
}

/* Example: procedure completion */
/* label: "read", kind: 3 (Function), detail: "(s: Sensor) -> float" */
```

### 6.6 Phase 2B Exit Criteria

- [ ] Red squiggles appear on type errors in `.fg` files
- [ ] Squiggles update within 1–2 seconds of typing
- [ ] Hover over a procedure name shows its signature and doc comment
- [ ] Hover over a variable shows its type and declaration location
- [ ] Hover over a record name shows all fields and their types
- [ ] `F12` on a proc call jumps to the proc declaration
- [ ] `F12` on a module-qualified name (`sensors.read`) jumps into the correct file
- [ ] `Ctrl+Space` after `sensors.` shows only exported symbols from `sensors`
- [ ] `Ctrl+Space` at statement start shows keywords and in-scope procs
- [ ] Completion items show correct type signatures in the detail field

---

## 7. Phase 2C — Advanced LSP Features

**Goal:** Implement P2 features. These add significant value but are not as universally needed as P1.

### 7.1 Task 2C.1 — Document Symbols (`textDocument/documentSymbol`)

Returns an outline of all declarations in the current file. Appears in VS Code's Outline panel and breadcrumb navigation.

For each file, walk the top-level declarations and return:

```json
[
  { "name": "Sensor",          "kind": 23, "range": {...}, "selectionRange": {...} },
  { "name": "read",            "kind": 12, "range": {...}, "selectionRange": {...} },
  { "name": "depth_reading",   "kind": 14, "range": {...}, "selectionRange": {...} },
  { "name": "validate",        "kind": 12, "range": {...}, "selectionRange": {...} }
]
```

Symbol kinds: 5=Class (use for record), 12=Function (proc), 14=Event (channel), 13=Variable (var/const).

### 7.2 Task 2C.2 — Find All References (`textDocument/references`)

Returns all locations where a symbol is used. Walk the AST of all open documents, find all `IDENT` and `QUALIFIED_IDENT` nodes that resolve to the target symbol.

### 7.3 Task 2C.3 — Rename Symbol (`textDocument/rename`)

Returns a `WorkspaceEdit` — a list of text replacements across all open files. Use the same logic as Find All References to locate all occurrences, then generate replacement operations for each.

Important: renaming a symbol does not rename the module (file). Renaming a module is a file rename operation, which is a separate LSP method (`workspace/willRenameFiles`).

### 7.4 Task 2C.4 — Signature Help (`textDocument/signatureHelp`)

When the user types `(` after a procedure name (or `,` between arguments), show the procedure's full signature with the current parameter highlighted.

```
read(|s: Sensor|) -> float
```

This is particularly useful for FORGE because procedure signatures are explicit and informative.

### 7.5 Task 2C.5 — Semantic Tokens (`textDocument/semanticTokens/full`)

Semantic tokens override TextMate grammar coloring with semantically accurate coloring. They are computed after type checking, so they can do things the regex grammar cannot:

- Color all uses of a specific record type in the same color
- Distinguish a local variable from a module name
- Color channel names consistently across emit sites and handler declarations
- Distinguish `forge.io.print` (stdlib) from `mymodule.print` (user-defined)

Semantic tokens require declaring a token type and modifier legend in the `initialize` response, then returning a compact array of `[line, char, length, tokenType, tokenModifiers]` tuples.

### 7.6 Phase 2C Exit Criteria

- [ ] Outline panel shows all procs, records, channels in current file
- [ ] "Find All References" returns correct locations across all open files
- [ ] Rename works correctly within a single file
- [ ] Signature help appears when typing procedure arguments
- [ ] Semantic highlighting provides more accurate coloring than TextMate grammar alone

---

## 8. Phase 2D — Extension Client

### 8.1 TypeScript Client Code

The extension client is minimal. Its job is to start the server and connect the LSP client.

```typescript
// client/src/extension.ts
import * as path from 'path';
import * as vscode from 'vscode';
import {
    LanguageClient,
    LanguageClientOptions,
    ServerOptions,
    TransportKind
} from 'vscode-languageclient/node';

let client: LanguageClient;

export function activate(context: vscode.ExtensionContext) {
    // Select the correct binary for this platform
    const platform = process.platform;
    const arch     = process.arch;
    const binaryName = platform === 'win32' ? 'forge-lsp.exe' : 'forge-lsp';
    const binaryDir  = `${platform}-${arch}`;
    const serverBin  = context.asAbsolutePath(
        path.join('bin', binaryDir, binaryName)
    );

    const serverOptions: ServerOptions = {
        run:   { command: serverBin, transport: TransportKind.stdio },
        debug: { command: serverBin, transport: TransportKind.stdio,
                 args: ['--debug'] }
    };

    const clientOptions: LanguageClientOptions = {
        // Activate for .fg files only
        documentSelector: [{ scheme: 'file', language: 'forge' }],
        synchronize: {
            // Re-analyze when .fg files change on disk
            fileEvents: vscode.workspace.createFileSystemWatcher('**/*.fg')
        },
        outputChannelName: 'FORGE Language Server'
    };

    client = new LanguageClient(
        'forge-language-server',
        'FORGE Language Server',
        serverOptions,
        clientOptions
    );

    client.start();
}

export function deactivate(): Thenable<void> | undefined {
    if (!client) return undefined;
    return client.stop();
}
```

### 8.2 Updated package.json for Part 2

The manifest gains a `main` entry point and an `activationEvents` list:

```jsonc
{
  "name": "forge-language",
  "version": "1.0.0",       // Major bump for LSP addition
  "main": "./client/out/extension.js",
  "activationEvents": [
    "onLanguage:forge"       // Activate when a .fg file is opened
  ],
  "dependencies": {
    "vscode-languageclient": "^9.0.0"
  },
  "devDependencies": {
    "@types/vscode": "^1.75.0",
    "typescript": "^5.0.0",
    "@vscode/vsce": "^2.0.0"
  },
  "scripts": {
    "compile": "tsc -p client/tsconfig.json",
    "watch":   "tsc -p client/tsconfig.json --watch",
    "package": "vsce package",
    "publish": "vsce publish"
  }
}
```

### 8.3 TypeScript Configuration

```jsonc
// client/tsconfig.json
{
  "compilerOptions": {
    "module": "commonjs",
    "target": "ES2020",
    "outDir": "../client/out",
    "rootDir": "src",
    "strict": true,
    "lib": ["ES2020"]
  },
  "exclude": ["node_modules"]
}
```

---

## 9. Phase 2E — Integration and Polish

### 9.1 Task 2E.1 — Performance Tuning

The analysis pipeline runs on every keystroke (with a small debounce). Profile under realistic conditions:

- A FORGE file with 200+ lines
- A workspace with 10 `.fg` files
- Analysis time per keystroke should be < 100ms for good UX

If analysis is too slow, add a debounce (wait 300ms after the last keystroke before re-analyzing):

```c
/* In the didChange handler: */
/* Instead of immediately re-analyzing, set a timer.
   If another change arrives before the timer fires, reset it. */
/* Implementation: use a background thread with a condition variable,
   or (simpler for v1): just analyze on save (didSave) rather than
   on every change (didChange). */
```

For v1, analyzing on `didSave` (rather than `didChange`) is a pragmatic starting point. Switch to `didChange` with debounce once the pipeline is proven fast enough.

### 9.2 Task 2E.2 — Multi-File Workspace Support

When a workspace contains multiple `.fg` files, the LSP must understand import relationships. When `main.fg` imports `sensors`, and `sensors.fg` is also in the workspace, go-to-definition from `main.fg` must resolve into `sensors.fg`.

Implementation:
1. On `initialize`, enumerate all `.fg` files in the workspace root
2. Pre-analyze all files to populate the symbol index
3. When a file changes, re-analyze it and update the symbol index for that file only
4. Cross-file resolution uses the symbol index (not re-parsing imports on demand)

### 9.3 Task 2E.3 — Error Logging and Diagnostics

The server logs to `/tmp/forge-lsp.log` during development. In production, log only warnings and errors. Provide a VS Code setting to enable verbose logging:

```jsonc
// In package.json contributes.configuration:
"forge.lsp.trace.server": {
  "type": "string",
  "enum": ["off", "messages", "verbose"],
  "default": "off",
  "description": "Enable LSP message tracing for debugging"
}
```

### 9.4 Task 2E.4 — Settings

Expose useful settings in VS Code:

```jsonc
"contributes": {
  "configuration": {
    "title": "FORGE Language",
    "properties": {
      "forge.lsp.enabled": {
        "type": "boolean",
        "default": true,
        "description": "Enable the FORGE Language Server for diagnostics and IntelliSense"
      },
      "forge.lsp.maxNumberOfProblems": {
        "type": "number",
        "default": 100,
        "description": "Maximum number of problems reported per file"
      },
      "forge.lsp.trace.server": {
        "type": "string",
        "enum": ["off", "messages", "verbose"],
        "default": "off",
        "description": "Trace LSP communication (for debugging)"
      }
    }
  }
}
```

### 9.5 Phase 2E Exit Criteria

- [ ] Analysis completes in < 100ms for typical FORGE files (or debounce is implemented)
- [ ] Workspace with 10+ `.fg` files initializes without errors
- [ ] Cross-file go-to-definition works reliably
- [ ] Server does not crash on malformed FORGE source
- [ ] Server recovers gracefully after a panic/crash and restarts
- [ ] All P1 features work after a VS Code window reload

---

## 10. LSP Feature Reference

Quick reference mapping FORGE language constructs to LSP features:

| FORGE Construct | Diagnostics | Hover | Definition | Completion | Symbol |
|----------------|-------------|-------|------------|------------|--------|
| `proc` declaration | ✓ | signature + doc | — | ✓ (definition) | ✓ |
| `proc` call | ✓ | signature | ✓ → decl | — | — |
| `record` declaration | ✓ | fields table | — | ✓ | ✓ |
| Record instantiation | ✓ | field types | ✓ → record decl | ✓ (fields) | — |
| `channel` declaration | ✓ | type | — | ✓ | ✓ |
| `emit` statement | ✓ (type check) | channel type | ✓ → channel decl | ✓ (channels) | — |
| `on` handler | ✓ | channel type | ✓ → channel decl | — | ✓ |
| `import` statement | ✓ (missing module) | — | ✓ → module file | ✓ (module names) | — |
| `var` declaration | ✓ | type | — | — | ✓ (local) |
| `const` declaration | ✓ | type + value | — | ✓ | ✓ |
| Module access `m.x` | ✓ | type of x | ✓ → x decl | ✓ (m's exports) | — |
| Built-in types | — | type description | — | ✓ | — |

---

## 11. Testing Strategy

### 11.1 Unit Tests — Server

Test each LSP handler in isolation using a test harness that sends JSON-RPC messages over pipes:

```c
/* tests/lsp/test_hover.c */
void test_hover_on_proc_name(void) {
    /* Set up: open a test document */
    send_lsp_request("textDocument/didOpen", make_did_open_params(
        "file:///test.fg",
        "proc add(a: int, b: int) -> int:\n    return a + b\n"
    ));

    /* Request hover at position of 'add' */
    cJSON* result = send_lsp_request("textDocument/hover",
        make_hover_params("file:///test.fg", 0, 5));  /* line 0, col 5 */

    /* Verify response */
    const char* value = get_hover_text(result);
    ASSERT(strstr(value, "proc add") != NULL, "hover should show proc signature");
    ASSERT(strstr(value, "int") != NULL, "hover should show types");
}
```

### 11.2 Integration Tests — VS Code

Use the VS Code Extension Test Runner (`@vscode/test-electron`) to run automated tests against a real VS Code instance:

```typescript
// client/src/test/hover.test.ts
test('Hover over proc name shows signature', async () => {
    const doc = await openTestDocument('sensors.fg');
    const hover = await vscode.commands.executeCommand<vscode.Hover[]>(
        'vscode.executeHoverProvider',
        doc.uri,
        new vscode.Position(5, 8)
    );
    assert(hover.length > 0);
    assert(hover[0].contents.some(c =>
        c.toString().includes('proc read')
    ));
});
```

### 11.3 Manual Testing Checklist

Before each release:

- [ ] Open a multi-file FORGE workspace
- [ ] Introduce a type error — squiggle appears in < 2 seconds
- [ ] Fix the error — squiggle disappears
- [ ] Hover over: proc name, record name, channel name, variable, module name
- [ ] Go-to-definition: local proc, imported proc, record field, channel
- [ ] Completions: after `sensors.`, after `var x:`, after `forge.io.`
- [ ] Outline panel shows all declarations
- [ ] Test on a file with syntax errors — server does not crash
- [ ] Close and reopen VS Code — server restarts cleanly

---

## 12. Build and Release Process

### 12.1 CI Pipeline (GitHub Actions)

```yaml
# .github/workflows/release.yml
name: Build and Release

on:
  push:
    tags: ['v*']

jobs:
  build-server:
    strategy:
      matrix:
        include:
          - os: ubuntu-latest
            target: linux-x64
          - os: macos-latest
            target: darwin-x64
          - os: macos-latest
            target: darwin-arm64
            arch: arm64
          - os: windows-latest
            target: win32-x64

    runs-on: ${{ matrix.os }}
    steps:
      - uses: actions/checkout@v4
      - name: Build forge-lsp
        run: make -C server
      - name: Upload binary
        uses: actions/upload-artifact@v4
        with:
          name: forge-lsp-${{ matrix.target }}
          path: server/forge-lsp${{ matrix.os == 'windows-latest' && '.exe' || '' }}

  package-extension:
    needs: build-server
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-node@v4
        with: { node-version: '20' }
      - run: npm install
      - run: npm run compile
      - name: Download all binaries
        uses: actions/download-artifact@v4
        with:
          path: bin/
      - run: npx vsce publish
        env:
          VSCE_PAT: ${{ secrets.VSCE_PAT }}
```

### 12.2 Version Alignment

| FORGE Toolchain | Extension Part 1 | Extension Part 2 |
|----------------|-----------------|-----------------|
| Phase 1 complete | `0.1.0` publish | — |
| Phase 3 complete | `0.2.0` (grammar refinements) | — |
| Phase 4 complete | `0.3.0` | LSP dev begins |
| Phase 5a complete | `0.4.0` | `1.0.0-beta` |
| FORGE v1.0 | `1.0.0` | `1.0.0` release |

---

## 13. Task Checklist

### Phase 2A — Server Foundation
- [ ] Set up `server/` directory with Makefile
- [ ] Download and integrate cJSON
- [ ] Implement `jsonrpc.c` — read/write framing
- [ ] Implement `document_store.c`
- [ ] Implement lifecycle handlers (initialize, shutdown, exit)
- [ ] Implement main server loop
- [ ] Test: server starts, handshakes with VS Code

### Phase 2B — Core Features
- [ ] Implement `symbol_index.c`
- [ ] Implement `analyse_document()` pipeline
- [ ] Implement diagnostic collection and `publishDiagnostics`
- [ ] Implement `handle_hover()` with all symbol types
- [ ] Implement `handle_definition()`
- [ ] Implement `handle_completion()` — module member context
- [ ] Implement `handle_completion()` — general context
- [ ] Write TypeScript client (`extension.ts`)
- [ ] Update `package.json` for Part 2
- [ ] End-to-end test: squiggles, hover, F12, completions all working

### Phase 2C — Advanced Features
- [ ] Implement `handle_document_symbols()`
- [ ] Implement `handle_references()`
- [ ] Implement `handle_rename()`
- [ ] Implement `handle_signature_help()`
- [ ] Implement semantic tokens (optional)

### Phase 2D–2E — Integration and Release
- [ ] Multi-file workspace support
- [ ] Performance profiling and debounce
- [ ] Settings contribution
- [ ] GitHub Actions CI pipeline
- [ ] Linux + macOS binaries building cleanly
- [ ] Windows binary (optional for v1)
- [ ] Publish `1.0.0` to VS Code Marketplace
- [ ] Update marketplace README with LSP features

---

## Appendix A — LSP Message Reference

Key LSP methods implemented by `forge-lsp`:

| Method | Direction | Description |
|--------|-----------|-------------|
| `initialize` | client → server | Capability negotiation |
| `initialized` | client → server | Init complete notification |
| `shutdown` | client → server | Request clean shutdown |
| `exit` | client → server | Terminate process |
| `textDocument/didOpen` | client → server | File opened |
| `textDocument/didChange` | client → server | File content changed |
| `textDocument/didSave` | client → server | File saved |
| `textDocument/didClose` | client → server | File closed |
| `textDocument/publishDiagnostics` | server → client | Error/warning list |
| `textDocument/hover` | client → server | Hover info request |
| `textDocument/definition` | client → server | Go-to-definition |
| `textDocument/completion` | client → server | Completion list request |
| `textDocument/documentSymbol` | client → server | File outline |
| `textDocument/references` | client → server | Find all references |
| `textDocument/rename` | client → server | Rename symbol |
| `textDocument/signatureHelp` | client → server | Parameter hints |
| `textDocument/semanticTokens/full` | client → server | Semantic coloring |

---

## Appendix B — Server Stdio Protocol

The LSP base protocol framing used by `forge-lsp`:

```
Message format:
  Content-Length: <N>\r\n
  \r\n
  <N bytes of UTF-8 JSON>

Request:
  { "jsonrpc": "2.0", "id": <int>, "method": "<name>", "params": {...} }

Notification (no response expected):
  { "jsonrpc": "2.0", "method": "<name>", "params": {...} }

Response (success):
  { "jsonrpc": "2.0", "id": <int>, "result": <value> }

Response (error):
  { "jsonrpc": "2.0", "id": <int>, "error": { "code": <int>, "message": "<str>" } }

Standard error codes:
  -32700  Parse error
  -32600  Invalid request
  -32601  Method not found
  -32602  Invalid params
  -32603  Internal error
```

---

## Appendix C — Capability Declaration Reference

The `initialize` response capabilities object for full P1+P2 feature set:

```jsonc
{
  "capabilities": {
    "textDocumentSync": {
      "openClose": true,
      "change": 1,            // 1=Full, 2=Incremental
      "save": { "includeText": false }
    },
    "hoverProvider": true,
    "definitionProvider": true,
    "referencesProvider": true,
    "documentSymbolProvider": true,
    "renameProvider": true,
    "completionProvider": {
      "triggerCharacters": [".", ":"],
      "resolveProvider": false
    },
    "signatureHelpProvider": {
      "triggerCharacters": ["(", ","]
    },
    "semanticTokensProvider": {
      "legend": {
        "tokenTypes": [
          "namespace", "type", "function", "variable",
          "parameter", "keyword", "comment", "string",
          "number", "operator"
        ],
        "tokenModifiers": [
          "declaration", "definition", "readonly",
          "deprecated", "modification"
        ]
      },
      "full": true
    }
  }
}
```

---

*FORGE VS Code Extension — Part 2 Plan v0.1 — Fragillidae Software*  
*Previous: `FORGE_VSCode_Extension_Part1.md`*  
*See also: `FORGE_Implementation_Plan.md` Phase 4 (Type Checker) — prerequisite for Part 2*
