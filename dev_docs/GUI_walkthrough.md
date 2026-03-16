# Walkthrough: `forge.gui` Standard Library Module

## Summary

Added a new `forge.gui` standard library module backed by **raylib + raygui**, providing windowing, drawing, input handling, and immediate-mode GUI widgets. The module follows the exact same pattern as existing stdlib modules (`forge.serial`, `forge.buf`, `forge.nmea`).

## Files Created

| File | Purpose |
|------|---------|
| [forge_gui.h](file:///home/chuck/Dropbox/Programming/Languages_and_Code/Programming_Projects/Programming_Tools/LANGUAGES/FORGE/runtime/forge_gui.h) | Header declaring ~30 `forge_gui_*()` C functions |
| [forge_gui.c](file:///home/chuck/Dropbox/Programming/Languages_and_Code/Programming_Projects/Programming_Tools/LANGUAGES/FORGE/runtime/forge_gui.c) | Implementation wrapping raylib/raygui behind `#ifdef FORGE_HAS_GUI`, with stub fallbacks |
| [test_stdlib_gui.fg](file:///home/chuck/Dropbox/Programming/Languages_and_Code/Programming_Projects/Programming_Tools/LANGUAGES/FORGE/tests/forge/test_stdlib_gui.fg) | Interactive test exercising shapes, text, input, and widgets |
| [gui_hello.fg](file:///home/chuck/Dropbox/Programming/Languages_and_Code/Programming_Projects/Programming_Tools/LANGUAGES/FORGE/examples/gui_hello.fg) | Minimal example for documentation |

## Files Modified

| File | Change |
|------|--------|
| [checker.c](file:///home/chuck/Dropbox/Programming/Languages_and_Code/Programming_Projects/Programming_Tools/LANGUAGES/FORGE/src/typecheck/checker.c) | Added return type mappings for all `forge.gui.*` functions |
| [interp.c](file:///home/chuck/Dropbox/Programming/Languages_and_Code/Programming_Projects/Programming_Tools/LANGUAGES/FORGE/src/interp/interp.c) | Added include, forward declaration, dispatch entries, and full [try_stdlib_gui()](file:///home/chuck/Dropbox/Programming/Languages_and_Code/Programming_Projects/Programming_Tools/LANGUAGES/FORGE/src/interp/interp.c#3543-3784) (~250 lines) |
| [Makefile](file:///home/chuck/Dropbox/Programming/Languages_and_Code/Programming_Projects/Programming_Tools/LANGUAGES/FORGE/Makefile) | Added conditional `GUI=1` build flag with `LDFLAGS` for raylib |

## Build Verification

All new and modified files compile cleanly with zero warnings:

| File | Result |
|------|--------|
| [runtime/forge_gui.c](file:///home/chuck/Dropbox/Programming/Languages_and_Code/Programming_Projects/Programming_Tools/LANGUAGES/FORGE/runtime/forge_gui.c) | ✅ Clean |
| [src/interp/interp.c](file:///home/chuck/Dropbox/Programming/Languages_and_Code/Programming_Projects/Programming_Tools/LANGUAGES/FORGE/src/interp/interp.c) | ✅ Clean |
| [src/typecheck/checker.c](file:///home/chuck/Dropbox/Programming/Languages_and_Code/Programming_Projects/Programming_Tools/LANGUAGES/FORGE/src/typecheck/checker.c) | ✅ Clean |

> [!NOTE]
> Full `make` fails on a **pre-existing** issue in [forge_runtime.c](file:///home/chuck/Dropbox/Programming/Languages_and_Code/Programming_Projects/Programming_Tools/LANGUAGES/FORGE/runtime/forge_runtime.c) (`usleep` implicit declaration — needs `_DEFAULT_SOURCE`). This is unrelated to our changes.

## How to Use

### Install raylib (one-time)

```bash
# Ubuntu/Debian
sudo apt install libraylib-dev

# Or build from source
git clone https://github.com/raysan5/raylib.git
cd raylib/src && make PLATFORM=PLATFORM_DESKTOP && sudo make install
```

### Install raygui (one-time, header-only)

```bash
# Download raygui.h to where your compiler can find it
sudo wget -O /usr/local/include/raygui.h \
  https://raw.githubusercontent.com/raysan5/raygui/master/src/raygui.h
```

### Build and run

```bash
make clean && make GUI=1
./forge run examples/gui_hello.fg
```

### Build without GUI (no raylib needed)

```bash
make clean && make
# All non-GUI tests and programs work normally
```
