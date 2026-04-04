/**********************************************************************************************
*
*   rlibretro - Raylib extension to interact with libretro cores.
*
*   DEPENDENCIES:
*            - raylib
*            - dl
*            - libretro-common
*              - dynamic/dylib
*              - libretro.h
*
*   LICENSE: zlib/libpng
*
*   rLibretro is licensed under an unmodified zlib/libpng license, which is an OSI-certified,
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

#ifndef RAYLIB_LIBRETRO_MENU_H
#define RAYLIB_LIBRETRO_MENU_H

#include "../vendor/raylib-nuklear/include/raylib-nuklear.h"

#if defined(__cplusplus)
extern "C" {
#endif

bool InitLibretroMenu(void);
void CloseLibretroMenu(void);
void UpdateLibretroMenu(void);
void DrawLibretroMenu(void);

#if defined(__cplusplus)
}
#endif

#endif

#ifdef RAYLIB_LIBRETRO_MENU_IMPLEMENTATION
#ifndef RAYLIB_LIBRETRO_MENU_IMPLEMENTATION_ONCE
#define RAYLIB_LIBRETRO_MENU_IMPLEMENTATION_ONCE

#define RAYLIB_NUKLEAR_INCLUDE_DEFAULT_FONT
#define RAYLIB_NUKLEAR_IMPLEMENTATION
#include "../vendor/raylib-nuklear/include/raylib-nuklear.h"

#define NK_GAMEPAD_IMPLEMENTATION
#include "../vendor/nuklear_gamepad/nuklear_gamepad.h"

#define NK_CONSOLE_IMPLEMENTATION
#define NK_CONSOLE_MALLOC nk_raylib_malloc
#define NK_CONSOLE_FREE nk_raylib_mfree
#include "../vendor/nuklear_console/nuklear_console.h"

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * The Nuklear Context.
 */
struct nk_context* ctx = NULL;
Font font;
nk_console* console = NULL;

bool InitLibretroMenu(void) {
    font = LoadFontFromNuklear(32);
    if (!IsFontValid(font)) {
        return false;
    }

    ctx = InitNuklearEx(font, 32.0f);
    if (!ctx) {
        UnloadFont(font);
        return false;
    }

    console = nk_console_init(ctx);
    if (!console) {
        UnloadFont(font);
        UnloadNuklear(ctx);
        return false;
    }

    // Build the Menu
    nk_console_button(console, "New Game");
    nk_console* options = nk_console_button(console, "Options");
    {
        nk_console_button(options, "Some cool option!");
        nk_console_button(options, "Option #2");
        nk_console_button_onclick(options, "Back", &nk_console_button_back);
    }
    nk_console_button(console, "Load Game");
    nk_console_button(console, "Save Game");
}

void CloseLibretroMenu(void) {
    if (ctx == NULL) {
        return;
    }

    if (console != NULL) {
        nk_console_free(console);
        console = NULL;
    }
    if (ctx != NULL) {
        UnloadNuklear(ctx);
        ctx = NULL;
    }

    if (IsFontValid(font)) {
        UnloadFont(font);
        font.recs = NULL;
    }
}

void UpdateLibretroMenu(void) {
    if (ctx == NULL) {
        return;
    }
    UpdateNuklear(ctx);


    // Render the console in a window
    nk_console_render_window(console, "raylib-libretro", nk_rect(0, 0, NK_MAX(GetScreenWidth() / 3.5f, 300), GetScreenHeight()), NK_WINDOW_TITLE);
}

void DrawLibretroMenu(void) {
    if (ctx == NULL) {
        return;
    }
    DrawNuklear(ctx);
}

#if defined(__cplusplus)
}
#endif

#endif // RAYLIB_LIBRETRO_IMPLEMENTATION_ONCE
#endif // RAYLIB_LIBRETRO_IMPLEMENTATION
