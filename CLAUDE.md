# CLAUDE.md — raylib-libretro

A [libretro](https://www.libretro.com/) frontend built with [raylib](https://www.raylib.com). Provides a single-header C library (`include/raylib-libretro.h`) and a full frontend executable (`bin/raylib-libretro.c`). License: zlib/libpng.

## Architecture

### Single-Header Library Pattern

Headers in `include/` follow the stb-style pattern. Define the implementation macro **once** in exactly one translation unit:

```c
#define RAYLIB_LIBRETRO_IMPLEMENTATION
#include "raylib-libretro.h"
```

Same for `RAYLIB_LIBRETRO_SHADERS_IMPLEMENTATION` and `RAYLIB_LIBRETRO_MENU_IMPLEMENTATION`.

### Global State

All state lives in a single static global `LibretroData LIBRETRO = {0};`. No context object is passed around — functions operate on the global, matching raylib's design.

### Libretro Core Loading

Cores are shared libraries (`.so`/`.dll`/`.dylib`) loaded at runtime via `dylib.h` from libretro-common, using the `LoadLibretroMethod(S)` macro.

## Build

```sh
git submodule update --init
mkdir build && cd build && cmake .. && cmake --build .
```

Linux deps: `xorg-dev`, `libglu1-mesa-dev`. CMake 3.11+. CMake first tries `find_package(raylib QUIET)`, then falls back to `vendor/raylib`.

CMake options: `BUILD_RAYLIB_LIBRETRO_EXAMPLE` (ON), `BUILD_RAYLIB_LIBRETRO` (ON).

Targets: `raylib-libretro-interface` (headers), `raylib-libretro-static` (static lib), `raylib-libretro` (exe), `raylib-libretro-basic` (example).

## CI

`.github/workflows/Tests.yml` runs CMake Debug builds on Ubuntu (GCC) and Windows (MSVC). **No unit tests** — CI validates compilation only. Ensure changes compile on both platforms.

## Coding Conventions

- **Language:** C99 (not C++)
- **Public API:** `PascalCase` verbs — `InitLibretro`, `DrawLibretro`, `GetLibretroTexture`
- **Internal helpers:** `Libretro`-prefixed — `LibretroLogger`, etc.
- **Static functions:** all functions in headers are `static`
- **Error handling:** return `bool`; log errors with `TraceLog(LOG_ERROR, ...)`
- **Memory:** use raylib's `MemAlloc` / `MemFree`, never raw `malloc`/`free`
- **Include guards:** `RAYLIB_LIBRETRO_*_H`
- **Implementation guards:** `RAYLIB_LIBRETRO_*_IMPLEMENTATION` / `..._IMPLEMENTATION_ONCE`
- **File headers:** raylib-style `/**** ... ****/` banners

## Key Files

- `include/raylib-libretro.h` — the entire library (callbacks, pixel format mapping, audio ring buffer)
- `example/raylib-libretro-basic.c` — canonical minimal usage; reference for the expected API flow
- `bin/raylib-libretro.c` — full app using `raylib-app` lifecycle callbacks
- `TASKS.md` — planned features; check here before implementing new work
