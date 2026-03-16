**FORGE**

Language Design Specification

Version 0.1 --- Initial Design

+-----------------------------------------------------------------------+
| Fast, Reliable, Organized, General-purpose, Event-driven              |
|                                                                       |
| Procedural · Modular · Message-passing · Dual-mode Execution          |
+-----------------------------------------------------------------------+

Designed by Fragillidae Software

Implemented in C · Targets: C Transpilation / LLVM IR

> **1. Overview & Vision**

FORGE is a structured, procedural, modular, event-driven programming
language designed for clarity, performance, and practical deployment. It
is not an object-oriented language. There are no classes, no inheritance
hierarchies, and no implicit runtime dispatch. Instead, FORGE organizes
programs around three explicit pillars: procedures, modules, and message
channels.

FORGE operates in two execution modes from a single source file ---
interpreted for rapid development and testing, compiled for production
deployment. The developer never rewrites or annotates code to switch
modes; the toolchain handles both paths from the same AST.

  ---------- -------------------------------------------------------------
  **WHY      You already have an interpreter in Python and a second
  FORGE?**   language in Go. FORGE is your third: implemented in C for
             maximum control, designed for both educational use and
             real-world systems work (including embedded/marine contexts),
             and built around message passing rather than OOP as the
             primary abstraction for coordination between program parts.

  ---------- -------------------------------------------------------------

**1.1 Name Rationale**

FORGE was chosen for layered meaning:

-   A forge is where raw material is shaped into useful, precise tools
    --- matching the language\'s goal of turning logic into efficient
    programs

-   FORGE as acronym: Fast, Reliable, Organized, General-purpose,
    Event-driven

-   Forging implies control and craft --- no magic, no hidden machinery,
    explicit structure

-   It fits your naming tradition of evocative single words that carry
    technical weight (Steps, PLAIN, CLASP, FABLE, FORTE\...)

**1.2 Alternative Names Considered**

  --------------- --------------------------- ---------------------------
  **Name**        **Meaning / Rationale**     **Notes**

  **FORGE**       Fast, Reliable, Organized,  ★ Recommended --- strong
                  General-purpose,            imagery, good acronym
                  Event-driven                

  **FRAME**       Fast, Reliable, Accurate,   Clean but more generic
                  Modular, Event-driven       

  **KEEL**        Structural backbone of a    Evocative but narrower
                  ship --- fits marine        domain feel
                  context                     

  **RIVET**       What holds structures       Good wordplay but slightly
                  together --- modular        industrial-only
                  fastening metaphor          

  **CHORD**       Musical harmony + marine    Interesting but ambiguous
                  line --- event coordination 

  **LASH**        Marine term for securing    Domain-specific, perhaps
                  modules together            too narrow
  --------------- --------------------------- ---------------------------

> **2. Core Design Principles**

FORGE is built on five non-negotiable design commitments:

**Explicit over implicit**

Nothing happens behind the scenes. Memory is managed by the programmer
(or via explicit region/scope rules). Message channels are declared.
Module boundaries are visible at the file level. No garbage collector
magic, no hidden vtables.

**Procedures, not objects**

The fundamental unit of logic is the procedure. Data is structured
(records/structs). Behavior lives in procedures that operate on data ---
not in methods bound to instances. This keeps the mental model flat and
auditable.

**Modules as units of trust**

Each .fg file is a module. A module declares what it exports. Consumers
import only what they need. There is no global namespace pollution. This
is the Go file-as-module model, which you already know works well.

**Messages as coordination**

When modules need to talk, they use channels and message passing --- not
shared mutable state, not global variables. This is the primary
event-driven mechanism: a module emits a message to a named channel;
other modules register handlers on that channel.

**One source, two paths**

The same .fg source runs interpreted (for development) or compiled (for
deployment). The interpreter walks the AST directly. The compiler emits
either C (for maximum portability and embedded targets) or LLVM IR (for
optimization). No source changes needed between modes.

> **3. Syntax Design**

FORGE uses Python-style indentation as block delimiters --- no braces,
no end keywords. The parser tracks indent levels. This keeps the visual
structure honest: the indentation you see is the structure the compiler
sees.

  ---------- -------------------------------------------------------------
  **DESIGN   Unlike Python, FORGE does not allow mixed tabs/spaces. The
  NOTE**     canonical indent unit is 4 spaces. The lexer enforces this
             and reports clear errors on violations. This is a deliberate
             teaching-friendly decision.

  ---------- -------------------------------------------------------------

**3.1 Basic Syntax Elements**

+-----------------------------------------------------------------------+
| \# This is a comment                                                  |
|                                                                       |
| \# Variable declaration --- type is explicit                          |
|                                                                       |
| var x: int = 42                                                       |
|                                                                       |
| var name: str = \"hello\"                                             |
|                                                                       |
| var ratio: float = 3.14                                               |
|                                                                       |
| var active: bool = true                                               |
|                                                                       |
| \# Constants                                                          |
|                                                                       |
| const MAX_RETRIES: int = 5                                            |
|                                                                       |
| const PI: float = 3.14159                                             |
+-----------------------------------------------------------------------+

**3.2 Procedures**

Procedures are the primary unit of logic. They are declared with the
proc keyword, have explicit parameter types and return types, and are
always at module scope or nested within another proc.

+-----------------------------------------------------------------------+
| proc add(a: int, b: int) -\> int:                                     |
|                                                                       |
| return a + b                                                          |
|                                                                       |
| proc greet(name: str) -\> void:                                       |
|                                                                       |
| print(\"Hello, \" + name)                                             |
|                                                                       |
| \# Multi-return via record (no tuple magic)                           |
|                                                                       |
| record DivResult:                                                     |
|                                                                       |
| quotient: int                                                         |
|                                                                       |
| remainder: int                                                        |
|                                                                       |
| proc divide(a: int, b: int) -\> DivResult:                            |
|                                                                       |
| return DivResult { quotient: a / b, remainder: a % b }                |
+-----------------------------------------------------------------------+

**3.3 Records (Structured Data)**

FORGE has no classes. Structured data is defined with record. Records
are plain data containers --- no methods, no constructors, no
inheritance. Procedures that operate on records are defined separately.

+-----------------------------------------------------------------------+
| record Sensor:                                                        |
|                                                                       |
| id: int                                                               |
|                                                                       |
| label: str                                                            |
|                                                                       |
| value: float                                                          |
|                                                                       |
| active: bool                                                          |
|                                                                       |
| \# Create a record instance                                           |
|                                                                       |
| var depth_sensor: Sensor = Sensor {                                   |
|                                                                       |
| id: 1,                                                                |
|                                                                       |
| label: \"depth\",                                                     |
|                                                                       |
| value: 0.0,                                                           |
|                                                                       |
| active: true                                                          |
|                                                                       |
| }                                                                     |
|                                                                       |
| \# Operate on records with procedures                                 |
|                                                                       |
| proc read_sensor(s: Sensor) -\> float:                                |
|                                                                       |
| if not s.active:                                                      |
|                                                                       |
| return -1.0                                                           |
|                                                                       |
| return s.value                                                        |
+-----------------------------------------------------------------------+

**3.4 Control Flow**

+-----------------------------------------------------------------------+
| \# if / elif / else                                                   |
|                                                                       |
| if x \> 10:                                                           |
|                                                                       |
| print(\"big\")                                                        |
|                                                                       |
| elif x \> 5:                                                          |
|                                                                       |
| print(\"medium\")                                                     |
|                                                                       |
| else:                                                                 |
|                                                                       |
| print(\"small\")                                                      |
|                                                                       |
| \# while loop                                                         |
|                                                                       |
| while active:                                                         |
|                                                                       |
| poll()                                                                |
|                                                                       |
| \# for loop --- range and collection                                  |
|                                                                       |
| for i in range(0, 10):                                                |
|                                                                       |
| process(i)                                                            |
|                                                                       |
| for item in items:                                                    |
|                                                                       |
| handle(item)                                                          |
|                                                                       |
| \# loop with break / continue / next                                  |
|                                                                       |
| loop:                                                                 |
|                                                                       |
| data = fetch()                                                        |
|                                                                       |
| if data == none:                                                      |
|                                                                       |
| break                                                                 |
|                                                                       |
| process(data)                                                         |
+-----------------------------------------------------------------------+

**3.5 Arrays and Maps**

+-----------------------------------------------------------------------+
| \# Fixed array                                                        |
|                                                                       |
| var readings: \[float; 10\]                                           |
|                                                                       |
| \# Dynamic array (heap-allocated, explicit)                           |
|                                                                       |
| var log: \[\]str = \[\]                                               |
|                                                                       |
| append(log, \"started\")                                              |
|                                                                       |
| \# Map (hash table)                                                   |
|                                                                       |
| var config: map\[str, int\] = {}                                      |
|                                                                       |
| config\[\"timeout\"\] = 30                                            |
|                                                                       |
| config\[\"retries\"\] = 3                                             |
|                                                                       |
| \# Check key exists                                                   |
|                                                                       |
| if has_key(config, \"timeout\"):                                      |
|                                                                       |
| print(config\[\"timeout\"\])                                          |
+-----------------------------------------------------------------------+

> **4. Module System**

Every .fg file is a module. The module name is the filename without
extension. There is no explicit module declaration statement --- the
file IS the module. This is the Go convention, which eliminates
boilerplate and makes the mapping between files and namespaces visually
obvious.

**4.1 Exports and Imports**

+-----------------------------------------------------------------------+
| \# In file: sensors.fg                                                |
|                                                                       |
| \# Exported symbols use \'export\' keyword                            |
|                                                                       |
| export record Sensor:                                                 |
|                                                                       |
| id: int                                                               |
|                                                                       |
| label: str                                                            |
|                                                                       |
| value: float                                                          |
|                                                                       |
| export proc read(s: Sensor) -\> float:                                |
|                                                                       |
| return s.value                                                        |
|                                                                       |
| \# Private --- not exported, internal only                            |
|                                                                       |
| proc calibrate(s: Sensor) -\> void:                                   |
|                                                                       |
| \...                                                                  |
+-----------------------------------------------------------------------+

+-----------------------------------------------------------------------+
| \# In file: main.fg                                                   |
|                                                                       |
| import sensors                                                        |
|                                                                       |
| import display                                                        |
|                                                                       |
| proc main() -\> void:                                                 |
|                                                                       |
| var s: sensors.Sensor = sensors.Sensor { id: 1, label: \"depth\",     |
| value: 12.5 }                                                         |
|                                                                       |
| var v: float = sensors.read(s)                                        |
|                                                                       |
| display.show(v)                                                       |
+-----------------------------------------------------------------------+

  ---------- -------------------------------------------------------------
  **RULE**   You can only access exported symbols from another module.
             Attempting to use a non-exported symbol from outside its
             module is a compile-time error. This makes API surfaces
             explicit and auditable.

  ---------- -------------------------------------------------------------

**4.2 Module Initialization**

A module may optionally define an init proc. It is called automatically
when the module is first imported. It takes no arguments and returns
void. Order of init calls follows import order.

+-----------------------------------------------------------------------+
| \# sensors.fg                                                         |
|                                                                       |
| var \_initialized: bool = false                                       |
|                                                                       |
| proc init() -\> void:                                                 |
|                                                                       |
| \_initialized = true                                                  |
|                                                                       |
| log(\"sensors module ready\")                                         |
+-----------------------------------------------------------------------+

> **5. Message Passing & Event System**

The event system in FORGE is built on named channels. A channel is a
typed message pipe. Any module can declare a channel, emit messages to
it, and register handlers on it. Channels are the ONLY sanctioned way to
coordinate between modules at runtime --- there is no shared mutable
global state.

This design is deliberately similar in philosophy to hardware interrupt
handlers and pub/sub buses --- both of which you work with in marine
electronics contexts. A depth sounder event, an NMEA sentence arrival,
or a UI button press all map naturally to channel messages.

**5.1 Declaring and Using Channels**

+-----------------------------------------------------------------------+
| \# Declare a channel --- gives it a name and message type             |
|                                                                       |
| channel depth_reading: float                                          |
|                                                                       |
| channel sensor_error: str                                             |
|                                                                       |
| channel shutdown: void                                                |
|                                                                       |
| \# Emit a message to a channel                                        |
|                                                                       |
| emit depth_reading -\> 12.5                                           |
|                                                                       |
| emit sensor_error -\> \"timeout on port 2\"                           |
|                                                                       |
| emit shutdown                                                         |
|                                                                       |
| \# Register a handler on a channel                                    |
|                                                                       |
| on depth_reading as value:                                            |
|                                                                       |
| display.update(value)                                                 |
|                                                                       |
| log(\"depth: \" + str(value))                                         |
|                                                                       |
| on sensor_error as msg:                                               |
|                                                                       |
| alert(msg)                                                            |
|                                                                       |
| on shutdown:                                                          |
|                                                                       |
| cleanup()                                                             |
|                                                                       |
| halt()                                                                |
+-----------------------------------------------------------------------+

**5.2 Cross-Module Channels**

Channels can be exported just like procedures and records. A module that
owns a channel exports it; other modules import the module and bind
handlers to its channels.

+-----------------------------------------------------------------------+
| \# nmea.fg --- owns the channel                                       |
|                                                                       |
| export channel sentence_received: str                                 |
|                                                                       |
| proc parse_port() -\> void:                                           |
|                                                                       |
| loop:                                                                 |
|                                                                       |
| var s: str = read_serial()                                            |
|                                                                       |
| emit sentence_received -\> s                                          |
|                                                                       |
| \# autopilot.fg --- listens to the channel                            |
|                                                                       |
| import nmea                                                           |
|                                                                       |
| on nmea.sentence_received as raw:                                     |
|                                                                       |
| var parsed = parse_nmea(raw)                                          |
|                                                                       |
| adjust_heading(parsed)                                                |
+-----------------------------------------------------------------------+

**5.3 Channel Delivery Model**

  --------------- -------------------------------------------------------------
  **INTERPRETED   Channels are synchronous by default in the interpreter. Emit
  MODE**          blocks until all registered handlers complete. This makes
                  behavior predictable and debuggable during development.

  --------------- -------------------------------------------------------------

  ------------ -------------------------------------------------------------
  **COMPILED   The compiler can optionally make channels asynchronous
  MODE**       (queued delivery) via a compile flag. Default compiled
               behavior is also synchronous unless \--async-channels is
               specified. Future versions may support buffered channels.

  ------------ -------------------------------------------------------------

> **6. Type System**

FORGE is statically typed. All variables, procedure parameters, and
return values have explicit types known at compile time. There is no
dynamic typing, no any type, and no implicit coercion between numeric
types.

**6.1 Primitive Types**

  ------------ ------------------ ----------------------------------------
  **Type**     **Size / Range**   **Notes**

  **int**      64-bit signed      Default integer. Use int8/int16/int32
                                  for embedded targets.

  **uint**     64-bit unsigned    Use uint8/uint16/uint32 for bit-level
                                  work.

  **float**    64-bit IEEE 754    Use float32 for embedded/sensor
                                  contexts.

  **bool**     true / false       Not an integer. Cannot be used in
                                  arithmetic directly.

  **str**      UTF-8, heap        Immutable. Operations return new
                                  strings.

  **byte**     uint8 alias        Preferred type for raw data buffers,
                                  NMEA, serial I/O.

  **none**     absence of value   Used as a return type for procedures
                                  with no result.

  **void**     channel signal     Used only in channel declarations with
                                  no payload.
  ------------ ------------------ ----------------------------------------

**6.2 Optional Values**

FORGE has no null pointer. Instead, values that may be absent use the
optional type wrapper.

+-----------------------------------------------------------------------+
| var result: ?int = find_item(list, key)                               |
|                                                                       |
| if result is some:                                                    |
|                                                                       |
| process(result.value)                                                 |
|                                                                       |
| else:                                                                 |
|                                                                       |
| log(\"not found\")                                                    |
|                                                                       |
| \# Unwrap with default                                                |
|                                                                       |
| var val: int = result or_else 0                                       |
+-----------------------------------------------------------------------+

> **7. Dual Execution Architecture**

This is the central technical innovation of FORGE. The same source file
passes through a single frontend (lexer → parser → AST) and then
diverges into two backend paths.

**7.1 Execution Pipeline**

+-----------------------------------+-----------------------------------+
| **INTERPRETED PATH**              | **COMPILED PATH**                 |
+-----------------------------------+-----------------------------------+
| forge run main.fg                 | forge build main.fg               |
|                                   |                                   |
| Lexer → Parser → AST              | Lexer → Parser → AST              |
|                                   |                                   |
| ↓ Tree-walking interpreter        | ↓ Code generator                  |
|                                   |                                   |
| Immediate execution               | Emit C → gcc/clang                |
|                                   |                                   |
| Full runtime error messages       | --- or ---                        |
|                                   |                                   |
| REPL available                    | Emit LLVM IR → opt + llc          |
+-----------------------------------+-----------------------------------+

**7.2 Toolchain CLI**

+-----------------------------------------------------------------------+
| forge run main.fg \# Interpret and execute                            |
|                                                                       |
| forge run main.fg \--repl \# Drop into REPL after main()              |
|                                                                       |
| forge check main.fg \# Type-check only, no execution                  |
|                                                                       |
| forge build main.fg \# Compile via C (default)                        |
|                                                                       |
| forge build main.fg \--target llvm \# Compile via LLVM IR             |
|                                                                       |
| forge build main.fg \--target c \# Explicit C transpilation           |
|                                                                       |
| forge build main.fg \--out ./bin/app \# Specify output binary         |
|                                                                       |
| forge fmt main.fg \# Auto-format source                               |
|                                                                       |
| forge doc main.fg \# Generate module documentation                    |
+-----------------------------------------------------------------------+

**7.3 Implementation Phases**

Recommended build order for the C implementation:

**Phase 1 --- Lexer**

Tokenize indentation, keywords, identifiers, literals. Emit
INDENT/DEDENT tokens (Python-style). Write in C with a simple
hand-written scanner.

**Phase 2 --- Parser**

Recursive descent parser. Builds an AST in C using a tagged union node
type. No external parser generator needed --- hand-written RD is easier
to extend and debug.

**Phase 3 --- Interpreter**

Tree-walking evaluator. Environment stack for scopes. Channel registry
as a linked list of (name, handler list) pairs. This gives you a working
FORGE immediately.

**Phase 4 --- Type Checker**

Walk the AST before interpretation/compilation. Infer types, check
procedure signatures, validate channel payload types.

**Phase 5a --- C Emitter**

Walk the AST and emit C source. Records → structs. Procedures →
functions. Channels → function pointer arrays with dispatch loops. Link
against a small FORGE runtime .c file.

**Phase 5b --- LLVM Emitter**

Optional / later. Walk the AST and emit LLVM IR text. Use llc to compile
to native. Enables optimization passes.

> **8. Memory Model**

FORGE does not have garbage collection. It uses a two-tier memory model
designed to be explicit and teachable:

-   Stack allocation for all local variables, records, and fixed arrays
    --- automatic, zero cost

-   Explicit heap allocation for dynamic arrays, maps, and strings
    longer than a compile-time constant

-   Heap allocations must be explicitly freed, or assigned to a scope
    that manages their lifetime

+-----------------------------------------------------------------------+
| \# Stack allocated --- no cleanup needed                              |
|                                                                       |
| var x: int = 42                                                       |
|                                                                       |
| var s: Sensor = Sensor { id: 1, label: \"d\", value: 0.0 }            |
|                                                                       |
| \# Heap allocated --- explicit                                        |
|                                                                       |
| var buf: \[\]byte = alloc(\[\]byte, 256)                              |
|                                                                       |
| \# \... use buf \...                                                  |
|                                                                       |
| free(buf)                                                             |
|                                                                       |
| \# Scope-managed heap (future v2 feature)                             |
|                                                                       |
| with alloc(\[\]byte, 1024) as buf:                                    |
|                                                                       |
| fill(buf)                                                             |
|                                                                       |
| process(buf)                                                          |
|                                                                       |
| \# buf is automatically freed here                                    |
+-----------------------------------------------------------------------+

  --------------- -------------------------------------------------------------
  **EDUCATIONAL   This explicit model is intentional. For teaching, students
  VALUE**         must understand that memory is a resource. The with-alloc
                  pattern gives a safe escalation path while keeping the
                  underlying model visible rather than hidden behind a GC.

  --------------- -------------------------------------------------------------

> **9. Standard Library (Core)**

FORGE ships with a minimal standard library. Every module in the stdlib
is prefixed with forge.\*

  ------------------ ----------------------------------------------------
  **Module**         **Contents**

  forge.io           print, read_line, read_file, write_file, open,
                     close, flush

  forge.str          length, split, join, trim, contains, starts_with,
                     ends_with, to_upper, to_lower, format

  forge.math         abs, min, max, pow, sqrt, floor, ceil, round, random

  forge.serial       open_port, close_port, read_bytes, write_bytes,
                     set_baud --- marine/embedded target

  forge.nmea         parse_sentence, extract_field, checksum --- NMEA
                     0183 utility module

  forge.time         now, sleep, timestamp, elapsed

  forge.sys          args, env, exit, halt, platform

  forge.buf          Buffer record, read, write, seek, position --- byte
                     buffer operations
  ------------------ ----------------------------------------------------

> **10. Sample Programs**

**10.1 Hello World**

+-----------------------------------------------------------------------+
| \# hello.fg                                                           |
|                                                                       |
| import forge.io                                                       |
|                                                                       |
| proc main() -\> void:                                                 |
|                                                                       |
| forge.io.print(\"Hello from FORGE\")                                  |
+-----------------------------------------------------------------------+

**10.2 Module Communication via Channel**

+-----------------------------------------------------------------------+
| \# logger.fg                                                          |
|                                                                       |
| import forge.io                                                       |
|                                                                       |
| export channel log_event: str                                         |
|                                                                       |
| on log_event as msg:                                                  |
|                                                                       |
| forge.io.print(\"\[LOG\] \" + msg)                                    |
|                                                                       |
| \# sensor.fg                                                          |
|                                                                       |
| import logger                                                         |
|                                                                       |
| import forge.time                                                     |
|                                                                       |
| export proc start_polling() -\> void:                                 |
|                                                                       |
| loop:                                                                 |
|                                                                       |
| var reading: float = read_hardware()                                  |
|                                                                       |
| if reading \< 0.0:                                                    |
|                                                                       |
| emit logger.log_event -\> \"sensor fault\"                            |
|                                                                       |
| forge.time.sleep(500)                                                 |
|                                                                       |
| \# main.fg                                                            |
|                                                                       |
| import sensor                                                         |
|                                                                       |
| import logger                                                         |
|                                                                       |
| proc main() -\> void:                                                 |
|                                                                       |
| emit logger.log_event -\> \"system starting\"                         |
|                                                                       |
| sensor.start_polling()                                                |
+-----------------------------------------------------------------------+

**10.3 NMEA Sentence Processor**

+-----------------------------------------------------------------------+
| \# nmea_processor.fg                                                  |
|                                                                       |
| import forge.serial                                                   |
|                                                                       |
| import forge.nmea                                                     |
|                                                                       |
| import forge.io                                                       |
|                                                                       |
| export channel gps_position: forge.nmea.Position                      |
|                                                                       |
| export channel sentence_error: str                                    |
|                                                                       |
| proc main() -\> void:                                                 |
|                                                                       |
| var port = forge.serial.open_port(\"/dev/ttyS0\", 4800)               |
|                                                                       |
| loop:                                                                 |
|                                                                       |
| var raw: str = forge.serial.read_line(port)                           |
|                                                                       |
| if forge.nmea.checksum(raw):                                          |
|                                                                       |
| var pos = forge.nmea.parse_gga(raw)                                   |
|                                                                       |
| if pos is some:                                                       |
|                                                                       |
| emit gps_position -\> pos.value                                       |
|                                                                       |
| else:                                                                 |
|                                                                       |
| emit sentence_error -\> \"bad checksum: \" + raw                      |
+-----------------------------------------------------------------------+

> **11. Educational Use Considerations**

FORGE is designed to be teachable alongside your existing Steps and
PLAIN curricula. Its design choices align with your educational
philosophy of conscious engagement --- students must understand what
code does, not just write it.

**Explicit types remove guesswork**

Students see exactly what kind of data they are working with. No
implicit coercion, no surprise type changes.

**Modules teach information hiding early**

The export/import boundary forces students to think about API design
from the first multi-file program.

**Channels make event flow visible**

Rather than callbacks buried in objects, event handling is a first-class
syntax construct. Students can read channel flow like a data diagram.

**Interpreted mode = safe playground**

Run forge run main.fg to test ideas immediately, with full error
messages. No compile step barrier for beginners.

**Compiled mode = CS concept demonstrator**

Show students that the same source can become native machine code.
Discuss what the compiler does. Bridge the abstraction gap.

**No GC means memory is a teaching moment**

Explicit alloc/free is harder but produces programmers who understand
what computers actually do with memory.

> **12. Next Steps**

Recommended development sequence:

-   Define the full token set and keyword list --- write the lexer in C
    first

-   Build the AST node types as a C tagged union (union + enum tag
    pattern)

-   Write the recursive descent parser --- start with expressions, then
    statements, then declarations

-   Build the tree-walking interpreter --- environment stack, channel
    registry, basic stdlib

-   Add the type checker as a separate AST pass

-   Write the C emitter --- generates compilable C from the AST

-   Design the IDE icon for FORGE to match the rest of your suite

-   Consider FORGE as the implementation language for future tools
    (after bootstrapping)

  ----------------- -------------------------------------------------------------
  **BOOTSTRAPPING   Once FORGE is stable enough, writing the FORGE interpreter or
  NOTE**            compiler in FORGE itself is a meaningful milestone. A
                    self-hosting language is a strong signal of maturity --- and
                    a powerful teaching demonstration.

  ----------------- -------------------------------------------------------------

FORGE Language Design Specification v0.1 · Fragillidae Software ·
Confidential
