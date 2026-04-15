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

#define RAYLIB_LIBRETRO_IMPLEMENTATION
#include "raylib-libretro.h"

#define RAYLIB_LIBRETRO_SHADERS_IMPLEMENTATION
#include "../include/raylib-libretro-shaders.h"

#define RAYLIB_LIBRETRO_MENU_IMPLEMENTATION
#include "../include/raylib-libretro-menu.h"

typedef struct {
    LibretroMenu* menu;
} AppData;

bool Init(void** userData, int argc, char** argv) {
    SetWindowMinSize(400, 300);

    AppData* data = (AppData*)MemAlloc(sizeof(AppData));
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

    // Update the shaders.
    UpdateLibretroShaders(GetFrameTime());

    // Run a frame of the core.
    if (!data->menu->active) {
        UpdateLibretro();
    }

    UpdateLibretroMenu();

    // Check if the core asks to be shutdown.
    if (LibretroShouldClose()) {
        UnloadLibretroGame();
        CloseLibretro();
    }

    // Render the libretro core.
    BeginDrawing();
    {
        ClearBackground(BLACK);

        if (data->menu->active) {
            BeginLibretroShader();
            DrawLibretroTint(ColorAlpha(WHITE, 0.1f));
            EndShaderMode();
        }
        else {
            BeginLibretroShader();
            DrawLibretro();
            EndLibretroShader();
        }

        DrawLibretroMenu();
    }
    EndDrawing();

    // Fullscreen
    if (IsKeyReleased(KEY_F11)) {
        ToggleFullscreen();
    }

    // Screenshot
    else if (IsKeyReleased(KEY_F8)) {
        for (int i = 1; i < 1000; i++) {
            const char* screenshotName = TextFormat("screenshot-%i.png", i);
            if (!FileExists(screenshotName)) {
                TakeScreenshot(screenshotName);
                break;
            }
        }
    }

    // Save State
    else if (IsKeyReleased(KEY_F2)) {
        unsigned int size;
        void* saveData = GetLibretroSerializedData(&size);
        if (saveData != NULL) {
            SaveFileData(TextFormat("save_%s.sav", GetLibretroName()), saveData, (int)size);
            MemFree(saveData);
        }
    }

    // Load State
    else if (IsKeyReleased(KEY_F4)) {
        int dataSize;
        void* saveData = LoadFileData(TextFormat("save_%s.sav", GetLibretroName()), &dataSize);
        if (saveData != NULL) {
            SetLibretroSerializedData(saveData, (unsigned int)dataSize);
            MemFree(saveData);
        }
    }

    return true;
}

void Close(void* userData) {
    AppData* data = (AppData*)userData;

    // Save core options before closing.
    if (IsLibretroReady()) {
        SaveLibretroCoreOptions();
    }

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
