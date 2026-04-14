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
#include "../vendor/nuklear_console/nuklear_console.h"

typedef enum LibretroMenuStyle {
    LIBRETRO_MENU_STYLE_DEFAULT,
    LIBRETRO_MENU_STYLE_DRACULA,
} LibretroMenuStyle;

typedef struct LibretroMenu {
    struct nk_context* ctx;
    Font font;
    nk_console* console;
    bool active;
    struct nk_rect lastBounds;
    nk_bool fullscreen;
} LibretroMenu;

#if defined(__cplusplus)
extern "C" {
#endif

LibretroMenu* InitLibretroMenu(void);
bool IsLibretroMenuReady(void);
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

#define CVECTOR_H "../vendor/c-vector/cvector.h"
#define NK_CONSOLE_IMPLEMENTATION
#define NK_CONSOLE_MALLOC nk_raylib_malloc
#define NK_CONSOLE_FREE nk_raylib_mfree
#include "../vendor/nuklear_console/nuklear_console.h"

#if defined(__cplusplus)
extern "C" {
#endif

static LibretroMenu menu = {0};

LibretroMenu* GetLibretroMenu(void) {
    return &menu;
}

bool IsLibretroMenuReady(void) {
    return menu.ctx != NULL;
}

void SetLibretroMenuStyle(LibretroMenuStyle style) {
    if (!IsLibretroMenuReady()) {
        return;
    }

    struct nk_color table[NK_COLOR_COUNT];
    switch (style) {
        case LIBRETRO_MENU_STYLE_DRACULA:{
            struct nk_color background = nk_rgba(40, 42, 54, 0);
            struct nk_color currentline = nk_rgba(68, 71, 90, 255);
            struct nk_color foreground = nk_rgba(248, 248, 242, 255);
            struct nk_color comment = nk_rgba(98, 114, 164, 255);
            /* struct nk_color cyan = nk_rgba(139, 233, 253, 255); */
            /* struct nk_color green = nk_rgba(80, 250, 123, 255); */
            /* struct nk_color orange = nk_rgba(255, 184, 108, 255); */
            struct nk_color pink = nk_rgba(255, 121, 198, 255);
            struct nk_color purple = nk_rgba(189, 147, 249, 255);
            /* struct nk_color red = nk_rgba(255, 85, 85, 255); */
            /* struct nk_color yellow = nk_rgba(241, 250, 140, 255); */
            table[NK_COLOR_TEXT] = foreground;
            table[NK_COLOR_WINDOW] = background;
            table[NK_COLOR_HEADER] = currentline;
            table[NK_COLOR_BORDER] = currentline;
            table[NK_COLOR_BUTTON] = currentline;
            table[NK_COLOR_BUTTON_HOVER] = comment;
            table[NK_COLOR_BUTTON_ACTIVE] = purple;
            table[NK_COLOR_TOGGLE] = currentline;
            table[NK_COLOR_TOGGLE_HOVER] = comment;
            table[NK_COLOR_TOGGLE_CURSOR] = pink;
            table[NK_COLOR_SELECT] = currentline;
            table[NK_COLOR_SELECT_ACTIVE] = comment;
            table[NK_COLOR_SLIDER] = background;
            table[NK_COLOR_SLIDER_CURSOR] = currentline;
            table[NK_COLOR_SLIDER_CURSOR_HOVER] = comment;
            table[NK_COLOR_SLIDER_CURSOR_ACTIVE] = comment;
            table[NK_COLOR_PROPERTY] = currentline;
            table[NK_COLOR_EDIT] = currentline;
            table[NK_COLOR_EDIT_CURSOR] = foreground;
            table[NK_COLOR_COMBO] = currentline;
            table[NK_COLOR_CHART] = currentline;
            table[NK_COLOR_CHART_COLOR] = comment;
            table[NK_COLOR_CHART_COLOR_HIGHLIGHT] = purple;
            table[NK_COLOR_SCROLLBAR] = background;
            table[NK_COLOR_SCROLLBAR_CURSOR] = currentline;
            table[NK_COLOR_SCROLLBAR_CURSOR_HOVER] = comment;
            table[NK_COLOR_SCROLLBAR_CURSOR_ACTIVE] = purple;
            table[NK_COLOR_TAB_HEADER] = currentline;
            table[NK_COLOR_KNOB] = table[NK_COLOR_SLIDER];
            table[NK_COLOR_KNOB_CURSOR] = table[NK_COLOR_SLIDER_CURSOR];
            table[NK_COLOR_KNOB_CURSOR_HOVER] = table[NK_COLOR_SLIDER_CURSOR_HOVER];
            table[NK_COLOR_KNOB_CURSOR_ACTIVE] = table[NK_COLOR_SLIDER_CURSOR_ACTIVE];
            nk_style_from_table(menu.ctx, table);
            break;
        }
        case LIBRETRO_MENU_STYLE_DEFAULT:
        default:
            nk_style_default(menu.ctx);
            break;
    }
}

static void LibretroMenuFullscreenChanged(nk_console* widget, void* user_data) {
    (void)widget;
    (void)user_data;
    ToggleFullscreen();
}

LibretroMenu* InitLibretroMenu(void) {
    menu = (LibretroMenu){0};
    menu.font = LoadFontFromNuklear(52);
    if (!IsFontValid(menu.font)) {
        return NULL;
    }

    menu.ctx = InitNuklearEx(menu.font, 26.0f);
    //menu.ctx = InitNuklear(32);
    if (!menu.ctx) {
        UnloadFont(menu.font);
        return NULL;
    }

    menu.console = nk_console_init(menu.ctx);
    if (!menu.console) {
        UnloadFont(menu.font);
        UnloadNuklear(menu.ctx);
        return NULL;
    }

    // Build the Menu
    nk_console_button(menu.console, "Load Game");
    menu.fullscreen = (nk_bool)IsWindowFullscreen();
    nk_console* options = nk_console_button(menu.console, "Options");
    {
        nk_console* fullscreenCheckbox = nk_console_checkbox(options, "Fullscreen", &menu.fullscreen);
        nk_console_add_event(fullscreenCheckbox, NK_CONSOLE_EVENT_CHANGED, LibretroMenuFullscreenChanged);
        nk_console_button_onclick(options, "Back", &nk_console_button_back);
    }
    nk_console_button(menu.console, "Settings");
    nk_console_button(menu.console, "Save State");
    nk_console_button(menu.console, "Load State");
    nk_console_button(menu.console, "Quit");

    SetLibretroMenuStyle(LIBRETRO_MENU_STYLE_DRACULA);
    menu.active = true;

    return &menu;
}

void CloseLibretroMenu(void) {
    if (menu.ctx == NULL) {
        return;
    }

    if (menu.console != NULL) {
        nk_console_free(menu.console);
        menu.console = NULL;
    }
    if (menu.ctx != NULL) {
        UnloadNuklear(menu.ctx);
        menu.ctx = NULL;
    }

    if (IsFontValid(menu.font)) {
        UnloadFont(menu.font);
        menu.font.recs = NULL;
    }
}

void UpdateLibretroMenu(void) {
    if (menu.ctx == NULL) {
        return;
    }

    // Toggle the menu
    if (IsGamepadButtonReleased(0, GAMEPAD_BUTTON_MIDDLE) || IsKeyReleased(KEY_F1)) {
        menu.active = !menu.active;
    }

    if (!menu.active) {
        return;
    }

    // Keep fullscreen checkbox in sync with the actual window state (e.g. F11 presses).
    menu.fullscreen = (nk_bool)IsWindowFullscreen();

    // Render the console centered in the screen, using last frame's bounds for height
    struct nk_rect windowPos;

    // Scale it appropriately.
    if (GetScreenWidth() > 1280 && GetScreenHeight() > 720) {
        SetNuklearScaling(menu.ctx, 2.0f);
        windowPos.w = NK_MAX(GetScreenWidth() / 3.0f, 640);
    }
    else {
        SetNuklearScaling(menu.ctx, 1.0f);
        windowPos.w = NK_MAX(GetScreenWidth() / 3.0f, 360);
    }

    UpdateNuklear(menu.ctx);

    windowPos.h = menu.lastBounds.h > 0 ? menu.lastBounds.h : (float)GetScreenHeight();
    if (windowPos.h > GetScreenHeight()) {
        windowPos.h = GetScreenHeight();
        windowPos.y = 0;
    }
    else {
        windowPos.y = (float)GetScreenHeight() * 0.5f - windowPos.h * 0.5f;
    }
    windowPos.x = (float)GetScreenWidth() * 0.5f - windowPos.w * 0.5f;

    if (GetNuklearScaling(menu.ctx) != 1.0f) {
        windowPos.x /= GetNuklearScaling(menu.ctx);
        windowPos.y /= GetNuklearScaling(menu.ctx);
        windowPos.w /= GetNuklearScaling(menu.ctx);
        windowPos.h /= GetNuklearScaling(menu.ctx);
    }

    menu.lastBounds = nk_console_render_window(menu.console, "raylib-libretro", windowPos, NK_WINDOW_SCROLL_AUTO_HIDE);
    if (GetNuklearScaling(menu.ctx) != 1.0f) {
        menu.lastBounds.x *= GetNuklearScaling(menu.ctx);
        menu.lastBounds.y *= GetNuklearScaling(menu.ctx);
        menu.lastBounds.w *= GetNuklearScaling(menu.ctx);
        menu.lastBounds.h *= GetNuklearScaling(menu.ctx);
    }
}

void DrawLibretroMenu(void) {
    if (menu.ctx == NULL) {
        return;
    }
    if (!menu.active) {
        return;
    }
    DrawNuklear(menu.ctx);
}

#if defined(__cplusplus)
}
#endif

#endif // RAYLIB_LIBRETRO_IMPLEMENTATION_ONCE
#endif // RAYLIB_LIBRETRO_IMPLEMENTATION
