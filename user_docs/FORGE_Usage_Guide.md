# FORGE Language Usage Guide

**Version:** 0.1  
**File Extension:** `.fg`  
**Toolchain Binary:** `forge`

> *FORGE — Fast, Reliable, Organized, General-purpose, Event-driven*

---

## Table of Contents

1. [Quick Start](#1-quick-start)
2. [CLI Commands Reference](#2-cli-commands-reference)
3. [Language Basics](#3-language-basics)
4. [Types](#4-types)
5. [Variables and Constants](#5-variables-and-constants)
6. [Expressions and Operators](#6-expressions-and-operators)
7. [Control Flow](#7-control-flow)
8. [Procedures](#8-procedures)
9. [Records](#9-records)
10. [Arrays and Maps](#10-arrays-and-maps)
11. [Optional Types](#11-optional-types)
12. [Module System](#12-module-system)
13. [Channel System](#13-channel-system)
14. [Standard Library](#14-standard-library)
15. [Memory Management](#15-memory-management)
16. [Error Handling](#16-error-handling)
17. [REPL Guide](#17-repl-guide)
18. [Reserved Words](#18-reserved-words)
19. [Operator Precedence](#19-operator-precedence)

### Companion Beginner Guides

If you are teaching or learning FORGE as part of the Steps -> PLAIN -> FORGE trilogy,
these slower, example-driven guides may help:

- [FORGE Optional Types Guide](FORGE_Optional_Types_Guide.md)
- [FORGE Memory Management Guide](FORGE_Memory_Management_Guide.md)

---

## 1. Quick Start

### Installation

Build the FORGE toolchain:

```bash
make
```

This produces the `forge` binary.

### Your First Program

Create `hello.fg`:

```forge
# Hello World - FORGE

proc main() -> void:
    print("Hello, FORGE!")
```

### Run It

```bash
$ ./forge run hello.fg
Hello, FORGE!
```

### Compile It

```bash
$ ./forge build hello.fg -o hello
$ ./hello
Hello, FORGE!
```

---

## 2. CLI Commands Reference

### Command Overview

| Command | Description |
|---------|-------------|
| `forge run <file.fg>` | Interpret and execute |
| `forge build <file.fg>` | Compile to native binary |
| `forge check <file.fg>` | Type-check only (no execution) |
| `forge fmt <file.fg>` | Format source in place |
| `forge emit <file.fg>` | Output generated C code |
| `forge emit-llvm <file.fg>` | Output LLVM IR |
| `forge doc <file.fg>` | Generate documentation |
| `forge repl` | Interactive REPL |
| `forge --version` | Show version |
| `forge --help` | Show help |

---

### 2.1 `forge run` — Interpret a Program

Run a FORGE program using the tree-walking interpreter.

**Syntax:**
```bash
forge run <file.fg> [options]
```

**Options:**

| Flag | Description |
|------|-------------|
| `--trace` | Print each statement as it executes |
| `--repl` | Drop into REPL after `main()` returns |
| `--no-color` | Disable colored terminal output |
| `-v, --verbose` | Verbose output |
| `-q, --quiet` | Suppress non-error output |

**Examples:**

```bash
# Basic execution
$ ./forge run hello.fg
Hello, FORGE!

# Run with tracing
$ ./forge run fibonacci.fg --trace
[TRACE] var n: int = 10
[TRACE] var result: int = fib(n)
...

# Run and then drop into REPL
$ ./forge run myprogram.fg --repl
Program output here...

--- main() finished, entering REPL ---
>>> x    # Access variables from program
42
>>> exit
```

---

### 2.2 `forge build` — Compile to Binary

Compile a FORGE program to a native executable.

**Syntax:**
```bash
forge build <file.fg> [options]
```

**Options:**

| Flag | Description |
|------|-------------|
| `-o, --out <file>` | Output file path |
| `--target <c\|llvm>` | Compilation backend (default: `c`) |
| `-O, --opt <0-3>` | Optimization level (default: 0) |
| `-g, --debug` | Include debug symbols |
| `--bounds-check` | Enable array bounds checking |
| `--async-channels` | Enable async channel delivery |
| `--strict` | Treat warnings as errors |
| `--cc <compiler>` | C compiler to use (default: `gcc`) |
| `--arch <arch>` | Cross-compile target architecture |
| `--os <os>` | Cross-compile target OS |
| `--runtime <path>` | Path to runtime directory |

**Examples:**

```bash
# Basic compilation
$ ./forge build hello.fg -o hello
$ ./hello
Hello, FORGE!

# Compile with optimizations
$ ./forge build program.fg -o program -O2

# Compile with LLVM backend
$ ./forge build program.fg -o program --target llvm

# Compile with all safety checks
$ ./forge build program.fg -o program --bounds-check --strict

# Debug build
$ ./forge build program.fg -o program -g --debug

# Use clang instead of gcc
$ ./forge build program.fg -o program --cc clang
```

---

### 2.3 `forge check` — Type-Check Only

Parse and type-check without executing or compiling. Useful for CI/CD pipelines.

**Syntax:**
```bash
forge check <file.fg> [options]
```

**Options:**

| Flag | Description |
|------|-------------|
| `--strict` | Treat warnings as errors |
| `-v, --verbose` | Verbose output |

**Examples:**

```bash
# Check a single file
$ ./forge check mymodule.fg
✓ mymodule.fg: OK

# Check with strict mode (warnings become errors)
$ ./forge check mymodule.fg --strict

# Check multiple files
$ ./forge check src/*.fg
✓ sensors.fg: OK
✓ nmea_parser.fg: OK
✓ main.fg: OK
```

---

### 2.4 `forge fmt` — Format Source Code

Format FORGE source files to canonical style.

**Formatting Rules:**
- 4-space indentation
- One blank line between top-level declarations
- Spaces around binary operators
- No trailing whitespace
- Consistent spacing in record literals and argument lists

**Syntax:**
```bash
forge fmt <file.fg> [files...]
```

**Examples:**

```bash
# Format a single file (modifies in place)
$ ./forge fmt myprogram.fg
✓ Formatted myprogram.fg

# Format multiple files
$ ./forge fmt src/*.fg
✓ Formatted src/main.fg
✓ Formatted src/sensors.fg
✓ Formatted src/parser.fg
```

---

### 2.5 `forge emit` / `forge emit-llvm` — View Generated Code

Output the generated C or LLVM IR code without compiling.

**Examples:**

```bash
# Show generated C code
$ ./forge emit hello.fg
/* Generated by FORGE */
#include "forge_runtime.h"

void forge_main(void) {
    forge_print(forge_str_lit("Hello, FORGE!"));
}
...

# Show generated LLVM IR
$ ./forge emit-llvm hello.fg
; Generated by FORGE LLVM Emitter
@.str.0 = private constant [14 x i8] c"Hello, FORGE!\00"

define void @forge_main() {
entry:
    ...
}
```

---

### 2.6 `forge doc` — Generate Documentation

Generate documentation from source comments.

**Syntax:**
```bash
forge doc <file.fg> [--format md|html|text]
```

**Examples:**

```bash
$ ./forge doc sensors.fg --format md
# Module: sensors

## Procedures

### read(s: Sensor) -> float
Reads the current value from the sensor.
Returns -1.0 if the sensor is inactive.
...
```

---

### 2.7 `forge repl` — Interactive Shell

Start an interactive read-eval-print loop.

**Features:**
- Variables persist across lines
- Procedures can be defined and called later
- Expression results are automatically printed
- Special commands: `help`, `exit`, `quit`

**Syntax:**
```bash
forge repl
```

**Example Session:**

```
$ ./forge repl
FORGE 0.1.0 REPL — type 'exit' or Ctrl-D to quit, 'help' for commands
>>> var x: int = 42
>>> x * 2
84
>>> proc double(n: int) -> int:
...     return n * 2
...
>>> double(21)
42
>>> record Point:
...     x: int
...     y: int
...
>>> var p = Point { x: 10, y: 20 }
>>> p.x + p.y
30
>>> help
REPL Commands:
  exit, quit    Exit the REPL
  help          Show this help
>>> exit
```

---

## 3. Language Basics

### 3.1 File Structure

Every FORGE source file is a module. The entry point is `proc main() -> void`.

```forge
# This is a comment

# Constants (optional)
const MAX_VALUE: int = 100

# Records (optional)
record Point:
    x: int
    y: int

# Procedures
proc helper() -> void:
    print("I'm a helper")

# Entry point (required for executables)
proc main() -> void:
    helper()
    print("Done!")
```

### 3.2 Indentation

FORGE uses **4-space indentation** to define blocks. Tabs are not allowed.

```forge
proc example() -> void:
    var x: int = 1        # 4 spaces
    if x > 0:
        print("positive") # 8 spaces (nested block)
    print("done")         # back to 4 spaces
```

### 3.3 Comments

Only line comments are supported:

```forge
# This is a comment
var x: int = 42  # Inline comment

# Multi-line comments use
# consecutive line comments
```

### 3.4 Identifiers

- Must begin with a letter (a-z, A-Z)
- May contain letters, digits, and underscores
- Case-sensitive (`count` ≠ `Count`)
- Cannot be a reserved word

```forge
# Valid identifiers
var depth: int = 0
var sensorID: int = 42
var max_retries: int = 3

# Invalid
# var 2fast: int = 0      # Starts with digit
# var proc: int = 0       # Reserved word
```

---

## 4. Types

### 4.1 Primitive Types

| Type | Description | Default Value |
|------|-------------|---------------|
| `int` | 64-bit signed integer | `0` |
| `int8` | 8-bit signed integer | `0` |
| `int16` | 16-bit signed integer | `0` |
| `int32` | 32-bit signed integer | `0` |
| `uint` | 64-bit unsigned integer | `0` |
| `uint8` | 8-bit unsigned integer | `0` |
| `uint16` | 16-bit unsigned integer | `0` |
| `uint32` | 32-bit unsigned integer | `0` |
| `float` | 64-bit IEEE 754 double | `0.0` |
| `float32` | 32-bit IEEE 754 float | `0.0` |
| `bool` | Boolean (true/false) | `false` |
| `str` | UTF-8 string | `""` |
| `byte` | Alias for `uint8` | `0` |
| `void` | No value (return type only) | — |

### 4.2 Type Examples

```forge
var count: int = 42
var temperature: float = 98.6
var active: bool = true
var name: str = "FORGE"
var ch: byte = 0x41        # 'A'
```

### 4.3 Type Conversions

All conversions must be explicit:

```forge
var a: int = 42
var b: float = float(a)    # int → float
var c: int32 = int32(a)    # int → int32 (may truncate)
var d: str = str(a)        # any → string
```

### 4.4 Runtime Type Inspection

Use the built-in `type()` function to get the runtime type of any value as a string:

```forge
var t: str = type(42)        # "int"
var t: str = type("hello")   # "str"
var t: str = type(true)      # "bool"
var t: str = type(3.14)      # "float"
```

**Signature:** `type(val: any) -> str`

This is useful when working with values of unknown or dynamic origin, such as data read from external input:

```forge
var raw: str = read_line()
var parsed: ?int = forge.str.to_int(raw)

if parsed is some:
    print("Got int: " + str(parsed.value))
else:
    print("Not an integer")
```

For compile-time type safety, prefer the static type checker (`forge check`) and the type system rather than runtime `type()` checks where possible.

---

## 5. Variables and Constants

### 5.1 Variable Declaration

```forge
var name: Type = expression
var name: Type              # Zero-initialized
```

**Examples:**

```forge
var x: int = 42
var pi: float = 3.14159
var greeting: str = "Hello"
var count: int              # Initialized to 0
var ratio: float            # Initialized to 0.0
```

### 5.2 Constant Declaration

Constants are compile-time values and use `SCREAMING_SNAKE_CASE`:

```forge
const MAX_RETRIES: int = 5
const PI: float = 3.14159265358979
const APP_NAME: str = "FORGE Navigator"
const BUFFER_SIZE: int = 512
```

### 5.3 Assignment

```forge
var x: int = 10
x = 20              # Simple assignment
x += 5              # Add and assign (x = x + 5)
x -= 3              # Subtract and assign
x *= 2              # Multiply and assign
x /= 4              # Divide and assign
x %= 3              # Modulo and assign

# Bitwise compound assignments
x &= 0xFF           # AND assign
x |= 0x80           # OR assign
x ^= 0x0F           # XOR assign
x <<= 2             # Left shift assign
x >>= 1             # Right shift assign
```

### 5.4 Variable Swapping

FORGE provides a built-in `swap(a, b)` function that swaps two variables in place. It works with any type and requires no import.

```forge
var a: int = 10
var b: int = 99
swap(a, b)
print(a)    # 99
print(b)    # 10
```

`swap` works with any type — `int`, `float`, `str`, and more:

```forge
var x: float = 3.14
var y: float = 2.71
swap(x, y)    # x == 2.71, y == 3.14

var s1: str = "hello"
var s2: str = "world"
swap(s1, s2)  # s1 == "world", s2 == "hello"
```

Both arguments must be simple variable names (not expressions or array elements). This is the FORGE equivalent of Python's `a, b = b, a`.

---

## 6. Expressions and Operators

### 6.1 Arithmetic Operators

```forge
a + b       # Addition
a - b       # Subtraction
a * b       # Multiplication
a / b       # Division (integer division truncates toward zero)
a % b       # Modulo (sign follows dividend)
-a          # Unary negation
```

### 6.2 Comparison Operators

All comparisons return `bool`:

```forge
a == b      # Equal
a != b      # Not equal
a < b       # Less than
a > b       # Greater than
a <= b      # Less than or equal
a >= b      # Greater than or equal
```

### 6.3 Logical Operators

```forge
a and b     # Logical AND (short-circuit)
a or b      # Logical OR (short-circuit)
not a       # Logical NOT
```

**Short-circuit evaluation:**
- In `a and b`, `b` is not evaluated if `a` is `false`
- In `a or b`, `b` is not evaluated if `a` is `true`

### 6.4 Bitwise Operators

```forge
a & b       # Bitwise AND
a | b       # Bitwise OR
a ^ b       # Bitwise XOR
~a          # Bitwise NOT (ones complement)
a << n      # Left shift
a >> n      # Right shift
```

### 6.5 String Concatenation

```forge
var greeting: str = "Hello, " + name + "!"
var message: str = "Count: " + str(42)
```

---

## 7. Control Flow

### 7.1 If / Elif / Else

```forge
# Simple if
if x > 0:
    print("positive")

# If-else
if x > 0:
    print("positive")
else:
    print("non-positive")

# If-elif-else chain
if grade >= 90:
    print("A")
elif grade >= 80:
    print("B")
elif grade >= 70:
    print("C")
else:
    print("F")
```

**Important:** Conditions must be `bool`. No implicit truthiness:

```forge
var count: int = 5
if count:               # ERROR: int is not bool
    print("has items")

if count != 0:          # OK: explicit comparison
    print("has items")
```

### 7.2 While Loop

```forge
var i: int = 0
while i < 10:
    print(i)
    i = i + 1
```

### 7.3 For Loop

```forge
# Range-based (exclusive end)
for i in range(0, 10):     # 0, 1, 2, ..., 9
    print(i)

# Range with step
for i in range(10, 0, -1): # 10, 9, 8, ..., 1
    print(i)

# Iterate over array
var nums: []int = [1, 2, 3, 4, 5]
for n in nums:
    print(n)
```

### 7.4 Loop (Infinite)

```forge
loop:
    var input: str = read_line()
    if input == "quit":
        break
    process(input)
```

### 7.5 Break and Continue

```forge
# Break exits the loop
var i: int = 0
while true:
    if i >= 5:
        break
    print(i)
    i = i + 1

# Continue skips to next iteration
for i in range(0, 10):
    if i == 5:
        continue
    print(i)   # Prints 0,1,2,3,4,6,7,8,9
```

---

## 8. Procedures

### 8.1 Basic Syntax

```forge
proc name(param1: Type1, param2: Type2) -> ReturnType:
    # body
    return value
```

### 8.2 Examples

```forge
# No parameters, no return
proc greet() -> void:
    print("Hello!")

# One parameter, returns value
proc square(n: int) -> int:
    return n * n

# Multiple parameters
proc add(a: int, b: int) -> int:
    return a + b

# Returning a record
record Point:
    x: int
    y: int

proc make_point(x: int, y: int) -> Point:
    return Point { x: x, y: y }
```

### 8.3 Recursion

```forge
proc factorial(n: int) -> int:
    if n <= 1:
        return 1
    return n * factorial(n - 1)

proc main() -> void:
    print(factorial(5))   # Output: 120
```

### 8.4 Reference Parameters

By default, parameters are passed by value. Use `ref` for reference passing:

```forge
proc increment(n: ref int) -> void:
    n += 1

proc main() -> void:
    var count: int = 0
    increment(ref count)
    print(count)  # Output: 1
```

The caller must use the `ref` keyword explicitly at the call site — this makes mutation of the caller's variable visible in the source.

> **Note:** For the common case of swapping two variables, use the built-in `swap(a, b)` rather than writing a custom swap procedure. See [Section 5.4](#54-variable-swapping).

---

## 9. Records

Records are plain data containers (no methods, no inheritance).

### 9.1 Declaration

```forge
record Point:
    x: int
    y: int

record Person:
    name: str
    age: int
    active: bool
```

### 9.2 Instantiation

All fields must be provided:

```forge
var p: Point = Point { x: 10, y: 20 }
var person: Person = Person {
    name: "Alice",
    age: 30,
    active: true
}
```

### 9.3 Field Access

```forge
var x_coord: int = p.x
var person_name: str = person.name

# Field assignment
p.x = 100
person.active = false
```

### 9.4 Nested Records

```forge
record Position:
    latitude: float
    longitude: float

record NavData:
    position: Position
    heading: float
    speed: float

var nav: NavData = NavData {
    position: Position { latitude: 40.7128, longitude: -74.0060 },
    heading: 275.0,
    speed: 6.5
}

var lat: float = nav.position.latitude
```

---

## 10. Arrays and Maps

### 10.1 Fixed Arrays

Stack-allocated, compile-time size:

```forge
var readings: [float; 10]              # 10 floats, zero-initialized
var buf: [byte; 512]                   # 512-byte buffer
var coords: [float; 3] = [1.0, 2.0, 3.0]
```

### 10.2 Dynamic Arrays

Heap-allocated, can grow:

```forge
# Array literal
var nums: []int = [1, 2, 3, 4, 5]

# Empty array
var log: []str = []

# Array operations
print(len(nums))        # Length: 5
print(nums[0])          # First element: 1
nums[0] = 100           # Modify element

# Append
append(log, "event one")
append(log, "event two")

# Iterate
for item in nums:
    print(item)
```

### 10.3 Maps

Hash tables with key-value pairs:

```forge
var config: map[str, int] = {}
config["timeout"] = 30
config["retries"] = 3

var t: int = config["timeout"]

# Check if key exists
if has_key(config, "debug"):
    print("debug mode")

# Delete a key
delete_key(config, "retries")

# Get length
print(len(config))
```

---

## 11. Optional Types

FORGE has no null pointers. Use optional types for values that may be absent.

For a more detailed beginner walkthrough with extra examples and practice,
see [FORGE_Optional_Types_Guide.md](FORGE_Optional_Types_Guide.md).

### 11.1 Declaring Optionals

```forge
var found: ?int = none        # No value
var result: ?int = some(42)   # Has value
```

### 11.2 Checking and Unwrapping

```forge
var found: ?int = search(list, key)

# Check with is
if found is some:
    print(found.value)    # Unwrap with .value
else:
    print("not found")

# Unwrap with default
var val: int = found or_else 0

# Direct check
if found is none:
    print("missing")
```

### 11.3 Returning Optionals

```forge
proc find_index(arr: []int, target: int) -> ?int:
    for i in range(0, len(arr)):
        if arr[i] == target:
            return some(i)
    return none

proc main() -> void:
    var nums: []int = [10, 20, 30, 40]
    var idx: ?int = find_index(nums, 30)

    if idx is some:
        print("Found at index: " + str(idx.value))
    else:
        print("Not found")
```

---

## 12. Module System

### 12.1 Module Identity

Every `.fg` file is a module. The module name is the filename without extension.

```
sensors.fg      → module: sensors
nmea_parser.fg  → module: nmea_parser
```

### 12.2 Exports

Symbols are private by default. Use `export` to make them accessible:

```forge
# sensors.fg

export record Sensor:
    id: int
    label: str
    value: float

export const MAX_SENSORS: int = 32

export proc read(s: Sensor) -> float:
    return s.value

# Private - not exported
proc validate(s: Sensor) -> bool:
    return s.id > 0
```

### 12.3 Imports

```forge
# main.fg

import sensors
import nmea_parser as nmea

proc main() -> void:
    var s: sensors.Sensor = sensors.Sensor {
        id: 1,
        label: "depth",
        value: 12.5
    }
    var v: float = sensors.read(s)
    print(v)
```

### 12.4 Standard Library Imports

```forge
import forge.io
import forge.str
import forge.math
import forge.sys
import forge.time
import forge.buf
import forge.serial
import forge.nmea
```

---

## 13. Channel System

Channels provide loose coupling between modules via message passing.

### 13.1 Declaring Channels

```forge
channel depth_reading: float
channel shutdown: void          # No payload
export channel sensor_alert: str
```

### 13.2 Emitting Messages

```forge
emit depth_reading -> 12.5
emit shutdown                   # Void channel
emit sensor_alert -> "High temperature!"
```

### 13.3 Registering Handlers

```forge
on depth_reading as value:
    print("Depth: " + str(value))

on shutdown:
    cleanup()
    forge.sys.exit(0)
```

### 13.4 Cross-Module Usage

```forge
# display.fg
import sensors

on sensors.reading_available as value:
    update_display(value)
```

---

## 14. Standard Library

### 14.0 Global Built-in Functions

These functions are available everywhere without any import:

| Function | Signature | Description |
|----------|-----------|-------------|
| `print` | `print(val: any) -> void` | Print any value to stdout with a newline |
| `len` | `len(col: any) -> int` | Length of an array, string, or map |
| `str` | `str(val: any) -> str` | Convert any primitive value to its string representation |
| `type` | `type(val: any) -> str` | Return the runtime type name as a string (`"int"`, `"str"`, etc.) |
| `append` | `append(arr: any, val: any) -> void` | Append an element to a dynamic array |
| `swap` | `swap(a: any, b: any) -> void` | Swap two variables in place |

```forge
# swap — swaps two variables in place, any type
var a: int = 1
var b: int = 2
swap(a, b)
print(a)    # 2
print(b)    # 1
```

`swap` accepts any two variables of the same type. Both arguments must be simple variable names. This is the FORGE equivalent of Python's `a, b = b, a`.

### 14.1 `forge.io` — Input/Output

```forge
import forge.io

# Console output
print("Hello")                    # Print with newline
print_raw("No newline")           # Print without newline
eprint("Error message")           # Print to stderr

# Console input
var name: str = read_line()
var age: str = read_line_prompt("Enter age: ")

# File operations
var content: str = read_file("data.txt")
write_file("output.txt", "Hello, file!")
append_file("log.txt", "New entry\n")
var exists: bool = file_exists("config.fg")
```

### 14.2 `forge.str` — String Operations

```forge
import forge.str

var s: str = "  Hello, World!  "

# Length
print(length(s))              # Byte length
print(length_chars(s))        # Unicode codepoint count

# Trimming
var trimmed: str = trim(s)    # "Hello, World!"
var left: str = trim_left(s)
var right: str = trim_right(s)

# Search
print(contains(s, "World"))   # true
print(starts_with(s, "  H"))  # true
print(ends_with(s, "!  "))    # true
var idx: ?int = index_of(s, "World")

# Case conversion
print(to_upper(s))            # "  HELLO, WORLD!  "
print(to_lower(s))            # "  hello, world!  "

# Split and join
var parts: []str = split("a,b,c", ",")  # ["a", "b", "c"]
var joined: str = join(parts, "-")       # "a-b-c"

# Replace
var replaced: str = replace(s, "World", "FORGE")

# Conversion
var num: ?int = to_int("42")
var flt: ?float = to_float("3.14")

# Character access
var ch: byte = char_at("Hello", 0)  # 72 ('H')
var sub: str = substring("Hello", 1, 4)  # "ell"
```

### 14.3 `forge.math` — Mathematics

```forge
import forge.math

# Constants
print(PI)       # 3.14159265358979
print(E)        # 2.71828182845904
print(TAU)      # 6.28318530717959

# Basic math
print(abs(-5.0))            # 5.0
print(abs_int(-5))          # 5
print(min(3.0, 5.0))        # 3.0
print(max(3.0, 5.0))        # 5.0
print(clamp(15.0, 0.0, 10.0))  # 10.0

# Powers and roots
print(pow(2.0, 10.0))       # 1024.0
print(sqrt(16.0))           # 4.0
print(cbrt(27.0))           # 3.0

# Rounding
print(floor(3.7))           # 3.0
print(ceil(3.2))            # 4.0
print(round(3.5))           # 4.0
print(trunc(3.9))           # 3.0

# Trigonometry (radians)
print(sin(PI / 2.0))        # 1.0
print(cos(0.0))             # 1.0
print(tan(PI / 4.0))        # ~1.0
print(atan2(1.0, 1.0))      # ~0.785

# Logarithms and exponentials
print(log(E))               # 1.0
print(log10(100.0))         # 2.0
print(log2(8.0))            # 3.0
print(exp(1.0))             # ~2.718

# Random numbers
seed_random(12345)
var r: int = random_int(1, 100)    # [1, 100)
var f: float = random_float()       # [0.0, 1.0)
```

### 14.4 `forge.sys` — System Interface

```forge
import forge.sys

# Command-line arguments
var arguments: []str = args()    # args()[0] is program name

# Environment variables
var home: ?str = env("HOME")
if home is some:
    print(home.value)

# Exit
exit(0)           # Exit with code 0
halt()            # Same as exit(0)

# Platform info
print(platform()) # "linux", "windows", "macos"
print(arch())     # "x86_64", "arm64", etc.
```

### 14.5 `forge.time` — Time and Timing

```forge
import forge.time

# Current time
var ms: uint = now()              # Unix timestamp in milliseconds
var ts: str = timestamp()         # ISO 8601 formatted

# Sleep
sleep(1000)                       # Sleep 1 second

# Elapsed time
var start: uint = now()
# ... do work ...
var elapsed: uint = elapsed_ms(start)

# Stopwatch
var clock: Clock = start_clock()
# ... do work ...
var lap1: uint = lap(ref clock)   # ms since start/last lap
# ... more work ...
var lap2: uint = lap(ref clock)
```

### 14.6 `forge.buf` — Byte Buffer Operations

```forge
import forge.buf

# Create a buffer
var b: Buffer = create(1024)

# Write operations
write_byte(ref b, 0x42)
write_bytes(ref b, [0x01, 0x02, 0x03])
write_str(ref b, "Hello")
write_int16_le(ref b, 1234)
write_int32_le(ref b, 123456)

# Seek and rewind
rewind(ref b)          # Position = 0
seek(ref b, 10)        # Position = 10

# Read operations
var byte_val: ?byte = read_byte(ref b)
var bytes: ?[]byte = read_bytes(ref b, 4)
var i16: ?int16 = read_int16_le(ref b)
var i32: ?int32 = read_int32_le(ref b)

# Utilities
var rem: int = remaining(b)
var s: str = to_str(b)
var hex: str = to_hex(b)  # "48 65 6C 6C 6F"

# Cleanup
free_buf(ref b)
```

### 14.7 `forge.serial` — Serial Port I/O

```forge
import forge.serial

# Open port
var port: ?Port = open("/dev/ttyUSB0", 4800)
if port is none:
    print("Failed to open port")
    return

var p: Port = port.value

# Configure
set_timeout(ref p, 1000)  # 1 second timeout

# Read
var byte_val: ?byte = read_byte(ref p)
var data: []byte = read_bytes(ref p, 10)
var line: str = read_line(ref p)

# Write
write_byte(ref p, 0x0D)
write_bytes(ref p, [0x01, 0x02, 0x03])
write_str(ref p, "$GPGGA,...")

# Check for data
var available: int = bytes_available(ref p)
flush(ref p)

# Close
close(ref p)
```

### 14.8 `forge.nmea` — NMEA 0183 Parsing

```forge
import forge.nmea

var sentence: str = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,47.0,M,,*47"

# Validate checksum
if not valid_checksum(sentence):
    print("Invalid checksum")
    return

# Get sentence type
var id: str = sentence_id(sentence)  # "GPGGA"

# Get individual fields
var utc: str = field(sentence, 1)    # "123519"

# Parse GGA sentence
var gga: ?GGA = parse_gga(sentence)
if gga is some:
    var lat: float = decimal_degrees(gga.value.latitude, gga.value.lat_dir)
    var lon: float = decimal_degrees(gga.value.longitude, gga.value.lon_dir)
    print("Position: " + str(lat) + ", " + str(lon))

# Parse RMC sentence
var rmc_sentence: str = "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A"
var rmc: ?RMC = parse_rmc(rmc_sentence)
if rmc is some:
    print("Speed: " + str(rmc.value.speed_kts) + " knots")

# Build a sentence
var fields: []str = ["GPGGA", "123519", "4807.038", "N", "01131.000", "E"]
var built: str = build_sentence("GPGGA", fields)
```

---

## 15. Memory Management

For a more detailed beginner walkthrough with extra examples and practice,
see [FORGE_Memory_Management_Guide.md](FORGE_Memory_Management_Guide.md).

### 15.1 Stack Allocation (Default)

Local variables, fixed arrays, and records are stack-allocated:

```forge
proc compute() -> float:
    var x: float = 3.14           # Stack
    var buf: [byte; 64]           # Stack (64 bytes)
    var sensor: Sensor = ...      # Stack
    return x
    # All automatically freed at end of scope
```

### 15.2 Heap Allocation

Dynamic arrays and maps are heap-allocated:

```forge
var data: []byte = alloc([]byte, 1024)
# ... use data ...
free(data)

var m: map[str, int] = {}
# ... use m ...
free(m)
```

### 15.3 Scope-Managed Allocation

Use `with alloc` for automatic cleanup:

```forge
with alloc([]byte, 1024) as buf:
    fill_buffer(buf)
    process(buf)
# buf is automatically freed here
```

---

## 16. Error Handling

### 16.1 Result Records

Define result types for procedures that can fail:

```forge
record ParseResult:
    ok: bool
    value: int
    error: str

proc parse_int(s: str) -> ParseResult:
    # ... parsing logic ...
    if failed:
        return ParseResult { ok: false, value: 0, error: "invalid integer" }
    return ParseResult { ok: true, value: result, error: "" }

proc main() -> void:
    var r: ParseResult = parse_int("abc")
    if not r.ok:
        print("Error: " + r.error)
        return
    print(r.value)
```

### 16.2 Optional Types for Simple Cases

```forge
proc find(arr: []int, target: int) -> ?int:
    for i in range(0, len(arr)):
        if arr[i] == target:
            return some(i)
    return none
```

### 16.3 Panic and Assert

```forge
# Panic for unrecoverable errors
panic("assertion failed: buffer overflow")

# Assert for invariant checking
assert len(buf) > 0
assert divisor != 0, "divisor must not be zero"
```

---

## 17. REPL Guide

### 17.1 Starting the REPL

```bash
$ ./forge repl
FORGE 0.1.0 REPL — type 'exit' or Ctrl-D to quit, 'help' for commands
>>>
```

### 17.2 Features

**Variables persist:**
```
>>> var x: int = 42
>>> x * 2
84
>>> var y: int = x + 10
>>> y
52
```

**Define procedures:**
```
>>> proc double(n: int) -> int:
...     return n * 2
...
>>> double(21)
42
```

**Define records:**
```
>>> record Point:
...     x: int
...     y: int
...
>>> var p = Point { x: 5, y: 10 }
>>> p.x + p.y
15
```

### 17.3 Running a Script then REPL

```bash
$ ./forge run myscript.fg --repl
# Script output...

--- main() finished, entering REPL ---
>>> # All global variables from script are available here
```

### 17.4 REPL Commands

| Command | Description |
|---------|-------------|
| `exit` | Exit the REPL |
| `quit` | Exit the REPL |
| `help` | Show available commands |
| Ctrl-D | Exit the REPL |

---

## 18. Reserved Words

The following identifiers cannot be used as variable, procedure, record, or module names:

```
and         as          bool        break       byte
channel     const       continue    elif        else
emit        export      false       float       float32
for         free        if          import      in
int         int8        int16       int32       is
loop        map         none        not         on
or          or_else     panic       proc        range
record      ref         return      some        str
true        type        uint        uint8       uint16
uint32      var         void        while       with
```

---

## 19. Operator Precedence

Higher level = higher precedence (binds more tightly).

| Level | Operators | Associativity |
|-------|-----------|---------------|
| 1 (lowest) | `or` | left |
| 2 | `and` | left |
| 3 | `not` | right (unary) |
| 4 | `==` `!=` `<` `>` `<=` `>=` | left |
| 5 | `\|` (bitwise OR) | left |
| 6 | `^` (bitwise XOR) | left |
| 7 | `&` (bitwise AND) | left |
| 8 | `<<` `>>` | left |
| 9 | `+` `-` | left |
| 10 | `*` `/` `%` | left |
| 11 | `-` `~` (unary) | right |
| 12 (highest) | `.` `[]` `()` | left |

---

## Appendix: Complete Example Program

```forge
# fibonacci.fg - Calculate Fibonacci numbers

const MAX_FIB: int = 50

proc fib(n: int) -> int:
    if n <= 1:
        return n
    return fib(n - 1) + fib(n - 2)

proc fib_iter(n: int) -> int:
    if n <= 1:
        return n
    var a: int = 0
    var b: int = 1
    var i: int = 2
    while i <= n:
        var tmp: int = a + b
        a = b
        b = tmp
        i = i + 1
    return b

proc main() -> void:
    print("Fibonacci Calculator")
    print("====================")

    for n in range(0, 15):
        var result: int = fib_iter(n)
        print("fib(" + str(n) + ") = " + str(result))

    print("")
    print("Large values (iterative):")
    print("fib(40) = " + str(fib_iter(40)))
    print("fib(45) = " + str(fib_iter(45)))
```

**Running:**

```bash
$ ./forge run fibonacci.fg
Fibonacci Calculator
====================
fib(0) = 0
fib(1) = 1
fib(2) = 1
fib(3) = 2
...
fib(14) = 377

Large values (iterative):
fib(40) = 102334155
fib(45) = 1134903170
```

---

*FORGE Usage Guide v0.1 — Fragillidae Software*


