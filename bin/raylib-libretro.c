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

#define RAYLIB_LIBRETRO_LOGO_IMPLEMENTATION
#include "../include/raylib-libretro-logo.h"

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

#define REWIND_CAPTURES_PER_SECOND 30
#define REWIND_CAPTURE_INTERVAL (1.0f / REWIND_CAPTURES_PER_SECOND)  // seconds between snapshots
#define REWIND_BUFFER_FRAMES 600  // REWIND_BUFFER_FRAMES / REWIND_CAPTURES_PER_SECOND seconds of rewind

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
    float rewindTimer;  // seconds accumulated toward the next rewind capture/step
    bool muted;
    bool pendingMenuOpen;
} AppData;

bool Init(void** userData, int argc, char** argv) {
    Image logo = GetLibretroLogo();
    SetWindowIcon(logo);
    UnloadImage(logo);

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
        UpdateLibretroShaders(GetFrameTime());

        // While rewinding we step back one snapshot per capture interval and
        // render only on that step (via UpdateLibretroEx(true)), so the picture holds
        // between steps instead of running the core forward.
        bool rewinding = false;

        if (IsLibretroGameReady()) {
            KeyboardKey rewindKey = NuklearKeyToKeyboardKey(data->menu->keyRewind);
            rewinding = data->menu->rewindEnabled && IsKeyDown(rewindKey);

            // Capture and playback both run at REWIND_CAPTURES_PER_SECOND.
            if (data->menu->rewindEnabled) {
                data->rewindTimer += GetFrameTime();
            }

            if (rewinding) {
                if (data->rewindTimer >= REWIND_CAPTURE_INTERVAL) {
                    data->rewindTimer = 0.0f;
                    void* stateData = NULL;
                    unsigned int stateSize = 0;
                    if (RewindBufferPop(&data->rewind, &stateData, &stateSize)) {
                        if (stateSize != GetLibretroSerializedSize()) {
                            // Snapshots belong to a different game/core (a new
                            // game was loaded). Drop the stale buffer instead of
                            // feeding the core a mismatched state.
                            MemFree(stateData);
                            RewindBufferFree(&data->rewind);
                        } else {
                            SetLibretroSerializedData(stateData, stateSize);
                            MemFree(stateData);
                            SetLibretroMessage("Rewind", 1.0f);
                            // Render the restored snapshot. Bypasses the time
                            // accumulator, which would run zero ticks when called
                            // this sparsely and leave the display frozen.
                            UpdateLibretroEx(true);
                        }
                    } else {
                        SetLibretroMessage("Rewind limit reached", 1.0f);
                    }
                }
            } else {
                // Capture a snapshot at the interval while playing forward.
                if (data->menu->rewindEnabled) {
                    if (data->rewindTimer >= REWIND_CAPTURE_INTERVAL) {
                        data->rewindTimer = 0.0f;
                        unsigned int size = 0;
                        void* state = GetLibretroSerializedData(&size);
                        if (state != NULL) {
                            RewindBufferPush(&data->rewind, state, size);
                        }
                    }
                    if (IsKeyReleased(rewindKey)) {
                        SetLibretroMessage(NULL, 0.0f);
                    }
                } else if (data->rewind.count > 0) {
                    RewindBufferFree(&data->rewind);
                }

                // Fast Forward
                KeyboardKey key = NuklearKeyToKeyboardKey(data->menu->keyFastForward);
                KeyboardKey slowMotionKey = NuklearKeyToKeyboardKey(data->menu->keySlowMotion);
                if (IsKeyDown(key)) {
                    if (IsKeyPressed(key)) {
                        // Halve the volume during fast-forward to soften the
                        // sped-up audio and ring-buffer overrun crackle.
                        SetLibretroVolume(data->menu->volumeSelected * 0.5f);
                        SetLibretroSpeed(data->menu->fastForwardSpeed);
                        SetLibretroMessage("Fast Forward", 1.0f);
                    }
                    // The time accumulator in UpdateLibretro() scales by LIBRETRO.speed,
                    // so the single UpdateLibretro() call below runs the extra ticks.
                }
                else if (IsKeyReleased(key)) {
                    SetLibretroVolume(data->menu->volumeSelected);
                    SetLibretroSpeed(1.0f);
                    SetLibretroMessage(NULL, 0.0f);
                }

                // Slow Motion
                else if (IsKeyDown(slowMotionKey)) {
                    if (IsKeyPressed(slowMotionKey)) {
                        SetLibretroSpeed(data->menu->slowMotionSpeed);
                        SetLibretroMessage("Slow Motion", 1.0f);
                    }
                }
                else if (IsKeyReleased(slowMotionKey)) {
                    SetLibretroSpeed(1.0f);
                    SetLibretroMessage(NULL, 0.0f);
                }
            }
        }

        // Run a paced frame while playing forward. During rewind the core is
        // advanced by UpdateLibretroEx(true) above, so skip the accumulator here.
        if (!rewinding) {
            UpdateLibretro();
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
            // The previous game's rewind snapshots are now meaningless.
            RewindBufferFree(&data->rewind);
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

    // Hot Keys
    if (!menu.active) {

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

        // FullScreen toggle key won't work in Emscripten.
    #ifndef __EMSCRIPTEN__
        else if (IsKeyReleased(NuklearKeyToKeyboardKey(menu.keyFullscreen))) {
            LibretroMenuFullscreenChanged(menu.console, NULL);
        }
    #endif

        // Cycle Shader Reverse
        else if (IsKeyReleased(NuklearKeyToKeyboardKey(menu.keyPrevShader))) {
            CycleLibretroShaderReverse();
            SetLibretroMessage(GetLibretroShaderName(GetActiveLibretroShaderType()), 2.0f);
            menu.shaderSelectedIndex = (int)GetActiveLibretroShaderType();
        }

        // Cycle Shader Next
        else if (IsKeyReleased(NuklearKeyToKeyboardKey(menu.keyNextShader))) {
            CycleLibretroShader();
            SetLibretroMessage(GetLibretroShaderName(GetActiveLibretroShaderType()), 2.0f);
            // TODO: For some reason, cycling the shader doens't update the menu label.
            menu.shaderSelectedIndex = (int)GetActiveLibretroShaderType();
        }

        // Save State
        else if (IsKeyReleased(NuklearKeyToKeyboardKey(menu.keySaveState))) {
            LibretroMenuSaveStateClicked(menu.console, NULL);
        }

        // Load State
        else if (IsKeyReleased(NuklearKeyToKeyboardKey(menu.keyLoadState))) {
            LibretroMenuLoadStateClicked(menu.console, NULL);
        }

        // Prev Slot
        else if (IsKeyReleased(NuklearKeyToKeyboardKey(menu.keyPrevSlot))) {
            menu.saveSlotIndex = (menu.saveSlotIndex - 1 + 10) % 10;
            SetLibretroMessage(TextFormat("Save Slot: %d", menu.saveSlotIndex + 1), 1.0f);
        }

        // Next Slot
        else if (IsKeyReleased(NuklearKeyToKeyboardKey(menu.keyNextSlot)) ) {
            menu.saveSlotIndex = (menu.saveSlotIndex + 1) % 10;
            SetLibretroMessage(TextFormat("Save Slot: %d", menu.saveSlotIndex + 1), 1.0f);
        }

        // Reset
        else if (IsKeyReleased(NuklearKeyToKeyboardKey(menu.keyReset))) {
            if (IsLibretroGameReady()) {
                ResetLibretro();
                SetLibretroMessage("Reset", 2.0f);
            }
        }

        // Volume Up
        else if (IsKeyReleased(NuklearKeyToKeyboardKey(menu.keyVolumeUp))) {
            float vol = GetLibretroVolume() + 0.1f;
            SetLibretroVolume(vol);
            vol = GetLibretroVolume();
            menu.volumeSelected = vol;
            SetLibretroMessage(TextFormat("Volume: %d%%", (int)(vol * 10.0f + 0.5f) * 10), 1.0f);
        }

        // Volume Down
        else if (IsKeyReleased(NuklearKeyToKeyboardKey(menu.keyVolumeDown))) {
            float vol = GetLibretroVolume() - 0.1f;
            SetLibretroVolume(vol);
            vol = GetLibretroVolume();
            menu.volumeSelected = vol;
            SetLibretroMessage(TextFormat("Volume: %d%%", (int)(vol * 10.0f + 0.5f) * 10), 1.0f);
        }

        // Mute
        else if (IsKeyPressed(NuklearKeyToKeyboardKey(menu.keyMute))) {
            if (!data->muted) {
                SetLibretroVolume(0.0f);
                data->muted = true;
                SetLibretroMessage("Muted", 1.0f);
            } else {
                SetLibretroVolume(data->menu->volumeSelected);
                data->muted = false;
                SetLibretroMessage(TextFormat("Volume: %d%%", (int)(data->menu->volumeSelected * 10.0f + 0.5f) * 10), 1.0f);
            }
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
