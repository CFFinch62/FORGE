# FORGE Memory Management Guide

**Audience:** Beginning students learning FORGE as an early C-like language  
**Focus:** Understanding where values live, how long they live, and when cleanup is needed

---

## 1. What “Memory Management” Means

When a program runs, it stores values somewhere in computer memory.

Examples:

- numbers
- strings
- arrays
- records
- maps

Memory management means answering three questions:

1. **Where is this value stored?**
2. **How long does it stay alive?**
3. **Who is responsible for cleaning it up?**

FORGE is designed to help students learn these ideas clearly.

---

## 2. The Big Picture in FORGE

FORGE uses two main memory areas:

- **stack** — automatic, short-lived storage
- **heap** — manually managed, flexible storage

Very roughly:

- simple local values usually live on the **stack**
- dynamic data structures usually live on the **heap**

FORGE does **not** use garbage collection.

That means some values are cleaned up automatically, but some must be cleaned up by the programmer.

---

## 3. The Stack: Automatic Local Storage

The stack is the default place for many local values inside a procedure.

Typical stack-allocated values include:

- local primitive variables
- fixed arrays
- record values declared locally

Example:

```forge
proc compute() -> int:
    var x: int = 5
    var buf: [byte; 16]
    return x
```

Here:

- `x` is stack-allocated
- `buf` is stack-allocated
- both are automatically destroyed when `compute()` finishes

You do **not** call `free()` on these.

---

## 4. Why the Stack Is Beginner-Friendly

The stack is simple because its lifetime follows scope.

If a variable is local to a block or procedure:

- it appears when execution enters that scope
- it disappears when execution leaves that scope

That is why stack memory feels “automatic.”

Example:

```forge
proc demo() -> void:
    var a: int = 10

    if a > 0:
        var b: int = 20
        print(b)

    print(a)
```

`b` only exists inside the `if` block.  
`a` exists for the whole procedure.

---

## 5. The Heap: Flexible but Manual Storage

The heap is used for data that needs dynamic size or a longer, less automatic lifetime.

Common heap-allocated data in FORGE:

- dynamic arrays: `[]T`
- maps: `map[K, V]`
- many runtime-created strings

Heap allocation is more flexible than stack allocation, but it comes with responsibility.

If you create heap data, you often need to free it when you are done.

---

## 6. Stack vs Heap at a Glance

| Feature | Stack | Heap |
|--------|-------|------|
| usual purpose | local, fixed-size values | dynamic or flexible data |
| cleanup | automatic | manual or scope-managed |
| speed | usually simple/fast | more costly than stack |
| examples | `int`, local record, `[byte; 64]` | `[]int`, `map[str, int]`, created strings |

---

## 7. Fixed Arrays Live on the Stack

Fixed arrays have a size known at compile time.

```forge
var counts: [int; 4]
var word: [byte; 8]
```

Inside a procedure, these are stack values.

Example:

```forge
proc use_buffer() -> void:
    var buf: [byte; 64]
    buf[0] = 65
    print(buf[0])
```

No `free()` is needed.

---

## 8. Dynamic Arrays Live on the Heap

Dynamic arrays can grow and shrink, so they use heap storage.

```forge
var nums: []int = [1, 2, 3]
append(nums, 4)
```

This is not like a fixed array. It uses managed heap memory behind the scenes.

Because it is heap-based, it must be handled more carefully.

---

## 9. Maps Also Live on the Heap

Maps are dynamic collections too.

```forge
var scores: map[str, int] = {}
scores["Ava"] = 95
scores["Ben"] = 88
```

Maps use heap storage and should be freed when you are done with them.

```forge
free(scores)
```

---

## 10. Explicit Allocation with `alloc`

FORGE provides `alloc(Type, count)` for explicit heap allocation.

Important detail from the language spec:

- `alloc([]byte, 256)` returns `?[]byte`
- that means allocation may fail

So beginners should write code like this:

```forge
var maybe_buf: ?[]byte = alloc([]byte, 256)

if maybe_buf is none:
    print("Out of memory")
    return

var buf: []byte = maybe_buf.value
```

This is a great example of optionals and memory management working together.

---

## 11. Freeing Heap Memory with `free`

When heap memory is no longer needed, release it with `free(...)`.

Example:

```forge
var maybe_buf: ?[]byte = alloc([]byte, 256)

if maybe_buf is some:
    var buf: []byte = maybe_buf.value
    buf[0] = 42
    free(buf)
```

Think of `free` as saying:

> “This heap memory is finished. The program must not use it anymore.”

---

## 12. The Golden Rule: Never Use Memory After `free`

This mistake is called **use-after-free**.

Bad example:

```forge
var maybe_buf: ?[]byte = alloc([]byte, 16)
if maybe_buf is some:
    var buf: []byte = maybe_buf.value
    free(buf)
    print(buf[0])      # wrong
```

Why is this wrong?

Because after `free(buf)`, that memory no longer belongs to your program in a safe way.

After freeing, do not:

- read from it
- write to it
- pass it to another procedure

---

## 13. Another Dangerous Bug: Double Free

Double free means calling `free` twice on the same heap value.

Bad example:

```forge
var maybe_buf: ?[]byte = alloc([]byte, 16)
if maybe_buf is some:
    var buf: []byte = maybe_buf.value
    free(buf)
    free(buf)      # wrong
```

This is a serious bug.

Rule:

- every heap allocation should be freed exactly once

---

## 14. Never Free Stack Data

This is another common beginner error.

Bad example:

```forge
proc wrong() -> void:
    var x: int = 5
    free(x)          # wrong
```

Why wrong?

- `x` lives on the stack
- stack values are cleaned up automatically
- `free` is only for heap-managed values

Also wrong:

```forge
proc wrong() -> void:
    var buf: [byte; 32]
    free(buf)        # wrong
```

Fixed arrays declared locally are stack values too.

---

## 15. `with alloc`: The Safer Beginner Pattern

FORGE includes a scope-managed form called `with alloc`.

```forge
with alloc([]byte, 1024) as buf:
    fill_buffer(buf)
    process(buf)
```

At the end of the block, `buf` is automatically freed.

This is often the best pattern for students because it reduces cleanup mistakes.

Think of it like this:

> “Create this heap memory only for this block, then clean it up automatically.”

---

## 16. Why `with alloc` Is So Helpful

Without `with alloc`, beginners must remember:

1. allocate
2. use the memory
3. free the memory on every path out of the code

That becomes easy to forget, especially with `if` and `return`.

With `with alloc`, the language handles that pattern for you.

Example:

```forge
with alloc([]byte, 128) as buf:
    if len(buf) == 0:
        return
    process(buf)
```

Even if the body returns early, the allocation is still cleaned up.

---

## 17. Strings and Memory

Strings need special attention.

### 17.1 String literals

A literal like this:

```forge
"hello"
```

is stored in the program's read-only data area.  
You do not free string literals.

### 17.2 Strings created during execution

Operations such as concatenation can create new strings.

```forge
var name: str = "Ava"
var message: str = "Hello, " + name
```

According to the spec, runtime-created strings may use heap memory.

### 17.3 Temporaries vs stored strings

The spec simplifies beginner usage:

- temporary string results used inside one statement are cleaned up automatically
- if you assign a created string to a variable, it becomes a value you should treat as real stored data

Example:

```forge
print("Hello, " + name)
```

The temporary string used for printing is cleaned up after the statement.

Example:

```forge
var greeting: str = "Hello, " + name
```

Now `greeting` is stored for later use.

---

## 18. Module-Level Variables

Variables declared outside procedures have **static storage duration**.

```forge
var _packet_count: int = 0
var _port_open: bool = false
```

These exist for the lifetime of the program.

They are not local stack variables, and they are not manually freed like heap allocations.

---

## 19. Lifetime Means “How Long a Value Lives”

This is one of the most important concepts in all systems programming.

### Stack lifetime

- tied to scope
- ends automatically

### Heap lifetime

- begins when allocated or created
- ends when freed

### Static lifetime

- lasts for the entire program

If you understand lifetime, you understand a large part of memory management.

---

## 20. A Simple Stack Example

```forge
proc add_one(n: int) -> int:
    var result: int = n + 1
    return result
```

`n` and `result` are local values.  
They exist only while `add_one` is running.

When the procedure returns, they are gone.

---

## 21. A Simple Heap Example

```forge
proc demo() -> void:
    var maybe_data: ?[]byte = alloc([]byte, 64)

    if maybe_data is none:
        print("Allocation failed")
        return

    var data: []byte = maybe_data.value
    data[0] = 99
    print(data[0])
    free(data)
```

Here the lifetime of `data` is controlled manually.

---

## 22. A Safer Heap Example with `with alloc`

```forge
proc demo() -> void:
    with alloc([]byte, 64) as data:
        data[0] = 99
        print(data[0])
```

This version is shorter and safer because cleanup is automatic at the end of the block.

---

## 23. Choosing Between Fixed and Dynamic Arrays

Students often ask:

> “Should I use `[byte; 64]` or `[]byte`?”

Use a fixed array when:

- the size is known in advance
- the size stays the same
- you want simple automatic storage

Use a dynamic array when:

- the size may change
- the amount of data is not known ahead of time
- you need heap-based flexible storage

Example:

```forge
var fixed: [byte; 64]
var dynamic: []byte = [1, 2, 3]
```

---

## 24. Common Memory Mistakes for Beginners

### Mistake 1: Forgetting to free heap memory

This causes a **memory leak**.

```forge
proc leak() -> void:
    var maybe_buf: ?[]byte = alloc([]byte, 100)
    if maybe_buf is some:
        var buf: []byte = maybe_buf.value
        print(len(buf))
        # forgot free(buf)
```

### Mistake 2: Freeing the wrong thing

Example: freeing a stack variable.

### Mistake 3: Using memory after it has been freed

Example: reading `buf[0]` after `free(buf)`.

### Mistake 4: Freeing twice

Example: calling `free(buf)` two times.

---

## 25. A Memory Leak Explained Simply

A memory leak means:

> the program asked for heap memory and then lost track of it without freeing it

If this happens repeatedly in a long-running program, memory use grows and grows.

That is why matching allocation and cleanup matters.

---

## 26. Matching Patterns to Memorize

### Manual pattern

```forge
var maybe_buf: ?[]byte = alloc([]byte, 256)
if maybe_buf is none:
    return

var buf: []byte = maybe_buf.value
# use buf
free(buf)
```

### Scope-managed pattern

```forge
with alloc([]byte, 256) as buf:
    # use buf
```

If both patterns solve the same problem, beginners should usually prefer the second one.

---

## 27. Thinking About Ownership in a Simple Way

FORGE v1 does not fully enforce ownership rules, but students should still think in these terms:

- “Who is responsible for freeing this value?”
- “Is this value still alive?”
- “Am I keeping it longer than its scope?”

That mental habit prevents many bugs.

---

## 28. References and Lifetime

FORGE supports `ref` parameters.

Example:

```forge
proc swap(a: ref int, b: ref int) -> void:
    var tmp: int = a
    a = b
    b = tmp
```

The key beginner lesson is this:

> Do not keep using a reference to local data after the local scope has ended.

If a value was local to a procedure, it should not be treated as if it lives forever.

---

## 29. Safety Rules from the Spec

The FORGE spec gives four especially important memory rules:

1. Every `alloc` should have a matching `free`, or be managed by `with alloc`
2. Do not free stack-allocated data
3. Do not access memory after `free`
4. Do not keep using local-scope data after its scope ends

Students should memorize these.

---

## 30. A Worked Example: Processing Input Bytes

```forge
proc process_packet() -> void:
    var maybe_buf: ?[]byte = alloc([]byte, 32)

    if maybe_buf is none:
        print("Could not allocate packet buffer")
        return

    var buf: []byte = maybe_buf.value

    for i in range(0, len(buf)):
        buf[i] = byte(i)

    print("First byte: " + str(buf[0]))
    print("Last byte: " + str(buf[len(buf) - 1]))

    free(buf)
```

What this teaches:

- allocation can fail
- `alloc` returns an optional
- heap values must be freed
- use ends before `free`

---

## 31. The Same Example, Better for Students

```forge
proc process_packet() -> void:
    with alloc([]byte, 32) as buf:
        for i in range(0, len(buf)):
            buf[i] = byte(i)

        print("First byte: " + str(buf[0]))
        print("Last byte: " + str(buf[len(buf) - 1]))
```

This version is usually easier to teach because the lifetime is clearly limited to one block.

---

## 32. Quick Questions Students Should Ask Themselves

When you write FORGE code, pause and ask:

1. Is this value stack, heap, or static?
2. If it is heap data, who frees it?
3. Could I use `with alloc` here?
4. Am I using any value after its lifetime ends?

These four questions will prevent many memory bugs.

---

## 33. Summary Table

| Kind of value | Example | Where it usually lives | Cleanup |
|--------------|---------|------------------------|---------|
| local integer | `var x: int = 5` | stack | automatic |
| local fixed array | `var buf: [byte; 64]` | stack | automatic |
| local record | `var p: Point = ...` | stack | automatic |
| dynamic array | `var a: []int = [1, 2, 3]` | heap | free or scope-managed |
| map | `var m: map[str, int] = {}` | heap | free |
| string literal | `"hello"` | static/read-only area | no free |
| module variable | `var _count: int = 0` | static storage | program lifetime |

---

## 34. Practical Rules to Memorize

Memorize these eight rules:

1. Local simple values usually live on the stack.
2. Fixed arrays are usually stack values.
3. Dynamic arrays and maps usually live on the heap.
4. `alloc` may fail, so check its optional result.
5. Heap memory must be freed exactly once.
6. Never free stack data.
7. Never use memory after `free`.
8. Prefer `with alloc` when the allocation only belongs to one block.

---

## 35. Practice Exercises

Try these after reading the guide:

1. For each value, decide whether it is mainly stack, heap, or static data:
   `var x: int = 5`, `var buf: [byte; 64]`, `"hello"`, and a buffer created by
   `forge.buf.create(64)`.
2. Find the bug in this idea: allocate heap memory, use it, and return without
   freeing it.
3. Find the bug in this idea: free a buffer and then read `buf[0]`.
4. Choose the better type for each case: `[byte; 64]` or `[]byte`.
   - a packet that is always exactly 64 bytes
   - a list of readings whose length changes during the program
5. Rewrite a manual `alloc`/`free` pattern as a `with alloc` pattern.

These questions help students practice the three core habits of memory-safe
thinking: identify where data lives, identify who cleans it up, and identify when
its lifetime ends.

---

## 36. Final Advice for Students

Memory management can sound scary at first, but FORGE keeps the basic model clean:

- stack for automatic local storage
- heap for flexible dynamic storage
- explicit cleanup when needed

If you stay disciplined about lifetime and cleanup, you will write much safer low-level code.

If you remember only one sentence, remember this one:

> Know where your value lives, know how long it lives, and know who cleans it up.

---

*FORGE Memory Management Guide — beginner edition*
