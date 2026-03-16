# Add `forge.gui` Standard Library Module (raylib-backed)

Add a new `forge.gui` standard library module that provides windowing, drawing, input handling, and immediate-mode GUI widgets, backed by [raylib](https://www.raylib.com/) + [raygui](https://github.com/raysan5/raygui). This follows the exact same extension pattern used by `forge.serial`, `forge.buf`, `forge.nmea`, etc.

## User Review Required

> [!IMPORTANT]
> **API Surface Scope Decision**: The plan below proposes a **Phase 1 (minimal)** API of ~30 functions covering window management, basic drawing, text, input, and a few raygui widgets. This is intentionally narrow to get a working vertical slice. Future phases can add sprites/textures, audio, advanced widgets, etc. — is this scope right for an initial release?

> [!IMPORTANT]
> **Interpreter Support**: raylib requires an active OpenGL context with a render loop. The interpreter (`forge run`) can support this since it operates in-process. However, this means `forge run gui_app.fg` will open a graphical window. Is that acceptable, or should GUI only be supported via `forge build` (compiled C)?

> [!WARNING]
> **Dependency**: raylib needs to be installed on the system (or vendored). On Ubuntu/Debian: `sudo apt install libraylib-dev`. On Arch: `pacman -S raylib`. This is the first FORGE stdlib module with an **external C library dependency** (all others use only libc). The Makefile will need conditional linking (`-lraylib`).

## Proposed Phase 1 API

The FORGE user writes:

```python
import forge.gui

proc main() -> void:
    gui.init_window(800, 600, "My App")
    gui.set_target_fps(60)

    while gui.window_open():
        gui.begin_draw()
        gui.clear(40, 40, 40, 255)

        gui.draw_text("Hello FORGE!", 200, 100, 24, 255, 255, 255, 255)
        gui.draw_rect(100, 200, 200, 80, 0, 120, 255, 200)
        gui.draw_circle(400, 300, 50.0, 255, 60, 60, 255)
        gui.draw_line(10, 10, 300, 300, 255, 255, 0, 255)

        if gui.is_key_pressed(32):  # SPACE
            print("Space pressed!")

        var mx: int = gui.mouse_x()
        var my: int = gui.mouse_y()

        if gui.button(300, 400, 120, 40, "Click Me"):
            print("Button clicked!")

        gui.end_draw()

    gui.close_window()
```

### Function List (Phase 1)

| Category | Function | Signature | Returns |
|----------|----------|-----------|---------|
| **Window** | `init_window` | [(width: int, height: int, title: str)](file:///home/chuck/Dropbox/Programming/Languages_and_Code/Programming_Projects/Programming_Tools/LANGUAGES/FORGE/src/interp/interp.c#2169-2172) | `void` |
| | `close_window` | [()](file:///home/chuck/Dropbox/Programming/Languages_and_Code/Programming_Projects/Programming_Tools/LANGUAGES/FORGE/src/interp/interp.c#2169-2172) | `void` |
| | `window_open` | [()](file:///home/chuck/Dropbox/Programming/Languages_and_Code/Programming_Projects/Programming_Tools/LANGUAGES/FORGE/src/interp/interp.c#2169-2172) | `bool` |
| | `set_target_fps` | [(fps: int)](file:///home/chuck/Dropbox/Programming/Languages_and_Code/Programming_Projects/Programming_Tools/LANGUAGES/FORGE/src/interp/interp.c#2169-2172) | `void` |
| | `get_fps` | [()](file:///home/chuck/Dropbox/Programming/Languages_and_Code/Programming_Projects/Programming_Tools/LANGUAGES/FORGE/src/interp/interp.c#2169-2172) | [int](file:///home/chuck/Dropbox/Programming/Languages_and_Code/Programming_Projects/Programming_Tools/LANGUAGES/FORGE/src/interp/interp.c#174-178) |
| | `get_dt` | [()](file:///home/chuck/Dropbox/Programming/Languages_and_Code/Programming_Projects/Programming_Tools/LANGUAGES/FORGE/src/interp/interp.c#2169-2172) | [float](file:///home/chuck/Dropbox/Programming/Languages_and_Code/Programming_Projects/Programming_Tools/LANGUAGES/FORGE/src/interp/interp.c#163-173) |
| **Draw** | `begin_draw` | [()](file:///home/chuck/Dropbox/Programming/Languages_and_Code/Programming_Projects/Programming_Tools/LANGUAGES/FORGE/src/interp/interp.c#2169-2172) | `void` |
| | `end_draw` | [()](file:///home/chuck/Dropbox/Programming/Languages_and_Code/Programming_Projects/Programming_Tools/LANGUAGES/FORGE/src/interp/interp.c#2169-2172) | `void` |
| | `clear` | [(r, g, b, a: int)](file:///home/chuck/Dropbox/Programming/Languages_and_Code/Programming_Projects/Programming_Tools/LANGUAGES/FORGE/src/interp/interp.c#2169-2172) | `void` |
| **Shapes** | `draw_line` | [(x1, y1, x2, y2, r, g, b, a: int)](file:///home/chuck/Dropbox/Programming/Languages_and_Code/Programming_Projects/Programming_Tools/LANGUAGES/FORGE/src/interp/interp.c#2169-2172) | `void` |
| | `draw_rect` | [(x, y, w, h, r, g, b, a: int)](file:///home/chuck/Dropbox/Programming/Languages_and_Code/Programming_Projects/Programming_Tools/LANGUAGES/FORGE/src/interp/interp.c#2169-2172) | `void` |
| | `draw_rect_lines` | [(x, y, w, h, r, g, b, a: int)](file:///home/chuck/Dropbox/Programming/Languages_and_Code/Programming_Projects/Programming_Tools/LANGUAGES/FORGE/src/interp/interp.c#2169-2172) | `void` |
| | `draw_circle` | [(cx, cy: int, radius: float, r, g, b, a: int)](file:///home/chuck/Dropbox/Programming/Languages_and_Code/Programming_Projects/Programming_Tools/LANGUAGES/FORGE/src/interp/interp.c#2169-2172) | `void` |
| | `draw_circle_lines` | [(cx, cy: int, radius: float, r, g, b, a: int)](file:///home/chuck/Dropbox/Programming/Languages_and_Code/Programming_Projects/Programming_Tools/LANGUAGES/FORGE/src/interp/interp.c#2169-2172) | `void` |
| **Text** | `draw_text` | [(text: str, x, y, size, r, g, b, a: int)](file:///home/chuck/Dropbox/Programming/Languages_and_Code/Programming_Projects/Programming_Tools/LANGUAGES/FORGE/src/interp/interp.c#2169-2172) | `void` |
| | `measure_text` | [(text: str, size: int)](file:///home/chuck/Dropbox/Programming/Languages_and_Code/Programming_Projects/Programming_Tools/LANGUAGES/FORGE/src/interp/interp.c#2169-2172) | [int](file:///home/chuck/Dropbox/Programming/Languages_and_Code/Programming_Projects/Programming_Tools/LANGUAGES/FORGE/src/interp/interp.c#174-178) |
| **Input — Keyboard** | `is_key_pressed` | [(key: int)](file:///home/chuck/Dropbox/Programming/Languages_and_Code/Programming_Projects/Programming_Tools/LANGUAGES/FORGE/src/interp/interp.c#2169-2172) | `bool` |
| | `is_key_down` | [(key: int)](file:///home/chuck/Dropbox/Programming/Languages_and_Code/Programming_Projects/Programming_Tools/LANGUAGES/FORGE/src/interp/interp.c#2169-2172) | `bool` |
| | `is_key_released` | [(key: int)](file:///home/chuck/Dropbox/Programming/Languages_and_Code/Programming_Projects/Programming_Tools/LANGUAGES/FORGE/src/interp/interp.c#2169-2172) | `bool` |
| | `get_key_pressed` | [()](file:///home/chuck/Dropbox/Programming/Languages_and_Code/Programming_Projects/Programming_Tools/LANGUAGES/FORGE/src/interp/interp.c#2169-2172) | [int](file:///home/chuck/Dropbox/Programming/Languages_and_Code/Programming_Projects/Programming_Tools/LANGUAGES/FORGE/src/interp/interp.c#174-178) |
| **Input — Mouse** | `mouse_x` | [()](file:///home/chuck/Dropbox/Programming/Languages_and_Code/Programming_Projects/Programming_Tools/LANGUAGES/FORGE/src/interp/interp.c#2169-2172) | [int](file:///home/chuck/Dropbox/Programming/Languages_and_Code/Programming_Projects/Programming_Tools/LANGUAGES/FORGE/src/interp/interp.c#174-178) |
| | `mouse_y` | [()](file:///home/chuck/Dropbox/Programming/Languages_and_Code/Programming_Projects/Programming_Tools/LANGUAGES/FORGE/src/interp/interp.c#2169-2172) | [int](file:///home/chuck/Dropbox/Programming/Languages_and_Code/Programming_Projects/Programming_Tools/LANGUAGES/FORGE/src/interp/interp.c#174-178) |
| | `is_mouse_pressed` | [(button: int)](file:///home/chuck/Dropbox/Programming/Languages_and_Code/Programming_Projects/Programming_Tools/LANGUAGES/FORGE/src/interp/interp.c#2169-2172) | `bool` |
| | `is_mouse_down` | [(button: int)](file:///home/chuck/Dropbox/Programming/Languages_and_Code/Programming_Projects/Programming_Tools/LANGUAGES/FORGE/src/interp/interp.c#2169-2172) | `bool` |
| **Widgets (raygui)** | `button` | [(x, y, w, h: int, text: str)](file:///home/chuck/Dropbox/Programming/Languages_and_Code/Programming_Projects/Programming_Tools/LANGUAGES/FORGE/src/interp/interp.c#2169-2172) | `bool` |
| | `label` | [(x, y, w, h: int, text: str)](file:///home/chuck/Dropbox/Programming/Languages_and_Code/Programming_Projects/Programming_Tools/LANGUAGES/FORGE/src/interp/interp.c#2169-2172) | `void` |
| | `checkbox` | [(x, y, w, h: int, text: str, checked: bool)](file:///home/chuck/Dropbox/Programming/Languages_and_Code/Programming_Projects/Programming_Tools/LANGUAGES/FORGE/src/interp/interp.c#2169-2172) | `bool` |
| | `slider` | [(x, y, w, h: int, min, max, value: float)](file:///home/chuck/Dropbox/Programming/Languages_and_Code/Programming_Projects/Programming_Tools/LANGUAGES/FORGE/src/interp/interp.c#2169-2172) | [float](file:///home/chuck/Dropbox/Programming/Languages_and_Code/Programming_Projects/Programming_Tools/LANGUAGES/FORGE/src/interp/interp.c#163-173) |
| | `textbox` | [(x, y, w, h: int, text: str, max_len: int)](file:///home/chuck/Dropbox/Programming/Languages_and_Code/Programming_Projects/Programming_Tools/LANGUAGES/FORGE/src/interp/interp.c#2169-2172) | [str](file:///home/chuck/Dropbox/Programming/Languages_and_Code/Programming_Projects/Programming_Tools/LANGUAGES/FORGE/src/interp/interp.c#1751-1762) |
| | `set_style_dark` | [()](file:///home/chuck/Dropbox/Programming/Languages_and_Code/Programming_Projects/Programming_Tools/LANGUAGES/FORGE/src/interp/interp.c#2169-2172) | `void` |
| | `set_style_light` | [()](file:///home/chuck/Dropbox/Programming/Languages_and_Code/Programming_Projects/Programming_Tools/LANGUAGES/FORGE/src/interp/interp.c#2169-2172) | `void` |

---

## Proposed Changes

### Runtime Library (C wrappers around raylib)

#### [NEW] [forge_gui.h](file:///home/chuck/Dropbox/Programming/Languages_and_Code/Programming_Projects/Programming_Tools/LANGUAGES/FORGE/runtime/forge_gui.h)

Header declaring all `forge_gui_*()` C functions. Pattern follows [forge_runtime.h](file:///home/chuck/Dropbox/Programming/Languages_and_Code/Programming_Projects/Programming_Tools/LANGUAGES/FORGE/runtime/forge_runtime.h) section structure.

#### [NEW] [forge_gui.c](file:///home/chuck/Dropbox/Programming/Languages_and_Code/Programming_Projects/Programming_Tools/LANGUAGES/FORGE/runtime/forge_gui.c)

Implementation that wraps raylib/raygui calls. Each function is a thin wrapper, e.g.:

```c
void forge_gui_draw_text(forge_str_t text, int64_t x, int64_t y,
                         int64_t size, int64_t r, int64_t g, int64_t b, int64_t a) {
    DrawText(text.data, (int)x, (int)y, (int)size,
             (Color){(uint8_t)r, (uint8_t)g, (uint8_t)b, (uint8_t)a});
}
```

For raygui widgets that modify state (checkbox, slider, textbox), the wrapper manages the state internally using static arrays keyed by position, since FORGE doesn't yet have mutable reference parameters.

---

### Type Checker

#### [MODIFY] [checker.c](file:///home/chuck/Dropbox/Programming/Languages_and_Code/Programming_Projects/Programming_Tools/LANGUAGES/FORGE/src/typecheck/checker.c)

Add a new `else if (strcmp(stdlib_module, "gui") == 0)` block ~line 1230, following the [nmea](file:///home/chuck/Dropbox/Programming/Languages_and_Code/Programming_Projects/Programming_Tools/LANGUAGES/FORGE/src/interp/interp.c#3436-3531) block. This maps each `forge.gui.*` function name to its return type (`TY_VOID`, `TY_BOOL`, `TY_INT`, `TY_FLOAT`, or `TY_STR`).

---

### Interpreter

#### [MODIFY] [interp.c](file:///home/chuck/Dropbox/Programming/Languages_and_Code/Programming_Projects/Programming_Tools/LANGUAGES/FORGE/src/interp/interp.c)

1. Add forward declaration: `static int try_stdlib_gui(...)` (~line 40)
2. Add dispatch entries in **both** dispatch blocks (~lines 860 and 924):
   ```c
   } else if (strcmp(mod_name, "forge.gui") == 0) {
       handled = try_stdlib_gui(interp, proc_name, args, arg_count, &stdlib_result);
   ```
3. Add the `try_stdlib_gui()` function (~after the [try_stdlib_nmea](file:///home/chuck/Dropbox/Programming/Languages_and_Code/Programming_Projects/Programming_Tools/LANGUAGES/FORGE/src/interp/interp.c#3436-3531) implementation, ~line 3400+): string-match on function name, call the corresponding `forge_gui_*()` runtime function, and wrap the return value in a `forge_value_t`.

---

### Build System

#### [MODIFY] [Makefile](file:///home/chuck/Dropbox/Programming/Languages_and_Code/Programming_Projects/Programming_Tools/LANGUAGES/FORGE/Makefile)

- Add `runtime/forge_gui.c` to `SRC_RUNTIME` (it's already picked up by the wildcard `$(wildcard runtime/*.c)`, so this happens automatically).
- Add raylib linker flags: change the link line to include `-lraylib -lGL -lm -lpthread -ldl -lrt -lX11`. This could be conditional via a `GUI=1` flag:

```makefile
ifeq ($(GUI),1)
  LDFLAGS += -lraylib -lGL -lpthread -ldl -lrt -lX11
  CFLAGS  += -DFORGE_HAS_GUI
endif
```

This way `make` still works without raylib installed, and `make GUI=1` enables GUI support.

---

### C Emitter (optional, Phase 1 skip)

#### [MODIFY] [emit_c.c](file:///home/chuck/Dropbox/Programming/Languages_and_Code/Programming_Projects/Programming_Tools/LANGUAGES/FORGE/src/emit_c/emit_c.c)

The C emitter currently doesn't handle serial/buf/nmea either. For Phase 1, we can defer C emitter support for `forge.gui` to stay consistent. When ready, it would emit calls like `forge_gui_init_window(...)` just as it does for `forge_io_print(...)`.

---

### Tests and Examples

#### [NEW] [test_stdlib_gui.fg](file:///home/chuck/Dropbox/Programming/Languages_and_Code/Programming_Projects/Programming_Tools/LANGUAGES/FORGE/tests/forge/test_stdlib_gui.fg)

A FORGE test program that exercises the GUI API. Since GUI is interactive (opens a window), this won't have a [.expected](file:///home/chuck/Dropbox/Programming/Languages_and_Code/Programming_Projects/Programming_Tools/LANGUAGES/FORGE/tests/forge/fib.expected) file — it'll be a manual/visual test, similar to [test_serial_loopback.fg](file:///home/chuck/Dropbox/Programming/Languages_and_Code/Programming_Projects/Programming_Tools/LANGUAGES/FORGE/tests/forge/test_serial_loopback.fg) and [test_stdlib_serial.fg](file:///home/chuck/Dropbox/Programming/Languages_and_Code/Programming_Projects/Programming_Tools/LANGUAGES/FORGE/tests/forge/test_stdlib_serial.fg) which also have no [.expected](file:///home/chuck/Dropbox/Programming/Languages_and_Code/Programming_Projects/Programming_Tools/LANGUAGES/FORGE/tests/forge/fib.expected) files.

#### [NEW] [gui_hello.fg](file:///home/chuck/Dropbox/Programming/Languages_and_Code/Programming_Projects/Programming_Tools/LANGUAGES/FORGE/examples/gui_hello.fg)

Simple showcase example for documentation.

---

## Verification Plan

### Automated Tests

1. **Existing test suite passes** (no regressions):
   ```bash
   cd /home/chuck/Dropbox/Programming/Languages_and_Code/Programming_Projects/Programming_Tools/LANGUAGES/FORGE
   make clean && make GUI=1 && bash tests/runner.sh
   ```
   All existing tests must still pass since GUI is additive.

2. **Build without GUI flag** (no raylib dependency required for non-GUI users):
   ```bash
   make clean && make && bash tests/runner.sh
   ```
   Must compile and pass all tests even without raylib installed.

### Manual Verification

3. **Run the GUI test program** and visually confirm a window opens with shapes, text, and interactive widgets:
   ```bash
   ./forge run tests/forge/test_stdlib_gui.fg
   ```
   **Expected**: A window appears with colored shapes, text, and clickable buttons. Pressing keys and clicking should produce console output. Closing the window should exit cleanly without crashes or leaks.

4. **Run under valgrind** to check for memory leaks:
   ```bash
   make valgrind ARGS="run examples/gui_hello.fg"
   ```
   **Expected**: No leaks from FORGE code (raylib's internal allocations are expected and acceptable).
