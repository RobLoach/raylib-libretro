/**********************************************************************************************
*
*   raylib-libretro - libretro frontend application using raylib.
*
*   LICENSE: GPL-3.0-or-later
*
**********************************************************************************************/

#include "raylib.h"

// On web, raylib-app's built-in loop (RaylibAppWebUpdate) calls WindowShouldClose()
// every frame, which raylib's web backend implements with emscripten_sleep(12).
// emscripten_sleep aborts unless the program is built with ASYNCIFY (which we avoid,
// since it force-enables DYNCALLS and breaks JS->wasm callbacks). WindowShouldClose()
// is always false on web anyway, so we supply our own entry point below that drives
// the emscripten main loop directly and never makes that call. Native keeps raylib-app's
// standard main().
#if defined(__EMSCRIPTEN__)
    #define RAYLIB_APP_NO_ENTRY
#endif
#define RAYLIB_APP_IMPLEMENTATION
#include "raylib-app.h"

#define RAYLIB_LIBRETRO_CONFIG_IMPLEMENTATION
#include "raylib-libretro-config.h"

#define RAYLIB_LIBRETRO_IMPLEMENTATION
#include "raylib-libretro.h"

#define PHYSFS_PLATFORM_RAYLIB
#define RAYLIB_PHYSFS_IMPLEMENTATION
#include "raylib-physfs.h"

#define RAYLIB_LIBRETRO_PHYSFS_IMPLEMENTATION
#include "../include/raylib-libretro-physfs.h"

#define RAYLIB_LIBRETRO_SHADERS_IMPLEMENTATION
#include "../include/raylib-libretro-shaders.h"

#define RAYLIB_LIBRETRO_MENU_IMPLEMENTATION
#include "../include/raylib-libretro-menu.h"

#define RAYLIB_LIBRETRO_TOUCH_IMPLEMENTATION
#include "../include/raylib-libretro-touch.h"

#define RAYLIB_LIBRETRO_LOGO_IMPLEMENTATION
#include "../include/raylib-libretro-logo.h"

#include "raylib-libretro-android.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>        // emscripten_set_main_loop / emscripten_cancel_main_loop
#include <emscripten/html5.h>

// JS-callable hot-load entry point. shell.html fetches ?game=<url> in the
// background and calls this once main() is running. If a core is already
// loaded (e.g. via ?core=) the game is loaded into it; otherwise the core
// is autodetected from the ROM extension, matching drag-and-drop behavior.
EMSCRIPTEN_KEEPALIVE
bool LoadLibretroGameFromJS(const char* gameFile) {
    if (gameFile == NULL || gameFile[0] == '\0') return false;
    // Save the SRAM, and close the game prior to loading the new one.
    MenuSaveGameSRAM();
    CloseLibretro();

    // Load the game through the menu system.
    return MenuLoadGame(gameFile);
}

// Called from shell.html on load/resize with physical-pixel dimensions
// (CSS size × devicePixelRatio) so the WebGL framebuffer matches the device
// resolution. raylib's own resize callback only sizes the canvas to CSS pixels,
// leaving HiDPI displays blurry/jagged; this overrides that with the true pixel
// size. SetWindowSize resizes the canvas backing store and GL viewport on web.
EMSCRIPTEN_KEEPALIVE
void ResizeCanvasFromJS(int width, int height) {
    if (width > 0 && height > 0) {
        SetWindowSize(width, height);
    }
}

#endif

#define REWIND_CAPTURES_PER_SECOND 30
#define REWIND_CAPTURE_INTERVAL (1.0f / REWIND_CAPTURES_PER_SECOND)  // seconds between snapshots
#define REWIND_BUFFER_FRAMES 600  // REWIND_BUFFER_FRAMES / REWIND_CAPTURES_PER_SECOND seconds of rewind

typedef struct {
    void* data;
    unsigned int size;
} RewindFrame;

typedef struct {
    RewindFrame frames[REWIND_BUFFER_FRAMES];
    int head;   // next write index
    int count;  // number of valid frames stored
} RewindBuffer;

static void RewindBufferPush(RewindBuffer* rb, void* data, unsigned int size) {
    int idx = rb->head;
    if (rb->frames[idx].data != NULL) {
        MemFree(rb->frames[idx].data);
    }
    rb->frames[idx].data = data;
    rb->frames[idx].size = size;
    rb->head = (rb->head + 1) % REWIND_BUFFER_FRAMES;
    if (rb->count < REWIND_BUFFER_FRAMES) rb->count++;
}

static bool RewindBufferPop(RewindBuffer* rb, void** outData, unsigned int* outSize) {
    if (rb->count <= 0) return false;
    rb->head = (rb->head - 1 + REWIND_BUFFER_FRAMES) % REWIND_BUFFER_FRAMES;
    rb->count--;
    *outData = rb->frames[rb->head].data;
    *outSize = rb->frames[rb->head].size;
    rb->frames[rb->head].data = NULL;
    rb->frames[rb->head].size = 0;
    return true;
}

static void RewindBufferFree(RewindBuffer* rb) {
    for (int i = 0; i < REWIND_BUFFER_FRAMES; i++) {
        if (rb->frames[i].data != NULL) {
            MemFree(rb->frames[i].data);
            rb->frames[i].data = NULL;
        }
    }
    rb->head = 0;
    rb->count = 0;
}

typedef struct {
    LibretroMenu* menu;
    RewindBuffer rewind;
    float rewindTimer;  // seconds accumulated toward the next rewind capture/step
    bool muted;
    bool pendingMenuOpen;
    int appliedOrientation;  // last orientation pushed to Android; -1 = none yet
} AppData;

// Breadcrumb logging through startup. On Android these go to logcat (and the
// file log below); compiled out everywhere else so the desktop log stays clean.
#if defined(__ANDROID__)
#define INIT_TRACE(...) TraceLog(LOG_INFO, __VA_ARGS__)
#else
#define INIT_TRACE(...) ((void)0)
#endif

#define RAYLIB_LIBRETRO_ANDROID_IMPLEMENTATION
#include "../include/raylib-libretro-android.h"

bool Init(void** userData, int argc, char** argv) {
#if defined(__ANDROID__)
    // Start teeing the log to a file before anything that can crash, so the
    // startup trail is recoverable even without a live logcat.
    AndroidInitCrashLog(GetAndroidApp());
#endif

    // Display loading screen
    MenuDrawLoadingScreen(NULL);

    // Window Icon
    Image logo = GetLibretroLogo();
    SetWindowIcon(logo);
    UnloadImage(logo);

    // Window Flags
    SetWindowMinSize(400, 300);
    SetExitKey(KEY_NULL);

    // Application data.
    AppData* data = (AppData*)MemAlloc(sizeof(AppData));
    memset(data, 0, sizeof(AppData));
    data->appliedOrientation = -1;  // force the first Update() to apply the saved orientation
    *userData = data;

    TraceLog(LOG_INFO, "LIBRETRO: Initializing Audio");
    InitAudioDevice();
    TraceLog(LOG_INFO, "LIBRETRO: Initializing physfs");
    InitLibretroPhysFS();

    // Load the shaders and the menu.
    TraceLog(LOG_INFO, "LIBRETRO: Initializing shaders");
    LoadLibretroShaders();
    TraceLog(LOG_INFO, "LIBRETRO: Initializing Menu");
    data->menu = InitLibretroMenu();
    if (!data->menu) {
        TraceLog(LOG_ERROR, "Failed to initialize menu");
        UnloadLibretroShaders();
        CloseLibretroPhysFS();
        CloseAudioDevice();
        return false;
    }

#if defined(__ANDROID__)
    INIT_TRACE("ANDROID/INIT: android environment");
    SetupAndroidEnvironment(data->menu);

    // If launched via "Open with" (ACTION_VIEW), load the game from the intent.
    char intentPath[RAYLIB_LIBRETRO_VFS_MAX_PATH];
    if (AndroidGetIntentFilePath(GetAndroidApp(), intentPath, sizeof(intentPath))) {
        TraceLog(LOG_INFO, "ANDROID: Intent file: %s", intentPath);
        if (MenuLoadGame(intentPath)) HideLibretroMenu(); else ShowLibretroMenu();
    }
#endif

    // Parse the command line arguments.
    // -L/--libretro <core> sets the core; the first non-flag argument is the game file.
    const char* corePath = NULL;
    const char* gameFile = NULL;
    for (int i = 1; i < argc; i++) {
        if (TextIsEqual(argv[i], "-h") || TextIsEqual(argv[i], "--help")) {
            printf("Usage: %s [-L <core>] [game]\n\n", argv[0]);
            printf("Options:\n");
            printf("  -L, --libretro <core>   Path to the libretro core (.so/.dll/.dylib)\n");
            printf("  -h, --help              Show this help message\n\n");
            printf("Examples:\n");
            printf("  %s -L fceumm_libretro.so smb.nes\n", argv[0]);
            printf("  %s -L fceumm_libretro.so smb.zip\n", argv[0]);
            printf("  %s smb.nes\n", argv[0]);
            return false;
        } else if ((TextIsEqual(argv[i], "-L") || TextIsEqual(argv[i], "--libretro")) && i + 1 < argc) {
            corePath = argv[++i];
        } else if (!gameFile) {
            gameFile = argv[i];
        }
    }

    if (corePath) {
        if (MenuInitCore(corePath)) {
            bool gameLoaded = gameFile
                ? LoadLibretroGameFromPhysFS(gameFile)
                : (!IsLibretroGameRequired() && LoadLibretroGame(NULL));
            if (gameLoaded) {
                BuildLibretroMenuOptions(data->menu);
                HideLibretroMenu();
            }
        }
    } else if (gameFile) {
        if (MenuLoadGame(gameFile)) HideLibretroMenu(); else ShowLibretroMenu();
    }

    TraceLog(LOG_INFO, "LIBRETRO: Init() complete");
    return true;
}

bool Update(void* userData) {
    AppData* data = (AppData*)userData;

#if defined(__ANDROID__)
    // Apply the menu's orientation choice when it changes (and the saved value
    // on the first frame, since appliedOrientation starts at -1).
    if (data->menu->orientationIndex != data->appliedOrientation) {
        data->appliedOrientation = data->menu->orientationIndex;
        AndroidSetOrientation(GetAndroidApp(), data->appliedOrientation);
    }
#endif

    // Deferred menu open: apply after input has refreshed so the release event
    // that triggered the MENU touch button is gone before Nuklear processes input.
    if (data->pendingMenuOpen) {
        ShowLibretroMenu();
        data->pendingMenuOpen = false;
    }

    // Update virtual joypad from touch controls.
    if (data->menu->touchControls) {
        SetTouchHapticsEnabled(data->menu->touchHapticsEnabled);
        SetTouchControlsScale(data->menu->touchScale);
        if (!data->menu->active) {
            UpdateLibretroTouchControls();
            if (IsTouchControlsMenuPressed()) {
                data->pendingMenuOpen = true;
            }
        } else {
            memset(LIBRETRO.core.virtualJoypadState, 0, sizeof(LIBRETRO.core.virtualJoypadState));
        }
    }

    // Run a frame of the core.
    if (!data->menu->active) {
        UpdateLibretroShaders(GetFrameTime());

        // While rewinding we step back one snapshot per capture interval and
        // render only on that step (via UpdateLibretroEx(true)), so the picture holds
        // between steps instead of running the core forward.
        bool rewinding = false;

        if (IsLibretroGameReady()) {
            KeyboardKey rewindKey = LibretroHotkeyToKeyboardKey(data->menu->hotkeys[LIBRETRO_HOTKEY_REWIND].key);
            rewinding = data->menu->rewindEnabled && !data->menu->disableHotKeysActive && (IsKeyDown(rewindKey) || LibretroHotkeyGPDown(data->menu->hotkeys[LIBRETRO_HOTKEY_REWIND].gamepad));

            // Capture and playback both run at REWIND_CAPTURES_PER_SECOND.
            if (data->menu->rewindEnabled) {
                data->rewindTimer += GetFrameTime();
            }

            if (rewinding) {
                if (data->rewindTimer >= REWIND_CAPTURE_INTERVAL) {
                    data->rewindTimer = 0.0f;
                    void* stateData = NULL;
                    unsigned int stateSize = 0;
                    if (RewindBufferPop(&data->rewind, &stateData, &stateSize)) {
                        if (stateSize != GetLibretroSerializedSize()) {
                            // Snapshots belong to a different game/core (a new
                            // game was loaded). Drop the stale buffer instead of
                            // feeding the core a mismatched state.
                            MemFree(stateData);
                            RewindBufferFree(&data->rewind);
                        } else {
                            SetLibretroSerializedData(stateData, stateSize);
                            MemFree(stateData);
                            SetLibretroMessage("Rewind", 1.0);
                            // Render the restored snapshot. Bypasses the time
                            // accumulator, which would run zero ticks when called
                            // this sparsely and leave the display frozen.
                            UpdateLibretroEx(true);
                        }
                    } else {
                        SetLibretroMessage("Rewind limit reached", 1.0);
                    }
                }
            } else {
                // Capture a snapshot at the interval while playing forward.
                if (data->menu->rewindEnabled) {
                    if (data->rewindTimer >= REWIND_CAPTURE_INTERVAL) {
                        data->rewindTimer = 0.0f;
                        unsigned int size = 0;
                        void* state = GetLibretroSerializedData(&size);
                        if (state != NULL) {
                            RewindBufferPush(&data->rewind, state, size);
                        }
                    }
                    if (IsKeyReleased(rewindKey) || LibretroHotkeyGPReleased(data->menu->hotkeys[LIBRETRO_HOTKEY_REWIND].gamepad)) {
                        SetLibretroMessage(NULL, 0.0);
                    }
                } else if (data->rewind.count > 0) {
                    RewindBufferFree(&data->rewind);
                }

                // Fast Forward / Slow Motion
                KeyboardKey key = LibretroHotkeyToKeyboardKey(data->menu->hotkeys[LIBRETRO_HOTKEY_FAST_FORWARD].key);
                KeyboardKey slowMotionKey = LibretroHotkeyToKeyboardKey(data->menu->hotkeys[LIBRETRO_HOTKEY_SLOW_MOTION].key);
                if (data->menu->disableHotKeysActive) {
                    // Skip the Fast Forward / Slow Motion entirely.
                }
                else if (IsKeyDown(key) || LibretroHotkeyGPDown(data->menu->hotkeys[LIBRETRO_HOTKEY_FAST_FORWARD].gamepad)) {
                    if (IsKeyPressed(key) || LibretroHotkeyGPPressed(data->menu->hotkeys[LIBRETRO_HOTKEY_FAST_FORWARD].gamepad)) {
                        // Halve audio stream volume during fast-forward without touching
                        // LIBRETRO.volume so restore is just re-applying the stored value.
                        if (IsAudioStreamValid(LIBRETRO.core.audioStream))
                            SetAudioStreamVolume(LIBRETRO.core.audioStream, LIBRETRO.volume * 0.5f);
                        SetLibretroSpeed(data->menu->fastForwardSpeed);
                        SetLibretroMessage("Fast Forward", 1.0);
                    }
                    // The time accumulator in UpdateLibretro() scales by LIBRETRO.speed,
                    // so the single UpdateLibretro() call below runs the extra ticks.
                }
                else if (IsKeyReleased(key) || LibretroHotkeyGPReleased(data->menu->hotkeys[LIBRETRO_HOTKEY_FAST_FORWARD].gamepad)) {
                    SetLibretroVolume(LIBRETRO.volume);
                    SetLibretroSpeed(1.0f);
                    SetLibretroMessage(NULL, 0.0);
                }

                // Slow Motion
                else if (IsKeyDown(slowMotionKey) || LibretroHotkeyGPDown(data->menu->hotkeys[LIBRETRO_HOTKEY_SLOW_MOTION].gamepad)) {
                    if (IsKeyPressed(slowMotionKey) || LibretroHotkeyGPPressed(data->menu->hotkeys[LIBRETRO_HOTKEY_SLOW_MOTION].gamepad)) {
                        SetLibretroSpeed(data->menu->slowMotionSpeed);
                        SetLibretroMessage("Slow Motion", 1.0);
                    }
                }
                else if (IsKeyReleased(slowMotionKey) || LibretroHotkeyGPReleased(data->menu->hotkeys[LIBRETRO_HOTKEY_SLOW_MOTION].gamepad)) {
                    SetLibretroSpeed(1.0f);
                    SetLibretroMessage(NULL, 0.0);
                }
            }
        }

        // Run a paced frame while playing forward. During rewind the core is
        // advanced by UpdateLibretroEx(true) above, so skip the accumulator here.
        if (!rewinding) {
            UpdateLibretro();
        }
    }

    UpdateLibretroMenu();

    // Handle drag-and-drop to load a game or core.
    if (IsFileDropped()) {
        FilePathList dropped = LoadDroppedFiles();
        if (dropped.count > 0) {
            const char* droppedPath = dropped.paths[0];
            SaveLibretroAllSettings();
            CloseLibretro();
            // The previous game's rewind snapshots are now meaningless.
            RewindBufferFree(&data->rewind);
            if (IsLibretroCoreFile(droppedPath)) {
                if (MenuInitCore(droppedPath)) {
                    BuildLibretroMenuOptions(data->menu);
                    ShowLibretroMenu();
                }
            } else {
                // MenuLoadGame autodetects a core for the dropped game via FindCoreForGame().
                if (MenuLoadGame(droppedPath)) HideLibretroMenu(); else ShowLibretroMenu();
            }
        }
        UnloadDroppedFiles(dropped);
    }

    // Check if the core or menu asks to be shutdown.
    if (LibretroShouldClose()) {
        RewindBufferFree(&data->rewind);
        SaveLibretroAllSettings();
        UnloadLibretroGame();
        CloseLibretro();
        ShowLibretroMenu();
    }
    if (data->menu->shouldQuit) {
        return false;
    }

    // Hot Key
    if (!menu.active) {
        // Disable HotKeys Button
        if (IsKeyReleased(LibretroHotkeyToKeyboardKey(menu.hotkeys[LIBRETRO_HOTKEY_DISABLE_HOTKEYS].key)) || LibretroHotkeyGPReleased(menu.hotkeys[LIBRETRO_HOTKEY_DISABLE_HOTKEYS].gamepad)) {
            menu.disableHotKeysActive = !menu.disableHotKeysActive;
            SetLibretroMessage(menu.disableHotKeysActive ? "Hot Keys Disabled" : "Hot Keys Enabled", 2.0);
            return true;
        }
    }

    // Check all the other Hot Keys
    if (!menu.active && !menu.disableHotKeysActive) {

        // Screenshot
        if (IsKeyReleased(LibretroHotkeyToKeyboardKey(menu.hotkeys[LIBRETRO_HOTKEY_SCREENSHOT].key)) || LibretroHotkeyGPReleased(menu.hotkeys[LIBRETRO_HOTKEY_SCREENSHOT].gamepad) && IsLibretroGameReady()) {
            const char* screenshotsDir = GetLibretroDirectory(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY);
            const char* contentName = GetLibretroContentName();
            const char* baseName = (contentName && contentName[0] != '\0') ? contentName : "screenshot";
            bool foundSlot = false;
            for (int i = 1; i < 1000; i++) {
                const char* screenshotName = TextFormat("%s/%s-%i.png", screenshotsDir, baseName, i);
                if (!FileExists(screenshotName)) {
                    //Image screenshot = LoadImageFromScreen();
                    //Image screenshot = LoadImageFromTexture(GetLibretroTexture());
                    Image screenshot = LoadImageFromLibretro();
                    foundSlot = true;
                    if (ExportImage(screenshot, screenshotName)) {
                        SetLibretroMessage(TextFormat("Screenshot: %s", screenshotName), 2.0);
                    }
                    else {
                        SetLibretroMessage(TextFormat("Screenshot failed: %s", screenshotName), 2.0);
                    }
                    if (screenshot.data != NULL) {
                        UnloadImage(screenshot);
                    }
                    break;
                }
            }
            if (!foundSlot) {
                SetLibretroMessage("Screenshot slots full", 2.0);
            }
        }

        // FullScreen toggle key won't work in Emscripten.
    #ifndef __EMSCRIPTEN__
        else if (IsKeyReleased(LibretroHotkeyToKeyboardKey(menu.hotkeys[LIBRETRO_HOTKEY_FULLSCREEN].key)) || LibretroHotkeyGPReleased(menu.hotkeys[LIBRETRO_HOTKEY_FULLSCREEN].gamepad)) {
            LibretroMenuFullscreenChanged(menu.console, NULL);
        }
    #endif

        // Cycle Shader Reverse
        else if (IsKeyReleased(LibretroHotkeyToKeyboardKey(menu.hotkeys[LIBRETRO_HOTKEY_PREV_SHADER].key)) || LibretroHotkeyGPReleased(menu.hotkeys[LIBRETRO_HOTKEY_PREV_SHADER].gamepad)) {
            CycleLibretroShaderReverse();
            SetLibretroMessage(GetLibretroShaderName(GetActiveLibretroShaderType()), 2.0);
            menu.shaderSelectedIndex = (int)GetActiveLibretroShaderType();
        }

        // Cycle Shader Next
        else if (IsKeyReleased(LibretroHotkeyToKeyboardKey(menu.hotkeys[LIBRETRO_HOTKEY_NEXT_SHADER].key)) || LibretroHotkeyGPReleased(menu.hotkeys[LIBRETRO_HOTKEY_NEXT_SHADER].gamepad)) {
            CycleLibretroShader();
            SetLibretroMessage(GetLibretroShaderName(GetActiveLibretroShaderType()), 2.0);
            // TODO: For some reason, cycling the shader doens't update the menu label.
            menu.shaderSelectedIndex = (int)GetActiveLibretroShaderType();
        }

        // Save State
        else if (IsKeyReleased(LibretroHotkeyToKeyboardKey(menu.hotkeys[LIBRETRO_HOTKEY_SAVE_STATE].key)) || LibretroHotkeyGPReleased(menu.hotkeys[LIBRETRO_HOTKEY_SAVE_STATE].gamepad)) {
            LibretroMenuSaveStateClicked(menu.console, NULL);
        }

        // Load State
        else if (IsKeyReleased(LibretroHotkeyToKeyboardKey(menu.hotkeys[LIBRETRO_HOTKEY_LOAD_STATE].key)) || LibretroHotkeyGPReleased(menu.hotkeys[LIBRETRO_HOTKEY_LOAD_STATE].gamepad)) {
            LibretroMenuLoadStateClicked(menu.console, NULL);
        }

        // Prev Slot
        else if (IsKeyReleased(LibretroHotkeyToKeyboardKey(menu.hotkeys[LIBRETRO_HOTKEY_PREV_SLOT].key)) || LibretroHotkeyGPReleased(menu.hotkeys[LIBRETRO_HOTKEY_PREV_SLOT].gamepad)) {
            menu.saveSlotIndex = (menu.saveSlotIndex - 1 + 10) % 10;
            SetLibretroMessage(TextFormat("Save Slot: %d", menu.saveSlotIndex + 1), 2.0);
        }

        // Next Slot
        else if (IsKeyReleased(LibretroHotkeyToKeyboardKey(menu.hotkeys[LIBRETRO_HOTKEY_NEXT_SLOT].key)) || LibretroHotkeyGPReleased(menu.hotkeys[LIBRETRO_HOTKEY_NEXT_SLOT].gamepad)) {
            menu.saveSlotIndex = (menu.saveSlotIndex + 1) % 10;
            SetLibretroMessage(TextFormat("Save Slot: %d", menu.saveSlotIndex + 1), 2.0);
        }

        // Reset
        else if (IsKeyReleased(LibretroHotkeyToKeyboardKey(menu.hotkeys[LIBRETRO_HOTKEY_RESET].key)) || LibretroHotkeyGPReleased(menu.hotkeys[LIBRETRO_HOTKEY_RESET].gamepad)) {
            if (IsLibretroGameReady()) {
                ResetLibretro();
                SetLibretroMessage("Reset", 2.0);
            }
        }

        // Volume Up
        else if (IsKeyReleased(LibretroHotkeyToKeyboardKey(menu.hotkeys[LIBRETRO_HOTKEY_VOLUME_UP].key)) || LibretroHotkeyGPReleased(menu.hotkeys[LIBRETRO_HOTKEY_VOLUME_UP].gamepad)) {
            float vol = GetLibretroVolume() + 0.1f;
            SetLibretroVolume(vol);
            vol = GetLibretroVolume();
            SetLibretroMessage(TextFormat("Volume: %d%%", (int)(vol * 10.0f + 0.5f) * 10), 1.0);
        }

        // Volume Down
        else if (IsKeyReleased(LibretroHotkeyToKeyboardKey(menu.hotkeys[LIBRETRO_HOTKEY_VOLUME_DOWN].key)) || LibretroHotkeyGPReleased(menu.hotkeys[LIBRETRO_HOTKEY_VOLUME_DOWN].gamepad)) {
            float vol = GetLibretroVolume() - 0.1f;
            SetLibretroVolume(vol);
            vol = GetLibretroVolume();
            SetLibretroMessage(TextFormat("Volume: %d%%", (int)(vol * 10.0f + 0.5f) * 10), 1.0);
        }

        // Mute
        else if (IsKeyPressed(LibretroHotkeyToKeyboardKey(menu.hotkeys[LIBRETRO_HOTKEY_MUTE].key)) || LibretroHotkeyGPReleased(menu.hotkeys[LIBRETRO_HOTKEY_MUTE].gamepad)) {
            if (!data->muted) {
                if (IsAudioStreamValid(LIBRETRO.core.audioStream))
                    SetAudioStreamVolume(LIBRETRO.core.audioStream, 0.0f);
                data->muted = true;
                SetLibretroMessage("Mute", 1.0f);
            } else {
                SetLibretroVolume(LIBRETRO.volume);
                data->muted = false;
                SetLibretroMessage(TextFormat("Volume: %d%%", (int)(LIBRETRO.volume * 10.0f + 0.5f) * 10), 1.0);
            }
        }

        // Quit (keyboard is handled by SetExitKey; this covers the gamepad binding).
        else if (LibretroHotkeyGPReleased(menu.hotkeys[LIBRETRO_HOTKEY_QUIT].gamepad)) {
            menu.shouldQuit = true;
        }
    }

    return true;
}

void Draw(void* userData) {
    AppData* data = (AppData*)userData;

    ClearBackground(BLACK);

    if (data->menu->active) {
        BeginLibretroShaderGreyscale();
        DrawLibretro();
        EndShaderMode();
    }
    else {
        BeginLibretroShader();
        DrawLibretro();
        EndLibretroShader();
    }

    DrawLibretroMenu();

    if (data->menu->touchControls && !data->menu->active) {
        DrawLibretroTouchControls();
    }

    DrawLibretroMessage();
}

void Close(void* userData) {
    AppData* data = (AppData*)userData;

    // Save all settings in one pass before closing.
    SaveLibretroAllSettings();

    // Free the rewind buffer.
    RewindBufferFree(&data->rewind);

    // Unload the game and close the core.
    UnloadLibretroGame();
    CloseLibretro();

    CloseLibretroMenu();

    UnloadLibretroShaders();
    CloseLibretroPhysFS();
    CloseAudioDevice();
    MemFree(data);
}

App Main() {
    return (App){
        .title = "raylib-libretro",
#if defined(__ANDROID__)
        .width = 0,   // use full display resolution on Android
        .height = 0,
#else
        .width = 1024,
        .height = 768,
#endif
        .init = Init,
        .update = Update,
        .draw = Draw,
        .close = Close,
        .configFlags = FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT,
    };
}

