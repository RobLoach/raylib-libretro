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

#if defined(__ANDROID__)
#include <jni.h>
#include <android_native_app_glue.h>
// Provided by raylib's Android backend (rcore_android.c); not in raylib.h.
extern struct android_app *GetAndroidApp(void);
#endif

#ifdef __EMSCRIPTEN__
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
} AppData;

#if defined(__ANDROID__)
// Copy a single bundled core out of the read-only APK assets into the writable
// data directory so it can be dlopen()'d. No-op if it was already copied.
static void AndroidCopyCore(const char* coreName, const char* destDir) {
    char destPath[RAYLIB_LIBRETRO_VFS_MAX_PATH];
    char assetPath[RAYLIB_LIBRETRO_VFS_MAX_PATH];
    TextCopy(destPath, TextFormat("%s/%s", destDir, coreName));
    TextCopy(assetPath, TextFormat("cores/%s", coreName));

    if (FileExists(destPath)) return;  // already installed on a previous launch

    int size = 0;
    unsigned char* data = LoadFileData(assetPath, &size);  // reads from APK assets
    if (data == NULL || size <= 0) {
        TraceLog(LOG_WARNING, "ANDROID: Bundled core missing from assets: %s", assetPath);
        return;
    }
    if (SaveFileData(destPath, data, size)) {
        TraceLog(LOG_INFO, "ANDROID: Installed core %s", destPath);
    } else {
        TraceLog(LOG_ERROR, "ANDROID: Failed to install core %s", destPath);
    }
    UnloadFileData(data);
}

// On API 30+ ROM browsing across shared storage needs All-Files Access, which
// can only be granted from a system settings screen. Open it once if we don't
// already hold the permission. Older devices rely on legacy external storage.
static void AndroidRequestStorageAccess(struct android_app* app) {
    JavaVM* vm = app->activity->vm;
    JNIEnv* env = NULL;
    (*vm)->AttachCurrentThread(vm, &env, NULL);

    jclass versionClass = (*env)->FindClass(env, "android/os/Build$VERSION");
    jfieldID sdkIntField = (*env)->GetStaticFieldID(env, versionClass, "SDK_INT", "I");
    jint sdkInt = (*env)->GetStaticIntField(env, versionClass, sdkIntField);

    if (sdkInt >= 30) {
        jclass environmentClass = (*env)->FindClass(env, "android/os/Environment");
        jmethodID isManager = (*env)->GetStaticMethodID(env, environmentClass,
            "isExternalStorageManager", "()Z");
        jboolean granted = isManager ? (*env)->CallStaticBooleanMethod(env, environmentClass, isManager) : JNI_TRUE;

        if (!granted) {
            // Uri uri = Uri.parse("package:<applicationId>");
            jclass activityClass = (*env)->GetObjectClass(env, app->activity->clazz);
            jmethodID getPackageName = (*env)->GetMethodID(env, activityClass,
                "getPackageName", "()Ljava/lang/String;");
            jstring packageName = (jstring)(*env)->CallObjectMethod(env, app->activity->clazz, getPackageName);
            const char* packageUtf = (*env)->GetStringUTFChars(env, packageName, NULL);
            jstring uriString = (*env)->NewStringUTF(env, TextFormat("package:%s", packageUtf));
            (*env)->ReleaseStringUTFChars(env, packageName, packageUtf);

            jclass uriClass = (*env)->FindClass(env, "android/net/Uri");
            jmethodID uriParse = (*env)->GetStaticMethodID(env, uriClass, "parse",
                "(Ljava/lang/String;)Landroid/net/Uri;");
            jobject uri = (*env)->CallStaticObjectMethod(env, uriClass, uriParse, uriString);

            // Intent intent = new Intent(ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION, uri);
            jclass settingsClass = (*env)->FindClass(env, "android/provider/Settings");
            jfieldID actionField = (*env)->GetStaticFieldID(env, settingsClass,
                "ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION", "Ljava/lang/String;");
            jstring action = (jstring)(*env)->GetStaticObjectField(env, settingsClass, actionField);

            jclass intentClass = (*env)->FindClass(env, "android/content/Intent");
            jmethodID intentInit = (*env)->GetMethodID(env, intentClass, "<init>",
                "(Ljava/lang/String;Landroid/net/Uri;)V");
            jobject intent = (*env)->NewObject(env, intentClass, intentInit, action, uri);

            jmethodID startActivity = (*env)->GetMethodID(env, activityClass, "startActivity",
                "(Landroid/content/Intent;)V");
            (*env)->CallVoidMethod(env, app->activity->clazz, startActivity, intent);

            TraceLog(LOG_INFO, "ANDROID: Requested All-Files Access for ROM browsing");
        }
    }

    // Don't let a missing class/method on an odd ROM crash the app.
    if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
    (*vm)->DetachCurrentThread(vm);
}

static void AndroidEnableImmersiveMode(struct android_app* app);

// The desktop defaults (relative "cores"/"saves"/"system") are unusable on
// Android, where the working directory is "/". Point them at writable absolute
// paths, install the bundled cores so the menu can find them, and request the
// storage access needed to browse for ROMs.
static void SetupAndroidEnvironment(LibretroMenu* menu) {
    struct android_app* app = GetAndroidApp();
    if (app == NULL || app->activity == NULL || app->activity->internalDataPath == NULL) {
        TraceLog(LOG_WARNING, "ANDROID: No activity data path; skipping environment setup");
        return;
    }
    const char* dataDir = app->activity->internalDataPath;
    const char* externalDir = app->activity->externalDataPath;

    // Only override values still at the relative desktop defaults — an absolute
    // path means the user (or a saved config) already chose a directory.
    if (menu->coreDirectory[0]   != '/') TextCopy(menu->coreDirectory,   TextFormat("%s/cores", dataDir));
    if (menu->saveDirectory[0]   != '/') TextCopy(menu->saveDirectory,   TextFormat("%s/saves", dataDir));
    if (menu->systemDirectory[0] != '/') TextCopy(menu->systemDirectory, TextFormat("%s/system", dataDir));
    if (menu->fileBrowserStartDirectory[0] == '\0' && externalDir != NULL) {
        TextCopy(menu->fileBrowserStartDirectory, externalDir);
    }

    MakeDirectory(menu->coreDirectory);
    MakeDirectory(menu->saveDirectory);
    MakeDirectory(menu->systemDirectory);

    // Install each core listed in the build-generated assets/cores.list.
    char* list = LoadFileText("cores.list");
    if (list != NULL) {
        int count = 0;
        const char** lines = TextSplit(list, '\n', &count);
        for (int i = 0; i < count; i++) {
            char coreName[256];
            TextCopy(coreName, lines[i]);
            int len = TextLength(coreName);
            if (len > 0 && coreName[len - 1] == '\r') coreName[len - 1] = '\0';
            if (coreName[0] == '\0') continue;
            AndroidCopyCore(coreName, menu->coreDirectory);
        }
        UnloadFileText(list);
    } else {
        TraceLog(LOG_WARNING, "ANDROID: cores.list missing; no bundled cores to install");
    }

    // Re-apply the directories and rescan now that the cores are on disk.
    LibretroApplyDirectories();
    ScanLibretroCoreDirectory();
    if (menu->loadGameWidget != NULL && menu->fileBrowserStartDirectory[0] != '\0') {
        nk_console_file_set_directory(menu->loadGameWidget, menu->fileBrowserStartDirectory);
    }

    AndroidRequestStorageAccess(app);
    AndroidEnableImmersiveMode(app);
}

// Enable immersive fullscreen mode: hide navigation and status bars so the
// game gets the entire display. Called once at startup; IMMERSIVE_STICKY means
// the bars stay hidden until the user explicitly swipes them back.
static void AndroidEnableImmersiveMode(struct android_app* app) {
    JavaVM* vm = app->activity->vm;
    JNIEnv* env = NULL;
    (*vm)->AttachCurrentThread(vm, &env, NULL);

    jobject activity = app->activity->clazz;
    jclass activityClass = (*env)->GetObjectClass(env, activity);

    jmethodID getWindow = (*env)->GetMethodID(env, activityClass, "getWindow", "()Landroid/view/Window;");
    jobject window = (*env)->CallObjectMethod(env, activity, getWindow);
    jclass windowClass = (*env)->GetObjectClass(env, window);

    jmethodID getDecorView = (*env)->GetMethodID(env, windowClass, "getDecorView", "()Landroid/view/View;");
    jobject decorView = (*env)->CallObjectMethod(env, window, getDecorView);
    jclass viewClass = (*env)->GetObjectClass(env, decorView);

    jmethodID setSystemUiVisibility = (*env)->GetMethodID(env, viewClass, "setSystemUiVisibility", "(I)V");
    jint flags =
        0x00000002 |  // SYSTEM_UI_FLAG_HIDE_NAVIGATION
        0x00000004 |  // SYSTEM_UI_FLAG_FULLSCREEN
        0x00001000 |  // SYSTEM_UI_FLAG_IMMERSIVE_STICKY
        0x00000100 |  // SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
        0x00000200 |  // SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
        0x00000400;   // SYSTEM_UI_FLAG_LAYOUT_STABLE
    (*env)->CallVoidMethod(env, decorView, setSystemUiVisibility, flags);

    // On Android P (API 28+), render into the display cutout (notch) area.
    jclass versionClass = (*env)->FindClass(env, "android/os/Build$VERSION");
    jfieldID sdkIntField = (*env)->GetStaticFieldID(env, versionClass, "SDK_INT", "I");
    jint sdkInt = (*env)->GetStaticIntField(env, versionClass, sdkIntField);
    if (sdkInt >= 28) {
        jmethodID getAttributes = (*env)->GetMethodID(env, windowClass, "getAttributes",
            "()Landroid/view/WindowManager$LayoutParams;");
        jobject attrs = (*env)->CallObjectMethod(env, window, getAttributes);
        jclass attrsClass = (*env)->GetObjectClass(env, attrs);
        jfieldID cutoutField = (*env)->GetFieldID(env, attrsClass, "layoutInDisplayCutoutMode", "I");
        (*env)->SetIntField(env, attrs, cutoutField, 1);  // LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES
        jmethodID setAttributes = (*env)->GetMethodID(env, windowClass, "setAttributes",
            "(Landroid/view/WindowManager$LayoutParams;)V");
        (*env)->CallVoidMethod(env, window, setAttributes, attrs);
    }

    if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
    (*vm)->DetachCurrentThread(vm);
}
#endif  // __ANDROID__

bool Init(void** userData, int argc, char** argv) {
    Image logo = GetLibretroLogo();
    SetWindowIcon(logo);
    UnloadImage(logo);

    SetWindowMinSize(400, 300);
    SetExitKey(KEY_NULL);

    BeginDrawing();
        ClearBackground(BLACK);
        const char* loadingText = "Loading";
        int fontSize = 20;
        DrawText(loadingText,
            (GetScreenWidth()  - MeasureText(loadingText, fontSize)) / 2,
            (GetScreenHeight() - fontSize) / 2,
            fontSize, GRAY);
    EndDrawing();

    AppData* data = (AppData*)MemAlloc(sizeof(AppData));
    memset(data, 0, sizeof(AppData));
    *userData = data;

    InitAudioDevice();
    InitLibretroPhysFS();

    // Load the shaders and the menu.
    LoadLibretroShaders();
    data->menu = InitLibretroMenu();
    if (!data->menu) {
        TraceLog(LOG_ERROR, "Failed to initialize menu");
        UnloadLibretroShaders();
        CloseLibretroPhysFS();
        CloseAudioDevice();
        return false;
    }

#if defined(__ANDROID__)
    SetupAndroidEnvironment(data->menu);
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
        if (MenuInitCore(corePath) && LoadLibretroGameFromPhysFS(gameFile)) {
            BuildLibretroMenuOptions(data->menu);
            HideLibretroMenu();
        }
    } else if (gameFile) {
        if (MenuLoadGame(gameFile)) HideLibretroMenu(); else ShowLibretroMenu();
    }

    return true;
}

bool Update(void* userData) {
    AppData* data = (AppData*)userData;

    // Deferred menu open: apply after input has refreshed so the release event
    // that triggered the MENU touch button is gone before Nuklear processes input.
    if (data->pendingMenuOpen) {
        ShowLibretroMenu();
        data->pendingMenuOpen = false;
    }

    // Update virtual joypad from touch controls.
    if (data->menu->touchControls) {
        SetTouchHapticsEnabled(data->menu->touchHapticsEnabled);
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
            KeyboardKey rewindKey = LibretroHotkeyToKeyboardKey(data->menu->keyRewind);
            rewinding = data->menu->rewindEnabled && (IsKeyDown(rewindKey) || LibretroHotkeyGPDown(data->menu->gamepadRewind));

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
                    if (IsKeyReleased(rewindKey) || LibretroHotkeyGPReleased(data->menu->gamepadRewind)) {
                        SetLibretroMessage(NULL, 0.0);
                    }
                } else if (data->rewind.count > 0) {
                    RewindBufferFree(&data->rewind);
                }

                // Fast Forward
                KeyboardKey key = LibretroHotkeyToKeyboardKey(data->menu->keyFastForward);
                KeyboardKey slowMotionKey = LibretroHotkeyToKeyboardKey(data->menu->keySlowMotion);
                if (IsKeyDown(key) || LibretroHotkeyGPDown(data->menu->gamepadFastForward)) {
                    if (IsKeyPressed(key) || LibretroHotkeyGPPressed(data->menu->gamepadFastForward)) {
                        // Halve the volume during fast-forward to soften the
                        // sped-up audio and ring-buffer overrun crackle.
                        SetLibretroVolume(data->menu->volumeSelected * 0.5f);
                        SetLibretroSpeed(data->menu->fastForwardSpeed);
                        SetLibretroMessage("Fast Forward", 1.0);
                    }
                    // The time accumulator in UpdateLibretro() scales by LIBRETRO.speed,
                    // so the single UpdateLibretro() call below runs the extra ticks.
                }
                else if (IsKeyReleased(key) || LibretroHotkeyGPReleased(data->menu->gamepadFastForward)) {
                    SetLibretroVolume(data->menu->volumeSelected);
                    SetLibretroSpeed(1.0f);
                    SetLibretroMessage(NULL, 0.0);
                }

                // Slow Motion
                else if (IsKeyDown(slowMotionKey) || LibretroHotkeyGPDown(data->menu->gamepadSlowMotion)) {
                    if (IsKeyPressed(slowMotionKey) || LibretroHotkeyGPPressed(data->menu->gamepadSlowMotion)) {
                        SetLibretroSpeed(data->menu->slowMotionSpeed);
                        SetLibretroMessage("Slow Motion", 1.0);
                    }
                }
                else if (IsKeyReleased(slowMotionKey) || LibretroHotkeyGPReleased(data->menu->gamepadSlowMotion)) {
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
    }
    if (data->menu->shouldQuit) {
        return false;
    }

    // Hot Keys
    if (!menu.active) {

        // Screenshot
        if (IsKeyReleased(LibretroHotkeyToKeyboardKey(menu.keyScreenshot)) || LibretroHotkeyGPReleased(menu.gamepadScreenshot)) {
            const char* screenshotsDir = GetLibretroDirectory(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY);
            const char* contentName = GetLibretroContentName();
            const char* baseName = (contentName && contentName[0] != '\0') ? contentName : "screenshot";
            bool taken = false;
            for (int i = 1; i < 1000; i++) {
                const char* screenshotName = TextFormat("%s/%s-%i.png", screenshotsDir, baseName, i);
                if (!FileExists(screenshotName)) {
                    Image screenshot = LoadImageFromTexture(GetLibretroTexture());
                    ExportImage(screenshot, screenshotName);
                    UnloadImage(screenshot);
                    SetLibretroMessage(TextFormat("Screenshot: %s", screenshotName), 2.0);
                    taken = true;
                    break;
                }
            }
            if (!taken) {
                SetLibretroMessage("Screenshot slots full", 2.0);
            }
        }

        // FullScreen toggle key won't work in Emscripten.
    #ifndef __EMSCRIPTEN__
        else if (IsKeyReleased(LibretroHotkeyToKeyboardKey(menu.keyFullscreen)) || LibretroHotkeyGPReleased(menu.gamepadFullscreen)) {
            LibretroMenuFullscreenChanged(menu.console, NULL);
        }
    #endif

        // Cycle Shader Reverse
        else if (IsKeyReleased(LibretroHotkeyToKeyboardKey(menu.keyPrevShader)) || LibretroHotkeyGPReleased(menu.gamepadPrevShader)) {
            CycleLibretroShaderReverse();
            SetLibretroMessage(GetLibretroShaderName(GetActiveLibretroShaderType()), 2.0);
            menu.shaderSelectedIndex = (int)GetActiveLibretroShaderType();
        }

        // Cycle Shader Next
        else if (IsKeyReleased(LibretroHotkeyToKeyboardKey(menu.keyNextShader)) || LibretroHotkeyGPReleased(menu.gamepadNextShader)) {
            CycleLibretroShader();
            SetLibretroMessage(GetLibretroShaderName(GetActiveLibretroShaderType()), 2.0);
            // TODO: For some reason, cycling the shader doens't update the menu label.
            menu.shaderSelectedIndex = (int)GetActiveLibretroShaderType();
        }

        // Save State
        else if (IsKeyReleased(LibretroHotkeyToKeyboardKey(menu.keySaveState)) || LibretroHotkeyGPReleased(menu.gamepadSaveState)) {
            LibretroMenuSaveStateClicked(menu.console, NULL);
        }

        // Load State
        else if (IsKeyReleased(LibretroHotkeyToKeyboardKey(menu.keyLoadState)) || LibretroHotkeyGPReleased(menu.gamepadLoadState)) {
            LibretroMenuLoadStateClicked(menu.console, NULL);
        }

        // Prev Slot
        else if (IsKeyReleased(LibretroHotkeyToKeyboardKey(menu.keyPrevSlot)) || LibretroHotkeyGPReleased(menu.gamepadPrevSlot)) {
            menu.saveSlotIndex = (menu.saveSlotIndex - 1 + 10) % 10;
            SetLibretroMessage(TextFormat("Save Slot: %d", menu.saveSlotIndex + 1), 2.0);
        }

        // Next Slot
        else if (IsKeyReleased(LibretroHotkeyToKeyboardKey(menu.keyNextSlot)) || LibretroHotkeyGPReleased(menu.gamepadNextSlot)) {
            menu.saveSlotIndex = (menu.saveSlotIndex + 1) % 10;
            SetLibretroMessage(TextFormat("Save Slot: %d", menu.saveSlotIndex + 1), 2.0);
        }

        // Reset
        else if (IsKeyReleased(LibretroHotkeyToKeyboardKey(menu.keyReset)) || LibretroHotkeyGPReleased(menu.gamepadReset)) {
            if (IsLibretroGameReady()) {
                ResetLibretro();
                SetLibretroMessage("Reset", 2.0);
            }
        }

        // Volume Up
        else if (IsKeyReleased(LibretroHotkeyToKeyboardKey(menu.keyVolumeUp)) || LibretroHotkeyGPReleased(menu.gamepadVolumeUp)) {
            float vol = GetLibretroVolume() + 0.1f;
            SetLibretroVolume(vol);
            vol = GetLibretroVolume();
            menu.volumeSelected = vol;
            SetLibretroMessage(TextFormat("Volume: %d%%", (int)(vol * 10.0f + 0.5f) * 10), 1.0);
        }

        // Volume Down
        else if (IsKeyReleased(LibretroHotkeyToKeyboardKey(menu.keyVolumeDown)) || LibretroHotkeyGPReleased(menu.gamepadVolumeDown)) {
            float vol = GetLibretroVolume() - 0.1f;
            SetLibretroVolume(vol);
            vol = GetLibretroVolume();
            menu.volumeSelected = vol;
            SetLibretroMessage(TextFormat("Volume: %d%%", (int)(vol * 10.0f + 0.5f) * 10), 1.0);
        }

        // Mute
        else if (IsKeyPressed(LibretroHotkeyToKeyboardKey(menu.keyMute)) || LibretroHotkeyGPReleased(menu.gamepadMute)) {
            if (!data->muted) {
                SetLibretroVolume(0.0f);
                data->muted = true;
                SetLibretroMessage("Mute", 1.0f);
            } else {
                SetLibretroVolume(data->menu->volumeSelected);
                data->muted = false;
                SetLibretroMessage(TextFormat("Volume: %d%%", (int)(data->menu->volumeSelected * 10.0f + 0.5f) * 10), 1.0);
            }
        }

        // Quit (keyboard is handled by SetExitKey; this covers the gamepad binding).
        else if (LibretroHotkeyGPReleased(menu.gamepadQuit)) {
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
