# Tier 2 Student Worksheets

---

## Worksheet 1 — Modules

1. Create two files: `shapes.fg` (with an `export record Circle: radius:
   float` and an `export proc area(c: Circle) -> float`) and `main.fg`
   (which imports `shapes` and prints the area of a circle with radius
   `4.0`).
2. **Predict, then run**: what happens if you remove `export` from
   `Circle` and try to build/run `main.fg` anyway?
3. In your own words, why can't a module-level `var` be exported
   directly, and what would you do instead if another module needed to
   read or change it?

---

## Worksheet 2 — Optional Types

1. Type and run:
   ```forge
   proc find_index(nums: []int, target: int) -> ?int:
       for i in range(0, len(nums)):
           if nums[i] == target:
               return some(i)
       return none

   proc main() -> void:
       var scores: []int = [81, 94, 76, 88]
       var found: ?int = find_index(scores, 94)
       if found is some:
           print("Found it")
       else:
           print("Not found")
       print(str(found or_else -1))
   ```
2. Write a proc `find_by_id` that takes a `[]int` of IDs and a target ID
   and returns `?int` (the index, or `none`). Call it with an ID that
   exists and one that doesn't, printing both results correctly.
3. What does `found or_else -1` do if `found` is `none`? What if it's
   `some(3)`?

---

## Worksheet 3 — Errors as Values

1. Type and run:
   ```forge
   record Result:
       ok:    bool
       value: int
       error: str

   proc safe_divide(a: int, b: int) -> Result:
       if b == 0:
           return Result { ok: false, value: 0, error: "division by zero" }
       return Result { ok: true, value: a / b, error: "" }

   proc main() -> void:
       var r: Result = safe_divide(10, 0)
       if r.ok:
           print(str(r.value))
       else:
           print("Error: " + r.error)
   ```
2. For each situation, say whether it should be a `Result`-style return
   or a `panic`: (a) a user enters text where a number is expected, (b) a
   loop index somehow goes negative due to a logic bug, (c) a file the
   program expects to exist is missing, (d) a state machine reaches a
   branch that should be impossible by the program's own logic.
3. Write an `assert` that checks a `count` variable is never negative.

---

## Worksheet 4 — Memory

1. Type and run (or trace by hand if `forge.buf` isn't available in your
   setup):
   ```forge
   proc use_heap_buffer() -> void:
       var buf: int = forge.buf.create(32)
       forge.buf.write_str(buf, "FORGE")
       var text: str = forge.buf.to_str(buf)
       print(text)
       forge.buf.free_buf(buf)
   ```
   For each line, write "stack," "heap," or "neither" for what memory
   action (if any) is happening.
2. Rewrite the buffer creation/use/free above using `with alloc(...) as`
   instead of manual `free_buf`.
3. Write a proc `increment(n: ref int)` that adds 1 to whatever variable
   is passed in, and call it correctly from `main`.

---

## Worksheet 5 — Channels

1. Declare a channel `alert_raised: str`, write a proc that `emit`s it
   with a message, and write an `on alert_raised as msg:` handler that
   prints the message.
2. On paper: design a channel for a "score changed" event in a simple
   game. What type does it carry? Which part of the program would `emit`
   it? Which part(s) would have an `on` handler?
3. In your own words, what does the module that calls `emit` know about
   who (if anyone) is listening?

---

## Worksheet 6 — Run vs. Build

1. Run a small program (yours or a provided one) with `forge run`, then
   with `forge build` followed by running the resulting binary. Confirm
   the output matches.
2. Run `forge emit` on the same program and open the generated C file.
   Find the part that corresponds to your `main` proc.
3. Deliberately introduce a type error and run `forge check`. What does
   it report, and does it run the program at all?

---

## Worksheet 7 — Capstone Plan

Write a short plan (turned in, not typed into the IDE):
1. Which project are you building (Sensor Dashboard, Inventory Tracker,
   or your own idea)?
2. List each module you'll create and what each one exports.
3. Where does an optional type (`?T`) or a `Result`-style record protect
   your program from a missing or failed value?
4. Where does a channel connect two parts of your program, and what
   would break if you replaced it with a direct proc call instead?
