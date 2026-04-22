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
    bool shouldQuit;
    nk_bool fullscreen;
    nk_console* optionsMenu;              // "Core Options" submenu node
    nk_console* saveStateButton;
    nk_console* loadStateButton;
    nk_console* resumeButton;
    int shaderSelectedIndex;              // tracks active shader for combobox sync
    int optionSelectedIndices[128];       // per-option combobox index (matches LIBRETRO_MAX_CORE_VARIABLES)
    nk_bool optionCheckboxValues[128];    // per-option checkbox state for enabled/disabled options
} LibretroMenu;

#if defined(__cplusplus)
extern "C" {
#endif

LibretroMenu* InitLibretroMenu(void);
bool IsLibretroMenuReady(void);
void CloseLibretroMenu(void);
void UpdateLibretroMenu(void);
void DrawLibretroMenu(void);
void BuildLibretroMenuOptions(LibretroMenu* menu); // Populate "Core Options" with comboboxes from the loaded core.
void SetLibretroMenuStyle(LibretroMenuStyle style);

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

// Event handler for shader combobox change
static void MenuSettingsShaderChanged(nk_console* widget, void* user_data) {
    int* sel = (int*)user_data;
    if (!sel) {
        return;
    }
    SetActiveLibretroShader((LibretroShaderType)(*sel));
}


static void MenuSettingsThemeChanged(nk_console* widget, void* user_data) {
    int* theme = (int*)user_data;
    if (!theme) {
        return;
    }
    SetLibretroMenuStyle((LibretroShaderType)(*theme));
}

static void MenuSettingsVolumeChanged(nk_console* widget, void* user_data) {
    float* volume = (float*)user_data;
    if (!volume) {
        return;
    }
    SetLibretroVolume(*volume);
}

static void MenuSettingsFpsChanged(nk_console* widget, void* user_data) {
    int* fps = (int*)user_data;
    if (!fps) {
        return;
    }
    switch (*fps) {
        case 0: SetTargetFPS(0); break;
        case 1: SetTargetFPS(30); break;
        case 2: SetTargetFPS(50); break;
        case 3: SetTargetFPS(60); break;
        case 4: SetTargetFPS(80); break;
        case 5: SetTargetFPS(100); break;
        case 6: SetTargetFPS(120); break;
        case 7: SetTargetFPS(144); break;
    }
    TraceLog(LOG_INFO, "Target FPS changed to %d", *fps == 0 ? 0 : (int[]){0,30,50,60,80,100,120,144}[*fps]);
}

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
            struct nk_color background = nk_rgba(40, 42, 54, 200);
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
    menu.fullscreen = IsWindowFullscreen();
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
        SaveFileData(TextFormat("save_%s.sav", GetLibretroName()), saveData, (int)size);
        MemFree(saveData);
        ShowLibretroMessage("State Saved", 2.0f);
        menu.active = false;
    }
    else {
        ShowLibretroMessage("State Saved Failed", 2.0f);
    }
}

static void MenuResumeClicked(nk_console* widget, void* user_data) {
    if (IsLibretroGameReady()) {
        menu.active = false;
    }
}

static void LibretroMenuLoadStateClicked(nk_console* widget, void* user_data) {
    (void)widget;
    (void)user_data;
    if (!IsLibretroGameReady()) return;
    int dataSize;
    void* saveData = LoadFileData(TextFormat("save_%s.sav", GetLibretroName()), &dataSize);
    if (saveData != NULL) {
        SetLibretroSerializedData(saveData, (unsigned int)dataSize);
        MemFree(saveData);
        ShowLibretroMessage("Loaded State", 2.0f);
        menu.active = false;
    }
    else {
        ShowLibretroMessage("Load State failed", 2.0f);
    }
}

LibretroMenu* InitLibretroMenu(void) {
    int fontSize = 13 * 2;
    menu = (LibretroMenu){0};
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

    // Build the Menu
    menu.resumeButton = nk_console_button_onclick(menu.console, "Resume", &MenuResumeClicked);
    nk_console_button_set_symbol(menu.resumeButton, NK_SYMBOL_TRIANGLE_RIGHT);

    // Load Game
    nk_console_button(menu.console, "Load Game");
    menu.optionsMenu = nk_console_button(menu.console, "Core Options");
    {
        nk_console_button_set_symbol(
            nk_console_button_onclick(menu.optionsMenu, "Back", &nk_console_button_back),
            NK_SYMBOL_TRIANGLE_LEFT);
    }

    // Settings
    nk_console* settings = nk_console_button(menu.console, "Settings");
    {
        // Back
        nk_console_button_set_symbol(
            nk_console_button_onclick(settings, "Back", &nk_console_button_back),
            NK_SYMBOL_TRIANGLE_LEFT);

        // Fullscreen
        menu.fullscreen = (nk_bool)IsWindowFullscreen();
        nk_console* fullscreenCheckbox = nk_console_checkbox(settings, "Fullscreen", &menu.fullscreen);
        nk_console_add_event(fullscreenCheckbox, NK_CONSOLE_EVENT_CHANGED, LibretroMenuFullscreenChanged);

        // Shader
        static char shaderNames[256] = {0};
        if (shaderNames[0] == '\0') {
            int offset = 0;
            for (int i = 0; i < LIBRETRO_SHADER_TYPE_COUNT; ++i) {
                const char* name = GetLibretroShaderName((LibretroShaderType)i);
                int len = (int)strlen(name);
                if (offset + len + 1 < (int)sizeof(shaderNames)) {
                    if (i > 0) shaderNames[offset++] = '|';
                    memcpy(shaderNames + offset, name, len);
                    offset += len;
                }
            }
        }
        menu.shaderSelectedIndex = (int)GetActiveLibretroShaderType();
        nk_console* shaderCombo = nk_console_combobox(settings, "Shader", shaderNames, '|', &menu.shaderSelectedIndex);
        nk_console_add_event_handler(shaderCombo, NK_CONSOLE_EVENT_CHANGED, &MenuSettingsShaderChanged, &menu.shaderSelectedIndex, NULL);

        // Theme
        static int themeSelectedIndex = LIBRETRO_MENU_STYLE_DRACULA;
        nk_console* theme = nk_console_combobox(settings, "Theme", "Dark|Dracula", '|', &themeSelectedIndex);
        nk_console_add_event_handler(theme, NK_CONSOLE_EVENT_CHANGED, &MenuSettingsThemeChanged, &themeSelectedIndex, NULL);

        // Volume
        static float volumeSelected = 1.0f;
        nk_console* volume = nk_console_slider_float(settings, "Volume", 0.0f, &volumeSelected, 1.0f, 0.1f);
        nk_console_add_event_handler(volume, NK_CONSOLE_EVENT_CHANGED, &MenuSettingsVolumeChanged, &volumeSelected, NULL);

        // Target FPS
        static int fpsSelected = 0;
        nk_console* fps = nk_console_combobox(settings, "Target FPS", "Off|30|50|60|80|100|120|144", '|', &fpsSelected);
        nk_console_add_event_handler(volume, NK_CONSOLE_EVENT_CHANGED, &MenuSettingsFpsChanged, &fpsSelected, NULL);
    }

    menu.saveStateButton = nk_console_button(menu.console, "Save State");
    nk_console_add_event(menu.saveStateButton, NK_CONSOLE_EVENT_CLICKED, &LibretroMenuSaveStateClicked);
    nk_console_button_set_symbol(menu.saveStateButton, NK_SYMBOL_RECT_SOLID);

    menu.loadStateButton = nk_console_button(menu.console, "Load State");
    nk_console_add_event(menu.loadStateButton, NK_CONSOLE_EVENT_CLICKED, &LibretroMenuLoadStateClicked);
    nk_console_button_set_symbol(menu.loadStateButton, NK_SYMBOL_RECT_OUTLINE);

    nk_console* quitButton = nk_console_button(menu.console, "Quit");
    nk_console_add_event(quitButton, NK_CONSOLE_EVENT_CLICKED, &LibretroMenuQuitClicked);
    nk_console_button_set_symbol(quitButton, NK_SYMBOL_X);

    SetLibretroMenuStyle(LIBRETRO_MENU_STYLE_DRACULA);
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
            if (len >= outSize) len = outSize - 1;
            memcpy(out, p, (unsigned int)len);
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
    LibretroMenuGetNthToken(LibretroCore.variableValuesList[i],
                            menu.optionSelectedIndices[i],
                            value, LIBRETRO_CORE_VARIABLE_VALUE_LEN);
    if (TextLength(value) > 0) {
        SetLibretroCoreOption(LibretroCore.variableKeys[i], value);
        SaveLibretroCoreOptions();
    }
}

// Called when a core option enabled/disabled checkbox changes.
// user_data is the option index cast to (void*).
static void LibretroMenuOptionCheckboxChanged(nk_console* widget, void* user_data) {
    (void)widget;
    unsigned i = (unsigned)(uintptr_t)user_data;
    const char* value = menu.optionCheckboxValues[i] ? "enabled" : "disabled";
    SetLibretroCoreOption(LibretroCore.variableKeys[i], value);
    SaveLibretroCoreOptions();
}

// Returns true if the only values are "enabled" and "disabled" (in either order).
static bool LibretroMenuIsEnabledDisabledOption(const char* valuesList) {
    if (!valuesList || !*valuesList) return false;
    return TextIsEqual(valuesList, "enabled|disabled") ||
           TextIsEqual(valuesList, "disabled|enabled");
}

void BuildLibretroMenuOptions(LibretroMenu* m) {
    if (!m || !m->optionsMenu) return;

    // Clear any previously built option children and restore Back button
    nk_console_free_children(m->optionsMenu);

    if (LibretroCore.variableCount == 0) {
        m->optionsMenu->visible = nk_false;
        return;
    }

    m->optionsMenu->visible = nk_true;

    nk_console_button_set_symbol(
        nk_console_button_onclick(menu.optionsMenu, "Back", &nk_console_button_back),
        NK_SYMBOL_TRIANGLE_LEFT);

    for (unsigned i = 0; i < LibretroCore.variableCount; i++) {
        if (TextLength(LibretroCore.variableValuesList[i]) == 0) continue;
        if (!LibretroCore.variableVisible[i]) continue;

        const char *label = TextLength(LibretroCore.variableLabels[i]) > 0
            ? LibretroCore.variableLabels[i]
            : LibretroCore.variableKeys[i];

        nk_console* widget;

        if (LibretroMenuIsEnabledDisabledOption(LibretroCore.variableValuesList[i])) {
            m->optionCheckboxValues[i] = (nk_bool)TextIsEqual(LibretroCore.variableValues[i], "enabled");
            widget = nk_console_checkbox(m->optionsMenu, label, &m->optionCheckboxValues[i]);
            nk_console_add_event_handler(widget, NK_CONSOLE_EVENT_CHANGED,
                                         LibretroMenuOptionCheckboxChanged,
                                         (void*)(uintptr_t)i, NULL);
        } else {
            // Resolve current selected index by matching variableValues[i] against valuesList
            int selIdx = 0, tok = 0;
            const char *cur = LibretroCore.variableValues[i];
            const char *p = LibretroCore.variableValuesList[i];
            while (p && *p) {
                const char *pipe = p;
                while (*pipe && *pipe != '|') pipe++;
                int len = (int)(pipe - p);
                char tokBuf[LIBRETRO_CORE_VARIABLE_VALUE_LEN] = {0};
                if (len < LIBRETRO_CORE_VARIABLE_VALUE_LEN) {
                    memcpy(tokBuf, p, (unsigned int)len);
                }
                if (TextIsEqual(tokBuf, cur)) { selIdx = tok; break; }
                if (!*pipe) break;
                p = pipe + 1;
                tok++;
            }
            m->optionSelectedIndices[i] = selIdx;

            const char *displayStr = TextLength(LibretroCore.variableDisplayList[i]) > 0
                ? LibretroCore.variableDisplayList[i]
                : LibretroCore.variableValuesList[i];

            widget = nk_console_combobox(m->optionsMenu, label,
                                         displayStr, '|',
                                         &m->optionSelectedIndices[i]);
            nk_console_add_event_handler(widget, NK_CONSOLE_EVENT_CHANGED,
                                         LibretroMenuOptionChanged,
                                         (void*)(uintptr_t)i, NULL);
        }

        if (TextLength(LibretroCore.variableTooltips[i]) > 0) {
            widget->tooltip = LibretroCore.variableTooltips[i];
        }
    }
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

void UpdateLibretroMenuVisibility() {

    // Keep fullscreen checkbox in sync with the actual window state (e.g. F11 presses).
    menu.fullscreen = (nk_bool)IsWindowFullscreen();

    // Disable game-dependent items when no game is loaded.
    nk_bool gameReady = (nk_bool)IsLibretroGameReady();
    if (menu.optionsMenu) menu.optionsMenu->visible = LibretroCore.variableCount > 0 && IsLibretroReady();
    if (menu.saveStateButton) menu.saveStateButton->visible = gameReady;
    if (menu.loadStateButton) menu.loadStateButton->visible = gameReady;
    if (menu.resumeButton) menu.resumeButton->visible = gameReady;
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

    if (LibretroCore.variablesVisibilityDirty) {
        BuildLibretroMenuOptions(&menu);
        LibretroCore.variablesVisibilityDirty = false;
    }

    UpdateLibretroMenuVisibility();

    menu.shaderSelectedIndex = (int)GetActiveLibretroShaderType();
    UpdateNuklear(menu.ctx);

    struct nk_rect windowPos = nk_rect(0, 0, (float)GetScreenWidth(), (float)GetScreenHeight());
    nk_console_render_window(menu.console, "raylib-libretro", windowPos, NK_WINDOW_SCROLL_AUTO_HIDE);
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

#endif // RAYLIB_LIBRETRO_MENU_IMPLEMENTATION_ONCE
#endif // RAYLIB_LIBRETRO_MENU_IMPLEMENTATION
