# FORGE Deployment Guide

How to package and deploy the FORGE toolchain for testing or distribution
outside the development environment.

---

## Quick Summary

To deploy FORGE, you need exactly **4 items**:

| Item | Description |
|------|-------------|
| `forge` | The compiled binary |
| `runtime/` | C runtime library (needed by `forge build`) |
| `vendor/` | Vendored GUI libraries (only for GUI=1 builds) |
| `examples/` | Example programs (optional, for testing) |

---

## Step-by-Step Deployment

### 1. Build the Release Binary

```bash
# Standard build (no GUI)
make clean
make

# OR — with GUI support
make clean
make GUI=1
```

For a more optimized binary:

```bash
make clean
make release           # Without GUI
make release GUI=1     # With GUI
```

### 2. Create a Distribution Folder

```bash
mkdir forge-dist
```

### 3. Copy Required Files

```bash
# The binary (REQUIRED)
cp forge forge-dist/

# The runtime library (REQUIRED for `forge build`)
cp -r runtime/ forge-dist/runtime/

# Vendored GUI libraries (REQUIRED only if built with GUI=1)
cp -r vendor/ forge-dist/vendor/

# Examples (OPTIONAL — good for testing)
cp -r examples/ forge-dist/examples/

# User documentation (OPTIONAL)
cp -r user_docs/ forge-dist/user_docs/
```

### 4. Verify the Distribution

```bash
cd forge-dist

# Test interpreter
./forge run examples/gui_hello.fg         # GUI test (if GUI=1 build)

# Test a simple console program
echo 'proc main() -> void:
    print("Hello from deployed FORGE!")
' > /tmp/test_deploy.fg
./forge run /tmp/test_deploy.fg

# Test compiler (requires gcc on the target machine)
./forge build /tmp/test_deploy.fg -o /tmp/test_deploy
/tmp/test_deploy
```

---

## Complete File Listing

Here is exactly what should be in your distribution folder:

```
forge-dist/
├── forge                          # The toolchain binary (~1.2 MB)
├── runtime/
│   ├── forge_runtime.h            # Runtime header
│   ├── forge_runtime.c            # Runtime implementation
│   ├── forge_gui.h                # GUI module header
│   └── forge_gui.c                # GUI module implementation
├── vendor/                        # (only for GUI builds)
│   ├── README.md
│   └── raylib/
│       ├── include/
│       │   ├── raylib.h
│       │   ├── raymath.h
│       │   ├── rlgl.h
│       │   ├── rcamera.h
│       │   └── raygui.h
│       └── lib/
│           └── libraylib.a        # Pre-built static library
├── examples/                      # (optional)
│   ├── gui_hello.fg
│   └── nmea_terminal.fg
└── user_docs/                     # (optional)
    ├── FORGE_Usage_Guide.md
    ├── FORGE_GUI_Library_Guide.md
    ├── quick_reference.md
    └── ...
```

---

## What Each Component Does

### `forge` binary
The main toolchain. Handles `run`, `build`, `check`, `fmt`, `emit`, and `repl` commands.
This is the only file needed to **interpret** `.fg` programs (`forge run`).

### `runtime/` directory
Contains the C runtime library source code. This is needed when using
`forge build` to compile FORGE programs to native executables, because the
generated C code `#include`s these headers and links against the runtime.

**Without this folder:** `forge run` still works, but `forge build` will fail.

### `vendor/` directory
Contains vendored copies of raylib (static library + headers) and raygui
(header). Only needed if the `forge` binary was compiled with `GUI=1`.

**Without this folder (non-GUI build):** Everything works normally.

---

## Target Machine Requirements

### For `forge run` (interpreter only)

| Requirement | Details |
|-------------|---------|
| Linux x86_64 | The binary is compiled for this architecture |
| No other dependencies | Self-contained for non-GUI programs |

### For `forge run` with GUI

| Requirement | Details |
|-------------|---------|
| Linux x86_64 | Binary architecture |
| OpenGL | Usually pre-installed (`libGL.so`) |
| X11 | Usually pre-installed (`libX11.so`) |
| pthreads | Part of glibc |

### For `forge build` (compiler)

| Requirement | Details |
|-------------|---------|
| GCC or Clang | Used to compile generated C code |
| `runtime/` folder | Must be accessible (beside the binary or via `--runtime` flag) |

---

## One-Line Deploy Script

Copy this to create a ready-to-distribute tarball:

```bash
# From the FORGE project directory, after building:
mkdir -p forge-dist && \
cp forge forge-dist/ && \
cp -r runtime/ forge-dist/runtime/ && \
cp -r vendor/ forge-dist/vendor/ && \
cp -r examples/ forge-dist/examples/ && \
cp -r user_docs/ forge-dist/user_docs/ && \
tar -czf forge-dist.tar.gz forge-dist/ && \
echo "Created forge-dist.tar.gz ($(du -sh forge-dist.tar.gz | cut -f1))"
```

On the target machine:

```bash
tar -xzf forge-dist.tar.gz
cd forge-dist
./forge run examples/gui_hello.fg
```

---

## Cross-Platform Notes

The current vendored `libraylib.a` is built for **Linux x86_64**. To deploy on
a different platform, you would need to rebuild raylib for that target. See
`vendor/README.md` for instructions.

---

*FORGE Deployment Guide v0.1 — Fragillidae Software*
