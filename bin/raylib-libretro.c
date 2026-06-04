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
#include <stdio.h>
#include <stdarg.h>
#include <android/log.h>
#include <android/window.h>            // AWINDOW_FLAG_* for ANativeActivity_setWindowFlags()
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
    float savedVolume;  // volume saved before fast-forward halves it
} AppData;

// Breadcrumb logging through startup. On Android these go to logcat (and the
// file log below); compiled out everywhere else so the desktop log stays clean.
#if defined(__ANDROID__)
#define INIT_TRACE(...) TraceLog(LOG_INFO, __VA_ARGS__)
#else
#define INIT_TRACE(...) ((void)0)
#endif

#if defined(__ANDROID__)
// --- Crash diagnostics -----------------------------------------------------
// raylib's Android logger writes to logcat; we additionally tee TraceLog output
// to a file in the app's data directory so the startup trail survives a crash
// and can be retrieved later (e.g. `adb pull /sdcard/Android/data/<pkg>/files/`)
// even when logcat wasn't being watched at the moment it died.
static FILE* AndroidLogFile = NULL;

static void AndroidTraceLog(int logLevel, const char* text, va_list args) {
    int prio = ANDROID_LOG_INFO;
    switch (logLevel) {
        case LOG_TRACE:   prio = ANDROID_LOG_VERBOSE; break;
        case LOG_DEBUG:   prio = ANDROID_LOG_DEBUG;   break;
        case LOG_INFO:    prio = ANDROID_LOG_INFO;    break;
        case LOG_WARNING: prio = ANDROID_LOG_WARN;    break;
        case LOG_ERROR:   prio = ANDROID_LOG_ERROR;   break;
        case LOG_FATAL:   prio = ANDROID_LOG_FATAL;   break;
        default: break;
    }
    // va_list is single-use; copy it before the second consumer.
    va_list fileArgs;
    va_copy(fileArgs, args);
    __android_log_vprint(prio, "raylib-libretro", text, args);
    if (AndroidLogFile != NULL) {
        vfprintf(AndroidLogFile, text, fileArgs);
        fputc('\n', AndroidLogFile);
        fflush(AndroidLogFile);  // flush per line so a hard crash can't lose the trail
    }
    va_end(fileArgs);
}

// Install the file logger as early as possible. Prefers the external data dir
// (pullable over adb without root), falling back to the internal one.
static void AndroidInitCrashLog(struct android_app* app) {
    if (app == NULL || app->activity == NULL) return;
    const char* dir = app->activity->externalDataPath;
    if (dir == NULL) dir = app->activity->internalDataPath;
    if (dir == NULL) return;
    const char* path = TextFormat("%s/raylib-libretro.log", dir);
    AndroidLogFile = fopen(path, "w");
    if (AndroidLogFile != NULL) {
        SetTraceLogCallback(AndroidTraceLog);
        TraceLog(LOG_INFO, "ANDROID: Crash log at %s", path);
    }
}

// Attach the calling thread to the JVM only if it isn't already attached, so we
// detach exactly the attachments we create. Returns NULL if no env is available.
static JNIEnv* AndroidJNIBegin(struct android_app* app, bool* outNeedDetach) {
    *outNeedDetach = false;
    if (app == NULL || app->activity == NULL || app->activity->vm == NULL) return NULL;
    JavaVM* vm = app->activity->vm;
    JNIEnv* env = NULL;
    jint res = (*vm)->GetEnv(vm, (void**)&env, JNI_VERSION_1_6);
    if (res == JNI_EDETACHED) {
        if ((*vm)->AttachCurrentThread(vm, &env, NULL) != 0 || env == NULL) return NULL;
        *outNeedDetach = true;
    } else if (res != JNI_OK || env == NULL) {
        return NULL;
    }
    return env;
}

static void AndroidJNIEnd(struct android_app* app, bool needDetach) {
    if (needDetach && app != NULL && app->activity != NULL && app->activity->vm != NULL) {
        (*app->activity->vm)->DetachCurrentThread(app->activity->vm);
    }
}

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
    bool needDetach = false;
    JNIEnv* env = AndroidJNIBegin(app, &needDetach);
    if (env == NULL) {
        TraceLog(LOG_WARNING, "ANDROID: Could not attach JNI for storage access");
        return;
    }

    // Every JNI lookup below is NULL-checked, and the pending-exception state is
    // cleared after each Java call, so a missing class/method on an odd ROM can
    // never feed a NULL handle (native SIGSEGV) or a pending exception into the
    // next JNI call. The whole helper degrades to a no-op instead of crashing.
    jint sdkInt = 0;
    jclass versionClass = (*env)->FindClass(env, "android/os/Build$VERSION");
    if (versionClass != NULL) {
        jfieldID sdkIntField = (*env)->GetStaticFieldID(env, versionClass, "SDK_INT", "I");
        if (sdkIntField != NULL) sdkInt = (*env)->GetStaticIntField(env, versionClass, sdkIntField);
    }
    if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);

    if (sdkInt >= 30) {
        jboolean granted = JNI_TRUE;
        jclass environmentClass = (*env)->FindClass(env, "android/os/Environment");
        if (environmentClass != NULL) {
            jmethodID isManager = (*env)->GetStaticMethodID(env, environmentClass,
                "isExternalStorageManager", "()Z");
            if (isManager != NULL) {
                granted = (*env)->CallStaticBooleanMethod(env, environmentClass, isManager);
            }
        }
        if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);

        if (!granted) {
            // Intent intent = new Intent(ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION,
            //                            Uri.parse("package:<applicationId>"));
            // activity.startActivity(intent);
            jclass  activityClass   = (*env)->GetObjectClass(env, app->activity->clazz);
            jmethodID getPackageName = activityClass ? (*env)->GetMethodID(env, activityClass,
                "getPackageName", "()Ljava/lang/String;") : NULL;
            jstring packageName = getPackageName ?
                (jstring)(*env)->CallObjectMethod(env, app->activity->clazz, getPackageName) : NULL;

            jclass  uriClass = (*env)->FindClass(env, "android/net/Uri");
            jmethodID uriParse = uriClass ? (*env)->GetStaticMethodID(env, uriClass, "parse",
                "(Ljava/lang/String;)Landroid/net/Uri;") : NULL;

            jclass  settingsClass = (*env)->FindClass(env, "android/provider/Settings");
            jfieldID actionField = settingsClass ? (*env)->GetStaticFieldID(env, settingsClass,
                "ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION", "Ljava/lang/String;") : NULL;
            jstring action = actionField ?
                (jstring)(*env)->GetStaticObjectField(env, settingsClass, actionField) : NULL;

            jclass  intentClass = (*env)->FindClass(env, "android/content/Intent");
            jmethodID intentInit = intentClass ? (*env)->GetMethodID(env, intentClass, "<init>",
                "(Ljava/lang/String;Landroid/net/Uri;)V") : NULL;
            jmethodID startActivity = activityClass ? (*env)->GetMethodID(env, activityClass,
                "startActivity", "(Landroid/content/Intent;)V") : NULL;

            if (packageName && uriParse && action && intentInit && startActivity) {
                const char* packageUtf = (*env)->GetStringUTFChars(env, packageName, NULL);
                jstring uriString = (*env)->NewStringUTF(env,
                    TextFormat("package:%s", packageUtf ? packageUtf : ""));
                if (packageUtf) (*env)->ReleaseStringUTFChars(env, packageName, packageUtf);

                jobject uri = (*env)->CallStaticObjectMethod(env, uriClass, uriParse, uriString);
                jobject intent = uri ? (*env)->NewObject(env, intentClass, intentInit, action, uri) : NULL;
                if (intent != NULL) {
                    (*env)->CallVoidMethod(env, app->activity->clazz, startActivity, intent);
                    TraceLog(LOG_INFO, "ANDROID: Requested All-Files Access for ROM browsing");
                }
            } else {
                TraceLog(LOG_WARNING, "ANDROID: Could not build All-Files Access intent");
            }
            if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
        }
    }

    AndroidJNIEnd(app, needDetach);
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

    INIT_TRACE("ANDROID/INIT: requesting storage access");
    AndroidRequestStorageAccess(app);
    INIT_TRACE("ANDROID/INIT: enabling immersive mode");
    AndroidEnableImmersiveMode(app);
    INIT_TRACE("ANDROID/INIT: environment setup complete");
}

// Make the game fill the screen and keep it awake during play. Status-bar
// hiding and notch/cutout rendering come declaratively from FullscreenTheme
// (res/values/styles.xml); raylib itself also sets AWINDOW_FLAG_FULLSCREEN.
//
// This deliberately avoids touching the view hierarchy (setSystemUiVisibility /
// Window.setAttributes): Init() runs on the android_main thread, not the UI
// thread, so those calls throw CalledFromWrongThreadException — they never
// applied, and on some ART builds the resulting pending exception aborted the
// process right after the "Loading" screen. ANativeActivity_setWindowFlags is
// the NDK-sanctioned, thread-safe equivalent.
static void AndroidEnableImmersiveMode(struct android_app* app) {
    if (app == NULL || app->activity == NULL) return;
    ANativeActivity_setWindowFlags(app->activity,
        AWINDOW_FLAG_FULLSCREEN | AWINDOW_FLAG_KEEP_SCREEN_ON, 0);
}
#endif  // __ANDROID__

bool Init(void** userData, int argc, char** argv) {
#if defined(__ANDROID__)
    // Start teeing the log to a file before anything that can crash, so the
    // startup trail is recoverable even without a live logcat.
    AndroidInitCrashLog(GetAndroidApp());
#endif

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

    TraceLog(LOG_INFO, "LIBRETRO: Init() complete");
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
            KeyboardKey rewindKey = LibretroHotkeyToKeyboardKey(data->menu->hotkeys[LIBRETRO_HOTKEY_REWIND].key);
            rewinding = data->menu->rewindEnabled && (IsKeyDown(rewindKey) || LibretroHotkeyGPDown(data->menu->hotkeys[LIBRETRO_HOTKEY_REWIND].gamepad));

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

                // Fast Forward
                KeyboardKey key = LibretroHotkeyToKeyboardKey(data->menu->hotkeys[LIBRETRO_HOTKEY_FAST_FORWARD].key);
                KeyboardKey slowMotionKey = LibretroHotkeyToKeyboardKey(data->menu->hotkeys[LIBRETRO_HOTKEY_SLOW_MOTION].key);
                if (IsKeyDown(key) || LibretroHotkeyGPDown(data->menu->hotkeys[LIBRETRO_HOTKEY_FAST_FORWARD].gamepad)) {
                    if (IsKeyPressed(key) || LibretroHotkeyGPPressed(data->menu->hotkeys[LIBRETRO_HOTKEY_FAST_FORWARD].gamepad)) {
                        // Halve the volume during fast-forward to soften the
                        // sped-up audio and ring-buffer overrun crackle.
                        data->savedVolume = LIBRETRO.volume;
                        SetLibretroVolume(LIBRETRO.volume * 0.5f);
                        SetLibretroSpeed(data->menu->fastForwardSpeed);
                        SetLibretroMessage("Fast Forward", 1.0);
                    }
                    // The time accumulator in UpdateLibretro() scales by LIBRETRO.speed,
                    // so the single UpdateLibretro() call below runs the extra ticks.
                }
                else if (IsKeyReleased(key) || LibretroHotkeyGPReleased(data->menu->hotkeys[LIBRETRO_HOTKEY_FAST_FORWARD].gamepad)) {
                    SetLibretroVolume(data->savedVolume);
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
    }
    if (data->menu->shouldQuit) {
        return false;
    }

    // Hot Keys
    if (!menu.active) {

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
                data->savedVolume = LIBRETRO.volume;
                SetLibretroVolume(0.0f);
                data->muted = true;
                SetLibretroMessage("Mute", 1.0f);
            } else {
                float vol = data->savedVolume > 0.0f ? data->savedVolume : 1.0f;
                SetLibretroVolume(vol);
                data->muted = false;
                SetLibretroMessage(TextFormat("Volume: %d%%", (int)(vol * 10.0f + 0.5f) * 10), 1.0);
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
