/**********************************************************************************************
*
*   raylib-libretro - A libretro frontend using raylib.
*
*   LICENSE: zlib/libpng
*
*   raylib-libretro is licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software:
*
*   Copyright (c) 2020 Rob Loach (@RobLoach)
*
*   This software is provided "as-is", without any express or implied warranty. In no event
*   will the authors be held liable for any damages arising from the use of this software.
*
*   Permission is granted to anyone to use this software for any purpose, including commercial
*   applications, and to alter it and redistribute it freely, subject to the following restrictions:
*
*     1. The origin of this software must not be misrepresented; you must not claim that you
*     wrote the original software. If you use this software in a product, an acknowledgment
*     in the product documentation would be appreciated but is not required.
*
*     2. Altered source versions must be plainly marked as such, and must not be misrepresented
*     as being the original software.
*
*     3. This notice may not be removed or altered from any source distribution.
*
**********************************************************************************************/

#include "raylib.h"

#define RAYLIB_APP_IMPLEMENTATION
#include "raylib-app.h"

#define RAYLIB_LIBRETRO_CONFIG_IMPLEMENTATION
#include "raylib-libretro-config.h"

#define RAYLIB_LIBRETRO_IMPLEMENTATION
#include "raylib-libretro.h"

#define PHYSFS_PLATFORM_RAYLIB
#define RAYLIB_PHYSFS_IMPLEMENTATION
#include "raylib-physfs.h"

#define RAYLIB_LIBRETRO_PHYSFS_IMPLEMENTATION
#include "../include/raylib-libretro-physfs.h"

#define RAYLIB_LIBRETRO_SHADERS_IMPLEMENTATION
#include "../include/raylib-libretro-shaders.h"

#define RAYLIB_LIBRETRO_MENU_IMPLEMENTATION
#include "../include/raylib-libretro-menu.h"

#define RAYLIB_LIBRETRO_TOUCH_IMPLEMENTATION
#include "../include/raylib-libretro-touch.h"

#ifdef __EMSCRIPTEN__
#include <emscripten/html5.h>

// JS-callable hot-load entry point. shell.html fetches ?game=<url> in the
// background and calls this once main() is running. If a core is already
// loaded (e.g. via ?core=) the game is loaded into it; otherwise the core
// is autodetected from the ROM extension, matching drag-and-drop behavior.
EMSCRIPTEN_KEEPALIVE
bool LoadLibretroGameFromJS(const char* gameFile) {
    if (gameFile == NULL || gameFile[0] == '\0') return false;
    if (IsLibretroReady()) {
        return LoadLibretroGameFromPhysFS(gameFile);
    }
    return MenuLoadGame(gameFile);
}

#endif

#define REWIND_BUFFER_FRAMES 600  // ~10 seconds at 60fps

typedef struct {
    void* data;
    unsigned int size;
} RewindFrame;

typedef struct {
    RewindFrame frames[REWIND_BUFFER_FRAMES];
    int head;   // next write index
    int count;  // number of valid frames stored
} RewindBuffer;

static void RewindBufferPush(RewindBuffer* rb, void* data, unsigned int size) {
    int idx = rb->head;
    if (rb->frames[idx].data != NULL) {
        MemFree(rb->frames[idx].data);
    }
    rb->frames[idx].data = data;
    rb->frames[idx].size = size;
    rb->head = (rb->head + 1) % REWIND_BUFFER_FRAMES;
    if (rb->count < REWIND_BUFFER_FRAMES) rb->count++;
}

static bool RewindBufferPop(RewindBuffer* rb, void** outData, unsigned int* outSize) {
    if (rb->count <= 0) return false;
    rb->head = (rb->head - 1 + REWIND_BUFFER_FRAMES) % REWIND_BUFFER_FRAMES;
    rb->count--;
    *outData = rb->frames[rb->head].data;
    *outSize = rb->frames[rb->head].size;
    rb->frames[rb->head].data = NULL;
    rb->frames[rb->head].size = 0;
    return true;
}

static void RewindBufferFree(RewindBuffer* rb) {
    for (int i = 0; i < REWIND_BUFFER_FRAMES; i++) {
        if (rb->frames[i].data != NULL) {
            MemFree(rb->frames[i].data);
            rb->frames[i].data = NULL;
        }
    }
    rb->head = 0;
    rb->count = 0;
}

typedef struct {
    LibretroMenu* menu;
    RewindBuffer rewind;
    float savedVolume;
    bool muted;
    float savedVolumeBeforeMute;
    bool pendingMenuOpen;
} AppData;

bool Init(void** userData, int argc, char** argv) {
    SetWindowMinSize(400, 300);
    SetExitKey(KEY_NULL);

    BeginDrawing();
        ClearBackground(BLACK);
        const char* loadingText = "Loading";
        int fontSize = 20;
        DrawText(loadingText,
            (GetScreenWidth()  - MeasureText(loadingText, fontSize)) / 2,
            (GetScreenHeight() - fontSize) / 2,
            fontSize, GRAY);
    EndDrawing();

    AppData* data = (AppData*)MemAlloc(sizeof(AppData));
    memset(data, 0, sizeof(AppData));
    *userData = data;

    InitAudioDevice();
    InitLibretroPhysFS();

    // Load the shaders and the menu.
    LoadLibretroShaders();
    data->menu = InitLibretroMenu();
    if (!data->menu) {
        TraceLog(LOG_ERROR, "Failed to initialize menu");
        UnloadLibretroShaders();
        CloseLibretroPhysFS();
        CloseAudioDevice();
        return false;
    }

    // Parse the command line arguments.
    // -L/--libretro <core> sets the core; the first non-flag argument is the game file.
    const char* corePath = NULL;
    const char* gameFile = NULL;
    for (int i = 1; i < argc; i++) {
        if (TextIsEqual(argv[i], "-h") || TextIsEqual(argv[i], "--help")) {
            printf("Usage: %s [-L <core>] [game]\n\n", argv[0]);
            printf("Options:\n");
            printf("  -L, --libretro <core>   Path to the libretro core (.so/.dll/.dylib)\n");
            printf("  -h, --help              Show this help message\n\n");
            printf("Examples:\n");
            printf("  %s -L fceumm_libretro.so smb.nes\n", argv[0]);
            printf("  %s -L fceumm_libretro.so smb.zip\n", argv[0]);
            printf("  %s smb.nes\n", argv[0]);
            return false;
        } else if ((TextIsEqual(argv[i], "-L") || TextIsEqual(argv[i], "--libretro")) && i + 1 < argc) {
            corePath = argv[++i];
        } else if (!gameFile) {
            gameFile = argv[i];
        }
    }

    if (corePath) {
        if (MenuInitCore(corePath) && LoadLibretroGameFromPhysFS(gameFile)) {
            BuildLibretroMenuOptions(data->menu);
            data->menu->active = false;
        }
    } else if (gameFile) {
        data->menu->active = !MenuLoadGame(gameFile);
    }

    return true;
}

bool Update(void* userData) {
    AppData* data = (AppData*)userData;

    // Deferred menu open: apply after input has refreshed so the release event
    // that triggered the MENU touch button is gone before Nuklear processes input.
    if (data->pendingMenuOpen) {
        data->menu->active = true;
        data->pendingMenuOpen = false;
    }

    // Update virtual joypad from touch controls.
    if (data->menu->touchControls) {
        SetTouchHapticsEnabled(data->menu->touchHapticsEnabled);
        if (!data->menu->active) {
            UpdateLibretroTouchControls();
            if (IsTouchControlsMenuPressed()) {
                data->pendingMenuOpen = true;
            }
        } else {
            memset(LIBRETRO.core.virtualJoypadState, 0, sizeof(LIBRETRO.core.virtualJoypadState));
        }
    }

    // Run a frame of the core.
    if (!data->menu->active) {
        // Update the shaders, then show OSD if a shader key was pressed.
        LibretroShaderType shaderTypeBefore = GetActiveLibretroShaderType();
        UpdateLibretroShaders(GetFrameTime());

        if (data->menu->rewindEnabled && IsLibretroGameReady()) {
            if (IsKeyDown(NuklearKeyToKeyboardKey(data->menu->keyRewind))) {
                void* stateData = NULL;
                unsigned int stateSize = 0;
                if (RewindBufferPop(&data->rewind, &stateData, &stateSize)) {
                    SetLibretroSerializedData(stateData, stateSize);
                    MemFree(stateData);
                } else {
                    SetLibretroMessage("Rewind limit reached", 1.0f);
                }
            } else {
                unsigned int size = 0;
                void* state = GetLibretroSerializedData(&size);
                if (state != NULL) {
                    RewindBufferPush(&data->rewind, state, size);
                }
            }
        } else if (data->rewind.count > 0) {
            RewindBufferFree(&data->rewind);
        }

        if (IsLibretroGameReady()) {
            bool ffDown = data->menu->keyFastForward != NK_KEY_NONE &&
                          IsKeyDown(NuklearKeyToKeyboardKey(data->menu->keyFastForward));
            bool smDown = data->menu->keySlowMotion != NK_KEY_NONE &&
                          IsKeyDown(NuklearKeyToKeyboardKey(data->menu->keySlowMotion));

            if (ffDown) {
                if (GetLibretroSpeed() <= 1.0f) {
                    data->savedVolume = GetLibretroVolume();
                    SetLibretroSpeed(data->menu->fastForwardSpeed);
                    SetLibretroVolume(0.0f);
                    SetLibretroMessage(TextFormat("Fast Forward %dx", data->menu->fastForwardSpeed), 1.0f);
                }
            } else if (smDown) {
                if (GetLibretroSpeed() > 1.0f) SetLibretroVolume(data->savedVolume);
                if (GetLibretroSpeed() != data->menu->slowMotionSpeed) {
                    SetLibretroSpeed(data->menu->slowMotionSpeed);
                    SetLibretroMessage(TextFormat("Slow Motion %.0f%%", data->menu->slowMotionSpeed * 100.0f), 1.0f);
                }
            } else if (GetLibretroSpeed() != 1.0f) {
                if (GetLibretroSpeed() > 1.0f) SetLibretroVolume(data->savedVolume);
                SetLibretroSpeed(1.0f);
                SetLibretroMessage("Normal Speed", 1.0f);
            }
        }

        bool wasFastForwarding = GetLibretroSpeed() > 1.0f;
        if (wasFastForwarding) {
            int steps = (int)GetLibretroSpeed();
            if (steps < 1) steps = 1;
            for (int i = 0; i < steps; i++) UpdateLibretro();
        } else {
            UpdateLibretro();
        }
        if (wasFastForwarding && GetLibretroSpeed() <= 1.0f) {
            SetLibretroVolume(data->savedVolume);
        }
    }

    UpdateLibretroMenu();

    // Handle drag-and-drop to load a game or core.
    if (IsFileDropped()) {
        FilePathList dropped = LoadDroppedFiles();
        if (dropped.count > 0) {
            const char* droppedPath = dropped.paths[0];
            SaveLibretroAllSettings();
            CloseLibretro();
            if (IsLibretroCoreFile(droppedPath)) {
                if (MenuInitCore(droppedPath)) {
                    BuildLibretroMenuOptions(data->menu);
                    data->menu->active = true;
                }
            } else {
                // MenuLoadGame autodetects a core for the dropped game via FindCoreForGame().
                data->menu->active = !MenuLoadGame(droppedPath);
            }
        }
        UnloadDroppedFiles(dropped);
    }

    // Check if the core or menu asks to be shutdown.
    if (LibretroShouldClose()) {
        RewindBufferFree(&data->rewind);
        SaveLibretroAllSettings();
        UnloadLibretroGame();
        CloseLibretro();
    }
    if (data->menu->shouldQuit) {
        return false;
    }

    // F11 fullscreen toggle is skipped on Emscripten — browsers intercept F11
    // at the OS level so it never reaches the web page as a key event.
    // The Settings > Audio & Video > Fullscreen checkbox uses
    // emscripten_request_fullscreen with deferUntilInEventHandler instead.
#ifndef __EMSCRIPTEN__
    if (IsKeyReleased(NuklearKeyToKeyboardKey(menu.keyFullscreen))) {
        LibretroMenuFullscreenChanged(menu.console, NULL);
    }
#endif

    // Screenshot
    if (IsKeyReleased(NuklearKeyToKeyboardKey(menu.keyScreenshot))) {
        const char* screenshotsDir = GetLibretroDirectory(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY);
        bool taken = false;
        for (int i = 1; i < 1000; i++) {
            const char* screenshotName = TextFormat("%s/screenshot-%i.png", screenshotsDir, i);
            if (!FileExists(screenshotName)) {
                TakeScreenshot(screenshotName);
                SetLibretroMessage(TextFormat("Screenshot: %s", screenshotName), 2.0f);
                taken = true;
                break;
            }
        }
        if (!taken) {
            SetLibretroMessage("Screenshot slots full", 2.0f);
        }
    }

    // Cycle Shader Reverse
    else if (IsKeyReleased(NuklearKeyToKeyboardKey(menu.keyPrevShader)) && !menu.active) {
        CycleLibretroShaderReverse();
        SetLibretroMessage(GetLibretroShaderName(GetActiveLibretroShaderType()), 2.0f);
        menu.shaderSelectedIndex = (int)GetActiveLibretroShaderType();
    }

    // Cycle Shader Next
    else if (IsKeyReleased(NuklearKeyToKeyboardKey(menu.keyNextShader)) && !menu.active) {
        CycleLibretroShader();
        SetLibretroMessage(GetLibretroShaderName(GetActiveLibretroShaderType()), 2.0f);
        // TODO: For some reason, cycling the shader doens't update the menu label.
        menu.shaderSelectedIndex = (int)GetActiveLibretroShaderType();
    }

    // Save State
    else if (IsKeyReleased(NuklearKeyToKeyboardKey(menu.keySaveState)) && !menu.active) {
        LibretroMenuSaveStateClicked(menu.console, NULL);
    }

    // Load State
    else if (IsKeyReleased(NuklearKeyToKeyboardKey(menu.keyLoadState)) && !menu.active) {
        LibretroMenuLoadStateClicked(menu.console, NULL);
    }

    // Prev Slot
    else if (IsKeyReleased(NuklearKeyToKeyboardKey(menu.keyPrevSlot)) && !menu.active) {
        menu.saveSlotIndex = (menu.saveSlotIndex - 1 + 10) % 10;
        SetLibretroMessage(TextFormat("Save Slot: %d", menu.saveSlotIndex + 1), 1.0f);
    }

    // Next Slot
    else if (IsKeyReleased(NuklearKeyToKeyboardKey(menu.keyNextSlot)) && !menu.active) {
        menu.saveSlotIndex = (menu.saveSlotIndex + 1) % 10;
        SetLibretroMessage(TextFormat("Save Slot: %d", menu.saveSlotIndex + 1), 1.0f);
    }

    // Reset
    else if (IsKeyReleased(NuklearKeyToKeyboardKey(menu.keyReset)) && !menu.active) {
        if (IsLibretroGameReady()) {
            ResetLibretro();
            SetLibretroMessage("Reset", 2.0f);
        }
    }

    // Volume Up
    else if (IsKeyReleased(NuklearKeyToKeyboardKey(menu.keyVolumeUp)) && !menu.active) {
        float vol = GetLibretroVolume() + 0.1f;
        SetLibretroVolume(vol);
        vol = GetLibretroVolume();
        menu.volumeSelected = vol;
        SetLibretroMessage(TextFormat("Volume: %d%%", (int)(vol * 10.0f + 0.5f) * 10), 1.0f);
    }

    // Volume Down
    else if (IsKeyReleased(NuklearKeyToKeyboardKey(menu.keyVolumeDown)) && !menu.active) {
        float vol = GetLibretroVolume() - 0.1f;
        SetLibretroVolume(vol);
        vol = GetLibretroVolume();
        menu.volumeSelected = vol;
        SetLibretroMessage(TextFormat("Volume: %d%%", (int)(vol * 10.0f + 0.5f) * 10), 1.0f);
    }

    // Mute
    else if (IsKeyPressed(NuklearKeyToKeyboardKey(menu.keyMute)) && !menu.active) {
        if (!data->muted) {
            data->savedVolumeBeforeMute = GetLibretroVolume();
            SetLibretroVolume(0.0f);
            data->muted = true;
            SetLibretroMessage("Muted", 1.0f);
        } else {
            SetLibretroVolume(data->savedVolumeBeforeMute);
            menu.volumeSelected = data->savedVolumeBeforeMute;
            data->muted = false;
            SetLibretroMessage(TextFormat("Volume: %d%%", (int)(data->savedVolumeBeforeMute * 10.0f + 0.5f) * 10), 1.0f);
        }
    }

    return true;
}

void Draw(void* userData) {
    AppData* data = (AppData*)userData;

    ClearBackground(BLACK);

    if (data->menu->active) {
        BeginLibretroShaderGreyscale();
        DrawLibretro();
        EndShaderMode();
    }
    else {
        BeginLibretroShader();
        DrawLibretro();
        EndLibretroShader();
    }

    DrawLibretroMenu();
    if (data->menu->touchControls && !data->menu->active) {
        DrawLibretroTouchControls();
    }
    DrawLibretroMessage();
}

void Close(void* userData) {
    AppData* data = (AppData*)userData;

    // Save all settings in one pass before closing.
    SaveLibretroAllSettings();

    // Free the rewind buffer.
    RewindBufferFree(&data->rewind);

    // Unload the game and close the core.
    UnloadLibretroGame();
    CloseLibretro();

    CloseLibretroMenu();

    UnloadLibretroShaders();
    CloseLibretroPhysFS();
    CloseAudioDevice();
    MemFree(data);
}

App Main() {
    return (App){
        .title = "raylib-libretro",
        .width = 1280,
        .height = 720,
        .init = Init,
        .update = Update,
        .draw = Draw,
        .close = Close,
        .configFlags = FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT,
    };
}
