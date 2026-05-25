# raylib-libretro API Reference

This document describes how to integrate `raylib-libretro.h` into your own raylib application.

## Setup

### Single-header integration

Copy `raylib-libretro.h` into your project. In **exactly one** `.c` file, define the implementation macro before including it:

```c
#define RAYLIB_LIBRETRO_IMPLEMENTATION
#include "raylib-libretro.h"
```

All other files that need the API include it without the macro:

```c
#include "raylib-libretro.h"
```

### CMake integration

Add the repo as a submodule, then link against `raylib-libretro-static`:

```cmake
add_subdirectory(vendor/raylib-libretro)
target_link_libraries(my_app PRIVATE raylib-libretro-static)
target_include_directories(my_app PRIVATE vendor/raylib-libretro/include)
```

---

## Minimal example

```c
#include "raylib.h"

#define RAYLIB_LIBRETRO_IMPLEMENTATION
#include "raylib-libretro.h"

int main(int argc, char* argv[]) {
    InitWindow(1280, 720, "My libretro app");
    InitAudioDevice();

    InitLibretro(argv[1]);          // Load core (.so/.dll/.dylib)
    LoadLibretroGame(argv[2]);      // Load game file (or NULL for content-less cores)

    while (!WindowShouldClose() && !LibretroShouldClose()) {
        UpdateLibretro();

        BeginDrawing();
            ClearBackground(BLACK);
            DrawLibretro();
        EndDrawing();
    }

    UnloadLibretroGame();
    CloseLibretro();
    CloseAudioDevice();
    CloseWindow();
    return 0;
}
```

---

## API

### Lifecycle

#### `bool InitLibretro(const char* core)`
Load a libretro core from the given shared library path (`.so`, `.dll`, or `.dylib`). Returns `true` on success. Must be called before any other API function.

#### `bool InitLibretroEx(const char* core, bool peek)`
Like `InitLibretro()`, but when `peek` is `true` the core is inspected for metadata without fully initialising it.

#### `bool LoadLibretroGame(const char* gameFile)`
Load content (ROM, disk image, etc.) from `gameFile`. Pass `NULL` for cores that do not require content. Returns `true` on success.

#### `void UnloadLibretroGame()`
Unload the currently loaded game. Call this before `CloseLibretro()`.

#### `void ResetLibretro()`
Soft-reset the currently loaded core and content.

#### `void CloseLibretro()`
Unload the core and free all resources. Always call `UnloadLibretroGame()` first.

---

### Per-frame

#### `void UpdateLibretro()`
Run one emulation frame. Call this once per game loop iteration before drawing.

#### `bool LibretroShouldClose()`
Returns `true` when the core has requested shutdown (e.g. the user chose "Quit" inside the core's own UI).

---

### Drawing

All draw functions render the core's current framebuffer. Call them inside `BeginDrawing()` / `EndDrawing()`.

| Function | Description |
|---|---|
| `DrawLibretro()` | Draw centered on screen |
| `DrawLibretroTint(Color tint)` | Draw centered with a color tint |
| `DrawLibretroV(Vector2 position, Color tint)` | Draw at position with tint |
| `DrawLibretroTexture(int x, int y, Color tint)` | Draw at (x, y) with tint |
| `DrawLibretroEx(Vector2 pos, float rotation, float scale, Color tint)` | Draw with full transform |
| `DrawLibretroPro(Rectangle dest, Color tint)` | Draw scaled into a destination rectangle |

#### `Texture2D GetLibretroTexture()`
Returns the raw `Texture2D` used to render the core's framebuffer. Useful for custom rendering pipelines.

---

### State queries

| Function | Returns | Description |
|---|---|---|
| `bool IsLibretroReady()` | `bool` | `true` if a core is loaded and ready |
| `bool IsLibretroGameReady()` | `bool` | `true` if content has been loaded |
| `bool IsLibretroGameRequired()` | `bool` | `true` if the core requires content to run |
| `const char* GetLibretroName()` | `const char*` | Core library name string |
| `const char* GetLibretroVersion()` | `const char*` | Core version string |
| `unsigned GetLibretroWidth()` | `unsigned` | Core's desired framebuffer width |
| `unsigned GetLibretroHeight()` | `unsigned` | Core's desired framebuffer height |
| `unsigned GetLibretroRotation()` | `unsigned` | Screen rotation (0=0°, 1=90°, 2=180°, 3=270°) |

---

### Audio

#### `void SetLibretroVolume(float volume)`
Set playback volume in the range `[0.0, 1.0]`.

#### `float GetLibretroVolume()`
Get the current playback volume.

---

### Core options

Cores expose configuration keys (e.g. video filter, region, difficulty). Read and write them with:

#### `bool SetLibretroCoreOption(const char* key, const char* value)`
Set a core option. Returns `false` if `key` is not recognised by the loaded core.

#### `const char* GetLibretroCoreOption(const char* key)`
Get the current value of a core option. Returns `NULL` if not found.

---

### Save states

#### `void* GetLibretroSerializedData(unsigned int* size)`
Capture a save state. Returns a heap-allocated buffer and sets `*size` to its byte length. The caller must free the buffer with `MemFree()`.

#### `bool SetLibretroSerializedData(void* data, unsigned int size)`
Restore a save state from a previously captured buffer. Returns `true` on success.

---

### OSD messages

#### `void SetLibretroMessage(const char* msg, float duration)`
Queue an on-screen message to display for `duration` seconds.

#### `bool DrawLibretroMessage()`
Render the current OSD message (if any). Returns `true` if a message was drawn. Call inside `BeginDrawing()` / `EndDrawing()` after `DrawLibretro()`.

---

### Input and controller info

#### `const struct retro_input_descriptor* GetLibretroInputDescriptors(unsigned* count)`
Returns the core's input descriptor table and sets `*count` to the number of entries.

#### `const struct retro_controller_info* GetLibretroControllerInfo(unsigned* count)`
Returns the core's controller port info and sets `*count` to the number of ports.

---

### Directories

#### `const char* GetLibretroDirectory(int directory)`
Returns the path for the requested directory type. `directory` maps to `RETRO_ENVIRONMENT_GET_*_DIRECTORY` values from `libretro.h`.
