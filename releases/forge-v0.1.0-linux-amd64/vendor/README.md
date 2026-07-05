# Vendored Dependencies

This directory contains vendored (bundled) third-party libraries so that
FORGE can be built with GUI support (`make GUI=1`) without installing
external dependencies.

## Contents

### raylib/
- **include/** — raylib headers (`raylib.h`, `raymath.h`, `rlgl.h`, `rcamera.h`) and raygui header (`raygui.h`)
- **lib/** — Pre-built static library (`libraylib.a`) for Linux x86_64

## Licenses
- **raylib** — zlib/libpng license (https://github.com/raysan5/raylib)
- **raygui** — zlib/libpng license (https://github.com/raysan5/raygui)

## Updating

To update raylib:
```bash
git clone --depth 1 https://github.com/raysan5/raylib.git /tmp/raylib_build
cd /tmp/raylib_build/src && make PLATFORM=PLATFORM_DESKTOP
cp /tmp/raylib_build/src/libraylib.a vendor/raylib/lib/
cp /tmp/raylib_build/src/raylib.h /tmp/raylib_build/src/raymath.h \
   /tmp/raylib_build/src/rlgl.h /tmp/raylib_build/src/rcamera.h \
   vendor/raylib/include/
```

To update raygui:
```bash
wget -O vendor/raylib/include/raygui.h \
  https://raw.githubusercontent.com/raysan5/raygui/master/src/raygui.h
```
