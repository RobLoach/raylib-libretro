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

### Hardware-accelerated OpenGL rendering

GPU-rendered cores (Beetle PSX HW, Mupen64Plus-Next, Flycast, etc.) negotiate via `RETRO_ENVIRONMENT_SET_HW_RENDER`. The frontend shares raylib's single GL context with the core — no second/shared/threaded context.

State lives in `LIBRETRO.core.hwRender` (enabled, active, FBO, lifecycle flags). The gate is `hwRender.enabled` (set when `SET_HW_RENDER` succeeds); every HW branch is guarded by it, leaving the software path byte-for-byte untouched.

Per-frame flow (all on the main thread inside `LibretroTick`, outside `BeginDrawing`/`EndDrawing`):
1. Flush raylib's batch (`rlDrawRenderBatchActive`), bind the frontend-owned FBO.
2. `retro_run` — the core calls `get_current_framebuffer` (returns FBO id) and `get_proc_address` (returns `rlGetProcAddress`), renders, then calls `video_refresh(RETRO_HW_FRAME_BUFFER_VALID, w, h, 0)`.
3. Post-run: restore GL state (FBO, viewport, blend, depth, scissor, VAO, sampler objects, pixel-store params) through raylib setters + raw GL so the `RLGL` cache stays consistent.
4. `Draw()` blits `LIBRETRO.core.texture` (aliased to the FBO color attachment) with a vertical flip for `bottom_left_origin` cores.

FBO is sized to `max_width × max_height` from `retro_get_system_av_info`. Stencil-requesting cores get a manually-built `DEPTH24_STENCIL8` renderbuffer (tracked in `hwRender.depthStencilRb`); depth-only cores use `LoadRenderTexture`. `context_reset` fires after `InitLibretroVideo`; `context_destroy` fires in `UnloadLibretroGame` before FBO teardown.

Accepted context types depend on the build: desktop GL 3.3 (`OPENGL`/`OPENGL_CORE` ≤ 3.3), GL 4.3 with opt-in rebuild, GLES2, GLES3 (WebGL2). Vulkan/D3D cores are rejected cleanly. Test core in `tests/test_opengl/`.

### VFS alt-filesystem bridge

The core VFS layer (`raylib-libretro-vfs.h`) exposes three nullable global function-pointer hooks — `raylib_libretro_vfs_alt_load_file_data` / `_stat` / `_load_dir_files`. When set, VFS read/stat/listdir operations consult them *before* the real filesystem and fall back if they return not-found. This is the bridge that lets a `needs_fullpath` core read content that lives **inside a mounted archive**: the PhysFS layer (`raylib-libretro-physfs.h`) installs PhysFS-backed implementations in `InitLibretroPhysFS()` (and clears them in `CloseLibretroPhysFS()`), so when such a core `fopen`s a `/game/...` virtual path, the VFS serves it from the zip. The core layer stays PhysFS-free; the dependency points one way (frontend → core globals).

## Build

```sh
git submodule update --init    # submodules are required
mkdir build && cd build && cmake .. && cmake --build .
```

Submodules pull raylib, libretro-common, the Nuklear UI stack (nuklear_console, nuklear_gamepad, raylib-nuklear), PhysFS + raylib-physfs, c-vector, raylib-app, and `vendor/libretro-core-info`. A bundled core whose `.info` is missing from that submodule is a **hard CMake error** (`bin/CMakeLists.txt` → `copy_core_info`), since a core with no `.info` would silently never appear in the menu.

Linux deps: `xorg-dev`, `libglu1-mesa-dev`. CMake 3.11+. CMake first tries `find_package(raylib QUIET)`, then falls back to building `vendor/raylib` with a trimmed feature set.

CMake options: `BUILD_RAYLIB_LIBRETRO_EXAMPLE` (ON), `BUILD_RAYLIB_LIBRETRO` (ON), `BUILD_RAYLIB_LIBRETRO_TEST_CORES` (ON, skipped on Web).

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
- `tests/test_opengl/test_opengl_libretro.c` — minimal GL 3.3 core that draws a spinning triangle; smoke-tests the HW render path end-to-end
- `vendor/libretro-core-info/` — submodule of `.info` core metadata; CMake copies the relevant ones next to built cores
- `TASKS.md` — planned features; check here before implementing new work
