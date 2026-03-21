# FORGE Quick Reference

**FORGE — Fast, Reliable, Organized, General-purpose, Event-driven**  
File extension: `.fg` | Toolchain binary: `forge` | Version: 0.1

---

## CLI Commands

| Command | Description |
|---------|-------------|
| `forge run <file.fg>` | Interpret and execute |
| `forge build <file.fg> -o <out>` | Compile to native binary |
| `forge check <file.fg>` | Type-check only (no execution) |
| `forge fmt <file.fg>` | Format source in place |
| `forge emit <file.fg>` | Output generated C code |
| `forge repl` | Interactive REPL |
| `forge --version` | Show version |

**`forge run` flags:** `--trace`, `--repl`, `--no-color`, `-v`, `-q`  
**`forge build` flags:** `-O0`–`-O3`, `--bounds-check`, `--async-channels`, `--strict`, `--target c|llvm`

---

## File Structure

```
# comments use '#'
# No block comments — use consecutive '#' lines

const MAX_VALUE: int = 100    # module-level constant

record Point:                 # module-level record
    x: int
    y: int

proc helper() -> void:        # module-level proc
    print("helper")

proc main() -> void:          # entry point
    helper()
```

---

## Indentation Rules

- **4 spaces per level** — tabs are a lexer error
- Blocks begin after `:` and are defined by indentation
- Blank lines and comment-only lines don't affect indentation tracking

---

## Types

### Primitive Types

| Type | Description | Zero Value |
|------|-------------|------------|
| `int` | 64-bit signed integer | `0` |
| `int8` / `int16` / `int32` | Signed variants | `0` |
| `uint` | 64-bit unsigned integer | `0` |
| `uint8` / `uint16` / `uint32` | Unsigned variants | `0` |
| `float` | 64-bit IEEE 754 double | `0.0` |
| `float32` | 32-bit IEEE 754 float | `0.0` |
| `bool` | Boolean — `true` or `false` only | `false` |
| `str` | Immutable UTF-8 string (heap) | `""` |
| `byte` | Alias for `uint8` | `0` |

**`bool` is NOT an integer** — `if count:` is a type error; use `if count != 0:`

### Type Conversions (always explicit)

```
var b: float = float(a)     # int → float
var c: int32 = int32(a)     # narrowing (may truncate)
var s: str   = str(a)       # any primitive → string
```

### Runtime Type Inspection

```
var t: str = type(42)        # "int"
var t: str = type("hello")   # "str"
var t: str = type(true)      # "bool"
var t: str = type(3.14)      # "float"

if type(x) == "int":
    print("x is an integer")
```

`type(val) -> str` — returns the runtime type name of any value as a string. Accepts any type.

### Optional Types (`?T`)

FORGE has no null pointers. Use `?T` for values that may be absent.

```
var found: ?int = none         # absent
var result: ?int = some(42)    # present

if found is some:
    print(found.value)         # unwrap with .value
else:
    print("not found")

var val: int = found or_else 0 # unwrap with fallback
```

---

## Variables and Constants

```
var name: Type = expression    # declared and initialized
var name: Type                 # zero-initialized
const NAME: Type = expression  # compile-time constant (SCREAMING_SNAKE_CASE)
```

**Compound assignments:** `+=` `-=` `*=` `/=` `%=` `&=` `|=` `^=` `<<=` `>>=`

---

## Operators

| Category | Operators |
|----------|-----------|
| Arithmetic | `+` `-` `*` `/` `%` (unary `-`) |
| Comparison | `==` `!=` `<` `>` `<=` `>=` |
| Logical | `and` `or` `not` (short-circuit) |
| Bitwise | `&` `\|` `^` `~` `<<` `>>` |
| String | `+` (concatenation) |
| Optional | `is some` `is none` `.value` `or_else` `some(x)` |
| Range | `0..10` (exclusive) `0..=10` (inclusive) |

**Precedence** (high → low): `()` `.` `[]` → unary → `*/%` → `+-` → `<<>>` → `&` → `^` → `|` → comparisons → `not` → `and` → `or`

---

## Control Flow

```
# If / elif / else
if condition:
    body
elif other_condition:
    body
else:
    body

# While loop
while i < 10:
    i += 1

# For loop — range is half-open [start, stop)
for i in range(0, 10):
    print(i)

for i in range(10, 0, -1):    # countdown
    print(i)

for item in collection:        # iterate array
    process(item)

# Infinite loop (must contain 'break')
loop:
    if done:
        break

# break / continue
break       # exit nearest enclosing loop
continue    # skip to next iteration
```

---

## Procedures

```
proc name(param: Type, ...) -> ReturnType:
    return value

proc add(a: int, b: int) -> int:
    return a + b

proc greet() -> void:
    print("Hello!")

# Reference parameters (modifies caller's variable)
proc swap(a: ref int, b: ref int) -> void:
    var tmp: int = a
    a = b
    b = tmp

# Caller must use 'ref' keyword
swap(ref x, ref y)
```

- Parameters are **passed by value** by default
- No nested `proc` declarations
- Procedures may call each other regardless of declaration order (two-pass scan)
- Entry point: `proc main() -> void:`

---

## Records

Plain data containers — no methods, no inheritance, no constructors.

```
record Sensor:
    id:     int
    label:  str
    value:  float
    active: bool

# Instantiation — ALL fields required, any order
var s: Sensor = Sensor { id: 1, label: "depth", value: 0.0, active: true }

# Field access and mutation
var v: float = s.value
s.value = 12.5

# Records are value types — assignment copies all fields
var copy: Sensor = s

# Exported record (accessible from other modules)
export record Waypoint:
    name: str
    latitude: float
    longitude: float
```

---

## Arrays and Maps

### Fixed Arrays (stack-allocated)

```
var buf: [byte; 512]                   # 512 bytes, zero-initialized
var coords: [float; 3] = [1.0, 2.0, 3.0]

var n: int = len(buf)                  # compile-time constant
var first: float = coords[0]           # zero-based index
```

### Dynamic Arrays (heap-allocated)

```
var log: []str = []
append(log, "event one")
var n: int = len(log)
for item in log:
    print(item)
free(log)                              # explicit free required
```

### Maps

```
var cfg: map[str, int] = {}
cfg["timeout"] = 30

var t: int = cfg["timeout"]           # runtime error if key absent
var val: ?int = get(cfg, "missing")   # safe access returns ?int

if has_key(cfg, "debug"):
    delete_key(cfg, "debug")

for key in map_keys(cfg):
    print(key + " = " + str(cfg[key]))

free(cfg)
```

---

## Module System

```
# Every .fg file is a module (name = filename without extension)
# sensors.fg → module: sensors

# Export symbols (private by default)
export record Sensor: ...
export const MAX_SENSORS: int = 32
export proc read(s: Sensor) -> float: ...

# Import
import sensors
import nmea_parser as nmea

# Access via dot notation
var s: sensors.Sensor = sensors.Sensor { ... }
var v: float = sensors.read(s)

# Module init (called once before any external use)
proc init() -> void:
    # module setup
```

**Import rules:** No circular imports. No wildcard imports. Unused imports = warning.

---

## Channel System

Channels provide loose, event-driven coupling between modules.

```
# Declare at module scope
channel depth_reading: float
channel shutdown: void                  # no payload
export channel sensor_alert: str

# Emit
emit depth_reading -> 12.5
emit shutdown                           # void channel

# Register handlers (at module scope)
on depth_reading as value:
    print("Depth: " + str(value))

on shutdown:
    cleanup()
    forge.sys.exit(0)

# Cross-module handler
import sensors
on sensors.reading_available as v:
    update_display(v)
```

Multiple handlers per channel are called in registration order.

---

## Memory Management

```
# Stack (automatic) — local vars, fixed arrays, records
proc compute() -> void:
    var buf: [byte; 64]      # freed automatically at scope exit

# Heap — dynamic arrays and maps (must be freed)
var data: []byte = alloc([]byte, 1024)
free(data)

# Scope-managed heap (automatic free at block exit)
with alloc([]byte, 1024) as buf:
    process(buf)
# buf freed here
```

---

## Error Handling

```
# Optional type for simple success/failure
proc find(arr: []int, target: int) -> ?int:
    for i in range(0, len(arr)):
        if arr[i] == target:
            return some(i)
    return none

# Result record for detailed errors
record Result:
    ok:    bool
    value: int
    error: str

# Panic for unrecoverable errors
panic("invariant violated: buffer overflow")

# Assert
assert len(buf) > 0
assert divisor != 0, "divisor must not be zero"
```

---

## Standard Library Imports

```
import forge.io       # print, read_line, read_file, write_file
import forge.str      # trim, split, join, contains, to_int, to_float, ...
import forge.math     # abs, sqrt, pow, sin, cos, floor, ceil, PI, E, ...
import forge.sys      # args, env, exit, platform, arch
import forge.time     # now, sleep, elapsed_ms, timestamp
import forge.buf      # byte buffer read/write utilities
import forge.serial   # serial port I/O (open, read_byte, write_str, close)
import forge.nmea     # NMEA 0183 parsing (parse_gga, parse_rmc, valid_checksum)
import forge.gui      # GUI windows, drawing, input, widgets (requires GUI=1 build)
```

### forge.io

```
print("Hello")                    # stdout with newline
print_raw("no newline")
eprint("error message")           # stderr
var line: str = read_line()
var content: str = read_file("data.txt")
write_file("out.txt", "Hello")
var ok: bool = file_exists("cfg.fg")
```

### forge.str

```
trim(s)  trim_left(s)  trim_right(s)
contains(s, sub)   starts_with(s, pre)   ends_with(s, suf)
to_upper(s)   to_lower(s)
split("a,b,c", ",")              # ["a", "b", "c"]
join(parts, "-")                 # "a-b-c"
replace(s, old, new)
var n: ?int = to_int("42")       # returns ?int
var f: ?float = to_float("3.14") # returns ?float
substring(s, start, end)
```

### forge.math

```
abs(-5.0)   abs_int(-5)   min(a, b)   max(a, b)   clamp(v, lo, hi)
sqrt(x)   pow(base, exp)   cbrt(x)
floor(x)   ceil(x)   round(x)   trunc(x)
sin(x)   cos(x)   tan(x)   atan2(y, x)
log(x)   log10(x)   log2(x)   exp(x)
PI   E   TAU
seed_random(n)   random_int(lo, hi)   random_float()
```

### forge.sys

```
var argv: []str = args()
var home: ?str = env("HOME")
exit(0)
print(platform())  # "linux" / "windows" / "macos"
```

### forge.serial

```
var port: ?Port = open("/dev/ttyUSB0", 4800)
if port is none: return
var p: Port = port.value
var b: ?byte = read_byte(ref p)
var line: str = read_line(ref p)
write_str(ref p, "$GPGGA,...")
close(ref p)
```

### forge.nmea

```
valid_checksum(sentence)              # bool
sentence_id(sentence)                 # "GPGGA"
field(sentence, n)                    # nth field string
var gga: ?GGA = parse_gga(sentence)
if gga is some:
    var lat: float = decimal_degrees(gga.value.latitude, gga.value.lat_dir)
var rmc: ?RMC = parse_rmc(sentence)
```

### forge.gui  *(requires `make GUI=1` build)*

```
# Window
gui.init_window(800, 600, "My App")
gui.set_target_fps(60)
gui.set_style_dark()
while gui.window_open():
    gui.begin_draw()
    gui.clear(30, 30, 35, 255)
    # ... draw here ...
    gui.end_draw()
gui.close_window()

# Drawing
gui.draw_rect(x, y, w, h, r, g, b, a)
gui.draw_circle(cx, cy, radius, r, g, b, a)
gui.draw_line(x1, y1, x2, y2, r, g, b, a)
gui.draw_text("hello", x, y, size, r, g, b, a)

# Widgets
var clicked: bool = gui.button(x, y, w, h, "OK")
var text: str = gui.textbox(x, y, w, h, text, 256)
var sel: int = gui.dropdown(x, y, w, h, "A;B;C", sel)

# Scrollable log
gui.log_create(0, x, y, w, h, 4096, 16)
gui.log_add(0, "message", r, g, b, a)
gui.log_draw(0)
```

> See [FORGE GUI Library Guide](FORGE_GUI_Library_Guide.md) for complete documentation.

---

## REPL

```bash
$ forge repl
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

Run a script then drop into REPL: `forge run program.fg --repl`

---

## Keywords

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

## Complete Example

```
# fibonacci.fg

const MAX_N: int = 15

proc fib(n: int) -> int:
    if n <= 1:
        return n
    var a: int = 0
    var b: int = 1
    for i in range(2, n + 1):
        var tmp: int = a + b
        a = b
        b = tmp
    return b

proc main() -> void:
    for n in range(0, MAX_N):
        print("fib(" + str(n) + ") = " + str(fib(n)))
```

```bash
$ forge run fibonacci.fg
fib(0) = 0
fib(1) = 1
...
fib(14) = 377
```

---

*FORGE Quick Reference v0.1 — Fragillidae Software*

