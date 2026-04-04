# raylib-libretro :space_invader: [![Tests](https://github.com/RobLoach/raylib-libretro/workflows/Tests/badge.svg)](https://github.com/RobLoach/raylib-libretro/actions)

[libretro](https://www.libretro.com/) frontend to play emulators, game engines and media players, using [raylib](https://www.raylib.com). The [raylib-libretro.h](include/raylib-libretro.h) raylib extension allows integrating any raylib application with the libretro API. *Still in early development.*

![Screenshot of raylib-libretro](src/screenshot.png)

## Usage

``` sh
raylib-libretro [core] [game]
```

## Controls

| Control       | Keyboard    |
| ---           | ---         |
| D-Pad         | Arrow Keys  |
| Buttons       | ZX AS QW    |
| Start         | Enter       |
| Select        | Right Shift |
| Save State    | F2          |
| Load State    | F4          |
| Screenshot    | F8          |
| Shader        | F10         |
| Fullscreen    | F11         |

## Core Support

The following cores have been tested with raylib-libretro:

- fceumm
- snes9x
- picodrive

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

## License

[zlib/libpng](LICENSE)
