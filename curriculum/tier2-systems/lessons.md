# Tier 2 — FORGE Systems

Seven lessons, and the last tier in the whole four-language suite. Tier 1
made students say what type everything is. Tier 2 asks the bigger
questions a real systems language has to answer: how do separate files
trust each other, what do you do about a *missing* value instead of
`null`, what happens when something fails if there's no `try`/`catch`,
who's responsible for memory, how do independent parts of a program talk
to each other, and — the closing lesson — what does it actually mean for
source code to become a running program. Give each lesson real time; none
of these compress the way Tier 1's syntax-only lessons did.

---

## Lesson 1 — Modules: `export` and `import`

**Objective**: Split a program across FORGE modules using explicit
`export`/`import`, and explain FORGE's stricter visibility rule versus
PLAIN's.

**Teach**:

```forge
# sensors.fg
export record Sensor:
    id:    int
    label: str
    value: float

export proc read(s: Sensor) -> float:
    return s.value

# Private — not exported:
proc validate(s: Sensor) -> bool:
    return s.id > 0
```

```forge
# main.fg
import sensors

proc main() -> void:
    var s: Sensor = sensors.Sensor { id: 1, label: "depth", value: 3.2 }
    print(str(sensors.read(s)))
```

Compare directly to PLAIN's Package/Assembly/Module system: same
instinct (force a decision about what's public), a simpler mechanism —
one file is one module, nothing is visible outside it unless marked
`export`, and there's no wildcard import to accidentally pull in more
than intended. Note what's exportable (`record`, `proc`, `const`, `type`,
`channel`) and what never is: a module-level `var` can't be exported at
all — if another module needs to affect that state, it has to go through
an exported `proc`. Ask: *"Why would a language stop you from exporting a
plain variable, but let you export a function that changes it?"* (The
function is a controlled door; a raw exported variable would let any
importer reach in and set it directly, bypassing whatever rules the
`proc` might otherwise enforce.)

**Guided practice**: As a class, split a Tier 1 program (e.g., the
`Sensor` record and its helpers) into two files — one module, one main —
with correct `export`/`import`.

**Independent practice**: Worksheet 1.

**Wrap-up**: Exit ticket — why can't a module-level `var` be exported
directly, and what's the workaround?

**Differentiation**: *Extension* — a three-module chain (`main` imports
`b`, `b` imports `c`). *Support* — the two-file `sensors`/`main` split
above, typed, run, and confirmed working, is a complete goal.

---

## Lesson 2 — No Null: Optional Types

**Objective**: Use `?T`, `some`, `none`, `is some`/`is none`, and
`or_else` to represent a value that might be missing.

**Teach**: Hand out
[user_docs/FORGE_Optional_Types_Guide.md](../../user_docs/FORGE_Optional_Types_Guide.md)
alongside this lesson — it's already written at beginner level. Walk
through the core example live:

```forge
proc find_index(nums: []int, target: int) -> ?int:
    for i in range(0, len(nums)):
        if nums[i] == target:
            return some(i)
    return none

proc describe_search(nums: []int, target: int) -> void:
    var found: ?int = find_index(nums, target)

    if found is some:
        print("Found it")
    else:
        print("Did not find it")

    var index_or_default: int = found or_else -1
    print("Index or default: " + str(index_or_default))
```

Point out the key idea: `find_index`'s own signature (`-> ?int`) tells
every caller, right there in the header, "you might not get an answer" —
there's no way to accidentally treat the result as a plain `int` without
either checking `is some`/`is none` first or providing a fallback with
`or_else`. Contrast with `null` in languages that have it: `null` is a
value that silently *pretends* to be any type until something crashes
trying to use it; FORGE's `?T` makes "this might be missing" part of the
type itself, checked before the program runs.

**Guided practice**: As a class, write a `find_by_name` proc over a `[]Sensor`
that returns `?Sensor`, and call it twice — once with a name that exists,
once with one that doesn't — printing the result correctly both times.

**Independent practice**: Worksheet 2.

**Wrap-up**: Exit ticket — what does `or_else` do, and when would you
reach for it instead of `is some`/`is none`?

**Differentiation**: *Extension* — a proc returning `?Sensor` used inside
another proc that itself returns an optional, chaining the "might be
missing" idea two levels deep. *Support* — the `find_index` example
above, run with a target that exists and one that doesn't, is a complete
goal.

---

## Lesson 3 — Errors Without Exceptions: Values, `panic`, and `assert`

**Objective**: Represent a failure as a returned value, and distinguish
that from `panic` (a bug) and `assert` (a checked invariant).

**Teach**: Land the philosophy first: *"PLAIN taught you to handle errors
with `attempt`/`handle` — the program tries something, and if it fails,
a separate block catches it. FORGE asks a harder question: what if the
*type system itself* forces you to check for failure before you're
allowed to use a result at all?"*

```forge
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

A caller must check `.ok` before trusting `.value` — there's no exception
to accidentally not catch, because there was never an exception to begin
with; the failure is just data, sitting in the return value like anything
else. Show `panic` and `assert` as the *other* half of error handling —
reserved for actual bugs, not ordinary failures:

```forge
panic("unreachable code reached in state machine")
assert count >= 0
```

`panic` is not catchable and stops the program — use it only for "this
should be impossible" conditions. `assert` checks an invariant and panics
if it's false. Neither is for something as ordinary as "the user typed
letters instead of a number" — that's a `Result`-style return, checked by
the caller.

**Guided practice**: As a class, write a `safe_divide` proc returning a
`Result`-style record, and a caller that checks `.ok` before printing
`.value`.

**Independent practice**: Worksheet 3.

**Wrap-up**: Exit ticket — why is "the user entered invalid input" a
`Result`-style error, while "an array index went past its bounds due to a
logic bug" is a `panic`?

**Differentiation**: *Extension* — use `?T` (Lesson 2) instead of a
custom `Result` record for a case where there's no useful error message,
just present/absent, and discuss which one fits better and why.
*Support* — the `safe_divide` example above, triggered with a zero
divisor and checked correctly, is a complete goal.

---

## Lesson 4 — Memory You Manage Yourself: `alloc`/`free`/`ref`

**Objective**: Explain stack vs. heap allocation, use `alloc`/`free` and
`with alloc ... as`, and use `ref` for explicit pass-by-reference.

**Teach**: Say directly: *"Nothing in BARE, STEPS, or PLAIN ever asked
you to think about where a value actually lives in memory. This is the
first time, and it's the single most 'welcome to systems programming'
idea in the whole suite — don't rush it, and don't worry if it takes a
few tries to feel natural."* Hand out
[user_docs/FORGE_Memory_Management_Guide.md](../../user_docs/FORGE_Memory_Management_Guide.md)
and walk through:

```forge
proc show_local_values() -> void:
    var retry_limit: int = 3      # stack — released when this proc returns
    print(str(retry_limit))

proc use_heap_buffer() -> void:
    var buf: int = forge.buf.create(32)   # heap — must be freed
    forge.buf.write_str(buf, "FORGE")
    var text: str = forge.buf.to_str(buf)
    print(text)
    forge.buf.free_buf(buf)
```

Contrast the two: a local `var` inside a `proc` lives on the **stack** and
is released automatically the moment the `proc` returns — no work
required. A dynamic array, a map, or a buffer created with `alloc`/
`forge.buf.create` lives on the **heap** and stays allocated until
something explicitly frees it — forever, if nothing does. Show the safer
pattern:

```forge
with alloc([]byte, 1024) as buf:
    fill_buffer(buf)
    process(buf)
# buf is automatically freed here, even on an early return
```

Recommend `with alloc ... as` as the default while the habit is forming;
introduce bare `alloc`/`free` once that's solid. Cover `ref` last, as a
smaller, related idea:

```forge
proc increment(n: ref int) -> void:
    n += 1

var count: int = 0
increment(ref count)     # caller must write 'ref' explicitly, both places
```

Every parameter is pass-by-value by default, including records — `ref`
opts in to letting a proc modify the caller's actual variable. Mention
FORGE's built-in `swap(a, b)` as a deliberate ergonomics exception: it
works without `ref` at all, specifically so a common operation doesn't
force the concept before students are ready for it.

**Guided practice**: As a class, trace through the `use_heap_buffer`
example line by line, saying out loud at each line whether something was
just allocated, used, or freed.

**Independent practice**: Worksheet 4.

**Wrap-up**: Exit ticket — what's the difference between what happens to
a stack-allocated `var` and a heap-allocated buffer when their enclosing
`proc` returns?

**Differentiation**: *Extension* — a `proc` using `ref` to swap two record
fields between two variables (without using the built-in `swap`).
*Support* — the `use_heap_buffer` example, run and traced by hand line by
line, is a complete goal; writing new heap-allocating code from scratch is
a stretch goal, not a requirement, at this stage.

---

## Lesson 5 — Channels and Events: `emit` / `on`

**Objective**: Declare a channel, emit a message on it, and register a
handler with `on`, explaining why this is different from calling a proc
directly.

**Vocabulary**: channel, emit, handler, pub/sub.

**Teach**: This is the most advanced idea in the tier — save it for a
session on its own, and lean on
[examples/forge_instruments/](../../examples/forge_instruments/) (a real
multi-module dashboard reading live sensor data) to show it used for
real.

```forge
channel depth_reading: float

emit depth_reading -> 12.5
```

```forge
on depth_reading as value:
    display.update_depth(value)
    log("depth: " + str(value))
```

Frame the core idea with a question: *"If module A wants module B to
react to something, why not just have A call a proc in B directly?"*
Sometimes that's exactly right — but a channel is for when the emitting
code (say, a sensor-reading module) shouldn't need to know *who*, if
anyone, is listening, or how many listeners there are. `emit` broadcasts;
zero or more `on` handlers elsewhere react, and the emitting module never
names them. This is explicitly modeled on hardware interrupts — say that
directly, since it's a real, concrete mental model: a sensor doesn't call
a specific function when a new reading arrives, it raises a signal, and
whatever's listening responds. Show `export channel` for a channel other
modules can emit on or listen to, versus a private, module-only channel.

**Guided practice**: As a class, design (on paper) a channel for a
"timer expired" event: what type does it carry, which module would
`emit` it, and which module(s) would register `on` handlers.

**Independent practice**: Worksheet 5.

**Wrap-up**: Exit ticket — what does the emitting code know about who's
listening to a channel? What does that buy you compared to a direct proc
call?

**Differentiation**: *Extension* — a channel with two independent `on`
handlers in two different modules, both reacting to the same `emit`.
*Support* — the `depth_reading` example above, typed, emitted, and
handled once, is a complete goal.

---

## Lesson 6 — Two Paths, One Source: `forge run` vs. `forge build`

**Objective**: Run the same FORGE source both interpreted and compiled,
and inspect the compiler's generated C output.

**Teach**: This is the payoff lesson for the entire four-language suite —
where BARE's Tier 3 could only *describe* the gap to "real" languages,
FORGE lets students cross it themselves. Run a small, already-written
program two ways:

```
forge run main.fg
```

```
forge build main.fg -o main
./main
```

Ask: *"Same source file, same output — so what actually happened
differently between those two commands?"* `forge run` interprets the
source directly — fast feedback, full error messages, no separate build
step, the "safe playground" mode every language in the suite has used so
far. `forge build` runs the source through an actual compiler and
produces a native binary that runs on its own, with no FORGE toolchain
needed at runtime. Then show what's normally invisible:

```
forge emit main.fg
```

— and open the generated C file together. Don't expect students to read
all of it; the point is simply *seeing* that a compiler is not magic, it
is a program that turns one text file into another, more machine-shaped
one. This is a good moment to also mention `forge check` (type-check
only, no run) and `forge fmt`, as the kind of tooling every real compiled
language ships with.

**Guided practice**: As a class, deliberately break a program's types,
run `forge check`, and read the error before it ever reaches `forge run`
or `forge build`.

**Independent practice**: Worksheet 6.

**Wrap-up**: Exit ticket — name one thing `forge build` gives you that
`forge run` doesn't, and one thing `forge run` gives you that `forge
build` doesn't.

**Differentiation**: *Extension* — compare the `forge emit` output of two
small variations of the same program (e.g., with and without a loop) and
discuss what changed. *Support* — running one program both ways and
confirming identical output is a complete goal.

---

## Lesson 7 — Capstone: A Compiled, Multi-Module FORGE Program

**Objective**: Design and build a multi-module FORGE program using a
record, an optional type, a channel, and a clean `forge build`.

**Teach**: Show [examples/forge_instruments/](../../examples/forge_instruments/)
as the model — scoped down considerably, since that's a full product, not
a student assignment. Present capstone options:
- **Sensor Dashboard (simplified)** — a `sensors.fg` module (a `Sensor`
  record, a channel for new readings), a `display.fg` module with an `on`
  handler that prints readings, a `main.fg` that emits a few fake
  readings.
- **Inventory Tracker** — an `items.fg` module (an `Item` record, a
  `find_by_name` proc returning `?Item`), a `main.fg` that looks up items
  by name and handles a not-found case with `or_else` or an explicit
  check.
- **A project of the student's own design**, approved by the teacher,
  using at least two modules, one record, one optional type or
  `Result`-style error, and one channel.

Require a written plan: which modules, what each exports, where the
optional type or error-as-value shows up, and where the channel connects
the pieces — before any code. The finished project must build cleanly
with `forge build`.

**Guided practice**: Peer-review plans in pairs — a partner should be
able to say, from the plan alone, which module would break first if the
channel were removed.

**Independent practice**: Build the capstone; confirm `forge build`
succeeds before showcasing.

**Wrap-up**: Showcase — each student runs their compiled binary and
explains: what does the channel decouple that a direct proc call
wouldn't, what does their optional-type or `Result` check protect
against, and what did `forge emit` show them about their own program that
surprised them.

**Differentiation**: *Extension* — a third module, or a second channel
with a real chain of `emit`/`on` across three modules. *Support* — the
Sensor Dashboard option with a provided 2-module skeleton (`Sensor`
record and channel already declared) is the lowest-friction path.

This is the end of the four-language suite. Close the showcase by saying
so plainly: everything a first course in C, Rust, Go, or Swift will ask —
explicit types, modules, manual memory concepts, a compile step — has
already been done here, by hand, in a language built to make each one
approachable on its own.
