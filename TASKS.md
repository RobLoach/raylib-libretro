# raylib-libretro

## GUI

- [ ] Add nuklear_console with dependencies nuklear_gamepad, c-vector, nuklear
- [ ] Load Game button to load a file. Then select a Core for it if it doesn't detect.
- [ ] Core Options
- [ ] Rebindable Inputs
- [ ] History
- [ ] Game library / ROM browser with box art thumbnails (RetroArch thumbnail packs)
- [ ] Per-game core association (remember which core goes with which ROM)
- [ ] Per-game settings overrides (core options, shaders, controls)
- [ ] On-screen display (OSD) for notifications (state saved, shader changed, etc.) using nk_console_show_message()

## WASM Cores

- [ ] Document how to compile a libretro core to WASM32 (emcc / wasi-sdk toolchain, required exports, import namespace)
- [ ] Build at least one reference core (e.g. fceumm or a minimal test core) as `.wasm` and verify end-to-end
- [ ] Add the compiled `.wasm` test core to CI so the runtime path is exercised, not just compilation
- [ ] Investigate WAMR AOT compilation for better performance on constrained devices

## Tasks

- [ ] `raylib-libretro.h` documentation
- [ ] Doxygen
- [ ] Clean up Shader API to be a family of applicable shaders
- [ ] `.zip` Loading
- [ ] Binary Releases
- [ ] OpenGL Cores

## Gameplay

- [ ] Save state slots (multiple named saves per game)
- [ ] SRAM / battery save auto-save on exit
- [ ] Rewind support (`retro_serialize` API)
- [ ] Fast-forward / slow-motion toggle
- [ ] Turbo button support
- [ ] Cheats support (libretro cheat interface)
- [ ] RetroAchievements integration (rcheevos)
- [ ] Playlist / M3U disc swapping for multi-disc games

## Audio / Video

- [ ] Audio volume control and mute toggle
- [ ] Aspect ratio options (pixel-perfect, stretch, 4:3 locked)
- [ ] Integer scaling mode
- [ ] Scanline / CRT shader presets
- [ ] Frame limiter / VSync toggle

## Input

- [ ] Gamepad auto-detection and mapping
- [ ] Analog stick support for cores that use it
- [ ] Multi-player / multi-gamepad assignment
- [ ] Command-line ROM drag-and-drop onto window
