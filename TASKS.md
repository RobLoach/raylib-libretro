# raylib-libretro

A roadmap of some interesting features to consider.

## Quick Fixes
- [ ] Within raylib-libretro-touch.h, within LibretroTouchJoypadKeyboard(), if the menu is available, then get the menu setting for the related keyboard controls instead of hardcoding the keys.
- [ ] Within raylib-libretro-touch.h, there is a LibretroTouchJoypadGamepad() function. Isn't there a function like this directly withing raylib-libretro.h we could use instead? Would save some function definitions.
- [ ] Within raylib-libretro-touch.h, there's a LibretroTouchFadeColor() function. Isn't there a raylib function we could use for this instead?
- [ ] Is there a way to have the Emscripten Fullscreen capabilities be handled through the Settings>Graphics>Fullscreen toggle widget?

## Project & Documentation

- [ ] `raylib-libretro.h` API reference documentation
- [ ] Doxygen build target + hosted docs
- [ ] Clean up Shader API into a family of applicable shaders
- [ ] Additional integration examples beyond `raylib-libretro-basic.c`
- [ ] Migration / config-compatibility notes for users coming from RetroArch
- [ ] Per-platform packaging recipes (Flatpak, `.deb`, Homebrew, Scoop)
- [ ] Contributor guide & coding conventions doc

## Frontend UI / Menu

- [ ] Game library / ROM browser with box-art thumbnails
- [ ] Recently-played history list
- [ ] Favorites list (pinned games)
- [x] Quick Menu overlay (pause + per-game options without leaving game)
- [ ] File browser with metadata preview pane
- [ ] Multi-language i18n framework + base translations
- [ ] Additional menu themes / skins (light, high-contrast, RGUI-style)
- [ ] DPI / scale factor option for hi-res & 4K displays
- [ ] Full gamepad navigation across every menu screen
- [ ] Kiosk mode with password-locked settings

## Content Management

- [ ] RetroArch-compatible playlist (`.lpl`) load + save
- [ ] Recursive ROM directory scanner with progress UI
- [ ] Soft-patching at load time: IPS / UPS / BPS / Xdelta
- [ ] Subsystem content support (multi-cart cores)
- [ ] Online core downloader / repo browser
- [ ] Core `.info` file parsing for extensions & manifest
- [ ] Remember last content directory per core
- [ ] Drag-and-drop folder import (bulk library add)

## Save / State System

- [ ] Undo Save State (revert last save)
- [ ] Undo Load State (revert last load)
- [ ] Save state thumbnail (PNG screenshot stored alongside state)
- [ ] Auto-load most recent state on game launch (optional)
- [ ] Save state compression (zstd or zlib)
- [ ] Periodic SRAM auto-save at configurable interval
- [ ] Cloud save sync (WebDAV / Git / Dropbox / generic HTTP PUT)
- [ ] RetroArch-compatible save state file format

## Audio

- [ ] Dynamic Rate Control for audio/video sync
- [ ] Configurable resampler quality (linear / sinc / sinc-HQ)
- [ ] Audio DSP plugin support (`.dsp` chain)
- [ ] Built-in audio mixer for menu BGM and SFX
- [ ] MIDI interface (`GET_MIDI_INTERFACE`)
- [ ] Microphone interface (`GET_MICROPHONE_INTERFACE`)
- [ ] Mono / stereo output toggle

## Video / Display

- [ ] Integer scaling mode
- [ ] Custom viewport position + size
- [ ] Override core rotation (force 0 / 90 / 180 / 270)
- [ ] Frame limiter / VSync toggle
- [ ] Black Frame Insertion (BFI) for CRT-style motion clarity
- [ ] Hard GPU sync (reduce render-ahead frames)
- [ ] Threaded video rendering off the emulation thread
- [ ] Bezel / overlay PNG frame around game viewport
- [ ] GPU-side (post-shader) screenshot capture

## Shaders

- [ ] Scanline / CRT shader presets
- [ ] `.glslp` shader preset format (multi-pass chains)
- [ ] Per-game / per-core shader auto-apply hierarchy
- [ ] Live shader parameter tweaking in menu
- [ ] Wildcards in preset paths (`$GAME$`, `$CORE$`, `$ROT$`)
- [ ] Bundle more public-domain shaders (xBR, scale2x, HQ2x, dot-mask)
- [ ] Shader hot-reload on source-file change
- [ ] Slang → GLSL transpile path (or documented Slang exclusion)

## Input

- [ ] Rebindable gamepad controller inputs
- [x] Rebindable keyboard controls
- [ ] Gamepad auto-detection and mapping
- [ ] Multi-player / multi-gamepad assignment
- [ ] Turbo button support
- [ ] Per-core / per-game input remap files (`.rmp` compatible)
- [ ] Mouse (`RETRO_DEVICE_MOUSE`) fully wired through
- [ ] Light gun (`RETRO_DEVICE_LIGHTGUN`) with cursor capture
- [ ] Multitap (`RETRO_DEVICE_JOYPAD_MULTITAP`)
- [ ] On-screen overlay touch layouts (PNG, auto-scale, snap-to-edges)
- [ ] Analog stick deadzone + sensitivity per port
- [ ] Game-Focus mode (pass all keys through, ignore hotkeys)

## Performance / Latency

- [ ] Run-Ahead single-instance (re-run frames after late input)
- [ ] Run-Ahead second-instance (separate core copy)
- [ ] Preemptive frames (poll input multiple times per frame)
- [ ] Auto frame delay (calibrate wait before render)
- [ ] Input polling type (early / normal / late)
- [ ] Frame skip when CPU-bound

## Cheats

- [ ] Cheat code support via libretro cheat interface
- [ ] `.cht` file load / save (RetroArch compatible)
- [ ] Built-in cheat search (8/16/32-bit, signed/unsigned, BE/LE)
- [ ] Game Genie / GameShark / Action Replay code parsers

## Achievements / Online

- [ ] RetroAchievements (rcheevos) integration
- [ ] RetroAchievements Hardcore Mode (disables rewind/cheats/states)
- [ ] Discord Rich Presence (game name, time played, achievements)
- [ ] Netplay with rollback netcode (GGPO-style)
- [ ] Netplay spectator + lobby browser

## Accessibility

- [ ] Text-to-speech menu narration (speech-dispatcher / SAPI / `say`)
- [ ] High-contrast / colorblind menu palettes
- [ ] On-screen keyboard for text entry (search, names, codes)

## Disc / BIOS

- [ ] Playlist / M3U disc swapping for multi-disc games
- [ ] Disc Control Interface (`SET_DISK_CONTROL_INTERFACE` + `_EXT`) - eject/swap/append/set index
- [ ] BIOS detection in `system/` with hash verification

## libretro API Coverage

- [ ] OpenGL cores / hardware rendering context (`SET_HW_RENDER`)
- [ ] Sensor interface (accelerometer / gyro) for mobile / handheld
- [ ] LED interface for RGB keyboards / DualSense

## Database / Thumbnails

- [ ] CRC32-based ROM identification against No-Intro / Redump dat files
- [ ] Thumbnail packs downloader (`thumbnails.libretro.com` - boxart / title / snap)
- [ ] Game metadata display in library (year, publisher, genre, region)

## Recording / Streaming

- [ ] Frame-by-frame video recording (raylib framebuffer → file)
- [ ] RTMP streaming to Twitch / YouTube

## System / Networking

- [ ] Network Control Interface (UDP remote commands: `SAVE_STATE`, `READ_CORE_RAM`, ...)

## Completed

- [x] On-screen display (OSD) for notifications using `nk_console_show_message()`
- [x] `.zip` loading via PhysFS
- [x] Binary releases
- [x] Save state slots (multiple named saves per game)
- [x] SRAM / battery save auto-save on exit
- [x] Audio volume control and mute toggle
- [x] `RETRO_ENVIRONMENT_SET_AUDIO_BUFFER_STATUS_CALLBACK` and `SET_MINIMUM_AUDIO_LATENCY`
