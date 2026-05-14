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

#define NK_GAMEPAD_RAYLIB
#define NK_BUTTON_TRIGGER_ON_RELEASE
#define RAYLIB_NUKLEAR_INCLUDE_DEFAULT_FONT
#include "../vendor/raylib-nuklear/include/raylib-nuklear.h"
#include "../../vendor/nuklear_gamepad/nuklear_gamepad.h"
#include "../vendor/nuklear_console/nuklear_console.h"

typedef enum LibretroMenuStyle {
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
    nk_console* optionsMenu;              // "Core Options" submenu node
    nk_console* saveStateButton;
    nk_console* loadStateButton;
    nk_console* resumeButton;
    nk_console* closeGameButton;
    int shaderSelectedIndex;
    int textureFilterIndex;
    int themeSelectedIndex;
    float volumeSelected;
    bool rewindEnabled;
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
    nk_rune keyFastForward;
    int fastForwardSpeed;
    nk_rune keySlowMotion;
    float slowMotionSpeed;
    int optionSelectedIndices[128];       // per-option combobox index (matches LIBRETRO_MAX_CORE_VARIABLES)
    nk_bool optionCheckboxValues[128];    // per-option checkbox state for enabled/disabled options
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

#if defined(__cplusplus)
}
#endif

#endif

#ifdef RAYLIB_LIBRETRO_MENU_IMPLEMENTATION
#ifndef RAYLIB_LIBRETRO_MENU_IMPLEMENTATION_ONCE
#define RAYLIB_LIBRETRO_MENU_IMPLEMENTATION_ONCE

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

static void LibretroMenuSettingChanged(nk_console* widget, void* user_data) {
    (void)widget;
    (void)user_data;
    SetActiveLibretroShader((LibretroShaderType)menu.shaderSelectedIndex);
    SetLibretroMenuStyle((LibretroMenuStyle)menu.themeSelectedIndex);
    SetLibretroVolume(menu.volumeSelected);
    if (LibretroCore.textureFilter != menu.textureFilterIndex) {
        LibretroCore.textureFilter = menu.textureFilterIndex;
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

    switch (style) {
        case LIBRETRO_MENU_STYLE_DRACULA: {
            struct nk_color table[NK_COLOR_COUNT];
            nk_style_default(menu.ctx);
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
        case LIBRETRO_MENU_STYLE_DARK:
        default: {
            nk_style_default(menu.ctx);
            break;
        }
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
        const char* savesDir = GetLibretroDirectory(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY);
        SaveFileData(TextFormat("%s/%s_%02d.sav", savesDir, GetLibretroContentName(), menu.saveSlotIndex + 1), saveData, (int)size);
        MemFree(saveData);
        ShowLibretroMessage(TextFormat("Slot %d Saved", menu.saveSlotIndex + 1), 2.0f);
        menu.active = false;
    }
    else {
        ShowLibretroMessage("State Saved Failed", 2.0f);
    }
}

static void MenuResumeClicked(nk_console* widget, void* user_data) {
    NK_UNUSED(widget);
    NK_UNUSED(user_data);
    if (IsLibretroGameReady()) {
        menu.active = false;
    }
}

static void MenuCloseGameClicked(nk_console* widget, void* user_data) {
    NK_UNUSED(widget);
    NK_UNUSED(user_data);
    if (!IsLibretroReady()) {
        return;
    }
    if (IsLibretroGameReady()) {
        UnloadLibretroGame();
    }
    CloseLibretro();
    UpdateLibretroMenuVisibility();
}

static void MenuLoadGameClicked(nk_console* widget, void* user_data) {
    NK_UNUSED(widget);
    NK_UNUSED(user_data);
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
    const char* dir = LibretroCore.coreDirectory;
    if (!dir || !dir[0]) return;

    int currentCount = LibretroCoreDirectoryFileCount(dir);
    int cachedCount = rlconfig_get_int(menu.cfg, LIBRETRO_CORE_CACHE_SECTION, "file_count", -1);
    if (cachedCount == currentCount) {
        int cached = rlconfig_get_int(menu.cfg, LIBRETRO_CORE_CACHE_SECTION, "count", 0);
        TraceLog(LOG_INFO, "LIBRETRO: Core cache valid (%d cores)", cached);
        return;
    }

    TraceLog(LOG_INFO, "LIBRETRO: Rescanning cores in %s", dir);
    rlconfig_clear_section(menu.cfg, LIBRETRO_CORE_CACHE_SECTION);
    rlconfig_set_int(menu.cfg, LIBRETRO_CORE_CACHE_SECTION, "file_count", currentCount);

    if (!DirectoryExists(dir)) {
        rlconfig_save(menu.cfg, RAYLIB_LIBRETRO_CFG_FILE);
        return;
    }

    FilePathList files = LoadDirectoryFiles(dir);
    int coreIndex = 0;
    for (unsigned int i = 0; i < files.count; i++) {
        const char* ext = GetFileExtension(files.paths[i]);
        if (!IsLibretroCoreFile(files.paths[i])) continue;
        if (!InitLibretroEx(files.paths[i], true)) continue;
        if (TextLength(LibretroCore.validExtensions) == 0) continue;

        char keyPath[64], keyExts[64];
        TextCopy(keyPath, TextFormat("core_%d_path", coreIndex));
        TextCopy(keyExts, TextFormat("core_%d_extensions", coreIndex));
        rlconfig_set(menu.cfg, LIBRETRO_CORE_CACHE_SECTION, keyPath, files.paths[i]);
        rlconfig_set(menu.cfg, LIBRETRO_CORE_CACHE_SECTION, keyExts, LibretroCore.validExtensions);
        TraceLog(LOG_INFO, "LIBRETRO: Cached %s (%s)", LibretroCore.libraryName, LibretroCore.validExtensions);
        coreIndex++;
    }
    UnloadDirectoryFiles(files);

    rlconfig_set_int(menu.cfg, LIBRETRO_CORE_CACHE_SECTION, "count", coreIndex);
    rlconfig_save(menu.cfg, RAYLIB_LIBRETRO_CFG_FILE);
    TraceLog(LOG_INFO, "LIBRETRO: Cached %d cores in %s", coreIndex, dir);
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
        ShowLibretroMessage(TextFormat("Slot %d Loaded", menu.saveSlotIndex + 1), 2.0f);
        menu.active = false;
    }
    else {
        ShowLibretroMessage("Load State failed", 2.0f);
    }
}

LibretroMenu* InitLibretroMenu(void) {
    int fontSize = 13 * 2;
    menu = (LibretroMenu){0};
    menu.themeSelectedIndex = LIBRETRO_MENU_STYLE_DRACULA;
    menu.volumeSelected = 1.0f;
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
    menu.keyFastForward = (nk_rune)'F';
    menu.fastForwardSpeed = 3;
    menu.keySlowMotion = (nk_rune)'G';
    menu.slowMotionSpeed = 0.5f;
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
    TextCopy(LibretroCore.coreDirectory, "cores");
    LoadLibretroMenuSettings();
    ScanLibretroCoreDirectory();

    // Build the Menu
    menu.resumeButton = nk_console_button_onclick(menu.console, "Resume", &MenuResumeClicked);
    nk_console_button_set_symbol(menu.resumeButton, NK_SYMBOL_TRIANGLE_RIGHT);



    // Load Game
    nk_console_button_onclick(menu.console, "Load Game", &MenuLoadGameClicked);

    // Close Game
    menu.closeGameButton = nk_console_button_onclick(menu.console, "Close Game", &MenuCloseGameClicked);
    nk_console_button_set_symbol(menu.resumeButton, NK_SYMBOL_X);

    // Settings
    nk_console* settings = nk_console_button(menu.console, "Settings");
    {
        // Back
        nk_console_button_set_symbol(
            nk_console_button_onclick(settings, "Back", &nk_console_button_back),
            NK_SYMBOL_TRIANGLE_LEFT);

        // Fullscreen
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
                    TextCopy(shaderNames + offset, name);
                    offset += len;
                }
            }
        }
        menu.shaderSelectedIndex = (int)GetActiveLibretroShaderType();
        nk_console* shaderCombo = nk_console_combobox(settings, "Shader", shaderNames, '|', &menu.shaderSelectedIndex);
        nk_console_add_event_handler(shaderCombo, NK_CONSOLE_EVENT_CHANGED, &LibretroMenuSettingChanged, NULL, NULL);

        // Texture Filter
        nk_console* textureFilter = nk_console_combobox(settings, "Texture Filter", "None|Bilinear|Trilinear|Anisotropic 4x|Anisotropic 8x|Anisotropic 16x", '|', &menu.textureFilterIndex);
        nk_console_add_event_handler(textureFilter, NK_CONSOLE_EVENT_CHANGED, &LibretroMenuSettingChanged, NULL, NULL);

        // Theme
        nk_console* themeCombo = nk_console_combobox(settings, "Theme", "Dracula|Dark", '|', &menu.themeSelectedIndex);
        nk_console_add_event_handler(themeCombo, NK_CONSOLE_EVENT_CHANGED, &LibretroMenuSettingChanged, NULL, NULL);
        SetLibretroMenuStyle((LibretroMenuStyle)menu.themeSelectedIndex);

        // Volume
        nk_console* volume = nk_console_slider_float(settings, "Volume", 0.0f, &menu.volumeSelected, 1.0f, 0.1f);
        nk_console_add_event_handler(volume, NK_CONSOLE_EVENT_CHANGED, &LibretroMenuSettingChanged, NULL, NULL);

        // Fast Forward Speed
        nk_console* ffSpeed = nk_console_slider_int(settings, "Fast Forward Speed", 2, &menu.fastForwardSpeed, 10, 1);
        nk_console_add_event_handler(ffSpeed, NK_CONSOLE_EVENT_CHANGED, &LibretroMenuSettingChanged, NULL, NULL);

        // Slow Motion Speed
        nk_console* smSpeed = nk_console_slider_float(settings, "Slow Motion Speed", 0.1f, &menu.slowMotionSpeed, 0.9f, 0.1f);
        nk_console_add_event_handler(smSpeed, NK_CONSOLE_EVENT_CHANGED, &LibretroMenuSettingChanged, NULL, NULL);

        // Rewind
        nk_console* rewind = nk_console_checkbox(settings, "Rewind", &menu.rewindEnabled);
        nk_console_add_event_handler(rewind, NK_CONSOLE_EVENT_CHANGED, &LibretroMenuSettingChanged, NULL, NULL);

        // Save Slot
        nk_console* slotCombo = nk_console_combobox(settings, "Save Slot",
            "Slot 1|Slot 2|Slot 3|Slot 4|Slot 5|Slot 6|Slot 7|Slot 8|Slot 9|Slot 10",
            '|', &menu.saveSlotIndex);
        nk_console_add_event_handler(slotCombo, NK_CONSOLE_EVENT_CHANGED, &LibretroMenuSettingChanged, NULL, NULL);

        // Keys
        nk_console* keysTree = nk_console_tree(settings, "Keys", nk_false);
        {
            nk_console* w;
            w = nk_console_key(keysTree, "Screenshot", &menu.keyScreenshot);
            nk_console_add_event_handler(w, NK_CONSOLE_EVENT_CHANGED, &LibretroMenuSettingChanged, NULL, NULL);
            w = nk_console_key(keysTree, "Rewind", &menu.keyRewind);
            nk_console_add_event_handler(w, NK_CONSOLE_EVENT_CHANGED, &LibretroMenuSettingChanged, NULL, NULL);
            w = nk_console_key(keysTree, "Menu", &menu.keyMenu);
            nk_console_add_event_handler(w, NK_CONSOLE_EVENT_CHANGED, &LibretroMenuSettingChanged, NULL, NULL);
            w = nk_console_key(keysTree, "Save State", &menu.keySaveState);
            nk_console_add_event_handler(w, NK_CONSOLE_EVENT_CHANGED, &LibretroMenuSettingChanged, NULL, NULL);
            w = nk_console_key(keysTree, "Load State", &menu.keyLoadState);
            nk_console_add_event_handler(w, NK_CONSOLE_EVENT_CHANGED, &LibretroMenuSettingChanged, NULL, NULL);
            w = nk_console_key(keysTree, "Prev Slot", &menu.keyPrevSlot);
            nk_console_add_event_handler(w, NK_CONSOLE_EVENT_CHANGED, &LibretroMenuSettingChanged, NULL, NULL);
            w = nk_console_key(keysTree, "Next Slot", &menu.keyNextSlot);
            nk_console_add_event_handler(w, NK_CONSOLE_EVENT_CHANGED, &LibretroMenuSettingChanged, NULL, NULL);
            w = nk_console_key(keysTree, "Fullscreen", &menu.keyFullscreen);
            nk_console_add_event_handler(w, NK_CONSOLE_EVENT_CHANGED, &LibretroMenuSettingChanged, NULL, NULL);
            w = nk_console_key(keysTree, "Previous Shader", &menu.keyPrevShader);
            nk_console_add_event_handler(w, NK_CONSOLE_EVENT_CHANGED, &LibretroMenuSettingChanged, NULL, NULL);
            w = nk_console_key(keysTree, "Next Shader", &menu.keyNextShader);
            nk_console_add_event_handler(w, NK_CONSOLE_EVENT_CHANGED, &LibretroMenuSettingChanged, NULL, NULL);
            w = nk_console_key(keysTree, "Reset", &menu.keyReset);
            nk_console_add_event_handler(w, NK_CONSOLE_EVENT_CHANGED, &LibretroMenuSettingChanged, NULL, NULL);
            w = nk_console_key(keysTree, "Quit", &menu.keyQuit);
            nk_console_add_event_handler(w, NK_CONSOLE_EVENT_CHANGED, &LibretroMenuSettingChanged, NULL, NULL);
            w = nk_console_key(keysTree, "Volume Up", &menu.keyVolumeUp);
            nk_console_add_event_handler(w, NK_CONSOLE_EVENT_CHANGED, &LibretroMenuSettingChanged, NULL, NULL);
            w = nk_console_key(keysTree, "Volume Down", &menu.keyVolumeDown);
            nk_console_add_event_handler(w, NK_CONSOLE_EVENT_CHANGED, &LibretroMenuSettingChanged, NULL, NULL);
            w = nk_console_key(keysTree, "Fast Forward", &menu.keyFastForward);
            nk_console_add_event_handler(w, NK_CONSOLE_EVENT_CHANGED, &LibretroMenuSettingChanged, NULL, NULL);
            w = nk_console_key(keysTree, "Slow Motion", &menu.keySlowMotion);
            nk_console_add_event_handler(w, NK_CONSOLE_EVENT_CHANGED, &LibretroMenuSettingChanged, NULL, NULL);
        }

        // Directories
        nk_console* directoryTree = nk_console_tree(settings, "Directories", nk_false);
        {
            nk_console* coreDirectory = nk_console_dir(directoryTree, "Cores", LibretroCore.coreDirectory, RAYLIB_LIBRETRO_VFS_MAX_PATH);
            nk_console_add_event_handler(coreDirectory, NK_CONSOLE_EVENT_CHANGED, &LibretroMenuSettingChanged, NULL, NULL);

            nk_console* saveDirectory = nk_console_dir(directoryTree, "Saves", LibretroCore.saveDirectory, RAYLIB_LIBRETRO_VFS_MAX_PATH);
            nk_console_add_event_handler(saveDirectory, NK_CONSOLE_EVENT_CHANGED, &LibretroMenuSettingChanged, NULL, NULL);

            nk_console* coreAssetsDirectory = nk_console_dir(directoryTree, "Assets", LibretroCore.coreAssetsDirectory, RAYLIB_LIBRETRO_VFS_MAX_PATH);
            nk_console_add_event_handler(coreAssetsDirectory, NK_CONSOLE_EVENT_CHANGED, &LibretroMenuSettingChanged, NULL, NULL);

            nk_console* systemDirectory = nk_console_dir(directoryTree, "System", LibretroCore.systemDirectory, RAYLIB_LIBRETRO_VFS_MAX_PATH);
            nk_console_add_event_handler(systemDirectory, NK_CONSOLE_EVENT_CHANGED, &LibretroMenuSettingChanged, NULL, NULL);

            nk_console* playlistsDirectory = nk_console_dir(directoryTree, "Playlists", LibretroCore.playlistsDirectory, RAYLIB_LIBRETRO_VFS_MAX_PATH);
            nk_console_add_event_handler(playlistsDirectory, NK_CONSOLE_EVENT_CHANGED, &LibretroMenuSettingChanged, NULL, NULL);

            nk_console* fileBrowserStartDirectory = nk_console_dir(directoryTree, "Content", LibretroCore.fileBrowserStartDirectory, RAYLIB_LIBRETRO_VFS_MAX_PATH);
            nk_console_add_event_handler(fileBrowserStartDirectory, NK_CONSOLE_EVENT_CHANGED, &LibretroMenuSettingChanged, NULL, NULL);
        }
    }

    // Core Options
    menu.optionsMenu = nk_console_button(menu.console, "Core Options");
    {
        nk_console_button_set_symbol(
            nk_console_button_onclick(menu.optionsMenu, "Back", &nk_console_button_back),
            NK_SYMBOL_TRIANGLE_LEFT);
    }

    // Save State
    menu.saveStateButton = nk_console_button(menu.console, "Save State");
    nk_console_add_event(menu.saveStateButton, NK_CONSOLE_EVENT_CLICKED, &LibretroMenuSaveStateClicked);
    nk_console_button_set_symbol(menu.saveStateButton, NK_SYMBOL_RECT_SOLID);

    // Load State
    menu.loadStateButton = nk_console_button(menu.console, "Load State");
    nk_console_add_event(menu.loadStateButton, NK_CONSOLE_EVENT_CLICKED, &LibretroMenuLoadStateClicked);
    nk_console_button_set_symbol(menu.loadStateButton, NK_SYMBOL_RECT_OUTLINE);

    // Quit
    nk_console* quitButton = nk_console_button(menu.console, "Quit");
    nk_console_add_event(quitButton, NK_CONSOLE_EVENT_CLICKED, &LibretroMenuQuitClicked);
    nk_console_button_set_symbol(quitButton, NK_SYMBOL_X);

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
    LibretroMenuGetNthToken(LibretroCore.variableValuesList[i],
                            menu.optionSelectedIndices[i],
                            value, LIBRETRO_CORE_VARIABLE_VALUE_LEN);
    if (TextLength(value) > 0) {
        SetLibretroCoreOption(LibretroCore.variableKeys[i], value);
    }
}

// Called when a core option enabled/disabled checkbox changes.
// user_data is the option index cast to (void*).
static void LibretroMenuOptionCheckboxChanged(nk_console* widget, void* user_data) {
    (void)widget;
    unsigned i = (unsigned)(uintptr_t)user_data;
    const char* value = menu.optionCheckboxValues[i] ? "enabled" : "disabled";
    SetLibretroCoreOption(LibretroCore.variableKeys[i], value);
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
        nk_console_button_onclick(m->optionsMenu, "Back", &nk_console_button_back),
        NK_SYMBOL_TRIANGLE_LEFT);

    for (unsigned i = 0; i < LibretroCore.variableCount; i++) {
        if (TextLength(LibretroCore.variableValuesList[i]) == 0) continue;
        if (!LibretroCore.variableVisible[i]) continue;

        const char* label = TextLength(LibretroCore.variableLabels[i]) > 0
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
            m->optionSelectedIndices[i] = LibretroMenuFindTokenIndex(
                LibretroCore.variableValuesList[i], LibretroCore.variableValues[i]);

            const char* displayStr = TextLength(LibretroCore.variableDisplayList[i]) > 0
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

static void LibretroMenuUpdateConfig(void) {
#ifdef RAYLIB_LIBRETRO_CONFIG_H
    if (!menu.cfg) return;
    rlconfig_set_int(menu.cfg, "raylib-libretro", "fullscreen", (int)menu.fullscreen);
    rlconfig_set_int(menu.cfg, "raylib-libretro", "shader", menu.shaderSelectedIndex);
    rlconfig_set_int(menu.cfg, "raylib-libretro", "textureFilter", menu.textureFilterIndex);
    rlconfig_set_int(menu.cfg, "raylib-libretro", "theme", menu.themeSelectedIndex);
    rlconfig_set_int(menu.cfg, "raylib-libretro", "volume", (int)(menu.volumeSelected * 100.0f));
    rlconfig_set_int(menu.cfg, "raylib-libretro", "rewind", menu.rewindEnabled ? 1 : 0);
    rlconfig_set_int(menu.cfg, "raylib-libretro", "keyScreenshot", (int)menu.keyScreenshot);
    rlconfig_set_int(menu.cfg, "raylib-libretro", "keyRewind", (int)menu.keyRewind);
    rlconfig_set_int(menu.cfg, "raylib-libretro", "keyMenu", (int)menu.keyMenu);
    rlconfig_set_int(menu.cfg, "raylib-libretro", "keySaveState", (int)menu.keySaveState);
    rlconfig_set_int(menu.cfg, "raylib-libretro", "keyLoadState", (int)menu.keyLoadState);
    rlconfig_set_int(menu.cfg, "raylib-libretro", "keyPrevSlot",  (int)menu.keyPrevSlot);
    rlconfig_set_int(menu.cfg, "raylib-libretro", "keyNextSlot",  (int)menu.keyNextSlot);
    rlconfig_set_int(menu.cfg, "raylib-libretro", "saveSlot",     menu.saveSlotIndex);
    rlconfig_set_int(menu.cfg, "raylib-libretro", "keyFullscreen", (int)menu.keyFullscreen);
    rlconfig_set_int(menu.cfg, "raylib-libretro", "keyPrevShader", (int)menu.keyPrevShader);
    rlconfig_set_int(menu.cfg, "raylib-libretro", "keyNextShader",  (int)menu.keyNextShader);
    rlconfig_set_int(menu.cfg, "raylib-libretro", "keyReset",       (int)menu.keyReset);
    rlconfig_set_int(menu.cfg, "raylib-libretro", "keyQuit",       (int)menu.keyQuit);
    rlconfig_set_int(menu.cfg, "raylib-libretro", "keyVolumeUp",    (int)menu.keyVolumeUp);
    rlconfig_set_int(menu.cfg, "raylib-libretro", "keyVolumeDown",  (int)menu.keyVolumeDown);
    rlconfig_set_int(menu.cfg, "raylib-libretro", "keyFastForward", (int)menu.keyFastForward);
    rlconfig_set_int(menu.cfg, "raylib-libretro", "fastForwardSpeed", menu.fastForwardSpeed);
    rlconfig_set_int(menu.cfg, "raylib-libretro", "keySlowMotion", (int)menu.keySlowMotion);
    rlconfig_set_int(menu.cfg, "raylib-libretro", "slowMotionSpeed", (int)(menu.slowMotionSpeed * 10.0f));
    rlconfig_set(menu.cfg, "raylib-libretro", "coreDirectory", LibretroResolveAbsoluteDirectory(LibretroCore.coreDirectory));
    rlconfig_set(menu.cfg, "raylib-libretro", "saveDirectory", LibretroResolveAbsoluteDirectory(LibretroCore.saveDirectory));
    rlconfig_set(menu.cfg, "raylib-libretro", "coreAssetsDirectory", LibretroResolveAbsoluteDirectory(LibretroCore.coreAssetsDirectory));
    rlconfig_set(menu.cfg, "raylib-libretro", "systemDirectory", LibretroResolveAbsoluteDirectory(LibretroCore.systemDirectory));
    rlconfig_set(menu.cfg, "raylib-libretro", "playlistsDirectory", LibretroResolveAbsoluteDirectory(LibretroCore.playlistsDirectory));
    rlconfig_set(menu.cfg, "raylib-libretro", "fileBrowserStartDirectory", LibretroResolveAbsoluteDirectory(LibretroCore.fileBrowserStartDirectory));
#endif
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
    if (!menu.cfg || LibretroCore.variableCount == 0) return false;
    const char *coreName = LibretroCore.libraryName;
    if (!coreName || !coreName[0]) return false;
    rlconfig_clear_section(menu.cfg, coreName);
    for (unsigned i = 0; i < LibretroCore.variableCount; i++) {
        rlconfig_set(menu.cfg, coreName, LibretroCore.variableKeys[i], LibretroCore.variableValues[i]);
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
    const char *coreName = LibretroCore.libraryName;
    if (!coreName || !coreName[0]) return false;
    int loaded = 0;
    for (unsigned i = 0; i < LibretroCore.variableCount; i++) {
        const char *val = rlconfig_get(menu.cfg, coreName, LibretroCore.variableKeys[i]);
        if (val && SetLibretroCoreOption(LibretroCore.variableKeys[i], val)) loaded++;
    }
    TraceLog(LOG_INFO, "LIBRETRO: Loaded %d core option(s) from %s", loaded, RAYLIB_LIBRETRO_CFG_FILE);
    return loaded > 0;
#else
    return false;
#endif
}

bool SaveLibretroAllSettings(void) {
#ifdef RAYLIB_LIBRETRO_CONFIG_H
    if (!menu.cfg) return false;
    LibretroMenuUpdateConfig();
    const char *coreName = LibretroCore.libraryName;
    if (coreName && coreName[0] && LibretroCore.variableCount > 0) {
        rlconfig_clear_section(menu.cfg, coreName);
        for (unsigned i = 0; i < LibretroCore.variableCount; i++) {
            rlconfig_set(menu.cfg, coreName, LibretroCore.variableKeys[i], LibretroCore.variableValues[i]);
        }
    }
    bool ok = rlconfig_save(menu.cfg, RAYLIB_LIBRETRO_CFG_FILE);
    TraceLog(LOG_INFO, "MENU: Saved all settings to %s", RAYLIB_LIBRETRO_CFG_FILE);
    return ok;
#else
    return false;
#endif
}

static bool LoadLibretroMenuSettings(void) {
#ifdef RAYLIB_LIBRETRO_CONFIG_H
    if (!menu.cfg) return false;

    nk_bool savedFullscreen = (nk_bool)rlconfig_get_int(menu.cfg, "raylib-libretro", "fullscreen", (int)menu.fullscreen);
    if (savedFullscreen != (nk_bool)IsWindowFullscreen()) ToggleFullscreen();
    menu.fullscreen = savedFullscreen;

    menu.shaderSelectedIndex = rlconfig_get_int(menu.cfg, "raylib-libretro", "shader", LIBRETRO_SHADER_NONE);
    if (menu.shaderSelectedIndex < 0 || menu.shaderSelectedIndex >= LIBRETRO_SHADER_TYPE_COUNT) {
        menu.shaderSelectedIndex = LIBRETRO_SHADER_NONE;
    }
    SetActiveLibretroShader((LibretroShaderType)menu.shaderSelectedIndex);

    menu.textureFilterIndex = rlconfig_get_int(menu.cfg, "raylib-libretro", "textureFilter", 0);
    if (menu.textureFilterIndex < 0 || menu.textureFilterIndex > TEXTURE_FILTER_ANISOTROPIC_16X)
        menu.textureFilterIndex = 0;
    LibretroCore.textureFilter = menu.textureFilterIndex;

    menu.themeSelectedIndex = rlconfig_get_int(menu.cfg, "raylib-libretro", "theme", LIBRETRO_MENU_STYLE_DRACULA);
    if (menu.themeSelectedIndex < 0 || menu.themeSelectedIndex >= LIBRETRO_MENU_STYLE_COUNT)
        menu.themeSelectedIndex = LIBRETRO_MENU_STYLE_DRACULA;
    SetLibretroMenuStyle((LibretroMenuStyle)menu.themeSelectedIndex);

    int volumeInt = (float)rlconfig_get_int(menu.cfg, "raylib-libretro", "volume", 100);
    if (volumeInt < 0) volumeInt = 0;
    if (volumeInt > 100) volumeInt = 100;
    menu.volumeSelected = (float)volumeInt / 100.0f;
    SetLibretroVolume(menu.volumeSelected);

    menu.rewindEnabled = rlconfig_get_int(menu.cfg, "raylib-libretro", "rewind", 0) > 0;

    menu.keyScreenshot = (nk_rune)rlconfig_get_int(menu.cfg, "raylib-libretro", "keyScreenshot", (int)menu.keyScreenshot);
    menu.keyRewind     = (nk_rune)rlconfig_get_int(menu.cfg, "raylib-libretro", "keyRewind",     (int)menu.keyRewind);
    menu.keyMenu       = (nk_rune)rlconfig_get_int(menu.cfg, "raylib-libretro", "keyMenu",       (int)menu.keyMenu);
    menu.keySaveState  = (nk_rune)rlconfig_get_int(menu.cfg, "raylib-libretro", "keySaveState",  (int)menu.keySaveState);
    menu.keyLoadState  = (nk_rune)rlconfig_get_int(menu.cfg, "raylib-libretro", "keyLoadState",  (int)menu.keyLoadState);
    menu.keyPrevSlot   = (nk_rune)rlconfig_get_int(menu.cfg, "raylib-libretro", "keyPrevSlot",   (int)menu.keyPrevSlot);
    menu.keyNextSlot   = (nk_rune)rlconfig_get_int(menu.cfg, "raylib-libretro", "keyNextSlot",   (int)menu.keyNextSlot);
    menu.saveSlotIndex = rlconfig_get_int(menu.cfg, "raylib-libretro", "saveSlot", 0);
    if (menu.saveSlotIndex < 0 || menu.saveSlotIndex > 9) menu.saveSlotIndex = 0;
    menu.keyFullscreen = (nk_rune)rlconfig_get_int(menu.cfg, "raylib-libretro", "keyFullscreen", (int)menu.keyFullscreen);
    menu.keyPrevShader = (nk_rune)rlconfig_get_int(menu.cfg, "raylib-libretro", "keyPrevShader", (int)menu.keyPrevShader);
    menu.keyNextShader  = (nk_rune)rlconfig_get_int(menu.cfg, "raylib-libretro", "keyNextShader",  (int)menu.keyNextShader);
    menu.keyReset       = (nk_rune)rlconfig_get_int(menu.cfg, "raylib-libretro", "keyReset",       (int)menu.keyReset);
    menu.keyQuit       = (nk_rune)rlconfig_get_int(menu.cfg, "raylib-libretro", "keyQuit",       (int)menu.keyQuit);
    SetExitKey(NuklearKeyToKeyboardKey(menu.keyQuit));
    menu.keyVolumeUp    = (nk_rune)rlconfig_get_int(menu.cfg, "raylib-libretro", "keyVolumeUp",    (int)menu.keyVolumeUp);
    menu.keyVolumeDown  = (nk_rune)rlconfig_get_int(menu.cfg, "raylib-libretro", "keyVolumeDown",  (int)menu.keyVolumeDown);
    menu.keyFastForward = (nk_rune)rlconfig_get_int(menu.cfg, "raylib-libretro", "keyFastForward", (int)menu.keyFastForward);
    menu.fastForwardSpeed = rlconfig_get_int(menu.cfg, "raylib-libretro", "fastForwardSpeed", menu.fastForwardSpeed);
    if (menu.fastForwardSpeed < 2) menu.fastForwardSpeed = 2;
    if (menu.fastForwardSpeed > 10) menu.fastForwardSpeed = 10;
    menu.keySlowMotion = (nk_rune)rlconfig_get_int(menu.cfg, "raylib-libretro", "keySlowMotion", (int)menu.keySlowMotion);
    int smSpeedInt = rlconfig_get_int(menu.cfg, "raylib-libretro", "slowMotionSpeed", (int)(menu.slowMotionSpeed * 10.0f));
    menu.slowMotionSpeed = (float)smSpeedInt / 10.0f;
    if (menu.slowMotionSpeed < 0.1f) menu.slowMotionSpeed = 0.1f;
    if (menu.slowMotionSpeed > 0.9f) menu.slowMotionSpeed = 0.9f;

    const char* coreDirectory = rlconfig_get(menu.cfg, "raylib-libretro", "coreDirectory");
    if (coreDirectory) TextCopy(LibretroCore.coreDirectory, coreDirectory);
    const char* saveDirectory = rlconfig_get(menu.cfg, "raylib-libretro", "saveDirectory");
    if (saveDirectory) TextCopy(LibretroCore.saveDirectory, saveDirectory);
    const char* coreAssetsDirectory = rlconfig_get(menu.cfg, "raylib-libretro", "coreAssetsDirectory");
    if (coreAssetsDirectory) TextCopy(LibretroCore.coreAssetsDirectory, coreAssetsDirectory);
    const char* systemDirectory = rlconfig_get(menu.cfg, "raylib-libretro", "gamesDirectory");
    if (systemDirectory) TextCopy(LibretroCore.systemDirectory, systemDirectory);
    const char* playlistsDirectory = rlconfig_get(menu.cfg, "raylib-libretro", "playlistsDirectory");
    if (playlistsDirectory) TextCopy(LibretroCore.playlistsDirectory, playlistsDirectory);
    const char* fileBrowserStartDirectory = rlconfig_get(menu.cfg, "raylib-libretro", "fileBrowserStartDirectory");
    if (fileBrowserStartDirectory) TextCopy(LibretroCore.fileBrowserStartDirectory, fileBrowserStartDirectory);

    TraceLog(LOG_INFO, "MENU: Loaded menu settings from %s", RAYLIB_LIBRETRO_CFG_FILE);
    return true;
#else
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
    menu.fullscreen = (nk_bool)IsWindowFullscreen();

    // Disable game-dependent items when no game is loaded.
    nk_bool gameReady = (nk_bool)IsLibretroGameReady();
    if (menu.optionsMenu) menu.optionsMenu->visible = LibretroCore.variableCount > 0 && IsLibretroReady();
    if (menu.saveStateButton) menu.saveStateButton->visible = gameReady;
    if (menu.loadStateButton) menu.loadStateButton->visible = gameReady;
    if (menu.resumeButton) menu.resumeButton->visible = gameReady;
    if (menu.closeGameButton) menu.closeGameButton->visible = gameReady;
}

void UpdateLibretroMenu(void) {
    if (menu.ctx == NULL) {
        return;
    }

    // Toggle the menu via gamepad or via the menu key when inactive.
    // When the menu is active, nk_console handles the menu key internally:
    // back-navigation in submenus, and MenuCloseOnBack at root level.
    if (IsGamepadButtonReleased(0, GAMEPAD_BUTTON_MIDDLE) || IsGamepadButtonReleased(1, GAMEPAD_BUTTON_MIDDLE) || IsGamepadButtonReleased(3, GAMEPAD_BUTTON_MIDDLE) || IsGamepadButtonReleased(4, GAMEPAD_BUTTON_MIDDLE)) {
        menu.active = !menu.active;
    } else if (!menu.active && IsKeyReleased(NuklearKeyToKeyboardKey(menu.keyMenu))) {
        menu.active = true;
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

    nk_gamepad_update(nk_console_get_gamepads(menu.console));
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
