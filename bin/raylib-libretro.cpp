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

#include "raylib-cpp.hpp"

#define RAYGUI_IMPLEMENTATION
#define RAYGUI_SUPPORT_ICONS
#include "raygui.hpp"

#undef RAYGUI_IMPLEMENTATION            // Avoid including raygui implementation again

#define GUI_FILE_DIALOG_IMPLEMENTATION
#include "../vendor/raygui/examples/custom_file_dialog/gui_file_dialog.h"

#include "../include/rlibretro.h"
#include "../src/shaders.h"
#include "../src/menu.h"

int main(int argc, char* argv[]) {
    // Ensure proper amount of arguments.
    SetTraceLogExit(LOG_FATAL);

    // Create the window and audio.
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    raylib::Window window(800, 600, "raylib-libretro");
    window.SetMinSize(575, 450);
    raylib::AudioDevice audioDevice;

    // Load the shaders and the menu.
    Shaders shaders;
    Menu menu(&shaders);

    // Parse the command line arguments.
    if (argc > 1) {
        // Initialize the given core.
        if (InitLibretro(argv[1])) {
            // Load the given game.
            const char* gameFile = (argc > 2) ? argv[2] : NULL;
            if (LoadLibretroGame(gameFile)) {
                menu.SetMenuActive(false);
            }
        }
    }

    while (!WindowShouldClose()) {
        // Fullscreen
        if (IsKeyReleased(KEY_F11)) {
            ToggleFullscreen();
        }

        // Update the shaders.
        shaders.Update();

        if (!menu.IsMenuActive()) {
            // Run a frame of the core.
            UpdateLibretro();
        }

        // Check if the core asks to be shutdown.
        if (LibretroShouldClose()) {
            UnloadLibretroGame();
            CloseLibretro();
        }

        // Render the libretro core.
        BeginDrawing();
        {
            ClearBackground(BLACK);

            shaders.Begin();

            DrawLibretro();

            shaders.End();

            menu.Update();
        }
        EndDrawing();
    }

    // Unload the game and close the core.
    UnloadLibretroGame();
    CloseLibretro();

    return 0;
}
