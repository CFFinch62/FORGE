# Tier 1 — FORGE Foundations

Four lessons. This tier assumes a PLAIN graduate: tasks, `choose`/branching,
loops, and records should already be solid. FORGE keeps every one of
those ideas but demands one new thing of the student throughout: **say
exactly what kind of data you have, every time, and let the computer
check you before your program even runs.** That single habit is the
whole tier — nothing here is new computational thinking yet (Tier 2 is
where that starts); it's a new, stricter relationship with the same ideas.

---

## Lesson 1 — Say the Type Out Loud: Mandatory Type Annotations

**Objective**: Declare a typed variable, procedure parameter, and return
type, and explain why FORGE never infers a type the way PLAIN could.

**Vocabulary**: type annotation, `proc`, sized numeric type.

**Teach**:

```forge
proc add(a: int, b: int) -> int:
    return a + b

proc greet(name: str) -> void:
    print("Hello, " + name)

proc main() -> void:
    var retry_limit: int = 3
    var label: str = "packet"
    print(label + " retry limit: " + str(retry_limit))
```

Point out what's gone: PLAIN let a `int`/`str` name-prefix hint at a
type and left the rest to the interpreter; FORGE requires the type on
**every** `var`, every parameter, and every return, written with a colon,
checked before the program runs at all. Say it directly: *"You've been
implicitly typing this whole time — PLAIN's `intScore`, STEPS' `as
number`. FORGE just makes you say it out loud, everywhere, and it checks
you."* Introduce sized numeric types lightly — `int` (64-bit) and `float`
are the everyday defaults; `int8`/`int16`/`int32`, the `uint` family, and
`byte` exist for when a size actually matters, which is rare at this
level. Show a type-mismatch error on purpose:

```forge
var x: int = 5
var y: float = x     # error: type mismatch, expected float, got int
```

— and the fix, an explicit cast: `var y: float = float(x)`. No numeric
type ever silently converts into another; every conversion is spelled
out.

**Guided practice**: As a class, take a PLAIN task from memory (e.g., a
temperature converter) and rewrite it as a fully-typed FORGE `proc`,
including its return type.

**Independent practice**: Worksheet 1.

**Wrap-up**: Exit ticket — what happens if you assign a `float` value to
an `int` variable without a cast?

**Differentiation**: *Extension* — a `proc` with three differently-typed
parameters (`int`, `float`, `str`) and a `str` return, built from
scratch. *Support* — the `add`/`greet` examples above, typed and run
successfully, is a complete goal.

---

## Lesson 2 — No Truthiness: Every Condition Must Be a Real Boolean

**Objective**: Write only genuine boolean expressions in `if`/`while`
conditions, and explain why FORGE removes truthiness entirely.

**Teach**: Revisit the running theme across the whole suite: BARE made
`0` and `""` deliberately **truthy**, specifically so `if score` wouldn't
silently misbehave when `score` was `0`. FORGE solves the same underlying
problem a different way — by removing the question entirely:

```forge
var count: int = 5
if count:               # ERROR: int is not bool
    print("has items")
```

The fix is always an explicit comparison:

```forge
if count > 0:
    print("has items")
```

No type — not `int`, not `str`, not a pointer — ever coerces to `bool`.
Ask students to trace the whole suite's answer to this one question, in
order: *"BARE made 0 and empty string truthy on purpose, to avoid a
surprise. STEPS and PLAIN inherited that model. FORGE throws it out
completely and makes a non-boolean condition a compile error. Which
approach do you think actually prevents more bugs — and what do you give
up by going FORGE's way?"* (A fair answer: FORGE prevents the bug
entirely at compile time rather than relying on a sensible default, at
the cost of always having to write out the comparison explicitly.)

**Guided practice**: As a class, take 2-3 lines that would work in PLAIN
or STEPS by relying on truthiness and rewrite each as an explicit FORGE
comparison.

**Independent practice**: Worksheet 2.

**Wrap-up**: Exit ticket — will `if some_list:` compile in FORGE? What
would you write instead to check whether a list has any elements?

**Differentiation**: *Extension* — predict, before running, every line of
a short program that would need a truthiness-to-comparison rewrite.
*Support* — the `count` example above, broken and then fixed by hand, is
a complete goal.

---

## Lesson 3 — `elif`, `for`/`range`, and Arrays

**Objective**: Use `elif` chains, `for`/`range` loops, and fixed/dynamic
arrays with explicit element types.

**Teach**: Show the familiar shapes, now fully typed:

```forge
proc describe(score: int) -> void:
    if score >= 90:
        print("A")
    elif score >= 80:
        print("B")
    else:
        print("C or below")

proc main() -> void:
    for i in range(0, 5):
        print(i)

    for i in range(10, 0, -1):
        print(i)

    var scores: []int = []
    append(scores, 88)
    append(scores, 92)
    print(str(len(scores)))
```

Connect each piece back: `elif` is a familiar shape after STEPS'
`otherwise if` and PLAIN's `choose` — just a fourth answer to the same
recurring "multi-way decision" question, now with every branch's
condition type-checked. `for i in range(start, stop)` is a typed cousin of
PLAIN's `loop i from ... to ...`; a third argument gives the step
(`range(10, 0, -1)` counts down). Introduce `[]int` (a dynamic array,
grows with `append`, sized with `len`) as the typed version of a PLAIN
`list of integer` — every element must be the declared type, no
exceptions. Mention fixed arrays (`[int; 5]`, a constant size, stack-
allocated) exist for when the size is known and fixed, but dynamic arrays
are the everyday tool at this level.

**Guided practice**: As a class, write a `for`/`range` loop that fills a
`[]int` with the first 10 square numbers, then prints them.

**Independent practice**: Worksheet 3.

**Wrap-up**: Exit ticket — what's the difference between `range(0, 5)`
and `range(0, 5, 2)`?

**Differentiation**: *Extension* — a nested `for`/`range` loop building a
multiplication table into a `[][]int` (array of arrays). *Support* — the
`describe`/`scores` examples above, typed and run, is a complete goal.

---

## Lesson 4 — Records, Revisited: Fields With Fixed Types

**Objective**: Define a FORGE `record` with typed fields, create an
instance, and compare FORGE's record rules to PLAIN's.

**Teach**:

```forge
record Sensor:
    id:     int
    label:  str
    value:  float
    active: bool

proc main() -> void:
    var s: Sensor = Sensor {
        id:     1,
        label:  "depth",
        value:  0.0,
        active: true
    }

    print(s.label + ": " + str(s.value))
    s.value = 12.5
```

Point out the syntax shift directly: PLAIN records are constructed with
parentheses and named arguments (`Person(name: "Chuck", age: 63)`); FORGE
uses curly braces (`Sensor { id: 1, ... }`), and — a stricter rule than
PLAIN's default-value system — **every field must be provided**. There
are no default values and no partial initialization; leaving one out is a
compile-time error, not a runtime surprise. Also note: assigning a record
to another variable **copies every field** (`var b: Sensor = a` — `b` is
a completely separate copy); this is worth demonstrating live, since it's
easy to assume `b` and `a` share state the way objects do in some other
languages. FORGE records still have no methods and no inheritance — same
"data, not objects" philosophy as PLAIN, just typed more strictly.

**Guided practice**: As a class, define a `Position` record (latitude,
longitude, altitude — all `float`) and create two instances, showing that
changing one doesn't affect the other.

**Independent practice**: Worksheet 4.

**Wrap-up**: Exit ticket — what happens if you try to create a `Sensor`
instance and leave out the `active` field?

**Differentiation**: *Extension* — a record containing another record as
a field (e.g., a `NavData` record with a `Position` field). *Support* —
the `Sensor` example above, typed, run, and one field changed, is a
complete goal.
