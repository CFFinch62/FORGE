# FORGE Curriculum — Teacher's Guide

This assumes your students have finished PLAIN and covers FORGE from
scratch, since FORGE has no existing tutorial to build on — only reference
docs and examples (see [README.md](README.md)). Keep it moving; the goal
isn't to teach programming again, it's to teach *explicitness*.

## 1. Why FORGE, and why it's the last one

FORGE's own internal design notes state the goal directly: it should be
"teachable alongside your existing Steps and PLAIN curricula," aligned
with "conscious engagement — students must understand what code does, not
just write it." Every feature below exists to close a gap between "the
language is helping you" and "you are doing the thinking yourself." That's
the right note to end the suite on: FORGE is where the training wheels
PLAIN still had come off.

Use this recurring theme across the whole FORGE unit — **look at how the
same problem gets a different answer in each language you've learned**:

| Question | BARE | STEPS | PLAIN | FORGE |
|---|---|---|---|---|
| Is `0` or an empty value "true" in an `if`? | Yes, deliberately (only `false`/`null` are falsy) | inherited from BARE's model | inherited from BARE's model | **No non-boolean condition is legal at all** — `if count:` is a type error; you must write `if count > 0:` |
| How do you handle a multi-way decision? | Nested `if`/`else` only | `otherwise if` | `choose`/`choice`/`default` | `elif` — back to an if-chain, now with full type-checking on every branch |
| What happens when something goes wrong? | Program stops, full stop | `attempt`/`unsuccessful`/`then continue` | `attempt`/`handle`/`ensure` (no silent re-throw) | **No exceptions at all** — a failing operation returns a value (often `?T`/`none`) that you must check; `panic`/`assert` exist only for actual bugs |

Walking students through that table explicitly is one of the highest-value
conversations in the whole four-language suite — it turns "why does this
language do it differently" into "here are four real engineering answers
to the same question, each with a trade-off."

## 2. Tier 1 — FORGE Foundations

New material, roughly in teaching order:

- **Mandatory type annotations.** Every `var`, `const`, parameter, and
  return type is declared: `var retry_limit: int = 3`. No inference.
  Frame it as: "PLAIN let you hint at a type with a prefix like `int`/`str`
  and the language worked the rest out. FORGE makes you spell it out, every
  time, and checks you before your program even runs."
- **Sized numeric types** — `int8/16/32/64`, `uint8/16/32/64`,
  `float32/64`, `byte`. Don't over-teach this; the point for beginners is
  "numbers have a size now," not memorizing every width. `int` (64-bit)
  and `float` are the defaults students will use almost everywhere.
- **`elif`** for else-if chains — a familiar shape after STEPS'
  `otherwise if`, just a new spelling.
- **`for` loops**: `for i in range(0, 10):` and `for x in some_list:`, plus
  a separate `loop:` for infinite loops with `break`/`continue` — the
  first time in the suite loops have an early-exit keyword (STEPS
  deliberately had none; here it's back, now that types keep things safer).
- **Arrays and maps** — fixed arrays `[T; N]`, dynamic arrays `[]T` with
  `append`/`len`, and `map[K, V]`. Compare directly to PLAIN's
  `list of integer` — same idea, typed more strictly.
- **Records, again, but stricter** — same shape as PLAIN's records
  (typed fields, no methods, no inheritance), now with every field's type
  mandatory rather than inferred.
- **Explicit casts everywhere** — `int(x)`, `float(x)`, etc.; no implicit
  numeric promotion, ever. Assigning a `float` where an `int` is expected
  is a compile error, not a quiet conversion.

## 3. Tier 2 — FORGE Systems

This is the tier that makes FORGE feel like a "real" language rather than
a stricter PLAIN. Give each of these its own session.

- **Modules** — one file, one module; `export`/`import` only, no wildcard
  imports, circular imports are a compile error. Compare to PLAIN's
  Package/Assembly/Module system: same instinct (force students to think
  about what's public vs. private), simpler mechanism.
- **Optional types instead of `null`** — `?int`, `some(x)`, `none`,
  `is some`/`is none`, `or_else`. Hand students
  [user_docs/FORGE_Optional_Types_Guide.md](../user_docs/FORGE_Optional_Types_Guide.md)
  and [examples/optional_types_intro.fg](../examples/optional_types_intro.fg)
  directly — both are already written at beginner level. The core example
  worth demonstrating live:

  ```forge
  proc find_index(nums: []int, target: int) -> ?int:
      for i in range(0, len(nums)):
          if nums[i] == target:
              return some(i)
      return none
  ```

  The teaching point: the function's own signature (`-> ?int`) tells the
  caller "you might not get an answer" — the type system won't let you
  forget to check.
- **Errors as values, `panic`, `assert`** — a failing operation returns
  something a caller must check (often paired with an optional type or a
  result record); `panic()` is reserved for a genuine bug the program
  cannot recover from, and `assert` checks an invariant. There is no
  `try`/`catch`. This is the fourth and final answer in the table in §1 —
  make sure students can articulate why "the compiler forces you to check"
  is a different guarantee than "you're allowed to catch it."
- **Manual memory** — `alloc`/`free`, and the safer `with alloc(...) as x:`
  form that frees automatically at the end of the block. Hand students
  [user_docs/FORGE_Memory_Management_Guide.md](../user_docs/FORGE_Memory_Management_Guide.md)
  and [examples/memory_management_intro.fg](../examples/memory_management_intro.fg).
  This is genuinely new territory — nothing in BARE, STEPS, or PLAIN asked
  a student to think about stack vs. heap. Don't rush it, and don't be
  afraid of some confusion here; it's the single most "welcome to systems
  programming" idea in the whole four-language suite.
- **`ref` parameters** — pass-by-reference is opt-in and explicit
  (`proc swap_vals(a: ref int, b: ref int)`); the default is always
  pass-by-value, including for records. Note FORGE special-cases a
  built-in `swap(a, b)` specifically so students don't need `ref` just to
  swap two values — a deliberate ergonomics exception worth pointing out.
- **Channels and events** — `channel name: Type`, `emit name -> value`,
  `on name as value: ...`. This is FORGE's answer to "how do parts of a
  program talk to each other without one calling the other directly" —
  explicitly modeled on hardware interrupts. It's the most advanced idea in
  the curriculum; save it for last, and lean on
  [examples/forge_instruments/](../examples/forge_instruments/) to show it
  used for real (sensor readings arriving over serial, broadcast as
  channel events, picked up by a display module that never calls the
  sensor code directly).
- **`forge run` vs. `forge build`** — the same source runs interpreted
  (fast feedback, full error messages — "the safe playground") or compiles
  to a native binary via a C or LLVM backend ("what a real compiler does
  with your code"). Make this its own dedicated lesson: have students
  `forge build` a small program, then `forge emit` it and actually look at
  the generated C. This is the concrete version of the lesson BARE's
  Tier 3 could only describe in the abstract.

## 4. Misconceptions and gotchas

| What trips students up | What's actually happening |
|---|---|
| Writing `if some_number:` expecting truthiness | Not legal — FORGE requires an explicit boolean expression (`if some_number > 0:`). No type coerces to boolean. |
| Assigning a `float` to an `int` variable, or vice versa | Compile error — casts are always explicit (`int(x)`, `float(x)`), even between numeric types. |
| Expecting `try`/`catch` | Doesn't exist. A function that can fail says so in its return type (often `?T`); check it before use, don't wrap it. |
| Forgetting to `free` a heap allocation | No garbage collector — memory not freed stays allocated for the program's life. Prefer `with alloc(...) as x:` while students are still building the habit; introduce bare `alloc`/`free` once that's solid. |
| Mixing tabs and spaces, or using a tab at all | Both are lexer errors in FORGE, not just style warnings — configure editors to insert spaces only. |
| Expecting to define a class with methods | FORGE has records (data) and procedures (behavior) as two separate things, on purpose — no OOP, no `self`, no inheritance. This is consistent with every language in the suite; FORGE just makes the split explicit rather than there simply being no classes to miss. |

## 5. Capstone and what comes after

A strong capstone is a small multi-module FORGE program that uses at least
one record, one optional type, one channel, and compiles cleanly with
`forge build` — [examples/forge_instruments/](../examples/forge_instruments/)
is the model to show before students design their own (scoped down
considerably — that example is a full product, not a student assignment).

This is the end of the suite. When a student finishes FORGE, say plainly
what's true: they have already done, by hand, almost everything a first
course in C, Rust, Go, or Swift will ask of them — explicit types, modules,
manual memory concepts, a compile step they've watched happen. What's left
to learn in those languages is mostly syntax and a larger standard
library, not new ways of thinking. That's the whole point of the four
languages, from BARE's first `print "Hello, world!"` onward.
