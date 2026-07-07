# Tier 2 Assessment

Two checkpoints plus the capstone rubric — the last assessment in the
whole four-language suite.

---

## Checkpoint 1 — Modules, Optional Types, and Errors (after Lesson 3)

1. What keyword makes a `record`, `proc`, or `channel` visible to another
   module, and what's true of everything that doesn't have it?
2. Why can't a module-level `var` be exported directly?
3. What will this print?
   ```forge
   proc find_index(nums: []int, target: int) -> ?int:
       for i in range(0, len(nums)):
           if nums[i] == target:
               return some(i)
       return none

   proc main() -> void:
       var found: ?int = find_index([1, 2, 3], 9)
       print(str(found or_else -1))
   ```
4. What's the key difference between FORGE's `?T`/`none` and a `null`
   value in a language that has one?
5. Give one example of a failure that should be a `Result`-style return,
   and one that should be a `panic`. Explain the difference.

**Answer key**: 1. `export`; everything without it is private to that
module and cannot be imported  2. Because an exported raw variable would
let any importing module read or change it directly with no control — an
exported `proc` acts as a controlled door instead  3. `-1` — `9` isn't in
the list, so `find_index` returns `none`, and `or_else -1` supplies the
fallback  4. `?T` makes "this might be missing" part of the type itself,
checked before the program runs and impossible to use without either
checking or supplying a fallback; `null` is a value that silently
pretends to be any type until something tries to use it and crashes
5. A `Result`-style return: something ordinary and expected, like a user
entering invalid input. A `panic`: a condition that represents a bug in
the program itself, like reaching a branch that should be logically
impossible — `panic` is for "this should never happen," not for ordinary
failure.

---

## Checkpoint 2 — Memory, Channels, and Compilation (after Lesson 6)

1. What's the difference between what happens to a stack-allocated `var`
   and a heap-allocated buffer when the `proc` they're in returns?
2. What does `with alloc(...) as name:` do that a bare `alloc`/`free`
   pair requires you to remember to do yourself?
3. Why does `increment(n: ref int)` require the caller to write
   `increment(ref count)` instead of just `increment(count)`?
4. What is a channel's `emit` allowed to *not* know, compared to calling a
   proc directly?
5. What does `forge build` produce that `forge run` does not, and what
   does `forge run` give you that `forge build` doesn't?

**Answer key**: 1. The stack-allocated `var` is released automatically the
instant the `proc` returns; a heap-allocated buffer stays allocated until
something explicitly `free`s it — forever, if nothing does  2. It frees
the allocation automatically at the end of the block, even on an early
return, so there's no way to forget  3. Because FORGE parameters are
pass-by-value by default, including at the call site — writing `ref`
explicitly, both in the proc's signature and at the call, makes it
visually obvious that this call can modify the caller's variable
4. Which module(s), if any, are listening — `emit` broadcasts without
naming a specific receiver, unlike a direct proc call which always names
exactly what it's calling  5. `forge build` produces a native binary that
runs without the FORGE toolchain present; `forge run` gives fast
feedback and full error messages with no separate build step.

---

## Capstone Rubric

Score each category 0-3.

| Category | 0 | 1 | 2 | 3 |
|---|---|---|---|---|
| **Correctness & build** | Doesn't run, or a core feature is missing | Runs under `forge run` but `forge build` fails, or a core feature is broken | Works correctly under both `forge run` and a clean `forge build` | Also handles at least one edge case cleanly (missing lookup, invalid input) |
| **Modules & data** | Everything in one file, no record used meaningfully | Split across files, but the split is arbitrary; or a record exists but is unused in the program's actual logic | A sensible module split, each exporting only what's needed; at least one record used meaningfully | Also keeps a genuinely private (non-exported) helper where it belongs |
| **Optional types / errors** | No `?T` or `Result`-style handling anywhere it's needed | Present but not actually checked before use (would compile, but the safety isn't exercised) | Used correctly where a value could realistically be missing or an operation could fail, and checked before use every time | Can also clearly explain, when asked, why this case is a `Result`/`?T` situation and not a `panic` |
| **Channels & explanation** | No channel used, or `emit`/`on` present but disconnected/unused | A channel exists and fires, but the student can't explain what it decouples | A channel meaningfully connects two modules, and the student can explain what would break if it were replaced with a direct proc call | Can also walk through their own `forge emit` output and describe, in general terms, what changed between source and compiled form |

A student scoring 2+ across all four categories has completed the
four-language suite.
