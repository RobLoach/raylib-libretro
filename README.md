# raylib-libretro :space_invader: [![Tests](https://github.com/RobLoach/raylib-libretro/workflows/Tests/badge.svg)](https://github.com/RobLoach/raylib-libretro/actions)

[libretro](https://www.libretro.com/) frontend to play emulators, game engines and media players, using [raylib](https://www.raylib.com). The [raylib-libretro.h](include/raylib-libretro.h) raylib extension allows integrating any raylib application with the libretro API. *Still in early development.*

![Screenshot of raylib-libretro](docs/screenshot.png)

## Usage

``` sh
raylib-libretro [core] [game]
```

`[game]` can be a loose ROM file or a `.zip` archive. Archives are mounted
through [PhysFS](https://icculus.org/physfs/) at a virtual `/game` path —
the ROM inside the archive is picked by basename match (e.g. `mario.zip`
→ `mario.nes`), falling back to the first entry matching the core's
supported extensions. Cores that set `block_extract=true` receive the
archive path directly so they can handle it themselves.

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
git clone http://github.com/robloach/raylib-libretro.git
cd raylib-libretro
git submodule update --init
mkdir build
cd build
cmake ..
make
bin/raylib-libretro -L ~/.config/retroarch/cores/fceumm_libretro.so smb.nes
# Or with a .zip:
bin/raylib-libretro -L ~/.config/retroarch/cores/fceumm_libretro.so smb.zip
# Or with autodetection of cores in the cores directory
bin/raylib-libretro smb.nes
```

### Mac OSX

- Make sure you have you have `cmake/xcode-cli-tools` installed
- Run the above compile instructions
- After installing RetroArch and some cores, you should be able to run the below:
    ```bash
    bin/raylib-libretro -L ~/Library/Application\ Support/RetroArch/cores/fceumm_libretro.dylib ~/Desktop/smb.nes
    ```

## API Usage

```c
InitLibretro("fceumm.so");
LoadLibretroGame("mario.nes");
while (!WindowShouldClose()) {
    UpdateLibretro();
    BeginDrawing();
        ClearBackground(BLACK);
        DrawLibretro();
    EndDrawing();
}
CloseLibretro();
```

## API Reference

See the full [API documentation](docs/API.md) for integration details, CMake setup, and function descriptions.

### Quick Reference

``` c
bool InitLibretro(const char* core);
bool LoadLibretroGame(const char* gameFile);
bool IsLibretroReady();
bool IsLibretroGameReady();
void UpdateLibretro();
bool LibretroShouldClose();
void DrawLibretro();
void DrawLibretroTint(Color tint);
void DrawLibretroEx(Vector2 position, float rotation, float scale, Color tint);
void DrawLibretroV(Vector2 position, Color tint);
void DrawLibretroTexture(int posX, int posY, Color tint);
void DrawLibretroPro(Rectangle destRec, Color tint);
const char* GetLibretroName();
const char* GetLibretroVersion();
unsigned GetLibretroWidth();
unsigned GetLibretroHeight();
unsigned GetLibretroRotation();
Texture2D GetLibretroTexture();
bool IsLibretroGameRequired();
void ResetLibretro();
void UnloadLibretroGame();
void CloseLibretro();
void SetLibretroVolume(float volume);
float GetLibretroVolume();
bool SetLibretroCoreOption(const char* key, const char* value);
const char* GetLibretroCoreOption(const char* key);
void* GetLibretroSerializedData(unsigned int* size);
bool SetLibretroSerializedData(void* data, unsigned int size);
void ShowLibretroMessage(const char* msg, float duration);
bool DrawLibretroMessage();
const char* GetLibretroDirectory(int directory);
```

## Development

Update the dependencies through git submodules...
```sh
git submodule update --recursive --remote --init --force
```

Use clang-format to apply coding standards...
```c
clang-format -i *.h
```

## Contributors

- [Konsumer](https://github.com/konsumer)
- [MikeDX](https://github.com/MikeDX) and [libretro-raylib](https://github.com/MikeDX/libretro-raylib)

## License

[zlib/libpng](LICENSE)
