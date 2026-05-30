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
*   LibretroData is licensed under an unmodified zlib/libpng license, which is an OSI-certified,
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

#define NK_GAMEPAD_RAYLIB
#define NK_BUTTON_TRIGGER_ON_RELEASE
#define RAYLIB_NUKLEAR_INCLUDE_DEFAULT_FONT
#include "../vendor/raylib-nuklear/include/raylib-nuklear.h"
#include "../../vendor/nuklear_gamepad/nuklear_gamepad.h"
#include "../vendor/nuklear_console/nuklear_console.h"
#include "raylib-libretro-physfs.h"

typedef enum LibretroMenuStyle {
    LIBRETRO_MENU_STYLE_CATPPUCCIN_MOCHA,
    LIBRETRO_MENU_STYLE_CATPPUCCIN_LATTE,
    LIBRETRO_MENU_STYLE_CATPPUCCIN_FRAPPE,
    LIBRETRO_MENU_STYLE_CATPPUCCIN_MACCHIATO,
    LIBRETRO_MENU_STYLE_DRACULA,
    LIBRETRO_MENU_STYLE_DARK,
    LIBRETRO_MENU_STYLE_COUNT,
} LibretroMenuStyle;

typedef struct LibretroMenu {
    struct nk_context* ctx;
    Font font;
    nk_console* console;
    struct nk_gamepads gamepads;
    bool active;
    bool shouldQuit;
    nk_bool fullscreen;
    nk_bool vsync;
    int fpsIndex;                         // index into the "FPS" combobox (Auto/30/60/120/144/240/Unlimited)
    nk_console* optionsMenu;              // "Core Options" submenu node
    nk_console* loadGameWidget;
    nk_console* saveStateButton;
    nk_console* loadStateButton;
    nk_console* resumeButton;
    nk_console* resetGameButton;
    nk_console* cheatsMenuButton;
    //nk_console* closeGameButton;
    int shaderSelectedIndex;
    int textureFilterIndex;
    int themeSelectedIndex;
    float volumeSelected;
    bool rewindEnabled;
    int analogToDpadIndex;
    nk_rune keyScreenshot;
    nk_rune keyRewind;
    nk_rune keyMenu;
    nk_rune keySaveState;
    nk_rune keyLoadState;
    nk_rune keyPrevSlot;
    nk_rune keyNextSlot;
    int saveSlotIndex;
    nk_rune keyFullscreen;
    nk_rune keyPrevShader;
    nk_rune keyNextShader;
    nk_rune keyReset;
    nk_rune keyQuit;
    nk_rune keyVolumeUp;
    nk_rune keyVolumeDown;
    nk_rune keyMute;
    nk_rune keyFastForward;
    float fastForwardSpeed;
    nk_rune keySlowMotion;
    float slowMotionSpeed;
    int optionSelectedIndices[128];       // per-option combobox index (matches LIBRETRO_MAX_CORE_VARIABLES)
    nk_bool optionCheckboxValues[128];    // per-option checkbox state for enabled/disabled options
    char coreDirectory[RAYLIB_LIBRETRO_VFS_MAX_PATH];
    char saveDirectory[RAYLIB_LIBRETRO_VFS_MAX_PATH];
    char coreAssetsDirectory[RAYLIB_LIBRETRO_VFS_MAX_PATH];
    char systemDirectory[RAYLIB_LIBRETRO_VFS_MAX_PATH];
    char playlistsDirectory[RAYLIB_LIBRETRO_VFS_MAX_PATH];
    char fileBrowserStartDirectory[RAYLIB_LIBRETRO_VFS_MAX_PATH];
    char loadGamePath[RAYLIB_LIBRETRO_VFS_MAX_PATH];
    bool touchControls;
    bool touchHapticsEnabled;
    char cheatBuffer[256];
    char cheatList[1024];
    unsigned cheatIndex;
    nk_rune keyboardP1[16]; // Indexed by RETRO_DEVICE_ID_JOYPAD_*
#ifdef RAYLIB_LIBRETRO_CONFIG_H
    RLibretroConfig* cfg;                 // persistent config, owned for the lifetime of the menu
#endif
} LibretroMenu;

#if defined(__cplusplus)
extern "C" {
#endif

LibretroMenu* InitLibretroMenu(void);
void CloseLibretroMenu(void);
void UpdateLibretroMenu(void);
void DrawLibretroMenu(void);
void BuildLibretroMenuOptions(LibretroMenu* menu); // Populate "Core Options" with comboboxes from the loaded core.
bool LoadLibretroCoreOptions(void);    // Apply saved core options from config to the loaded core.
bool SaveLibretroAllSettings(void);    // Save menu settings + core options in a single file write.
static Font GetLibretroMenuFont(void);

#if defined(__cplusplus)
}
#endif

#endif

#ifdef RAYLIB_LIBRETRO_MENU_IMPLEMENTATION
#ifndef RAYLIB_LIBRETRO_MENU_IMPLEMENTATION_ONCE
#define RAYLIB_LIBRETRO_MENU_IMPLEMENTATION_ONCE

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>

// Push MEMFS -> IDBFS so any writes under /userdata survive a page reload.
// Called at every commit point (settings save, save state, etc.) because
// Firefox aggressively kills the JS context on tab close and won't reliably
// complete an async syncfs queued from pagehide.
static void LibretroFlushPersistentStorage(void) {
    EM_ASM({
        FS.syncfs(false, function (err) {
            if (err) console.error('IDBFS sync failed:', err);
        });
    });
}
#else
#define LibretroFlushPersistentStorage() ((void)0)
#endif

#define RAYLIB_NUKLEAR_IMPLEMENTATION
#include "../vendor/raylib-nuklear/include/raylib-nuklear.h"

#define NK_GAMEPAD_IMPLEMENTATION
#define NK_GAMEPAD_RAYLIB
#include "../vendor/nuklear_gamepad/nuklear_gamepad.h"

/* Redirect cvector allocations through raylib's memory functions. */
#define cvector_clib_malloc(sz)    MemAlloc((unsigned int)(sz))
#define cvector_clib_realloc(p,sz) MemRealloc((p), (unsigned int)(sz))
#define cvector_clib_free          MemFree
#define cvector_clib_calloc(n,sz)  memset(MemAlloc((unsigned int)((n)*(sz))), 0, (size_t)((n)*(sz)))
#define CVECTOR_H "../vendor/c-vector/cvector.h"

#define NK_CONSOLE_IMPLEMENTATION
#define NK_CONSOLE_MALLOC nk_raylib_malloc
#define NK_CONSOLE_FREE nk_raylib_mfree
#include "../vendor/nuklear_console/nuklear_console.h"

#ifndef RAYLIB_LIBRETRO_CFG_FILE
#define RAYLIB_LIBRETRO_CFG_FILE "raylib-libretro.cfg"
#endif

// Number of increments a float settings slider divides its range into.
#ifndef RAYLIB_LIBRETRO_MENU_SLIDER_STEPS
#define RAYLIB_LIBRETRO_MENU_SLIDER_STEPS 20.0f
#endif

// Step size that divides [min, max] into RAYLIB_LIBRETRO_MENU_SLIDER_STEPS steps.
#define RAYLIB_LIBRETRO_MENU_SLIDER_STEP(min, max) (((max) - (min)) / RAYLIB_LIBRETRO_MENU_SLIDER_STEPS)

#if defined(__cplusplus)
extern "C" {
#endif

static LibretroMenu menu = {0};

static void SetLibretroMenuStyle(LibretroMenuStyle style);
static bool IsLibretroMenuReady(void);
static bool SaveLibretroMenuSettings(void);
static bool SaveLibretroCoreOptions(void);
static bool LoadLibretroMenuSettings(void);
static void UpdateLibretroMenuVisibility(void);
static void LibretroMenuApplyKeyboardPlayer1(void);
static Font GetLibretroMenuFont(void) {
    return menu.font;
}

static void LibretroMenuSettingChanged(nk_console* widget, void* user_data) {
    (void)widget;
    (void)user_data;
    SetActiveLibretroShader((LibretroShaderType)menu.shaderSelectedIndex);
    SetLibretroMenuStyle((LibretroMenuStyle)menu.themeSelectedIndex);
    SetLibretroVolume(menu.volumeSelected);
    if (LIBRETRO.textureFilter != menu.textureFilterIndex) {
        LIBRETRO.textureFilter = menu.textureFilterIndex;
        InitLibretroVideo();
    }
    SetExitKey(NuklearKeyToKeyboardKey(menu.keyQuit));
}

static LibretroMenu* GetLibretroMenu(void) {
    return &menu;
}

static bool IsLibretroMenuReady(void) {
    return menu.ctx != NULL;
}

static void SetLibretroMenuStyle(LibretroMenuStyle style) {
    if (!IsLibretroMenuReady()) {
        return;
    }
    struct nk_color table[NK_COLOR_COUNT];
    struct nk_context* ctx = menu.ctx;

    // Reset the styles to default first.
    nk_style_default(menu.ctx);

    switch (style) {
        case LIBRETRO_MENU_STYLE_DRACULA: {
            struct nk_color background = nk_rgba(40, 42, 54, 235);
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
        case LIBRETRO_MENU_STYLE_CATPPUCCIN_LATTE: {
            /*struct nk_color rosewater = nk_rgba(220, 138, 120, 255);*/
            /*struct nk_color flamingo = nk_rgba(221, 120, 120, 255);*/
            struct nk_color pink = nk_rgba(234, 118, 203, 255);
            struct nk_color mauve = nk_rgba(136, 57, 239, 255);
            /*struct nk_color red = nk_rgba(210, 15, 57, 255);*/
            /*struct nk_color maroon = nk_rgba(230, 69, 83, 255);*/
            /*struct nk_color peach = nk_rgba(254, 100, 11, 255);*/
            struct nk_color yellow = nk_rgba(223, 142, 29, 255);
            /*struct nk_color green = nk_rgba(64, 160, 43, 255);*/
            struct nk_color teal = nk_rgba(23, 146, 153, 255);
            /*struct nk_color sky = nk_rgba(4, 165, 229, 255);*/
            /*struct nk_color sapphire = nk_rgba(32, 159, 181, 255);*/
            /*struct nk_color blue = nk_rgba(30, 102, 245, 255);*/
            /*struct nk_color lavender = nk_rgba(114, 135, 253, 255);*/
            struct nk_color text = nk_rgba(76, 79, 105, 255);
            /*struct nk_color subtext1 = nk_rgba(92, 95, 119, 255);*/
            /*struct nk_color subtext0 = nk_rgba(108, 111, 133, 255);*/
            struct nk_color overlay2 = nk_rgba(124, 127, 147, 55);
            /*struct nk_color overlay1 = nk_rgba(140, 143, 161, 255);*/
            struct nk_color overlay0 = nk_rgba(156, 160, 176, 255);
            struct nk_color surface2 = nk_rgba(172, 176, 190, 255);
            struct nk_color surface1 = nk_rgba(188, 192, 204, 255);
            struct nk_color surface0 = nk_rgba(204, 208, 218, 255);
            struct nk_color base = nk_rgba(239, 241, 245, 235);
            struct nk_color mantle = nk_rgba(230, 233, 239, 255);
            /*struct nk_color crust = nk_rgba(220, 224, 232, 255);*/
            table[NK_COLOR_TEXT] = text;
            table[NK_COLOR_WINDOW] = base;
            table[NK_COLOR_HEADER] = mantle;
            table[NK_COLOR_BORDER] = mantle;
            table[NK_COLOR_BUTTON] = surface0;
            table[NK_COLOR_BUTTON_HOVER] = overlay2;
            table[NK_COLOR_BUTTON_ACTIVE] = overlay0;
            table[NK_COLOR_TOGGLE] = surface2;
            table[NK_COLOR_TOGGLE_HOVER] = overlay2;
            table[NK_COLOR_TOGGLE_CURSOR] = yellow;
            table[NK_COLOR_SELECT] = surface0;
            table[NK_COLOR_SELECT_ACTIVE] = overlay0;
            table[NK_COLOR_SLIDER] = surface1;
            table[NK_COLOR_SLIDER_CURSOR] = teal;
            table[NK_COLOR_SLIDER_CURSOR_HOVER] = teal;
            table[NK_COLOR_SLIDER_CURSOR_ACTIVE] = teal;
            table[NK_COLOR_PROPERTY] = surface0;
            table[NK_COLOR_EDIT] = surface0;
            table[NK_COLOR_EDIT_CURSOR] = mauve;
            table[NK_COLOR_COMBO] = surface0;
            table[NK_COLOR_CHART] = surface0;
            table[NK_COLOR_CHART_COLOR] = teal;
            table[NK_COLOR_CHART_COLOR_HIGHLIGHT] = mauve;
            table[NK_COLOR_SCROLLBAR] = surface0;
            table[NK_COLOR_SCROLLBAR_CURSOR] = overlay0;
            table[NK_COLOR_SCROLLBAR_CURSOR_HOVER] = mauve;
            table[NK_COLOR_SCROLLBAR_CURSOR_ACTIVE] = mauve;
            table[NK_COLOR_TAB_HEADER] = surface0;
            table[NK_COLOR_KNOB] = table[NK_COLOR_SLIDER];
            table[NK_COLOR_KNOB_CURSOR] = pink;
            table[NK_COLOR_KNOB_CURSOR_HOVER] = pink;
            table[NK_COLOR_KNOB_CURSOR_ACTIVE] = pink;
            nk_style_from_table(ctx, table);
            break;
        }
        case LIBRETRO_MENU_STYLE_CATPPUCCIN_FRAPPE: {
            /*struct nk_color rosewater = nk_rgba(242, 213, 207, 255);*/
            /*struct nk_color flamingo = nk_rgba(238, 190, 190, 255);*/
            struct nk_color pink = nk_rgba(244, 184, 228, 255);
            /*struct nk_color mauve = nk_rgba(202, 158, 230, 255);*/
            /*struct nk_color red = nk_rgba(231, 130, 132, 255);*/
            /*struct nk_color maroon = nk_rgba(234, 153, 156, 255);*/
            /*struct nk_color peach = nk_rgba(239, 159, 118, 255);*/
            /*struct nk_color yellow = nk_rgba(229, 200, 144, 255);*/
            struct nk_color green = nk_rgba(166, 209, 137, 255);
            /*struct nk_color teal = nk_rgba(129, 200, 190, 255);*/
            /*struct nk_color sky = nk_rgba(153, 209, 219, 255);*/
            /*struct nk_color sapphire = nk_rgba(133, 193, 220, 255);*/
            /*struct nk_color blue = nk_rgba(140, 170, 238, 255);*/
            struct nk_color lavender = nk_rgba(186, 187, 241, 255);
            struct nk_color text = nk_rgba(198, 208, 245, 255);
            /*struct nk_color subtext1 = nk_rgba(181, 191, 226, 255);*/
            /*struct nk_color subtext0 = nk_rgba(165, 173, 206, 255);*/
            struct nk_color overlay2 = nk_rgba(148, 156, 187, 255);
            struct nk_color overlay1 = nk_rgba(131, 139, 167, 255);
            struct nk_color overlay0 = nk_rgba(115, 121, 148, 255);
            struct nk_color surface2 = nk_rgba(98, 104, 128, 255);
            struct nk_color surface1 = nk_rgba(81, 87, 109, 255);
            struct nk_color surface0 = nk_rgba(65, 69, 89, 255);
            struct nk_color base = nk_rgba(48, 52, 70, 235);
            struct nk_color mantle = nk_rgba(41, 44, 60, 255);
            /*struct nk_color crust = nk_rgba(35, 38, 52, 255);*/
            table[NK_COLOR_TEXT] = text;
            table[NK_COLOR_WINDOW] = base;
            table[NK_COLOR_HEADER] = mantle;
            table[NK_COLOR_BORDER] = mantle;
            table[NK_COLOR_BUTTON] = surface0;
            table[NK_COLOR_BUTTON_HOVER] = overlay1;
            table[NK_COLOR_BUTTON_ACTIVE] = overlay0;
            table[NK_COLOR_TOGGLE] = surface2;
            table[NK_COLOR_TOGGLE_HOVER] = overlay2;
            table[NK_COLOR_TOGGLE_CURSOR] = pink;
            table[NK_COLOR_SELECT] = surface0;
            table[NK_COLOR_SELECT_ACTIVE] = overlay0;
            table[NK_COLOR_SLIDER] = surface1;
            table[NK_COLOR_SLIDER_CURSOR] = green;
            table[NK_COLOR_SLIDER_CURSOR_HOVER] = green;
            table[NK_COLOR_SLIDER_CURSOR_ACTIVE] = green;
            table[NK_COLOR_PROPERTY] = surface0;
            table[NK_COLOR_EDIT] = surface0;
            table[NK_COLOR_EDIT_CURSOR] = pink;
            table[NK_COLOR_COMBO] = surface0;
            table[NK_COLOR_CHART] = surface0;
            table[NK_COLOR_CHART_COLOR] = lavender;
            table[NK_COLOR_CHART_COLOR_HIGHLIGHT] = pink;
            table[NK_COLOR_SCROLLBAR] = surface0;
            table[NK_COLOR_SCROLLBAR_CURSOR] = overlay0;
            table[NK_COLOR_SCROLLBAR_CURSOR_HOVER] = lavender;
            table[NK_COLOR_SCROLLBAR_CURSOR_ACTIVE] = lavender;
            table[NK_COLOR_TAB_HEADER] = surface0;
            table[NK_COLOR_KNOB] = table[NK_COLOR_SLIDER];
            table[NK_COLOR_KNOB_CURSOR] = pink;
            table[NK_COLOR_KNOB_CURSOR_HOVER] = pink;
            table[NK_COLOR_KNOB_CURSOR_ACTIVE] = pink;
            nk_style_from_table(ctx, table);
            break;
        }
        case LIBRETRO_MENU_STYLE_CATPPUCCIN_MACCHIATO: {
            /*struct nk_color rosewater = nk_rgba(244, 219, 214, 255);*/
            /*struct nk_color flamingo = nk_rgba(240, 198, 198, 255);*/
            struct nk_color pink = nk_rgba(245, 189, 230, 255);
            /*struct nk_color mauve = nk_rgba(198, 160, 246, 255);*/
            /*struct nk_color red = nk_rgba(237, 135, 150, 255);*/
            /*struct nk_color maroon = nk_rgba(238, 153, 160, 255);*/
            /*struct nk_color peach = nk_rgba(245, 169, 127, 255);*/
            struct nk_color yellow = nk_rgba(238, 212, 159, 255);
            struct nk_color green = nk_rgba(166, 218, 149, 255);
            /*struct nk_color teal = nk_rgba(139, 213, 202, 255);*/
            /*struct nk_color sky = nk_rgba(145, 215, 227, 255);*/
            /*struct nk_color sapphire = nk_rgba(125, 196, 228, 255);*/
            /*struct nk_color blue = nk_rgba(138, 173, 244, 255);*/
            struct nk_color lavender = nk_rgba(183, 189, 248, 255);
            struct nk_color text = nk_rgba(202, 211, 245, 255);
            /*struct nk_color subtext1 = nk_rgba(184, 192, 224, 255);*/
            /*struct nk_color subtext0 = nk_rgba(165, 173, 203, 255);*/
            struct nk_color overlay2 = nk_rgba(147, 154, 183, 255);
            struct nk_color overlay1 = nk_rgba(128, 135, 162, 255);
            struct nk_color overlay0 = nk_rgba(110, 115, 141, 255);
            struct nk_color surface2 = nk_rgba(91, 96, 120, 255);
            struct nk_color surface1 = nk_rgba(73, 77, 100, 255);
            struct nk_color surface0 = nk_rgba(54, 58, 79, 255);
            struct nk_color base = nk_rgba(36, 39, 58, 235);
            struct nk_color mantle = nk_rgba(30, 32, 48, 255);
            /*struct nk_color crust = nk_rgba(24, 25, 38, 255);*/
            table[NK_COLOR_TEXT] = text;
            table[NK_COLOR_WINDOW] = base;
            table[NK_COLOR_HEADER] = mantle;
            table[NK_COLOR_BORDER] = mantle;
            table[NK_COLOR_BUTTON] = surface0;
            table[NK_COLOR_BUTTON_HOVER] = overlay1;
            table[NK_COLOR_BUTTON_ACTIVE] = overlay0;
            table[NK_COLOR_TOGGLE] = surface2;
            table[NK_COLOR_TOGGLE_HOVER] = overlay2;
            table[NK_COLOR_TOGGLE_CURSOR] = yellow;
            table[NK_COLOR_SELECT] = surface0;
            table[NK_COLOR_SELECT_ACTIVE] = overlay0;
            table[NK_COLOR_SLIDER] = surface1;
            table[NK_COLOR_SLIDER_CURSOR] = green;
            table[NK_COLOR_SLIDER_CURSOR_HOVER] = green;
            table[NK_COLOR_SLIDER_CURSOR_ACTIVE] = green;
            table[NK_COLOR_PROPERTY] = surface0;
            table[NK_COLOR_EDIT] = surface0;
            table[NK_COLOR_EDIT_CURSOR] = pink;
            table[NK_COLOR_COMBO] = surface0;
            table[NK_COLOR_CHART] = surface0;
            table[NK_COLOR_CHART_COLOR] = lavender;
            table[NK_COLOR_CHART_COLOR_HIGHLIGHT] = yellow;
            table[NK_COLOR_SCROLLBAR] = surface0;
            table[NK_COLOR_SCROLLBAR_CURSOR] = overlay0;
            table[NK_COLOR_SCROLLBAR_CURSOR_HOVER] = lavender;
            table[NK_COLOR_SCROLLBAR_CURSOR_ACTIVE] = lavender;
            table[NK_COLOR_TAB_HEADER] = surface0;
            table[NK_COLOR_KNOB] = table[NK_COLOR_SLIDER];
            table[NK_COLOR_KNOB_CURSOR] = pink;
            table[NK_COLOR_KNOB_CURSOR_HOVER] = pink;
            table[NK_COLOR_KNOB_CURSOR_ACTIVE] = pink;
            nk_style_from_table(ctx, table);
            break;
        }
        case LIBRETRO_MENU_STYLE_CATPPUCCIN_MOCHA: {
            /*struct nk_color rosewater = nk_rgba(245, 224, 220, 255);*/
            /*struct nk_color flamingo = nk_rgba(242, 205, 205, 255);*/
            struct nk_color pink = nk_rgba(245, 194, 231, 255);
            /*struct nk_color mauve = nk_rgba(203, 166, 247, 255);*/
            /*struct nk_color red = nk_rgba(243, 139, 168, 255);*/
            /*struct nk_color maroon = nk_rgba(235, 160, 172, 255);*/
            /*struct nk_color peach = nk_rgba(250, 179, 135, 255);*/
            /*struct nk_color yellow = nk_rgba(249, 226, 175, 255);*/
            struct nk_color green = nk_rgba(166, 227, 161, 255);
            /*struct nk_color teal = nk_rgba(148, 226, 213, 255);*/
            /*struct nk_color sky = nk_rgba(137, 220, 235, 255);*/
            /*struct nk_color sapphire = nk_rgba(116, 199, 236, 255);*/
            /*struct nk_color blue = nk_rgba(137, 180, 250, 255);*/
            struct nk_color lavender = nk_rgba(180, 190, 254, 255);
            struct nk_color text = nk_rgba(205, 214, 244, 255);
            /*struct nk_color subtext1 = nk_rgba(186, 194, 222, 255);*/
            /*struct nk_color subtext0 = nk_rgba(166, 173, 200, 255);*/
            struct nk_color overlay2 = nk_rgba(147, 153, 178, 255);
            struct nk_color overlay1 = nk_rgba(127, 132, 156, 255);
            struct nk_color overlay0 = nk_rgba(108, 112, 134, 255);
            struct nk_color surface2 = nk_rgba(88, 91, 112, 255);
            struct nk_color surface1 = nk_rgba(69, 71, 90, 255);
            struct nk_color surface0 = nk_rgba(49, 50, 68, 255);
            struct nk_color base = nk_rgba(30, 30, 46, 235);
            struct nk_color mantle = nk_rgba(24, 24, 37, 255);
            /*struct nk_color crust = nk_rgba(17, 17, 27, 255);*/
            table[NK_COLOR_TEXT] = text;
            table[NK_COLOR_WINDOW] = base;
            table[NK_COLOR_HEADER] = mantle;
            table[NK_COLOR_BORDER] = mantle;
            table[NK_COLOR_BUTTON] = surface0;
            table[NK_COLOR_BUTTON_HOVER] = overlay1;
            table[NK_COLOR_BUTTON_ACTIVE] = overlay0;
            table[NK_COLOR_TOGGLE] = surface2;
            table[NK_COLOR_TOGGLE_HOVER] = overlay2;
            table[NK_COLOR_TOGGLE_CURSOR] = lavender;
            table[NK_COLOR_SELECT] = surface0;
            table[NK_COLOR_SELECT_ACTIVE] = overlay0;
            table[NK_COLOR_SLIDER] = surface1;
            table[NK_COLOR_SLIDER_CURSOR] = green;
            table[NK_COLOR_SLIDER_CURSOR_HOVER] = green;
            table[NK_COLOR_SLIDER_CURSOR_ACTIVE] = green;
            table[NK_COLOR_PROPERTY] = surface0;
            table[NK_COLOR_EDIT] = surface0;
            table[NK_COLOR_EDIT_CURSOR] = lavender;
            table[NK_COLOR_COMBO] = surface0;
            table[NK_COLOR_CHART] = surface0;
            table[NK_COLOR_CHART_COLOR] = lavender;
            table[NK_COLOR_CHART_COLOR_HIGHLIGHT] = pink;
            table[NK_COLOR_SCROLLBAR] = surface0;
            table[NK_COLOR_SCROLLBAR_CURSOR] = overlay0;
            table[NK_COLOR_SCROLLBAR_CURSOR_HOVER] = lavender;
            table[NK_COLOR_SCROLLBAR_CURSOR_ACTIVE] = pink;
            table[NK_COLOR_TAB_HEADER] = surface0;
            table[NK_COLOR_KNOB] = table[NK_COLOR_SLIDER];
            table[NK_COLOR_KNOB_CURSOR] = pink;
            table[NK_COLOR_KNOB_CURSOR_HOVER] = pink;
            table[NK_COLOR_KNOB_CURSOR_ACTIVE] = pink;
            nk_style_from_table(ctx, table);
            break;
        }
        case LIBRETRO_MENU_STYLE_DARK:
        default: {
            // Nothing
            break;
        }
    }
}

// Map the "FPS" combobox index to a SetTargetFPS() value. "Auto" follows the
// monitor refresh rate; "Unlimited" returns 0 (no cap).
static int LibretroMenuResolveTargetFps(void) {
    switch (menu.fpsIndex) {
        case 1: return 30;
        case 2: return 60;
        case 3: return 120;
        case 4: return 144;
        case 5: return 240;
        case 6: return 0; // Unlimited
        case 0:
        default: {
            int refreshRate = GetMonitorRefreshRate(GetCurrentMonitor());
            return refreshRate > 0 ? refreshRate : 60;
        }
    }
}

// Apply the current VSYNC + FPS selection to the window. Emulation speed is
// driven by the time accumulator in UpdateLibretro(), so these only govern how
// often the frontend renders/polls; "Auto" doubles as a busy-loop safety cap
// when a compositor or driver ignores the vsync hint.
static void LibretroMenuApplyVideoSettings(void) {
    if (menu.vsync) {
        SetWindowState(FLAG_VSYNC_HINT);
    } else {
        ClearWindowState(FLAG_VSYNC_HINT);
    }
    SetTargetFPS(LibretroMenuResolveTargetFps());
}

static void LibretroMenuVideoChanged(nk_console* widget, void* user_data) {
    NK_UNUSED(widget);
    NK_UNUSED(user_data);
    LibretroMenuApplyVideoSettings();
}

static void LibretroMenuFullscreenChanged(nk_console* widget, void* user_data) {
    (void)widget;
    (void)user_data;
#ifdef __EMSCRIPTEN__
    // requestFullscreen() must originate from a user-gesture event handler.
    // Raylib buffers input between frames, so by the time this callback fires
    // we are inside requestAnimationFrame — the gesture context is gone.
    // deferUntilInEventHandler=1 queues the request until the next user input.
    if (menu.fullscreen) {
        emscripten_request_fullscreen("#canvas", 1);
    } else {
        emscripten_exit_fullscreen();
    }
#else
    ToggleFullscreen();
    menu.fullscreen = IsWindowFullscreen();
#endif
}

static void LibretroMenuQuitClicked(nk_console* widget, void* user_data) {
    (void)widget;
    (void)user_data;
    menu.shouldQuit = true;
}

static void LibretroMenuSaveStateClicked(nk_console* widget, void* user_data) {
    (void)widget;
    (void)user_data;
    if (!IsLibretroGameReady()) return;
    unsigned int size;
    void* saveData = GetLibretroSerializedData(&size);
    if (saveData != NULL) {
        const char* savesDir = GetLibretroDirectory(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY);
        SaveFileData(TextFormat("%s/%s_%02d.sav", savesDir, GetLibretroContentName(), menu.saveSlotIndex + 1), saveData, (int)size);
        MemFree(saveData);
        LibretroFlushPersistentStorage();
        SetLibretroMessage(TextFormat("Slot %d Saved", menu.saveSlotIndex + 1), 2.0);
        menu.active = false;
    }
    else {
        SetLibretroMessage("State Saved Failed", 2.0);
    }
}

static void MenuResumeClicked(nk_console* widget, void* user_data) {
    NK_UNUSED(widget);
    NK_UNUSED(user_data);
    if (IsLibretroGameReady()) {
        menu.active = false;
    }
}

static void MenuResetGameClicked(nk_console* widget, void* user_data) {
    NK_UNUSED(widget);
    NK_UNUSED(user_data);
    if (!IsLibretroGameReady()) return;
    ResetLibretro();
    menu.active = false;
}

static void LibretroMenuCheatChanged(nk_console* widget, void* user_data) {
    NK_UNUSED(widget);
    NK_UNUSED(user_data);
    if (menu.cheatBuffer[0] == '\0') return;
    if (SetLibretroCheat(menu.cheatIndex, true, menu.cheatBuffer)) {
        int len = TextLength(menu.cheatList);
        const char* line = TextFormat("%u. %s\n", menu.cheatIndex + 1, menu.cheatBuffer);
        if (len + TextLength(line) < (int)sizeof(menu.cheatList)) {
            TextAppend(menu.cheatList, line, &len);
        }
        SetLibretroMessage(TextFormat("Cheat %u applied", menu.cheatIndex + 1), 2.0);
        menu.cheatIndex++;
    } else {
        SetLibretroMessage("Cheat failed", 2.0);
    }
    menu.cheatBuffer[0] = '\0';
}

static void LibretroMenuResetCheatsClicked(nk_console* widget, void* user_data) {
    NK_UNUSED(widget);
    NK_UNUSED(user_data);
    if (ResetLibretroCheats()) {
        menu.cheatIndex = 0;
        menu.cheatBuffer[0] = '\0';
        menu.cheatList[0] = '\0';
        SetLibretroMessage("Cheats reset", 2.0);
    }
}

// Fired when the user navigates back from Settings or Core Options. This is
// the natural commit point: anything the user just touched in those submenus
// is now part of the loaded state, so write the cfg out and flush IDBFS so
// the change survives a page reload / tab close on the web.
static void MenuCommitSettings(nk_console* widget, void* user_data) {
    NK_UNUSED(widget);
    NK_UNUSED(user_data);
    LibretroMenuApplyKeyboardPlayer1();
    LIBRETRO.analogToDpadIndex = menu.analogToDpadIndex;
    SaveLibretroAllSettings();
}

// Close Game has been removed for now, since it's not really needed.
// static void MenuCloseGameClicked(nk_console* widget, void* user_data) {
//     NK_UNUSED(widget);
//     NK_UNUSED(user_data);
//     if (!IsLibretroReady()) {
//         return;
//     }
//     if (IsLibretroGameReady()) {
//         UnloadLibretroGame();
//     }
//     CloseLibretro();
//     UpdateLibretroMenuVisibility();
// }

static void ScanLibretroCoreDirectory(void);

static void LibretroApplyDirectories(void) {
    TextCopy(LIBRETRO.coreDirectory,            menu.coreDirectory);
    TextCopy(LIBRETRO.saveDirectory,            menu.saveDirectory);
    TextCopy(LIBRETRO.coreAssetsDirectory,      menu.coreAssetsDirectory);
    TextCopy(LIBRETRO.systemDirectory,          menu.systemDirectory);
    TextCopy(LIBRETRO.playlistsDirectory,       menu.playlistsDirectory);
    TextCopy(LIBRETRO.fileBrowserStartDirectory, menu.fileBrowserStartDirectory);
    if (menu.saveDirectory[0] != '\0' && !DirectoryExists(menu.saveDirectory)) {
        MakeDirectory(menu.saveDirectory);
    }
}

static void MenuDirChanged(nk_console* widget, void* user_data) {
    LibretroMenuSettingChanged(widget, user_data);
    LibretroApplyDirectories();
}

static void MenuCoreDirChanged(nk_console* widget, void* user_data) {
    MenuDirChanged(widget, user_data);
    ScanLibretroCoreDirectory();
}

static void MenuContentDirChanged(nk_console* widget, void* user_data) {
    MenuDirChanged(widget, user_data);
    if (menu.loadGameWidget && menu.fileBrowserStartDirectory[0] != '\0') {
        nk_console_file_set_directory(menu.loadGameWidget, menu.fileBrowserStartDirectory);
    }
}

#define LIBRETRO_CORE_CACHE_SECTION "core_cache"

static bool IsLibretroCoreFile(const char* path) {
    const char* ext = GetFileExtension(path);
    return TextIsEqual(ext, ".so") || TextIsEqual(ext, ".dll") || TextIsEqual(ext, ".dylib") || TextIsEqual(ext, ".wasm");
}

static int LibretroCoreDirectoryFileCount(const char* dir) {
    if (!dir || !dir[0] || !DirectoryExists(dir)) return 0;
    FilePathList files = LoadDirectoryFiles(dir);
    int count = 0;
    for (unsigned int i = 0; i < files.count; i++) {
        if (IsLibretroCoreFile(files.paths[i])) count++;
    }
    UnloadDirectoryFiles(files);
    return count;
}

static void ScanLibretroCoreDirectory(void) {
#ifdef RAYLIB_LIBRETRO_CONFIG_H
    if (!menu.cfg) return;
    const char* dir = menu.coreDirectory;
    if (!dir || !dir[0]) return;

    // If a core is already loaded we can't peek at other cores; just invalidate
    // so the next launch performs a full rescan.
    if (IsLibretroReady()) {
        rlconfig_set_int(menu.cfg, LIBRETRO_CORE_CACHE_SECTION, "file_count", -1);
        rlconfig_save(menu.cfg, RAYLIB_LIBRETRO_CFG_FILE);
        return;
    }

    int currentCount = LibretroCoreDirectoryFileCount(dir);
    int cachedCount = rlconfig_get_int(menu.cfg, LIBRETRO_CORE_CACHE_SECTION, "file_count", -1);
    const char* cachedDir = rlconfig_get(menu.cfg, LIBRETRO_CORE_CACHE_SECTION, "dir_path");
    if (cachedCount == currentCount && cachedDir && TextIsEqual(cachedDir, dir)) {
        int cached = rlconfig_get_int(menu.cfg, LIBRETRO_CORE_CACHE_SECTION, "count", 0);
        TraceLog(LOG_INFO, "LIBRETRO: Core cache valid (%d cores)", cached);
        return;
    }

    TraceLog(LOG_INFO, "LIBRETRO: Rescanning cores in %s", dir);
    rlconfig_clear_section(menu.cfg, LIBRETRO_CORE_CACHE_SECTION);
    rlconfig_set_int(menu.cfg, LIBRETRO_CORE_CACHE_SECTION, "file_count", currentCount);
    rlconfig_set(menu.cfg, LIBRETRO_CORE_CACHE_SECTION, "dir_path", dir);

    if (!DirectoryExists(dir)) {
        rlconfig_save(menu.cfg, RAYLIB_LIBRETRO_CFG_FILE);
        return;
    }

    FilePathList files = LoadDirectoryFiles(dir);
    int coreIndex = 0;
    for (unsigned int i = 0; i < files.count; i++) {
        if (!IsLibretroCoreFile(files.paths[i])) continue;
        if (!PeekLibretroCoreInfo(files.paths[i])) continue;
        if (TextLength(LIBRETRO.core.validExtensions) == 0) {
            CloseLibretro();
            continue;
        }

        char keyPath[64], keyExts[64];
        TextCopy(keyPath, TextFormat("core_%d_path", coreIndex));
        TextCopy(keyExts, TextFormat("core_%d_extensions", coreIndex));
        rlconfig_set(menu.cfg, LIBRETRO_CORE_CACHE_SECTION, keyPath, files.paths[i]);
        rlconfig_set(menu.cfg, LIBRETRO_CORE_CACHE_SECTION, keyExts, LIBRETRO.core.validExtensions);
        TraceLog(LOG_INFO, "LIBRETRO: Cached %s (%s)", LIBRETRO.core.libraryName, LIBRETRO.core.validExtensions);
        CloseLibretro();
        coreIndex++;
    }
    UnloadDirectoryFiles(files);

    rlconfig_set_int(menu.cfg, LIBRETRO_CORE_CACHE_SECTION, "count", coreIndex);
    rlconfig_save(menu.cfg, RAYLIB_LIBRETRO_CFG_FILE);
    TraceLog(LOG_INFO, "LIBRETRO: Cached %d cores in %s", coreIndex, dir);
#endif
}

/**
 * Peek inside a .zip and return the first inner-file extension that any
 * cached core advertises in its valid_extensions.
 *
 * Mounts the archive at a scratch PhysFS namespace ("/peek") so an active
 * /game mount used by an in-flight load is not disturbed, enumerates the
 * archive recursively, and matches against the core cache built by
 * raylib-libretro-config.h.
 *
 * @param zipPath OS path to a .zip archive.
 * @param outExt  Caller-provided buffer (at least 32 bytes) that receives the
 *                lower-case extension (without the leading dot) on success.
 * @return \c true if a matching inner file was found and copied into @p outExt.
 */
static bool FindZipInnerExtensionForCore(const char* zipPath, char* outExt) {
#ifdef RAYLIB_LIBRETRO_CONFIG_H
    if (!IsPhysFSReady() && !InitLibretroPhysFS()) return false;
    // Mount at a scratch point to avoid clobbering any /game mount in flight.
    if (!MountPhysFS(zipPath, "/peek")) return false;

    FilePathList entries = LoadDirectoryFilesFromPhysFSEx("/peek", NULL, true);
    bool found = false;
    int coreCount = rlconfig_get_int(menu.cfg, LIBRETRO_CORE_CACHE_SECTION, "count", 0);

    for (unsigned int e = 0; e < entries.count && !found; e++) {
        const char* innerExt = GetFileExtension(entries.paths[e]);
        if (!innerExt || !innerExt[0]) continue;
        if (innerExt[0] == '.') innerExt++;
        char innerLower[32] = {0};
        TextCopy(innerLower, TextToLower(innerExt));

        for (int i = 0; i < coreCount; i++) {
            const char* exts = rlconfig_get(menu.cfg, LIBRETRO_CORE_CACHE_SECTION,
                                            TextFormat("core_%d_extensions", i));
            if (!exts) continue;
            int extCount = 0;
            char** extList = TextSplit(exts, '|', &extCount);
            for (int j = 0; j < extCount; j++) {
                if (TextIsEqual(extList[j], innerLower)) {
                    TextCopy(outExt, innerLower);
                    found = true;
                    break;
                }
            }
            if (found) break;
        }
    }

    UnloadDirectoryFiles(entries);
    UnmountPhysFS(zipPath);
    return found;
#else
    (void)zipPath;
    (void)outExt;
    return false;
#endif
}

static const char* FindCoreForGame(const char* gamePath) {
#ifdef RAYLIB_LIBRETRO_CONFIG_H
    if (!menu.cfg || !gamePath || !gamePath[0]) return NULL;

    const char* gameExt = GetFileExtension(gamePath);
    if (!gameExt || !gameExt[0]) return NULL;
    if (gameExt[0] == '.') gameExt++;

    char gameExtLower[32] = {0};
    TextCopy(gameExtLower, TextToLower(gameExt));

    // If the path is a .zip, look up the actual ROM extension from inside.
    if (TextIsEqual(gameExtLower, "zip")) {
        char zipInner[32] = {0};
        if (FindZipInnerExtensionForCore(gamePath, zipInner)) {
            TextCopy(gameExtLower, zipInner);
        }
    }

    int count = rlconfig_get_int(menu.cfg, LIBRETRO_CORE_CACHE_SECTION, "count", 0);
    for (int i = 0; i < count; i++) {
        char keyExts[64], keyPath[64];
        TextCopy(keyExts, TextFormat("core_%d_extensions", i));
        TextCopy(keyPath, TextFormat("core_%d_path", i));

        const char* extensions = rlconfig_get(menu.cfg, LIBRETRO_CORE_CACHE_SECTION, keyExts);
        if (!extensions) continue;

        int extCount = 0;
        char** extList = TextSplit(extensions, '|', &extCount);
        for (int j = 0; j < extCount; j++) {
            if (TextIsEqual(extList[j], gameExtLower)) {
                return rlconfig_get(menu.cfg, LIBRETRO_CORE_CACHE_SECTION, keyPath);
            }
        }
    }
#endif
    return NULL;
}

static bool MenuInitCore(const char* corePath) {
    LibretroApplyDirectories();
    if (!InitLibretro(corePath)) return false;
    LoadLibretroCoreOptions();
    SetLibretroVolume(menu.volumeSelected);
    menu.cheatIndex = 0;
    menu.cheatBuffer[0] = '\0';
    menu.cheatList[0] = '\0';
    return true;
}

/**
 * Load the battery save (SRAM) for the currently loaded game from disk.
 *
 * Constructs the save path as `<saveDir>/<contentName>.srm` and, if the file
 * exists, reads it into the core's SRAM region via @ref SetLibretroSRAMData.
 * Does nothing when no game is ready or the core has no SRAM region.
 *
 * @return true  if SRAM was found on disk and successfully applied.
 * @return false if no game is ready, no SRAM region exists, or the file is absent.
 */
static bool MenuLoadGameSRAM(void) {
    if (!IsLibretroGameReady()) return false;
    const char* sramPath = TextFormat("%s/%s.srm",
        GetLibretroDirectory(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY),
        GetLibretroContentName());
    if (!FileExists(sramPath)) return false;
    int fileSize = 0;
    unsigned char* fileData = LoadFileData(sramPath, &fileSize);
    if (fileData == NULL || fileSize == 0) {
        UnloadFileData(fileData);
        return false;
    }
    bool ok = SetLibretroSRAMData(fileData, (size_t)fileSize);
    UnloadFileData(fileData);
    if (ok) TraceLog(LOG_INFO, "MENU: SRAM loaded from %s", sramPath);
    return ok;
}

/**
 * Save the battery save (SRAM) for the currently loaded game to disk.
 *
 * Constructs the save path as `<saveDir>/<contentName>.srm` and writes the
 * core's SRAM region (obtained via @ref GetLibretroSRAMData) to that file.
 * Does nothing when no game is ready or the core has no SRAM region.
 *
 * @return true  if SRAM data exists and was written to disk successfully.
 * @return false if no game is ready, no SRAM region exists, or the write failed.
 */
static bool MenuSaveGameSRAM(void) {
    if (!IsLibretroGameReady()) return false;
    size_t sramSize = 0;
    void* sramData = GetLibretroSRAMData(&sramSize);
    if (sramData == NULL || sramSize == 0) return false;
    const char* sramPath = TextFormat("%s/%s.srm",
        GetLibretroDirectory(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY),
        GetLibretroContentName());
    bool ok = SaveFileData(sramPath, sramData, (unsigned int)sramSize);
    if (ok) {
        TraceLog(LOG_INFO, "MENU: SRAM saved to %s", sramPath);
        LibretroFlushPersistentStorage();
    }
    return ok;
}

static bool MenuLoadGame(const char* gamePath) {
    BeginDrawing();
        ClearBackground(BLACK);
        const char* loadingText = TextFormat("Loading %s", GetFileName(gamePath));
        Font font = IsFontValid(menu.font) ? menu.font : GetFontDefault();
        float fontSize = (float)font.baseSize;
        Vector2 textSize = MeasureTextEx(font, loadingText, fontSize, 1);
        DrawTextEx(font, loadingText,
            (Vector2){ (GetScreenWidth() - textSize.x) / 2, (GetScreenHeight() - textSize.y) / 2 },
            fontSize, 1, GRAY);
    EndDrawing();

    // Unload the current game if it's a thing.
    if (IsLibretroGameReady()) {
        UnloadLibretroGame();
    }

    // Detect a workable core.
    const char* corePath = FindCoreForGame(gamePath);
    if (!corePath) {
        SetLibretroMessage(TextFormat("No core found for %s", GetFileName(gamePath)), 2.0);
        return false;
    }

    // Load the core
    if (!MenuInitCore(corePath)) {
        SetLibretroMessage("Failed to load core", 2.0);
        return false;
    }

    // Load the game (PhysFS-aware so .zip archives Just Work).
    if (!LoadLibretroGameFromPhysFS(gamePath)) {
        if (IsLibretroGameRequired()) {
            SetLibretroMessage("Failed to load game", 2.0);
            return false;
        }
        // Core supports running without content; fall back to standalone.
        if (!LoadLibretroGame(NULL)) {
            SetLibretroMessage("Failed to load core", 2.0);
            return false;
        }
    }

    BuildLibretroMenuOptions(&menu);
    menu.active = false;
    MenuLoadGameSRAM();
    return true;
}

static void MenuGameFileChanged(nk_console* widget, void* user_data) {
    NK_UNUSED(widget);
    char* path = (char*)user_data;
    if (path && path[0]) {
        SaveLibretroAllSettings();
        CloseLibretro();
        menu.active = !MenuLoadGame(path);
        path[0] = '\0';
    }
}

static void LibretroMenuLoadStateClicked(nk_console* widget, void* user_data) {
    (void)widget;
    (void)user_data;
    if (!IsLibretroGameReady()) return;
    int dataSize;
    const char* savesDir = GetLibretroDirectory(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY);
    void* saveData = LoadFileData(TextFormat("%s/%s_%02d.sav", savesDir, GetLibretroContentName(), menu.saveSlotIndex + 1), &dataSize);
    if (saveData != NULL) {
        SetLibretroSerializedData(saveData, (unsigned int)dataSize);
        MemFree(saveData);
        // A save-state load is a one-shot wall-clock discontinuity; drop the
        // accumulator backlog so the next frame doesn't burst to catch up.
        ResetLibretroTiming();
        SetLibretroMessage(TextFormat("Slot %d Loaded", menu.saveSlotIndex + 1), 2.0);
        menu.active = false;
    }
    else {
        SetLibretroMessage("Load State failed", 2.0);
    }
}

LibretroMenu* InitLibretroMenu(void) {
    int screenWidth = GetScreenWidth();
    //int menuScale = (screenWidth >= 2560) ? 3 : (screenWidth >= 1280) ? 2 : 1;
    int fontSize = 13;// * menuScale;
    menu = (LibretroMenu){0};
    menu.themeSelectedIndex = LIBRETRO_MENU_STYLE_CATPPUCCIN_MOCHA;
    menu.volumeSelected = 1.0f;
    menu.vsync = nk_true;
    menu.fpsIndex = 0; // Auto
    menu.keyScreenshot  = (nk_rune)NK_KEY_F8;
    menu.keyRewind      = (nk_rune)'R';
    menu.keyMenu        = (nk_rune)NK_KEY_TEXT_RESET_MODE;
    menu.keySaveState   = (nk_rune)NK_KEY_F2;
    menu.keyLoadState   = (nk_rune)NK_KEY_F4;
    menu.keyPrevSlot    = (nk_rune)NK_KEY_NONE;
    menu.keyNextSlot    = (nk_rune)NK_KEY_NONE;
    menu.saveSlotIndex  = 0;
    menu.keyFullscreen  = (nk_rune)NK_KEY_F11;
    menu.keyPrevShader  = (nk_rune)NK_KEY_F9;
    menu.keyNextShader  = (nk_rune)NK_KEY_F10;
    menu.keyReset       = (nk_rune)NK_KEY_NONE;
    menu.keyQuit        = (nk_rune)NK_KEY_NONE;
    menu.keyVolumeUp    = (nk_rune)'=';
    menu.keyVolumeDown  = (nk_rune)'-';
    menu.keyMute        = (nk_rune)'M';
    menu.keyFastForward = (nk_rune)'F';
    menu.fastForwardSpeed = 3.0f;
    menu.keySlowMotion = (nk_rune)'G';
    menu.slowMotionSpeed = 0.5f;
    menu.keyboardP1[RETRO_DEVICE_ID_JOYPAD_B]      = (nk_rune)'Z';
    menu.keyboardP1[RETRO_DEVICE_ID_JOYPAD_Y]      = (nk_rune)'A';
    menu.keyboardP1[RETRO_DEVICE_ID_JOYPAD_SELECT] = (nk_rune)NK_KEY_SHIFT;
    menu.keyboardP1[RETRO_DEVICE_ID_JOYPAD_START]  = (nk_rune)NK_KEY_ENTER;
    menu.keyboardP1[RETRO_DEVICE_ID_JOYPAD_UP]     = (nk_rune)NK_KEY_UP;
    menu.keyboardP1[RETRO_DEVICE_ID_JOYPAD_DOWN]   = (nk_rune)NK_KEY_DOWN;
    menu.keyboardP1[RETRO_DEVICE_ID_JOYPAD_LEFT]   = (nk_rune)NK_KEY_LEFT;
    menu.keyboardP1[RETRO_DEVICE_ID_JOYPAD_RIGHT]  = (nk_rune)NK_KEY_RIGHT;
    menu.keyboardP1[RETRO_DEVICE_ID_JOYPAD_A]      = (nk_rune)'X';
    menu.keyboardP1[RETRO_DEVICE_ID_JOYPAD_X]      = (nk_rune)'S';
    menu.keyboardP1[RETRO_DEVICE_ID_JOYPAD_L]      = (nk_rune)'Q';
    menu.keyboardP1[RETRO_DEVICE_ID_JOYPAD_R]      = (nk_rune)'W';
    menu.keyboardP1[RETRO_DEVICE_ID_JOYPAD_L2]     = (nk_rune)'E';
    menu.keyboardP1[RETRO_DEVICE_ID_JOYPAD_R2]     = (nk_rune)'R';
    menu.keyboardP1[RETRO_DEVICE_ID_JOYPAD_L3]     = (nk_rune)'D';
    menu.keyboardP1[RETRO_DEVICE_ID_JOYPAD_R3]     = (nk_rune)'F';
    menu.font = LoadFontFromNuklear(fontSize);
    if (!IsFontValid(menu.font)) {
        return NULL;
    }

    menu.ctx = InitNuklearEx(menu.font, (float)fontSize);
    //menu.ctx = InitNuklear(32);
    if (!menu.ctx) {
        UnloadFont(menu.font);
        return NULL;
    }

    menu.console = nk_console_init(menu.ctx);
    if (!menu.console) {
        UnloadFont(menu.font);
        UnloadNuklear(menu.ctx);
        menu.ctx = NULL;
        return NULL;
    }

    nk_gamepad_init(&menu.gamepads, menu.ctx, (void*)&menu.console);
    nk_console_set_gamepads(menu.console, &menu.gamepads);

    // When trying to go back from the main menu, exit the menu.
    nk_console_add_event(menu.console, NK_CONSOLE_EVENT_BACK, &MenuResumeClicked);

#ifdef RAYLIB_LIBRETRO_CONFIG_H
    menu.cfg = rlconfig_load(FileExists(RAYLIB_LIBRETRO_CFG_FILE) ? RAYLIB_LIBRETRO_CFG_FILE : NULL);
#endif
    TextCopy(menu.coreDirectory, "cores");
    TextCopy(menu.saveDirectory, "saves");
    TextCopy(menu.systemDirectory, "system");
#ifdef __EMSCRIPTEN__
    // Default save/system directories point at the IDBFS mount so they
    // persist across page reloads. The user can still override via the
    // Settings menu; saved values take precedence on subsequent runs.
    TextCopy(menu.saveDirectory,   "/userdata/saves");
    TextCopy(menu.systemDirectory, "/userdata/system");
#endif
    LoadLibretroMenuSettings();
    LibretroApplyDirectories();
    ScanLibretroCoreDirectory();

    // Apply VSYNC/FPS even when no config was loaded, so the frame cap is always
    // in place (defends against a vsync hint the driver/compositor ignores).
    LibretroMenuApplyVideoSettings();

    // Build the Menu
    menu.resumeButton = nk_console_button_onclick(menu.console, "Resume", &MenuResumeClicked);
    nk_console_button_set_symbol(menu.resumeButton, NK_SYMBOL_TRIANGLE_RIGHT);

    // Save States
    {
        nk_console* saveStateRow = nk_console_row_begin(menu.console);
        // Save State
        menu.saveStateButton = nk_console_button(saveStateRow, "Save State");
        nk_console_add_event(menu.saveStateButton, NK_CONSOLE_EVENT_CLICKED, &LibretroMenuSaveStateClicked);
        nk_console_button_set_symbol(menu.saveStateButton, NK_SYMBOL_RECT_SOLID);

        // Load State
        menu.loadStateButton = nk_console_button(saveStateRow, "Load State");
        nk_console_add_event(menu.loadStateButton, NK_CONSOLE_EVENT_CLICKED, &LibretroMenuLoadStateClicked);
        nk_console_button_set_symbol(menu.loadStateButton, NK_SYMBOL_RECT_OUTLINE);
        nk_console_row_end(saveStateRow);
    }

    // Reset Game
    menu.resetGameButton = nk_console_button_onclick(menu.console, "Reset Game", &MenuResetGameClicked);
    nk_console_button_set_symbol(menu.resetGameButton, NK_SYMBOL_CIRCLE_SOLID);

    // Cheats
    {
        nk_console* cheatsMenu = nk_console_button(menu.console, "Cheats");
        menu.cheatsMenuButton = cheatsMenu;
        nk_console_button_set_symbol(
            nk_console_button_onclick(cheatsMenu, "Cheats", &nk_console_button_back),
            NK_SYMBOL_TRIANGLE_UP);

        // Add Cheat Code
        nk_console* cheatEntry = nk_console_textedit_action(cheatsMenu, "Add Cheat Code",
            menu.cheatBuffer, sizeof(menu.cheatBuffer));
        nk_console_add_event(cheatEntry, NK_CONSOLE_EVENT_CHANGED, &LibretroMenuCheatChanged);

        // Reset Cheats
        nk_console* resetCheatsButton = nk_console_button_onclick(cheatsMenu, "Reset Cheats",
            &LibretroMenuResetCheatsClicked);
        nk_console_button_set_symbol(resetCheatsButton, NK_SYMBOL_X);

        // The list of active cheats.
        nk_console_label(cheatsMenu, menu.cheatList);
    }

    // Load Game
    menu.loadGameWidget = nk_console_file_action(menu.console, "Load Game", menu.loadGamePath, RAYLIB_LIBRETRO_VFS_MAX_PATH);
    nk_console_add_event_handler(menu.loadGameWidget, NK_CONSOLE_EVENT_CHANGED, &MenuGameFileChanged, menu.loadGamePath, NULL);
    if (menu.fileBrowserStartDirectory[0] != '\0') {
        nk_console_file_set_directory(menu.loadGameWidget, menu.fileBrowserStartDirectory);
    }

    // Close Game
    //menu.closeGameButton = nk_console_button_onclick(menu.console, "Close Game", &MenuCloseGameClicked);
    //nk_console_button_set_symbol(menu.resumeButton, NK_SYMBOL_X);

    // Settings
    nk_console* settings = nk_console_button(menu.console, "Settings");
    nk_console_add_event(settings, NK_CONSOLE_EVENT_BACK, &MenuCommitSettings);
    {
        // Back
        nk_console_button_set_symbol(
            nk_console_button_onclick(settings, "Settings", &nk_console_button_back),
            NK_SYMBOL_TRIANGLE_UP);

        // Audio & Video
        {
            nk_console* graphicsMenu = nk_console_button(settings, "Audio & Video");
            nk_console_add_event(graphicsMenu, NK_CONSOLE_EVENT_BACK, &MenuCommitSettings);
            nk_console_button_set_symbol(
                nk_console_button_onclick(graphicsMenu, "Audio & Video", &nk_console_button_back),
                NK_SYMBOL_TRIANGLE_UP);

            nk_console* fullscreenCheckbox = nk_console_checkbox(graphicsMenu, "Fullscreen", &menu.fullscreen);
            nk_console_add_event(fullscreenCheckbox, NK_CONSOLE_EVENT_CHANGED, LibretroMenuFullscreenChanged);

            nk_console* vsyncCheckbox = nk_console_checkbox(graphicsMenu, "VSYNC", &menu.vsync);
            nk_console_add_event(vsyncCheckbox, NK_CONSOLE_EVENT_CHANGED, LibretroMenuVideoChanged);

            nk_console* fpsCombo = nk_console_combobox(graphicsMenu, "FPS", "Auto|30|60|120|144|240|Unlimited", '|', &menu.fpsIndex);
            nk_console_add_event_handler(fpsCombo, NK_CONSOLE_EVENT_CHANGED, &LibretroMenuVideoChanged, NULL, NULL);

            static char shaderNames[256] = {0};
            if (shaderNames[0] == '\0') {
                int offset = 0;
                for (int i = 0; i < LIBRETRO_SHADER_TYPE_COUNT; ++i) {
                    const char* name = GetLibretroShaderName((LibretroShaderType)i);
                    int len = (int)strlen(name);
                    if (offset + len + 1 < (int)sizeof(shaderNames)) {
                        if (i > 0) shaderNames[offset++] = '|';
                        TextCopy(shaderNames + offset, name);
                        offset += len;
                    }
                }
            }
            menu.shaderSelectedIndex = (int)GetActiveLibretroShaderType();
            nk_console* shaderCombo = nk_console_combobox(graphicsMenu, "Shader", shaderNames, '|', &menu.shaderSelectedIndex);
            nk_console_add_event_handler(shaderCombo, NK_CONSOLE_EVENT_CHANGED, &LibretroMenuSettingChanged, NULL, NULL);

            nk_console* textureFilter = nk_console_combobox(graphicsMenu, "Texture Filter", "None|Bilinear|Trilinear|Anisotropic 4x|Anisotropic 8x|Anisotropic 16x", '|', &menu.textureFilterIndex);
            nk_console_add_event_handler(textureFilter, NK_CONSOLE_EVENT_CHANGED, &LibretroMenuSettingChanged, NULL, NULL);

            nk_console* themeCombo = nk_console_combobox(graphicsMenu, "Theme", "Mocha|Latte|Frappe|Macchiato|Dracula|Dark", '|', &menu.themeSelectedIndex);
            nk_console_add_event_handler(themeCombo, NK_CONSOLE_EVENT_CHANGED, &LibretroMenuSettingChanged, NULL, NULL);
            SetLibretroMenuStyle((LibretroMenuStyle)menu.themeSelectedIndex);

            // Volume
            nk_console* volume = nk_console_slider_float(graphicsMenu, "Volume", 0.0f, &menu.volumeSelected, 1.0f, RAYLIB_LIBRETRO_MENU_SLIDER_STEP(0.0f, 1.0f));
            nk_console_add_event_handler(volume, NK_CONSOLE_EVENT_CHANGED, &LibretroMenuSettingChanged, NULL, NULL);
        }

        // Gameplay
        {
            nk_console* gameplayMenu = nk_console_button(settings, "Gameplay");
            nk_console_add_event(gameplayMenu, NK_CONSOLE_EVENT_BACK, &MenuCommitSettings);
            nk_console_button_set_symbol(
                nk_console_button_onclick(gameplayMenu, "Gameplay", &nk_console_button_back),
                NK_SYMBOL_TRIANGLE_UP);

            nk_console_slider_float(gameplayMenu, "Fast Forward Speed", 1.1f, &menu.fastForwardSpeed, 10.0f, RAYLIB_LIBRETRO_MENU_SLIDER_STEP(1.1f, 10.0f));
            nk_console_slider_float(gameplayMenu, "Slow Motion Speed", 0.1f, &menu.slowMotionSpeed, 0.9f, RAYLIB_LIBRETRO_MENU_SLIDER_STEP(0.1f, 0.9f));
            nk_console_checkbox(gameplayMenu, "Touch Controls", &menu.touchControls);
            #if defined(PLATFORM_WEB)
            nk_console_checkbox(gameplayMenu, "Touch Haptics", &menu.touchHapticsEnabled);
            #endif
            nk_console_checkbox(gameplayMenu, "Rewind", &menu.rewindEnabled);
            nk_console* analogCombo = nk_console_combobox(gameplayMenu, "Analog to D-Pad",
                "None|Left Analog|Right Analog", '|', &menu.analogToDpadIndex);
            analogCombo->tooltip = "Map an analog stick to D-Pad inputs";
            nk_console_combobox(gameplayMenu, "Save Slot",
                "Slot 1|Slot 2|Slot 3|Slot 4|Slot 5|Slot 6|Slot 7|Slot 8|Slot 9|Slot 10",
                '|', &menu.saveSlotIndex);
            nk_console_textedit(gameplayMenu, "Username", LIBRETRO.username, 128);
        }

        // Keys
        {
            nk_console* keysMenu = nk_console_button(settings, "Keys");
            nk_console_add_event(keysMenu, NK_CONSOLE_EVENT_BACK, &MenuCommitSettings);
            nk_console_button_set_symbol(
                nk_console_button_onclick(keysMenu, "Keys", &nk_console_button_back),
                NK_SYMBOL_TRIANGLE_UP);
            nk_console_key(keysMenu, "Screenshot", &menu.keyScreenshot);
            nk_console_key(keysMenu, "Rewind", &menu.keyRewind);
            nk_console_key(keysMenu, "Menu", &menu.keyMenu);
            nk_console_key(keysMenu, "Save State", &menu.keySaveState);
            nk_console_key(keysMenu, "Load State", &menu.keyLoadState);
            nk_console_key(keysMenu, "Prev Slot", &menu.keyPrevSlot);
            nk_console_key(keysMenu, "Next Slot", &menu.keyNextSlot);
            nk_console_key(keysMenu, "Fullscreen", &menu.keyFullscreen);
            nk_console_key(keysMenu, "Previous Shader", &menu.keyPrevShader);
            nk_console_key(keysMenu, "Next Shader", &menu.keyNextShader);
            nk_console_key(keysMenu, "Reset", &menu.keyReset);
            nk_console_key(keysMenu, "Quit", &menu.keyQuit);
            nk_console_key(keysMenu, "Volume Up", &menu.keyVolumeUp);
            nk_console_key(keysMenu, "Volume Down", &menu.keyVolumeDown);
            nk_console_key(keysMenu, "Mute", &menu.keyMute);
            nk_console_key(keysMenu, "Fast Forward", &menu.keyFastForward);
            nk_console_key(keysMenu, "Slow Motion", &menu.keySlowMotion);
        }

        // Keyboard Controls (Player 1)
        {
            nk_console* kbMenu = nk_console_button(settings, "Keyboard Controls");
            nk_console_add_event(kbMenu, NK_CONSOLE_EVENT_BACK, &MenuCommitSettings);
            nk_console_button_set_symbol(
                nk_console_button_onclick(kbMenu, "Keyboard Controls", &nk_console_button_back),
                NK_SYMBOL_TRIANGLE_UP);
            nk_console_key(kbMenu, "B",      &menu.keyboardP1[RETRO_DEVICE_ID_JOYPAD_B]);
            nk_console_key(kbMenu, "Y",      &menu.keyboardP1[RETRO_DEVICE_ID_JOYPAD_Y]);
            nk_console_key(kbMenu, "Select", &menu.keyboardP1[RETRO_DEVICE_ID_JOYPAD_SELECT]);
            nk_console_key(kbMenu, "Start",  &menu.keyboardP1[RETRO_DEVICE_ID_JOYPAD_START]);
            nk_console_key(kbMenu, "Up",     &menu.keyboardP1[RETRO_DEVICE_ID_JOYPAD_UP]);
            nk_console_key(kbMenu, "Down",   &menu.keyboardP1[RETRO_DEVICE_ID_JOYPAD_DOWN]);
            nk_console_key(kbMenu, "Left",   &menu.keyboardP1[RETRO_DEVICE_ID_JOYPAD_LEFT]);
            nk_console_key(kbMenu, "Right",  &menu.keyboardP1[RETRO_DEVICE_ID_JOYPAD_RIGHT]);
            nk_console_key(kbMenu, "A",      &menu.keyboardP1[RETRO_DEVICE_ID_JOYPAD_A]);
            nk_console_key(kbMenu, "X",      &menu.keyboardP1[RETRO_DEVICE_ID_JOYPAD_X]);
            nk_console_key(kbMenu, "L",      &menu.keyboardP1[RETRO_DEVICE_ID_JOYPAD_L]);
            nk_console_key(kbMenu, "R",      &menu.keyboardP1[RETRO_DEVICE_ID_JOYPAD_R]);
            nk_console_key(kbMenu, "L2",     &menu.keyboardP1[RETRO_DEVICE_ID_JOYPAD_L2]);
            nk_console_key(kbMenu, "R2",     &menu.keyboardP1[RETRO_DEVICE_ID_JOYPAD_R2]);
            nk_console_key(kbMenu, "L3",     &menu.keyboardP1[RETRO_DEVICE_ID_JOYPAD_L3]);
            nk_console_key(kbMenu, "R3",     &menu.keyboardP1[RETRO_DEVICE_ID_JOYPAD_R3]);
        }

        // Directories
        {
            nk_console* dirMenu = nk_console_button(settings, "Directories");
            nk_console_add_event(dirMenu, NK_CONSOLE_EVENT_BACK, &MenuCommitSettings);
            nk_console_button_set_symbol(
                nk_console_button_onclick(dirMenu, "Directories", &nk_console_button_back),
                NK_SYMBOL_TRIANGLE_UP);

            nk_console* coreDirectory = nk_console_dir(dirMenu, "Cores", menu.coreDirectory, RAYLIB_LIBRETRO_VFS_MAX_PATH);
            nk_console_add_event_handler(coreDirectory, NK_CONSOLE_EVENT_CHANGED, &MenuCoreDirChanged, NULL, NULL);

            nk_console* saveDirectory = nk_console_dir(dirMenu, "Saves", menu.saveDirectory, RAYLIB_LIBRETRO_VFS_MAX_PATH);
            nk_console_add_event_handler(saveDirectory, NK_CONSOLE_EVENT_CHANGED, &MenuDirChanged, NULL, NULL);

            nk_console* coreAssetsDirectory = nk_console_dir(dirMenu, "Assets", menu.coreAssetsDirectory, RAYLIB_LIBRETRO_VFS_MAX_PATH);
            nk_console_add_event_handler(coreAssetsDirectory, NK_CONSOLE_EVENT_CHANGED, &MenuDirChanged, NULL, NULL);

            nk_console* systemDirectory = nk_console_dir(dirMenu, "System", menu.systemDirectory, RAYLIB_LIBRETRO_VFS_MAX_PATH);
            nk_console_add_event_handler(systemDirectory, NK_CONSOLE_EVENT_CHANGED, &MenuDirChanged, NULL, NULL);

            nk_console* playlistsDirectory = nk_console_dir(dirMenu, "Playlists", menu.playlistsDirectory, RAYLIB_LIBRETRO_VFS_MAX_PATH);
            nk_console_add_event_handler(playlistsDirectory, NK_CONSOLE_EVENT_CHANGED, &MenuDirChanged, NULL, NULL);

            nk_console* fileBrowserStartDirectory = nk_console_dir(dirMenu, "Content", menu.fileBrowserStartDirectory, RAYLIB_LIBRETRO_VFS_MAX_PATH);
            nk_console_add_event_handler(fileBrowserStartDirectory, NK_CONSOLE_EVENT_CHANGED, &MenuContentDirChanged, NULL, NULL);
        }

        // Core Options (nested inside Settings)
        menu.optionsMenu = nk_console_button(settings, "Core Options");
        nk_console_add_event(menu.optionsMenu, NK_CONSOLE_EVENT_BACK, &MenuCommitSettings);
        {
            nk_console_button_set_symbol(
                nk_console_button_onclick(menu.optionsMenu, "Core Options", &nk_console_button_back),
                NK_SYMBOL_TRIANGLE_UP);
        }
    }

    // Quit
#ifndef __EMSCRIPTEN__
    nk_console* quitButton = nk_console_button(menu.console, "Quit");
    nk_console_add_event(quitButton, NK_CONSOLE_EVENT_CLICKED, &LibretroMenuQuitClicked);
    nk_console_button_set_symbol(quitButton, NK_SYMBOL_X);
#endif

    menu.active = true;
    return &menu;
}

// Extract the Nth pipe-delimited token from str into out[outSize].
static void LibretroMenuGetNthToken(const char* str, int n, char* out, int outSize) {
    int idx = 0;
    const char* p = str;
    while (p && *p) {
        const char* pipe = p;
        while (*pipe && *pipe != '|') pipe++;
        if (idx == n) {
            int len = (int)(pipe - p);
            if (len < 0) len = 0;
            if (len >= outSize) len = outSize - 1;
            if (len > 0) memcpy(out, p, (size_t)len);
            out[len] = '\0';
            return;
        }
        if (!*pipe) break;
        p = pipe + 1;
        idx++;
    }
    if (out && outSize > 0) out[0] = '\0';
}

// Called when a core option combobox selection changes.
// user_data is the option index cast to (void*).
static void LibretroMenuOptionChanged(nk_console* widget, void* user_data) {
    (void)widget;
    unsigned i = (unsigned)(uintptr_t)user_data;
    char value[LIBRETRO_CORE_VARIABLE_VALUE_LEN] = {0};
    LibretroMenuGetNthToken(LIBRETRO.core.variableValuesList[i],
                            menu.optionSelectedIndices[i],
                            value, LIBRETRO_CORE_VARIABLE_VALUE_LEN);
    if (TextLength(value) > 0) {
        SetLibretroCoreOption(LIBRETRO.core.variableKeys[i], value);
    }
}

// Called when a core option enabled/disabled checkbox changes.
// user_data is the option index cast to (void*).
static void LibretroMenuOptionCheckboxChanged(nk_console* widget, void* user_data) {
    (void)widget;
    unsigned i = (unsigned)(uintptr_t)user_data;
    const char* value = menu.optionCheckboxValues[i] ? "enabled" : "disabled";
    SetLibretroCoreOption(LIBRETRO.core.variableKeys[i], value);
}

static int LibretroMenuFindTokenIndex(const char* str, const char* value) {
    int idx = 0;
    const char* p = str;
    while (p && *p) {
        const char* pipe = p;
        while (*pipe && *pipe != '|') pipe++;
        int len = (int)(pipe - p);
        char tok[LIBRETRO_CORE_VARIABLE_VALUE_LEN] = {0};
        if (len < LIBRETRO_CORE_VARIABLE_VALUE_LEN) memcpy(tok, p, (size_t)len);
        if (TextIsEqual(tok, value)) return idx;
        if (!*pipe) break;
        p = pipe + 1;
        idx++;
    }
    return 0;
}

// Returns true if the only values are "enabled" and "disabled" (in either order).
static bool LibretroMenuIsEnabledDisabledOption(const char* valuesList) {
    if (!valuesList || !*valuesList) return false;
    return TextIsEqual(valuesList, "enabled|disabled") ||
           TextIsEqual(valuesList, "disabled|enabled") ||
           TextIsEqual(valuesList, "off|on");
}

void BuildLibretroMenuOptions(LibretroMenu* m) {
    if (!m || !m->optionsMenu) return;

    // The menu now reflects the current option set; clear the dirty flag so
    // the lazy-rebuild trigger in UpdateLibretroMenu doesn't fire redundantly.
    LIBRETRO.core.variablesVisibilityDirty = false;

    // Clear any previously built option children and restore Back button
    nk_console_free_children(m->optionsMenu);

    if (LIBRETRO.core.variableCount == 0) {
        m->optionsMenu->visible = nk_false;
        return;
    }

    // Count how many options will actually be shown. If none are visible
    // (e.g. all hidden by options_update_display_callback), hide the button
    // rather than showing an empty submenu.
    int shownCount = 0;
    for (unsigned i = 0; i < LIBRETRO.core.variableCount; i++) {
        if (TextLength(LIBRETRO.core.variableValuesList[i]) > 0 && LIBRETRO.core.variableVisible[i]) {
            shownCount++;
        }
    }
    if (shownCount == 0) {
        m->optionsMenu->visible = nk_false;
        return;
    }

    m->optionsMenu->visible = nk_true;

    nk_console_button_set_symbol(
        nk_console_button_onclick(m->optionsMenu, "Core Options", &nk_console_button_back),
        NK_SYMBOL_TRIANGLE_UP);

    for (unsigned i = 0; i < LIBRETRO.core.variableCount; i++) {
        if (TextLength(LIBRETRO.core.variableValuesList[i]) == 0) continue;
        if (!LIBRETRO.core.variableVisible[i]) continue;

        const char* label = TextLength(LIBRETRO.core.variableLabels[i]) > 0
            ? LIBRETRO.core.variableLabels[i]
            : LIBRETRO.core.variableKeys[i];

        nk_console* widget;

        if (LibretroMenuIsEnabledDisabledOption(LIBRETRO.core.variableValuesList[i])) {
            m->optionCheckboxValues[i] = (nk_bool)TextIsEqual(LIBRETRO.core.variableValues[i], "enabled");
            widget = nk_console_checkbox(m->optionsMenu, label, &m->optionCheckboxValues[i]);
            nk_console_add_event_handler(widget, NK_CONSOLE_EVENT_CHANGED,
                                         LibretroMenuOptionCheckboxChanged,
                                         (void*)(uintptr_t)i, NULL);
        } else {
            m->optionSelectedIndices[i] = LibretroMenuFindTokenIndex(
                LIBRETRO.core.variableValuesList[i], LIBRETRO.core.variableValues[i]);

            const char* displayStr = TextLength(LIBRETRO.core.variableDisplayList[i]) > 0
                ? LIBRETRO.core.variableDisplayList[i]
                : LIBRETRO.core.variableValuesList[i];

            widget = nk_console_combobox(m->optionsMenu, label,
                                         displayStr, '|',
                                         &m->optionSelectedIndices[i]);
            nk_console_add_event_handler(widget, NK_CONSOLE_EVENT_CHANGED,
                                         LibretroMenuOptionChanged,
                                         (void*)(uintptr_t)i, NULL);
        }

        if (TextLength(LIBRETRO.core.variableTooltips[i]) > 0) {
            widget->tooltip = LIBRETRO.core.variableTooltips[i];
        }
    }
}

static void LibretroMenuUpdateConfig(void) {
#ifdef RAYLIB_LIBRETRO_CONFIG_H
    if (!menu.cfg) return;
    rlconfig_set_int(menu.cfg, "raylib-libretro", "fullscreen", (int)menu.fullscreen);
    rlconfig_set_int(menu.cfg, "raylib-libretro", "vsync", (int)menu.vsync);
    rlconfig_set_int(menu.cfg, "raylib-libretro", "fps", menu.fpsIndex);
    rlconfig_set_int(menu.cfg, "raylib-libretro", "shader", menu.shaderSelectedIndex);
    rlconfig_set_int(menu.cfg, "raylib-libretro", "textureFilter", menu.textureFilterIndex);
    rlconfig_set_int(menu.cfg, "raylib-libretro", "theme", menu.themeSelectedIndex);
    rlconfig_set_float(menu.cfg, "raylib-libretro", "volume", menu.volumeSelected);
    rlconfig_set_int(menu.cfg, "raylib-libretro", "rewind", menu.rewindEnabled ? 1 : 0);
    rlconfig_set_int(menu.cfg, "raylib-libretro", "analogToDpad", menu.analogToDpadIndex);
    rlconfig_set_int(menu.cfg, "raylib-libretro", "touchControls", menu.touchControls ? 1 : 0);
    rlconfig_set_int(menu.cfg, "raylib-libretro", "touchHaptics", menu.touchHapticsEnabled ? 1 : 0);
    rlconfig_set_int(menu.cfg, "raylib-libretro", "keyScreenshot", (int)menu.keyScreenshot);
    rlconfig_set_int(menu.cfg, "raylib-libretro", "keyRewind", (int)menu.keyRewind);
    rlconfig_set_int(menu.cfg, "raylib-libretro", "keyMenu", (int)menu.keyMenu);
    rlconfig_set_int(menu.cfg, "raylib-libretro", "keySaveState", (int)menu.keySaveState);
    rlconfig_set_int(menu.cfg, "raylib-libretro", "keyLoadState", (int)menu.keyLoadState);
    rlconfig_set_int(menu.cfg, "raylib-libretro", "keyPrevSlot",  (int)menu.keyPrevSlot);
    rlconfig_set_int(menu.cfg, "raylib-libretro", "keyNextSlot",  (int)menu.keyNextSlot);
    rlconfig_set_int(menu.cfg, "raylib-libretro", "saveSlot", menu.saveSlotIndex);
    rlconfig_set(menu.cfg, "raylib-libretro", "username", LIBRETRO.username);
    rlconfig_set_int(menu.cfg, "raylib-libretro", "keyFullscreen", (int)menu.keyFullscreen);
    rlconfig_set_int(menu.cfg, "raylib-libretro", "keyPrevShader", (int)menu.keyPrevShader);
    rlconfig_set_int(menu.cfg, "raylib-libretro", "keyNextShader",  (int)menu.keyNextShader);
    rlconfig_set_int(menu.cfg, "raylib-libretro", "keyReset",       (int)menu.keyReset);
    rlconfig_set_int(menu.cfg, "raylib-libretro", "keyQuit",       (int)menu.keyQuit);
    rlconfig_set_int(menu.cfg, "raylib-libretro", "keyVolumeUp",    (int)menu.keyVolumeUp);
    rlconfig_set_int(menu.cfg, "raylib-libretro", "keyVolumeDown",  (int)menu.keyVolumeDown);
    rlconfig_set_int(menu.cfg, "raylib-libretro", "keyMute",        (int)menu.keyMute);
    rlconfig_set_int(menu.cfg, "raylib-libretro", "keyFastForward", (int)menu.keyFastForward);
    rlconfig_set_float(menu.cfg, "raylib-libretro", "fastForwardSpeed", menu.fastForwardSpeed);
    rlconfig_set_int(menu.cfg, "raylib-libretro", "keySlowMotion", (int)menu.keySlowMotion);
    rlconfig_set_float(menu.cfg, "raylib-libretro", "slowMotionSpeed", menu.slowMotionSpeed);
    rlconfig_set(menu.cfg, "raylib-libretro", "coreDirectory", LibretroResolveAbsoluteDirectory(menu.coreDirectory));
    rlconfig_set(menu.cfg, "raylib-libretro", "saveDirectory", LibretroResolveAbsoluteDirectory(menu.saveDirectory));
    rlconfig_set(menu.cfg, "raylib-libretro", "coreAssetsDirectory", LibretroResolveAbsoluteDirectory(menu.coreAssetsDirectory));
    rlconfig_set(menu.cfg, "raylib-libretro", "systemDirectory", LibretroResolveAbsoluteDirectory(menu.systemDirectory));
    rlconfig_set(menu.cfg, "raylib-libretro", "playlistsDirectory", LibretroResolveAbsoluteDirectory(menu.playlistsDirectory));
    rlconfig_set(menu.cfg, "raylib-libretro", "fileBrowserStartDirectory", LibretroResolveAbsoluteDirectory(menu.fileBrowserStartDirectory));
    for (int i = 0; i < 16; i++) {
        rlconfig_set_int(menu.cfg, "raylib-libretro", TextFormat("keyP1[%d]", i), (int)menu.keyboardP1[i]);
    }
#endif
}

static void LibretroMenuApplyKeyboardPlayer1(void) {
    for (int i = 0; i < 16; i++) {
        LIBRETRO.keyboardPlayer1[i] = (int)NuklearKeyToKeyboardKey(menu.keyboardP1[i]);
    }
}

static bool SaveLibretroMenuSettings(void) {
#ifdef RAYLIB_LIBRETRO_CONFIG_H
    if (!menu.cfg) return false;
    LibretroMenuUpdateConfig();
    bool ok = rlconfig_save(menu.cfg, RAYLIB_LIBRETRO_CFG_FILE);
    TraceLog(LOG_INFO, "MENU: Saved menu settings to %s", RAYLIB_LIBRETRO_CFG_FILE);
    return ok;
#else
    return false;
#endif
}

static bool SaveLibretroCoreOptions(void) {
#ifdef RAYLIB_LIBRETRO_CONFIG_H
    if (!menu.cfg || LIBRETRO.core.variableCount == 0) return false;
    const char *coreName = LIBRETRO.core.libraryName;
    if (!coreName || !coreName[0]) return false;
    rlconfig_clear_section(menu.cfg, coreName);
    for (unsigned i = 0; i < LIBRETRO.core.variableCount; i++) {
        rlconfig_set(menu.cfg, coreName, LIBRETRO.core.variableKeys[i], LIBRETRO.core.variableValues[i]);
    }
    bool ok = rlconfig_save(menu.cfg, RAYLIB_LIBRETRO_CFG_FILE);
    TraceLog(LOG_INFO, "LIBRETRO: Saved core options to %s", RAYLIB_LIBRETRO_CFG_FILE);
    return ok;
#else
    return false;
#endif
}

bool LoadLibretroCoreOptions(void) {
#ifdef RAYLIB_LIBRETRO_CONFIG_H
    if (!menu.cfg) return false;
    const char *coreName = LIBRETRO.core.libraryName;
    if (!coreName || !coreName[0]) return false;
    int loaded = 0;
    for (unsigned i = 0; i < LIBRETRO.core.variableCount; i++) {
        const char *val = rlconfig_get(menu.cfg, coreName, LIBRETRO.core.variableKeys[i]);
        if (val && SetLibretroCoreOption(LIBRETRO.core.variableKeys[i], val)) loaded++;
    }
    TraceLog(LOG_INFO, "LIBRETRO: Loaded %d core option(s) from %s", loaded, RAYLIB_LIBRETRO_CFG_FILE);
    return loaded > 0;
#else
    return false;
#endif
}

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE // expose to JS as Module._SaveLibretroAllSettings
#endif
bool SaveLibretroAllSettings(void) {
#ifdef RAYLIB_LIBRETRO_CONFIG_H
    if (!menu.cfg) return false;
    LibretroMenuUpdateConfig();
    const char *coreName = LIBRETRO.core.libraryName;
    if (coreName && coreName[0] && LIBRETRO.core.variableCount > 0) {
        rlconfig_clear_section(menu.cfg, coreName);
        for (unsigned i = 0; i < LIBRETRO.core.variableCount; i++) {
            rlconfig_set(menu.cfg, coreName, LIBRETRO.core.variableKeys[i], LIBRETRO.core.variableValues[i]);
        }
    }
    bool ok = rlconfig_save(menu.cfg, RAYLIB_LIBRETRO_CFG_FILE);
    TraceLog(LOG_INFO, "MENU: Saved all settings to %s", RAYLIB_LIBRETRO_CFG_FILE);
    MenuSaveGameSRAM();
    LibretroFlushPersistentStorage();
    return ok;
#else
    return false;
#endif
}

static bool LoadLibretroMenuSettings(void) {
#ifdef RAYLIB_LIBRETRO_CONFIG_H
    if (!menu.cfg) return false;

    nk_bool savedFullscreen = (nk_bool)rlconfig_get_int(menu.cfg, "raylib-libretro", "fullscreen", (int)menu.fullscreen);
    menu.fullscreen = savedFullscreen;
#ifdef __EMSCRIPTEN__
    // Queue a deferred fullscreen request so the saved preference is applied on
    // the first user interaction after page load (the only timing browsers allow).
    if (savedFullscreen) {
        emscripten_request_fullscreen("#canvas", 1);
    }
#else
    if (savedFullscreen != (nk_bool)IsWindowFullscreen()) ToggleFullscreen();
#endif

    menu.vsync = (nk_bool)rlconfig_get_int(menu.cfg, "raylib-libretro", "vsync", (int)menu.vsync);
    menu.fpsIndex = rlconfig_get_int(menu.cfg, "raylib-libretro", "fps", menu.fpsIndex);
    if (menu.fpsIndex < 0 || menu.fpsIndex > 6) menu.fpsIndex = 0;
    LibretroMenuApplyVideoSettings();

    menu.shaderSelectedIndex = rlconfig_get_int(menu.cfg, "raylib-libretro", "shader", LIBRETRO_SHADER_NONE);
    if (menu.shaderSelectedIndex < 0 || menu.shaderSelectedIndex >= LIBRETRO_SHADER_TYPE_COUNT) {
        menu.shaderSelectedIndex = LIBRETRO_SHADER_NONE;
    }
    SetActiveLibretroShader((LibretroShaderType)menu.shaderSelectedIndex);

    menu.textureFilterIndex = rlconfig_get_int(menu.cfg, "raylib-libretro", "textureFilter", 0);
    if (menu.textureFilterIndex < 0 || menu.textureFilterIndex > TEXTURE_FILTER_ANISOTROPIC_16X)
        menu.textureFilterIndex = 0;
    LIBRETRO.textureFilter = menu.textureFilterIndex;

    menu.themeSelectedIndex = rlconfig_get_int(menu.cfg, "raylib-libretro", "theme", LIBRETRO_MENU_STYLE_DRACULA);
    if (menu.themeSelectedIndex < 0 || menu.themeSelectedIndex >= LIBRETRO_MENU_STYLE_COUNT)
        menu.themeSelectedIndex = LIBRETRO_MENU_STYLE_DRACULA;
    SetLibretroMenuStyle((LibretroMenuStyle)menu.themeSelectedIndex);

    menu.volumeSelected = rlconfig_get_float(menu.cfg, "raylib-libretro", "volume", menu.volumeSelected);
    // Migrate the legacy 0..100 integer storage to the 0.0..1.0 float scale.
    if (menu.volumeSelected > 1.0f) menu.volumeSelected /= 100.0f;
    if (menu.volumeSelected < 0.0f) menu.volumeSelected = 0.0f;
    if (menu.volumeSelected > 1.0f) menu.volumeSelected = 1.0f;
    SetLibretroVolume(menu.volumeSelected);

    menu.rewindEnabled = rlconfig_get_int(menu.cfg, "raylib-libretro", "rewind", 0) > 0;
    menu.analogToDpadIndex = rlconfig_get_int(menu.cfg, "raylib-libretro", "analogToDpad", 0);
    if (menu.analogToDpadIndex < 0 || menu.analogToDpadIndex > 2) menu.analogToDpadIndex = 0;
    LIBRETRO.analogToDpadIndex = menu.analogToDpadIndex;
#if defined(PLATFORM_WEB)
    menu.touchControls = rlconfig_get_int(menu.cfg, "raylib-libretro", "touchControls", 1) > 0;
#else
    menu.touchControls = rlconfig_get_int(menu.cfg, "raylib-libretro", "touchControls", 0) > 0;
#endif
    menu.touchHapticsEnabled = rlconfig_get_int(menu.cfg, "raylib-libretro", "touchHaptics", 1) > 0;

    menu.keyScreenshot = (nk_rune)rlconfig_get_int(menu.cfg, "raylib-libretro", "keyScreenshot", (int)menu.keyScreenshot);
    menu.keyRewind     = (nk_rune)rlconfig_get_int(menu.cfg, "raylib-libretro", "keyRewind",     (int)menu.keyRewind);
    menu.keyMenu       = (nk_rune)rlconfig_get_int(menu.cfg, "raylib-libretro", "keyMenu",       (int)menu.keyMenu);
    menu.keySaveState  = (nk_rune)rlconfig_get_int(menu.cfg, "raylib-libretro", "keySaveState",  (int)menu.keySaveState);
    menu.keyLoadState  = (nk_rune)rlconfig_get_int(menu.cfg, "raylib-libretro", "keyLoadState",  (int)menu.keyLoadState);
    menu.keyPrevSlot   = (nk_rune)rlconfig_get_int(menu.cfg, "raylib-libretro", "keyPrevSlot",   (int)menu.keyPrevSlot);
    menu.keyNextSlot   = (nk_rune)rlconfig_get_int(menu.cfg, "raylib-libretro", "keyNextSlot",   (int)menu.keyNextSlot);
    menu.saveSlotIndex = rlconfig_get_int(menu.cfg, "raylib-libretro", "saveSlot", 0);
    if (menu.saveSlotIndex < 0 || menu.saveSlotIndex > 9) menu.saveSlotIndex = 0;
    const char* username = rlconfig_get(menu.cfg, "raylib-libretro", "username");
    if (username) TextCopy(LIBRETRO.username, username);
    menu.keyFullscreen = (nk_rune)rlconfig_get_int(menu.cfg, "raylib-libretro", "keyFullscreen", (int)menu.keyFullscreen);
    menu.keyPrevShader = (nk_rune)rlconfig_get_int(menu.cfg, "raylib-libretro", "keyPrevShader", (int)menu.keyPrevShader);
    menu.keyNextShader  = (nk_rune)rlconfig_get_int(menu.cfg, "raylib-libretro", "keyNextShader",  (int)menu.keyNextShader);
    menu.keyReset       = (nk_rune)rlconfig_get_int(menu.cfg, "raylib-libretro", "keyReset",       (int)menu.keyReset);
    menu.keyQuit       = (nk_rune)rlconfig_get_int(menu.cfg, "raylib-libretro", "keyQuit",       (int)menu.keyQuit);
    SetExitKey(NuklearKeyToKeyboardKey(menu.keyQuit));
    menu.keyVolumeUp    = (nk_rune)rlconfig_get_int(menu.cfg, "raylib-libretro", "keyVolumeUp",    (int)menu.keyVolumeUp);
    menu.keyVolumeDown  = (nk_rune)rlconfig_get_int(menu.cfg, "raylib-libretro", "keyVolumeDown",  (int)menu.keyVolumeDown);
    menu.keyMute        = (nk_rune)rlconfig_get_int(menu.cfg, "raylib-libretro", "keyMute",        (int)menu.keyMute);
    menu.keyFastForward = (nk_rune)rlconfig_get_int(menu.cfg, "raylib-libretro", "keyFastForward", (int)menu.keyFastForward);
    menu.fastForwardSpeed = rlconfig_get_float(menu.cfg, "raylib-libretro", "fastForwardSpeed", menu.fastForwardSpeed);
    if (menu.fastForwardSpeed < 1.1f) menu.fastForwardSpeed = 1.1f;
    if (menu.fastForwardSpeed > 10.0f) menu.fastForwardSpeed = 10.0f;
    menu.keySlowMotion = (nk_rune)rlconfig_get_int(menu.cfg, "raylib-libretro", "keySlowMotion", (int)menu.keySlowMotion);
    menu.slowMotionSpeed = rlconfig_get_float(menu.cfg, "raylib-libretro", "slowMotionSpeed", menu.slowMotionSpeed);
    // Migrate the legacy x10 integer storage (1..9) to the 0.1..0.9 float scale.
    if (menu.slowMotionSpeed > 0.95f) menu.slowMotionSpeed /= 10.0f;
    if (menu.slowMotionSpeed < 0.1f) menu.slowMotionSpeed = 0.1f;
    if (menu.slowMotionSpeed > 0.9f) menu.slowMotionSpeed = 0.9f;

    const char* coreDirectory = rlconfig_get(menu.cfg, "raylib-libretro", "coreDirectory");
    if (coreDirectory) TextCopy(menu.coreDirectory, coreDirectory);
    const char* saveDirectory = rlconfig_get(menu.cfg, "raylib-libretro", "saveDirectory");
    if (saveDirectory) TextCopy(menu.saveDirectory, saveDirectory);
    const char* coreAssetsDirectory = rlconfig_get(menu.cfg, "raylib-libretro", "coreAssetsDirectory");
    if (coreAssetsDirectory) TextCopy(menu.coreAssetsDirectory, coreAssetsDirectory);
    const char* systemDirectory = rlconfig_get(menu.cfg, "raylib-libretro", "systemDirectory");
    if (systemDirectory) TextCopy(menu.systemDirectory, systemDirectory);
    const char* playlistsDirectory = rlconfig_get(menu.cfg, "raylib-libretro", "playlistsDirectory");
    if (playlistsDirectory) TextCopy(menu.playlistsDirectory, playlistsDirectory);
    const char* fileBrowserStartDirectory = rlconfig_get(menu.cfg, "raylib-libretro", "fileBrowserStartDirectory");
    if (fileBrowserStartDirectory) TextCopy(menu.fileBrowserStartDirectory, fileBrowserStartDirectory);

    for (int i = 0; i < 16; i++) {
        menu.keyboardP1[i] = (nk_rune)rlconfig_get_int(menu.cfg, "raylib-libretro", TextFormat("keyP1[%d]", i), (int)menu.keyboardP1[i]);
    }
    LibretroMenuApplyKeyboardPlayer1();

    TraceLog(LOG_INFO, "MENU: Loaded menu settings from %s", RAYLIB_LIBRETRO_CFG_FILE);
    return true;
#else
    LibretroMenuApplyKeyboardPlayer1();
    return false;
#endif
}

void CloseLibretroMenu(void) {
    if (menu.ctx == NULL) {
        return;
    }

    // Clear out the gamepad system.
    nk_gamepad_free(nk_console_get_gamepads(menu.console));

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

#ifdef RAYLIB_LIBRETRO_CONFIG_H
    rlconfig_free(menu.cfg);
    menu.cfg = NULL;
#endif
}

static void UpdateLibretroMenuVisibility(void) {

    // Keep fullscreen checkbox in sync with the actual window state (e.g. F11 presses).
    // On Emscripten, fullscreen is deferred so IsWindowFullscreen() lags behind the
    // intended state — skip the sync and let menu.fullscreen drive the checkbox.
#ifndef __EMSCRIPTEN__
    menu.fullscreen = (nk_bool)IsWindowFullscreen();
#endif

    // Disable game-dependent items when no game is loaded.
    nk_bool gameReady = (nk_bool)IsLibretroGameReady();

    // Show "Core Options" only when at least one visible option exists. This
    // prevents an empty submenu when all options are hidden by the core's
    // options_update_display_callback (e.g. PicoDrive hiding platform-specific
    // options until visibility is resolved after the first retro_run frame).
    if (menu.optionsMenu && IsLibretroReady()) {
        int visibleOptions = 0;
        for (unsigned i = 0; i < LIBRETRO.core.variableCount; i++) {
            if (TextLength(LIBRETRO.core.variableValuesList[i]) > 0 && LIBRETRO.core.variableVisible[i]) {
                visibleOptions++;
                break;
            }
        }
        menu.optionsMenu->visible = (nk_bool)(visibleOptions > 0);
    } else if (menu.optionsMenu) {
        menu.optionsMenu->visible = nk_false;
    }
    if (menu.saveStateButton) {
        menu.saveStateButton->parent->visible = gameReady;
    }
    if (menu.resumeButton) menu.resumeButton->visible = gameReady;
    if (menu.resetGameButton) menu.resetGameButton->visible = gameReady;
    if (menu.cheatsMenuButton) menu.cheatsMenuButton->visible = gameReady;
}

void UpdateLibretroMenu(void) {
    if (menu.ctx == NULL) {
        return;
    }

    // Menu Guide Button
    if (IsGamepadButtonReleased(0, GAMEPAD_BUTTON_MIDDLE) ||
            IsGamepadButtonReleased(1, GAMEPAD_BUTTON_MIDDLE) ||
            IsGamepadButtonReleased(3, GAMEPAD_BUTTON_MIDDLE) ||
            IsGamepadButtonReleased(4, GAMEPAD_BUTTON_MIDDLE)) {
        menu.active = !menu.active;
    }

    // Menu Key
    if (IsKeyReleased(NuklearKeyToKeyboardKey(menu.keyMenu)) && !menu.active) {
        menu.active = true;
    }

    if (!menu.active && IsLibretroGameReady() && IsWindowMinimized()) {
        menu.active = true;
    }

    // Track menu open state so we can force a rebuild on every open. The
    // dirty flag alone has been seen to miss late-arriving options on Firefox
    // where wasm dynamic linking and event-loop timing differ from Chrome.
    static bool menuWasActive = false;
    if (!menu.active) {
        menuWasActive = false;
        return;
    }
    bool menuJustOpened = !menuWasActive;
    menuWasActive = true;

    // Rebuild when any of these signals say the options pane is stale:
    //   * dirty flag set by a SET_CORE_OPTIONS_DISPLAY / late SET_VARIABLES
    //   * variable count changed since we last built (count diff catches
    //     additions/removals that didn't set the dirty flag for any reason)
    //   * library name changed (core swap; same count but different options)
    //   * the menu just transitioned from closed to open (fresh-state guard)
    static unsigned lastBuiltVariableCount = 0;
    static char lastBuiltCoreName[sizeof(LIBRETRO.core.libraryName)] = {0};
    bool countChanged = (lastBuiltVariableCount != LIBRETRO.core.variableCount);
    bool coreChanged  = !TextIsEqual(lastBuiltCoreName, LIBRETRO.core.libraryName);

    if (LIBRETRO.core.variablesVisibilityDirty || countChanged || coreChanged || menuJustOpened) {
        // When the menu first opens and the game is running, invoke the display
        // callback to get the latest visibility state. Cores like PicoDrive call
        // SET_CORE_OPTIONS_DISPLAY during retro_load_game to set initial
        // visibility, but the callback may resolve it differently after the
        // first retro_run frame. Invoking it here ensures the menu always opens
        // with up-to-date option visibility even if no frames have run yet.
        if (menuJustOpened && LIBRETRO.core.options_update_display_callback && IsLibretroGameReady()) {
            LIBRETRO.core.options_update_display_callback();
        }
        BuildLibretroMenuOptions(&menu);
        LIBRETRO.core.variablesVisibilityDirty = false;
        lastBuiltVariableCount = LIBRETRO.core.variableCount;
        TextCopy(lastBuiltCoreName, LIBRETRO.core.libraryName);
    }

    UpdateLibretroMenuVisibility();

    menu.shaderSelectedIndex = (int)GetActiveLibretroShaderType();

    // Scaling
    float scaling = (GetScreenWidth() >= 2560) ? 4.0f : (GetScreenWidth() >= 1280) ? 3.0f : 2.0f;
    SetNuklearScaling(menu.ctx, scaling);

    // Input & Update
    nk_gamepad_update(nk_console_get_gamepads(menu.console));
    UpdateNuklear(menu.ctx);

    // Render
    struct nk_rect windowPos = nk_rect(0, 0, (float)GetScreenWidth()/scaling, (float)GetScreenHeight()/scaling);
    nk_console_render_window(menu.console, "raylib-libretro", windowPos,
        NK_WINDOW_SCROLL_AUTO_HIDE |
        // Show the window title only on the top level.
        ((nk_console_active_parent(menu.console) == menu.console) ? NK_WINDOW_TITLE : 0));
}

void DrawLibretroMenu(void) {
    if (menu.ctx == NULL) {
        return;
    }
    // Always draw when a context exists so nk_clear runs every frame nk_begin
    // ran in UpdateLibretroMenu — otherwise a callback that toggles menu.active
    // off mid-frame leaves win->seq == ctx->seq and trips the next nk_begin.
    DrawNuklear(menu.ctx);
}

#if defined(__cplusplus)
}
#endif

#endif // RAYLIB_LIBRETRO_MENU_IMPLEMENTATION_ONCE
#endif // RAYLIB_LIBRETRO_MENU_IMPLEMENTATION
