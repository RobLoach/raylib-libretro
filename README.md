# raylib-libretro :space_invader: [![Tests](https://github.com/RobLoach/raylib-libretro/workflows/Tests/badge.svg)](https://github.com/RobLoach/raylib-libretro/actions)

[libretro](https://www.libretro.com/) frontend to play emulators, game engines and media players, for Windows, Mac, Linux, and Web with [raylib](https://www.raylib.com). The [raylib-libretro.h](include/raylib-libretro.h) allows integrating any raylib application with the libretro API. *Still in early development.*

![Screenshot of raylib-libretro](docs/screenshot.png)

## Features

- Multi-Emulator for NES, SNES, GBA, Genesis, etc
- Shipped Cores: fceumm, snes9x, gambatte, mgba, picodrive
- Gamepad, Keyboard, On-Screen Controls
- Audio
- Fast Forward, Slow Motion, Rewind
- Save Slots and SRAM
- Shaders
- Menu Themes
- Core Options
- Zip File Support

## Usage

``` sh
raylib-libretro [-L <core>] [game]
```

- `[game]` can be a loose ROM file or a `.zip` archive
- `-L <core>` is optional — path to the libretro core (`.so`/`.dll`/`.dylib`)

### Example
```
raylib-libretro -L ~/.config/retroarch/cores/fceumm_libretro.so smb.nes
raylib-libretro -L ~/.config/retroarch/cores/fceumm_libretro.so smb.zip
raylib-libretro smb.nes
```

## Embed in a website

Add the emulator to any web page with a single `<script>` tag — it loads the frontend and
your game inline, with a click-to-play poster by default. The script and the WebAssembly
build are published to GitHub Pages on every release.

```html
<script src="https://robloach.github.io/raylib-libretro/embed.js"
        data-game="https://example.com/mario.nes"></script>
```

Place the tag where you want the game to appear. Other ways to use it:

```html
<!-- Marker element (scanned on load) -->
<div data-raylib-libretro data-game="mario.nes" data-core="fceumm"></div>
<script src="https://robloach.github.io/raylib-libretro/embed.js"></script>

<!-- Programmatic -->
<div id="game"></div>
<script src="https://robloach.github.io/raylib-libretro/embed.js"></script>
<script>RaylibLibretro.embed('#game', { game: 'mario.nes', core: 'fceumm' });</script>
```

### Options

Set as `data-*` attributes or in the `embed()` options object:

| Option      | Description |
| ----------- | ----------- |
| `game`      | ROM URL (required to play). Must be reachable via CORS. |
| `core`      | Bundled core name (`fceumm`, `snes9x`, `gambatte`, `mgba`, `picodrive`) or a core URL. Omit to autodetect from the ROM extension. |
| `autostart` | Load immediately instead of showing the click-to-play poster. |
| `poster`    | Placeholder image URL shown before play. |
| `aspect`    | Container aspect ratio (`4:3` default, `3:2`, `1.5`, …). |
| `base`      | Asset base URL (where `index.js`/`.wasm`/`.data` live). Defaults to wherever `embed.js` is served from, so self-hosting just works. |

### Notes

- **CORS:** the game ROM (and self-hosted assets) must be served with
  `Access-Control-Allow-Origin`. GitHub Pages already does.
- **One instance per page:** the WebAssembly runtime uses a global module, so only one
  embedded game can run per page. Saves persist per host-page origin via IndexedDB.
- See [`bin/embed-example.html`](bin/embed-example.html) for a working demo, and self-host by
  serving the `-web-wasm32` release zip (which includes `embed.js`) from your own site.

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
double GetLibretroFPS();
float GetLibretroAspectRatio();
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
void SetLibretroMessage(const char* msg, float duration);
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

Build for web with emscripten...
```sh
mkdir build
cd build
emcmake cmake .. -DPLATFORM=Web -DCMAKE_BUILD_TYPE=Release
emmake make
```

To build the Doxygen documentation...
```sh
doxygen .Doxyfile
```

## Contributors

- [Konsumer](https://github.com/konsumer)
- [MikeDX](https://github.com/MikeDX) and [libretro-raylib](https://github.com/MikeDX/libretro-raylib)

## License

[zlib/libpng](LICENSE)
