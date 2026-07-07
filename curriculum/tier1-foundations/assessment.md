# Tier 1 Assessment

Two checkpoints — no capstone here; Tier 2 builds toward the full FORGE
capstone once modules, optional types, and memory are on the table.

---

## Checkpoint 1 — Types and Truthiness (after Lesson 2)

1. What will this produce?
   ```forge
   proc main() -> void:
       var x: int = 5
       var y: float = x
   ```
2. How do you fix it?
3. What will this produce?
   ```forge
   proc main() -> void:
       var count: int = 0
       if count:
           print("has items")
       else:
           print("empty")
   ```
4. Across the whole suite, BARE made `0` truthy on purpose, to avoid a
   surprise. What does FORGE do instead, and why is that a different
   solution to the same problem rather than just "no solution"?

**Answer key**: 1. A compile-time type error — assigning an `int` to a
`float` variable requires an explicit cast  2.
`var y: float = float(x)`  3. A compile-time error — `count` is an `int`,
and FORGE requires every condition to be a real `bool`; there is no
truthiness for any type  4. FORGE removes the ambiguity entirely rather
than picking a sensible default: no type ever coerces to boolean, so the
"is 0 truthy or falsy" question can't come up at all — the trade-off is
that every condition must be written out explicitly.

---

## Checkpoint 2 — Control Flow, Arrays, Records (after Lesson 4)

1. What will this print?
   ```forge
   proc main() -> void:
       for i in range(0, 3):
           print(str(i))
   ```
2. What will this print?
   ```forge
   proc main() -> void:
       var nums: []int = []
       append(nums, 10)
       append(nums, 20)
       print(str(len(nums)))
   ```
3. Given
   ```forge
   record Point:
       x: int
       y: int
   ```
   what's wrong with `var p: Point = Point { x: 5 }`?
4. If `var b: Point = a`, and then you change a field on `b`, does `a`
   change too? Why or why not?
5. How is FORGE's record instantiation syntax different from PLAIN's?

**Answer key**: 1. `0`, `1`, `2` (one per line)  2. `2`  3. It's a
compile-time error — every field must be provided; FORGE records have no
default values, unlike PLAIN's  4. No — assigning a record copies every
field; `a` and `b` are independent after the assignment  5. FORGE uses
curly braces (`Point { x: 5, y: 10 }`); PLAIN uses parentheses with named
arguments (`Point(x: 5, y: 10)`).
