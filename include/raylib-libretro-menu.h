/**********************************************************************************************
*
*   raylib-libretro-menu - Menu system for raylib-libretro
*
*   LICENSE: GPL-3.0-or-later
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
#include "raylib-libretro-config.h"

#include "raylib-libretro-styles.h"

// Maximum number of cores the "Select Core" picker offers for one game file.
#ifndef LIBRETRO_MAX_GAME_CORES
#define LIBRETRO_MAX_GAME_CORES 16
#endif

typedef enum LibretroMenuCombo {
    LIBRETRO_MENU_COMBO_NONE = 0,
    LIBRETRO_MENU_COMBO_SELECT_START,
    LIBRETRO_MENU_COMBO_L1_R1,
    LIBRETRO_MENU_COMBO_L2_R2,
    LIBRETRO_MENU_COMBO_L3_R3,
    LIBRETRO_MENU_COMBO_DOWN_SELECT,
    LIBRETRO_MENU_COMBO_COUNT,
} LibretroMenuCombo;

typedef struct LibretroMenuBinding {
    nk_rune key;
    enum nk_gamepad_button gamepad;
    const char* name;
    nk_rune defaultKey;
} LibretroMenuBinding;

typedef enum LibretroHotkey {
    LIBRETRO_HOTKEY_SCREENSHOT,
    LIBRETRO_HOTKEY_REWIND,
    LIBRETRO_HOTKEY_MENU,
    LIBRETRO_HOTKEY_SAVE_STATE,
    LIBRETRO_HOTKEY_LOAD_STATE,
    LIBRETRO_HOTKEY_PREV_SLOT,
    LIBRETRO_HOTKEY_NEXT_SLOT,
    LIBRETRO_HOTKEY_FULLSCREEN,
    LIBRETRO_HOTKEY_PREV_SHADER,
    LIBRETRO_HOTKEY_NEXT_SHADER,
    LIBRETRO_HOTKEY_RESET,
    LIBRETRO_HOTKEY_QUIT,
    LIBRETRO_HOTKEY_VOLUME_UP,
    LIBRETRO_HOTKEY_VOLUME_DOWN,
    LIBRETRO_HOTKEY_MUTE,
    LIBRETRO_HOTKEY_FAST_FORWARD,
    LIBRETRO_HOTKEY_SLOW_MOTION,
    LIBRETRO_HOTKEY_DISABLE_HOTKEYS,
    LIBRETRO_HOTKEY_COUNT,
} LibretroHotkey;

typedef struct {
    char path[256];
    char displayName[256];
    char coreName[256];
    char systemName[256];
    char license[64];
    char supportedExtensions[RLCONFIG_VALUE_MAX];
    bool supportsNoGame;
    bool needsFullpath;
} LibretroCoreInfo;

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
    nk_console* controllersMenu;          // "Controllers" submenu node
    int portDeviceIndex[16];              // per-port combobox selection index
    char portDeviceOptions[16][512];      // per-port pipe-separated device list (must outlive the combobox)
    char portDeviceLabel[16][16];         // per-port "Port N" label (must outlive the combobox)
    nk_console* loadGameWidget;
    nk_console* saveStateButton;
    nk_console* loadStateButton;
    nk_console* resumeButton;
    nk_console* resetGameButton;
    nk_console* cheatsMenuButton;
    //nk_console* closeGameButton;
    int shaderSelectedIndex;
    int themeSelectedIndex;
    bool rewindEnabled;
    int menuComboIndex;
    LibretroMenuBinding hotkeys[LIBRETRO_HOTKEY_COUNT];
    int saveSlotIndex;
    float fastForwardSpeed;
    float slowMotionSpeed;
    int optionSelectedIndices[LIBRETRO_MAX_CORE_VARIABLES];    // per-option combobox index
    nk_bool optionCheckboxValues[LIBRETRO_MAX_CORE_VARIABLES]; // per-option checkbox state for enabled/disabled options
    char loadGamePath[RAYLIB_LIBRETRO_VFS_MAX_PATH];
    bool touchControls;
    bool touchHapticsEnabled;
    nk_bool disableHotKeysActive; // Disable HotKeys: pass all keys to the core, suspend frontend hotkeys.
    int sramAutoSaveIndex; // combobox index for the SRAM auto-save interval (Off / 15s / 30s / 60s / 2min / 5min / 10min)
    float sramAutoSaveAccumulator; // seconds since the last successful auto-save
    int orientationIndex;                 // Android screen orientation: 0 = Landscape, 1 = Portrait, 2 = Auto
    nk_bool hideCursor;
    nk_bool lockCursor;
    char cheatBuffer[256];
    char cheatList[1024];
    unsigned cheatIndex;
    char aboutVersion[64];
    char aboutRenderer[48];
    char aboutResolution[64];
    char aboutCoreName[128];
    char aboutCoreVersion[64];
    char aboutCoreResolution[64];
    char aboutCorePixelFormat[48];
    char aboutCoreSampleRate[48];
    char aboutCoreAspectRatio[48];
    char aboutContent[160];
    char aboutExtensions[256];
    nk_rune keyboardControls[RETRO_DEVICE_ID_JOYPAD_R3 + 1];
    cvector(LibretroCoreInfo*) coreInfos;
    nk_console* corePickerMenu;
    char pendingGamePath[RAYLIB_LIBRETRO_VFS_MAX_PATH];
    char pendingCorePaths[LIBRETRO_MAX_GAME_CORES][512];
    char pendingCoreNames[LIBRETRO_MAX_GAME_CORES][128]; // stable picker button labels (GetFileNameWithoutExt reuses a shared static buffer)
    int pendingCoreCount;                 // >0 requests the core picker on the next menu update (deferred past the file browser's navigate_back)
    RLibretroConfig* cfg;                 // persistent config, owned for the lifetime of the menu
} LibretroMenu;

#if defined(__cplusplus)
extern "C" {
#endif

LibretroMenu* InitLibretroMenu(void);
void CloseLibretroMenu(void);
void UpdateLibretroMenu(void);
void DrawLibretroMenu(void);
void ShowLibretroMenu(void);      // Show the menu and apply cursor settings.
void HideLibretroMenu(void);      // Hide the menu and apply cursor settings.
void ToggleLibretroMenu(void);    // Toggle menu visibility.
bool IsLibretroMenuShown(void);   // Returns true if the menu is currently visible.
void BuildLibretroMenuOptions(LibretroMenu* menu); // Populate "Core Options" with comboboxes from the loaded core.
void BuildLibretroMenuControllers(LibretroMenu* menu); // Populate "Controllers" with per-port device comboboxes.
bool LoadLibretroCoreOptions(void);    // Apply saved core options from config to the loaded core.
bool SaveLibretroAllSettings(void);    // Save menu settings + core options in a single file write.
static bool SaveLibretroPortDevices(void); // Persist per-port device selections for the loaded core.
static bool LoadLibretroPortDevices(void); // Apply saved per-port device selections for the loaded core.
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

/**
 * Push the memory file system to IBDFS so that any writes under /userdata are captured.
 *
 * This is called at commit points, like when the user is leaving the settings menu, or saving the state.
 */
static void LibretroFlushPersistentStorage(void) {
    EM_ASM({
        FS.syncfs(false, function (err) {
            if (err) console.error('IDBFS sync failed:', err);
        });
    });
}

EM_JS(void, LibretroMenuEmscriptenOpenFilePicker, (void), {
    // Leverage an input element to pick a file.
    let input = document.createElement('input');
    input.type = 'file';
    input.onchange = function(e) {
        let file = e.target.files[0];
        if (!file) return;

        // Read in the file
        let reader = new FileReader();
        reader.onload = function() {
            // Save the file to the internal file system.
            let uint8Array = new Uint8Array(reader.result);
            FS.writeFile(file.name, uint8Array);

            // Grab the filename, and ask raylib-libreto to load it.
            let lengthBytes = lengthBytesUTF8(file.name) + 1;
            let ptr = _malloc(lengthBytes);
            stringToUTF8(file.name, ptr, lengthBytes);
            Module._LoadLibretroGameFromJS(ptr);
            _free(ptr);
        };
        reader.readAsArrayBuffer(file);
    };
    input.click();
});

static void LibretroMenuEmscriptenLoadGameClicked(nk_console* widget, void* user_data) {
    (void)user_data;
    (void)widget;
    TraceLog(LOG_INFO, "LIBRETRO: Opening the file dialog");
    LibretroMenuEmscriptenOpenFilePicker();
}
#else
#define LibretroFlushPersistentStorage() ((void)0)
#endif /* __EMSCRIPTEN__ */

#define RAYLIB_NUKLEAR_IMPLEMENTATION
#include "../vendor/raylib-nuklear/include/raylib-nuklear.h"

#include "rlgl.h" // rlGetVersion()

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

/**
 * The version is set by a define, defaults to DEV.
 */
#ifndef RAYLIB_LIBRETRO_VERSION
#define RAYLIB_LIBRETRO_VERSION "dev"
#endif

// Compile-time platform name shown in the About screen.
#if defined(__EMSCRIPTEN__) || defined(PLATFORM_WEB)
#define RAYLIB_LIBRETRO_PLATFORM "Web"
#elif defined(__ANDROID__) || defined(PLATFORM_ANDROID)
#define RAYLIB_LIBRETRO_PLATFORM "Android"
#elif defined(_WIN32)
#define RAYLIB_LIBRETRO_PLATFORM "Windows"
#elif defined(__APPLE__)
#define RAYLIB_LIBRETRO_PLATFORM "macOS"
#elif defined(__linux__)
#define RAYLIB_LIBRETRO_PLATFORM "Linux"
#else
#define RAYLIB_LIBRETRO_PLATFORM "Unknown"
#endif

/**
 * Number of increments a float settings slider divides its range into.
 */
#ifndef RAYLIB_LIBRETRO_MENU_SLIDER_STEPS
#define RAYLIB_LIBRETRO_MENU_SLIDER_STEPS 20.0f
#endif

/**
 * Step size that divides [min, max] into RAYLIB_LIBRETRO_MENU_SLIDER_STEPS steps.
 */
#define RAYLIB_LIBRETRO_MENU_SLIDER_STEP(min, max) (((max) - (min)) / RAYLIB_LIBRETRO_MENU_SLIDER_STEPS)

#if defined(__cplusplus)
extern "C" {
#endif

static LibretroMenu menu = {
    .themeSelectedIndex = LIBRETRO_MENU_STYLE_CATPPUCCIN_MOCHA,
    .vsync              = nk_true,
    .fastForwardSpeed   = 3.0f,
    .slowMotionSpeed    = 0.5f,
    .menuComboIndex     = LIBRETRO_MENU_COMBO_SELECT_START,
    .hotkeys = {
        [LIBRETRO_HOTKEY_SCREENSHOT] = {
            .name = "Screenshot",
            .defaultKey = NK_CONSOLE_KEY_F8,
            .key = NK_CONSOLE_KEY_F8,
            .gamepad = NK_GAMEPAD_BUTTON_INVALID
        },
        [LIBRETRO_HOTKEY_REWIND] = {
            .name = "Rewind",
            .defaultKey = (nk_rune)'R',
            .key = (nk_rune)'R',
            .gamepad = NK_GAMEPAD_BUTTON_INVALID
        },
        [LIBRETRO_HOTKEY_MENU] = {
            .name = "Menu",
            .defaultKey = NK_CONSOLE_KEY_ESCAPE,
            .key = NK_CONSOLE_KEY_ESCAPE,
            .gamepad = NK_GAMEPAD_BUTTON_INVALID
        },
        [LIBRETRO_HOTKEY_SAVE_STATE] = {
            .name = "Save State",
            .defaultKey = NK_CONSOLE_KEY_F2,
            .key = NK_CONSOLE_KEY_F2,
            .gamepad = NK_GAMEPAD_BUTTON_INVALID
        },
        [LIBRETRO_HOTKEY_LOAD_STATE] = {
            .name = "Load State",
            .defaultKey = NK_CONSOLE_KEY_F4,
            .key = NK_CONSOLE_KEY_F4,
            .gamepad = NK_GAMEPAD_BUTTON_INVALID
        },
        [LIBRETRO_HOTKEY_PREV_SLOT] = {
            .name = "Prev Slot",
            .defaultKey = NK_CONSOLE_KEY_NONE,
            .key = NK_CONSOLE_KEY_NONE,
            .gamepad = NK_GAMEPAD_BUTTON_INVALID
        },
        [LIBRETRO_HOTKEY_NEXT_SLOT] = {
            .name = "Next Slot",
            .defaultKey = NK_CONSOLE_KEY_NONE,
            .key = NK_CONSOLE_KEY_NONE,
            .gamepad = NK_GAMEPAD_BUTTON_INVALID
        },
        [LIBRETRO_HOTKEY_FULLSCREEN]   = {
            .name = "Fullscreen",
            .defaultKey = NK_CONSOLE_KEY_F11,
            .key = NK_CONSOLE_KEY_F11,
            .gamepad = NK_GAMEPAD_BUTTON_INVALID
        },
        [LIBRETRO_HOTKEY_PREV_SHADER]  = {
            .name = "Previous Shader",
            .defaultKey = NK_CONSOLE_KEY_F9,
            .key = NK_CONSOLE_KEY_F9,
            .gamepad = NK_GAMEPAD_BUTTON_INVALID
        },
        [LIBRETRO_HOTKEY_NEXT_SHADER] = {
            .name = "Next Shader",
            .defaultKey = NK_CONSOLE_KEY_F10,
            .key = NK_CONSOLE_KEY_F10,
            .gamepad = NK_GAMEPAD_BUTTON_INVALID
        },
        [LIBRETRO_HOTKEY_RESET] = {
            .name = "Reset",
            .defaultKey = NK_CONSOLE_KEY_NONE,
            .key = NK_CONSOLE_KEY_NONE,
            .gamepad = NK_GAMEPAD_BUTTON_INVALID
        },
        [LIBRETRO_HOTKEY_QUIT] = {
            .name = "Quit",
            .defaultKey = NK_CONSOLE_KEY_NONE,
            .key = NK_CONSOLE_KEY_NONE,
            .gamepad = NK_GAMEPAD_BUTTON_INVALID
        },
        [LIBRETRO_HOTKEY_VOLUME_UP] = {
            .name = "Volume Up",
            .defaultKey = (nk_rune)'=',
            .key = (nk_rune)'=',
            .gamepad = NK_GAMEPAD_BUTTON_INVALID
        },
        [LIBRETRO_HOTKEY_VOLUME_DOWN]  = {
            .name = "Volume Down",
            .defaultKey = (nk_rune)'-',
            .key = (nk_rune)'-',
            .gamepad = NK_GAMEPAD_BUTTON_INVALID
        },
        [LIBRETRO_HOTKEY_MUTE] = {
            .name = "Mute",
            .defaultKey = (nk_rune)'M',
            .key = (nk_rune)'M',
            .gamepad = NK_GAMEPAD_BUTTON_INVALID
        },
        [LIBRETRO_HOTKEY_FAST_FORWARD] = {
            .name = "Fast Forward",
            .defaultKey = (nk_rune)'F',
            .key = (nk_rune)'F',
            .gamepad = NK_GAMEPAD_BUTTON_INVALID
        },
        [LIBRETRO_HOTKEY_SLOW_MOTION]  = {
            .name = "Slow Motion",
            .defaultKey = (nk_rune)'G',
            .key = (nk_rune)'G',
            .gamepad = NK_GAMEPAD_BUTTON_INVALID
        },
        [LIBRETRO_HOTKEY_DISABLE_HOTKEYS] = {
            .name = "Disable Hot Keys",
            .defaultKey = NK_CONSOLE_KEY_F12,
            .key = NK_CONSOLE_KEY_F12,
            .gamepad = NK_GAMEPAD_BUTTON_INVALID
        },
    },
};

static bool IsLibretroMenuReady(void);
static bool SaveLibretroMenuSettings(void);
static bool SaveLibretroCoreOptions(void);
static bool LoadLibretroMenuSettings(void);
static void UpdateLibretroMenuVisibility(void);
static Font GetLibretroMenuFont(void) {
    return menu.font;
}

/**
 * Ports a given input nk_rune to a raylib KeyboardKey.
 */
static KeyboardKey LibretroHotkeyToKeyboardKey(nk_rune key) {
    enum nk_keys special = nk_console_input_rune_to_keys(key);
    return NuklearKeyToKeyboardKey(special != NK_KEY_NONE ? (nk_rune)special : key);
}

/**
 * Inverse of LibretroHotkeyToKeyboardKey(): encode a raylib KeyboardKey as the
 * NK_CONSOLE_KEY_* rune the nk_console key widgets display and capture. Used to
 * seed each binding row from the canonical LIBRETRO.keyboardPlayer1.
 */
static nk_rune LibretroKeyboardKeyToHotkey(int key) {
    if (key <= KEY_NULL) {
        return NK_CONSOLE_KEY_NONE;
    }
    if (key == KEY_RIGHT_SHIFT) { // Left shift and right shift are the same.
        return NK_CONSOLE_KEY_SHIFT;
    }
    nk_rune nkKey = KeyboardKeyToNuklearKey((KeyboardKey)key);
    // Encode recognised special keys in the NK_CONSOLE_KEY_* range. A printable
    // whose code happens to land in the nk_keys range (e.g. Space = 32) won't
    // round-trip, so it falls through unchanged as its codepoint.
    if (nkKey > NK_KEY_NONE && nkKey < (nk_rune)NK_KEY_MAX &&
            NuklearKeyToKeyboardKey(nkKey) == (KeyboardKey)key) {
        return nk_console_input_rune_from_keys((enum nk_keys)nkKey);
    }
    return nkKey;
}

static void LibretroMenuKeyBindingChanged(nk_console* widget, void* user_data) {
    NK_UNUSED(widget);
    int btn = (int)(intptr_t)user_data;
    LIBRETRO.keyboardPlayer1[btn] = (int)LibretroHotkeyToKeyboardKey(menu.keyboardControls[btn]);
}

static nk_console* LibretroMenuKeyConsole(nk_console* parent, const char* label, int btn) {
    menu.keyboardControls[btn] = LibretroKeyboardKeyToHotkey(LIBRETRO.keyboardPlayer1[btn]);
    nk_console* widget = nk_console_key(parent, label, &menu.keyboardControls[btn]);
    nk_console_add_event_handler(widget, NK_CONSOLE_EVENT_CHANGED,
        &LibretroMenuKeyBindingChanged, (void*)(intptr_t)btn, NULL);
    return widget;
}

static void LibretroMenuSettingChanged(nk_console* widget, void* user_data) {
    (void)widget;
    (void)user_data;
    SetActiveLibretroShader((LibretroShaderType)menu.shaderSelectedIndex);
    SetLibretroMenuStyle((LibretroMenuStyle)menu.themeSelectedIndex);
    SetLibretroVolume(LIBRETRO.volume);
    SetExitKey(LibretroHotkeyToKeyboardKey(menu.hotkeys[LIBRETRO_HOTKEY_QUIT].key));
}

static void LibretroMenuTextureFilterChanged(nk_console* widget, void* user_data) {
    NK_UNUSED(widget);
    NK_UNUSED(user_data);
    InitLibretroVideo();
}

static LibretroMenu* GetLibretroMenu(void) {
    return &menu;
}

static bool IsLibretroMenuReady(void) {
    return menu.ctx != NULL;
}

#define RAYLIB_LIBRETRO_STYLES_IMPLEMENTATION
#include "raylib-libretro-styles.h"

/**
 * Make the FPS combobox index to a SetTargetFPS() call. Auto uses the monitor refresh rate.
 */
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

/**
 * Apply the mouse cursor show/hide settings.
 */
static void LibretroMenuApplyCursorSettings(void) {
    if (menu.active) {
        if (menu.hideCursor) ShowCursor();
        if (menu.lockCursor) EnableCursor();
        return;
    }

    if (menu.hideCursor) HideCursor();
    if (menu.lockCursor) DisableCursor();
}

void ShowLibretroMenu(void) {
    if (!menu.active) {
        menu.active = true;
        LibretroMenuApplyCursorSettings();
    }
}

void HideLibretroMenu(void) {
    if (menu.active) {
        menu.active = false;
        LibretroMenuApplyCursorSettings();
    }
}

void ToggleLibretroMenu(void) {
    if (menu.active) HideLibretroMenu(); else ShowLibretroMenu();
}

bool IsLibretroMenuShown(void) {
    return menu.active;
}

/**
 * Applies the video VSYNC/FPS settings.
 */
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

static bool LibretroMenuComboTriggered(int gamepad) {
    switch (menu.menuComboIndex) {
        case LIBRETRO_MENU_COMBO_SELECT_START:
            return (IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_MIDDLE_LEFT) && IsGamepadButtonReleased(gamepad, GAMEPAD_BUTTON_MIDDLE_RIGHT))
                || (IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_MIDDLE_RIGHT) && IsGamepadButtonReleased(gamepad, GAMEPAD_BUTTON_MIDDLE_LEFT));
        case LIBRETRO_MENU_COMBO_L1_R1:
            return (IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_LEFT_TRIGGER_1) && IsGamepadButtonReleased(gamepad, GAMEPAD_BUTTON_RIGHT_TRIGGER_1))
                || (IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_RIGHT_TRIGGER_1) && IsGamepadButtonReleased(gamepad, GAMEPAD_BUTTON_LEFT_TRIGGER_1));
        case LIBRETRO_MENU_COMBO_L2_R2:
            return (IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_LEFT_TRIGGER_2) && IsGamepadButtonReleased(gamepad, GAMEPAD_BUTTON_RIGHT_TRIGGER_2))
                || (IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_RIGHT_TRIGGER_2) && IsGamepadButtonReleased(gamepad, GAMEPAD_BUTTON_LEFT_TRIGGER_2));
        case LIBRETRO_MENU_COMBO_L3_R3:
            return (IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_LEFT_THUMB) && IsGamepadButtonReleased(gamepad, GAMEPAD_BUTTON_RIGHT_THUMB))
                || (IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_RIGHT_THUMB) && IsGamepadButtonReleased(gamepad, GAMEPAD_BUTTON_LEFT_THUMB));
        case LIBRETRO_MENU_COMBO_DOWN_SELECT:
            return (IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_LEFT_FACE_DOWN) && IsGamepadButtonReleased(gamepad, GAMEPAD_BUTTON_MIDDLE_LEFT))
                || (IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_MIDDLE_LEFT) && IsGamepadButtonReleased(gamepad, GAMEPAD_BUTTON_LEFT_FACE_DOWN));
        default: return false;
    }
}

#ifdef __EMSCRIPTEN__
// requestFullscreen() must originate from a user-gesture event handler.
// Raylib buffers input between frames, so by the time this callback fires
// we are inside requestAnimationFrame — the gesture context is gone.
// We defer by registering a one-shot listener for the next gesture event.
// emscripten_request_fullscreen's deferUntilInEventHandler only hooks
// mousedown, which is never synthesised on mobile when touch-action:none
// is set on the canvas — so we register for touch/pointer events too.
EM_JS(void, LibretroMenuRequestFullscreen, (int enter), {
    if (!enter) {
        if (document.exitFullscreen) document.exitFullscreen();
        return;
    }
    var types = ['mousedown', 'touchend', 'pointerup'];
    function onGesture() {
        types.forEach(function(t) { document.removeEventListener(t, onGesture, true); });
        var el = document.documentElement;
        if (el.requestFullscreen) {
            el.requestFullscreen({navigationUI: 'hide'}).catch(function(){});
        }
    }
    types.forEach(function(t) { document.addEventListener(t, onGesture, true); });
});
#endif

static void LibretroMenuFullscreenChanged(nk_console* widget, void* user_data) {
    (void)widget;
    (void)user_data;
#ifdef __EMSCRIPTEN__
    LibretroMenuRequestFullscreen(menu.fullscreen ? 1 : 0);
#else
    ToggleFullscreen();
    menu.fullscreen = IsWindowFullscreen();
#endif
}

/**
 * Human-readable name for raylib's active rendering backend (rlgl.h enum).
 */
static const char* LibretroMenuRendererName(void) {
    switch (rlGetVersion()) {
        case RL_OPENGL_11:       return "OpenGL 1.1";
        case RL_OPENGL_21:       return "OpenGL 2.1";
        case RL_OPENGL_33:       return "OpenGL 3.3";
        case RL_OPENGL_43:       return "OpenGL 4.3";
        case RL_OPENGL_ES_20:    return "OpenGL ES 2.0";
        case RL_OPENGL_ES_30:    return "OpenGL ES 3.0";
        case RL_OPENGL_SOFTWARE: return "Software";
        default:                 return "Unknown";
    }
}

/**
 * Human-readable name for the core's framebuffer pixel format.
 */
static const char* LibretroMenuPixelFormatName(enum retro_pixel_format fmt) {
    switch (fmt) {
        case RETRO_PIXEL_FORMAT_0RGB1555: return "0RGB1555";
        case RETRO_PIXEL_FORMAT_XRGB8888: return "XRGB8888";
        case RETRO_PIXEL_FORMAT_RGB565:   return "RGB565";
        default:                          return "Unknown";
    }
}

static void LibretroMenuUpdateAbout(void) {
    // Frontend / runtime.
    TextCopy(menu.aboutVersion,  TextFormat("raylib-libretro: %s", RAYLIB_LIBRETRO_VERSION));
    TextCopy(menu.aboutRenderer, TextFormat("Renderer:        %s", LibretroMenuRendererName()));
    TextCopy(menu.aboutResolution, TextFormat("Window:          %dx%d",
        GetScreenWidth(), GetScreenHeight()));

    // Core.
    const char* coreName = GetLibretroName();
    const char* coreVer  = GetLibretroVersion();
    if (coreName && coreName[0]) {
        TextCopy(menu.aboutCoreName,    TextFormat("Core:            %s", coreName));
        TextCopy(menu.aboutCoreVersion, TextFormat("Core Version:    %s", coreVer ? coreVer : ""));
        TextCopy(menu.aboutCoreResolution, TextFormat("Core Size:       %ux%u",
            GetLibretroWidth(), GetLibretroHeight()));
        TextCopy(menu.aboutCorePixelFormat, TextFormat("Pixel Format:    %s",
            LibretroMenuPixelFormatName(LIBRETRO.core.pixelFormat)));
        TextCopy(menu.aboutCoreSampleRate, TextFormat("Sample Rate:     %g Hz",
            LIBRETRO.core.sampleRate));
        float aspect = GetLibretroAspectRatio();
        if (aspect > 0.0f) {
            TextCopy(menu.aboutCoreAspectRatio, TextFormat("Aspect Ratio:    %.3f", aspect));
        } else {
            TextCopy(menu.aboutCoreAspectRatio, "Aspect Ratio:    (auto)");
        }
        const char* exts = GetLibretroValidExtensions();
        TextCopy(menu.aboutExtensions, TextFormat("Extensions:      %s",
            (exts && exts[0]) ? exts : "(any)"));
    } else {
        TextCopy(menu.aboutCoreName, "Core:            (none)");
        menu.aboutCoreVersion[0] = '\0';
        menu.aboutCoreResolution[0] = '\0';
        menu.aboutCorePixelFormat[0] = '\0';
        menu.aboutCoreSampleRate[0] = '\0';
        menu.aboutCoreAspectRatio[0] = '\0';
        menu.aboutExtensions[0] = '\0';
    }

    // Content.
    if (LIBRETRO.core.contentPath[0] != '\0') {
        TextCopy(menu.aboutContent, TextFormat("Content:         %s",
            GetFileName(LIBRETRO.core.contentPath)));
    } else {
        menu.aboutContent[0] = '\0';
    }
}

static void LibretroMenuAboutOpened(nk_console* widget, void* user_data) {
    (void)user_data;
    LibretroMenuUpdateAbout();
    // Registering a CLICKED handler suppresses nk_console_button_render()'s
    // default "navigate into submenu" behavior, so do it here after refreshing.
    nk_console_set_active_parent(widget);
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
        HideLibretroMenu();
    }
    else {
        SetLibretroMessage("State Saved Failed", 2.0);
    }
}

static void MenuResumeClicked(nk_console* widget, void* user_data) {
    NK_UNUSED(widget);
    NK_UNUSED(user_data);
    if (IsLibretroGameReady()) {
        HideLibretroMenu();
    }
}

static void MenuResetGameClicked(nk_console* widget, void* user_data) {
    NK_UNUSED(widget);
    NK_UNUSED(user_data);
    if (!IsLibretroGameReady()) return;
    ResetLibretro();
    HideLibretroMenu();
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
        nk_console_show_message(menu.console, TextFormat("Cheat %u applied", menu.cheatIndex + 1));
        menu.cheatIndex++;
    } else {
        nk_console_show_message(menu.console, "Failed to apply cheat");
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
        nk_console_show_message(menu.console, "Cheats have been reset");
    }
}

/**
 * Fired when the user navigates back from Settings or Core Options.
 *
 * This is the natural commit point: anything the user just touched in those submenus is now part of the loaded state, so write the cfg out and flush IDBFS so the change survives a page reload / tab close on the web.
 */
static void MenuCommitSettings(nk_console* widget, void* user_data) {
    NK_UNUSED(widget);
    NK_UNUSED(user_data);
    SaveLibretroAllSettings();
}

static void ScanLibretroCoreDirectory(void);

static void LibretroMenuEnsureSaveDir(void) {
    if (LIBRETRO.saveDirectory[0] != '\0' && !DirectoryExists(LIBRETRO.saveDirectory)) {
        MakeDirectory(LIBRETRO.saveDirectory);
    }
}

static void MenuDirChanged(nk_console* widget, void* user_data) {
    LibretroMenuSettingChanged(widget, user_data);
    LibretroMenuEnsureSaveDir();
}

static void MenuCoreDirChanged(nk_console* widget, void* user_data) {
    MenuDirChanged(widget, user_data);
    ScanLibretroCoreDirectory();
}

static void MenuContentDirChanged(nk_console* widget, void* user_data) {
    MenuDirChanged(widget, user_data);
#ifndef __EMSCRIPTEN__
    if (menu.loadGameWidget && LIBRETRO.fileBrowserStartDirectory[0] != '\0') {
        nk_console_file_set_directory(menu.loadGameWidget, LIBRETRO.fileBrowserStartDirectory);
    }
#endif
}

static bool IsLibretroCoreFile(const char* path) {
    const char* ext = GetFileExtension(path);
    return TextIsEqual(ext, ".so") || TextIsEqual(ext, ".dll") || TextIsEqual(ext, ".dylib") || TextIsEqual(ext, ".wasm");
}

static void FreeLibretroCoreInfos(void) {
    for (size_t i = 0; i < cvector_size(menu.coreInfos); i++) {
        MemFree(menu.coreInfos[i]);
    }
    cvector_free(menu.coreInfos);
    menu.coreInfos = NULL;
}

/**
 * Scan the core directory and build the menu's core list.
 *
 * Each core's metadata (name, supported extensions, needs_fullpath,
 * supports_no_game) is read from its sibling `<core>.info` file — fast, no
 * dlopen. A core binary with no matching `.info` is probed directly via
 * PeekLibretroCoreInfo() so it still appears rather than vanishing silently.
 *
 * IMPORTANT: the `.info` values are PRE-LOAD HINTS for menu filtering only,
 * never a hard gate. The authoritative values come from the core's runtime
 * retro_get_system_info() / CONTENT_INFO_OVERRIDE once it is loaded, and the
 * two can diverge — e.g. genesis_plus_gx declares needs_fullpath="true"
 * globally (for SegaCD) yet loads cartridge ROMs from memory. Filtering a core
 * out on a coarse `.info` flag is exactly how zipped Genesis games got wrongly
 * rejected; let the loader make the final per-content decision.
 */
static void ScanLibretroCoreDirectory(void) {
    const char* dir = LIBRETRO.coreDirectory;
    FreeLibretroCoreInfos();
    if (!dir || !dir[0] || !DirectoryExists(dir)) return;

    RLibretroConfig* infoCfg = rlconfig_load(NULL);
    FilePathList files = LoadDirectoryFiles(dir);
    for (unsigned int i = 0; i < files.count; i++) {
        if (!TextIsEqual(GetFileExtension(files.paths[i]), ".info")) continue;

        char infoBase[RAYLIB_LIBRETRO_VFS_MAX_PATH];
        TextCopy(infoBase, GetFileNameWithoutExt(files.paths[i]));
        const char* coreFile = NULL;
        for (unsigned int j = 0; j < files.count; j++) {
            if (!IsLibretroCoreFile(files.paths[j])) continue;
            char coreBase[RAYLIB_LIBRETRO_VFS_MAX_PATH];
            TextCopy(coreBase, GetFileNameWithoutExt(files.paths[j]));
            if (TextIsEqual(coreBase, infoBase) ||
                TextIsEqual(coreBase, TextFormat("%s_android", infoBase))) {
                coreFile = GetFileName(files.paths[j]);
                break;
            }
        }
        if (!coreFile) continue;

        rlconfig_load_info(infoCfg, infoBase, files.paths[i]);
        const char* coreName = rlconfig_get(infoCfg, infoBase, "corename");
        const char* exts = rlconfig_get(infoCfg, infoBase, "supported_extensions");
        if (!coreName || !coreName[0] || !exts || !exts[0]) {
            rlconfig_clear_section(infoCfg, infoBase);
            continue;
        }

        LibretroCoreInfo* info = (LibretroCoreInfo*)MemAlloc(sizeof(LibretroCoreInfo));
        TextCopy(info->path, coreFile);
        TextCopy(info->supportedExtensions, exts);
        TextCopy(info->coreName, coreName);
        const char* displayName = rlconfig_get(infoCfg, infoBase, "display_name");
        TextCopy(info->displayName, (displayName && displayName[0]) ? displayName : coreName);
        const char* systemName = rlconfig_get(infoCfg, infoBase, "systemname");
        TextCopy(info->systemName, (systemName && systemName[0]) ? systemName : "");
        const char* license = rlconfig_get(infoCfg, infoBase, "license");
        TextCopy(info->license, (license && license[0]) ? license : "");
        const char* noGame = rlconfig_get(infoCfg, infoBase, "supports_no_game");
        info->supportsNoGame = noGame && TextIsEqual(noGame, "true");
        const char* fullpath = rlconfig_get(infoCfg, infoBase, "needs_fullpath");
        info->needsFullpath = fullpath && TextIsEqual(fullpath, "true");

        TraceLog(LOG_INFO, "LIBRETRO: Found %s (%s)", info->displayName, exts);
        cvector_push_back(menu.coreInfos, info);
        rlconfig_clear_section(infoCfg, infoBase);
    }

    // Fallback: a core binary with no sibling .info would otherwise be invisible
    // (the loop above is .info-driven). Probe such cores directly so they still
    // appear. Guarded on IsLibretroReady(): the probe's CloseLibretro() wipes
    // LIBRETRO.core, which would tear down a game in progress — and this scan
    // also runs from MenuCoreDirChanged while in-game. supports_no_game can't be
    // known without a full init, so it defaults to false for probed cores.
    if (!IsLibretroReady()) {
        for (unsigned int i = 0; i < files.count; i++) {
            if (!IsLibretroCoreFile(files.paths[i])) continue;
            const char* coreFile = GetFileName(files.paths[i]);

            bool hasInfo = false;
            for (int k = 0; k < (int)cvector_size(menu.coreInfos); k++) {
                if (TextIsEqual(menu.coreInfos[k]->path, coreFile)) { hasInfo = true; break; }
            }
            if (hasInfo) continue;

            if (PeekLibretroCoreInfo(files.paths[i])) {
                const char* name = GetLibretroName();
                const char* exts = GetLibretroValidExtensions();
                if (name[0] && exts[0]) {
                    LibretroCoreInfo* info = (LibretroCoreInfo*)MemAlloc(sizeof(LibretroCoreInfo));
                    TextCopy(info->path, coreFile);
                    TextCopy(info->supportedExtensions, exts);
                    TextCopy(info->coreName, name);
                    TextCopy(info->displayName, name);
                    info->systemName[0] = '\0';
                    info->license[0] = '\0';
                    info->needsFullpath = GetLibretroNeedFullpath(NULL, NULL);
                    info->supportsNoGame = false;
                    TraceLog(LOG_INFO, "LIBRETRO: Probed %s (%s) [no .info]", name, exts);
                    cvector_push_back(menu.coreInfos, info);
                }
            }
            // Always tear down: PeekLibretroCoreInfo leaves the dylib open on
            // success, and may leave it open on a mid-way symbol-load failure.
            CloseLibretro();
        }
    }

    UnloadDirectoryFiles(files);
    rlconfig_free(infoCfg);
    TraceLog(LOG_INFO, "LIBRETRO: Found %d cores in %s", (int)cvector_size(menu.coreInfos), dir);
}

/**
 * Lower-cased file extension of @p path (no leading dot) into @p out (>= 32 bytes).
 * @return false when @p path has no extension.
 */
static bool LibretroExtLower(const char* path, char* out) {
    const char* ext = GetFileExtension(path);
    if (!ext || !ext[0]) return false;
    if (ext[0] == '.') ext++;
    TextCopy(out, TextToLower(ext));
    return true;
}

/**
 * @return true when cached core @p index lists extension @p extLower
 *         (lower-case, no dot) in its valid_extensions.
 */
static bool LibretroCoreSupportsExt(int index, const char* extLower) {
    if (index < 0 || index >= (int)cvector_size(menu.coreInfos)) return false;
    const char* exts = menu.coreInfos[index]->supportedExtensions;
    if (!exts[0]) return false;
    int extCount = 0;
    char** extList = TextSplit(exts, '|', &extCount);
    for (int j = 0; j < extCount; j++) {
        if (TextIsEqual(extList[j], extLower)) return true;
    }
    return false;
}

/**
 * Display name for cached core @p index: its cached human-readable name, or the
 * de-suffixed file name as a fallback. The returned pointer is only valid until
 * the next raylib text-buffer call, so callers must copy it immediately.
 */
static const char* LibretroCoreDisplayName(int index) {
    if (index < 0 || index >= (int)cvector_size(menu.coreInfos)) return "";
    LibretroCoreInfo* info = menu.coreInfos[index];
    if (info->displayName[0]) return info->displayName;
    if (info->coreName[0]) return info->coreName;
    return info->path[0] ? TextReplace(GetFileNameWithoutExt(info->path), "_libretro", "") : "";
}

/**
 * Peek inside a .zip and return an inner-file extension that any cached core
 * advertises in its valid_extensions.
 *
 * Disc metadata (.m3u, .cue) is preferred over other entries so the core list
 * matches what LibretroPhysFSPickFileInZip will actually hand to the core;
 * otherwise the first matching entry wins.
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
    if (!IsPhysFSReady() && !InitLibretroPhysFS()) return false;
    // Mount at a scratch point to avoid clobbering any /game mount in flight.
    if (!MountPhysFS(zipPath, "/peek")) return false;

    FilePathList entries = LoadDirectoryFilesFromPhysFSEx("/peek", NULL, true);
    bool found = false;

    // Prefer disc metadata (.m3u, .cue) so the core list matches what the
    // loader will actually hand to the core.
    for (unsigned int e = 0; e < entries.count && !found; e++) {
        if (!IsFileExtension(entries.paths[e], ".m3u;.cue")) continue;
        char innerLower[32];
        if (!LibretroExtLower(entries.paths[e], innerLower)) continue;
        for (int i = 0; i < (int)cvector_size(menu.coreInfos); i++) {
            if (LibretroCoreSupportsExt(i, innerLower)) {
                TextCopy(outExt, innerLower);
                found = true;
                break;
            }
        }
    }

    for (unsigned int e = 0; e < entries.count && !found; e++) {
        char innerLower[32];
        if (!LibretroExtLower(entries.paths[e], innerLower)) continue;
        for (int i = 0; i < (int)cvector_size(menu.coreInfos); i++) {
            if (LibretroCoreSupportsExt(i, innerLower)) {
                TextCopy(outExt, innerLower);
                found = true;
                break;
            }
        }
    }

    UnloadDirectoryFiles(entries);
    UnmountPhysFS(zipPath);
    return found;
}

static int FindCoresForGame(const char* gamePath, char paths[][512], char names[][128], int maxCount) {
    if (!paths || !names || maxCount <= 0) return 0;

    bool noGame = !gamePath || !gamePath[0];
    char gameExtLower[32] = {0};
    bool isZip = false;

    if (!noGame) {
        if (!LibretroExtLower(gamePath, gameExtLower)) return 0;
        isZip = TextIsEqual(gameExtLower, "zip");
        if (isZip) {
            char zipInner[32];
            if (FindZipInnerExtensionForCore(gamePath, zipInner)) {
                TextCopy(gameExtLower, zipInner);
            }
        }
    }

    int found = 0;
    for (int i = 0; i < (int)cvector_size(menu.coreInfos) && found < maxCount; i++) {
        LibretroCoreInfo* ci = menu.coreInfos[i];
        if (noGame) {
            if (!ci->supportsNoGame) continue;
        } else {
            if (!LibretroCoreSupportsExt(i, gameExtLower)) continue;
            // Some cores require "needs_fullpath", like for CD. We allow attempting to load directly from the .zip for now.
        }

        TextCopy(names[found], LibretroCoreDisplayName(i));
        if (!ci->path[0]) continue;
        TextCopy(paths[found], TextFormat("%s/%s", LIBRETRO.coreDirectory, ci->path));
        found++;
    }
    return found;
}

static bool MenuInitCore(const char* corePath) {
    if (!InitLibretro(corePath)) return false;
    LoadLibretroCoreOptions();
    SetLibretroVolume(LIBRETRO.volume);
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

/**
 * Return the number of seconds for the given Auto Save SRAM interval.
 */
static int MenuSRAMAutoSaveIntervalSeconds(int index) {
    switch (index) {
        case 1: return 15;
        case 2: return 30;
        case 3: return 60;
        case 4: return 120;
        case 5: return 300;
        case 6: return 600;
        case 0:
        default: return 0;
    }
}

// Accumulate frame time and flush SRAM to disk when the configured interval
// elapses. Safe to call every frame; no-ops when auto-save is off, the menu is
// open (game paused), or no game is loaded.
static void MenuTickSRAMAutoSave(void) {
    if (menu.sramAutoSaveIndex <= 0 || menu.active || !IsLibretroGameReady()) {
        return;
    }

    menu.sramAutoSaveAccumulator += GetFrameTime();
    if (menu.sramAutoSaveAccumulator >= MenuSRAMAutoSaveIntervalSeconds(menu.sramAutoSaveIndex)) {
        menu.sramAutoSaveAccumulator = 0.0f;
        MenuSaveGameSRAM();
    }
}

static bool MenuDoLoadGame(const char* gamePath) {
    if (!LoadLibretroGameFromPhysFS(gamePath)) {
        if (IsLibretroGameRequired()) {
            nk_console_show_message(menu.console, "Failed to load game");
            return false;
        }
        if (!LoadLibretroGame(NULL)) {
            nk_console_show_message(menu.console, "Failed to load core");
            return false;
        }
    }
    BuildLibretroMenuOptions(&menu);
    LoadLibretroPortDevices();
    BuildLibretroMenuControllers(&menu);
    HideLibretroMenu();
    MenuLoadGameSRAM();
    return true;
}

// Draw a one-frame "Loading <game>" splash, shown right before a (potentially
// slow) core init + game load so there is feedback during the stall.
/** */
static void MenuDrawLoadingScreen(const char* gamePath) {
    BeginDrawing();
        // Colors
        Color background = (Color){ 17, 17, 27, 255 };
        Color foreground = (Color){ 245, 224, 220, 255 };
        if (menu.ctx != NULL) {
            background = NuklearColorToColor(menu.ctx->style.window.background);
            foreground = NuklearColorToColor(menu.ctx->style.button.text_hover);
        }

        // Background
        ClearBackground(background);

        // Text
        const char* loadingText = "Loading";
        if (gamePath != NULL) {
            loadingText = TextFormat("Loading %s", GetFileNameWithoutExt(gamePath));
        }
        Font font = IsFontValid(menu.font) ? menu.font : GetFontDefault();
        float fontSize = (float)font.baseSize;
        Vector2 textSize = MeasureTextEx(font, loadingText, fontSize, 1);

        // Write it.
        DrawTextEx(font, loadingText,
            (Vector2){ (GetScreenWidth() - textSize.x) / 2, (GetScreenHeight() - textSize.y) / 2 },
            fontSize, 1, foreground);
    EndDrawing();
}

/**
 * Initialize the given core, and load the associated game.
 */
static bool MenuLoadCoreAndGame(const char* corePath, const char* gamePath) {
    MenuDrawLoadingScreen(gamePath);
    if (!MenuInitCore(corePath)) {
        nk_console_show_message(menu.console, "Failed to load core");
        return false;
    }
    return MenuDoLoadGame(gamePath);
}

/**
 * Enter a console submenu, and focus its first selectable child.
 */
static void MenuNavigateInto(nk_console* parent) {
    nk_console_set_active_parent(parent);
    nk_console_set_active_widget(nk_console_find_first_selectable(parent));
}

static void MenuCorePickerLoad(nk_console* widget, void* user_data) {
    (void)widget;
    const char* gamePath = menu.pendingGamePath[0] ? menu.pendingGamePath : NULL;
    if (!MenuLoadCoreAndGame((const char*)user_data, gamePath)) {
        ShowLibretroMenu();
        return;
    }
    // Game loaded: leave the picker behind so reopening the menu lands on the
    // top-level main menu rather than the (now hidden) Select Core submenu.
    MenuNavigateInto(menu.console);
}

/**
 * Build and open the "Select Core" picker for the pending game.
 *
 * Registered as a POST_RENDER_ONCE handler (see MenuLoadGame): switching the
 * active parent updates the Nuklear window scroll, so it must run while a
 * window is current, and after the file browser's navigate_back() that would
 * otherwise pull focus back to the main menu.
 */
static void MenuShowCorePicker(nk_console* widget, void* user_data) {
    NK_UNUSED(widget);
    NK_UNUSED(user_data);
    int coreCount = menu.pendingCoreCount;
    menu.pendingCoreCount = 0;
    if (coreCount <= 0) return;

    nk_console_free_children(menu.corePickerMenu);

    // Back Button
    nk_console* backButton = nk_console_button_onclick(menu.corePickerMenu, "Select Core", &nk_console_button_back);
    nk_console_button_set_symbol(backButton, NK_SYMBOL_X);

    // Tooltip
    const char* tooltip;
    const char* gameName = GetFileNameWithoutExt(menu.pendingGamePath);
    if (menu.pendingGamePath[0] != '\0' && gameName != NULL && gameName[0] != '\0') {
        tooltip = TextFormat("Load %s with which core?", gameName);
    }
    else {
        tooltip ="Choose a core to load the game.";
    }
    nk_console_set_tooltip(backButton, tooltip);

    // Core List
    for (int i = 0; i < coreCount; i++) {
        // pendingCoreNames holds each core's cached human-readable name
        nk_console* coreButton = nk_console_button_onclick_handler(menu.corePickerMenu, menu.pendingCoreNames[i],
            MenuCorePickerLoad, menu.pendingCorePaths[i], NULL);
        nk_console_set_tooltip(coreButton, tooltip);
    }
    ShowLibretroMenu();
    MenuNavigateInto(menu.corePickerMenu);
}

static bool MenuLoadGame(const char* gamePath) {
    // Unload the current game if it's a thing.
    if (IsLibretroGameReady()) {
        UnloadLibretroGame();
    }

    // Find all cores that support this file type.
    int coreCount = FindCoresForGame(gamePath, menu.pendingCorePaths, menu.pendingCoreNames, LIBRETRO_MAX_GAME_CORES);
    if (coreCount == 0) {
        nk_console_show_message(menu.console, TextFormat("No core found for %s", GetFileName(gamePath)));
        return false;
    }

    if (coreCount > 1) {
        // Defer opening the picker to a post-render event.
        TextCopy(menu.pendingGamePath, gamePath);
        menu.pendingCoreCount = coreCount;
        ShowLibretroMenu();
        nk_console_add_event(menu.console, NK_CONSOLE_EVENT_POST_RENDER_ONCE, &MenuShowCorePicker);
        return false;
    }

    // Single core: load it straight away.
    return MenuLoadCoreAndGame(menu.pendingCorePaths[0], gamePath);
}

static void MenuGameFileChanged(nk_console* widget, void* user_data) {
    NK_UNUSED(widget);
    char* path = (char*)user_data;
    if (path && path[0]) {
        SaveLibretroAllSettings();
        CloseLibretro();
        if (MenuLoadGame(path)) HideLibretroMenu(); else ShowLibretroMenu();
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
        HideLibretroMenu();
    }
    else {
        SetLibretroMessage("Load State failed", 2.0);
    }
}

static void LibretroMenuUpdateLoadGameFilter(LibretroMenu* m);

/**
 * Event that's triggered when a menu hot key was changed.
 */
static void LibretroMenuHotkeyChanged(nk_console* widget, void* user_data) {
    NK_UNUSED(user_data);
    if (widget == NULL || widget->data == NULL) {
        return;
    }

    nk_console_input_data* data = (nk_console_input_data*)widget->data;
    nk_bool isKey = nk_console_input_is_key(widget);
    nk_bool isGamepad = nk_console_input_is_gamepad(widget);
    if (!isKey && !isGamepad) {
        return; // The binding was cleared; nothing to deconflict.
    }

    // Check if there were any conflicts with other keys/buttons.
    for (int i = 0; i < LIBRETRO_HOTKEY_COUNT; i++) {
        // Skip the binding that was just changed (matched by output pointer).
        if (&menu.hotkeys[i].gamepad == data->out_gamepad_button || &menu.hotkeys[i].key == data->out_key) {
            continue;
        }

        // Check if the key conflicts.
        if (isKey && menu.hotkeys[i].key == *data->out_key) {
            menu.hotkeys[i].key = NK_CONSOLE_KEY_NONE;
            nk_console_show_message(menu.console, TextFormat("%s has been unbound", menu.hotkeys[i].name));
        }
        // Check if the gamepad button conflicts.
        else if (isGamepad && menu.hotkeys[i].gamepad == *data->out_gamepad_button) {
            menu.hotkeys[i].gamepad = NK_GAMEPAD_BUTTON_INVALID;
            nk_console_show_message(menu.console, TextFormat("%s has been unbound", menu.hotkeys[i].name));
        }
    }
}

LibretroMenu* InitLibretroMenu(void) {
    const int fontSize = 13;

    // Font
    menu.font = LoadFontFromNuklear(fontSize);
    if (!IsFontValid(menu.font)) {
        TraceLog(LOG_ERROR, "LIBRETRO: Error loading menu font");
        return NULL;
    }

    // Nuklear
    menu.ctx = InitNuklearEx(menu.font, (float)fontSize);
    if (!menu.ctx) {
        TraceLog(LOG_ERROR, "LIBRETRO: Error building Nuklear menu");
        UnloadFont(menu.font);
        return NULL;
    }

    // Nuklear Console
    menu.console = nk_console_init(menu.ctx);
    if (!menu.console) {
        UnloadFont(menu.font);
        UnloadNuklear(menu.ctx);
        menu.ctx = NULL;
        TraceLog(LOG_ERROR, "LIBRETRO: Error building Nuklear menu");
        return NULL;
    }

    // Gamepads
    nk_gamepad_init(&menu.gamepads, menu.ctx, (void*)&menu.console);
    nk_console_set_gamepads(menu.console, &menu.gamepads);

    // When trying to go back from the main menu, exit the menu.
    nk_console_add_event(menu.console, NK_CONSOLE_EVENT_BACK, &MenuResumeClicked);

    menu.cfg = rlconfig_load(RAYLIB_LIBRETRO_CFG_FILE);

    // Directories
    TextCopy(LIBRETRO.coreDirectory, "cores");
    TextCopy(LIBRETRO.saveDirectory, "saves");
    TextCopy(LIBRETRO.systemDirectory, "system");
#ifdef __EMSCRIPTEN__
    // Default save/system directories point at the IDBFS mount so they
    // persist across page reloads. The user can still override via the
    // Settings menu; saved values take precedence on subsequent runs.
    TextCopy(LIBRETRO.saveDirectory,   "/userdata/saves");
    TextCopy(LIBRETRO.systemDirectory, "/userdata/system");
#endif

    // Menu Settings
    LoadLibretroMenuSettings();
    LibretroMenuEnsureSaveDir();
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
#if defined(__EMSCRIPTEN__) || defined(PLATFORM_WEB)
    menu.loadGameWidget = nk_console_button_onclick(menu.console, "Load Game", &LibretroMenuEmscriptenLoadGameClicked);
#else
    // Desktop and Android both use the in-app file browser. On Android the app
    // holds MANAGE_EXTERNAL_STORAGE, so raylib file I/O can read shared storage;
    // SetupAndroidEnvironment() points this widget at fileBrowserStartDirectory.
    menu.loadGameWidget = nk_console_file_action(menu.console, "Load Game", menu.loadGamePath, RAYLIB_LIBRETRO_VFS_MAX_PATH);
    nk_console_add_event_handler(menu.loadGameWidget, NK_CONSOLE_EVENT_CHANGED, &MenuGameFileChanged, menu.loadGamePath, NULL);
    if (LIBRETRO.fileBrowserStartDirectory[0] != '\0') {
        nk_console_file_set_directory(menu.loadGameWidget, LIBRETRO.fileBrowserStartDirectory);
    }
#endif
    LibretroMenuUpdateLoadGameFilter(&menu);

    // Close Game
    //menu.closeGameButton = nk_console_button_onclick(menu.console, "Close Game", &MenuCloseGameClicked);
    //nk_console_button_set_symbol(menu.resumeButton, NK_SYMBOL_X);

    // Settings
    nk_console* settings = nk_console_button(menu.console, "Settings");
    nk_console_button_set_symbol(settings, NK_SYMBOL_HAMBURGER);
    nk_console_add_event(settings, NK_CONSOLE_EVENT_BACK, &MenuCommitSettings);
    {
        // Back
        nk_console_button_set_symbol(
            nk_console_button_onclick(settings, "Settings", &nk_console_button_back),
            NK_SYMBOL_TRIANGLE_UP);

        // Audio & Video
        {
            nk_console* graphicsMenu = nk_console_button(settings, "Audio & Video");
            nk_console_button_set_symbol(graphicsMenu, NK_SYMBOL_TRIANGLE_RIGHT);
            nk_console_add_event(graphicsMenu, NK_CONSOLE_EVENT_BACK, &MenuCommitSettings);
            nk_console_button_set_symbol(
                nk_console_button_onclick(graphicsMenu, "Audio & Video", &nk_console_button_back),
                NK_SYMBOL_TRIANGLE_UP);

            // Volume
            nk_console* volume = nk_console_slider_float(graphicsMenu, "Volume", 0.0f, &LIBRETRO.volume, 1.0f, RAYLIB_LIBRETRO_MENU_SLIDER_STEP(0.0f, 1.0f));
            nk_console_add_event_handler(volume, NK_CONSOLE_EVENT_CHANGED, &LibretroMenuSettingChanged, NULL, NULL);

            // Theme
            nk_console* themeCombo = nk_console_combobox(graphicsMenu, "Theme", RAYLIB_LIBRETRO_STYLES_NAMES, '|', &menu.themeSelectedIndex);
            nk_console_add_event_handler(themeCombo, NK_CONSOLE_EVENT_CHANGED, &LibretroMenuSettingChanged, NULL, NULL);
            SetLibretroMenuStyle((LibretroMenuStyle)menu.themeSelectedIndex);

            // Full Screen
            nk_console* fullscreenCheckbox = nk_console_checkbox(graphicsMenu, "Fullscreen", &menu.fullscreen);
            nk_console_add_event(fullscreenCheckbox, NK_CONSOLE_EVENT_CHANGED, LibretroMenuFullscreenChanged);

            // VSYNC
            nk_console* vsyncCheckbox = nk_console_checkbox(graphicsMenu, "VSYNC", &menu.vsync);
            nk_console_add_event(vsyncCheckbox, NK_CONSOLE_EVENT_CHANGED, LibretroMenuVideoChanged);

            // FPS
            nk_console* fpsCombo = nk_console_combobox(graphicsMenu, "FPS", "Auto|30|60|120|144|240|Unlimited", '|', &menu.fpsIndex);
            nk_console_add_event_handler(fpsCombo, NK_CONSOLE_EVENT_CHANGED, &LibretroMenuVideoChanged, NULL, NULL);

            // Integer Scaling
            nk_console_checkbox(graphicsMenu, "Integer Scaling", &LIBRETRO.integerScaling)
                ->tooltip = "Keep pixel graphics looking sharp and crisp.";

            // Shader
            static char shaderNames[256] = {0};
            if (shaderNames[0] == '\0') {
                int offset = 0;
                for (int i = 0; i < LIBRETRO_SHADER_TYPE_COUNT; ++i) {
                    const char* name = GetLibretroShaderName((LibretroShaderType)i);
                    int len = (int)TextLength(name);
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

            // Texture Filter
            nk_console* textureFilter = nk_console_combobox(graphicsMenu, "Texture Filter", "None|Bilinear|Trilinear|Anisotropic 4x|Anisotropic 8x|Anisotropic 16x", '|', &LIBRETRO.textureFilter);
            nk_console_add_event_handler(textureFilter, NK_CONSOLE_EVENT_CHANGED, &LibretroMenuTextureFilterChanged, NULL, NULL);

            // Rotation
            nk_console_combobox(graphicsMenu, "Rotation", "0 Degrees|90 Degrees|180 Degrees|270 Degrees", '|', &LIBRETRO.core.rotation)
                ->tooltip = "Override the display rotation for the running game.";
        }

        // Gameplay
        {
            nk_console* gameplayMenu = nk_console_button(settings, "Gameplay");
            nk_console_button_set_symbol(gameplayMenu, NK_SYMBOL_TRIANGLE_RIGHT);
            nk_console_add_event(gameplayMenu, NK_CONSOLE_EVENT_BACK, &MenuCommitSettings);
            nk_console_button_set_symbol(
                nk_console_button_onclick(gameplayMenu, "Gameplay", &nk_console_button_back),
                NK_SYMBOL_TRIANGLE_UP);

            // Fast Forward Speed
            nk_console_slider_float(gameplayMenu, "Fast Forward Speed", 1.1f, &menu.fastForwardSpeed, 10.0f, RAYLIB_LIBRETRO_MENU_SLIDER_STEP(1.1f, 10.0f));

            // Slow Motion Speed
            nk_console_slider_float(gameplayMenu, "Slow Motion Speed", 0.1f, &menu.slowMotionSpeed, 0.9f, RAYLIB_LIBRETRO_MENU_SLIDER_STEP(0.1f, 0.9f));

            // Touch Control
            nk_console_checkbox(gameplayMenu, "Touch Controls", &menu.touchControls);

            // Touch Haptics
            #if defined(PLATFORM_WEB)
            nk_console_checkbox(gameplayMenu, "Touch Haptics", &menu.touchHapticsEnabled);
            #endif

            // Screen Orientation (applied by the Android activity; see bin/raylib-libretro.c)
            #if defined(__ANDROID__) || defined(PLATFORM_ANDROID)
            nk_console* orientationCombo = nk_console_combobox(gameplayMenu, "Orientation",
                "Landscape|Portrait|Auto", '|', &menu.orientationIndex);
            orientationCombo->tooltip = "Lock the screen to landscape or portrait, or follow the sensor";
            #endif

            // Rewind
            nk_console_checkbox(gameplayMenu, "Rewind", &menu.rewindEnabled);

            // Disable Hot Keys
            nk_console_checkbox(gameplayMenu, "Disable Hot Keys", &menu.disableHotKeysActive)
                ->tooltip = "Pass all keyboard input to the core and suspend frontend hotkeys";

            // Analog to D-Pad
            nk_console_combobox(gameplayMenu, "Analog to D-Pad",
                "None|Left Analog|Right Analog", '|', &LIBRETRO.analogToDpadIndex)
                ->tooltip = "Map an analog stick to D-Pad inputs";

            // Cursor
            nk_console_checkbox(gameplayMenu, "Hide Cursor", &menu.hideCursor);
            nk_console_checkbox(gameplayMenu, "Lock Cursor", &menu.lockCursor);

            // Save Slot
            nk_console_combobox(gameplayMenu, "Save Slot",
                "Slot 1|Slot 2|Slot 3|Slot 4|Slot 5|Slot 6|Slot 7|Slot 8|Slot 9|Slot 10",
                '|', &menu.saveSlotIndex);

            // SRAM Auto-Save
            nk_console_combobox(gameplayMenu, "SRAM Auto-Save",
                "Off|15s|30s|1m|2m|5m|10m", '|', &menu.sramAutoSaveIndex)
                ->tooltip = "Periodically save battery data to disk";

            // Username
            nk_console_textedit(gameplayMenu, "Username", LIBRETRO.username, 128);
        }

        // Hot Keys
        {
            nk_console* keysMenu = nk_console_button(settings, "Hot Keys");
            nk_console_button_set_symbol(keysMenu, NK_SYMBOL_TRIANGLE_RIGHT);
            nk_console_add_event(keysMenu, NK_CONSOLE_EVENT_BACK, &MenuCommitSettings);
            nk_console_button_set_symbol(
                nk_console_button_onclick(keysMenu, "Hot Keys", &nk_console_button_back),
                NK_SYMBOL_TRIANGLE_UP);

            // Menu Controller Combo
            nk_console* menuCombo = nk_console_combobox(keysMenu, "Menu Controller Combo",
                "None|Select + Start|L1 + R1|L2 + R2|L3 + R3|Down + Select",
                '|', &menu.menuComboIndex);
            menuCombo->tooltip = "Controller button combination to toggle the menu";

            // Keys
            for (int i = 0; i < LIBRETRO_HOTKEY_COUNT; i++) {
                nk_console* widget = nk_console_input(keysMenu, menu.hotkeys[i].name, -1, NULL, &menu.hotkeys[i].gamepad, &menu.hotkeys[i].key, NULL);
                nk_console_add_event(widget, NK_CONSOLE_EVENT_CHANGED, &LibretroMenuHotkeyChanged);
            }
        }

        // Keyboard Controls (Player 1)
        {
            nk_console* kbMenu = nk_console_button(settings, "Keyboard Controls");
            nk_console_button_set_symbol(kbMenu, NK_SYMBOL_TRIANGLE_RIGHT);
            nk_console_add_event(kbMenu, NK_CONSOLE_EVENT_BACK, &MenuCommitSettings);
            nk_console_button_set_symbol(
                nk_console_button_onclick(kbMenu, "Keyboard Controls", &nk_console_button_back),
                NK_SYMBOL_TRIANGLE_UP);
            LibretroMenuKeyConsole(kbMenu, "B",      RETRO_DEVICE_ID_JOYPAD_B);
            LibretroMenuKeyConsole(kbMenu, "Y",      RETRO_DEVICE_ID_JOYPAD_Y);
            LibretroMenuKeyConsole(kbMenu, "Select", RETRO_DEVICE_ID_JOYPAD_SELECT);
            LibretroMenuKeyConsole(kbMenu, "Start",  RETRO_DEVICE_ID_JOYPAD_START);
            LibretroMenuKeyConsole(kbMenu, "Up",     RETRO_DEVICE_ID_JOYPAD_UP);
            LibretroMenuKeyConsole(kbMenu, "Down",   RETRO_DEVICE_ID_JOYPAD_DOWN);
            LibretroMenuKeyConsole(kbMenu, "Left",   RETRO_DEVICE_ID_JOYPAD_LEFT);
            LibretroMenuKeyConsole(kbMenu, "Right",  RETRO_DEVICE_ID_JOYPAD_RIGHT);
            LibretroMenuKeyConsole(kbMenu, "A",      RETRO_DEVICE_ID_JOYPAD_A);
            LibretroMenuKeyConsole(kbMenu, "X",      RETRO_DEVICE_ID_JOYPAD_X);
            LibretroMenuKeyConsole(kbMenu, "L",      RETRO_DEVICE_ID_JOYPAD_L);
            LibretroMenuKeyConsole(kbMenu, "R",      RETRO_DEVICE_ID_JOYPAD_R);
            LibretroMenuKeyConsole(kbMenu, "L2",     RETRO_DEVICE_ID_JOYPAD_L2);
            LibretroMenuKeyConsole(kbMenu, "R2",     RETRO_DEVICE_ID_JOYPAD_R2);
            LibretroMenuKeyConsole(kbMenu, "L3",     RETRO_DEVICE_ID_JOYPAD_L3);
            LibretroMenuKeyConsole(kbMenu, "R3",     RETRO_DEVICE_ID_JOYPAD_R3);
        }

        // Directories
        {
            nk_console* dirMenu = nk_console_button(settings, "Directories");
            nk_console_button_set_symbol(dirMenu, NK_SYMBOL_TRIANGLE_RIGHT);
            nk_console_add_event(dirMenu, NK_CONSOLE_EVENT_BACK, &MenuCommitSettings);
            nk_console_button_set_symbol(
                nk_console_button_onclick(dirMenu, "Directories", &nk_console_button_back),
                NK_SYMBOL_TRIANGLE_UP);

            // Cores
            nk_console* coreDirectory = nk_console_dir(dirMenu, "Cores", LIBRETRO.coreDirectory, RAYLIB_LIBRETRO_VFS_MAX_PATH);
            nk_console_add_event_handler(coreDirectory, NK_CONSOLE_EVENT_CHANGED, &MenuCoreDirChanged, NULL, NULL);

            // Saves
            nk_console* saveDirectory = nk_console_dir(dirMenu, "Saves", LIBRETRO.saveDirectory, RAYLIB_LIBRETRO_VFS_MAX_PATH);
            nk_console_add_event_handler(saveDirectory, NK_CONSOLE_EVENT_CHANGED, &MenuDirChanged, NULL, NULL);

            // Assets
            nk_console* coreAssetsDirectory = nk_console_dir(dirMenu, "Assets", LIBRETRO.coreAssetsDirectory, RAYLIB_LIBRETRO_VFS_MAX_PATH);
            nk_console_add_event_handler(coreAssetsDirectory, NK_CONSOLE_EVENT_CHANGED, &MenuDirChanged, NULL, NULL);

            // System
            nk_console* systemDirectory = nk_console_dir(dirMenu, "System", LIBRETRO.systemDirectory, RAYLIB_LIBRETRO_VFS_MAX_PATH);
            nk_console_add_event_handler(systemDirectory, NK_CONSOLE_EVENT_CHANGED, &MenuDirChanged, NULL, NULL);

            // Playlists
            nk_console* playlistsDirectory = nk_console_dir(dirMenu, "Playlists", LIBRETRO.playlistsDirectory, RAYLIB_LIBRETRO_VFS_MAX_PATH);
            nk_console_add_event_handler(playlistsDirectory, NK_CONSOLE_EVENT_CHANGED, &MenuDirChanged, NULL, NULL);

            // Content
            nk_console* fileBrowserStartDirectory = nk_console_dir(dirMenu, "Content", LIBRETRO.fileBrowserStartDirectory, RAYLIB_LIBRETRO_VFS_MAX_PATH);
            nk_console_add_event_handler(fileBrowserStartDirectory, NK_CONSOLE_EVENT_CHANGED, &MenuContentDirChanged, NULL, NULL);
        }

        // Core Options (nested inside Settings)
        menu.optionsMenu = nk_console_button(settings, "Core Options");
        nk_console_add_event(menu.optionsMenu, NK_CONSOLE_EVENT_BACK, &MenuCommitSettings);
        nk_console_button_set_symbol(menu.optionsMenu, NK_SYMBOL_TRIANGLE_RIGHT);
        {
            nk_console_button_set_symbol(
                nk_console_button_onclick(menu.optionsMenu, "Core Options", &nk_console_button_back),
                NK_SYMBOL_TRIANGLE_UP);
        }

        // Controllers (per-port device selection, populated by BuildLibretroMenuControllers)
        menu.controllersMenu = nk_console_button(settings, "Controllers");
        nk_console_add_event(menu.controllersMenu, NK_CONSOLE_EVENT_BACK, &MenuCommitSettings);
        nk_console_button_set_symbol(menu.controllersMenu, NK_SYMBOL_TRIANGLE_RIGHT);
        {
            nk_console_button_set_symbol(
                nk_console_button_onclick(menu.controllersMenu, "Controllers", &nk_console_button_back),
                NK_SYMBOL_TRIANGLE_UP);
        }
        menu.controllersMenu->visible = nk_false;
    }

    // About
    {
        LibretroMenuUpdateAbout();
        nk_console* aboutMenu = nk_console_button(menu.console, "About");
        nk_console_add_event(aboutMenu, NK_CONSOLE_EVENT_CLICKED, &LibretroMenuAboutOpened);
        {
            nk_console_button_set_symbol(
                nk_console_button_onclick(aboutMenu, "About", &nk_console_button_back),
                NK_SYMBOL_TRIANGLE_UP);

            // System
            nk_console* systemTree = nk_console_tree(aboutMenu, "System", nk_true);
            nk_console_label(systemTree, menu.aboutVersion);
            nk_console_label(systemTree, "Build:           " __DATE__);
            nk_console_label(systemTree, "raylib:          " RAYLIB_VERSION);
            nk_console_label(systemTree, menu.aboutRenderer);
            nk_console_label(systemTree, "Platform:        " RAYLIB_LIBRETRO_PLATFORM);
            nk_console_label(systemTree, menu.aboutResolution);
            nk_console_label(systemTree, "License:         GPL-3.0-or-later");
            nk_console_label(systemTree, "https://github.com/RobLoach/raylib-libretro");

            // Core
            nk_console* coreTree = nk_console_tree(aboutMenu, "Core", nk_true);
            nk_console_label(coreTree, menu.aboutCoreName);
            nk_console_label(coreTree, menu.aboutCoreVersion);
            nk_console_label(coreTree, menu.aboutCoreResolution);
            nk_console_label(coreTree, menu.aboutCorePixelFormat);
            nk_console_label(coreTree, menu.aboutCoreSampleRate);
            nk_console_label(coreTree, menu.aboutCoreAspectRatio);
            nk_console_label(coreTree, menu.aboutContent);
            nk_console_label(coreTree, menu.aboutExtensions);

            // Available Cores
            nk_console* coresTree = nk_console_tree(aboutMenu, "Available Cores", nk_true);
            if (cvector_size(menu.coreInfos) == 0) {
                nk_console_label(coresTree, "(none found)");
            }
            for (int i = 0; i < (int)cvector_size(menu.coreInfos); i++) {
                LibretroCoreInfo* ci = menu.coreInfos[i];
                nk_console_label(coresTree, LibretroCoreDisplayName(i));
            }

            nk_console_rule_horizontal(aboutMenu, nk_rgba(0,0,0,0), nk_false);
            nk_console_button_set_symbol(
                nk_console_button_onclick(aboutMenu, "Back", &nk_console_button_back),
                NK_SYMBOL_TRIANGLE_LEFT);
        }
    }

    // Quit
#if !defined(__EMSCRIPTEN__) && !defined(__ANDROID__) && !defined(PLATFORM_ANDROID)
    nk_console* quitButton = nk_console_button(menu.console, "Quit");
    nk_console_add_event(quitButton, NK_CONSOLE_EVENT_CLICKED, &LibretroMenuQuitClicked);
    nk_console_button_set_symbol(quitButton, NK_SYMBOL_X);
#endif

    // Core picker (hidden; shown dynamically when a game matches multiple cores)
    menu.corePickerMenu = nk_console_button(menu.console, "Select Core");
    menu.corePickerMenu->visible = nk_false;

    ShowLibretroMenu();
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
            TextCopy(out, TextSubtext(p, 0, len));
            return;
        }
        if (!*pipe) break;
        p = pipe + 1;
        idx++;
    }
    if (out && outSize > 0) out[0] = '\0';
}

// Boolean-ish option tokens. libretro cores express on/off options with a
// handful of conventional value pairs; recognising both sides lets us render
// any of them as a checkbox while writing back the core's actual token.
static bool LibretroMenuTokenIsTruthy(const char* tok) {
    return TextIsEqual(tok, "enabled") || TextIsEqual(tok, "on") ||
           TextIsEqual(tok, "true")    || TextIsEqual(tok, "yes") ||
           TextIsEqual(tok, "1");
}
static bool LibretroMenuTokenIsFalsy(const char* tok) {
    return TextIsEqual(tok, "disabled") || TextIsEqual(tok, "off") ||
           TextIsEqual(tok, "false")    || TextIsEqual(tok, "no")  ||
           TextIsEqual(tok, "0");
}

/**
 * Called when a core option combobox selection changes.
 *
 * @param user_data The option index cast to (void*).
 */
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

/**
 * Called when a core option boolean checkbox changes.
 *
 * @param user_data The option index cast to (void*). Writes back the actual token the core declared (e.g. "on"/"off", "enabled"/"disabled") rather than assuming a fixed pair, so non-"enabled|disabled" booleans work too.
 */
static void LibretroMenuOptionCheckboxChanged(nk_console* widget, void* user_data) {
    (void)widget;
    unsigned i = (unsigned)(uintptr_t)user_data;
    char tok0[LIBRETRO_CORE_VARIABLE_VALUE_LEN] = {0};
    char tok1[LIBRETRO_CORE_VARIABLE_VALUE_LEN] = {0};
    LibretroMenuGetNthToken(LIBRETRO.core.variableValuesList[i], 0, tok0, sizeof(tok0));
    LibretroMenuGetNthToken(LIBRETRO.core.variableValuesList[i], 1, tok1, sizeof(tok1));
    // Map checked->truthy token, unchecked->falsy token, regardless of order.
    bool tok0Truthy = LibretroMenuTokenIsTruthy(tok0);
    const char* truthy = tok0Truthy ? tok0 : tok1;
    const char* falsy  = tok0Truthy ? tok1 : tok0;
    SetLibretroCoreOption(LIBRETRO.core.variableKeys[i],
                          menu.optionCheckboxValues[i] ? truthy : falsy);
}

static int LibretroMenuFindTokenIndex(const char* str, const char* value) {
    int idx = 0;
    const char* p = str;
    while (p && *p) {
        const char* pipe = p;
        while (*pipe && *pipe != '|') pipe++;
        int len = (int)(pipe - p);
        char tok[LIBRETRO_CORE_VARIABLE_VALUE_LEN] = {0};
        if (len < LIBRETRO_CORE_VARIABLE_VALUE_LEN) TextCopy(tok, TextSubtext(p, 0, len));
        if (TextIsEqual(tok, value)) return idx;
        if (!*pipe) break;
        p = pipe + 1;
        idx++;
    }
    return 0;
}

/**
 * Returns true if value is an exact token in the pipe-delimited list (e.g. "a|b|c").
 *
 * Unlike LibretroMenuFindTokenIndex, this distinguishes "not present" from "present at index 0", so it can validate a saved option value.
 */
static bool LibretroMenuValueInList(const char* valuesList, const char* value) {
    const char* p = valuesList;
    while (p && *p) {
        const char* pipe = p;
        while (*pipe && *pipe != '|') pipe++;
        int len = (int)(pipe - p);
        char tok[LIBRETRO_CORE_VARIABLE_VALUE_LEN] = {0};
        if (len < LIBRETRO_CORE_VARIABLE_VALUE_LEN) TextCopy(tok, TextSubtext(p, 0, len));
        if (TextIsEqual(tok, value)) return true;
        if (!*pipe) break;
        p = pipe + 1;
    }
    return false;
}

/**
 * Returns true if the values are exactly a boolean pair.
 *
 * One truthy, one falsy token in either order, like "enabled|disabled", "off|on", "true|false".
 */
static bool LibretroMenuIsBooleanOption(const char* valuesList) {
    if (!valuesList || !*valuesList) return false;
    char tok0[LIBRETRO_CORE_VARIABLE_VALUE_LEN] = {0};
    char tok1[LIBRETRO_CORE_VARIABLE_VALUE_LEN] = {0};
    char tok2[LIBRETRO_CORE_VARIABLE_VALUE_LEN] = {0};
    LibretroMenuGetNthToken(valuesList, 0, tok0, sizeof(tok0));
    LibretroMenuGetNthToken(valuesList, 1, tok1, sizeof(tok1));
    LibretroMenuGetNthToken(valuesList, 2, tok2, sizeof(tok2));
    if (tok1[0] == '\0' || tok2[0] != '\0') return false; // not exactly two values
    return (LibretroMenuTokenIsTruthy(tok0) && LibretroMenuTokenIsFalsy(tok1)) ||
           (LibretroMenuTokenIsFalsy(tok0)  && LibretroMenuTokenIsTruthy(tok1));
}

/**
 * Returns the registered category index for option i, or -1 if it has no category (or references a category the core never declared — treated flat).
 */
static int LibretroMenuOptionCategory(unsigned i) {
    if (LIBRETRO.core.variableCategoryKeys[i][0] == '\0') return -1;
    for (unsigned c = 0; c < LIBRETRO.core.categoryCount; c++) {
        if (TextIsEqual(LIBRETRO.core.variableCategoryKeys[i], LIBRETRO.core.categoryKeys[c])) {
            return (int)c;
        }
    }
    return -1;
}

/**
 * Build the checkbox/combobox widget for option i under the given parent node.
 */
static void LibretroMenuBuildOptionWidget(LibretroMenu* m, nk_console* parent, unsigned i) {
    const char* label = TextLength(LIBRETRO.core.variableLabels[i]) > 0
        ? LIBRETRO.core.variableLabels[i]
        : LIBRETRO.core.variableKeys[i];

    nk_console* widget;

    if (LibretroMenuIsBooleanOption(LIBRETRO.core.variableValuesList[i])) {
        m->optionCheckboxValues[i] = (nk_bool)LibretroMenuTokenIsTruthy(LIBRETRO.core.variableValues[i]);
        widget = nk_console_checkbox(parent, label, &m->optionCheckboxValues[i]);
        nk_console_add_event_handler(widget, NK_CONSOLE_EVENT_CHANGED,
                                     LibretroMenuOptionCheckboxChanged,
                                     (void*)(uintptr_t)i, NULL);
    } else {
        m->optionSelectedIndices[i] = LibretroMenuFindTokenIndex(
            LIBRETRO.core.variableValuesList[i], LIBRETRO.core.variableValues[i]);

        const char* displayStr = TextLength(LIBRETRO.core.variableDisplayList[i]) > 0
            ? LIBRETRO.core.variableDisplayList[i]
            : LIBRETRO.core.variableValuesList[i];

        widget = nk_console_combobox(parent, label,
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

static void LibretroMenuResetCoreOptionsClicked(nk_console* widget, void* user_data) {
    NK_UNUSED(user_data);
    ResetAllLibretroCoreOptions();
    LIBRETRO.core.variablesVisibilityDirty = true;
    nk_console_show_message(nk_console_get_top(widget), "Core options have been reset to defaults");
    nk_console_navigate_back(widget->parent);
}

/**
 * Append an extension token to the pipe-separated list if it is not already present and there is room. Returns the updated list length.
 */
static unsigned int LibretroMenuAddExtension(char* list, unsigned int len, size_t cap,
                                             const char* tok, unsigned int tokLen) {
    if (tokLen == 0) return len;

    // Skip if the token already exists as a whole entry in the list.
    for (const char* p = list; *p != '\0'; ) {
        const char* end = p;
        while (*end != '\0' && *end != '|') end++;
        if ((unsigned int)(end - p) == tokLen) {
            unsigned int k = 0;
            while (k < tokLen && p[k] == tok[k]) k++;
            if (k == tokLen) return len;
        }
        p = (*end == '|') ? end + 1 : end;
    }

    // +2 reserves the '|' separator and the null terminator.
    if (len + tokLen + 2 >= cap) return len;
    if (len > 0) list[len++] = '|';
    TextCopy(list + len, TextSubtext(tok, 0, (int)tokLen));
    len += tokLen;
    return len;
}

static void LibretroMenuUpdateLoadGameFilter(LibretroMenu* m) {
#if defined(__EMSCRIPTEN__) || defined(PLATFORM_WEB) || defined(__ANDROID__) || defined(PLATFORM_ANDROID)
    (void)m;
    return;
#endif
    if (!m || !m->loadGameWidget) return;

    // Collect the unique set of extensions from every cached core. Cores
    // commonly share extensions (zip, bin, cue, ...), so dedup the tokens —
    // otherwise duplicates eat into the buffer and distinct extensions from
    // later cores get dropped once it fills.
    char allExts[1024] = {0};
    unsigned int allLen = 0;
    for (int i = 0; i < (int)cvector_size(m->coreInfos); i++) {
        const char* exts = m->coreInfos[i]->supportedExtensions;
        if (exts[0] == '\0') continue;
        // Split this core's "ext1|ext2|..." list and add each token once.
        for (const char* tok = exts; *tok != '\0'; ) {
            const char* end = tok;
            while (*end != '\0' && *end != '|') end++;
            allLen = LibretroMenuAddExtension(allExts, allLen, sizeof(allExts), tok, (unsigned int)(end - tok));
            tok = (*end == '|') ? end + 1 : end;
        }
    }

    // Archives are browsable regardless of core; add "zip" to the set (deduped,
    // in case a core already declared it).
    allLen = LibretroMenuAddExtension(allExts, allLen, sizeof(allExts), "zip", 3);

    if (allLen == 0) {
        nk_console_file_set_filter(m->loadGameWidget, NULL);
        return;
    }

    // "a|b" expands to ".a;.b" (under 2x), so this can never truncate.
    char filter[sizeof(allExts) * 2];
    GetLibretroFileExtensionPattern(allExts, filter, sizeof(filter));
    nk_console_file_set_filter(m->loadGameWidget, filter);
}

void BuildLibretroMenuOptions(LibretroMenu* m) {
    if (!m || !m->optionsMenu) return;

    // Update the Load Game file filter to match the newly loaded core.
    LibretroMenuUpdateLoadGameFilter(m);

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

    // Uncategorized options first, directly under Core Options, so they appear
    // above the per-category submenus rather than below them.
    for (unsigned i = 0; i < LIBRETRO.core.variableCount; i++) {
        if (TextLength(LIBRETRO.core.variableValuesList[i]) == 0) continue;
        if (!LIBRETRO.core.variableVisible[i]) continue;
        if (LibretroMenuOptionCategory(i) >= 0) continue; // belongs to a category submenu
        LibretroMenuBuildOptionWidget(m, m->optionsMenu, i);
    }

    // Then a submenu button per category (v2 only), each holding its options.
    for (unsigned c = 0; c < LIBRETRO.core.categoryCount; c++) {
        // Skip categories with no visible options.
        bool hasVisible = false;
        for (unsigned i = 0; i < LIBRETRO.core.variableCount; i++) {
            if (!LIBRETRO.core.variableVisible[i]) continue;
            if (TextLength(LIBRETRO.core.variableValuesList[i]) == 0) continue;
            if (LibretroMenuOptionCategory(i) == (int)c) { hasVisible = true; break; }
        }
        if (!hasVisible) continue;

        nk_console* categoryButton = nk_console_button(m->optionsMenu, LIBRETRO.core.categoryLabels[c]);
        nk_console_button_set_symbol(categoryButton, NK_SYMBOL_TRIANGLE_RIGHT);
        // Back button header so each category submenu can return to Core Options.
        nk_console_button_set_symbol(
            nk_console_button_onclick(categoryButton, LIBRETRO.core.categoryLabels[c], &nk_console_button_back),
            NK_SYMBOL_TRIANGLE_UP);

        for (unsigned i = 0; i < LIBRETRO.core.variableCount; i++) {
            if (TextLength(LIBRETRO.core.variableValuesList[i]) == 0) continue;
            if (!LIBRETRO.core.variableVisible[i]) continue;
            if (LibretroMenuOptionCategory(i) != (int)c) continue;
            LibretroMenuBuildOptionWidget(m, categoryButton, i);
        }
    }

    // "Reset to defaults" button at the bottom of the Core Options submenu.
    nk_console_rule_horizontal(m->optionsMenu, nk_rgba(0,0,0,0), nk_false);
    nk_console* resetButton = nk_console_button_onclick(m->optionsMenu,
        "Reset to defaults", &LibretroMenuResetCoreOptionsClicked);
    nk_console_button_set_symbol(resetButton, NK_SYMBOL_TRIANGLE_LEFT);
}

static void LibretroMenuPortDeviceChanged(nk_console* widget, void* user_data) {
    NK_UNUSED(widget);
    unsigned port = (unsigned)(uintptr_t)user_data;
    unsigned count;
    const struct retro_controller_info* info = GetLibretroControllerInfo(&count);
    if (!info || port >= count) return;
    unsigned idx = (unsigned)menu.portDeviceIndex[port];
    if (idx >= info[port].num_types) return;
    SetLibretroPortDevice(port, info[port].types[idx].id);
}

void BuildLibretroMenuControllers(LibretroMenu* m) {
    if (!m || !m->controllersMenu) return;
    nk_console_free_children(m->controllersMenu);

    nk_console_button_set_symbol(
        nk_console_button_onclick(m->controllersMenu, "Controllers", &nk_console_button_back),
        NK_SYMBOL_TRIANGLE_UP);

    unsigned count = 0;
    const struct retro_controller_info* info = GetLibretroControllerInfo(&count);
    if (!info || count == 0) {
        m->controllersMenu->visible = nk_false;
        return;
    }

    bool anyWidget = false;
    for (unsigned port = 0; port < count && port < 16; port++) {
        if (info[port].num_types <= 1) continue;

        // nk_console_combobox stores pointers into these strings rather than
        // copying them, so they must persist for the lifetime of the widget.
        char* options = m->portDeviceOptions[port];
        options[0] = '\0';
        int optLen = 0;
        for (unsigned i = 0; i < info[port].num_types; i++) {
            if (i > 0) TextAppend(options, "|", &optLen);
            TextAppend(options, info[port].types[i].desc, &optLen);
        }

        unsigned currentDevice = GetLibretroPortDevice(port);
        m->portDeviceIndex[port] = 0;
        for (unsigned i = 0; i < info[port].num_types; i++) {
            if (info[port].types[i].id == currentDevice) {
                m->portDeviceIndex[port] = (int)i;
                break;
            }
        }

        TextCopy(m->portDeviceLabel[port], TextFormat("Port %u", port + 1));
        nk_console* widget = nk_console_combobox(m->controllersMenu,
            m->portDeviceLabel[port], options, '|', &m->portDeviceIndex[port]);
        nk_console_add_event_handler(widget, NK_CONSOLE_EVENT_CHANGED,
            LibretroMenuPortDeviceChanged, (void*)(uintptr_t)port, NULL);
        anyWidget = true;
    }

    m->controllersMenu->visible = (nk_bool)anyWidget;
}

// Copy a "raylib-libretro" string setting into dest when present; dest is left
// unchanged if the key is absent, so its existing default stays in place.
static void LibretroMenuLoadString(const char* key, char* dest) {
    const char* value = rlconfig_get(menu.cfg, "raylib-libretro", key);
    if (value) TextCopy(dest, value);
}

// Write the loaded core's options into its [libraryName] section (no disk flush).
// No-op when no core is loaded or it exposed no variables.
static void LibretroMenuWriteCoreOptions(void) {
    const char* coreName = LIBRETRO.core.libraryName;
    if (!menu.cfg || !coreName || !coreName[0] || LIBRETRO.core.variableCount == 0) return;
    rlconfig_clear_section(menu.cfg, coreName);
    for (unsigned i = 0; i < LIBRETRO.core.variableCount; i++) {
        rlconfig_set(menu.cfg, coreName, LIBRETRO.core.variableKeys[i], LIBRETRO.core.variableValues[i]);
    }
}

static void LibretroMenuUpdateConfig(void) {
    if (!menu.cfg) return;
    if (!IsWindowFullscreen()) {
        rlconfig_set_int(menu.cfg, "raylib-libretro", "windowWidth", GetScreenWidth());
        rlconfig_set_int(menu.cfg, "raylib-libretro", "windowHeight", GetScreenHeight());
    }
    rlconfig_set_int(menu.cfg, "raylib-libretro", "fullscreen", (int)menu.fullscreen);
    rlconfig_set_int(menu.cfg, "raylib-libretro", "vsync", (int)menu.vsync);
    rlconfig_set_int(menu.cfg, "raylib-libretro", "fps", menu.fpsIndex);
    rlconfig_set_int(menu.cfg, "raylib-libretro", "shader", menu.shaderSelectedIndex);
    rlconfig_set_int(menu.cfg, "raylib-libretro", "textureFilter", LIBRETRO.textureFilter);
    rlconfig_set_int(menu.cfg, "raylib-libretro", "integerScaling", LIBRETRO.integerScaling ? 1 : 0);
    rlconfig_set_int(menu.cfg, "raylib-libretro", "theme", menu.themeSelectedIndex);
    rlconfig_set_float(menu.cfg, "raylib-libretro", "volume", LIBRETRO.volume);
    rlconfig_set_int(menu.cfg, "raylib-libretro", "rewind", menu.rewindEnabled ? 1 : 0);
    rlconfig_set_int(menu.cfg, "raylib-libretro", "analogToDpad", LIBRETRO.analogToDpadIndex);
    rlconfig_set_int(menu.cfg, "raylib-libretro", "menuCombo", menu.menuComboIndex);
    rlconfig_set_int(menu.cfg, "raylib-libretro", "hideCursor", menu.hideCursor ? 1 : 0);
    rlconfig_set_int(menu.cfg, "raylib-libretro", "lockCursor", menu.lockCursor ? 1 : 0);
    rlconfig_set_int(menu.cfg, "raylib-libretro", "touchControls", menu.touchControls ? 1 : 0);
    rlconfig_set_int(menu.cfg, "raylib-libretro", "touchHaptics", menu.touchHapticsEnabled ? 1 : 0);
    rlconfig_set_int(menu.cfg, "raylib-libretro", "orientation", menu.orientationIndex);

    rlconfig_set_int(menu.cfg, "raylib-libretro", "saveSlot", menu.saveSlotIndex);
    rlconfig_set_int(menu.cfg, "raylib-libretro", "sramAutoSave", menu.sramAutoSaveIndex);
    rlconfig_set(menu.cfg, "raylib-libretro", "username", LIBRETRO.username);
    rlconfig_set_float(menu.cfg, "raylib-libretro", "fastForwardSpeed", menu.fastForwardSpeed);
    rlconfig_set_float(menu.cfg, "raylib-libretro", "slowMotionSpeed", menu.slowMotionSpeed);

    // Hotkey bindings (keyboard + gamepad), keyed by name as "key<Name>"/"gamepad<Name>".
    for (int i = 0; i < LIBRETRO_HOTKEY_COUNT; i++) {
        const char* shortName = TextReplace(menu.hotkeys[i].name, " ", "");
        rlconfig_set_int(menu.cfg, "raylib-libretro", TextFormat("key%s", shortName), (int)menu.hotkeys[i].key);
        rlconfig_set_int(menu.cfg, "raylib-libretro", TextFormat("gamepad%s", shortName), (int)menu.hotkeys[i].gamepad);
    }

    rlconfig_set(menu.cfg, "raylib-libretro", "coreDirectory", LibretroResolveAbsoluteDirectory(LIBRETRO.coreDirectory));
    rlconfig_set(menu.cfg, "raylib-libretro", "saveDirectory", LibretroResolveAbsoluteDirectory(LIBRETRO.saveDirectory));
    rlconfig_set(menu.cfg, "raylib-libretro", "coreAssetsDirectory", LibretroResolveAbsoluteDirectory(LIBRETRO.coreAssetsDirectory));
    rlconfig_set(menu.cfg, "raylib-libretro", "systemDirectory", LibretroResolveAbsoluteDirectory(LIBRETRO.systemDirectory));
    rlconfig_set(menu.cfg, "raylib-libretro", "playlistsDirectory", LibretroResolveAbsoluteDirectory(LIBRETRO.playlistsDirectory));
    rlconfig_set(menu.cfg, "raylib-libretro", "fileBrowserStartDirectory", LibretroResolveAbsoluteDirectory(LIBRETRO.fileBrowserStartDirectory));

    for (int i = 0; i <= RETRO_DEVICE_ID_JOYPAD_R3; i++) {
        rlconfig_set_int(menu.cfg, "raylib-libretro", TextFormat("keyboard%d", i), LIBRETRO.keyboardPlayer1[i]);
    }
}

static bool SaveLibretroMenuSettings(void) {
    if (!menu.cfg) return false;
    LibretroMenuUpdateConfig();
    bool ok = rlconfig_save(menu.cfg, RAYLIB_LIBRETRO_CFG_FILE);
    TraceLog(LOG_INFO, "MENU: Saved menu settings to %s", RAYLIB_LIBRETRO_CFG_FILE);
    return ok;
}

static bool SaveLibretroCoreOptions(void) {
    const char *coreName = LIBRETRO.core.libraryName;
    if (!menu.cfg || LIBRETRO.core.variableCount == 0 || !coreName || !coreName[0]) return false;
    LibretroMenuWriteCoreOptions();
    bool ok = rlconfig_save(menu.cfg, RAYLIB_LIBRETRO_CFG_FILE);
    TraceLog(LOG_INFO, "LIBRETRO: Saved core options to %s", RAYLIB_LIBRETRO_CFG_FILE);
    return ok;
}

bool LoadLibretroCoreOptions(void) {
    if (!menu.cfg) return false;
    const char *coreName = LIBRETRO.core.libraryName;
    if (!coreName || !coreName[0]) return false;
    int loaded = 0;
    for (unsigned i = 0; i < LIBRETRO.core.variableCount; i++) {
        const char *val = rlconfig_get(menu.cfg, coreName, LIBRETRO.core.variableKeys[i]);
        if (!val) continue;
        // Reject a saved value the core no longer offers (e.g. a value renamed
        // or removed by a core update) rather than feeding the core an invalid
        // option string. The value already holds its default from registration,
        // so skipping leaves it correct. An empty values list means the core
        // declared no choices to validate against, so apply the saved value as-is.
        const char *valuesList = LIBRETRO.core.variableValuesList[i];
        if (valuesList && valuesList[0] && !LibretroMenuValueInList(valuesList, val)) {
            TraceLog(LOG_WARNING, "LIBRETRO: Ignoring saved value '%s' for '%s'; not offered by core, using default '%s'",
                val, LIBRETRO.core.variableKeys[i], LIBRETRO.core.variableDefaults[i]);
            continue;
        }
        if (SetLibretroCoreOption(LIBRETRO.core.variableKeys[i], val)) loaded++;
    }
    TraceLog(LOG_INFO, "LIBRETRO: Loaded %d core option(s) from %s", loaded, RAYLIB_LIBRETRO_CFG_FILE);
    return loaded > 0;
}

/**
 * Save the configured port devices to the config in [core.controllers]
 */
static bool SaveLibretroPortDevices(void) {
    if (!menu.cfg) return false;
    const char* coreName = LIBRETRO.core.libraryName;
    if (!coreName || !coreName[0]) return false;
    unsigned count = 0;
    const struct retro_controller_info* info = GetLibretroControllerInfo(&count);
    if (!info || count == 0) return false;

    char section[256];
    TextCopy(section, TextFormat("%s.controllers", coreName));
    rlconfig_clear_section(menu.cfg, section);
    for (unsigned port = 0; port < count && port < 16; port++) {
        if (info[port].num_types <= 1) continue;
        rlconfig_set_int(menu.cfg, section, TextFormat("port%u", port), (int)GetLibretroPortDevice(port));
    }
    return true;
}

static bool LoadLibretroPortDevices(void) {
    if (!menu.cfg) return false;
    const char* coreName = LIBRETRO.core.libraryName;
    if (!coreName || !coreName[0]) return false;
    unsigned count = 0;
    const struct retro_controller_info* info = GetLibretroControllerInfo(&count);
    if (!info || count == 0) return false;

    char section[256];
    TextCopy(section, TextFormat("%s.controllers", coreName));
    int loaded = 0;
    for (unsigned port = 0; port < count && port < 16; port++) {
        if (info[port].num_types <= 1) continue;
        int saved = rlconfig_get_int(menu.cfg, section, TextFormat("port%u", port), -1);
        if (saved < 0) continue;
        // Only apply a device the core still offers for this port; a core update may have renamed or dropped the saved device id.
        for (unsigned i = 0; i < info[port].num_types; i++) {
            if (info[port].types[i].id == (unsigned)saved) {
                SetLibretroPortDevice(port, (unsigned)saved);
                loaded++;
                break;
            }
        }
    }
    TraceLog(LOG_INFO, "MENU: Loaded %d controller port device(s) from %s", loaded, RAYLIB_LIBRETRO_CFG_FILE);
    return loaded > 0;
}

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE // expose to JS as Module._SaveLibretroAllSettings
#endif
bool SaveLibretroAllSettings(void) {
    if (!menu.cfg) return false;
    LibretroMenuUpdateConfig();
    LibretroMenuWriteCoreOptions();
    SaveLibretroPortDevices();
    bool ok = rlconfig_save(menu.cfg, RAYLIB_LIBRETRO_CFG_FILE);
    TraceLog(LOG_INFO, "MENU: Saved all settings to %s", RAYLIB_LIBRETRO_CFG_FILE);
    MenuSaveGameSRAM();
    LibretroFlushPersistentStorage();
    return ok;
}

static bool LoadLibretroMenuSettings(void) {
    if (!menu.cfg) return false;

    // Full Screen
    nk_bool savedFullscreen = (nk_bool)rlconfig_get_int(menu.cfg, "raylib-libretro", "fullscreen", (int)menu.fullscreen);
    menu.fullscreen = savedFullscreen;
#ifdef __EMSCRIPTEN__
    // Queue a deferred fullscreen request so the saved preference is applied on
    // the first user interaction after page load (the only timing browsers allow).
    if (savedFullscreen) {
        LibretroMenuRequestFullscreen(1);
    }
#else
    if (savedFullscreen != (nk_bool)IsWindowFullscreen()) ToggleFullscreen();
#endif

    // Window Size
    if (!savedFullscreen) {
        int w = rlconfig_get_int(menu.cfg, "raylib-libretro", "windowWidth", 0);
        int h = rlconfig_get_int(menu.cfg, "raylib-libretro", "windowHeight", 0);
        if (w > 0 && h > 0) SetWindowSize(w, h);
    }

    // VSYNC
    menu.vsync = (nk_bool)rlconfig_get_int(menu.cfg, "raylib-libretro", "vsync", (int)menu.vsync);
    menu.fpsIndex = rlconfig_get_int(menu.cfg, "raylib-libretro", "fps", menu.fpsIndex);
    if (menu.fpsIndex < 0 || menu.fpsIndex > 6) menu.fpsIndex = 0;
    LibretroMenuApplyVideoSettings();

    // Shader
    menu.shaderSelectedIndex = rlconfig_get_int(menu.cfg, "raylib-libretro", "shader", LIBRETRO_SHADER_NONE);
    if (menu.shaderSelectedIndex < 0 || menu.shaderSelectedIndex >= LIBRETRO_SHADER_TYPE_COUNT) {
        menu.shaderSelectedIndex = LIBRETRO_SHADER_NONE;
    }
    SetActiveLibretroShader((LibretroShaderType)menu.shaderSelectedIndex);

    // Texture Filter
    LIBRETRO.textureFilter = rlconfig_get_int(menu.cfg, "raylib-libretro", "textureFilter", 0);
    if (LIBRETRO.textureFilter < 0 || LIBRETRO.textureFilter > TEXTURE_FILTER_ANISOTROPIC_16X)
        LIBRETRO.textureFilter = 0;

    // Integer Scaling
    LIBRETRO.integerScaling = (bool)rlconfig_get_int(menu.cfg, "raylib-libretro", "integerScaling", 0);

    // Theme
    menu.themeSelectedIndex = rlconfig_get_int(menu.cfg, "raylib-libretro", "theme", 0);
    if (menu.themeSelectedIndex < 0 || menu.themeSelectedIndex >= LIBRETRO_MENU_STYLE_COUNT)
        menu.themeSelectedIndex = 0;
    SetLibretroMenuStyle((LibretroMenuStyle)menu.themeSelectedIndex);

    // Volume
    float savedVolume = rlconfig_get_float(menu.cfg, "raylib-libretro", "volume", LIBRETRO.volume);
    // Migrate the legacy 0..100 integer storage to the 0.0..1.0 float scale.
    if (savedVolume > 1.0f) savedVolume /= 100.0f;
    SetLibretroVolume(savedVolume);

    // Rewind
    menu.rewindEnabled = rlconfig_get_int(menu.cfg, "raylib-libretro", "rewind", 0) > 0;

    // Analog to D-Pad
    LIBRETRO.analogToDpadIndex = rlconfig_get_int(menu.cfg, "raylib-libretro", "analogToDpad", 0);
    if (LIBRETRO.analogToDpadIndex < 0 || LIBRETRO.analogToDpadIndex > 2) LIBRETRO.analogToDpadIndex = 0;

    // Menu Gamepad Combo
    menu.menuComboIndex = rlconfig_get_int(menu.cfg, "raylib-libretro", "menuCombo", LIBRETRO_MENU_COMBO_SELECT_START);
    if (menu.menuComboIndex < 0 || menu.menuComboIndex >= LIBRETRO_MENU_COMBO_COUNT) menu.menuComboIndex = LIBRETRO_MENU_COMBO_SELECT_START;

    // Cursor
    menu.hideCursor = (nk_bool)(rlconfig_get_int(menu.cfg, "raylib-libretro", "hideCursor", 0) > 0);
    menu.lockCursor = (nk_bool)(rlconfig_get_int(menu.cfg, "raylib-libretro", "lockCursor", 0) > 0);

    // Touch Controls
#if defined(PLATFORM_WEB) || defined(__ANDROID__) || defined(PLATFORM_ANDROID)
    menu.touchControls = rlconfig_get_int(menu.cfg, "raylib-libretro", "touchControls", 1) > 0;
#else
    menu.touchControls = rlconfig_get_int(menu.cfg, "raylib-libretro", "touchControls", 0) > 0;
#endif

    // Screen Orientation (0 = Landscape, 1 = Portrait, 2 = Auto; applied on Android)
    menu.orientationIndex = rlconfig_get_int(menu.cfg, "raylib-libretro", "orientation", 0);
    if (menu.orientationIndex < 0 || menu.orientationIndex > 2) menu.orientationIndex = 0;

    // Save Slot
    menu.saveSlotIndex = rlconfig_get_int(menu.cfg, "raylib-libretro", "saveSlot", 0);
    menu.sramAutoSaveIndex = rlconfig_get_int(menu.cfg, "raylib-libretro", "sramAutoSave", menu.sramAutoSaveIndex);
    if (menu.saveSlotIndex < 0 || menu.saveSlotIndex > 9) menu.saveSlotIndex = 0;

    // Username
    LibretroMenuLoadString("username", LIBRETRO.username);

    // Hotkey bindings (keyboard + gamepad), keyed by name as "key<Name>"/"gamepad<Name>".
    for (int i = 0; i < LIBRETRO_HOTKEY_COUNT; i++) {
        const char* shortName = TextReplace(menu.hotkeys[i].name, " ", "");
        menu.hotkeys[i].key = (nk_rune)rlconfig_get_int(menu.cfg, "raylib-libretro", TextFormat("key%s", shortName), (int)menu.hotkeys[i].key);
        menu.hotkeys[i].gamepad = (enum nk_gamepad_button)rlconfig_get_int(menu.cfg, "raylib-libretro", TextFormat("gamepad%s", shortName), (int)menu.hotkeys[i].gamepad);
    }
    SetExitKey(LibretroHotkeyToKeyboardKey(menu.hotkeys[LIBRETRO_HOTKEY_QUIT].key));

    // Fast Forward
    menu.fastForwardSpeed = rlconfig_get_float(menu.cfg, "raylib-libretro", "fastForwardSpeed", menu.fastForwardSpeed);
    if (menu.fastForwardSpeed < 1.1f) menu.fastForwardSpeed = 1.1f;
    if (menu.fastForwardSpeed > 10.0f) menu.fastForwardSpeed = 10.0f;

    // Slow Motion
    menu.slowMotionSpeed = rlconfig_get_float(menu.cfg, "raylib-libretro", "slowMotionSpeed", menu.slowMotionSpeed);
    // Migrate the legacy x10 integer storage (1..9) to the 0.1..0.9 float scale.
    if (menu.slowMotionSpeed > 0.95f) menu.slowMotionSpeed /= 10.0f;
    if (menu.slowMotionSpeed < 0.1f) menu.slowMotionSpeed = 0.1f;
    if (menu.slowMotionSpeed > 0.9f) menu.slowMotionSpeed = 0.9f;

    // Directories
    LibretroMenuLoadString("coreDirectory", LIBRETRO.coreDirectory);
    LibretroMenuLoadString("saveDirectory", LIBRETRO.saveDirectory);
    LibretroMenuLoadString("coreAssetsDirectory", LIBRETRO.coreAssetsDirectory);
    LibretroMenuLoadString("systemDirectory", LIBRETRO.systemDirectory);
    LibretroMenuLoadString("playlistsDirectory", LIBRETRO.playlistsDirectory);
    LibretroMenuLoadString("fileBrowserStartDirectory", LIBRETRO.fileBrowserStartDirectory);

    // Keyboard Controls
    for (int i = 0; i <= RETRO_DEVICE_ID_JOYPAD_R3; i++) {
        LIBRETRO.keyboardPlayer1[i] = rlconfig_get_int(menu.cfg, "raylib-libretro", TextFormat("keyboard%d", i), LIBRETRO.keyboardPlayer1[i]);
    }

    TraceLog(LOG_INFO, "MENU: Loaded menu settings from %s", RAYLIB_LIBRETRO_CFG_FILE);
    return true;
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

    FreeLibretroCoreInfos();

    rlconfig_free(menu.cfg);
    menu.cfg = NULL;
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
    if (menu.controllersMenu && !IsLibretroReady()) {
        menu.controllersMenu->visible = nk_false;
    }
    if (menu.saveStateButton) {
        menu.saveStateButton->parent->visible = gameReady && GetLibretroSerializedSize() > 0;
    }
    if (menu.resumeButton) menu.resumeButton->visible = gameReady;
    if (menu.resetGameButton) menu.resetGameButton->visible = gameReady;
    if (menu.cheatsMenuButton) menu.cheatsMenuButton->visible = gameReady;
}

static nk_bool LibretroHotkeyGPReleased(enum nk_gamepad_button btn) {
    return btn != NK_GAMEPAD_BUTTON_INVALID && nk_gamepad_is_button_released(&menu.gamepads, -1, btn);
}
static nk_bool LibretroHotkeyGPDown(enum nk_gamepad_button btn) {
    return btn != NK_GAMEPAD_BUTTON_INVALID && nk_gamepad_is_button_down(&menu.gamepads, -1, btn);
}
static nk_bool LibretroHotkeyGPPressed(enum nk_gamepad_button btn) {
    return btn != NK_GAMEPAD_BUTTON_INVALID && nk_gamepad_is_button_pressed(&menu.gamepads, -1, btn);
}

void UpdateLibretroMenu(void) { // If there is no menu, skip.
    if (menu.ctx == NULL) {
        return;
    }

    MenuTickSRAMAutoSave();

    // Update the gamepad state, so that menu inputs can still be found.
    nk_gamepad_update(nk_console_get_gamepads(menu.console));

    // Menu Guide Button
    if (IsGamepadButtonReleased(0, GAMEPAD_BUTTON_MIDDLE) ||
            IsGamepadButtonReleased(1, GAMEPAD_BUTTON_MIDDLE) ||
            IsGamepadButtonReleased(3, GAMEPAD_BUTTON_MIDDLE) ||
            IsGamepadButtonReleased(4, GAMEPAD_BUTTON_MIDDLE)) {
        ToggleLibretroMenu();
    }

    // Menu Controller Combo
    if (menu.menuComboIndex > 0) {
        for (int gp = 0; gp < 4; gp++) {
            if (IsGamepadAvailable(gp) && LibretroMenuComboTriggered(gp)) ToggleLibretroMenu();
        }
    }

    // Menu Key
    if ((!menu.disableHotKeysActive && IsKeyReleased(LibretroHotkeyToKeyboardKey(menu.hotkeys[LIBRETRO_HOTKEY_MENU].key)) || LibretroHotkeyGPReleased(menu.hotkeys[LIBRETRO_HOTKEY_MENU].gamepad)) && !menu.active) {
        ShowLibretroMenu();
    }

    if (!menu.active && IsLibretroGameReady() && IsWindowMinimized()) {
        ShowLibretroMenu();
    }

    // Track whether the menu just opened this frame for the rebuild logic below.
    // Updated before the early-return so closing the menu resets the flag; a
    // fresh open is then detected on every open, not just the first of the
    // session. (Otherwise menuWasActive latches true forever and the core
    // options visibility callback below never re-fires after a game loads.)
    static bool menuWasActive = false;
    bool menuJustOpened = menu.active && !menuWasActive;
    menuWasActive = menu.active;

    if (!menu.active) {
        return;
    }

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
        BuildLibretroMenuControllers(&menu);
        LIBRETRO.core.variablesVisibilityDirty = false;
        lastBuiltVariableCount = LIBRETRO.core.variableCount;
        TextCopy(lastBuiltCoreName, LIBRETRO.core.libraryName);
    }

    UpdateLibretroMenuVisibility();

    menu.shaderSelectedIndex = (int)GetActiveLibretroShaderType();

    // Scaling
    float scaling = (GetScreenWidth() >= 3840) ? 6.0f :
            (GetScreenWidth() >= 2560) ? 5.0f :
            (GetScreenWidth() >= 1280) ? 4.0f :
            (GetScreenWidth() >= 620) ? 3.0f : 2.0f; // Always use at least 2X scaling.
    SetNuklearScaling(menu.ctx, scaling);

    // Back gesture: swipe from left to right to navigate back.
    if (menu.touchControls && GetGestureDetected() == GESTURE_SWIPE_RIGHT) {
        nk_console_navigate_back(nk_console_active_parent(menu.console));
    }

    // Input & Update
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
