/**********************************************************************************************
*
*   raylib-libretro-basic - A basic example of using raylib-libretro.h.
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

#define RAYLIB_APP_IMPLEMENTATION
#include "raylib-app.h"

#define RAYLIB_LIBRETRO_IMPLEMENTATION
#include "raylib-libretro.h"

typedef struct {
    const char* corePath;
    const char* gamePath;
} AppData;

bool Init(void** userData, int argc, char** argv) {
    if (argc <= 1) {
        TraceLog(LOG_ERROR, "Usage: %s <core> [game]", argv[0]);
        return false;
    }

    AppData* data = (AppData*)MemAlloc(sizeof(AppData));
    data->corePath = argv[1];
    data->gamePath = (argc > 2) ? argv[2] : NULL;
    *userData = data;

    InitAudioDevice();

    // Initialize the given core.
    if (!InitLibretro(data->corePath)) {
        TraceLog(LOG_ERROR, "Failed to initialize libretro core");
        CloseAudioDevice();
        return false;
    }

    // Load the given game.
    if (!LoadLibretroGame(data->gamePath)) {
        TraceLog(LOG_ERROR, "Failed to initialize libretro content");
        CloseLibretro();
        CloseAudioDevice();
        return false;
    }

    // Resize the window and set the title to match the loaded core.
    SetWindowSize((int)GetLibretroWidth(), (int)GetLibretroHeight());
    SetWindowTitle(GetLibretroName());

    return true;
}

bool UpdateDrawFrame(void* userData) {
    // Run a frame of the core.
    UpdateLibretro();

    // Render the libretro core.
    BeginDrawing();
        ClearBackground(BLACK);
        DrawLibretro();
    EndDrawing();

    return !LibretroShouldClose();
}

void Close(void* userData) {
    // Unload the game first, and then close the core.
    UnloadLibretroGame();
    CloseLibretro();

    CloseAudioDevice();
    MemFree(userData);
}

App Main() {
    return (App){
        .title = "raylib-libretro",
        .width = 800,
        .height = 600,
        .init = Init,
        .update = UpdateDrawFrame,
        .close = Close,
        .fps = 60,
        .configFlags = FLAG_WINDOW_RESIZABLE,
    };
}
