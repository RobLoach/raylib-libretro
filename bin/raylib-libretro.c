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
#define RAYLIB_LIBRETRO_IMPLEMENTATION
#include "raylib-libretro.h"

#define RAYLIB_LIBRETRO_SHADERS_IMPLEMENTATION
#include "../include/raylib-libretro-shaders.h"

int main(int argc, char* argv[]) {
    // Create the window and audio.
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(800, 600, "raylib-libretro");
    SetWindowMinSize(400, 300);
    InitAudioDevice();

    // Load the shaders and the menu.
    LoadLibretroShaders();

    // Parse the command line arguments.
    if (argc > 1) {
        // Initialize the given core.
        if (InitLibretro(argv[1])) {
            // Load the given game.
            const char* gameFile = (argc > 2) ? argv[2] : NULL;
            if (LoadLibretroGame(gameFile)) {
                // TODO: Quit?
            }
        }
    }

    while (!WindowShouldClose()) {
        // Update the shaders.
        UpdateLibretroShaders(GetFrameTime());

        // Run a frame of the core.
        UpdateLibretro();

        // Check if the core asks to be shutdown.
        if (LibretroShouldClose()) {
            UnloadLibretroGame();
            CloseLibretro();
        }

        // Render the libretro core.
        BeginDrawing();
        {
            ClearBackground(BLACK);

            BeginLibretroShader();
            DrawLibretro();
            EndLibretroShader();
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
            void* data = GetLibretroSerializedData(&size);
            if (data != NULL) {
                SaveFileData(TextFormat("save_%s.sav", GetLibretroName()), data, (int)size);
                MemFree(data);
            }
        }

        // Load State
        else if (IsKeyReleased(KEY_F4)) {
            int dataSize;
            void* data = LoadFileData(TextFormat("save_%s.sav", GetLibretroName()), &dataSize);
            if (data != NULL) {
                SetLibretroSerializedData(data, (unsigned int)dataSize);
                MemFree(data);
            }
        }
    }

    // Unload the game and close the core.
    UnloadLibretroGame();
    CloseLibretro();

    UnloadLibretroShaders();
    CloseAudioDevice();
    CloseWindow();

    return 0;
}
