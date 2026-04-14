# CLAUDE.md — raylib-libretro

This file provides guidance for AI assistants working with this codebase.

## Project Overview

**raylib-libretro** (v0.0.7) is a [libretro](https://www.libretro.com/) frontend built with [raylib](https://www.raylib.com). It provides:

1. `raylib-libretro.h` — a single-header C library that integrates the libretro API into any raylib application
2. `raylib-libretro` — a full frontend executable with shaders, save states, and a menu UI
3. Supporting headers for shaders, VFS, and menus

The project is in early development. License: zlib/libpng (allows static linking with closed-source software).

---

## Repository Structure

```
raylib-libretro/
├── .github/workflows/Tests.yml   # CI: Linux + Windows build validation
├── bin/
│   ├── CMakeLists.txt
│   └── raylib-libretro.c         # Full frontend executable (entry point)
├── example/
│   ├── CMakeLists.txt
│   └── raylib-libretro-basic.c   # Minimal integration example (~85 lines)
├── include/
│   ├── CMakeLists.txt
│   ├── raylib-libretro.h         # Core libretro↔raylib integration (~2400 lines)
│   ├── raylib-libretro-menu.h    # Nuklear-based menu UI
│   ├── raylib-libretro-shaders.h # Post-process shader system
│   ├── raylib-libretro-vfs.h     # Virtual file system
│   └── raylib-libretro-shaders/  # 28 GLSL shader source files (100/120/330)
├── lib/
│   ├── CMakeLists.txt
│   └── raylib-libretro-static.c  # Thin wrapper for static library target
├── src/
│   └── screenshot.png
├── vendor/                        # Git submodules (see Dependencies)
├── CMakeLists.txt                 # Root build config
├── LICENSE
├── README.md
└── TASKS.md                       # Planned features / roadmap
```

---

## Architecture

### Single-Header Library Pattern

All headers in `include/` follow the stb-style single-header pattern. The implementation is compiled by defining a macro **once** in exactly one translation unit:

```c
// In one .c file only:
#define RAYLIB_LIBRETRO_IMPLEMENTATION
#include "raylib-libretro.h"

#define RAYLIB_LIBRETRO_SHADERS_IMPLEMENTATION
#include "raylib-libretro-shaders.h"

#define RAYLIB_LIBRETRO_MENU_IMPLEMENTATION
#include "raylib-libretro-menu.h"
```

Other files include the headers without the `_IMPLEMENTATION` define.

### Global State

The library holds all state in a single static global:

```c
static rLibretro LibretroCore = {0};
```

There is no explicit context object passed around — all functions operate on this global. This matches raylib's own design pattern.

### Libretro Core Loading

Cores (emulators) are shared libraries (`.so`/`.dll`/`.dylib`) loaded at runtime using `dylib.h` from libretro-common. The macro pattern for loading symbols is:

```c
#define LoadLibretroMethod(S) LoadLibretroMethodHandle(LibretroCore.S, S)
```

---

## Core API Lifecycle

The minimal usage sequence (see `example/raylib-libretro-basic.c`):

```c
InitWindow(...);
InitAudioDevice();

InitLibretro(core_path);         // Dynamically load the core .so/.dll
LoadLibretroGame(game_path);     // Load ROM (NULL for content-less cores)

while (!WindowShouldClose() && !LibretroShouldClose()) {
    UpdateLibretro();            // Run one emulation frame
    BeginDrawing();
        DrawLibretro();          // Render core framebuffer to screen
    EndDrawing();
}

UnloadLibretroGame();
CloseLibretro();
CloseAudioDevice();
CloseWindow();
```

### Full API Reference (raylib-libretro.h)

**Lifecycle:**
- `InitLibretro(core)` / `CloseLibretro()`
- `LoadLibretroGame(file)` / `UnloadLibretroGame()`
- `ResetLibretro()`

**Per-frame:**
- `UpdateLibretro()` — run one core frame
- `LibretroShouldClose()` — core requested shutdown

**Drawing (multiple overloads):**
- `DrawLibretro()` — centered
- `DrawLibretroTint(color)`
- `DrawLibretroEx(pos, rotation, scale, tint)`
- `DrawLibretroV(pos, tint)`
- `DrawLibretroTexture(x, y, tint)`
- `DrawLibretroPro(destRect, tint)`

**State queries:**
- `GetLibretroName()` / `GetLibretroVersion()`
- `GetLibretroWidth()` / `GetLibretroHeight()` / `GetLibretroRotation()`
- `GetLibretroTexture()` — raw `Texture2D` for custom rendering
- `IsLibretroReady()` / `IsLibretroGameReady()`
- `DoesLibretroCoreNeedContent()`

**Audio:**
- `SetLibretroVolume(float)` / `GetLibretroVolume()`

**Core options (key-value):**
- `SetLibretroCoreOption(key, value)` → `bool` (false if key unknown)
- `GetLibretroCoreOption(key)` → `const char*` (NULL if not found)

**Serialization / save states:**
- `GetLibretroSerializedData(&size)` → `void*` (caller must `MemFree()`)
- `SetLibretroSerializedData(data, size)` → `bool`

---

## Shader System (raylib-libretro-shaders.h)

### Available Shader Types

| Enum | Name | Notes |
|------|------|-------|
| `SHADER_NONE` | None | Pass-through |
| `SHADER_CRT` | CRT | Barrel distortion, phosphor mask |
| `SHADER_SCANLINES` | Scanlines | Horizontal scanline overlay |
| `SHADER_PIXELATE` | Pixelate | Block downscale |
| `SHADER_CHROMATIC_ABERR` | Chromatic Aberration | RGB channel split |
| `SHADER_VIGNETTE` | Vignette | Darkened corners |
| `SHADER_BLOOM` | Bloom | Additive glow |
| `SHADER_GRAYSCALE` | Grayscale | Monochrome + tint |
| `SHADER_LCD_GRID` | LCD Grid | Game Boy / GBA style |
| `SHADER_NTSC` | NTSC | Composite artifacts |
| `SHADER_COLOR_GRADE` | Color Grade | Hue/sat/contrast/gamma |

### Shader Usage

```c
LoadLibretroShaders();                     // Load all shaders with defaults

while (!WindowShouldClose()) {
    UpdateLibretroShaders(GetFrameTime()); // F10 = next, F9 = previous
    BeginDrawing();
        BeginLibretroShader();
            DrawLibretro();
        EndLibretroShader();
    EndDrawing();
}

UnloadLibretroShaders();
```

Key shader API functions: `LoadLibretroShader(type)`, `LoadLibretroShaderEx(type, params)`, `SetActiveLibretroShader(type)`, `GetActiveLibretroShaderType()`, `GetActiveLibretroShaderState()`, `CycleLibretroShader()`, `CycleLibretroShaderReverse()`.

GLSL sources are embedded directly in the header as string literals, with multiple versions (GLSL 100 for OpenGL ES / web, 120 for older desktop, 330 for modern desktop). Platform selection is via `PLATFORM_DESKTOP`.

---

## Build System

### Prerequisites

- CMake 3.11+
- C compiler (GCC, Clang, or MSVC)
- On Linux: `xorg-dev`, `libglu1-mesa-dev`
- Git submodules initialized

### Build Steps

```sh
git clone https://github.com/robloach/raylib-libretro.git
cd raylib-libretro
git submodule update --init
mkdir build && cd build
cmake ..
cmake --build .
```

The binary is at `build/bin/raylib-libretro`.

### CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `BUILD_RAYLIB_LIBRETRO_EXAMPLE` | ON | Build `example/` |
| `BUILD_RAYLIB_LIBRETRO` | ON | Build `bin/` executable |

### raylib Discovery

CMake first calls `find_package(raylib QUIET)`. If not found on the system, it falls back to `vendor/raylib` (the submodule). No explicit flag needed.

### CMake Targets

- `raylib-libretro-interface` — interface/header-only target (`include/`)
- `raylib-libretro-static` — static library (`lib/`), links raylib + dylib
- `raylib-libretro` — full executable (`bin/`)
- `raylib-libretro-basic` — example executable

---

## Running the Frontend

```sh
# General usage
raylib-libretro <core.so> [game_file]

# Linux example
bin/raylib-libretro ~/.config/retroarch/cores/fceumm_libretro.so smb.nes

# macOS example
bin/raylib-libretro ~/Library/Application\ Support/RetroArch/cores/fceumm_libretro.dylib smb.nes
```

Tested cores: `fceumm` (NES), `snes9x` (SNES), `picodrive` (Sega).

### Keyboard Controls

| Key | Action |
|-----|--------|
| Arrow Keys | D-Pad |
| Z X A S Q W | Buttons |
| Enter | Start |
| Right Shift | Select |
| F2 | Save State |
| F4 | Load State |
| F8 | Screenshot |
| F9 | Previous Shader |
| F10 | Next Shader |
| F11 | Fullscreen |

---

## Dependencies

All dependencies are git submodules in `vendor/`:

| Submodule | Purpose |
|-----------|---------|
| `raylib` | Graphics, audio, input, windowing |
| `libretro-common` | `dylib.h` (dynamic loading), `libretro.h`, CPU features |
| `raylib-nuklear` | Nuklear GUI integration for raylib |
| `nuklear_console` | Terminal-style console UI widget |
| `nuklear_gamepad` | Gamepad input for Nuklear |
| `c-vector` | Dynamic array / vector utility |
| `raylib-app` | App framework wrapper (Init/UpdateDrawFrame/Cleanup lifecycle) |

Always run `git submodule update --init` after cloning or switching branches.

---

## CI / Tests

GitHub Actions (`.github/workflows/Tests.yml`) runs on push/PR to `master`:

- **build-linux**: Ubuntu, installs `xorg-dev libglu1-mesa-dev`, CMake Debug build
- **build-windows**: Windows latest, MSVC, CMake Debug build

There are **no unit tests** — CI validates compilation only. When adding code, ensure it compiles cleanly on both platforms.

---

## Coding Conventions

- **Language:** C (not C++), compatible with C99
- **Naming:** Public API uses `PascalCase` verbs — `InitLibretro`, `DrawLibretro`, `GetLibretroTexture`
- **Internal helpers:** Prefixed with `Libretro` — `LibretroLogger`, `LibretroPerfRegister`, `LibretroMapRetroPixelFormatToPixelFormat`
- **Static functions:** All functions in headers are declared `static` (single-header pattern)
- **Error handling:** Functions return `bool` for success/failure; errors go to `TraceLog(LOG_ERROR, ...)`
- **Memory:** Use raylib's `MemAlloc` / `MemFree` (not raw `malloc`/`free`)
- **Include guards:** `#ifndef RAYLIB_LIBRETRO_*_H` / `#define RAYLIB_LIBRETRO_*_H`
- **Implementation guards:** `#ifdef RAYLIB_LIBRETRO_*_IMPLEMENTATION` / `#ifndef RAYLIB_LIBRETRO_*_IMPLEMENTATION_ONCE`
- **Comments:** File headers use the raylib-style `/**** ... ****/` banner format
- **Pixel formats:** libretro uses `RGB565`, `XRGB8888`, `0RGB1555`; mapping functions convert to raylib equivalents

---

## Key Files for AI Assistants

When making changes, these are the most important files to understand:

| File | Why It Matters |
|------|---------------|
| `include/raylib-libretro.h` | The entire library implementation — all callbacks, mappings, audio ring buffer |
| `include/raylib-libretro-shaders.h` | Shader types, GLSL sources, parameter structs, cycling logic |
| `bin/raylib-libretro.c` | Full app using `raylib-app` lifecycle callbacks |
| `example/raylib-libretro-basic.c` | Canonical minimal usage — good reference for the expected API flow |
| `CMakeLists.txt` | Dependency resolution and build options |
| `TASKS.md` | Planned features — check here before implementing something to see if it's already scoped |

---

## Planned / In-Progress Features (TASKS.md)

Before implementing new features, check `TASKS.md`. Key planned items:

- **GUI:** ROM browser, core options UI, rebindable inputs, OSD notifications
- **Gameplay:** Save state slots, SRAM auto-save, rewind, fast-forward, turbo, cheats, RetroAchievements
- **Audio/Video:** Volume control, aspect ratio options, integer scaling, VSync toggle
- **Input:** Gamepad auto-detection, analog sticks, multi-player support
- **Misc:** `.zip` loading, Doxygen docs, binary releases, OpenGL cores

---

## Pixel Format Handling

The core may report one of three pixel formats via the environment callback:

- `RETRO_PIXEL_FORMAT_0RGB1555` — converted to RGB565 before upload
- `RETRO_PIXEL_FORMAT_XRGB8888` — converted to RGBA8888 before upload
- `RETRO_PIXEL_FORMAT_RGB565` — used directly

Conversion functions: `LibretroMapPixelFormatARGB1555ToRGB565`, `LibretroMapPixelFormatARGB8888ToRGBA8888`.

---

## Audio Implementation

Audio uses a pre-allocated ring buffer of 8192 stereo frames to avoid per-frame allocation. The libretro audio callbacks accumulate samples into this buffer, which is drained by raylib's audio stream. Volume is applied at drain time via `SetLibretroVolume`.
