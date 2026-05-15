# raylib-libretro

## GUI

- [ ] Rebindable Inputs
- [ ] History
- [ ] Game library / ROM browser with box art thumbnails (RetroArch thumbnail packs)
- [ ] On-screen display (OSD) for notifications (state saved, shader changed, etc.) using nk_console_show_message()

## Tasks

- [ ] Move rini dependency to just bin/raylib-libretro.c
- [ ] `raylib-libretro.h` documentation
- [ ] Doxygen
- [ ] Clean up Shader API to be a family of applicable shaders
- [ ] `.zip` Loading
- [ ] Binary Releases
- [ ] OpenGL Cores

## Gameplay

- [ ] Save state slots (multiple named saves per game)
- [ ] SRAM / battery save auto-save on exit
- [ ] Turbo button support
- [ ] Cheats support (libretro cheat interface)
- [ ] RetroAchievements integration (rcheevos)
- [ ] Playlist / M3U disc swapping for multi-disc games

## Audio / Video

- [x] Audio volume control and mute toggle
- [ ] Aspect ratio options (pixel-perfect, stretch, 4:3 locked)
- [ ] Integer scaling mode
- [ ] Scanline / CRT shader presets
- [ ] Frame limiter / VSync toggle

## Input

- [ ] Gamepad auto-detection and mapping
- [ ] Multi-player / multi-gamepad assignment
