# FORGE VS Code Extension — Part 1
## TextMate Grammar and Language Configuration

**Document:** Development Plan  
**Version:** 0.1  
**Status:** Active  
**Companion Documents:** `FORGE_Language_Spec.md` v0.1, `FORGE_Implementation_Plan.md` v0.1  
**Author:** Fragillidae Software  
**Prerequisite:** FORGE Lexer (Phase 1) stable  
**Estimated Effort:** 2–4 days for full Part 1 completion

---

> This document covers the first phase of FORGE's VS Code integration: a purely declarative extension requiring no compiled toolchain components. It can be developed in parallel with the FORGE interpreter (Phase 3) and published to the VS Code Marketplace before the language is feature-complete. No TypeScript build pipeline is required for Part 1.

---

## Table of Contents

1. [Overview and Goals](#1-overview-and-goals)
2. [Repository Structure](#2-repository-structure)
3. [Extension Manifest](#3-extension-manifest)
4. [Language Configuration](#4-language-configuration)
5. [TextMate Grammar](#5-textmate-grammar)
6. [Snippet Definitions](#6-snippet-definitions)
7. [Theme Color Recommendations](#7-theme-color-recommendations)
8. [Icon and Branding](#8-icon-and-branding)
9. [Testing the Extension Locally](#9-testing-the-extension-locally)
10. [Publishing to the VS Code Marketplace](#10-publishing-to-the-vs-code-marketplace)
11. [Maintenance and Evolution](#11-maintenance-and-evolution)
12. [Task Checklist](#12-task-checklist)
13. [Appendix A — TextMate Scope Reference](#appendix-a--textmate-scope-reference)
14. [Appendix B — Complete Grammar Skeleton](#appendix-b--complete-grammar-skeleton)

---

## 1. Overview and Goals

### 1.1 What Part 1 Delivers

| Feature | Mechanism | Effort |
|---------|-----------|--------|
| Syntax highlighting | TextMate grammar (JSON) | Medium |
| Bracket matching | Language configuration (JSON) | Trivial |
| Auto-closing pairs | Language configuration (JSON) | Trivial |
| Comment toggling (`Ctrl+/`) | Language configuration (JSON) | Trivial |
| Smart indentation after `:` | Language configuration (JSON) | Low |
| Code folding | Language configuration (JSON) | Low |
| Code snippets | Snippet file (JSON) | Low |
| File icon association | Extension manifest (JSON) | Trivial |

### 1.2 What Part 1 Does Not Deliver

These are Part 2 (LSP) deliverables:

- Go-to-definition
- Hover type information
- Inline error diagnostics
- Auto-complete
- Rename symbol
- Find all references

### 1.3 Strategic Rationale

Part 1 requires zero toolchain integration. It is a collection of JSON files packaged as a VS Code extension. The grammar file is a direct translation of the FORGE lexer's token rules into regex patterns. Once the lexer token set is stable (Phase 1 of the toolchain plan), the grammar file can be written definitively.

Publish Part 1 as soon as the lexer is stable. Mark the extension version `0.1.x`. Begin iterating on Part 2 after toolchain Phase 4 (type checker) is complete.

### 1.4 Naming

| Item | Value |
|------|-------|
| Extension name | `forge-language` |
| Display name | `FORGE Language` |
| Publisher ID | `fragillidae` (register at marketplace.visualstudio.com) |
| Extension ID | `fragillidae.forge-language` |
| File extension | `.fg` |
| Language ID | `forge` (used internally by VS Code) |
| Marketplace URL | `marketplace.visualstudio.com/items?itemName=fragillidae.forge-language` |

---

## 2. Repository Structure

The extension lives in its own repository, separate from the FORGE toolchain.

```
forge-vscode/
├── package.json                    # Extension manifest — the heart of the extension
├── package-lock.json
├── README.md                       # Marketplace listing description
├── CHANGELOG.md
├── LICENSE
├── .vscodeignore                   # Files to exclude from published package
│
├── syntaxes/
│   └── forge.tmLanguage.json       # TextMate grammar
│
├── language-configuration.json     # Bracket matching, indentation, comments
│
├── snippets/
│   └── forge.code-snippets         # Code snippet definitions
│
├── icons/
│   └── forge-file-icon.png         # File icon for .fg files (optional)
│
└── themes/                         # Optional: semantic token color suggestions
    └── forge-color-theme.json
```

### 2.1 No Build Step Required

Part 1 has no TypeScript, no webpack, no compilation. Every file is static JSON (or JSON with comments — JSONC). The `node_modules` directory is needed only for `vsce` (the VS Code extension publishing tool) and is not bundled into the published extension.

---

## 3. Extension Manifest

`package.json` is the extension manifest. It declares everything VS Code needs to know about the extension.

```jsonc
// package.json
{
  "name": "forge-language",
  "displayName": "FORGE Language",
  "description": "Syntax highlighting, language configuration, and snippets for the FORGE programming language (.fg files)",
  "version": "0.1.0",
  "publisher": "fragillidae",
  "author": {
    "name": "Fragillidae Software"
  },
  "license": "MIT",
  "homepage": "https://fragillidaesoftware.com",
  "repository": {
    "type": "git",
    "url": "https://github.com/fragillidae/forge-vscode"
  },
  "bugs": {
    "url": "https://github.com/fragillidae/forge-vscode/issues"
  },
  "engines": {
    "vscode": "^1.75.0"
  },
  "categories": [
    "Programming Languages",
    "Snippets"
  ],
  "keywords": [
    "forge", "fg", "fragillidae", "procedural", "embedded",
    "marine", "event-driven", "educational"
  ],
  "icon": "icons/forge-file-icon.png",

  "contributes": {

    // ── Language registration ──────────────────────────
    "languages": [
      {
        "id": "forge",
        "aliases": ["FORGE", "forge", "Forge"],
        "extensions": [".fg"],
        "configuration": "./language-configuration.json",
        "icon": {
          "light": "./icons/forge-file-icon.png",
          "dark":  "./icons/forge-file-icon.png"
        }
      }
    ],

    // ── Grammar ───────────────────────────────────────
    "grammars": [
      {
        "language": "forge",
        "scopeName": "source.forge",
        "path": "./syntaxes/forge.tmLanguage.json"
      }
    ],

    // ── Snippets ──────────────────────────────────────
    "snippets": [
      {
        "language": "forge",
        "path": "./snippets/forge.code-snippets"
      }
    ]
  }
}
```

### 3.1 Engine Version

`"vscode": "^1.75.0"` targets VS Code 1.75 and above. This covers all VS Code releases from early 2023 onward, which is the vast majority of active installations. Do not target a version newer than necessary — it reduces the potential user base.

### 3.2 Categories

`"Programming Languages"` is required to appear in the Languages section of the marketplace. `"Snippets"` is appropriate given the snippet file.

---

## 4. Language Configuration

`language-configuration.json` controls editor behavior for `.fg` files that is purely syntactic — no semantic analysis required.

```jsonc
// language-configuration.json
{
  // ── Comments ───────────────────────────────────────────
  "comments": {
    "lineComment": "#"
    // No block comment — FORGE uses consecutive # lines
  },

  // ── Bracket pairs ──────────────────────────────────────
  // VS Code highlights matching brackets and indents correctly
  "brackets": [
    ["(", ")"],
    ["[", "]"],
    ["{", "}"]
  ],

  // ── Auto-closing pairs ─────────────────────────────────
  // Typing the opening character inserts the closing character
  "autoClosingPairs": [
    { "open": "(", "close": ")" },
    { "open": "[", "close": "]" },
    { "open": "{", "close": "}" },
    { "open": "\"", "close": "\"", "notIn": ["string", "comment"] },
    { "open": "`", "close": "`",   "notIn": ["string", "comment"] }
  ],

  // ── Auto-surrounding ───────────────────────────────────
  // Selecting text and typing an opening char wraps the selection
  "surroundingPairs": [
    ["(", ")"],
    ["[", "]"],
    ["{", "}"],
    ["\"", "\""],
    ["`", "`"]
  ],

  // ── Indentation rules ──────────────────────────────────
  // After a line ending with ':', auto-indent the next line.
  // After a blank line or a dedented line, remove indent.
  "indentationRules": {
    "increaseIndentPattern": "^((?!#).)*:\\s*(#.*)?$",
    "decreaseIndentPattern": "^\\s*(else|elif)\\b.*:\\s*(#.*)?$"
  },

  // ── On-enter rules ─────────────────────────────────────
  // Fine-grained control over what happens when Enter is pressed.
  "onEnterRules": [
    {
      // After a line ending with ':', indent the next line
      "beforeText": "^((?!#).)*:\\s*(#.*)?$",
      "action": {
        "indent": "indent"
      }
    },
    {
      // After a comment line starting with '#', continue with '#'
      "beforeText": "^\\s*#",
      "action": {
        "indent": "none",
        "appendText": "# "
      }
    }
  ],

  // ── Word pattern ───────────────────────────────────────
  // What counts as a "word" for double-click selection and
  // word-based completion. FORGE identifiers: letters, digits, underscore.
  "wordPattern": "[a-zA-Z_][a-zA-Z0-9_]*",

  // ── Folding ────────────────────────────────────────────
  // FORGE uses indentation for blocks, so use indentation-based folding.
  // VS Code handles this automatically for indentation-based languages
  // when no explicit fold markers are defined.
  "folding": {
    "offSide": true
  }
}
```

### 4.1 Key Decisions

**`"offSide": true`** is the critical setting for indentation-based languages. It tells VS Code to use indentation levels for code folding rather than bracket matching. Python uses this same setting. Without it, folding will not work correctly for FORGE.

**`increaseIndentPattern`** matches any line ending with `:` that is not a comment. This covers all FORGE block-opening lines: `proc`, `if`, `elif`, `else`, `while`, `for`, `loop`, `record`, `on`, `with`.

**`decreaseIndentPattern`** matches `else:` and `elif ...:` lines so VS Code automatically dedents them to align with their corresponding `if`.

---

## 5. TextMate Grammar

The grammar file is the most substantial Part 1 artifact. It defines regex-based rules that VS Code uses to tokenize FORGE source and assign semantic scope names to each token. Those scope names map to colors in the active color theme.

### 5.1 Grammar File Structure

```jsonc
// syntaxes/forge.tmLanguage.json
{
  "$schema": "https://raw.githubusercontent.com/martinring/tmlanguage/master/tmlanguage.json",
  "name": "FORGE",
  "scopeName": "source.forge",
  "fileTypes": ["fg"],
  "patterns": [
    { "include": "#comment" },
    { "include": "#string_double" },
    { "include": "#string_raw" },
    { "include": "#keyword_control" },
    { "include": "#keyword_declaration" },
    { "include": "#keyword_operator" },
    { "include": "#keyword_channel" },
    { "include": "#type_builtin" },
    { "include": "#type_named" },
    { "include": "#constant_language" },
    { "include": "#constant_numeric" },
    { "include": "#function_call" },
    { "include": "#function_definition" },
    { "include": "#variable_module" },
    { "include": "#operator" },
    { "include": "#punctuation" }
  ],
  "repository": {
    // ... all rule definitions here ...
  }
}
```

### 5.2 Rule Definitions

Each entry in `"repository"` is a named rule. Rules can reference each other via `{ "include": "#rule_name" }`.

#### 5.2.1 Comments

```jsonc
"comment": {
  "name": "comment.line.number-sign.forge",
  "match": "#.*$"
}
```

This matches `#` through end of line. The scope `comment.line.number-sign` is the standard TextMate scope for `#`-style comments. All color themes that support comments will color this correctly.

#### 5.2.2 String Literals

```jsonc
"string_double": {
  "name": "string.quoted.double.forge",
  "begin": "\"",
  "end": "\"",
  "patterns": [
    {
      "name": "constant.character.escape.forge",
      "match": "\\\\(?:[ntr\\\\\"0]|x[0-9a-fA-F]{2})"
    },
    {
      "name": "invalid.illegal.escape.forge",
      "match": "\\\\."
    }
  ]
},

"string_raw": {
  "name": "string.quoted.other.forge",
  "begin": "`",
  "end": "`"
  // No escape sequences in raw strings
}
```

The `begin`/`end` pattern handles multi-token spans. The nested `patterns` array matches escape sequences inside the string. Valid escapes (`\n`, `\t`, `\r`, `\\`, `\"`, `\0`, `\xNN`) get `constant.character.escape` scope (typically colored distinctly within the string). Invalid escapes get `invalid.illegal` scope (typically underlined in red).

#### 5.2.3 Control Flow Keywords

```jsonc
"keyword_control": {
  "name": "keyword.control.forge",
  "match": "\\b(if|elif|else|while|for|loop|break|continue|return|in|with)\\b"
}
```

`\b` is a word boundary anchor. This prevents matching `format` as containing `for`. The scope `keyword.control` is universally understood by color themes and will render in the theme's keyword color.

#### 5.2.4 Declaration Keywords

```jsonc
"keyword_declaration": {
  "name": "keyword.declaration.forge",
  "match": "\\b(proc|record|var|const|type|import|export|channel|on)\\b"
}
```

Some themes distinguish `keyword.control` from `keyword.declaration`. Those that don't will still color both. Using distinct scopes gives theme authors more control and gives you a hook for semantic theming later.

#### 5.2.5 Operator Keywords

```jsonc
"keyword_operator": {
  "name": "keyword.operator.word.forge",
  "match": "\\b(and|or|not|is|as|ref|emit|free|alloc|some|panic|assert|range)\\b"
}
```

`and`, `or`, `not` are logical operators. `is`, `as`, `ref` are structural keywords. `emit`, `alloc`, `free`, `some`, `panic`, `assert` are statement/expression keywords. Grouping them as `keyword.operator.word` is semantically accurate — they are operators that happen to be words.

#### 5.2.6 Optional Keywords

```jsonc
"keyword_optional": {
  "name": "keyword.other.forge",
  "match": "\\bor_else\\b"
}
```

`or_else` is a two-word keyword. Handle it separately to avoid partial matches.

#### 5.2.7 Built-in Types

```jsonc
"type_builtin": {
  "name": "support.type.primitive.forge",
  "match": "\\b(int|int8|int16|int32|uint|uint8|uint16|uint32|float|float32|bool|str|byte|void|map)\\b"
}
```

`support.type.primitive` is the standard scope for built-in language types. Most themes render this distinctly from user-defined types.

#### 5.2.8 Named Types (User-Defined Records)

```jsonc
"type_named": {
  "name": "entity.name.type.forge",
  "match": "(?<=[:\\s])\\b([A-Z][a-zA-Z0-9_]*)\\b"
}
```

FORGE convention (though not enforced by the language) is that record names are `PascalCase`. This rule matches an identifier starting with an uppercase letter when preceded by `:` or whitespace — the context where a type name appears. This catches `Sensor`, `Position`, `NavData` etc.

Note: this is a heuristic. The LSP (Part 2) will provide accurate type name detection. For now, uppercase-initial identifiers in type position work well in practice.

#### 5.2.9 Language Constants

```jsonc
"constant_language": {
  "name": "constant.language.forge",
  "match": "\\b(true|false|none)\\b"
}
```

`true`, `false`, and `none` are the three language-level constant values.

#### 5.2.10 Numeric Literals

```jsonc
"constant_numeric": {
  "patterns": [
    {
      "comment": "Hexadecimal: 0xFF, 0xFF_A0",
      "name": "constant.numeric.hex.forge",
      "match": "\\b0x[0-9a-fA-F][0-9a-fA-F_]*\\b"
    },
    {
      "comment": "Binary: 0b1010, 0b1010_0011",
      "name": "constant.numeric.binary.forge",
      "match": "\\b0b[01][01_]*\\b"
    },
    {
      "comment": "Octal: 0o755",
      "name": "constant.numeric.octal.forge",
      "match": "\\b0o[0-7][0-7_]*\\b"
    },
    {
      "comment": "Float: 3.14, 2.998e8, 1.0e-9",
      "name": "constant.numeric.float.forge",
      "match": "\\b[0-9][0-9_]*\\.[0-9][0-9_]*(?:[eE][+-]?[0-9]+)?\\b"
    },
    {
      "comment": "Float with exponent only: 2e8",
      "name": "constant.numeric.float.forge",
      "match": "\\b[0-9][0-9_]*[eE][+-]?[0-9]+\\b"
    },
    {
      "comment": "Decimal integer: 42, 1_000_000",
      "name": "constant.numeric.integer.forge",
      "match": "\\b[0-9][0-9_]*\\b"
    }
  ]
}
```

Order matters in TextMate grammars: more specific rules must come before more general ones. Hex/binary/octal before decimal. Float before integer. The `[0-9_]*` pattern handles underscore separators correctly.

#### 5.2.11 Procedure Calls

```jsonc
"function_call": {
  "name": "entity.name.function.forge",
  "match": "\\b([a-z_][a-zA-Z0-9_]*)(?=\\s*\\()"
}
```

Matches an identifier immediately followed by `(`. The lookahead `(?=\\s*\\()` does not consume the `(`. This highlights function call sites. Note: this will also match built-in function calls like `len(`, `append(`, `alloc(` — which is correct and desirable.

#### 5.2.12 Procedure Definitions

```jsonc
"function_definition": {
  "match": "\\b(proc)\\s+([a-z_][a-zA-Z0-9_]*)",
  "captures": {
    "1": { "name": "keyword.declaration.forge" },
    "2": { "name": "entity.name.function.definition.forge" }
  }
}
```

A `captures` rule matches multiple groups in one pattern and assigns different scopes to each. This matches `proc my_function` and scopes `proc` as a keyword and `my_function` as a function definition name. Most themes render definition names more prominently than call sites.

#### 5.2.13 Record Definitions

```jsonc
"record_definition": {
  "match": "\\b(record)\\s+([A-Z][a-zA-Z0-9_]*)",
  "captures": {
    "1": { "name": "keyword.declaration.forge" },
    "2": { "name": "entity.name.type.definition.forge" }
  }
}
```

Matches `record Sensor` and scopes the name as a type definition.

#### 5.2.14 Channel Definitions and Events

```jsonc
"channel_declaration": {
  "match": "\\b(channel)\\s+([a-z_][a-zA-Z0-9_]*)",
  "captures": {
    "1": { "name": "keyword.declaration.forge" },
    "2": { "name": "entity.name.other.channel.forge" }
  }
},

"emit_statement": {
  "match": "\\b(emit)\\s+([a-zA-Z_][a-zA-Z0-9_.]*)",
  "captures": {
    "1": { "name": "keyword.operator.word.forge" },
    "2": { "name": "entity.name.other.channel.forge" }
  }
},

"on_handler": {
  "match": "\\b(on)\\s+([a-zA-Z_][a-zA-Z0-9_.]*)",
  "captures": {
    "1": { "name": "keyword.declaration.forge" },
    "2": { "name": "entity.name.other.channel.forge" }
  }
}
```

Channel names get their own scope `entity.name.other.channel`. Themes that don't define this scope will fall back to `entity.name.other`, which is usually a distinct color. This makes channels visually identifiable throughout the file.

#### 5.2.15 Module References

```jsonc
"variable_module": {
  "match": "\\b([a-z_][a-zA-Z0-9_]*)(?=\\.)",
  "name": "entity.name.namespace.forge"
}
```

Matches the module name part of a qualified reference (`sensors.read`, `forge.io.print`). The lookahead `(?=\\.)` ensures only the module/namespace prefix is colored, not the member name. `entity.name.namespace` is the standard scope for module/namespace identifiers.

#### 5.2.16 Operators

```jsonc
"operator": {
  "patterns": [
    {
      "name": "keyword.operator.arrow.forge",
      "match": "->"
    },
    {
      "name": "keyword.operator.range.forge",
      "match": "\\.\\.|\\.\\.="
    },
    {
      "name": "keyword.operator.assignment.compound.forge",
      "match": "[+\\-*/%&|^]=|<<=|>>="
    },
    {
      "name": "keyword.operator.comparison.forge",
      "match": "==|!=|<=|>=|<|>"
    },
    {
      "name": "keyword.operator.bitwise.forge",
      "match": "<<|>>|[&|^~]"
    },
    {
      "name": "keyword.operator.arithmetic.forge",
      "match": "[+\\-*/%]"
    },
    {
      "name": "keyword.operator.assignment.forge",
      "match": "="
    }
  ]
}
```

Order is critical here too: `==` must be matched before `=`, `<=` before `<`, `<<=` before `<<` before `<`. More specific patterns first.

#### 5.2.17 Punctuation

```jsonc
"punctuation": {
  "patterns": [
    {
      "name": "punctuation.definition.parameters.forge",
      "match": "[()]"
    },
    {
      "name": "punctuation.definition.array.forge",
      "match": "[\\[\\]]"
    },
    {
      "name": "punctuation.definition.block.forge",
      "match": "[{}]"
    },
    {
      "name": "punctuation.separator.comma.forge",
      "match": ","
    },
    {
      "name": "punctuation.separator.colon.forge",
      "match": ":"
    },
    {
      "name": "punctuation.definition.optional.forge",
      "match": "\\?"
    }
  ]
}
```

### 5.3 Grammar Rule Order in `patterns`

The top-level `patterns` array in the grammar is processed in order. Later rules do not override earlier ones for the same text position. The recommended order:

```
1. comment           — must be first; # inside strings is already handled by string rules
2. string_double     — strings must come before keywords (keywords inside strings are not keywords)
3. string_raw
4. function_definition  — proc name: capture before generic keyword rule fires
5. record_definition    — record name: same reason
6. channel_declaration
7. emit_statement
8. on_handler
9. keyword_control
10. keyword_declaration
11. keyword_operator
12. keyword_optional
13. type_builtin
14. type_named
15. constant_language
16. constant_numeric
17. function_call     — after keywords so keywords followed by ( don't double-match
18. variable_module
19. operator
20. punctuation
```

---

## 6. Snippet Definitions

Snippets accelerate development for new FORGE programmers. Tab stops (`$1`, `$2`) define cursor positions. `$0` is the final cursor position. `${1:placeholder}` shows placeholder text.

```jsonc
// snippets/forge.code-snippets
{
  // ── Declarations ─────────────────────────────────────
  "Procedure": {
    "prefix": "proc",
    "body": [
      "proc ${1:name}(${2:param}: ${3:type}) -> ${4:void}:",
      "\t${0:pass}"
    ],
    "description": "Define a FORGE procedure"
  },

  "Procedure (no params)": {
    "prefix": "proc0",
    "body": [
      "proc ${1:name}() -> ${2:void}:",
      "\t${0:pass}"
    ],
    "description": "Define a FORGE procedure with no parameters"
  },

  "Main procedure": {
    "prefix": "main",
    "body": [
      "proc main() -> void:",
      "\t${0}"
    ],
    "description": "Define the main entry point"
  },

  "Record": {
    "prefix": "record",
    "body": [
      "record ${1:Name}:",
      "\t${2:field}: ${3:type}",
      "\t${0}"
    ],
    "description": "Define a FORGE record"
  },

  "Channel declaration": {
    "prefix": "channel",
    "body": [
      "channel ${1:name}: ${2:type}"
    ],
    "description": "Declare a FORGE channel"
  },

  "On handler": {
    "prefix": "on",
    "body": [
      "on ${1:channel_name} as ${2:value}:",
      "\t${0}"
    ],
    "description": "Register a channel handler"
  },

  "On handler (void channel)": {
    "prefix": "onv",
    "body": [
      "on ${1:channel_name}:",
      "\t${0}"
    ],
    "description": "Register a handler for a void channel"
  },

  "Emit (with payload)": {
    "prefix": "emit",
    "body": [
      "emit ${1:channel_name} -> ${2:value}"
    ],
    "description": "Emit a message on a channel"
  },

  "Emit (void)": {
    "prefix": "emitv",
    "body": [
      "emit ${1:channel_name}"
    ],
    "description": "Emit a void channel signal"
  },

  // ── Variables ─────────────────────────────────────────
  "Variable declaration": {
    "prefix": "var",
    "body": [
      "var ${1:name}: ${2:type} = ${3:value}"
    ],
    "description": "Declare a variable"
  },

  "Constant declaration": {
    "prefix": "const",
    "body": [
      "const ${1:NAME}: ${2:type} = ${3:value}"
    ],
    "description": "Declare a constant"
  },

  // ── Control flow ──────────────────────────────────────
  "If statement": {
    "prefix": "if",
    "body": [
      "if ${1:condition}:",
      "\t${0}"
    ],
    "description": "If statement"
  },

  "If / else": {
    "prefix": "ife",
    "body": [
      "if ${1:condition}:",
      "\t${2}",
      "else:",
      "\t${0}"
    ],
    "description": "If / else statement"
  },

  "If / elif / else": {
    "prefix": "ifee",
    "body": [
      "if ${1:condition}:",
      "\t${2}",
      "elif ${3:condition}:",
      "\t${4}",
      "else:",
      "\t${0}"
    ],
    "description": "If / elif / else statement"
  },

  "While loop": {
    "prefix": "while",
    "body": [
      "while ${1:condition}:",
      "\t${0}"
    ],
    "description": "While loop"
  },

  "For range loop": {
    "prefix": "for",
    "body": [
      "for ${1:i} in range(${2:0}, ${3:n}):",
      "\t${0}"
    ],
    "description": "For loop over a range"
  },

  "For collection loop": {
    "prefix": "forc",
    "body": [
      "for ${1:item} in ${2:collection}:",
      "\t${0}"
    ],
    "description": "For loop over a collection"
  },

  "Infinite loop": {
    "prefix": "loop",
    "body": [
      "loop:",
      "\t${0}"
    ],
    "description": "Infinite loop"
  },

  // ── Imports ───────────────────────────────────────────
  "Import module": {
    "prefix": "import",
    "body": [
      "import ${1:module_name}"
    ],
    "description": "Import a module"
  },

  "Import with alias": {
    "prefix": "importa",
    "body": [
      "import ${1:module_name} as ${2:alias}"
    ],
    "description": "Import a module with an alias"
  },

  "Import forge.io": {
    "prefix": "impio",
    "body": [
      "import forge.io"
    ],
    "description": "Import the forge.io standard library module"
  },

  // ── Memory ────────────────────────────────────────────
  "With alloc": {
    "prefix": "with",
    "body": [
      "with alloc(${1:[]byte}, ${2:256}) as ${3:buf}:",
      "\t${0}"
    ],
    "description": "Scope-managed heap allocation"
  },

  // ── Optional handling ─────────────────────────────────
  "Optional check": {
    "prefix": "issome",
    "body": [
      "if ${1:value} is some:",
      "\t${2:${1}.value}",
      "else:",
      "\t${0}"
    ],
    "description": "Check an optional value"
  },

  // ── Common patterns ───────────────────────────────────
  "Module init": {
    "prefix": "init",
    "body": [
      "proc init() -> void:",
      "\t${0}"
    ],
    "description": "Module initialization procedure"
  },

  "Export procedure": {
    "prefix": "eproc",
    "body": [
      "export proc ${1:name}(${2:param}: ${3:type}) -> ${4:void}:",
      "\t${0}"
    ],
    "description": "Define an exported procedure"
  },

  "Export record": {
    "prefix": "erecord",
    "body": [
      "export record ${1:Name}:",
      "\t${2:field}: ${3:type}",
      "\t${0}"
    ],
    "description": "Define an exported record"
  },

  "Export channel": {
    "prefix": "echannel",
    "body": [
      "export channel ${1:name}: ${2:type}"
    ],
    "description": "Declare an exported channel"
  },

  // ── File templates ────────────────────────────────────
  "New FORGE module": {
    "prefix": "newmodule",
    "body": [
      "# ${1:module_name}.fg",
      "# ${2:Brief description of this module}",
      "",
      "import forge.io",
      "",
      "${0}"
    ],
    "description": "New FORGE module template"
  },

  "New FORGE main": {
    "prefix": "newmain",
    "body": [
      "# main.fg",
      "# ${1:Brief description of this program}",
      "",
      "import forge.io",
      "",
      "proc main() -> void:",
      "\t${0}"
    ],
    "description": "New FORGE main module template"
  }
}
```

---

## 7. Theme Color Recommendations

TextMate grammars assign scope names. Color themes map scope names to colors. FORGE uses standard scope names wherever possible so it works well out of the box with any theme. However, a custom semantic token color configuration can provide richer coloring in VS Code's "semantic highlighting" mode.

### 7.1 Recommended Scope Mappings

These recommendations can be included in a bundled color theme or documented for users who want to customize their theme's `settings.json`:

```jsonc
// Suggested additions to any theme's tokenColors for best FORGE rendering
[
  {
    "scope": "entity.name.other.channel.forge",
    "settings": {
      "foreground": "#C09050",   // Amber — channels are visually distinct
      "fontStyle": "italic"
    }
  },
  {
    "scope": "keyword.operator.word.forge",
    "settings": {
      "foreground": "#569CD6",   // Blue — word operators
      "fontStyle": "bold"
    }
  },
  {
    "scope": "entity.name.namespace.forge",
    "settings": {
      "foreground": "#9CDCFE",   // Light blue — module names
      "fontStyle": ""
    }
  }
]
```

### 7.2 Alignment with Existing Themes

The scope names chosen map cleanly to the following popular themes without any additional configuration:

| Scope | Dark+ | One Dark Pro | Dracula | GitHub Dark |
|-------|-------|--------------|---------|-------------|
| `keyword.control` | Blue | Purple | Pink | Red |
| `keyword.declaration` | Blue | Purple | Pink | Red |
| `entity.name.function` | Yellow | Yellow | Green | Purple |
| `entity.name.type` | Green | Teal | Cyan | Orange |
| `constant.language` | Blue | Orange | Purple | Blue |
| `constant.numeric` | Green | Orange | Purple | Blue |
| `string.quoted.double` | Orange | Green | Yellow | Blue |
| `comment.line` | Green | Gray | Gray | Gray |
| `support.type.primitive` | Blue | Cyan | Cyan | Red |

---

## 8. Icon and Branding

### 8.1 File Icon

Create a `forge-file-icon.png` at 128×128 pixels. Suggested design aligned with the FORGE identity:

- Dark navy (`#1B3A5C`) background with rounded corners
- The letters `fg` or `.fg` in a clean monospace font
- An amber (`#C87800`) accent — anvil silhouette, flame, or forge imagery
- This ties visually to the amber/blue color scheme used in your existing tool suite

The icon appears in the VS Code file explorer next to `.fg` files and in the Marketplace listing thumbnail.

### 8.2 Marketplace README

The `README.md` is the extension's marketplace listing page. It should include:

```markdown
# FORGE Language Support for VS Code

Syntax highlighting, language configuration, and code snippets for the
[FORGE programming language](https://fragillidaesoftware.com) (`.fg` files).

## Features

- Full syntax highlighting for all FORGE constructs
- Smart indentation — auto-indents after `:` block headers
- Bracket matching and auto-closing pairs
- Comment toggling with `Ctrl+/` / `Cmd+/`
- Code folding based on indentation
- 30+ code snippets for common FORGE patterns
- File icon for `.fg` files

## What is FORGE?

FORGE (Fast, Reliable, Organized, General-purpose, Event-driven) is a
structured, procedural, modular, event-driven programming language...

## Snippets

| Prefix | Description |
|--------|-------------|
| `proc` | Procedure definition |
| `record` | Record definition |
| `channel` | Channel declaration |
| `on` | Channel handler |
| `emit` | Emit channel message |
| ...    | ...           |

## Language Server (Coming Soon)

A full Language Server Protocol implementation is in development and will
add go-to-definition, hover types, inline diagnostics, and auto-completion.
```

---

## 9. Testing the Extension Locally

### 9.1 Prerequisites

```bash
npm install -g @vscode/vsce    # VS Code extension packaging tool
```

### 9.2 Development Workflow

**Step 1 — Open the extension folder in VS Code:**
```bash
code forge-vscode/
```

**Step 2 — Launch the Extension Development Host:**
Press `F5` in VS Code. This opens a second VS Code window with the extension loaded. Open any `.fg` file in that window to test highlighting and language configuration.

**Step 3 — Reload after changes:**
Press `Ctrl+Shift+P` → "Developer: Reload Window" in the Extension Development Host to pick up grammar changes without restarting.

### 9.3 Grammar Testing Checklist

Open a test `.fg` file containing all language constructs and visually verify:

- [ ] `#` comments are colored as comments
- [ ] String content is colored as strings
- [ ] Escape sequences inside strings are colored distinctly
- [ ] All keywords are colored (not plain text)
- [ ] `true`, `false`, `none` are colored as constants
- [ ] Integer and float literals are colored as numbers
- [ ] Hex/binary/octal literals are colored as numbers
- [ ] Procedure names after `proc` are colored as function definitions
- [ ] Record names after `record` are colored as type definitions
- [ ] Channel names after `channel`, `emit`, `on` are colored distinctly
- [ ] Module names before `.` are colored as namespaces
- [ ] Built-in types (`int`, `float`, `bool`, etc.) are colored as types
- [ ] `->` arrow operator is colored as an operator
- [ ] Brackets are colored and matched correctly
- [ ] Pressing `Enter` after a `:` line auto-indents
- [ ] `Ctrl+/` toggles `#` comments
- [ ] Code folding works at `proc`, `record`, `if`, `for` blocks

### 9.4 Scope Inspector

Use VS Code's built-in scope inspector to verify scope names are being assigned correctly:

`Ctrl+Shift+P` → "Developer: Inspect Editor Tokens and Scopes"

Place the cursor on any token to see which TextMate scopes are applied. This is the primary debugging tool for grammar development.

---

## 10. Publishing to the VS Code Marketplace

### 10.1 First-Time Setup

**Step 1 — Create a publisher account:**
- Go to `marketplace.visualstudio.com/manage`
- Sign in with a Microsoft account
- Create a publisher with ID `fragillidae`

**Step 2 — Create a Personal Access Token (PAT):**
- Go to `dev.azure.com`
- User Settings → Personal Access Tokens
- Create a token with `Marketplace (Publish)` scope
- Save the token securely

**Step 3 — Login with vsce:**
```bash
vsce login fragillidae
# Enter PAT when prompted
```

### 10.2 Pre-publish Checklist

- [ ] `package.json` version bumped (start with `0.1.0`)
- [ ] `README.md` describes the extension clearly
- [ ] `CHANGELOG.md` has an entry for this version
- [ ] `icons/forge-file-icon.png` exists (128×128)
- [ ] `.vscodeignore` excludes development files:
  ```
  .gitignore
  .git/**
  node_modules/**
  **/*.ts
  **/*.map
  ```
- [ ] Grammar tested locally against all language constructs
- [ ] No console errors in the Extension Development Host

### 10.3 Package and Publish

```bash
# Package to a .vsix file (for local install / testing)
vsce package

# Publish directly to the marketplace
vsce publish

# Publish a specific version
vsce publish 0.1.0

# Publish a patch/minor/major version bump
vsce publish patch   # 0.1.0 → 0.1.1
vsce publish minor   # 0.1.0 → 0.2.0
vsce publish major   # 0.1.0 → 1.0.0
```

### 10.4 Versioning Strategy

| Version | Trigger |
|---------|---------|
| `0.1.x` patch | Grammar bug fixes, new snippets |
| `0.x.0` minor | New language features added to grammar (new keywords, syntax) |
| `x.0.0` major | Language Server added (Part 2 release) |

Keep the extension version in sync with FORGE language version milestones. When FORGE releases v1.0, the extension should be at or near `1.0.0`.

---

## 11. Maintenance and Evolution

### 11.1 Keeping the Grammar in Sync with the Language

As FORGE evolves (new keywords, new syntax), the grammar must be updated. The connection between the lexer and the grammar is direct: every keyword in the FORGE lexer's keyword table should appear in the grammar's keyword rules.

Maintain a comment in the grammar file listing the FORGE version it targets:

```jsonc
{
  "$schema": "...",
  "name": "FORGE",
  // Grammar version: aligned with FORGE Language Spec v0.1
  // Last updated: corresponds to toolchain Phase 1 lexer
  "scopeName": "source.forge",
  ...
}
```

### 11.2 Transition to Part 2 (LSP)

When the Language Server is ready (see Part 2 plan), the extension package will be updated to include the LSP client. The TextMate grammar remains active — it provides baseline syntax coloring even when the language server is not running. The LSP adds semantic highlighting on top.

Users upgrade seamlessly: the same `fragillidae.forge-language` extension gains new capabilities in a minor version bump.

### 11.3 Tree-sitter Grammar (Future)

A Tree-sitter grammar would improve highlighting accuracy (especially for complex nested expressions) and enable use in Neovim, Helix, and GitHub's syntax highlighting. It is a future addition and does not block Part 1 or Part 2.

---

## 12. Task Checklist

### Phase 1A — Setup (Day 1 morning)
- [ ] Create `forge-vscode/` repository
- [ ] Write `package.json` manifest
- [ ] Write `language-configuration.json`
- [ ] Create `.vscodeignore`
- [ ] Write initial `README.md` and `CHANGELOG.md`
- [ ] Test that VS Code recognizes `.fg` files

### Phase 1B — Grammar (Day 1 afternoon – Day 2)
- [ ] Create `syntaxes/forge.tmLanguage.json` skeleton
- [ ] Implement comment rule — test
- [ ] Implement string rules (double-quoted and raw) — test
- [ ] Implement keyword rules (control, declaration, operator) — test
- [ ] Implement type rules (built-in and named) — test
- [ ] Implement constant rules (language and numeric, all bases) — test
- [ ] Implement procedure and record definition rules — test
- [ ] Implement channel-related rules — test
- [ ] Implement module namespace rule — test
- [ ] Implement operator rules (all, correct order) — test
- [ ] Implement punctuation rules — test
- [ ] Full visual review against comprehensive `.fg` test file

### Phase 1C — Snippets (Day 3)
- [ ] Create `snippets/forge.code-snippets`
- [ ] Implement all declaration snippets
- [ ] Implement all control flow snippets
- [ ] Implement all import snippets
- [ ] Implement file template snippets
- [ ] Test every snippet in Extension Development Host

### Phase 1D — Polish and Publish (Day 4)
- [ ] Create file icon (128×128 PNG)
- [ ] Final grammar review against test file
- [ ] Write complete `README.md` for marketplace
- [ ] Register `fragillidae` publisher account
- [ ] Run `vsce package` and install `.vsix` locally
- [ ] Final smoke test on installed package
- [ ] Run `vsce publish 0.1.0`
- [ ] Verify listing on marketplace

---

## Appendix A — TextMate Scope Reference

Standard TextMate scope names used in the FORGE grammar and their conventional meanings:

| Scope | Used For |
|-------|---------|
| `comment.line.number-sign` | `#` line comments |
| `string.quoted.double` | `"..."` strings |
| `string.quoted.other` | `` `...` `` raw strings |
| `constant.character.escape` | `\n`, `\t`, `\xNN` etc. |
| `invalid.illegal` | Invalid escape sequences |
| `keyword.control` | `if`, `else`, `while`, `for`, `loop`, `break`, `continue`, `return` |
| `keyword.declaration` | `proc`, `record`, `var`, `const`, `channel`, `on`, `import`, `export` |
| `keyword.operator.word` | `and`, `or`, `not`, `is`, `as`, `emit`, `ref` |
| `keyword.other` | `or_else` |
| `support.type.primitive` | `int`, `float`, `bool`, `str`, `byte`, `void` |
| `entity.name.type` | User-defined record type names |
| `entity.name.type.definition` | Record name in `record Foo:` |
| `entity.name.function` | Procedure call sites |
| `entity.name.function.definition` | Procedure name in `proc foo():` |
| `entity.name.other.channel` | Channel names in `channel`, `emit`, `on` |
| `entity.name.namespace` | Module names in qualified access `module.name` |
| `constant.language` | `true`, `false`, `none` |
| `constant.numeric.integer` | Decimal integers |
| `constant.numeric.hex` | Hex literals `0xFF` |
| `constant.numeric.binary` | Binary literals `0b101` |
| `constant.numeric.octal` | Octal literals `0o755` |
| `constant.numeric.float` | Float literals |
| `keyword.operator.arrow` | `->` |
| `keyword.operator.comparison` | `==`, `!=`, `<`, `>`, `<=`, `>=` |
| `keyword.operator.assignment` | `=` |
| `keyword.operator.assignment.compound` | `+=`, `-=`, etc. |
| `keyword.operator.arithmetic` | `+`, `-`, `*`, `/`, `%` |
| `keyword.operator.bitwise` | `&`, `|`, `^`, `~`, `<<`, `>>` |
| `keyword.operator.range` | `..`, `..=` |
| `punctuation.separator.comma` | `,` |
| `punctuation.separator.colon` | `:` |
| `punctuation.definition.parameters` | `(`, `)` |
| `punctuation.definition.array` | `[`, `]` |
| `punctuation.definition.block` | `{`, `}` |

---

## Appendix B — Complete Grammar Skeleton

The complete `forge.tmLanguage.json` with all rules assembled in the correct order, ready to populate with the rule bodies defined in Section 5:

```jsonc
{
  "$schema": "https://raw.githubusercontent.com/martinring/tmlanguage/master/tmlanguage.json",
  "name": "FORGE",
  "scopeName": "source.forge",
  "fileTypes": ["fg"],
  "patterns": [
    { "include": "#comment" },
    { "include": "#string_double" },
    { "include": "#string_raw" },
    { "include": "#function_definition" },
    { "include": "#record_definition" },
    { "include": "#channel_declaration" },
    { "include": "#emit_statement" },
    { "include": "#on_handler" },
    { "include": "#keyword_control" },
    { "include": "#keyword_declaration" },
    { "include": "#keyword_operator" },
    { "include": "#keyword_optional" },
    { "include": "#type_builtin" },
    { "include": "#type_named" },
    { "include": "#constant_language" },
    { "include": "#constant_numeric" },
    { "include": "#function_call" },
    { "include": "#variable_module" },
    { "include": "#operator" },
    { "include": "#punctuation" }
  ],
  "repository": {
    "comment":              { /* Section 5.2.1  */ },
    "string_double":        { /* Section 5.2.2  */ },
    "string_raw":           { /* Section 5.2.2  */ },
    "keyword_control":      { /* Section 5.2.3  */ },
    "keyword_declaration":  { /* Section 5.2.4  */ },
    "keyword_operator":     { /* Section 5.2.5  */ },
    "keyword_optional":     { /* Section 5.2.6  */ },
    "type_builtin":         { /* Section 5.2.7  */ },
    "type_named":           { /* Section 5.2.8  */ },
    "constant_language":    { /* Section 5.2.9  */ },
    "constant_numeric":     { /* Section 5.2.10 */ },
    "function_call":        { /* Section 5.2.11 */ },
    "function_definition":  { /* Section 5.2.12 */ },
    "record_definition":    { /* Section 5.2.13 */ },
    "channel_declaration":  { /* Section 5.2.14 */ },
    "emit_statement":       { /* Section 5.2.14 */ },
    "on_handler":           { /* Section 5.2.14 */ },
    "variable_module":      { /* Section 5.2.15 */ },
    "operator":             { /* Section 5.2.16 */ },
    "punctuation":          { /* Section 5.2.17 */ }
  }
}
```

---

*FORGE VS Code Extension — Part 1 Plan v0.1 — Fragillidae Software*  
*Next: `FORGE_VSCode_Extension_Part2_LSP.md`*
