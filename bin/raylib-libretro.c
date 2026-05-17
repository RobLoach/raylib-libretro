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
        if ((TextIsEqual(argv[i], "-L") || TextIsEqual(argv[i], "--libretro")) && i + 1 < argc) {
            corePath = argv[++i];
        } else if (!gameFile) {
            gameFile = argv[i];
        }
    }

    if (corePath) {
        if (MenuInitCore(corePath) && LoadLibretroGamePhysFS(gameFile)) {
            BuildLibretroMenuOptions(data->menu);
            data->menu->active = false;
        }
    } else if (gameFile) {
        data->menu->active = !MenuLoadGame(gameFile);
    }

    return true;
}

// On-screen button descriptor.
typedef struct {
    Rectangle rect;   // screen-space rectangle (pixels)
    int buttonId;     // RETRO_DEVICE_ID_JOYPAD_*
    const char* label;
    Color color;
} OnScreenButton;

// Build layout. All values are fractions of screen dimensions.
static int BuildOnScreenButtons(OnScreenButton* btns, int w, int h) {
    float bw = w * 0.09f;    // button width
    float bh = h * 0.09f;    // button height
    float pad = w * 0.015f;

    // D-pad: bottom-left
    float dx = w * 0.06f, dy = h * 0.72f;
    int n = 0;
    btns[n++] = (OnScreenButton){{ dx,          dy + bh + pad, bw, bh }, RETRO_DEVICE_ID_JOYPAD_LEFT,  "<", DARKGRAY };
    btns[n++] = (OnScreenButton){{ dx + bw+pad, dy,            bw, bh }, RETRO_DEVICE_ID_JOYPAD_UP,    "^", DARKGRAY };
    btns[n++] = (OnScreenButton){{ dx + bw+pad, dy + bh + pad, bw, bh }, RETRO_DEVICE_ID_JOYPAD_DOWN,  "v", DARKGRAY };
    btns[n++] = (OnScreenButton){{ dx+2*(bw+pad),dy + bh + pad,bw, bh }, RETRO_DEVICE_ID_JOYPAD_RIGHT, ">", DARKGRAY };

    // Face buttons: bottom-right (SNES layout)
    float fx = w * 0.72f;
    btns[n++] = (OnScreenButton){{ fx + bw+pad, dy,               bw, bh }, RETRO_DEVICE_ID_JOYPAD_X,     "X", DARKBLUE  };
    btns[n++] = (OnScreenButton){{ fx,          dy + bh + pad,    bw, bh }, RETRO_DEVICE_ID_JOYPAD_Y,     "Y", DARKPURPLE};
    btns[n++] = (OnScreenButton){{ fx+2*(bw+pad),dy + bh + pad,   bw, bh }, RETRO_DEVICE_ID_JOYPAD_A,     "A", MAROON    };
    btns[n++] = (OnScreenButton){{ fx + bw+pad, dy + bh*2+pad*2,  bw, bh }, RETRO_DEVICE_ID_JOYPAD_B,     "B", DARKGREEN };

    // Select / Start: bottom center
    float cx = w * 0.37f, cy = h * 0.91f;
    btns[n++] = (OnScreenButton){{ cx,          cy,            bw, bh }, RETRO_DEVICE_ID_JOYPAD_SELECT, "SEL", GRAY    };
    btns[n++] = (OnScreenButton){{ cx+bw+pad*3, cy,            bw, bh }, RETRO_DEVICE_ID_JOYPAD_START,  "STA", GRAY   };
    return n;
}

#define ONSCREEN_BTN_COUNT 10

static void UpdateOnScreenControls(LibretroMenu* m) {
    OnScreenButton btns[ONSCREEN_BTN_COUNT];
    int w = GetScreenWidth(), h = GetScreenHeight();
    int n = BuildOnScreenButtons(btns, w, h);

    // Clear virtual state each frame.
    for (int i = 0; i < 16; i++) LibretroCore.virtualJoypadState[i] = false;

    // Collect all active touch/click points.
    Vector2 points[10];
    int nPoints = 0;

    int touchCount = GetTouchPointCount();
    for (int t = 0; t < touchCount && nPoints < 10; t++) {
        points[nPoints++] = GetTouchPosition(t);
    }
    // Also support mouse on desktop.
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        points[nPoints++] = GetMousePosition();
    }

    for (int i = 0; i < n; i++) {
        for (int p = 0; p < nPoints; p++) {
            if (CheckCollisionPointRec(points[p], btns[i].rect)) {
                LibretroCore.virtualJoypadState[btns[i].buttonId] = true;
                break;
            }
        }
    }
}

static void DrawOnScreenControls(LibretroMenu* m) {
    OnScreenButton btns[ONSCREEN_BTN_COUNT];
    int w = GetScreenWidth(), h = GetScreenHeight();
    int n = BuildOnScreenButtons(btns, w, h);

    // Guide button: top-right corner.
    float bw = w * 0.09f, bh = h * 0.09f;
    Rectangle guide = { w - bw - w*0.02f, h * 0.02f, bw, bh };

    for (int i = 0; i < n; i++) {
        Color c = btns[i].color;
        c.a = LibretroCore.virtualJoypadState[btns[i].buttonId] ? 220 : 140;
        DrawRectangleRounded(btns[i].rect, 0.3f, 4, c);
        int fs = (int)(bh * 0.4f);
        DrawText(btns[i].label,
            (int)(btns[i].rect.x + btns[i].rect.width/2  - MeasureText(btns[i].label, fs)/2),
            (int)(btns[i].rect.y + btns[i].rect.height/2 - fs/2),
            fs, WHITE);
    }

    // Guide button
    bool guideHovered = false;
    int touchCount = GetTouchPointCount();
    for (int t = 0; t < touchCount; t++) {
        if (CheckCollisionPointRec(GetTouchPosition(t), guide)) { guideHovered = true; break; }
    }
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), guide)) {
        guideHovered = true;
    }
    Color gc = guideHovered ? GRAY : (Color){80,80,80,160};
    DrawRectangleRounded(guide, 0.3f, 4, gc);
    int gfs = (int)(bh * 0.35f);
    const char* gl = "MENU";
    DrawText(gl, (int)(guide.x + guide.width/2 - MeasureText(gl, gfs)/2),
             (int)(guide.y + guide.height/2 - gfs/2), gfs, WHITE);

    // Open menu on guide press.
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), guide)) {
        m->active = true;
    }
    for (int t = 0; t < touchCount; t++) {
        if (CheckCollisionPointRec(GetTouchPosition(t), guide)) {
            if (IsGestureDetected(GESTURE_TAP)) m->active = true;
            break;
        }
    }
}

bool UpdateDrawFrame(void* userData) {
    AppData* data = (AppData*)userData;

    // Update virtual joypad from on-screen controls.
    if (data->menu->onScreenControls && !data->menu->active) {
        UpdateOnScreenControls(data->menu);
    } else {
        for (int i = 0; i < 16; i++) LibretroCore.virtualJoypadState[i] = false;
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
                    ShowLibretroMessage("Rewind limit reached", 1.0f);
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
                    ShowLibretroMessage(TextFormat("Fast Forward %dx", data->menu->fastForwardSpeed), 1.0f);
                }
            } else if (smDown) {
                if (GetLibretroSpeed() > 1.0f) SetLibretroVolume(data->savedVolume);
                if (GetLibretroSpeed() != data->menu->slowMotionSpeed) {
                    SetLibretroSpeed(data->menu->slowMotionSpeed);
                    ShowLibretroMessage(TextFormat("Slow Motion %.0f%%", data->menu->slowMotionSpeed * 100.0f), 1.0f);
                }
            } else if (GetLibretroSpeed() != 1.0f) {
                if (GetLibretroSpeed() > 1.0f) SetLibretroVolume(data->savedVolume);
                SetLibretroSpeed(1.0f);
                ShowLibretroMessage("Normal Speed", 1.0f);
            }
        }

        bool wasFastForwarding = GetLibretroSpeed() > 1.0f;
        if (GetLibretroSpeed() > 1.0f) {
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

    // Render the libretro core.
    BeginDrawing();
    {
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
        DrawLibretroMessage();
        if (data->menu->onScreenControls && !data->menu->active) {
            DrawOnScreenControls(data->menu);
        }
    }
    EndDrawing();

    // Fullscreen
    if (IsKeyReleased(NuklearKeyToKeyboardKey(menu.keyFullscreen))) {
        LibretroMenuFullscreenChanged(menu.console, NULL);
    }

    // Screenshot
    else if (IsKeyReleased(NuklearKeyToKeyboardKey(menu.keyScreenshot))) {
        const char* screenshotsDir = GetLibretroDirectory(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY);
        bool taken = false;
        for (int i = 1; i < 1000; i++) {
            const char* screenshotName = TextFormat("%s/screenshot-%i.png", screenshotsDir, i);
            if (!FileExists(screenshotName)) {
                TakeScreenshot(screenshotName);
                ShowLibretroMessage(TextFormat("Screenshot: %s", screenshotName), 2.0f);
                taken = true;
                break;
            }
        }
        if (!taken) {
            ShowLibretroMessage("Screenshot slots full", 2.0f);
        }
    }

    // Cycle Shader Reverse
    else if (IsKeyReleased(NuklearKeyToKeyboardKey(menu.keyPrevShader)) && !menu.active) {
        CycleLibretroShaderReverse();
        ShowLibretroMessage(GetLibretroShaderName(GetActiveLibretroShaderType()), 2.0f);
        menu.shaderSelectedIndex = (int)GetActiveLibretroShaderType();
    }

    // Cycle Shader Next
    else if (IsKeyReleased(NuklearKeyToKeyboardKey(menu.keyNextShader)) && !menu.active) {
        CycleLibretroShader();
        ShowLibretroMessage(GetLibretroShaderName(GetActiveLibretroShaderType()), 2.0f);
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
        ShowLibretroMessage(TextFormat("Save Slot: %d", menu.saveSlotIndex + 1), 1.0f);
    }

    // Next Slot
    else if (IsKeyReleased(NuklearKeyToKeyboardKey(menu.keyNextSlot)) && !menu.active) {
        menu.saveSlotIndex = (menu.saveSlotIndex + 1) % 10;
        ShowLibretroMessage(TextFormat("Save Slot: %d", menu.saveSlotIndex + 1), 1.0f);
    }

    // Reset
    else if (IsKeyReleased(NuklearKeyToKeyboardKey(menu.keyReset)) && !menu.active) {
        if (IsLibretroGameReady()) {
            ResetLibretro();
            ShowLibretroMessage("Reset", 2.0f);
        }
    }

    // Volume
    else if (IsKeyReleased(NuklearKeyToKeyboardKey(menu.keyVolumeUp)) && !menu.active) {
        float vol = GetLibretroVolume() + 0.1f;
        SetLibretroVolume(vol);
        vol = GetLibretroVolume();
        menu.volumeSelected = vol;
        ShowLibretroMessage(TextFormat("Volume: %d%%", (int)(vol * 10.0f + 0.5f) * 10), 1.0f);
    }
    else if (IsKeyReleased(NuklearKeyToKeyboardKey(menu.keyVolumeDown)) && !menu.active) {
        float vol = GetLibretroVolume() - 0.1f;
        SetLibretroVolume(vol);
        vol = GetLibretroVolume();
        menu.volumeSelected = vol;
        ShowLibretroMessage(TextFormat("Volume: %d%%", (int)(vol * 10.0f + 0.5f) * 10), 1.0f);
    }

    // Mute
    else if (IsKeyPressed(NuklearKeyToKeyboardKey(menu.keyMute)) && !menu.active) {
        if (!data->muted) {
            data->savedVolumeBeforeMute = GetLibretroVolume();
            SetLibretroVolume(0.0f);
            data->muted = true;
            ShowLibretroMessage("Muted", 1.0f);
        } else {
            SetLibretroVolume(data->savedVolumeBeforeMute);
            menu.volumeSelected = data->savedVolumeBeforeMute;
            data->muted = false;
            ShowLibretroMessage(TextFormat("Volume: %d%%", (int)(data->savedVolumeBeforeMute * 10.0f + 0.5f) * 10), 1.0f);
        }
    }

    return true;
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
        .width = 800,
        .height = 600,
        .init = Init,
        .update = UpdateDrawFrame,
        .close = Close,
        .configFlags = FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT,
    };
}
