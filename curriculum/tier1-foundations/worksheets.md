# Tier 1 Student Worksheets

---

## Worksheet 1 — Typed Variables and Procedures

1. Type and run:
   ```forge
   proc add(a: int, b: int) -> int:
       return a + b

   proc main() -> void:
       print(str(add(5, 3)))
   ```
2. Write a typed `proc` called `average` that takes two `float` parameters
   and returns their average as a `float`.
3. **Predict, then run**: what error do you get from this, and how do you
   fix it?
   ```forge
   proc main() -> void:
       var x: int = 5
       var y: float = x
   ```

---

## Worksheet 2 — No Truthiness

1. **Predict, then run**: what error do you get from this?
   ```forge
   proc main() -> void:
       var count: int = 5
       if count:
           print("has items")
   ```
   Fix it with an explicit comparison.
2. Rewrite each of these (imagine they were written in a language that
   allows truthiness) as an explicit FORGE condition:
   - "if the string `name` is not empty"
   - "if the number `errors` is not zero"
3. Write a `proc` that takes a `[]int` and a target `int`, and returns
   `true` if the array is non-empty (hint: compare `len(arr)` to `0`
   explicitly).

---

## Worksheet 3 — `elif`, `for`/`range`, Arrays

1. Type and run:
   ```forge
   proc describe(score: int) -> void:
       if score >= 90:
           print("A")
       elif score >= 80:
           print("B")
       else:
           print("C or below")

   proc main() -> void:
       describe(95)
       describe(82)
       describe(40)
   ```
2. Write a `for`/`range` loop that prints every even number from 0 to 20.
3. Build a `[]int` of the first 10 square numbers (1, 4, 9, ...) using a
   `for`/`range` loop and `append`, then print its length.
4. **Predict, then run**: what does `range(10, 0, -1)` count through?

---

## Worksheet 4 — Records

1. Type and run:
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
   ```
2. Define a `Position` record (`latitude`, `longitude`, `altitude`, all
   `float`). Create two instances, change one field on the first, and
   print both to confirm the second is unaffected.
3. **Predict, then run**: what happens if you try to create a `Sensor`
   without providing the `active` field?
