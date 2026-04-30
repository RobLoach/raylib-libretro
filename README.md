# raylib-libretro :space_invader: [![Tests](https://github.com/RobLoach/raylib-libretro/workflows/Tests/badge.svg)](https://github.com/RobLoach/raylib-libretro/actions)

[libretro](https://www.libretro.com/) frontend to play emulators, game engines and media players, using [raylib](https://www.raylib.com). The [raylib-libretro.h](include/raylib-libretro.h) raylib extension allows integrating any raylib application with the libretro API. *Still in early development.*

![Screenshot of raylib-libretro](docs/screenshot.png)

## Usage

``` sh
raylib-libretro [core] [game]
```

## Controls

| Control            | Keyboard    |
| ---                | ---         |
| D-Pad              | Arrow Keys  |
| Buttons            | ZX AS QW    |
| Start              | Enter       |
| Select             | Right Shift |
| Rewind             | R           |
| Toggle Menu        | F1          |
| Save State         | F2          |
| Load State         | F4          |
| Screenshot         | F8          |
| Previous Shader    | F9          |
| Next Shader        | F10         |
| Fullscreen         | F11         |

## Core Support

The following cores have been tested with raylib-libretro:

- fceumm
- snes9x
- picodrive
- scummvm

## Compile

[CMake](https://cmake.org) is used to build raylib-libretro...

``` sh
git clone --recursive http://github.com/robloach/raylib-libretro.git
cd raylib-libretro
mkdir build
cd build
cmake ..
make
bin/raylib-libretro ~/.config/retroarch/cores/fceumm_libretro.so smb.nes
```

### Mac OSX

- Make sure you have you have cmake/xcode-cli-tools installed
- Run the above compile instructions
- After installing RetroArch and some cores, you should be able to run the below:
    ```bash
    bin/raylib-libretro ~/Library/Application\ Support/RetroArch/cores/fceumm_libretro.dylib ~/Desktop/smb.nes
    ```

## Contributors

- [Konsumer](https://github.com/konsumer)
- [MikeDX](https://github.com/MikeDX) and [libretro-raylib](https://github.com/MikeDX/libretro-raylib)

## Shaders

The [`raylib-libretro-shaders.h`](include/raylib-libretro-shaders.h) header provides a family of retro post-process shaders. Include it with the implementation defined in one translation unit:

```c
#define RAYLIB_LIBRETRO_SHADERS_IMPLEMENTATION
#include "raylib-libretro-shaders.h"
```

### Basic Usage

```c
LoadLibretroShaders();                        // load all shaders with defaults

while (!WindowShouldClose()) {
    UpdateLibretroShaders(GetFrameTime());    // cycle with F10/F9, update uniforms

    BeginDrawing();
        ClearBackground(BLACK);
        BeginLibretroShader();
            DrawLibretro();
        EndLibretroShader();
    EndDrawing();
}

UnloadLibretroShaders();
```

### API Reference

| Function | Description |
| --- | --- |
| `GetLibretroShaderCode(type)` | Returns the embedded GLSL source for the given type |
| `GetLibretroShaderDefaults(type)` | Returns a state populated with default parameters |
| `GetLibretroShaderName(type)` | Returns the display name (`"CRT"`, `"None"`, etc.) |
| `LoadLibretroShader(type)` | Compile shader with defaults |
| `LoadLibretroShaderEx(type, params)` | Compile shader with custom params |
| `UpdateLibretroShader(state, dt)` | Re-upload uniforms, accumulate time |
| `UnloadLibretroShader(state)` | Free GPU resource |
| `LoadLibretroShaders()` | Load all shaders with defaults |
| `UnloadLibretroShaders()` | Unload all shader GPU resources |
| `UpdateLibretroShaders(dt)` | Update active shader; F10 = next, F9 = previous |
| `CycleLibretroShader()` | Advance to next shader type |
| `CycleLibretroShaderReverse()` | Go back to previous shader type |
| `SetActiveLibretroShader(type)` | Activate a specific shader |
| `GetActiveLibretroShaderType()` | Returns the active `LibretroShaderType` |
| `GetActiveLibretroShaderState()` | Returns mutable pointer to active state, or NULL |
| `BeginLibretroShader()` | Begin shader mode (no-op when `SHADER_NONE`) |
| `EndLibretroShader()` | End shader mode (no-op when `SHADER_NONE`) |

## Development

Update the dependencies through git submodules...
```sh
git submodule update --recursive --remote --init --force
```

Use clang-format to apply coding standards...
```c
clang-format -i *.h
```

## License

[zlib/libpng](LICENSE)
