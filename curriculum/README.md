# Learn to Program with FORGE — the Capstone Language

FORGE is the last language in the BARE → STEPS → PLAIN → FORGE
progression, and the only one of the four with no existing tutorial or
lesson sequence — just reference documentation
([user_docs/](../user_docs/)) and example programs
([examples/](../examples/)). This folder is a from-scratch curriculum, but
kept short deliberately: FORGE's whole point is to be the bridge into
general-purpose compiled languages (C, Rust, Go, Swift), so the curriculum
is a punch list of "here is the one new idea per concept," not a
re-teaching of programming from zero.

**Start here if you're a teacher**: read
[teachers-guide.md](teachers-guide.md) in full.

## Why FORGE comes after PLAIN

PLAIN let students infer a lot: a variable's type came from a prefix or
was left to the interpreter to sort out at runtime, errors could be
handled and moved past, memory was somebody else's problem. FORGE takes
each of those away, one at a time, and makes the student responsible for
what the computer is actually doing. FORGE's own design docs say it best:
*"Explicit types remove guesswork... students see exactly what kind of
data they are working with. No implicit coercion, no surprise type
changes."* That's the whole language in one sentence — and it's also
exactly the mental model every statically-typed systems language a student
meets next (C, Rust, Go, Swift, Java) will demand of them.

FORGE is also the first language in the suite that can be **compiled**,
not just interpreted — `forge run` behaves like everything before it, but
`forge build` turns the same source into a native binary, and `forge emit`
shows the generated C or LLVM IR. Where BARE's Tier 3 curriculum ended with
a lesson *describing* the gap to "real" languages, FORGE lets students
*cross* it themselves and look at what's on the other side.

## The two tiers

| Tier | Folder | New big idea |
|---|---|---|
| **1 — FORGE Foundations** | [tier1-foundations/](tier1-foundations/) | Typed variables, `elif`, `for`/`range`, arrays, records, explicit casts, no truthiness — say out loud, every time, exactly what kind of data you have |
| **2 — FORGE Systems** | [tier2-systems/](tier2-systems/) | Modules (`export`/`import`), optional types, errors as values, manual memory (`alloc`/`free`/`ref`), channels/events, `forge run` vs `forge build` — programs as systems: what talks to what, who owns which memory, what happens when it fails |

Each tier folder has `lessons.md`, `worksheets.md`, and `assessment.md`,
matching BARE's format. Tier 2's capstone (assessment.md) is the closing
project for the entire four-language suite.

## How this relates to the rest of the FORGE project

- [user_docs/FORGE_Usage_Guide.md](../user_docs/FORGE_Usage_Guide.md) is
  the language/CLI reference.
- [user_docs/FORGE_Optional_Types_Guide.md](../user_docs/FORGE_Optional_Types_Guide.md)
  and
  [user_docs/FORGE_Memory_Management_Guide.md](../user_docs/FORGE_Memory_Management_Guide.md)
  are already written for beginners and can be handed to students directly
  in Tier 2 — the teachers-guide tells you when.
- [examples/optional_types_intro.fg](../examples/optional_types_intro.fg)
  and
  [examples/memory_management_intro.fg](../examples/memory_management_intro.fg)
  are small, beginner-scaffolded demo programs matching those two guides.
- [examples/forge_euler/](../examples/forge_euler/) (ten Project Euler
  solutions) is good Tier 1 extension/practice material.
- [examples/forge_instruments/](../examples/forge_instruments/) (a
  multi-module marine-electronics dashboard using channels, modules,
  optional types, records, and serial I/O together) is the capstone —
  it's the same "here's what this is really for" flagship demo that STEPS
  and PLAIN each have, and it exercises nearly every Tier 2 idea at once.
- The FORGE IDE has nested scope-block coloring like PLAIN's (View menu),
  and the same "tolerates code mid-edit" behavior — worth a quick mention
  in class, not a full lesson.

Test FORGE from source or a fresh `forge build`/`forge_ide` — the
prebuilt `dist/` executable in this project has been out of date before.
