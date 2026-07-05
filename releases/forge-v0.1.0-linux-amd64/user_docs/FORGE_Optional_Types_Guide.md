# FORGE Optional Types Guide

**Audience:** Beginning students learning their first C-like language in the Steps → PLAIN → FORGE trilogy  
**Focus:** Understanding missing values safely, clearly, and predictably

---

## 1. Why Optional Types Exist

In many programs, a value is **sometimes present** and **sometimes absent**.

Examples:

- A search may find an item, or it may not.
- A student record may have a middle name, or it may be missing.
- A map lookup may succeed, or the key may not exist.
- A conversion from text to number may work, or it may fail.

In FORGE, we do **not** use `null` for this idea.

Instead, FORGE uses **optional types**.

An optional type says:

> “This variable may contain a real value of type `T`, or it may contain no value at all.”

If the base type is `int`, the optional version is `?int`.  
If the base type is `str`, the optional version is `?str`.

---

## 2. The Core Idea in One Sentence

`T` means “always has a value.”  
`?T` means “may have a value, or may be empty.”

Examples:

```forge
var age: int = 14
var middle_name: ?str = none
var score: ?int = some(98)
```

Here:

- `age` must always contain an integer.
- `middle_name` currently has no value.
- `score` currently contains an integer wrapped as an optional.

---

## 3. Two Possible States: `some(...)` and `none`

Every optional value is in one of two states:

### 3.1 `some(value)`

This means the optional **does contain** a real value.

```forge
var winner: ?str = some("Ava")
var count: ?int = some(3)
```

### 3.2 `none`

This means the optional is **empty**.

```forge
var winner: ?str = none
var count: ?int = none
```

Important:

- `none` is **not** `0`
- `none` is **not** `false`
- `none` is **not** `""`
- `none` is **not** a null pointer

It means exactly one thing: **no value is present**.

---

## 4. Why FORGE Uses Optionals Instead of `null`

Optional types help beginners write safer code.

They force you to think clearly:

- “Can this variable ever be missing?”
- “Did I check before using it?”
- “What should happen if nothing is there?”

This avoids many classic bugs:

- using a missing value by accident
- confusing “empty” with “missing”
- forgetting to handle failure cases

In FORGE, `?int` and `int` are **different types**.  
That is a good thing. It makes mistakes easier to catch.

---

## 5. Declaring Optional Variables

Use `?` before a type name.

```forge
var a: ?int = none
var b: ?int = some(42)
var c: ?str = some("hello")
var d: ?bool = none
```

You can also declare an optional without giving a value immediately.  
Its default value is `none`.

```forge
var selected: ?int
```

That is equivalent to starting with no value.

---

## 6. Optional Types Are Distinct Types

This is one of the most important rules.

`int` and `?int` are not interchangeable.

### Wrong idea

```forge
var maybe_age: ?int = some(15)
var real_age: int = maybe_age      # Type error
```

Why is this wrong?

Because `maybe_age` might be `none`, and an `int` variable must always contain a real integer.

### Correct idea

You must **check** or **unwrap** first.

```forge
var maybe_age: ?int = some(15)

if maybe_age is some:
    var real_age: int = maybe_age.value
    print(real_age)
```

---

## 7. Checking Whether a Value Exists

FORGE provides two tests:

- `value is some`
- `value is none`

### Example

```forge
var answer: ?int = some(7)

if answer is some:
    print("We have a value")

if answer is none:
    print("No value stored")
```

Most of the time, students will use `is some` before reading the stored value.

---

## 8. Reading the Stored Value with `.value`

If an optional is `some(...)`, the actual value inside it can be read using `.value`.

```forge
var result: ?int = some(25)

if result is some:
    print(result.value)
```

### Very important safety rule

Only use `.value` when you know the optional is `some`.

Bad example:

```forge
var result: ?int = none
print(result.value)      # Runtime error
```

Good example:

```forge
var result: ?int = none

if result is some:
    print(result.value)
else:
    print("No result")
```

---

## 9. Using `or_else` for a Default Value

Sometimes you do not care whether a value is missing.  
You just want a fallback.

Use `or_else`.

```forge
var score: ?int = none
var final_score: int = score or_else 0
print(final_score)     # 0
```

If the optional is `some(value)`, you get that value.  
If the optional is `none`, you get the default on the right side.

More examples:

```forge
var nickname: ?str = none
print(nickname or_else "anonymous")

var retries: ?int = some(3)
print(retries or_else 10)   # prints 3
```

---

## 10. A First Complete Example: Searching a List

This is one of the best beginner uses for optionals.

```forge
proc find_index(arr: []int, target: int) -> ?int:
    for i in range(0, len(arr)):
        if arr[i] == target:
            return some(i)
    return none

proc main() -> void:
    var nums: []int = [10, 20, 30, 40]
    var idx: ?int = find_index(nums, 30)

    if idx is some:
        print("Found at index " + str(idx.value))
    else:
        print("Not found")
```

Why is `?int` perfect here?

- If the number is found, return an index.
- If the number is not found, return `none`.
- We avoid fake values like `-1`.

That is cleaner and safer than “special number means failure.”

---

## 11. Another Example: Optional Student Data

Imagine a class record system.

Some students have a middle name. Some do not.

```forge
record Student:
    first: str
    middle: ?str
    last: str

proc print_student(s: Student) -> void:
    if s.middle is some:
        print(s.first + " " + s.middle.value + " " + s.last)
    else:
        print(s.first + " " + s.last)
```

Example values:

```forge
var a: Student = Student {
    first: "Lena",
    middle: some("Marie"),
    last: "Cole"
}

var b: Student = Student {
    first: "Noah",
    middle: none,
    last: "Kim"
}
```

This is much clearer than pretending a missing middle name is the same as an empty string.

---

## 12. Optional Types with Procedures

Procedures often use optional return values when failure is simple and expected.

### Example: first even number

```forge
proc first_even(arr: []int) -> ?int:
    for n in arr:
        if n % 2 == 0:
            return some(n)
    return none
```

Use it like this:

```forge
var value: ?int = first_even([1, 3, 7, 10, 11])

if value is some:
    print("First even: " + str(value.value))
else:
    print("No even number found")
```

---

## 13. Optional Types with Maps

A map lookup may fail because the key might not exist.

```forge
var scores: map[str, int] = {}
scores["Ada"] = 95
scores["Ben"] = 88

var ben_score: ?int = get(scores, "Ben")
var zoe_score: ?int = get(scores, "Zoe")
```

Now handle the results safely:

```forge
if ben_score is some:
    print("Ben: " + str(ben_score.value))

if zoe_score is none:
    print("No score stored for Zoe")
```

---

## 14. Optional Types with String Conversion

Turning text into a number may succeed or fail.

That makes optionals a natural fit.

```forge
import forge.str

proc main() -> void:
    var a: ?int = to_int("42")
    var b: ?int = to_int("forty-two")

    if a is some:
        print("a = " + str(a.value))

    if b is none:
        print("b could not be converted")
```

This is easier for beginners than using exceptions.

---

## 15. Optionals with Records, Strings, and Arrays

An optional can wrap many kinds of values.

Examples:

```forge
var maybe_name: ?str = some("Mira")
var maybe_numbers: ?[]int = some([1, 2, 3])
var maybe_count: ?int = none
```

You can also use optionals with records:

```forge
record Prize:
    label: str
    points: int

var maybe_prize: ?Prize = some(Prize { label: "Gold Star", points: 10 })
```

Then unwrap normally:

```forge
if maybe_prize is some:
    print(maybe_prize.value.label)
```

---

## 16. When to Use an Optional

Use an optional when the question is mainly:

> “Is there a value, or not?”

Good uses:

- search results
- map lookups
- parsing simple input
- optional fields in records
- configuration settings that may be missing

Optionals are best when there are only two states:

1. a value exists
2. no value exists

---

## 17. When an Optional Is Not Enough

Sometimes you need more than “present or absent.”

Example:

- file missing
- file exists but permission denied
- file exists but content is invalid

In cases like that, a **result record** is often better than `?T` because it can carry a detailed error message.

Simple optional case:

```forge
proc find_bonus(name: str) -> ?int:
    if name == "Ava":
        return some(5)
    return none
```

Richer error case:

```forge
record ParseResult:
    ok: bool
    value: int
    error: str
```

Rule of thumb:

- use `?T` for simple missing/present cases
- use a result record when you need to explain *why* something failed

---

## 18. Common Beginner Mistakes

### Mistake 1: Treating `?int` like `int`

```forge
var maybe_x: ?int = some(10)
var y: int = maybe_x      # wrong
```

Fix: unwrap first.

### Mistake 2: Using `.value` without checking

```forge
var maybe_x: ?int = none
print(maybe_x.value)      # wrong
```

Fix: use `is some` first, or use `or_else`.

### Mistake 3: Using fake failure values

```forge
proc find_age(name: str) -> int:
    return -1
```

Why this is bad:

- `-1` is a real integer, not a clear “missing” marker
- students must remember a hidden rule
- bugs happen when the hidden rule is forgotten

Better:

```forge
proc find_age(name: str) -> ?int:
    return none
```

### Mistake 4: Confusing empty with missing

`""` means “a string exists, but it is empty.”  
`none` means “there is no string value.”

Those are not the same.

---

## 19. Step-by-Step Thinking Process

When you see a `?T`, ask these questions in order:

1. What is the real value type? (`int`, `str`, `Student`, etc.)
2. Could this value be missing?
3. If yes, what should the program do when it is `none`?
4. Do I need to:
   - check with `is some` / `is none`, or
   - use `or_else`?

This habit will make your code much safer.

---

## 20. A Longer Worked Example

Suppose we want to find the highest score in a list.  
But the list may be empty.

```forge
proc highest_score(scores: []int) -> ?int:
    if len(scores) == 0:
        return none

    var best: int = scores[0]

    for i in range(1, len(scores)):
        if scores[i] > best:
            best = scores[i]

    return some(best)
```

Using it:

```forge
proc main() -> void:
    var a: ?int = highest_score([81, 94, 76, 88])
    var b: ?int = highest_score([])

    print("A: " + str(a or_else 0))

    if b is none:
        print("B: no scores available")
```

Why optional is correct here:

- a highest score exists only if the list has at least one item
- `none` clearly represents the empty-list case

---

## 21. Quick Comparison Table

| Situation | Good FORGE Type | Why |
|----------|------------------|-----|
| search may fail | `?int` | maybe an index, maybe nothing |
| optional middle name | `?str` | value may be absent |
| map lookup | `?T` | key may not exist |
| parsing with detailed errors | result record | need more than absent/present |
| always required age | `int` | must always exist |

---

## 22. Practical Rules to Memorize

Memorize these six rules:

1. `?T` means “maybe a `T`, maybe nothing.”
2. Create present values with `some(...)`.
3. Represent absence with `none`.
4. Test with `is some` or `is none`.
5. Read the stored value with `.value` only after checking.
6. Use `or_else` when a default value makes sense.

---

## 23. Mini Practice

Predict what each variable means:

```forge
var a: ?int = some(5)
var b: ?int = none
var c: int = a or_else 0
var d: int = b or_else 0
```

Answers:

- `a` contains an integer
- `b` contains no value
- `c` becomes `5`
- `d` becomes `0`

---

## 24. Practice Exercises

Try these on your own before looking back at the earlier examples:

1. Write a procedure `find_name(id: int) -> ?str` that returns `some(name)` when
   the id is known and `none` when it is not.
2. Imagine an old procedure returns `-1` when a search fails. Rewrite the idea so
   it returns `?int` instead.
3. Given `var middle: ?str = none`, write one line that stores either the middle
   name or the string `"(no middle name)"`.
4. Write an `if` statement that safely prints the value inside
   `var score: ?int = some(98)`.
5. Explain in one sentence why `?int` and `int` are different types.

These are good beginner checks because they force you to practice the five most
important ideas: declaration, construction, checking, unwrapping, and defaults.

---

## 25. Final Advice for Students

Optional types are one of the cleanest safety features in FORGE.

When you are new to programming, they teach a very important lesson:

> A missing value should be handled on purpose, not by accident.

If you remember nothing else, remember this pattern:

```forge
if value is some:
    use(value.value)
else:
    handle_missing_case()
```

That pattern will appear again and again in real FORGE programs.

---

*FORGE Optional Types Guide — beginner edition*