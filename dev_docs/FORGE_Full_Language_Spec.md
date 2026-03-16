# FORGE Language Specification

**Version:** 0.1 (Initial Design)  
**Status:** Draft  
**Author:** Fragillidae Software  
**Implementation Language:** C  
**File Extension:** `.fg`  
**Toolchain Binary:** `forge`

---

> *FORGE — Fast, Reliable, Organized, General-purpose, Event-driven*  
> A structured, procedural, modular, event-driven language with dual interpreted and compiled execution modes.

---

## Table of Contents

1. [Introduction](#1-introduction)
2. [Lexical Structure](#2-lexical-structure)
3. [Types](#3-types)
4. [Variables and Constants](#4-variables-and-constants)
5. [Expressions](#5-expressions)
6. [Statements and Control Flow](#6-statements-and-control-flow)
7. [Procedures](#7-procedures)
8. [Records](#8-records)
9. [Arrays and Maps](#9-arrays-and-maps)
10. [Module System](#10-module-system)
11. [Channel and Event System](#11-channel-and-event-system)
12. [Memory Model](#12-memory-model)
13. [Error Handling](#13-error-handling)
14. [Standard Library](#14-standard-library)
15. [Toolchain CLI](#15-toolchain-cli)
16. [Dual Execution Architecture](#16-dual-execution-architecture)
17. [Implementation Guide](#17-implementation-guide)
18. [Grammar Reference](#18-grammar-reference)
19. [Appendix A — Reserved Words](#appendix-a--reserved-words)
20. [Appendix B — Operator Precedence](#appendix-b--operator-precedence)
21. [Appendix C — Standard Library Quick Reference](#appendix-c--standard-library-quick-reference)

---

## 1. Introduction

### 1.1 Design Goals

FORGE is designed around five core commitments:

| Goal | Description |
|------|-------------|
| **Explicit over implicit** | Nothing happens behind the scenes. Memory, channels, and module boundaries are all visible in source. |
| **Procedures, not objects** | The fundamental unit of logic is the procedure. Data is structured via records. No classes, no inheritance, no vtables. |
| **Modules as trust boundaries** | Each `.fg` file is a module. Exports are explicit. There is no global namespace pollution. |
| **Messages as coordination** | Cross-module runtime communication happens exclusively through typed, named channels. No shared mutable global state. |
| **One source, two paths** | The same `.fg` source runs interpreted for development and compiles for deployment. No source modification required to switch modes. |

### 1.2 What FORGE Is Not

FORGE deliberately excludes:

- **Object-oriented programming** — no classes, no methods on types, no inheritance, no interfaces, no `self`/`this`
- **Garbage collection** — memory is stack-allocated by default; heap allocation is explicit and manual
- **Implicit type coercion** — numeric types do not silently convert; casts are always explicit
- **Exceptions** — errors are values returned from procedures; there is no `throw`/`catch` mechanism
- **Generics or templates** — the type system is concrete; parameterized types are a potential future extension
- **Closures or first-class functions** — procedures are not values; there are no lambdas

### 1.3 Name and Acronym

**FORGE** — *Fast, Reliable, Organized, General-purpose, Event-driven*

The forge metaphor is intentional: a forge is where raw material is shaped into precise, useful tools through skilled, deliberate work. FORGE the language expects the same from its programmers.

### 1.4 File Conventions

| Item | Convention |
|------|-----------|
| Source files | `name.fg` |
| Module name | Filename without extension (lowercase) |
| Entry point | `proc main() -> void` in the root module |
| Encoding | UTF-8 |
| Line endings | LF (`\n`); CRLF tolerated by lexer |
| Indent unit | 4 spaces (hard rule; tabs are a lexer error) |

---

## 2. Lexical Structure

### 2.1 Character Set

FORGE source files are UTF-8 encoded. Identifiers are restricted to ASCII. String literals may contain any valid UTF-8 sequence.

### 2.2 Indentation and Block Structure

FORGE uses significant indentation. Blocks are opened by a colon (`:`) at the end of a header line and delimited by consistent indentation. The lexer emits synthetic `INDENT` and `DEDENT` tokens.

**Rules:**
- The canonical indent unit is **4 spaces**
- Tabs anywhere in a source file are a **lexer error**
- Mixed indent levels (e.g., 4 then 6) are a **lexer error**
- Blank lines and comment-only lines do not affect indent tracking
- A `DEDENT` to a level that was never opened is a **lexer error**

```
proc example() -> void:          # header ends with ':'
    var x: int = 1                # INDENT emitted here
    if x > 0:                     # nested header
        print("positive")         # second INDENT
    print("done")                 # DEDENT back one level
                                  # DEDENT back to proc level (implicit at EOF or next decl)
```

### 2.3 Comments

```
# This is a line comment. Everything from '#' to end-of-line is ignored.
```

There are no block comments. Use consecutive `#` lines for multi-line comments.

```
# This is a
# multi-line comment
# written as consecutive line comments.
```

### 2.4 Identifiers

```
identifier ::= letter ( letter | digit | '_' )*
letter     ::= [a-zA-Z]
digit      ::= [0-9]
```

**Rules:**
- Must begin with a letter (not a digit or underscore)
- May contain letters, digits, and underscores
- Case-sensitive: `count`, `Count`, and `COUNT` are distinct
- Leading underscore `_name` is a convention for module-private symbols (not enforced by the language, but respected by tooling)
- Reserved words may not be used as identifiers (see [Appendix A](#appendix-a--reserved-words))

**Valid:** `depth`, `sensorID`, `max_retries`, `Port2`, `x`  
**Invalid:** `_start` *(leading underscore — allowed but discouraged)*, `2fast` *(starts with digit)*, `proc` *(reserved)*

### 2.5 Keywords

```
and         as          bool        break       byte
channel     const       continue    elif        else
emit        export      false       float       for
free        if          import      in          int
is          loop        map         none        not
on          or          proc        range       record
return      some        str         true        uint
var         void        while       with
```

### 2.6 Literals

#### 2.6.1 Integer Literals

```
integer_lit ::= decimal_lit | hex_lit | binary_lit | octal_lit
decimal_lit ::= [0-9] ( [0-9] | '_' )*
hex_lit     ::= '0x' [0-9a-fA-F] ( [0-9a-fA-F] | '_' )*
binary_lit  ::= '0b' [01] ( [01] | '_' )*
octal_lit   ::= '0o' [0-7] ( [0-7] | '_' )*
```

Underscore separators are allowed for readability and are ignored by the lexer.

```
1_000_000       # one million
0xFF_A0         # hex with separator
0b1010_0011     # binary byte
0o755           # octal permissions
```

#### 2.6.2 Float Literals

```
float_lit ::= [0-9]+ '.' [0-9]+ ( exponent )?
            | [0-9]+ exponent
exponent  ::= ( 'e' | 'E' ) ( '+' | '-' )? [0-9]+
```

```
3.14
2.998e8
1.0e-9
```

#### 2.6.3 String Literals

Delimited by double quotes. Single quotes are not string delimiters in FORGE.

```
"hello"
"NMEA sentence: $GPGGA"
"line one\nline two"
```

**Escape sequences:**

| Sequence | Meaning |
|----------|---------|
| `\n` | Newline (LF) |
| `\t` | Tab |
| `\r` | Carriage return |
| `\\` | Literal backslash |
| `\"` | Literal double quote |
| `\0` | Null byte |
| `\xNN` | Byte with hex value NN |

Raw strings (no escape processing) use backtick delimiters:

```
`path\to\file`          # backslash is literal
`NMEA: $GPGGA,123`      # dollar sign is literal
```

#### 2.6.4 Boolean Literals

```
true
false
```

#### 2.6.5 The None Literal

```
none
```

Used as the absence-of-value literal for optional types. Not a pointer. Not null. See [Section 3.5](#35-optional-types).

### 2.7 Operators and Punctuation

```
Arithmetic:   +   -   *   /   %
Comparison:   ==  !=  <   >   <=  >=
Logical:      and  or  not
Bitwise:      &   |   ^   ~   <<  >>
Assignment:   =   +=  -=  *=  /=  %=  &=  |=  ^=  <<=  >>=
Arrow:        ->
Range:        ..  ..=
Channel:      ->   (also used in emit)
Access:       .
Optional:     ?   (prefix on type)  is  or_else
Punctuation:  :   ,   (   )   [   ]   {   }
```

### 2.8 Whitespace

Outside of indentation-significant contexts (the start of a logical line), whitespace is insignificant. Spaces and newlines within an expression, argument list, or record literal are ignored.

### 2.9 Line Continuation

A logical line may be continued onto the next physical line by ending with a backslash `\`, or implicitly when an open `(`, `[`, or `{` has not yet been closed.

```
var result: int = some_very_long_function_name(arg1, arg2,
                                               arg3, arg4)

var total: int = value_a \
               + value_b \
               + value_c
```

---

## 3. Types

### 3.1 Primitive Types

| Type | Width | Description |
|------|-------|-------------|
| `int` | 64-bit signed | Default integer type |
| `int8` | 8-bit signed | Range: −128 to 127 |
| `int16` | 16-bit signed | Range: −32768 to 32767 |
| `int32` | 32-bit signed | Range: −2³¹ to 2³¹−1 |
| `uint` | 64-bit unsigned | Default unsigned integer |
| `uint8` | 8-bit unsigned | Range: 0 to 255 |
| `uint16` | 16-bit unsigned | Range: 0 to 65535 |
| `uint32` | 32-bit unsigned | Range: 0 to 2³²−1 |
| `float` | 64-bit IEEE 754 | Double precision |
| `float32` | 32-bit IEEE 754 | Single precision |
| `bool` | 1 byte (impl. defined) | `true` or `false` only |
| `str` | variable | Immutable UTF-8 string, heap allocated |
| `byte` | alias for `uint8` | Preferred for raw data, serial I/O, buffers |

**`bool` is not an integer.** The following is a type error:

```
var b: bool = true
var x: int = b        # ERROR: cannot implicitly convert bool to int
var x: int = int(b)   # OK: explicit cast
```

### 3.2 Void

`void` is not a first-class type. It appears only as the return type of a procedure that produces no value, and in `channel` declarations that carry no payload.

```
proc cleanup() -> void:
    ...

channel shutdown: void
```

### 3.3 Numeric Conversions

All numeric conversions are explicit. There is no implicit promotion or narrowing.

```
var a: int   = 42
var b: float = float(a)     # explicit widening
var c: int32 = int32(a)     # explicit narrowing — may truncate
var d: uint8 = uint8(255)
```

Conversion from float to integer truncates toward zero. Overflow on narrowing conversions wraps (two's complement for signed, modular for unsigned) and is implementation-defined for float-to-integer out-of-range values.

### 3.4 The `byte` Type

`byte` is an alias for `uint8`. It is the preferred type for:
- Serial port buffers
- NMEA sentence raw data
- Network packet payloads
- Any context involving raw binary data

```
var buf: [byte; 256]
var ch: byte = 0x24     # '$' in ASCII
```

### 3.5 Optional Types

FORGE has no null pointer. A value that may be absent uses the `?T` optional type.

```
var found: ?int = search(list, key)

if found is some:
    process(found.value)
else:
    log("not found")

# Unwrap with a default
var val: int = found or_else 0

# Propagate absence (returns none if found is none)
var doubled: ?int = found.map(double)   # future: may not be in v1
```

**Constructing optionals:**

```
var a: ?int = some(42)     # explicitly wrap a value
var b: ?int = none         # explicitly absent
```

An optional type `?T` is distinct from `T` at the type system level. Passing a `?int` where an `int` is expected is a compile-time error. You must unwrap first.

### 3.6 Type Aliases

```
type Degrees = float
type Port    = uint16
type Buffer  = [byte; 512]
```

Aliases are transparent — `Degrees` and `float` are interchangeable at the type level. They exist for documentation and readability.

### 3.7 Sized vs Unsized Types

All types in FORGE are sized at compile time. There are no dynamically-sized types in the stack type system. Dynamic collections (`[]T`, `map[K,V]`) are heap-allocated and accessed through a descriptor (pointer + length + capacity).

---

## 4. Variables and Constants

### 4.1 Variable Declaration

```
var name: Type = expression
var name: Type              # zero-initialized
```

All variables must be explicitly typed. Type inference is not supported in v1.

```
var x: int = 42
var active: bool = true
var label: str = "depth sensor"
var ratio: float             # zero-initialized to 0.0
var count: int               # zero-initialized to 0
```

**Zero values by type:**

| Type | Zero Value |
|------|-----------|
| `int`, `uint`, variants | `0` |
| `float`, `float32` | `0.0` |
| `bool` | `false` |
| `str` | `""` |
| `byte` | `0` |
| `?T` | `none` |
| record | all fields zero-initialized |
| fixed array | all elements zero-initialized |

### 4.2 Constant Declaration

```
const NAME: Type = expression
```

Constants must use SCREAMING_SNAKE_CASE by convention. The value must be a compile-time constant expression (literals and arithmetic on literals only).

```
const MAX_RETRIES: int   = 5
const PI:          float = 3.14159265358979
const BAUD_4800:   uint  = 4800
const BUFFER_SIZE: int   = 512
const APP_NAME:    str   = "FORGE Navigator"
```

Constants are inlined at their use sites. They have no storage address. Taking the address of a constant is a compile-time error.

### 4.3 Scope

Variables are scoped to the block in which they are declared. A block is any indented region following a `:`.

```
proc example() -> void:
    var x: int = 10         # x is in proc scope
    if x > 5:
        var y: int = 20     # y is in if-block scope
        print(x + y)
    print(x)                # OK
    print(y)                # ERROR: y is out of scope
```

Shadowing a variable from an outer scope is allowed but produces a compiler warning.

### 4.4 Assignment

```
name = expression
name += expression
name -= expression
name *= expression
name /= expression
name %= expression
name &= expression
name |= expression
name ^= expression
name <<= expression
name >>= expression
```

Assignment is a statement, not an expression. You cannot write `if (x = compute())`.

---

## 5. Expressions

### 5.1 Arithmetic Expressions

```
a + b       # addition
a - b       # subtraction
a * b       # multiplication
a / b       # division (integer division truncates toward zero)
a % b       # modulo (sign follows dividend)
-a          # unary negation
```

Integer division by zero is a runtime error in interpreted mode and undefined behavior in compiled mode (wraps to implementation behavior). Use explicit checks.

### 5.2 Comparison Expressions

```
a == b      # equal
a != b      # not equal
a < b       # less than
a > b       # greater than
a <= b      # less than or equal
a >= b      # greater than or equal
```

All comparisons return `bool`. Comparing values of different types is a compile-time error.

### 5.3 Logical Expressions

```
a and b     # logical AND (short-circuit)
a or b      # logical OR (short-circuit)
not a       # logical NOT
```

Short-circuit evaluation: in `a and b`, `b` is not evaluated if `a` is `false`. In `a or b`, `b` is not evaluated if `a` is `true`.

### 5.4 Bitwise Expressions

```
a & b       # bitwise AND
a | b       # bitwise OR
a ^ b       # bitwise XOR
~a          # bitwise NOT (ones complement)
a << n      # left shift
a >> n      # right shift (logical, zero-fill for uint; arithmetic for int)
```

Bitwise operators require integer operands. Applying them to `float` or `bool` is a type error.

### 5.5 String Expressions

```
a + b           # string concatenation (produces new string)
str(value)      # convert any primitive to its string representation
len(s)          # length in bytes
```

### 5.6 Record Field Access

```
sensor.value        # field access on a record
sensor.id           # may be chained: device.sensor.id
```

### 5.7 Array and Map Access

```
arr[i]              # array element at index i (zero-based)
map[key]            # map lookup (runtime error if key absent)
```

### 5.8 Procedure Calls

```
result = compute(a, b, c)
display(sensor.value)
```

Procedures are not first-class values. You cannot store a procedure in a variable or pass it as an argument in v1.

### 5.9 Range Expressions

Used in `for` loops. Not a general-purpose value.

```
0..10       # half-open: 0, 1, 2, ..., 9
0..=10      # closed: 0, 1, 2, ..., 10
```

### 5.10 Optional Expressions

```
value is some           # bool: true if optional has a value
value is none           # bool: true if optional is absent
value.value             # unwrap (runtime error if none)
value or_else default   # unwrap with fallback
some(expr)              # wrap a value into optional
```

### 5.11 Cast Expressions

```
int(expr)
float(expr)
uint8(expr)
byte(expr)
str(expr)           # to string representation
```

Casts are explicit and always visible in source. There is no implicit cast anywhere.

---

## 6. Statements and Control Flow

### 6.1 If / Elif / Else

```
if condition:
    body

if condition:
    body
else:
    body

if condition_a:
    body_a
elif condition_b:
    body_b
elif condition_c:
    body_c
else:
    body_default
```

The condition must be of type `bool`. There is no implicit truthiness for integers, pointers, or strings.

```
var count: int = 5
if count:               # ERROR: int is not bool
    ...
if count != 0:          # OK: explicit comparison
    ...
```

### 6.2 While Loop

```
while condition:
    body
```

Executes `body` repeatedly as long as `condition` is `true`. Condition is checked before each iteration.

```
var i: int = 0
while i < 10:
    process(i)
    i += 1
```

### 6.3 For Loop

```
for variable in range(start, stop):
    body

for variable in range(start, stop, step):
    body

for variable in collection:
    body
```

`range(start, stop)` is half-open: `[start, stop)`. `range(start, stop, step)` uses the given step (may be negative for countdown).

```
for i in range(0, 10):          # 0 through 9
    print(i)

for i in range(10, 0, -1):      # 10 down to 1
    print(i)

for item in readings:           # iterate array
    process(item)
```

The loop variable is implicitly declared and scoped to the loop body. Its type is inferred from the collection element type.

### 6.4 Loop (Infinite Loop)

```
loop:
    body
```

An unconditional infinite loop. Must contain a `break` on some code path or the compiler will warn.

```
loop:
    var data: ?Packet = receive()
    if data is none:
        break
    process(data.value)
```

### 6.5 Break and Continue

```
break       # exit the nearest enclosing loop
continue    # skip to the next iteration of the nearest enclosing loop
```

Both are only valid inside `while`, `for`, or `loop` bodies.

### 6.6 Return

```
return                  # return from void procedure
return expression       # return value from typed procedure
```

A non-void procedure must return a value on all code paths. This is enforced at compile time and flagged as a warning in interpreted mode.

### 6.7 With (Scope-Managed Allocation)

```
with alloc(Type, count) as name:
    body
# name is automatically freed here
```

See [Section 12](#12-memory-model) for full details.

---

## 7. Procedures

### 7.1 Declaration Syntax

```
proc name(param: Type, ...) -> ReturnType:
    body
```

```
proc add(a: int, b: int) -> int:
    return a + b

proc greet(name: str) -> void:
    print("Hello, " + name)

proc clamp(val: float, lo: float, hi: float) -> float:
    if val < lo:
        return lo
    if val > hi:
        return hi
    return val
```

### 7.2 Parameters

Parameters are passed **by value** by default. The procedure receives a copy. Modifying a parameter does not affect the caller's variable.

```
proc double(x: int) -> int:
    x = x * 2          # modifies the local copy only
    return x
```

**Reference parameters** use the `ref` qualifier. A `ref` parameter is an alias for the caller's variable. Modifying it modifies the caller's value.

```
proc swap(a: ref int, b: ref int) -> void:
    var tmp: int = a
    a = b
    b = tmp
```

**Calling with ref:**

```
var x: int = 10
var y: int = 20
swap(ref x, ref y)      # caller must use 'ref' keyword explicitly
# x == 20, y == 10
```

Record parameters are passed by value (the whole record is copied). For large records, use `ref` to avoid the copy cost.

### 7.3 Return Types

A procedure returns exactly one value, or `void`. Multiple return values are handled by returning a record.

```
record ParseResult:
    value:   int
    success: bool
    error:   str

proc parse_int(s: str) -> ParseResult:
    # ... attempt to parse ...
    if failed:
        return ParseResult { value: 0, success: false, error: "not a number" }
    return ParseResult { value: result, success: true, error: "" }
```

### 7.4 Procedure Scope

Procedures may be declared at module scope only. Nested procedure declarations are not supported in v1. (Nested `proc` inside another `proc` is a syntax error.)

### 7.5 Recursion

Recursion is permitted. There is no tail-call optimization guarantee in v1.

```
proc factorial(n: int) -> int:
    if n <= 1:
        return 1
    return n * factorial(n - 1)
```

### 7.6 Forward Declarations

Within a module, procedures may call each other regardless of declaration order. The compiler performs a two-pass scan of module-level declarations.

```
proc is_even(n: int) -> bool:
    if n == 0:
        return true
    return is_odd(n - 1)        # OK: is_odd declared later

proc is_odd(n: int) -> bool:
    if n == 0:
        return false
    return is_even(n - 1)
```

### 7.7 The `main` Procedure

Every executable FORGE program must define `main` in its root module:

```
proc main() -> void:
    ...
```

`main` takes no parameters. Command-line arguments are accessed via `forge.sys.args()`. The exit code is set via `forge.sys.exit(code)`.

---

## 8. Records

### 8.1 Declaration

```
record Name:
    field1: Type1
    field2: Type2
    ...
```

Records are plain data containers. They have no methods, no constructors, and no inheritance.

```
record Sensor:
    id:      int
    label:   str
    value:   float
    active:  bool

record Position:
    latitude:  float
    longitude: float
    altitude:  float
    fix_type:  uint8
```

### 8.2 Instantiation

```
var s: Sensor = Sensor {
    id:     1,
    label:  "depth",
    value:  0.0,
    active: true
}
```

All fields must be provided. Partial initialization is a compile-time error. Fields may be listed in any order.

### 8.3 Field Access and Mutation

```
var reading: float = s.value
s.value = 12.5
s.active = false
```

### 8.4 Record Assignment

Assigning a record copies all fields. Records are value types.

```
var a: Sensor = Sensor { id: 1, label: "a", value: 0.0, active: true }
var b: Sensor = a       # b is a full copy of a
b.value = 99.9          # does not affect a
```

### 8.5 Records in Procedures

```
proc update_sensor(ref s: Sensor, new_value: float) -> void:
    s.value = new_value

proc format_sensor(s: Sensor) -> str:
    return s.label + ": " + str(s.value)
```

### 8.6 Nested Records

Records may contain other records as fields.

```
record NavData:
    position: Position
    heading:  float
    speed:    float

var nav: NavData = NavData {
    position: Position { latitude: 40.7128, longitude: -74.0060, altitude: 0.0, fix_type: 3 },
    heading:  275.0,
    speed:    6.5
}

var lat: float = nav.position.latitude
```

### 8.7 Exported Records

```
export record Waypoint:
    name:      str
    latitude:  float
    longitude: float
```

An exported record is accessible to importing modules. Its fields are all accessible — there is no field-level visibility control in v1.

---

## 9. Arrays and Maps

### 9.1 Fixed Arrays

Fixed arrays have a compile-time constant size and are stack-allocated.

```
var readings: [float; 10]           # 10 floats, zero-initialized
var buf: [byte; 512]                # 512-byte buffer
var coords: [float; 3] = [1.0, 2.0, 3.0]
```

**Syntax:** `[ElementType; Size]`

Element access is zero-based:

```
var first: float = readings[0]
readings[9] = 3.14
```

Accessing out-of-bounds index is a runtime error in interpreted mode and undefined behavior in compiled mode (add bounds checking with `--bounds-check` compile flag).

**Array length:**

```
var n: int = len(readings)      # returns 10 (compile-time constant for fixed arrays)
```

### 9.2 Dynamic Arrays

Dynamic arrays are heap-allocated and can grow. They must be explicitly freed (or managed with `with alloc`).

```
var log: []str = []             # empty dynamic array
append(log, "event one")
append(log, "event two")
var n: int = len(log)           # 2
free(log)
```

**Syntax:** `[]ElementType`

Dynamic arrays support:

```
append(arr, value)              # add element to end
len(arr)                        # current element count
cap(arr)                        # current capacity
arr[i]                          # element access (bounds-checked in interp mode)
arr[i] = value                  # element assignment
slice(arr, start, stop)         # returns a view (not a copy) — advanced use
```

### 9.3 Maps

Maps are hash tables mapping keys to values. Heap-allocated, must be freed.

```
var config: map[str, int] = {}
config["timeout"]  = 30
config["retries"]  = 3
config["port"]     = 4800

var t: int = config["timeout"]  # runtime error if key absent
```

**Key types:** `str`, `int`, `uint`, `byte`, `bool`  
**Value types:** any type including records and arrays

**Map operations:**

```
has_key(m, key)             # bool: true if key exists
delete_key(m, key)          # remove a key
len(m)                      # number of key-value pairs
free(m)                     # release heap memory
```

**Safe map access:**

```
var val: ?int = get(config, "missing")   # returns ?int
if val is some:
    use(val.value)
```

### 9.4 Iteration

```
# Iterate array by index
for i in range(0, len(arr)):
    process(arr[i])

# Iterate array by value
for item in arr:
    process(item)

# Iterate map
for key in map_keys(config):
    print(key + " = " + str(config[key]))
```

---
## 10. Module System

### 10.1 Module Identity

Every `.fg` source file is exactly one module. The module name is the filename without its extension, lowercased. There is no explicit `module` declaration statement.

```
sensors.fg      →   module: sensors
nmea_parser.fg  →   module: nmea_parser
main.fg         →   module: main (entry point)
```

Module names must be valid identifiers. Filenames containing characters other than letters, digits, and underscores are disallowed.

### 10.2 Exports

Symbols are private to a module by default. To make a symbol accessible to other modules, prefix its declaration with `export`.

```
# sensors.fg

export record Sensor:
    id:    int
    label: str
    value: float

export const MAX_SENSORS: int = 32

export channel reading_available: float

export proc read(s: Sensor) -> float:
    return s.value

# Private — not exported:
proc validate(s: Sensor) -> bool:
    return s.id > 0
```

Exportable declarations:
- `record`
- `proc`
- `const`
- `type` (alias)
- `channel`

Variables (`var`) at module scope may not be exported. Use procedures to expose module state.

### 10.3 Imports

```
import module_name
import module_name as alias
```

```
import sensors
import nmea_parser as nmea
import forge.io
```

Imported symbols are accessed with dot notation:

```
var s: sensors.Sensor = sensors.Sensor { ... }
var v: float = sensors.read(s)
forge.io.print("value: " + str(v))
```

With alias:
```
import nmea_parser as nmea
var pos: nmea.Position = nmea.parse_gga(sentence)
```

### 10.4 Import Rules

- Circular imports are a compile-time error
- Unused imports are a compile-time warning (and error with `--strict`)
- Importing a module that has no exported symbols is allowed but produces a warning
- Wildcard imports (`import sensors.*`) do not exist in FORGE — all access is qualified

### 10.5 Module Initialization

A module may define an `init` procedure. It is called exactly once, automatically, before any other procedure in the module is called from outside. It is called in import-dependency order (dependencies initialized first).

```
# sensors.fg
var _ready: bool = false

proc init() -> void:
    _ready = true
    forge.io.print("sensors module initialized")
```

`init` takes no parameters and returns `void`. Declaring `init` with any other signature is a compile-time error.

### 10.6 Module Search Path

The toolchain searches for modules in this order:

1. The directory containing the root source file
2. Directories specified with `-I path` flags
3. The FORGE standard library path (`FORGE_LIB` environment variable or installation default)

### 10.7 Standard Library Modules

Standard library modules are prefixed with `forge.`:

```
import forge.io
import forge.str
import forge.math
import forge.serial
import forge.nmea
import forge.time
import forge.sys
import forge.buf
```

See [Section 14](#14-standard-library) for full reference.

---

## 11. Channel and Event System

### 11.1 Design Rationale

Channels are FORGE's coordination primitive. When one module needs to notify others of an event, it emits a message on a named channel. Other modules register handlers that respond to those messages. No shared mutable state is involved.

This model mirrors hardware interrupt dispatch (a signal occurs; registered handlers run) and is a natural fit for sensor data pipelines, UI event loops, protocol parsers, and state machine transitions.

### 11.2 Channel Declaration

```
channel name: PayloadType
channel name: void          # signal with no data
```

Channels are declared at module scope, never inside procedures.

```
channel depth_reading:   float
channel heading_change:  float
channel nmea_sentence:   str
channel system_shutdown: void
channel sensor_fault:    SensorError
```

### 11.3 Emitting Messages

```
emit channel_name -> value      # emit with payload
emit channel_name               # emit void channel (no payload)
```

```
emit depth_reading -> 12.5
emit nmea_sentence -> raw_line
emit system_shutdown
```

`emit` is a statement, not an expression. It does not return a value.

**Cross-module emit:** To emit on another module's exported channel, qualify the channel name:

```
import sensors
emit sensors.reading_available -> 42.0
```

### 11.4 Registering Handlers

```
on channel_name as variable:
    body

on channel_name:                # for void channels
    body
```

Handlers are declared at module scope, never inside procedures.

```
on depth_reading as value:
    display.update_depth(value)
    log("depth: " + str(value))

on nmea_sentence as raw:
    var parsed: ?Position = parse_gga(raw)
    if parsed is some:
        emit position_fix -> parsed.value

on system_shutdown:
    cleanup()
    forge.sys.exit(0)
```

**Cross-module handler registration:**

```
import nmea_parser
import display

on nmea_parser.sentence_ready as raw:
    display.render(raw)
```

### 11.5 Multiple Handlers

Multiple handlers may be registered on the same channel. They are called in registration order (import order, then within-module declaration order).

```
on depth_reading as v:
    logger.record(v)

on depth_reading as v:
    alarm.check(v)      # also called, after logger.record
```

### 11.6 Delivery Semantics

**Interpreted mode:** Synchronous. `emit` blocks until all registered handlers have returned. Handlers execute in the calling goroutine/thread of the emitter.

**Compiled mode (default):** Also synchronous unless `--async-channels` is specified.

**Compiled mode with `--async-channels`:** Messages are placed in a per-channel queue. A scheduler delivers them to handlers. The emitter does not block. Handler execution order across channels is not guaranteed. Within a single channel, delivery order matches emission order (FIFO).

### 11.7 Channel Typing Rules

- The payload type of `emit` must exactly match the channel's declared payload type
- The handler variable type is inferred from the channel's declared payload type
- Re-declaring a channel with a different type in the same module is a compile-time error
- A channel may be declared in only one module (its owner). Other modules import it and register handlers or emit to it

### 11.8 Channel Visibility

Channels obey the same export rules as procedures and records:

```
export channel depth_reading: float     # accessible from any importing module
channel internal_event: int             # private to this module
```

### 11.9 Channels vs Procedure Calls

| | Procedure Call | Channel Emit |
|---|---|---|
| **Direction** | Caller → callee | Emitter → 0..N handlers |
| **Coupling** | Tight (caller names callee) | Loose (emitter doesn't know handlers) |
| **Return value** | Yes | No |
| **Fan-out** | No | Yes (multiple handlers) |
| **Cross-module** | Yes (import required) | Yes (import required) |
| **Timing** | Synchronous | Synchronous (default) or async |

Use procedure calls when you need a return value or tight coupling is correct. Use channels for notifications, events, and loose cross-module coordination.

---

## 12. Memory Model

### 12.1 Stack Allocation

All local variables, fixed arrays, and record instances declared with `var` inside a procedure are stack-allocated. They are created on entry to their scope and destroyed on exit. No explicit management is needed.

```
proc compute() -> float:
    var x: float = 3.14         # stack
    var buf: [byte; 64]         # stack — 64 bytes on the stack frame
    var sensor: Sensor = ...    # stack — sizeof(Sensor) bytes
    return x
    # x, buf, sensor all released here automatically
```

### 12.2 Heap Allocation

Dynamic arrays, maps, and strings longer than compile-time constants are heap-allocated. All heap allocation is explicit.

```
# Explicit allocation
var data: []byte = alloc([]byte, 1024)
# ... use data ...
free(data)          # explicit free — required

var m: map[str, int] = {}   # map is heap-allocated implicitly
# ... use m ...
free(m)
```

`alloc(Type, count)` allocates space for `count` elements of `Type` and returns a zero-initialized dynamic array of that type. It returns `none` if allocation fails (the return type is `?[]Type`).

```
var buf: ?[]byte = alloc([]byte, 4096)
if buf is none:
    log("out of memory")
    return
var data: []byte = buf.value
```

### 12.3 Scope-Managed Allocation

The `with alloc` statement manages the lifetime of a heap allocation automatically:

```
with alloc([]byte, 1024) as buf:
    fill_buffer(buf)
    process(buf)
# buf is automatically freed here, even if the body returns early
```

This is the preferred pattern when the allocation is only needed within a bounded scope. It is implemented as a compile-time transform — not a runtime mechanism — so it carries no overhead beyond the alloc/free calls.

### 12.4 String Memory

String literals are stored in the program's read-only data segment. They do not need to be freed. String operations (concatenation, substring) produce new heap-allocated strings that must be freed if they escape the local scope. In v1, string management is simplified: strings produced by operations are freed at the end of the statement in which they are used, unless assigned to a `var`.

```
print("hello " + name)     # temporary string freed after print() returns
var s: str = "hello " + name   # s is heap-allocated; free(s) when done
```

### 12.5 Module-Level Variables

Variables declared at module scope (outside any procedure) have static storage duration. They exist for the lifetime of the program and are initialized before `main` is called. They are never heap-allocated and never freed.

```
# module-level static
var _port_open: bool = false
var _packet_count: int = 0
```

Module-level variables are private to the module. They cannot be exported. Use procedures to expose or mutate them.

### 12.6 Safety Guidelines

1. Every `alloc` must have a matching `free`, or be managed by `with alloc`
2. Do not free stack-allocated data
3. Do not access memory after `free`
4. Do not store a pointer to a local variable and use it after the scope exits

The compiler (in v1) does not enforce these rules statically. A future version may add ownership tracking. The `forge run` interpreter reports use-after-free and double-free as runtime errors.

---

## 13. Error Handling

### 13.1 Philosophy

FORGE has no exceptions. Errors are values. A procedure that can fail returns a result type that encodes either a valid value or an error description. The caller is forced to handle the error explicitly.

### 13.2 Error Records

Define error results as records:

```
record Result:
    ok:    bool
    value: int
    error: str

proc parse_int(s: str) -> Result:
    # ... attempt parse ...
    if failed:
        return Result { ok: false, value: 0, error: "invalid integer: " + s }
    return Result { ok: true, value: parsed, error: "" }
```

Or use the optional type for simpler absent/present cases:

```
proc find(arr: []int, target: int) -> ?int:
    for i in range(0, len(arr)):
        if arr[i] == target:
            return some(i)
    return none
```

### 13.3 Caller Responsibility

```
var r: Result = parse_int(user_input)
if not r.ok:
    log("parse error: " + r.error)
    return
process(r.value)
```

Ignoring the return value of an error-returning procedure produces a compiler warning. With `--strict`, it is an error.

### 13.4 Panic

For unrecoverable conditions (programming errors, invariant violations), use `panic`:

```
panic("assertion failed: buffer overflow")
panic("unreachable code reached in state machine")
```

`panic` prints the message and a stack trace (in interpreted mode) or aborts the process (in compiled mode). It is not catchable. It is not for normal error flow — only for conditions that represent a bug in the program.

### 13.5 Assert

```
assert condition
assert condition, "message"
```

In interpreted mode and debug builds, `assert` evaluates the condition and panics if false. In optimized compiled builds (`--opt`), asserts are compiled out. Use for invariant checking during development.

```
assert len(buf) > 0, "buffer must not be empty"
assert divisor != 0
```

---

## 14. Standard Library

All standard library modules are in the `forge.*` namespace. They are implemented in FORGE itself (targeting a bootstrap C runtime) and can be used in both interpreted and compiled modes.

### 14.1 `forge.io` — Input/Output

```
forge.io.print(s: str) -> void
    # Print string to stdout followed by newline

forge.io.print_raw(s: str) -> void
    # Print string to stdout with no newline

forge.io.eprint(s: str) -> void
    # Print string to stderr followed by newline

forge.io.read_line() -> str
    # Read one line from stdin (newline stripped)

forge.io.read_line_prompt(prompt: str) -> str
    # Print prompt then read line

forge.io.read_file(path: str) -> Result_Str
    # Read entire file; Result_Str = { ok: bool, value: str, error: str }

forge.io.write_file(path: str, content: str) -> Result_Void
    # Write string to file (creates or overwrites)

forge.io.append_file(path: str, content: str) -> Result_Void
    # Append string to file

forge.io.file_exists(path: str) -> bool
```

### 14.2 `forge.str` — String Operations

```
forge.str.length(s: str) -> int
    # Length in bytes (not Unicode codepoints)

forge.str.length_chars(s: str) -> int
    # Length in Unicode codepoints

forge.str.split(s: str, sep: str) -> []str
    # Split s on separator; caller must free result

forge.str.join(parts: []str, sep: str) -> str
    # Join with separator; caller must free result

forge.str.trim(s: str) -> str
forge.str.trim_left(s: str) -> str
forge.str.trim_right(s: str) -> str

forge.str.contains(s: str, sub: str) -> bool
forge.str.starts_with(s: str, prefix: str) -> bool
forge.str.ends_with(s: str, suffix: str) -> bool

forge.str.index_of(s: str, sub: str) -> ?int
    # Index of first occurrence, or none

forge.str.to_upper(s: str) -> str
forge.str.to_lower(s: str) -> str

forge.str.replace(s: str, old: str, new: str) -> str

forge.str.format(template: str, ...) -> str
    # printf-style formatting: %d %f %s %x %b

forge.str.to_int(s: str) -> ?int
forge.str.to_float(s: str) -> ?float

forge.str.char_at(s: str, i: int) -> byte
    # Byte value at index i

forge.str.substring(s: str, start: int, stop: int) -> str
```

### 14.3 `forge.math` — Mathematics

```
forge.math.abs(x: float) -> float
forge.math.abs_int(x: int) -> int

forge.math.min(a: float, b: float) -> float
forge.math.max(a: float, b: float) -> float
forge.math.min_int(a: int, b: int) -> int
forge.math.max_int(a: int, b: int) -> int

forge.math.clamp(val: float, lo: float, hi: float) -> float

forge.math.pow(base: float, exp: float) -> float
forge.math.sqrt(x: float) -> float
forge.math.cbrt(x: float) -> float

forge.math.floor(x: float) -> float
forge.math.ceil(x: float) -> float
forge.math.round(x: float) -> float
forge.math.trunc(x: float) -> float

forge.math.sin(x: float) -> float     # x in radians
forge.math.cos(x: float) -> float
forge.math.tan(x: float) -> float
forge.math.atan2(y: float, x: float) -> float

forge.math.log(x: float) -> float     # natural log
forge.math.log10(x: float) -> float
forge.math.log2(x: float) -> float
forge.math.exp(x: float) -> float

forge.math.PI:  float   # 3.14159265358979323846
forge.math.E:   float   # 2.71828182845904523536
forge.math.TAU: float   # 6.28318530717958647692

forge.math.random_int(lo: int, hi: int) -> int
    # Uniform random integer in [lo, hi)
forge.math.random_float() -> float
    # Uniform random float in [0.0, 1.0)
forge.math.seed_random(seed: uint) -> void
```

### 14.4 `forge.sys` — System Interface

```
forge.sys.args() -> []str
    # Command-line arguments; args()[0] is the program name

forge.sys.env(name: str) -> ?str
    # Get environment variable value

forge.sys.exit(code: int) -> void
    # Terminate process with exit code

forge.sys.halt() -> void
    # Terminate process with code 0

forge.sys.platform() -> str
    # Returns "linux", "windows", "macos", "embedded"

forge.sys.arch() -> str
    # Returns "x86_64", "arm64", "riscv32", etc.
```

### 14.5 `forge.time` — Time and Timing

```
forge.time.now() -> uint
    # Unix timestamp in milliseconds

forge.time.sleep(ms: uint) -> void
    # Sleep for ms milliseconds

forge.time.timestamp() -> str
    # ISO 8601 formatted current timestamp

forge.time.elapsed_ms(start: uint) -> uint
    # Milliseconds since start (from forge.time.now())

record forge.time.Clock:
    start_ms: uint

forge.time.start_clock() -> forge.time.Clock
forge.time.lap(c: ref forge.time.Clock) -> uint
    # Milliseconds since last lap (or start)
```

### 14.6 `forge.buf` — Byte Buffer Operations

```
record forge.buf.Buffer:
    data:     []byte
    length:   int
    capacity: int
    position: int

forge.buf.create(capacity: int) -> forge.buf.Buffer
forge.buf.free_buf(b: ref forge.buf.Buffer) -> void

forge.buf.write_byte(b: ref forge.buf.Buffer, v: byte) -> void
forge.buf.write_bytes(b: ref forge.buf.Buffer, data: []byte) -> void
forge.buf.write_str(b: ref forge.buf.Buffer, s: str) -> void
forge.buf.write_int16_le(b: ref forge.buf.Buffer, v: int16) -> void
forge.buf.write_int32_le(b: ref forge.buf.Buffer, v: int32) -> void
forge.buf.write_float32_le(b: ref forge.buf.Buffer, v: float32) -> void

forge.buf.read_byte(b: ref forge.buf.Buffer) -> ?byte
forge.buf.read_bytes(b: ref forge.buf.Buffer, n: int) -> ?[]byte
forge.buf.read_int16_le(b: ref forge.buf.Buffer) -> ?int16
forge.buf.read_int32_le(b: ref forge.buf.Buffer) -> ?int32

forge.buf.seek(b: ref forge.buf.Buffer, pos: int) -> void
forge.buf.rewind(b: ref forge.buf.Buffer) -> void
forge.buf.remaining(b: forge.buf.Buffer) -> int
forge.buf.to_str(b: forge.buf.Buffer) -> str
forge.buf.to_hex(b: forge.buf.Buffer) -> str
    # Hex dump: "48 65 6C 6C 6F"
```

### 14.7 `forge.serial` — Serial Port I/O

```
record forge.serial.Port:
    fd:    int      # file descriptor (platform handle)
    baud:  uint
    open:  bool

forge.serial.open(path: str, baud: uint) -> ?forge.serial.Port
    # e.g. open("/dev/ttyS0", 4800)

forge.serial.close(ref p: forge.serial.Port) -> void

forge.serial.read_byte(ref p: forge.serial.Port) -> ?byte
    # Non-blocking; returns none if no byte available

forge.serial.read_bytes(ref p: forge.serial.Port, n: int) -> []byte
    # Blocking; reads exactly n bytes

forge.serial.read_line(ref p: forge.serial.Port) -> str
    # Reads until '\n'; strips '\r\n'

forge.serial.write_byte(ref p: forge.serial.Port, b: byte) -> void
forge.serial.write_bytes(ref p: forge.serial.Port, data: []byte) -> void
forge.serial.write_str(ref p: forge.serial.Port, s: str) -> void

forge.serial.set_timeout(ref p: forge.serial.Port, ms: uint) -> void
forge.serial.flush(ref p: forge.serial.Port) -> void
forge.serial.bytes_available(ref p: forge.serial.Port) -> int
```

### 14.8 `forge.nmea` — NMEA 0183 Utilities

```
forge.nmea.valid_checksum(sentence: str) -> bool
    # Validates the *XX checksum at end of NMEA sentence

forge.nmea.compute_checksum(sentence: str) -> byte
    # Computes XOR checksum of chars between '$' and '*'

forge.nmea.sentence_id(sentence: str) -> str
    # Returns "GPGGA", "GPRMC", etc.

forge.nmea.field(sentence: str, index: int) -> str
    # Returns comma-delimited field at index (0 = sentence id)

record forge.nmea.GGA:
    utc_time:   str
    latitude:   float
    lat_dir:    str     # "N" or "S"
    longitude:  float
    lon_dir:    str     # "E" or "W"
    fix_type:   int     # 0=invalid, 1=GPS, 2=DGPS
    satellites: int
    hdop:       float
    altitude:   float
    alt_unit:   str

forge.nmea.parse_gga(sentence: str) -> ?forge.nmea.GGA

record forge.nmea.RMC:
    utc_time:   str
    status:     str     # "A"=active, "V"=void
    latitude:   float
    lat_dir:    str
    longitude:  float
    lon_dir:    str
    speed_kts:  float
    track_deg:  float
    date:       str

forge.nmea.parse_rmc(sentence: str) -> ?forge.nmea.RMC

forge.nmea.decimal_degrees(nmea_coord: float, direction: str) -> float
    # Convert NMEA ddmm.mmmm + "N"/"S"/"E"/"W" to signed decimal degrees

forge.nmea.build_sentence(id: str, fields: []str) -> str
    # Build a valid NMEA sentence with correct checksum
```

---

## 15. Toolchain CLI

The `forge` binary is the single entry point for all FORGE toolchain operations.

### 15.1 Command Summary

```
forge run    <file.fg> [flags]       Interpret and execute
forge build  <file.fg> [flags]       Compile to binary
forge check  <file.fg> [flags]       Type-check only (no execution)
forge fmt    <file.fg>               Format source in place
forge doc    <file.fg> [flags]       Generate module documentation
forge repl   [file.fg]               Start interactive REPL
forge version                        Print toolchain version
forge help   [command]               Show help
```

### 15.2 `forge run`

```
forge run main.fg
forge run main.fg --repl          # drop into REPL after main() returns
forge run main.fg --trace         # print each statement as it executes
forge run main.fg --no-color      # disable colored terminal output
```

Interprets the program starting at `main()` in `main.fg`. All imported modules are loaded from the search path. Errors are reported with file, line, and column. Runtime errors include a stack trace.

### 15.3 `forge build`

```
forge build main.fg
forge build main.fg --target c              # transpile to C (default)
forge build main.fg --target llvm           # emit LLVM IR
forge build main.fg --out ./bin/myapp       # output path
forge build main.fg --opt                   # enable optimizations
forge build main.fg --debug                 # include debug symbols
forge build main.fg --bounds-check          # enable array bounds checking
forge build main.fg --async-channels        # enable async channel delivery
forge build main.fg --strict                # warnings are errors
forge build main.fg --cc clang             # specify C compiler (default: gcc)
forge build main.fg --arch arm64           # cross-compile target arch
forge build main.fg --os linux             # cross-compile target OS
```

**C transpilation pipeline:**

```
source.fg  →  [FORGE frontend]  →  AST
                                    ↓
                              [C emitter]
                                    ↓
              output.c  →  [gcc/clang]  →  binary
```

The intermediate `.c` file is preserved in `./forge-build/` for inspection.

**LLVM IR pipeline:**

```
source.fg  →  [FORGE frontend]  →  AST
                                    ↓
                             [LLVM emitter]
                                    ↓
              output.ll  →  [opt]  →  [llc]  →  binary
```

### 15.4 `forge check`

Runs the full frontend (lexer → parser → type checker) without executing or emitting code. Exits 0 on success, non-zero on error. Suitable for use in CI pipelines and editor integrations.

### 15.5 `forge fmt`

Formats FORGE source to canonical style:
- 4-space indentation
- One blank line between top-level declarations
- Two blank lines between procedures
- Spaces around binary operators
- No trailing whitespace
- Consistent spacing inside record literals and argument lists

Writes back to the original file. Use `--check` to exit non-zero if the file would change (for CI).

### 15.6 `forge repl`

Starts an interactive read-eval-print loop. Useful for quick experiments and teaching.

```
FORGE 0.1 REPL — type 'exit' to quit, 'help' for commands
>>> var x: int = 42
>>> x * 2
84
>>> proc double(n: int) -> int:
...     return n * 2
...
>>> double(21)
42
>>> exit
```

If a file is provided, it is loaded first and its exported symbols are available in the REPL.

### 15.7 `forge doc`

Generates documentation from exported symbols and their preceding `#` comments.

```
# sensors.fg
# Reads the current value from the sensor.
# Returns -1.0 if the sensor is inactive.
export proc read(s: Sensor) -> float:
    ...
```

Output formats: `--format md` (default), `--format html`, `--format text`.

---

## 16. Dual Execution Architecture

### 16.1 Single Frontend

Both execution paths share an identical frontend:

```
Source (.fg)
    │
    ▼
┌─────────┐
│  Lexer  │   Tokenizes source; emits INDENT/DEDENT; rejects tabs
└────┬────┘
     │  Token stream
     ▼
┌─────────┐
│ Parser  │   Recursive descent; builds typed AST
└────┬────┘
     │  Abstract Syntax Tree
     ▼
┌──────────────┐
│ Type Checker │   Resolves types; validates all expressions
└────┬─────────┘
     │  Annotated AST
     ├──────────────────┐
     ▼                  ▼
┌─────────────┐   ┌──────────────┐
│ Interpreter │   │ Code Emitter │
│ (tree walk) │   │ (C or LLVM)  │
└─────────────┘   └──────────────┘
```

The AST is the single source of truth. It is fully type-annotated before either backend sees it.

### 16.2 Interpreter Backend

The interpreter is a tree-walking evaluator implemented in C. It maintains:

- **Environment stack:** A linked list of scope frames, each holding a hash map of name → value bindings
- **Value representation:** A tagged union (`forge_value_t`) representing any FORGE value at runtime
- **Channel registry:** A hash map of channel name → linked list of handler procedure pointers
- **Call stack:** An explicit stack of activation records for error reporting and stack traces

Interpreted mode prioritizes developer experience over speed: rich error messages, full stack traces, bounds checking on arrays, use-after-free detection, and complete runtime type information.

### 16.3 C Emitter Backend

The C emitter performs a single pass over the annotated AST, emitting a single `.c` file. Mapping:

| FORGE construct | C equivalent |
|----------------|-------------|
| `record` | `typedef struct { ... } Name_t;` |
| `proc` | `static ReturnType name(ParamType param, ...)` |
| `export proc` | `ReturnType name(...)` (no `static`) |
| `channel` | `typedef void (*name_handler_t)(PayloadType); name_handler_t name_handlers[MAX_HANDLERS]; int name_handler_count;` |
| `emit` | Dispatch loop calling each registered handler |
| `on` | Registration call in module `init_` function |
| `var` (local) | Local variable declaration |
| `var` (module) | `static` global variable |
| `[]T` | `forge_dynarray_t` (runtime struct: `void* data, int len, int cap, size_t elem_size`) |
| `map[K,V]` | `forge_map_t` (open-addressing hash table) |
| `?T` | `forge_optional_t` (tagged union: `bool present; T value;`) |
| `alloc` | `forge_alloc(...)` (thin wrapper over `malloc`) |
| `free` | `forge_free(...)` |

A small `forge_runtime.c` / `forge_runtime.h` provides the runtime support types and functions. It is compiled alongside the emitted code.

### 16.4 Behavioral Parity

The interpreter and compiler must produce identical observable behavior for all valid FORGE programs, with these defined exceptions:

| Behavior | Interpreter | Compiled (default) |
|----------|-------------|-------------------|
| Array out-of-bounds | Runtime error, stack trace | Undefined (enable with `--bounds-check`) |
| Integer overflow | Wraps with warning | Wraps silently |
| Use-after-free | Runtime error | Undefined behavior |
| Stack overflow | Runtime error | Crash (OS signal) |
| Uninitialized variable | Runtime error (zero-init with warning) | Zero-initialized |

---

## 17. Implementation Guide

This section provides guidance for implementing the FORGE toolchain in C.

### 17.1 Recommended Build Order

```
Phase 1 — Lexer
Phase 2 — Parser / AST
Phase 3 — Tree-Walking Interpreter
Phase 4 — Type Checker
Phase 5a — C Emitter
Phase 5b — LLVM IR Emitter (optional / later)
```

Build Phase 1–3 first. This gives a fully working FORGE interpreter that can run real programs before any compilation work begins.

### 17.2 Lexer Implementation

**Token types (enum):**

```c
typedef enum {
    /* Literals */
    TOK_INT_LIT, TOK_FLOAT_LIT, TOK_STR_LIT, TOK_BOOL_LIT,
    /* Identifiers and keywords */
    TOK_IDENT,
    TOK_AND, TOK_AS, TOK_BOOL, TOK_BREAK, TOK_BYTE,
    TOK_CHANNEL, TOK_CONST, TOK_CONTINUE, TOK_ELIF, TOK_ELSE,
    TOK_EMIT, TOK_EXPORT, TOK_FALSE, TOK_FLOAT, TOK_FOR,
    TOK_FREE, TOK_IF, TOK_IMPORT, TOK_IN, TOK_INT,
    TOK_IS, TOK_LOOP, TOK_MAP, TOK_NONE, TOK_NOT,
    TOK_ON, TOK_OR, TOK_PROC, TOK_RANGE, TOK_RECORD,
    TOK_RETURN, TOK_SOME, TOK_STR, TOK_TRUE, TOK_UINT,
    TOK_VAR, TOK_VOID, TOK_WHILE, TOK_WITH,
    /* Operators */
    TOK_PLUS, TOK_MINUS, TOK_STAR, TOK_SLASH, TOK_PERCENT,
    TOK_EQ, TOK_NEQ, TOK_LT, TOK_GT, TOK_LEQ, TOK_GEQ,
    TOK_AMP, TOK_PIPE, TOK_CARET, TOK_TILDE, TOK_LSHIFT, TOK_RSHIFT,
    TOK_ASSIGN, TOK_PLUS_EQ, TOK_MINUS_EQ, /* ... etc ... */
    TOK_ARROW,          /* -> */
    TOK_DOTDOT,         /* .. */
    TOK_DOTDOT_EQ,      /* ..= */
    TOK_DOT, TOK_COMMA, TOK_COLON, TOK_QUESTION,
    TOK_LPAREN, TOK_RPAREN, TOK_LBRACKET, TOK_RBRACKET,
    TOK_LBRACE, TOK_RBRACE,
    /* Indentation */
    TOK_INDENT, TOK_DEDENT, TOK_NEWLINE,
    /* Meta */
    TOK_EOF, TOK_ERROR
} forge_token_type_t;

typedef struct {
    forge_token_type_t type;
    const char*        start;       /* pointer into source buffer */
    int                length;
    int                line;
    int                column;
    union {
        long long  int_val;
        double     float_val;
    } literal;
} forge_token_t;
```

**Indentation tracking:**

```c
/* Maintain a stack of indent levels */
int indent_stack[256];
int indent_depth = 0;
indent_stack[0] = 0;

/* At start of each logical line: */
int current_indent = count_leading_spaces(line);
if (current_indent > indent_stack[indent_depth]) {
    indent_stack[++indent_depth] = current_indent;
    emit_token(TOK_INDENT);
} else {
    while (indent_depth > 0 && indent_stack[indent_depth] > current_indent) {
        --indent_depth;
        emit_token(TOK_DEDENT);
    }
    if (indent_stack[indent_depth] != current_indent) {
        lexer_error("inconsistent indentation");
    }
}
```

### 17.3 AST Node Design

Use a tagged union for AST nodes:

```c
typedef enum {
    NODE_PROGRAM, NODE_MODULE,
    /* Declarations */
    NODE_PROC_DECL, NODE_RECORD_DECL, NODE_VAR_DECL, NODE_CONST_DECL,
    NODE_CHANNEL_DECL, NODE_ON_HANDLER, NODE_TYPE_ALIAS,
    /* Statements */
    NODE_BLOCK, NODE_IF, NODE_WHILE, NODE_FOR, NODE_LOOP,
    NODE_RETURN, NODE_BREAK, NODE_CONTINUE, NODE_EMIT,
    NODE_ASSIGN, NODE_WITH_ALLOC, NODE_PANIC, NODE_ASSERT,
    /* Expressions */
    NODE_INT_LIT, NODE_FLOAT_LIT, NODE_STR_LIT, NODE_BOOL_LIT, NODE_NONE_LIT,
    NODE_IDENT, NODE_QUALIFIED_IDENT,   /* module.name */
    NODE_BINARY_OP, NODE_UNARY_OP,
    NODE_CALL, NODE_FIELD_ACCESS, NODE_INDEX,
    NODE_RECORD_LITERAL, NODE_ARRAY_LITERAL,
    NODE_CAST, NODE_OPTIONAL_WRAP,      /* some(x) */
    NODE_OR_ELSE,
    NODE_IS_SOME, NODE_IS_NONE,
    NODE_RANGE,
} forge_node_type_t;

typedef struct forge_node forge_node_t;

struct forge_node {
    forge_node_type_t type;
    int               line;
    int               column;
    forge_type_t*     resolved_type;    /* filled by type checker */
    union {
        /* NODE_INT_LIT */   long long    int_val;
        /* NODE_FLOAT_LIT */ double       float_val;
        /* NODE_STR_LIT */   const char*  str_val;
        /* NODE_BOOL_LIT */  int          bool_val;
        /* NODE_IDENT */     const char*  name;
        struct {             /* NODE_BINARY_OP */
            int             op;         /* TOK_PLUS, TOK_EQ, etc. */
            forge_node_t*   left;
            forge_node_t*   right;
        } binop;
        struct {             /* NODE_PROC_DECL */
            const char*     name;
            forge_param_t*  params;
            int             param_count;
            forge_type_t*   return_type;
            forge_node_t*   body;       /* NODE_BLOCK */
            int             exported;
        } proc;
        struct {             /* NODE_IF */
            forge_node_t*   condition;
            forge_node_t*   then_body;
            forge_node_t**  elif_conditions;
            forge_node_t**  elif_bodies;
            int             elif_count;
            forge_node_t*   else_body;  /* may be NULL */
        } if_stmt;
        struct {             /* NODE_CALL */
            forge_node_t*   callee;
            forge_node_t**  args;
            int             arg_count;
        } call;
        struct {             /* NODE_BLOCK */
            forge_node_t**  stmts;
            int             count;
        } block;
        /* ... other union arms ... */
    };
};
```

### 17.4 Interpreter Environment

```c
typedef struct forge_env forge_env_t;

struct forge_env {
    struct {
        const char*    name;
        forge_value_t  value;
    }          bindings[256];
    int        count;
    forge_env_t* parent;    /* enclosing scope */
};

forge_value_t env_get(forge_env_t* env, const char* name);
void          env_set(forge_env_t* env, const char* name, forge_value_t val);
forge_env_t*  env_push(forge_env_t* parent);
void          env_pop(forge_env_t* env);
```

### 17.5 Value Representation

```c
typedef enum {
    VAL_INT, VAL_UINT, VAL_FLOAT, VAL_BOOL, VAL_STR,
    VAL_BYTE, VAL_RECORD, VAL_ARRAY, VAL_MAP,
    VAL_OPTIONAL, VAL_VOID, VAL_NONE
} forge_val_type_t;

typedef struct {
    forge_val_type_t type;
    union {
        long long     i;
        unsigned long long u;
        double        f;
        int           b;
        struct { char* data; int len; } str;
        struct { forge_value_t* fields; int count; } record;
        struct { forge_value_t* elems;  int len; int cap; } array;
        void*         map_ptr;
        struct { int present; struct forge_value* inner; } optional;
    };
} forge_value_t;
```

### 17.6 Channel Registry

```c
#define MAX_HANDLERS 64

typedef struct {
    const char*          name;
    forge_proc_node_t*   handlers[MAX_HANDLERS];
    int                  handler_count;
} forge_channel_t;

typedef struct {
    forge_channel_t channels[256];
    int             count;
} forge_channel_registry_t;

void channel_register(forge_channel_registry_t* reg,
                      const char* name,
                      forge_proc_node_t* handler);

void channel_emit(forge_channel_registry_t* reg,
                  forge_env_t* env,
                  const char* name,
                  forge_value_t* payload);
```

---

## 18. Grammar Reference

Formal grammar in EBNF notation. `*` = zero or more, `+` = one or more, `?` = zero or one, `|` = alternative, `( )` = grouping.

```ebnf
program         ::= ( import_decl | top_level_decl )* EOF

import_decl     ::= 'import' qualified_name ( 'as' IDENT )? NEWLINE

top_level_decl  ::= ( 'export' )? ( proc_decl
                                   | record_decl
                                   | channel_decl
                                   | const_decl
                                   | type_alias
                                   | on_handler
                                   | var_decl )

proc_decl       ::= 'proc' IDENT '(' param_list? ')' '->' type ':' NEWLINE block

param_list      ::= param ( ',' param )*
param           ::= ( 'ref' )? IDENT ':' type

record_decl     ::= 'record' IDENT ':' NEWLINE INDENT field_decl+ DEDENT

field_decl      ::= IDENT ':' type NEWLINE

channel_decl    ::= 'channel' IDENT ':' type NEWLINE

on_handler      ::= 'on' qualified_name ( 'as' IDENT )? ':' NEWLINE block

const_decl      ::= 'const' IDENT ':' type '=' expr NEWLINE

type_alias      ::= 'type' IDENT '=' type NEWLINE

var_decl        ::= 'var' IDENT ':' type ( '=' expr )? NEWLINE

block           ::= INDENT stmt+ DEDENT

stmt            ::= var_decl
                  | assign_stmt
                  | if_stmt
                  | while_stmt
                  | for_stmt
                  | loop_stmt
                  | return_stmt
                  | break_stmt
                  | continue_stmt
                  | emit_stmt
                  | with_stmt
                  | panic_stmt
                  | assert_stmt
                  | expr_stmt        (* procedure call as statement *)

assign_stmt     ::= qualified_name ( '[' expr ']' )? ( '.' IDENT )* assign_op expr NEWLINE
assign_op       ::= '=' | '+=' | '-=' | '*=' | '/=' | '%=' | '&=' | '|='
                  | '^=' | '<<=' | '>>='

if_stmt         ::= 'if' expr ':' NEWLINE block
                    ( 'elif' expr ':' NEWLINE block )*
                    ( 'else' ':' NEWLINE block )?

while_stmt      ::= 'while' expr ':' NEWLINE block

for_stmt        ::= 'for' IDENT 'in' ( range_expr | expr ) ':' NEWLINE block
range_expr      ::= 'range' '(' expr ',' expr ( ',' expr )? ')'

loop_stmt       ::= 'loop' ':' NEWLINE block

return_stmt     ::= 'return' expr? NEWLINE

break_stmt      ::= 'break' NEWLINE

continue_stmt   ::= 'continue' NEWLINE

emit_stmt       ::= 'emit' qualified_name ( '->' expr )? NEWLINE

with_stmt       ::= 'with' 'alloc' '(' type ',' expr ')' 'as' IDENT ':' NEWLINE block

panic_stmt      ::= 'panic' '(' expr ')' NEWLINE

assert_stmt     ::= 'assert' expr ( ',' expr )? NEWLINE

expr_stmt       ::= call_expr NEWLINE

expr            ::= or_expr
or_expr         ::= and_expr ( 'or' and_expr )*
and_expr        ::= not_expr ( 'and' not_expr )*
not_expr        ::= 'not' not_expr | cmp_expr
cmp_expr        ::= bitor_expr ( ( '==' | '!=' | '<' | '>' | '<=' | '>=' ) bitor_expr )*
bitor_expr      ::= bitxor_expr ( '|' bitxor_expr )*
bitxor_expr     ::= bitand_expr ( '^' bitand_expr )*
bitand_expr     ::= shift_expr ( '&' shift_expr )*
shift_expr      ::= add_expr ( ( '<<' | '>>' ) add_expr )*
add_expr        ::= mul_expr ( ( '+' | '-' ) mul_expr )*
mul_expr        ::= unary_expr ( ( '*' | '/' | '%' ) unary_expr )*
unary_expr      ::= ( '-' | '~' ) unary_expr | postfix_expr
postfix_expr    ::= primary_expr ( '.' IDENT | '[' expr ']' | '(' arg_list? ')' )*

primary_expr    ::= INT_LIT | FLOAT_LIT | STR_LIT | 'true' | 'false' | 'none'
                  | IDENT | qualified_name
                  | '(' expr ')'
                  | record_literal
                  | array_literal
                  | cast_expr
                  | optional_expr
                  | or_else_expr
                  | is_expr

qualified_name  ::= IDENT ( '.' IDENT )*

record_literal  ::= IDENT '{' ( IDENT ':' expr ',' )* IDENT ':' expr ','? '}'

array_literal   ::= '[' ( expr ',' )* expr? ']'

cast_expr       ::= type '(' expr ')'

optional_expr   ::= 'some' '(' expr ')'

or_else_expr    ::= expr 'or_else' expr

is_expr         ::= expr 'is' ( 'some' | 'none' )

arg_list        ::= ( 'ref' )? expr ( ',' ( 'ref' )? expr )*

type            ::= primitive_type
                  | '?' type
                  | '[' type ';' INT_LIT ']'   (* fixed array *)
                  | '[' ']' type               (* dynamic array *)
                  | 'map' '[' type ',' type ']'
                  | qualified_name             (* record or alias *)

primitive_type  ::= 'int' | 'int8' | 'int16' | 'int32'
                  | 'uint' | 'uint8' | 'uint16' | 'uint32'
                  | 'float' | 'float32'
                  | 'bool' | 'str' | 'byte' | 'void'
```

---

## Appendix A — Reserved Words

The following identifiers are reserved and may not be used as variable, procedure, record, or module names:

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

## Appendix B — Operator Precedence

Higher number = higher precedence (binds more tightly).

| Level | Operators | Associativity |
|-------|-----------|---------------|
| 1 (lowest) | `or` | left |
| 2 | `and` | left |
| 3 | `not` | right (unary) |
| 4 | `==` `!=` `<` `>` `<=` `>=` | left (non-associative) |
| 5 | `\|` | left |
| 6 | `^` | left |
| 7 | `&` | left |
| 8 | `<<` `>>` | left |
| 9 | `+` `-` | left |
| 10 | `*` `/` `%` | left |
| 11 | `-` `~` `not` | right (unary) |
| 12 (highest) | `.` `[]` `()` | left |

`or_else` and `is` are postfix/infix expressions parsed at a level between 3 and 4.

---

## Appendix C — Standard Library Quick Reference

```
forge.io      print, print_raw, eprint, read_line, read_file, write_file
forge.str     length, split, join, trim, contains, starts_with, ends_with,
              index_of, to_upper, to_lower, replace, format, to_int, to_float,
              char_at, substring
forge.math    abs, min, max, clamp, pow, sqrt, floor, ceil, round,
              sin, cos, tan, atan2, log, log10, exp, PI, E, TAU,
              random_int, random_float, seed_random
forge.sys     args, env, exit, halt, platform, arch
forge.time    now, sleep, timestamp, elapsed_ms, start_clock, lap
forge.buf     Buffer, create, free_buf, write_byte, write_bytes, write_str,
              write_int16_le, write_int32_le, read_byte, read_bytes,
              seek, rewind, remaining, to_str, to_hex
forge.serial  Port, open, close, read_byte, read_bytes, read_line,
              write_byte, write_bytes, write_str, set_timeout, flush,
              bytes_available
forge.nmea    valid_checksum, compute_checksum, sentence_id, field,
              GGA, parse_gga, RMC, parse_rmc, decimal_degrees, build_sentence
```

---

*FORGE Language Specification v0.1 — Fragillidae Software — Confidential*
