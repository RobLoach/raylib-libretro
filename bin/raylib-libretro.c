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
} AppData;

bool Init(void** userData, int argc, char** argv) {
    SetWindowMinSize(400, 300);

    AppData* data = (AppData*)MemAlloc(sizeof(AppData));
    memset(data, 0, sizeof(AppData));
    *userData = data;

    InitAudioDevice();

    // Load the shaders and the menu.
    LoadLibretroShaders();
    data->menu = InitLibretroMenu();
    if (!data->menu) {
        TraceLog(LOG_ERROR, "Failed to initialize menu");
        UnloadLibretroShaders();
        CloseAudioDevice();
        return false;
    }

    // Parse the command line arguments.
    if (argc > 1) {
        // Initialize the given core.
        if (InitLibretro(argv[1])) {
            // Apply any previously saved options before the game starts.
            LoadLibretroCoreOptions();

            // Load the given game.
            const char* gameFile = (argc > 2) ? argv[2] : NULL;
            if (LoadLibretroGame(gameFile)) {
                BuildLibretroMenuOptions(data->menu);
                data->menu->active = false;
            }
        }
    }

    return true;
}

bool UpdateDrawFrame(void* userData) {
    AppData* data = (AppData*)userData;

    // Update the shaders, then show OSD if a shader key was pressed.
    LibretroShaderType shaderTypeBefore = GetActiveLibretroShaderType();
    UpdateLibretroShaders(GetFrameTime());
    if (GetActiveLibretroShaderType() != shaderTypeBefore) {
        ShowLibretroMessage(GetLibretroShaderName(GetActiveLibretroShaderType()), 2.0f);
    }

    // Run a frame of the core.
    if (!data->menu->active) {
        if (data->menu->rewindEnabled && IsLibretroGameReady()) {
            if (IsKeyDown(KEY_R)) {
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
        UpdateLibretro();
    }

    UpdateLibretroMenu();

    // Check if the core or menu asks to be shutdown.
    if (LibretroShouldClose()) {
        RewindBufferFree(&data->rewind);
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
    }
    EndDrawing();

    // Fullscreen
    if (IsKeyReleased(KEY_F11)) {
        LibretroMenuFullscreenChanged(menu.console, NULL);
    }

    // Screenshot
    else if (IsKeyReleased(KEY_F8)) {
        const char* screenshotName = NULL;
        for (int i = 1; i < 1000; i++) {
            screenshotName = TextFormat("screenshot-%i.png", i);
            if (!FileExists(screenshotName)) {
                TakeScreenshot(screenshotName);
                break;
            }
        }
        if (screenshotName) {
            ShowLibretroMessage(TextFormat("Screenshot: %s", screenshotName), 2.0f);
        }
    }

    // Save State
    else if (IsKeyReleased(KEY_F2)) {
        LibretroMenuSaveStateClicked(menu.console, NULL);
    }

    // Load State
    else if (IsKeyReleased(KEY_F4)) {
        LibretroMenuLoadStateClicked(menu.console, NULL);
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
