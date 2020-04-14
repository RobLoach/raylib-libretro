# raylib-libretro :space_invader: ![Tests](https://github.com/RobLoach/raylib-libretro/workflows/Tests/badge.svg)

[libretro](https://www.libretro.com/) frontend using [raylib](https://www.raylib.com), along with the [`rlibretro.h`](include/rlibretro.h) raylib extension to integrate any raylib application with the libretro API. *Still in early development.*

![Screenshot of raylib-libretro](examples/rlibretro_basic_window.png)

## Usage

``` sh
raylib-libretro <core> [game]
```

| Control  | Keyboard    |
| ---      | ---         |
| D-Pad    | Arrow Keys  |
| Buttons  | ZXAS        |
| Start    | Enter       |
| Select   | Right Shift |

## Wishlist

- [x] Video
- [x] Resizable Window
- [ ] Graphical User Interface
- [ ] [`rlibretro.h`](include/rlibretro.h) documentation
- [ ] Cleaned Audio
- [ ] Fullscreen
- [ ] Rebindable Inputs
- [ ] Gamepad Support
- [ ] Project Templates (VS2017, etc)
- [ ] Mouse Support
- [ ] Zip Loading
- [ ] Shaders
- [ ] Core Options
- [ ] Binary Releases
- [ ] OpenGL Cores

## Compile

[CMake](https://cmake.org/) is used to build raylib-libretro. Looking to add more project templates to help the build process!

``` sh
git clone http://github.com/robloach/raylib-libretro.git
cd raylib-libretro
git submodule update --init
mkdir build
cd build
cmake ..
make
```

## License

[zlib/libpng](LICENSE)
