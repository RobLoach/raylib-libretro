# CLAUDE.md — raylib-libretro

A [libretro](https://www.libretro.com/) frontend built with [raylib](https://www.raylib.com). Ships a single-header C library (core API in `include/raylib-libretro.h`) plus a full frontend app (`bin/raylib-libretro.c`). C99, license zlib/libpng.

## Architecture

### Single-header pattern

Headers in `include/` are stb-style. Define the implementation macro **once**, in exactly one translation unit:

```c
#define RAYLIB_LIBRETRO_IMPLEMENTATION
#include "raylib-libretro.h"
```

Each opt-in layer has its own macro — `RAYLIB_LIBRETRO_MENU_IMPLEMENTATION`, `..._PHYSFS_IMPLEMENTATION`, `..._SHADERS_IMPLEMENTATION`, `..._TOUCH_IMPLEMENTATION`, `..._LOGO_IMPLEMENTATION`, `..._ANDROID_IMPLEMENTATION`. The VFS layer is pulled in automatically by `raylib-libretro.h`. A frontend composes only the layers it needs; the example uses just the core header. Guards: include `RAYLIB_LIBRETRO_*_H`, implementation `RAYLIB_LIBRETRO_*_IMPLEMENTATION` / `..._IMPLEMENTATION_ONCE`.

### Global state

The core library keeps all its state in one file-static global, `static LibretroData LIBRETRO` (`raylib-libretro.h`), with non-zero defaults — no context object is threaded through calls, matching raylib's design. Frontend layers follow the same shape with their own file-static globals (e.g. `menu`, `LibretroPhysFS`).

### Core loading

Cores are shared libraries (`.so`/`.dll`/`.dylib`/`.wasm`) loaded at runtime via `dylib.h` from libretro-common, using the `LoadLibretroMethod(S)` macro. `PeekLibretroCoreInfo()` opens a core just far enough to read `retro_get_system_info()` (no full init); `InitLibretro()` does the full load.

### Core metadata: two sources of truth

The menu builds its core list (`ScanLibretroCoreDirectory`) from sibling `<core>.info` files (bundled from the `vendor/libretro-core-info` submodule) — fast, no dlopen. A core binary with no matching `.info` is probed directly via `PeekLibretroCoreInfo()`.

The `.info` fields (`supported_extensions`, `needs_fullpath`, `supports_no_game`) are **pre-load hints for menu filtering only — never a hard gate**. Authoritative values come from the core's runtime `retro_get_system_info()` / `RETRO_ENVIRONMENT_SET_CONTENT_INFO_OVERRIDE` once loaded, and the two can diverge: e.g. `genesis_plus_gx` declares `needs_fullpath="true"` globally (for SegaCD) yet loads cartridge ROMs from memory. Filtering a core out on a coarse `.info` flag is how zipped Genesis games got wrongly rejected — let the loader make the final per-content decision.

## Build

```sh
git submodule update --init    # submodules are required
mkdir build && cd build && cmake .. && cmake --build .
```

Submodules pull raylib, libretro-common, the Nuklear UI stack (nuklear_console, nuklear_gamepad, raylib-nuklear), PhysFS + raylib-physfs, c-vector, raylib-app, and `vendor/libretro-core-info`. A bundled core whose `.info` is missing from that submodule is a **hard CMake error** (`bin/CMakeLists.txt` → `copy_core_info`), since a core with no `.info` would silently never appear in the menu.

Linux deps: `xorg-dev`, `libglu1-mesa-dev`. CMake 3.11+. CMake first tries `find_package(raylib QUIET)`, then falls back to building `vendor/raylib` with a trimmed feature set.

CMake options: `BUILD_RAYLIB_LIBRETRO_EXAMPLE` (ON), `BUILD_RAYLIB_LIBRETRO` (ON).

Targets:
- `raylib-libretro-h` — INTERFACE target (headers / include path)
- `raylib-libretro-static` — static lib (bundles the libretro-common `.c` files it needs)
- `raylib-libretro` — the full frontend app
- `raylib-libretro-basic` — the minimal example

## CI

- **`Tests.yml`** (push / PR to `master`): Linux only — `ubuntu-latest`, GCC, `cmake -DCMAKE_BUILD_TYPE=Debug`. Windows and Web jobs are present but commented out.
- **`Release.yml`** (on release): builds and packages Linux x64, Linux ARM (`ubuntu-24.04-arm`), and Windows (`windows-latest`, MSVC).
- **No unit tests** anywhere — CI only validates that things compile/package. PRs are gated on Linux/GCC, but keep code MSVC-clean since releases build Windows.

## Coding conventions

- **Language:** C99 (not C++).
- **Public API:** `PascalCase` verbs — `InitLibretro`, `DrawLibretro`, `GetLibretroTexture`.
- **Internal helpers:** `Libretro`-prefixed — `LibretroLogger`, etc.
- **All header functions are `static`.**
- **Errors:** return `bool`; log with `TraceLog(LOG_ERROR, ...)`.
- **Memory:** raylib's `MemAlloc` / `MemFree`, never raw `malloc`/`free`. Prefer raylib built-ins (`TextCopy`, `IsFileExtension`, `LoadFileData`, …) over `<string.h>`/stdio.
- **File headers:** raylib-style `/**** … ****/` banners.

## Key files

Core library:
- `include/raylib-libretro.h` — core loading (`dylib`), environment callbacks, pixel-format mapping, audio ring buffer, VFS wiring
- `include/raylib-libretro-vfs.h` — libretro VFS interface implementation, backed by raylib file I/O
- `include/raylib-libretro-config.h` — `rlconfig`: the INI / `.info` parser used for settings and core metadata

Frontend layers (opt-in):
- `include/raylib-libretro-menu.h` — full in-app UI (Nuklear / nuklear_console): core scanning, core picker, game-load orchestration, settings
- `include/raylib-libretro-physfs.h` — PhysFS-backed, zip-aware game loader (mounts archives at `/game`)
- `include/raylib-libretro-shaders.h` — GLSL retro shaders
- `include/raylib-libretro-touch.h` — on-screen / touch controls
- `include/raylib-libretro-android.h` — Android JNI glue (file picker, intents); guarded by `__ANDROID__`
- `include/raylib-libretro-styles.h`, `-logo.h` — menu theme + embedded logo image

Apps & data:
- `example/raylib-libretro-basic.c` — canonical minimal usage; reference for the expected API flow
- `bin/raylib-libretro.c` — full app using `raylib-app` lifecycle callbacks
- `vendor/libretro-core-info/` — submodule of `.info` core metadata; CMake copies the relevant ones next to built cores
- `TASKS.md` — planned features; check here before implementing new work
