/**********************************************************************************************
*
*   rlibretro - Raylib extension to interact with libretro cores.
*
*   DEPENDENCIES:
*            - raylib
*            - dl: dylib_proc(), dylib_error()
*            - libretro-common
*              - dynamic/dylib.h
*              - features/features_cpu.h
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

#ifndef RAYLIB_LIBRETRO_RLIBRETRO_H
#define RAYLIB_LIBRETRO_RLIBRETRO_H

#include "libretro.h"
#include "raylib.h"

#if defined(__cplusplus)
extern "C" {
#endif

//------------------------------------------------------------------------------------
// Libretro Raylib Integration (Module: rlibretro)
//------------------------------------------------------------------------------------

static bool InitLibretro(const char* core);              // Initialize the given libretro core.
static bool LoadLibretroGame(const char* gameFile);      // Load the provided content. Provide NULL to load the core without content.
static bool IsLibretroReady();                           // Whether or not the core was successfully loaded.
static bool IsLibretroGameReady();                       // Whether or not the game has been loaded.
static void UpdateLibretro();                            // Run an iteration of the core.
static bool LibretroShouldClose();                       // Check whether or not the core has requested to shutdown.
static void DrawLibretro();                              // Draw the libretro state on the screen.
static void DrawLibretroTint(Color tint);                // Draw the libretro state on the screen with a tint.
static void DrawLibretroEx(Vector2 position, float rotation, float scale, Color tint);
static void DrawLibretroV(Vector2 position, Color tint);
static void DrawLibretroTexture(int posX, int posY, Color tint);
static void DrawLibretroPro(Rectangle destRec, Color tint);
static const char* GetLibretroName();                    // Get the name of the loaded libretro core.
static const char* GetLibretroVersion();                 // Get the version of the loaded libretro core.
static unsigned GetLibretroWidth();                      // Get the desired width of the libretro core.
static unsigned GetLibretroHeight();                     // Get the desired height of the libretro core.
static unsigned GetLibretroRotation();                   // Get the current screen rotation (0=0°, 1=90°, 2=180°, 3=270°).
static Texture2D GetLibretroTexture();                   // Retrieve the texture used to render the libretro state.
static bool DoesLibretroCoreNeedContent();               // Determine whether or not the loaded core require content.
static void ResetLibretro();                             // Reset the currently loaded libretro core.
static void UnloadLibretroGame();                        // Unload the game that's currently loaded.
static void CloseLibretro();                             // Close the initialized libretro core.
static void SetLibretroVolume(float volume);             // Set the audio volume (0.0 - 1.0).
static float GetLibretroVolume();                        // Get the current audio volume.
static bool SetLibretroCoreOption(const char* key, const char* value);  // Set a core option by key.
static const char* GetLibretroCoreOption(const char* key);              // Get a core option value by key. Returns NULL if not found.
static bool SaveLibretroCoreOptions(void);  // Save current core's options to raylib-libretro.cfg (prefixed by core name).
static bool LoadLibretroCoreOptions(void);  // Load current core's options from raylib-libretro.cfg.
static void* GetLibretroSerializedData(unsigned int* size);     // Retrieve the serialized data of the save state. Must be MemFree()'d afterwards.
static bool SetLibretroSerializedData(void* data, unsigned int size);
static void ShowLibretroMessage(const char* msg, float duration); // Show an OSD message for the given duration in seconds.
static bool DrawLibretroMessage(); // Displays the OSD message on the screen. Returns true if there was one.

static void LibretroMapPixelFormatARGB1555ToRGB565(void *output_, const void *input_,
        int width, int height,
        int out_stride, int in_stride);
static void LibretroMapPixelFormatARGB8888ToRGBA8888(void *output_, const void *input_,
    int width, int height,
    int out_stride, int in_stride);

static int LibretroMapRetroPixelFormatToPixelFormat(int pixelFormat);
static int LibretroMapRetroJoypadButtonToGamepadButton(int button);
static int LibretroMapRetroJoypadButtonToRetroKey(int button);
static int LibretroMapRetroKeyToKeyboardKey(int key);
static int LibretroMapRetroLogLevelToTraceLogType(int level);

#if defined(__cplusplus)
}
#endif

#include "raylib-libretro-vfs.h"

#ifdef RAYLIB_LIBRETRO_IMPLEMENTATION
#ifndef RAYLIB_LIBRETRO_IMPLEMENTATION_ONCE
#define RAYLIB_LIBRETRO_IMPLEMENTATION_ONCE

// System dependencies
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

// libretro-common
#include <dynamic/dylib.h>
#include <features/features_cpu.h>

#define RAYLIB_LIBRETRO_VFS_IMPLEMENTATION
#include "raylib-libretro-vfs.h"

// Audio ring buffer size in stereo frames
#define LIBRETRO_AUDIO_RING_BUFFER_SIZE 8192
// Single-sample accumulation buffer size (stereo frames)
#define LIBRETRO_AUDIO_SINGLE_SAMPLE_BUFFER_SIZE 512
// Core options/variables storage limits
#define LIBRETRO_MAX_CORE_VARIABLES 128
#define LIBRETRO_CORE_VARIABLE_KEY_LEN 64
#define LIBRETRO_CORE_VARIABLE_VALUE_LEN 256
#define LIBRETRO_CORE_VARIABLE_LABEL_LEN 128   // human-readable option name
#define LIBRETRO_CORE_VARIABLE_VALUES_LEN 512   // pipe-separated list of valid values
#define LIBRETRO_CORE_VARIABLE_TOOLTIP_LEN 256  // option info/description text

// Shared config file used by SaveLibretroCoreOptions / LoadLibretroCoreOptions.
// Keys are prefixed with the core name: "CoreName.key=value"
#define RAYLIB_LIBRETRO_CFG_FILE "raylib-libretro.cfg"

// Dynamic loading methods.
#define LoadLibretroMethodHandle(V, S) do {\
    function_t func = dylib_proc(LibretroCore.handle, #S); \
    memcpy(&V, &func, sizeof(func)); \
    if (!func) { \
        TraceLog(LOG_ERROR, "LIBRETRO: Failed to load symbol '" #S "' ", dylib_error()); \
        return false; \
    }\
} while (0)
#define LoadLibretroMethod(S) LoadLibretroMethodHandle(LibretroCore.S, S)

typedef struct rLibretro {
    // Dynamic library symbols.
    void *handle;
    void (*retro_init)(void);
    void (*retro_deinit)(void);
    unsigned (*retro_api_version)(void);
    void (*retro_set_environment)(retro_environment_t);
    void (*retro_set_video_refresh)(retro_video_refresh_t);
    void (*retro_set_audio_sample)(retro_audio_sample_t);
    void (*retro_set_audio_sample_batch)(retro_audio_sample_batch_t);
    void (*retro_set_input_poll)(retro_input_poll_t);
    void (*retro_set_input_state)(retro_input_state_t);
    void (*retro_get_system_info)(struct retro_system_info *);
    void (*retro_get_system_av_info)(struct retro_system_av_info *);
    void (*retro_set_controller_port_device)(unsigned port, unsigned device);
    void (*retro_reset)(void);
    void (*retro_run)(void);
    size_t (*retro_serialize_size)(void);
    bool (*retro_serialize)(void *, size_t);
    bool (*retro_unserialize)(const void *, size_t);
    void (*retro_cheat_reset)(void);
    void (*retro_cheat_set)(unsigned index, bool enabled, const char *code);
    bool (*retro_load_game)(const struct retro_game_info *);
    bool (*retro_load_game_special)(unsigned, const struct retro_game_info*, size_t);
    void (*retro_unload_game)(void);
    unsigned (*retro_get_region)(void);
    void* (*retro_get_memory_data)(unsigned);
    size_t (*retro_get_memory_size)(unsigned);

    // System Information.
    bool shutdown;
    unsigned width, height, fps;
    double sampleRate;
    float aspectRatio;
    char libraryName[200];
    char libraryVersion[200];
    char validExtensions[200];
    bool needFullpath;
    bool blockExtract;
    bool supportNoGame;
    unsigned apiVersion;
    enum retro_pixel_format pixelFormat;
    unsigned performanceLevel;
    bool loaded;
    float volume;

    // The last performance counter registered. TODO: Make it a linked list.
    struct retro_perf_counter* perf_counter_last;
    struct retro_frame_time_callback runloop_frame_time;
    retro_usec_t runloop_frame_time_last;
    struct retro_audio_callback audio_callback;

    // Game data.
    Vector2 inputLastMousePosition;
    Vector2 inputMousePosition;

    // Raylib objects used to play the libretro core.
    Texture texture;

    // Pre-allocated frame conversion buffer (avoids per-frame MemAlloc).
    void *frameBuffer;
    size_t frameBufferSize;

    // Audio
    AudioStream audioStream;
    float *audioRingBuffer;
    size_t audioRingBufferSize;
    size_t audioRingWritePos;
    size_t audioRingAvailable;

    // Callbacks
    retro_keyboard_event_t keyboard_event;
    retro_core_options_update_display_callback_t options_update_display_callback;
    struct retro_vfs_interface vfs_interface;
    // struct retro_game_info_ext game_info_ext;

    // Core variables/options
    char variableKeys[LIBRETRO_MAX_CORE_VARIABLES][LIBRETRO_CORE_VARIABLE_KEY_LEN];
    char variableValues[LIBRETRO_MAX_CORE_VARIABLES][LIBRETRO_CORE_VARIABLE_VALUE_LEN];
    char variableLabels[LIBRETRO_MAX_CORE_VARIABLES][LIBRETRO_CORE_VARIABLE_LABEL_LEN];
    char variableValuesList[LIBRETRO_MAX_CORE_VARIABLES][LIBRETRO_CORE_VARIABLE_VALUES_LEN];
    char variableDisplayList[LIBRETRO_MAX_CORE_VARIABLES][LIBRETRO_CORE_VARIABLE_VALUES_LEN];
    char variableTooltips[LIBRETRO_MAX_CORE_VARIABLES][LIBRETRO_CORE_VARIABLE_TOOLTIP_LEN];
    bool variableVisible[LIBRETRO_MAX_CORE_VARIABLES];
    unsigned variableCount;
    unsigned variableOptionsVersion; // 0=legacy SET_VARIABLES, 1=SET_CORE_OPTIONS, 2=SET_CORE_OPTIONS_V2
    bool variablesDirty; // Whether or not the variables have been changed since last RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE call.
    bool variablesVisibilityDirty; // Whether option visibility changed and the menu should rebuild.

    // Screen rotation: 0=0°, 1=90°, 2=180°, 3=270°
    unsigned rotation;

    // OSD message (RETRO_ENVIRONMENT_SET_MESSAGE / SET_MESSAGE_EXT)
    char osdMessage[256];
    double osdEndTime;

    // Single-sample audio accumulation buffer (avoids static locals).
    int16_t singleSampleBuffer[LIBRETRO_AUDIO_SINGLE_SAMPLE_BUFFER_SIZE * 2];
    size_t singleSampleCount;
    int audioDropWarnCount;

    // Accumulated in-game time in nanoseconds (does not advance while menu is open).
    retro_perf_tick_t gameTimeNSEC;
} rLibretro;

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * The global libretro core object.
 */
static rLibretro LibretroCore = {0};

static void LibretroInitCoreVariable(const char *key, const char *defaultValue,
    const char *label, const char *valuesList, const char *displayList, const char *tooltip) {
    for (unsigned i = 0; i < LibretroCore.variableCount; i++) {
        if (TextIsEqual(LibretroCore.variableKeys[i], key)) {
            return; // Already registered; don't overwrite user-set value.
        }
    }
    if (LibretroCore.variableCount >= LIBRETRO_MAX_CORE_VARIABLES) {
        TraceLog(LOG_WARNING, "LIBRETRO: Core variable limit reached, ignoring: %s", key);
        return;
    }
    unsigned n = LibretroCore.variableCount;
    TextCopy(LibretroCore.variableKeys[n],        key);
    TextCopy(LibretroCore.variableValues[n],      defaultValue  ? defaultValue  : "");
    TextCopy(LibretroCore.variableLabels[n],      label         ? label         : "");
    TextCopy(LibretroCore.variableValuesList[n],  valuesList    ? valuesList    : "");
    TextCopy(LibretroCore.variableDisplayList[n], displayList   ? displayList   : "");
    TextCopy(LibretroCore.variableTooltips[n],    tooltip       ? tooltip       : "");
    LibretroCore.variableVisible[n] = true;
    LibretroCore.variableCount++;
}

static const char *LibretroGetCoreVariable(const char *key) {
    for (unsigned i = 0; i < LibretroCore.variableCount; i++) {
        if (TextIsEqual(LibretroCore.variableKeys[i], key)) {
            return LibretroCore.variableValues[i];
        }
    }
    return NULL;
}

static bool SetLibretroCoreOption(const char *key, const char *value) {
    for (unsigned i = 0; i < LibretroCore.variableCount; i++) {
        if (TextIsEqual(LibretroCore.variableKeys[i], key)) {
            snprintf(LibretroCore.variableValues[i], LIBRETRO_CORE_VARIABLE_VALUE_LEN, "%s", value);
            LibretroCore.variablesDirty = true;
            return true;
        }
    }
    return false;
}

static const char *GetLibretroCoreOption(const char *key) {
    return LibretroGetCoreVariable(key);
}

static bool SaveLibretroCoreOptions(void) {
    if (LibretroCore.variableCount == 0) return false;
    const char *coreName = LibretroCore.libraryName;
    unsigned prefixLen = TextLength(coreName) + 1; // +1 for '.'

    int newEntrySize = (int)LibretroCore.variableCount *
        ((int)prefixLen + LIBRETRO_CORE_VARIABLE_KEY_LEN + LIBRETRO_CORE_VARIABLE_VALUE_LEN + 4);
    int bufSize = newEntrySize + 65536; // 64 KB headroom for existing content
    char *buf = (char*)MemAlloc(bufSize);
    if (!buf) return false;
    int pos = 0;

    // Copy lines from existing file that belong to OTHER cores
    if (FileExists(RAYLIB_LIBRETRO_CFG_FILE)) {
        char *existing = LoadFileText(RAYLIB_LIBRETRO_CFG_FILE);
        if (existing) {
            char *line = existing;
            while (*line) {
                char *eol = line;
                while (*eol && *eol != '\n') eol++;

                // Check if this line starts with "{coreName}."
                bool isOurs = false;
                if ((int)(eol - line) > (int)prefixLen) {
                    char prefix[200] = {0};
                    unsigned pl = prefixLen - 1 < 199 ? prefixLen - 1 : 198;
                    memcpy(prefix, line, pl);
                    isOurs = TextIsEqual(prefix, coreName) && (line[pl] == '.');
                }

                if (!isOurs && pos + (int)(eol - line) + 2 < bufSize) {
                    memcpy(buf + pos, line, (size_t)(eol - line));
                    pos += (int)(eol - line);
                    buf[pos++] = '\n';
                }
                line = *eol ? eol + 1 : eol;
            }
            UnloadFileText(existing);
        }
    }

    // Append this core's current options
    for (unsigned i = 0; i < LibretroCore.variableCount; i++) {
        const char *entry = TextFormat("%s.%s=%s\n", coreName,
            LibretroCore.variableKeys[i], LibretroCore.variableValues[i]);
        int len = (int)TextLength(entry);
        if (pos + len < bufSize) {
            TextCopy(buf + pos, entry);
            pos += len;
        }
    }
    buf[pos] = '\0';

    bool ok = SaveFileText(RAYLIB_LIBRETRO_CFG_FILE, buf);
    MemFree(buf);
    TraceLog(ok ? LOG_INFO : LOG_WARNING, "LIBRETRO: %s core options to %s",
        ok ? "Saved" : "Failed to save", RAYLIB_LIBRETRO_CFG_FILE);
    return ok;
}

static bool LoadLibretroCoreOptions(void) {
    if (!FileExists(RAYLIB_LIBRETRO_CFG_FILE)) return false;
    char *text = LoadFileText(RAYLIB_LIBRETRO_CFG_FILE);
    if (!text) return false;
    const char *coreName = LibretroCore.libraryName;
    unsigned prefixLen = TextLength(coreName) + 1; // +1 for '.'

    int loaded = 0;
    char *line = text;
    while (*line) {
        char *eol = line;
        while (*eol && *eol != '\n') eol++;

        if ((int)(eol - line) > (int)prefixLen) {
            // Check prefix: "{coreName}."
            char prefix[200] = {0};
            unsigned pl = prefixLen - 1 < 199 ? prefixLen - 1 : 198;
            memcpy(prefix, line, pl);
            if (TextIsEqual(prefix, coreName) && line[pl] == '.') {
                const char *rest = line + prefixLen;
                char *eq = (char*)rest;
                while (eq < eol && *eq != '=') eq++;
                if (eq < eol) {
                    char key[LIBRETRO_CORE_VARIABLE_KEY_LEN]   = {0};
                    char val[LIBRETRO_CORE_VARIABLE_VALUE_LEN] = {0};
                    unsigned kl = (unsigned)(eq - rest);
                    unsigned vl = (unsigned)(eol - eq - 1);
                    if (kl >= LIBRETRO_CORE_VARIABLE_KEY_LEN)   kl = LIBRETRO_CORE_VARIABLE_KEY_LEN - 1;
                    if (vl >= LIBRETRO_CORE_VARIABLE_VALUE_LEN) vl = LIBRETRO_CORE_VARIABLE_VALUE_LEN - 1;
                    memcpy(key, rest, kl);
                    memcpy(val, eq + 1, vl);
                    if (SetLibretroCoreOption(key, val)) loaded++;
                }
            }
        }
        line = *eol ? eol + 1 : eol;
    }
    UnloadFileText(text);
    TraceLog(LOG_INFO, "LIBRETRO: Loaded %d core option(s) from %s", loaded, RAYLIB_LIBRETRO_CFG_FILE);
    return loaded > 0;
}

static void LibretroLogger(enum retro_log_level level, const char *fmt, ...) {
    int type = LibretroMapRetroLogLevelToTraceLogType(level);

    va_list va;
    va_start(va, fmt);
    char message[1024];
    vsnprintf(message, sizeof(message), fmt, va);
    va_end(va);

    // Strip trailing newlines/carriage returns.
    int len = (int)strlen(message);
    while (len > 0 && (message[len - 1] == '\n' || message[len - 1] == '\r')) {
        message[--len] = '\0';
    }

    if (len > 0) {
        TraceLog(type, "LIBRETRO: %s", message);
    }
}

static void LibretroInitAudio();  // Forward declaration.

static void LibretroInitVideo() {
    // Unload the existing texture if it exists already.
    UnloadTexture(LibretroCore.texture);

    // Build the rendering image.
    Image image = GenImageColor(LibretroCore.width, LibretroCore.height, BLACK);
    ImageFormat(&image, LibretroMapRetroPixelFormatToPixelFormat(LibretroCore.pixelFormat));

    // Create the texture.
    LibretroCore.texture = LoadTextureFromImage(image);
    SetTextureFilter(LibretroCore.texture, TEXTURE_FILTER_POINT);

    // We don't need the image anymore.
    UnloadImage(image);

    // (Re-)allocate the frame conversion buffer sized for XRGB8888→RGBA8888 (worst case).
    size_t needed = (size_t)GetPixelDataSize(LibretroCore.width, LibretroCore.height, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    if (needed > LibretroCore.frameBufferSize) {
        MemFree(LibretroCore.frameBuffer);
        LibretroCore.frameBuffer = MemAlloc(needed);
        LibretroCore.frameBufferSize = needed;
    }
}

/**
 * Retrieve the current in-game time in microseconds.
 *
 * @return retro_time_t The current in-game time in microseconds.
 */
static retro_time_t LibretroGetTimeUSEC() {
    return (retro_time_t)(LibretroCore.gameTimeNSEC / 1000);
}

/**
 * Get the CPU Features.
 *
 * @see retro_get_cpu_features_t
 * @return uint64_t Returns a bit-mask of detected CPU features (RETRO_SIMD_*).
 */
static uint64_t LibretroGetCPUFeatures() {
    return cpu_features_get();
}

/**
 * A simple counter. Usually nanoseconds, but can also be CPU cycles.
 *
 * @see retro_perf_get_counter_t
 * @return retro_perf_tick_t The current value of the high resolution counter.
 */
static retro_perf_tick_t LibretroGetPerfCounter() {
    return LibretroCore.gameTimeNSEC;
}

/**
 * Register a performance counter.
 *
 * @see retro_perf_register_t
 */
static void LibretroPerfRegister(struct retro_perf_counter* counter) {
    LibretroCore.perf_counter_last = counter;
    counter->registered = true;
}

/**
 * Starts a registered counter.
 *
 * @see retro_perf_start_t
 */
static void LibretroPerfStart(struct retro_perf_counter* counter) {
    if (counter->registered) {
        counter->start = LibretroGetPerfCounter();
    }
}

/**
 * Stops a registered counter.
 *
 * @see retro_perf_stop_t
 */
static void LibretroPerfStop(struct retro_perf_counter* counter) {
    counter->total = LibretroGetPerfCounter() - counter->start;
}

/**
 * Log and display the state of performance counters.
 *
 * @see retro_perf_log_t
 */
static void LibretroPerfLog() {
    // TODO: Use a linked list of counters, and loop through them all.
    TraceLog(LOG_INFO, "LIBRETRO: Timer %s: %i - %i", LibretroCore.perf_counter_last->ident, LibretroCore.perf_counter_last->start, LibretroCore.perf_counter_last->total);
}

static bool LibretroSetEnvironment(unsigned cmd, void * data) {
    switch (cmd) {
        case RETRO_ENVIRONMENT_SET_ROTATION: {
            if (data == NULL) {
                return false;
            }
            LibretroCore.rotation = *(const unsigned *)data;
            TraceLog(LOG_INFO, "LIBRETRO: RETRO_ENVIRONMENT_SET_ROTATION: %u (%u°)", LibretroCore.rotation, LibretroCore.rotation * 90);
            return true;
        }

        case RETRO_ENVIRONMENT_GET_OVERSCAN: {
            TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_GET_OVERSCAN not implemented");
            return false;
        }

        case RETRO_ENVIRONMENT_GET_CAN_DUPE: {
            bool* var = (bool*)data;
            if (var == NULL) {
                TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_GET_CAN_DUPE no data provided");
                return false;
            }
            *var = true;
            return true;
        }

        case RETRO_ENVIRONMENT_SET_MESSAGE: {
            const struct retro_message* message = (const struct retro_message *)data;
            if (message == NULL) {
                TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_SET_MESSAGE no data provided");
                return false;
            }
            if (message->msg == NULL) {
                TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_SET_MESSAGE: No message");
                return false;
            }
            TraceLog(LOG_INFO, "LIBRETRO: RETRO_ENVIRONMENT_SET_MESSAGE: %s (%i frames)", message->msg, message->frames);
            TextCopy(LibretroCore.osdMessage, message->msg);
            LibretroCore.osdEndTime = GetTime() + message->frames / (LibretroCore.fps > 0 ? (double)LibretroCore.fps : 60.0);
            return true;
        }

        case RETRO_ENVIRONMENT_SHUTDOWN: {
            TraceLog(LOG_INFO, "LIBRETRO: RETRO_ENVIRONMENT_SHUTDOWN");
            LibretroCore.shutdown = true;
            return true;
        }

        case RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL: {
            if (data == NULL) {
                TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL no data provided");
                return false;
            }
            LibretroCore.performanceLevel = *(const unsigned *)data;
            TraceLog(LOG_INFO, "LIBRETRO: RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL(%i)", LibretroCore.performanceLevel);
            return true;
        }

        case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY: {
            const char** dir = (const char**)data;
            if (dir == NULL) {
                TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY no data set");
                return false;
            }
            *dir = GetWorkingDirectory();
            return true;
        }

        case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: {
            const enum retro_pixel_format* pixelFormat = (const enum retro_pixel_format *)data;
            if (pixelFormat == NULL) {
                TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_SET_PIXEL_FORMAT no data set");
                return false;
            }
            LibretroCore.pixelFormat = *pixelFormat;

            switch (LibretroCore.pixelFormat) {
            case RETRO_PIXEL_FORMAT_0RGB1555:
                TraceLog(LOG_INFO, "LIBRETRO: RETRO_ENVIRONMENT_SET_PIXEL_FORMAT 0RGB1555");
                break;
            case RETRO_PIXEL_FORMAT_XRGB8888:
                TraceLog(LOG_INFO, "LIBRETRO: RETRO_ENVIRONMENT_SET_PIXEL_FORMAT XRGB8888");
                break;
            case RETRO_PIXEL_FORMAT_RGB565:
                TraceLog(LOG_INFO, "LIBRETRO: RETRO_ENVIRONMENT_SET_PIXEL_FORMAT RGB565");
                break;
            default:
                TraceLog(LOG_ERROR, "LIBRETRO: RETRO_ENVIRONMENT_SET_PIXEL_FORMAT Unknown. Falling to RGB565");
                LibretroCore.pixelFormat = RETRO_PIXEL_FORMAT_RGB565;
                return false;
            }
            return true;
        }

        case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS: {
            // TODO: Implement RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS
            const struct retro_input_descriptor *desc = (const struct retro_input_descriptor *)data;
            if (desc == NULL) {
                TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS no data set");
                return false;
            }
            for (; desc->description != NULL; desc++) {
                TraceLog(LOG_INFO, "LIBRETRO: Input port %u device %u index %u id %u: %s",
                    desc->port, desc->device, desc->index, desc->id, desc->description);
            }
            return false;
        }

        case RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK: {
            if (data == NULL) {
                TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK no data set");
                return false;
            }
            const struct retro_keyboard_callback * callback = (const struct retro_keyboard_callback *)data;
            LibretroCore.keyboard_event = callback->callback;
            return true;
        }

        case RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE: {
            TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE not implemented");
            return false;
        }

        case RETRO_ENVIRONMENT_SET_HW_RENDER: {
            TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_SET_HW_RENDER not implemented");
            return false;
        }

        case RETRO_ENVIRONMENT_GET_VARIABLE: {
            if (data == NULL) {
                TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_GET_VARIABLE no data set");
                return false;
            }
            struct retro_variable *var = (struct retro_variable *)data;
            if (var->key == NULL) {
                return false;
            }
            const char *value = LibretroGetCoreVariable(var->key);
            if (value != NULL) {
                var->value = value;
                return true;
            }
            TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_GET_VARIABLE unknown key: %s", var->key);
            return false;
        }

        case RETRO_ENVIRONMENT_SET_VARIABLES: {
            if (data == NULL) {
                TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_SET_VARIABLES no data set");
                return false;
            }
            LibretroCore.variableOptionsVersion = 0;
            const struct retro_variable *var = (const struct retro_variable *)data;
            for (; var->key != NULL; var++) {
                // Description format: "Desc text; option1|option2|...". Default is the first option.
                char defaultVal[LIBRETRO_CORE_VARIABLE_VALUE_LEN] = {0};
                char label[LIBRETRO_CORE_VARIABLE_LABEL_LEN] = {0};
                char valuesList[LIBRETRO_CORE_VARIABLE_VALUES_LEN] = {0};
                if (var->value != NULL) {
                    const char *semi = strchr(var->value, ';');
                    if (semi != NULL) {
                        // Label: text before ';', right-trimmed
                        size_t labelLen = (size_t)(semi - var->value);
                        while (labelLen > 0 && var->value[labelLen - 1] == ' ') labelLen--;
                        if (labelLen >= LIBRETRO_CORE_VARIABLE_LABEL_LEN) labelLen = LIBRETRO_CORE_VARIABLE_LABEL_LEN - 1;
                        memcpy(label, var->value, labelLen);

                        // Values list: text after ';', left-trimmed (already pipe-separated)
                        const char *opts = semi + 1;
                        while (*opts == ' ') opts++;
                        TextCopy(valuesList, opts);

                        // Default: first pipe token
                        const char *pipe = strchr(opts, '|');
                        size_t len = pipe ? (size_t)(pipe - opts) : strlen(opts);
                        if (len >= LIBRETRO_CORE_VARIABLE_VALUE_LEN) len = LIBRETRO_CORE_VARIABLE_VALUE_LEN - 1;
                        memcpy(defaultVal, opts, len);
                    }
                }
                LibretroInitCoreVariable(var->key, defaultVal, label, valuesList, valuesList, "");
            }
            return true;
        }

        case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE: {
            if (data == NULL) {
                TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE no data set");
                return false;
            }
            bool *updated = (bool *)data;
            *updated = LibretroCore.variablesDirty;
            LibretroCore.variablesDirty = false;
            return true;
        }

        case RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME: {
            // When set to true, the core will run without content.
            if (data == NULL) {
                TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME no data provided");
                return false;
            }
            LibretroCore.supportNoGame = *(bool*)data;
            TraceLog(LOG_INFO, "LIBRETRO: RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME: %s", LibretroCore.supportNoGame ? "true" : "false");
            return true;
        }

        case RETRO_ENVIRONMENT_GET_LIBRETRO_PATH: {
            if (data == NULL) {
                TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_GET_LIBRETRO_PATH no data provided");
                return false;
            }
            const char** dir = (const char**)data;
            *dir = GetWorkingDirectory();
            return true;
        }

        case RETRO_ENVIRONMENT_SET_FRAME_TIME_CALLBACK: {
            if (data == NULL) {
                TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_SET_FRAME_TIME_CALLBACK no data provided");
                return false;
            }
            const struct retro_frame_time_callback *frame_time = (const struct retro_frame_time_callback*)data;
            LibretroCore.runloop_frame_time = *frame_time;
            return true;
        }

        case RETRO_ENVIRONMENT_SET_AUDIO_CALLBACK: {
            if (data == NULL) {
                TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_SET_AUDIO_CALLBACK no data provided");
                return false;
            }
            struct retro_audio_callback *audio_cb = (struct retro_audio_callback*)data;
            LibretroCore.audio_callback = *audio_cb;
            return true;
        }

        case RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE: {
            TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE not implemented");
            return false;
        }

        case RETRO_ENVIRONMENT_GET_INPUT_DEVICE_CAPABILITIES: {
            if (data == NULL) {
                TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_GET_INPUT_DEVICE_CAPABILITIES no data provided");
                return false;
            }
            uint64_t *capabilities = (uint64_t*)data;
            *capabilities =
                (1 << RETRO_DEVICE_JOYPAD) |
                (1 << RETRO_DEVICE_MOUSE) |
                (1 << RETRO_DEVICE_KEYBOARD) |
                (1 << RETRO_DEVICE_ANALOG) |
                (1 << RETRO_DEVICE_POINTER);
            return true;
        }

        case RETRO_ENVIRONMENT_GET_SENSOR_INTERFACE: {
            TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_GET_SENSOR_INTERFACE not implemented");
            return false;
        }

        case RETRO_ENVIRONMENT_GET_CAMERA_INTERFACE: {
            TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_GET_CAMERA_INTERFACE not implemented");
            return false;
        }

        case RETRO_ENVIRONMENT_GET_LOG_INTERFACE: {
            if (data == NULL) {
                TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_GET_LOG_INTERFACE no data provided");
                return false;
            }
            struct retro_log_callback *callback = (struct retro_log_callback*)data;
            if (callback != NULL) {
                callback->log = LibretroLogger;
            }
            return true;
        }

        case RETRO_ENVIRONMENT_GET_PERF_INTERFACE: {
            if (data == NULL) {
                TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_GET_PERF_INTERFACE no data provided");
                return false;
            }
            struct retro_perf_callback *perf = (struct retro_perf_callback *)data;
            perf->get_time_usec = LibretroGetTimeUSEC;
            perf->get_cpu_features = LibretroGetCPUFeatures;
            perf->get_perf_counter = LibretroGetPerfCounter;
            perf->perf_register = LibretroPerfRegister;
            perf->perf_start = LibretroPerfStart;
            perf->perf_stop = LibretroPerfStop;
            perf->perf_log = LibretroPerfLog;
            return true;
        }

        case RETRO_ENVIRONMENT_GET_LOCATION_INTERFACE: {
            TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_GET_LOCATION_INTERFACE not implemented");
            return false;
        }

        case RETRO_ENVIRONMENT_GET_CORE_ASSETS_DIRECTORY: {
            if (data == NULL) {
                return false;
            }
            const char** dir = (const char**)data;
            *dir = GetWorkingDirectory();
            return true;
        }

        case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY: {
            if (data == NULL) {
                return false;
            }
            const char** dir = (const char**)data;
            *dir = GetWorkingDirectory();
            return true;
        }

        case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO: {
            if (data == NULL) {
                TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO no data provided");
                return false;
            }
            const struct retro_system_av_info av = *(const struct retro_system_av_info *)data;
            bool sampleRateChanged = (av.timing.sample_rate != LibretroCore.sampleRate);
            bool dimsChanged = (av.geometry.base_width != LibretroCore.width || av.geometry.base_height != LibretroCore.height);
            LibretroCore.width = av.geometry.base_width;
            LibretroCore.height = av.geometry.base_height;
            LibretroCore.fps = (unsigned int)av.timing.fps;
            LibretroCore.sampleRate = av.timing.sample_rate;
            LibretroCore.aspectRatio = av.geometry.aspect_ratio;
            TraceLog(LOG_INFO, "LIBRETRO: RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO %i x %i @ %i FPS",
                LibretroCore.width,
                LibretroCore.height,
                LibretroCore.fps
            );
            if (dimsChanged) {
                LibretroInitVideo();
            }
            if (sampleRateChanged) {
                StopAudioStream(LibretroCore.audioStream);
                UnloadAudioStream(LibretroCore.audioStream);
                MemFree(LibretroCore.audioRingBuffer);
                LibretroCore.audioRingBuffer = NULL;
                LibretroCore.audioRingAvailable = 0;
                LibretroCore.audioRingWritePos = 0;
                LibretroInitAudio();
            }
            SetTargetFPS(LibretroCore.fps);
            return true;
        }

        case RETRO_ENVIRONMENT_SET_PROC_ADDRESS_CALLBACK: {
            TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_SET_PROC_ADDRESS_CALLBACK not implemented");
            return false;
        }

        case RETRO_ENVIRONMENT_SET_SUBSYSTEM_INFO: {
            TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_SET_SUBSYSTEM_INFO not implemented");
            return false;
        }

        case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO: {
            if (data == NULL) {
                TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_SET_CONTROLLER_INFO no data provided");
                return false;
            }
            const struct retro_controller_info *info = (const struct retro_controller_info *)data;
            for (unsigned port = 0; info[port].types != NULL; port++) {
                TraceLog(LOG_INFO, "LIBRETRO: RETRO_ENVIRONMENT_SET_CONTROLLER_INFO port %u (%u types)", port, info[port].num_types);
                for (unsigned i = 0; i < info[port].num_types; i++) {
                    TraceLog(LOG_INFO, "    > [%u] %s (id: %u)", i, info[port].types[i].desc, info[port].types[i].id);
                }
            }
            // TODO: Implement RETRO_ENVIRONMENT_SET_CONTROLLER_INFO
            return false;
        }

        case RETRO_ENVIRONMENT_SET_MEMORY_MAPS: {
            TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_SET_MEMORY_MAPS not implemented");
            return false;
        }

        case RETRO_ENVIRONMENT_SET_GEOMETRY: {
            if (!data) {
                TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_SET_GEOMETRY no data provided");
                return false;
            }
            const struct retro_game_geometry *geom = (const struct retro_game_geometry *)data;
            LibretroCore.width = geom->base_width;
            LibretroCore.height = geom->base_height;
            LibretroCore.aspectRatio = geom->aspect_ratio;
            TraceLog(LOG_INFO, "LIBRETRO: RETRO_ENVIRONMENT_SET_GEOMETRY %i x %i @ %f",
                LibretroCore.width,
                LibretroCore.height,
                LibretroCore.aspectRatio
            );
            return true;
        }

        case RETRO_ENVIRONMENT_GET_USERNAME: {
            if (!data) {
                return false;
            }
            const char** name = (const char**)data;
            *name = "raylib";
            return true;
        }

        case RETRO_ENVIRONMENT_GET_LANGUAGE: {
            if (!data) {
                return false;
            }
            unsigned * language = (unsigned *)data;
            *language = RETRO_LANGUAGE_ENGLISH;
            return true;
        }

        case RETRO_ENVIRONMENT_GET_CURRENT_SOFTWARE_FRAMEBUFFER: {
            TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_GET_CURRENT_SOFTWARE_FRAMEBUFFER not implemented");
            return false;
        }

        case RETRO_ENVIRONMENT_GET_HW_RENDER_INTERFACE: {
            TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_GET_HW_RENDER_INTERFACE not implemented");
            return false;
        }

        case RETRO_ENVIRONMENT_SET_SUPPORT_ACHIEVEMENTS: {
            TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_SET_SUPPORT_ACHIEVEMENTS not implemented");
            return false;
        }

        case RETRO_ENVIRONMENT_SET_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE: {
            TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_SET_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE not implemented");
            return false;
        }

        case RETRO_ENVIRONMENT_SET_SERIALIZATION_QUIRKS: {
            TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_SET_SERIALIZATION_QUIRKS not implemented");
            return false;
        }

        case RETRO_ENVIRONMENT_SET_HW_SHARED_CONTEXT: {
            TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_SET_HW_SHARED_CONTEXT not implemented");
            return false;
        }

        case RETRO_ENVIRONMENT_GET_VFS_INTERFACE: {
            struct retro_vfs_interface_info * vfs_interface = (struct retro_vfs_interface_info *)data;
            if (vfs_interface == NULL) {
                TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_GET_VFS_INTERFACE data missing");
                return false;
            }
            if (vfs_interface->required_interface_version > 3) {
                TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_GET_VFS_INTERFACE version not supported");
                return false;
            }

            if (vfs_interface->iface == NULL) {
                vfs_interface->iface = &LibretroCore.vfs_interface;
            }

            vfs_interface->iface->get_path = &raylib_libretro_vfs_get_path;
            vfs_interface->iface->open = &raylib_libretro_vfs_open;
            vfs_interface->iface->close = &raylib_libretro_vfs_close;
            vfs_interface->iface->size = &raylib_libretro_vfs_size;
            vfs_interface->iface->tell = &raylib_libretro_vfs_tell;
            vfs_interface->iface->seek = &raylib_libretro_vfs_seek;
            vfs_interface->iface->read = &raylib_libretro_vfs_read;
            vfs_interface->iface->write = &raylib_libretro_vfs_write;
            vfs_interface->iface->flush = &raylib_libretro_vfs_flush;
            vfs_interface->iface->remove = &raylib_libretro_vfs_remove;
            vfs_interface->iface->rename = &raylib_libretro_vfs_rename;
            vfs_interface->iface->truncate = &raylib_libretro_vfs_truncate;
            vfs_interface->iface->stat = &raylib_libretro_vfs_stat;
            vfs_interface->iface->mkdir = &raylib_libretro_vfs_mkdir;
            vfs_interface->iface->opendir = &raylib_libretro_vfs_opendir;
            vfs_interface->iface->readdir = &raylib_libretro_vfs_readdir;
            vfs_interface->iface->dirent_get_name = &raylib_libretro_vfs_dirent_get_name;
            vfs_interface->iface->dirent_is_dir = &raylib_libretro_vfs_dirent_is_dir;
            vfs_interface->iface->closedir = &raylib_libretro_vfs_closedir;

            return true;
        }

        case RETRO_ENVIRONMENT_GET_LED_INTERFACE: {
            TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_GET_LED_INTERFACE not implemented");
            return false;
        }

        case RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE: {
            enum retro_av_enable_flags* output = (enum retro_av_enable_flags *)data;
            if (output == NULL) {
                TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE data missing");
                return false;
            }
            *output = RETRO_AV_ENABLE_VIDEO | RETRO_AV_ENABLE_AUDIO;
            return true;
        }

        case RETRO_ENVIRONMENT_GET_MIDI_INTERFACE: {
            TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_GET_MIDI_INTERFACE not implemented");
            return false;
        }

        case RETRO_ENVIRONMENT_GET_FASTFORWARDING: {
            bool* output = (bool *)data;
            if (output == NULL) {
                TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_GET_FASTFORWARDING data missing");
                return false;
            }
            // TODO: Implement Fast Forwarding.
            *output = false;
            return true;
        }

        case RETRO_ENVIRONMENT_GET_TARGET_REFRESH_RATE: {
            float *refreshRate = (float *)data;
            if (refreshRate == NULL) {
                TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_GET_TARGET_REFRESH_RATE data missing");
                return false;
            }
            *refreshRate = (float)GetMonitorRefreshRate(GetCurrentMonitor());
            TraceLog(LOG_INFO, "LIBRETRO: Monitor Refresh Rate: %i", (int)*refreshRate);
            return true;
        }

        case RETRO_ENVIRONMENT_GET_INPUT_BITMASKS: {
            return true;
        }

        case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION: {
            unsigned *version = (unsigned *)data;
            if (version == NULL) {
                TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION data missing");
                return false;
            }
            *version = 2;
            return true;
        }

        case RETRO_ENVIRONMENT_SET_CORE_OPTIONS: {
            const struct retro_core_option_definition *opts = (const struct retro_core_option_definition *)data;
            if (opts == NULL) {
                TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_SET_CORE_OPTIONS data missing");
                return false;
            }
            LibretroCore.variableOptionsVersion = 1;
            for (; opts->key != NULL; opts++) {
                const char *def = opts->default_value;
                if (def == NULL && opts->values[0].value != NULL) {
                    def = opts->values[0].value;
                }

                // Build pipe-separated values and display lists
                char valuesList[LIBRETRO_CORE_VARIABLE_VALUES_LEN] = {0};
                char displayList[LIBRETRO_CORE_VARIABLE_VALUES_LEN] = {0};
                int vpos = 0, dpos = 0;
                for (int j = 0; opts->values[j].value != NULL; j++) {
                    const char *val = opts->values[j].value;
                    const char *lbl = opts->values[j].label ? opts->values[j].label : val;
                    const char *sep = j > 0 ? "|" : "";
                    int slen = j > 0 ? 1 : 0;
                    int vlen = (int)TextLength(val);
                    int dlen = (int)TextLength(lbl);
                    if (vpos + slen + vlen < LIBRETRO_CORE_VARIABLE_VALUES_LEN - 1) {
                        if (slen) { valuesList[vpos++] = '|'; }
                        TextCopy(valuesList + vpos, val); vpos += vlen;
                    }
                    if (dpos + slen + dlen < LIBRETRO_CORE_VARIABLE_VALUES_LEN - 1) {
                        if (slen) { displayList[dpos++] = '|'; }
                        TextCopy(displayList + dpos, lbl); dpos += dlen;
                    }
                }

                LibretroInitCoreVariable(opts->key, def ? def : "",
                    opts->desc, valuesList, displayList,
                    opts->info ? opts->info : "");
            }
            return true;
        }

        case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL: {
            const struct retro_core_options_intl *intl = (const struct retro_core_options_intl *)data;
            if (intl == NULL) {
                TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL data missing");
                return false;
            }
            if (intl->us == NULL) {
                return false;
            }
            return LibretroSetEnvironment(RETRO_ENVIRONMENT_SET_CORE_OPTIONS, (void *)intl->us);
        }

        case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY: {
            const struct retro_core_option_display *disp = (const struct retro_core_option_display *)data;
            if (!disp || !disp->key) return false;
            for (unsigned i = 0; i < LibretroCore.variableCount; i++) {
                if (TextIsEqual(LibretroCore.variableKeys[i], disp->key)) {
                    LibretroCore.variableVisible[i] = disp->visible;
                    LibretroCore.variablesVisibilityDirty = true;
                    return true;
                }
            }
            return false;
        }

        case RETRO_ENVIRONMENT_GET_PREFERRED_HW_RENDER: {
            TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_GET_PREFERRED_HW_RENDER not implemented");
            return false;
        }

        case RETRO_ENVIRONMENT_GET_DISK_CONTROL_INTERFACE_VERSION: {
            TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_GET_DISK_CONTROL_INTERFACE_VERSION not implemented");
            return false;
        }

        case RETRO_ENVIRONMENT_SET_DISK_CONTROL_EXT_INTERFACE: {
            TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_SET_DISK_CONTROL_EXT_INTERFACE not implemented");
            return false;
        }

        case RETRO_ENVIRONMENT_GET_MESSAGE_INTERFACE_VERSION: {
            unsigned * interfaceVersion = (unsigned *)data;
            if (interfaceVersion == NULL) {
                TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_GET_MESSAGE_INTERFACE_VERSION data missing");
                return false;
            }
            *interfaceVersion = (unsigned)1;
            return true;
        }

        case RETRO_ENVIRONMENT_SET_MESSAGE_EXT: {
            const struct retro_message_ext* message = (const struct retro_message_ext *)data;
            if (message == NULL || message->msg == NULL) {
                TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_SET_MESSAGE_EXT no data provided");
                return false;
            }
            if (message->target == RETRO_MESSAGE_TARGET_LOG || message->target == RETRO_MESSAGE_TARGET_ALL) {
                TraceLog(LibretroMapRetroLogLevelToTraceLogType(message->level), "LIBRETRO: %s", message->msg);
            }
            if (message->target == RETRO_MESSAGE_TARGET_OSD || message->target == RETRO_MESSAGE_TARGET_ALL) {
                TextCopy(LibretroCore.osdMessage, message->msg);
                LibretroCore.osdEndTime = GetTime() + message->duration / 1000.0;
            }
            return true;
        }

        case RETRO_ENVIRONMENT_GET_INPUT_MAX_USERS: {
            unsigned * maxUsers = (unsigned *)data;
            if (maxUsers == NULL) {
                TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_GET_INPUT_MAX_USERS data missing");
                return false;
            }
            int count = 0;
            while (IsGamepadAvailable(count)) {
                count++;
            }
            *maxUsers = count + 1; // +1 for keyboard
            return true;
        }

        case RETRO_ENVIRONMENT_SET_AUDIO_BUFFER_STATUS_CALLBACK: {
            TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_SET_AUDIO_BUFFER_STATUS_CALLBACK not implemented");
            return false;
        }

        case RETRO_ENVIRONMENT_SET_MINIMUM_AUDIO_LATENCY: {
            TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_SET_MINIMUM_AUDIO_LATENCY not implemented");
            return false;
        }

        case RETRO_ENVIRONMENT_SET_FASTFORWARDING_OVERRIDE: {
            TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_SET_FASTFORWARDING_OVERRIDE not implemented");
            return false;
        }

        case RETRO_ENVIRONMENT_SET_CONTENT_INFO_OVERRIDE: {
            TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_SET_CONTENT_INFO_OVERRIDE not implemented");
            return false;
        }

        case RETRO_ENVIRONMENT_GET_GAME_INFO_EXT: {
            TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_GET_GAME_INFO_EXT not implemented");
            // const struct retro_game_info_ext ** game_info_ext = (const struct retro_game_info_ext **)data;
            // *game_info_ext = &LibretroCore.game_info_ext;
            // LibretroCore.game_info_ext.archive_file

            return false;
        }

        case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2: {
            if (data == NULL) {
                return false;
            }
            const struct retro_core_options_v2 *opts = (const struct retro_core_options_v2 *)data;
            if (opts->definitions == NULL) {
                return false;
            }
            LibretroCore.variableOptionsVersion = 2;
            const struct retro_core_option_v2_definition *def = opts->definitions;
            for (; def->key != NULL; def++) {
                const char *defaultVal = def->default_value;
                if (defaultVal == NULL && def->values[0].value != NULL) {
                    defaultVal = def->values[0].value;
                }

                // Prefer categorized variants when available
                const char *label   = (def->desc_categorized && def->desc_categorized[0])
                                      ? def->desc_categorized : def->desc;
                const char *tooltip = (def->info_categorized && def->info_categorized[0])
                                      ? def->info_categorized : (def->info ? def->info : "");

                // Build pipe-separated values and display lists
                char valuesList[LIBRETRO_CORE_VARIABLE_VALUES_LEN] = {0};
                char displayList[LIBRETRO_CORE_VARIABLE_VALUES_LEN] = {0};
                int vpos = 0, dpos = 0;
                for (int j = 0; def->values[j].value != NULL; j++) {
                    const char *val = def->values[j].value;
                    const char *lbl = def->values[j].label ? def->values[j].label : val;
                    int slen = j > 0 ? 1 : 0;
                    int vlen = (int)TextLength(val);
                    int dlen = (int)TextLength(lbl);
                    if (vpos + slen + vlen < LIBRETRO_CORE_VARIABLE_VALUES_LEN - 1) {
                        if (slen) { valuesList[vpos++] = '|'; }
                        TextCopy(valuesList + vpos, val); vpos += vlen;
                    }
                    if (dpos + slen + dlen < LIBRETRO_CORE_VARIABLE_VALUES_LEN - 1) {
                        if (slen) { displayList[dpos++] = '|'; }
                        TextCopy(displayList + dpos, lbl); dpos += dlen;
                    }
                }

                LibretroInitCoreVariable(def->key, defaultVal ? defaultVal : "",
                    label, valuesList, displayList, tooltip);
            }
            return true;
        }

        case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2_INTL: {
            if (data == NULL) {
                return false;
            }
            const struct retro_core_options_v2_intl *intl = (const struct retro_core_options_v2_intl *)data;
            if (intl->us == NULL) {
                return false;
            }
            return LibretroSetEnvironment(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2, (void *)intl->us);
        }

        case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_UPDATE_DISPLAY_CALLBACK: {
            const struct retro_core_options_update_display_callback *cb =
                (const struct retro_core_options_update_display_callback *)data;
            LibretroCore.options_update_display_callback = (cb && cb->callback) ? cb->callback : NULL;
            return true;
        }

        case RETRO_ENVIRONMENT_SET_VARIABLE: {
            const struct retro_variable *var = (const struct retro_variable *)data;
            if (var == NULL || var->key == NULL || var->value == NULL) {
                return false;
            }
            return SetLibretroCoreOption(var->key, var->value);
        }

        case RETRO_ENVIRONMENT_GET_THROTTLE_STATE: {
            TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_GET_THROTTLE_STATE not implemented");
            return false;
        }

        case RETRO_ENVIRONMENT_GET_SAVESTATE_CONTEXT: {
            TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_GET_SAVESTATE_CONTEXT not implemented");
            return false;
        }

        case RETRO_ENVIRONMENT_GET_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_SUPPORT: {
            TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_GET_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_SUPPORT not implemented");
            return false;
        }

        case RETRO_ENVIRONMENT_GET_JIT_CAPABLE: {
            TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_GET_JIT_CAPABLE not implemented");
            return false;
        }

        case RETRO_ENVIRONMENT_GET_MICROPHONE_INTERFACE: {
            TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_GET_MICROPHONE_INTERFACE not implemented");
            return false;
        }

        case RETRO_ENVIRONMENT_GET_DEVICE_POWER: {
            TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_GET_DEVICE_POWER not implemented");
            return false;
        }

        case RETRO_ENVIRONMENT_SET_NETPACKET_INTERFACE: {
            TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_SET_NETPACKET_INTERFACE not implemented");
            return false;
        }

        case RETRO_ENVIRONMENT_GET_PLAYLIST_DIRECTORY: {
            if (data == NULL) {
                TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_GET_PLAYLIST_DIRECTORY no data provided");
                return false;
            }
            const char** dir = (const char**)data;
            *dir = GetWorkingDirectory();
            return true;
        }

        case RETRO_ENVIRONMENT_GET_FILE_BROWSER_START_DIRECTORY: {
            if (data == NULL) {
                TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_GET_FILE_BROWSER_START_DIRECTORY no data provided");
                return false;
            }
            const char** dir = (const char**)data;
            *dir = GetWorkingDirectory();
            return true;
        }

        case RETRO_ENVIRONMENT_GET_TARGET_SAMPLE_RATE: {
            if (data == NULL) {
                TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_GET_TARGET_SAMPLE_RATE no data provided");
                return false;
            }
            unsigned *sampleRate = (unsigned *)data;
            *sampleRate = (unsigned)LibretroCore.sampleRate;
            TraceLog(LOG_INFO, "LIBRETRO: RETRO_ENVIRONMENT_GET_TARGET_SAMPLE_RATE: %u", *sampleRate);
            return true;
        }

        case RETRO_ENVIRONMENT_GET_NETPLAY_CLIENT_INDEX: {
            TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_GET_NETPLAY_CLIENT_INDEX not implemented");
            return false;
        }
    }

    TraceLog(LOG_WARNING, "LIBRETRO: Undefined environment call: %i", cmd);
    return false;
}

/**
 * Retrieve the audio/video information for the core.
 */
static bool LibretroGetAudioVideo() {
    struct retro_system_av_info av = {0};
    LibretroCore.retro_get_system_av_info(&av);
    LibretroCore.width = av.geometry.base_width;
    LibretroCore.height = av.geometry.base_height;
    LibretroCore.fps = (unsigned int)av.timing.fps;
    LibretroCore.sampleRate = av.timing.sample_rate;
    LibretroCore.aspectRatio = av.geometry.aspect_ratio;
    TraceLog(LOG_INFO, "LIBRETRO: Video and Audio information");
    TraceLog(LOG_INFO, "    > Display size: %i x %i", LibretroCore.width, LibretroCore.height);
    TraceLog(LOG_INFO, "    > Target FPS:   %i", LibretroCore.fps);
    TraceLog(LOG_INFO, "    > Sample rate:  %.2f Hz", LibretroCore.sampleRate);

    return true;
}

/**
 * Runs an iteration of the libretro core.
 */
static void UpdateLibretro() {
    LibretroCore.gameTimeNSEC += (retro_perf_tick_t)(GetFrameTime() * 1000000000.0);

    // Update the game loop timer.
    if (LibretroCore.runloop_frame_time.callback) {
        retro_time_t current = LibretroGetTimeUSEC();
        retro_time_t delta = current - LibretroCore.runloop_frame_time_last;

        if (!LibretroCore.runloop_frame_time_last) {
            delta = LibretroCore.runloop_frame_time.reference;
        }
        LibretroCore.runloop_frame_time_last = current;
        LibretroCore.runloop_frame_time.callback(delta);
    }

    // Ask the core to emit the audio.
    if (LibretroCore.audio_callback.callback) {
        LibretroCore.audio_callback.callback();
    }

    // Check keyboard event callback.
    if (LibretroCore.keyboard_event != NULL) {
        // Prepare the key modifiers.
        uint16_t key_modifiers = 0;
        if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) {
            key_modifiers |= RETROKMOD_SHIFT;
        }
        if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) {
            key_modifiers |= RETROKMOD_CTRL;
        }
        if (IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT)) {
            key_modifiers |= RETROKMOD_ALT;
        }
        if (IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER)) {
            key_modifiers |= RETROKMOD_META;
        }
        if (IsKeyDown(KEY_NUM_LOCK)) {
            key_modifiers |= RETROKMOD_NUMLOCK;
        }
        if (IsKeyDown(KEY_CAPS_LOCK)) {
            key_modifiers |= RETROKMOD_CAPSLOCK;
        }
        if (IsKeyDown(KEY_SCROLL_LOCK)) {
            key_modifiers |= RETROKMOD_SCROLLOCK;
        }

        // Build a per-key char map by draining both queues in tandem.
        int keyCharMap[512] = {0};
        int pressedKey;
        while ((pressedKey = GetKeyPressed()) != 0) {
            int pressedChar = GetCharPressed();
            if (pressedKey < 512) {
                keyCharMap[pressedKey] = pressedChar;
            }
        }

        // Check each keyboard key
        for (int key = RETROK_FIRST; key < RETROK_LAST; key++) {
            int raylibKey = LibretroMapRetroKeyToKeyboardKey(key);
            if (raylibKey > 0) {
                if (IsKeyPressed(raylibKey)) {
                    LibretroCore.keyboard_event(true, key, (uint32_t)keyCharMap[raylibKey], key_modifiers);
                }
                else if (IsKeyReleased(raylibKey)) {
                    LibretroCore.keyboard_event(false, key, 0, key_modifiers);
                }
            }
        }
    }

    if (IsLibretroGameReady()) {
        LibretroCore.retro_run();

        if (LibretroCore.options_update_display_callback) {
            if (LibretroCore.options_update_display_callback()) {
                LibretroCore.variablesVisibilityDirty = true;
            }
        }
    }

}

/**
 * Retrieve whether or not the core has been loaded.
 */
static bool IsLibretroReady() {
    return LibretroCore.handle != NULL;
}

/**
 * Returns whether or not the libretro core should close.
 */
static bool LibretroShouldClose() {
    return LibretroCore.shutdown;
}

static void LibretroMapPixelFormatARGB8888ToRGBA8888(void *output_, const void *input_,
    int width, int height,
    int out_stride, int in_stride) {
    int h, w;
    const uint32_t *input = (const uint32_t*)input_;
    uint32_t *output      = (uint32_t*)output_;

    for (h = 0; h < height;
          h++, output += out_stride >> 2, input += in_stride >> 2)
    {
       for (w = 0; w < width; w++)
       {
            uint8_t r   = (input[w] >> 16) & 0xff;
            uint8_t g   = (input[w] >> 8) & 0xff;
            uint8_t b   = (input[w]) & 0xff;
            uint8_t a   = 0xff;

            // Force the alpha channel
            output[w]    = (a << 24) | (b << 16) | (g << 8) | r;
       }
    }
}

/**
 * Called when the core is updating the video.
 */
static void LibretroVideoRefresh(const void *data, unsigned width, unsigned height, size_t pitch) {
    // Only act when there is usable pixel data.
    if (!data) {
        TraceLog(LOG_WARNING, "LIBRETRO: VideoRefresh provided no pixel data");
        return;
    }

    // Resize the video if needed.
    if (width != LibretroCore.width || height != LibretroCore.height) {
        LibretroCore.width = width;
        LibretroCore.height = height;
        LibretroInitVideo();
    }

    switch (LibretroCore.pixelFormat) {
        case RETRO_PIXEL_FORMAT_RGB565: {
            // For small pitches, we can use the data 1:1, otherwise we need to adjust it.
            if (pitch == width * 2) {
                // Core: FCEUM
                UpdateTexture(LibretroCore.texture, data);
            }
            else {
                // Core: SNES9x
                const uint8_t *src = (const uint8_t *)data;
                uint8_t *dst = (uint8_t *)LibretroCore.frameBuffer;
                size_t row_bytes = width * 2;
                for (unsigned h = 0; h < height; h++) {
                    memcpy(dst, src, row_bytes);
                    src += pitch;
                    dst += row_bytes;
                }
                UpdateTexture(LibretroCore.texture, LibretroCore.frameBuffer);
            }
        }
        break;
        case RETRO_PIXEL_FORMAT_0RGB1555: {
            LibretroMapPixelFormatARGB1555ToRGB565(LibretroCore.frameBuffer, data, width, height,
                GetPixelDataSize(width, 1, PIXELFORMAT_UNCOMPRESSED_R5G6B5),
                pitch);
            UpdateTexture(LibretroCore.texture, LibretroCore.frameBuffer);
        }
        break;
        case RETRO_PIXEL_FORMAT_XRGB8888: {
            // Core: Blastem
            // Core: BSNES
            LibretroMapPixelFormatARGB8888ToRGBA8888(LibretroCore.frameBuffer, data,
                width, height,
                GetPixelDataSize(width, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8), pitch);
            UpdateTexture(LibretroCore.texture, LibretroCore.frameBuffer);
        }
        break;
    }
}

static void LibretroInputPoll() {
    // Mouse
    LibretroCore.inputLastMousePosition = LibretroCore.inputMousePosition;
    LibretroCore.inputMousePosition = GetMousePosition();
}

static int16_t LibretroInputState(unsigned port, unsigned device, unsigned index, unsigned id) {
    switch (device) {
        case RETRO_DEVICE_KEYBOARD: {
            int raylibKey = LibretroMapRetroKeyToKeyboardKey(id);
            if (raylibKey > 0) {
                return (int)IsKeyDown(raylibKey);
            }
            return 0;
        }
        case RETRO_DEVICE_JOYPAD: {
            // Return a bitmask of all buttons when requested.
            if (id == RETRO_DEVICE_ID_JOYPAD_MASK) {
                int16_t mask = 0;
                for (int btn = 0; btn < 16; btn++) {
                    if (LibretroInputState(port, RETRO_DEVICE_JOYPAD, index, btn)) {
                        mask |= (1 << btn);
                    }
                }
                return mask;
            }

            // Port 0: gamepad if available, otherwise keyboard.
            if (port == 0) {
                if (IsGamepadAvailable(0)) {
                    int gamepadButton = LibretroMapRetroJoypadButtonToGamepadButton(id);
                    if (gamepadButton != GAMEPAD_BUTTON_UNKNOWN && IsGamepadButtonDown(0, gamepadButton)) {
                        return 1;
                    }
                }
                int retroKey = LibretroMapRetroJoypadButtonToRetroKey(id);
                if (retroKey == RETROK_UNKNOWN) {
                    return 0;
                }
                int raylibKey = LibretroMapRetroKeyToKeyboardKey(retroKey);
                return (raylibKey > 0) ? (int)IsKeyDown(raylibKey) : 0;
            }

            // Port 1+: map to gamepad (port 1 → gamepad 0).
            if (!IsGamepadAvailable(port - 1)) {
                return 0;
            }
            int gamepadButton = LibretroMapRetroJoypadButtonToGamepadButton(id);
            return (int)IsGamepadButtonDown(port - 1, gamepadButton);
        }
        case RETRO_DEVICE_MOUSE: {
            switch (id) {
                case RETRO_DEVICE_ID_MOUSE_LEFT:
                    return IsMouseButtonDown(MOUSE_LEFT_BUTTON);
                case RETRO_DEVICE_ID_MOUSE_RIGHT:
                    return IsMouseButtonDown(MOUSE_RIGHT_BUTTON);
                case RETRO_DEVICE_ID_MOUSE_MIDDLE:
                    return IsMouseButtonDown(MOUSE_MIDDLE_BUTTON);
                case RETRO_DEVICE_ID_MOUSE_X:
                    return LibretroCore.inputMousePosition.x - LibretroCore.inputLastMousePosition.x;
                case RETRO_DEVICE_ID_MOUSE_Y:
                    return LibretroCore.inputMousePosition.y - LibretroCore.inputLastMousePosition.y;
                case RETRO_DEVICE_ID_MOUSE_WHEELUP:
                    return GetMouseWheelMove() > 0;
                case RETRO_DEVICE_ID_MOUSE_WHEELDOWN:
                    return GetMouseWheelMove() < 0;
                case RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELUP:
                    return GetMouseWheelMoveV().x > 0;
                case RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELDOWN:
                    return GetMouseWheelMoveV().x < 0;
                case RETRO_DEVICE_ID_MOUSE_BUTTON_4:
                    return IsMouseButtonDown(MOUSE_BUTTON_SIDE);
                case RETRO_DEVICE_ID_MOUSE_BUTTON_5:
                    return IsMouseButtonDown(MOUSE_BUTTON_EXTRA);
            }
            return 0;
        }

        case RETRO_DEVICE_ANALOG: {
            switch (index) {
                case RETRO_DEVICE_INDEX_ANALOG_LEFT: {
                    int axis = (id == RETRO_DEVICE_ID_ANALOG_X) ? GAMEPAD_AXIS_LEFT_X : GAMEPAD_AXIS_LEFT_Y;
                    return (int16_t)(GetGamepadAxisMovement(port, axis) * 0x7fff);
                }
                case RETRO_DEVICE_INDEX_ANALOG_RIGHT: {
                    int axis = (id == RETRO_DEVICE_ID_ANALOG_X) ? GAMEPAD_AXIS_RIGHT_X : GAMEPAD_AXIS_RIGHT_Y;
                    return (int16_t)(GetGamepadAxisMovement(port, axis) * 0x7fff);
                }
                case RETRO_DEVICE_INDEX_ANALOG_BUTTON: {
                    if (id == RETRO_DEVICE_ID_JOYPAD_L2) {
                        float value = GetGamepadAxisMovement(port, GAMEPAD_AXIS_LEFT_TRIGGER);
                        return (int16_t)((value + 1.0f) * 0.5f * 0x7fff);
                    }
                    if (id == RETRO_DEVICE_ID_JOYPAD_R2) {
                        float value = GetGamepadAxisMovement(port, GAMEPAD_AXIS_RIGHT_TRIGGER);
                        return (int16_t)((value + 1.0f) * 0.5f * 0x7fff);
                    }
                    int gamepadButton = LibretroMapRetroJoypadButtonToGamepadButton(id);
                    return IsGamepadButtonDown(port, gamepadButton) ? 0x7fff : 0;
                }
            }
            return 0;
        }

        case RETRO_DEVICE_POINTER: {
            switch (id) {
                case RETRO_DEVICE_ID_POINTER_X:
                    return (int16_t)(((float)GetMouseX() / (float)GetScreenWidth()) * 2.0f - 1.0f) * 0x7fff;
                case RETRO_DEVICE_ID_POINTER_Y:
                    return (int16_t)(((float)GetMouseY() / (float)GetScreenHeight()) * 2.0f - 1.0f) * 0x7fff;
                case RETRO_DEVICE_ID_POINTER_PRESSED:
                    return IsMouseButtonDown(MOUSE_LEFT_BUTTON);
            }
            return 0;
        }

    }

    return 0;
}

/**
 * Audio stream callback: pulls from the ring buffer into raylib's audio system.
 */
static void LibretroAudioStreamCallback(void *audioData, unsigned int frameCount) {
    float *output = (float *)audioData;

    if (!LibretroCore.audioRingBuffer || LibretroCore.audioRingAvailable == 0) {
        memset(output, 0, frameCount * 2 * sizeof(float));
        return;
    }

    size_t frames_to_read = frameCount;
    if (frames_to_read > LibretroCore.audioRingAvailable) {
        size_t silence_frames = frames_to_read - LibretroCore.audioRingAvailable;
        memset(output + LibretroCore.audioRingAvailable * 2, 0, silence_frames * 2 * sizeof(float));
        frames_to_read = LibretroCore.audioRingAvailable;
    }

    size_t read_pos = (LibretroCore.audioRingWritePos + LibretroCore.audioRingBufferSize - LibretroCore.audioRingAvailable) % LibretroCore.audioRingBufferSize;
    for (size_t i = 0; i < frames_to_read; i++) {
        size_t idx = (read_pos + i) % LibretroCore.audioRingBufferSize;
        output[i * 2]     = LibretroCore.audioRingBuffer[idx * 2];
        output[i * 2 + 1] = LibretroCore.audioRingBuffer[idx * 2 + 1];
    }

    LibretroCore.audioRingAvailable -= frames_to_read;
}

/**
 * Write a batch of int16_t stereo frames into the ring buffer.
 */
static size_t LibretroAudioSampleBatch(const int16_t *data, size_t frames) {
    if (!data || frames == 0 || !LibretroCore.audioRingBuffer) return 0;

    size_t available_space = LibretroCore.audioRingBufferSize - LibretroCore.audioRingAvailable;
    size_t frames_to_write = (frames < available_space) ? frames : available_space;

    if (frames_to_write == 0) {
        if (LibretroCore.audioDropWarnCount++ < 3) {
            TraceLog(LOG_WARNING, "LIBRETRO: Audio ring buffer full, dropping %zu frames", frames);
        }
        return 0;
    }

    for (size_t i = 0; i < frames_to_write; i++) {
        size_t idx = (LibretroCore.audioRingWritePos + i) % LibretroCore.audioRingBufferSize;
        LibretroCore.audioRingBuffer[idx * 2]     = (float)data[i * 2]     / 32768.0f;
        LibretroCore.audioRingBuffer[idx * 2 + 1] = (float)data[i * 2 + 1] / 32768.0f;
    }

    LibretroCore.audioRingWritePos = (LibretroCore.audioRingWritePos + frames_to_write) % LibretroCore.audioRingBufferSize;
    LibretroCore.audioRingAvailable += frames_to_write;

    return frames_to_write;
}

/**
 * Accumulate single-sample callbacks into a buffer before flushing to the ring buffer.
 */
static void LibretroAudioSample(int16_t left, int16_t right) {
    if (LibretroCore.singleSampleCount >= LIBRETRO_AUDIO_SINGLE_SAMPLE_BUFFER_SIZE) {
        LibretroAudioSampleBatch(LibretroCore.singleSampleBuffer, LibretroCore.singleSampleCount);
        LibretroCore.singleSampleCount = 0;
    }

    LibretroCore.singleSampleBuffer[LibretroCore.singleSampleCount * 2]     = left;
    LibretroCore.singleSampleBuffer[LibretroCore.singleSampleCount * 2 + 1] = right;
    LibretroCore.singleSampleCount++;
}

static void LibretroInitAudio() {
    // Allocate the ring buffer (stereo float samples).
    LibretroCore.audioRingBufferSize = LIBRETRO_AUDIO_RING_BUFFER_SIZE;
    LibretroCore.audioRingBuffer = (float *)MemAlloc(LibretroCore.audioRingBufferSize * 2 * sizeof(float));
    LibretroCore.audioRingWritePos = 0;
    LibretroCore.audioRingAvailable = 0;

    // Create the audio stream: 32-bit float, stereo, pulled via callback.
    int sampleSize = 32;
    int channels = 2;
    LibretroCore.audioStream = LoadAudioStream((unsigned int)LibretroCore.sampleRate, sampleSize, channels);
    SetAudioStreamCallback(LibretroCore.audioStream, LibretroAudioStreamCallback);
    SetAudioStreamVolume(LibretroCore.audioStream, LibretroCore.volume);
    PlayAudioStream(LibretroCore.audioStream);

    // Let the core know that the audio device has been initialized.
    if (LibretroCore.audio_callback.set_state) {
        LibretroCore.audio_callback.set_state(true);
    }

    TraceLog(LOG_INFO, "LIBRETRO: Audio stream initialized (%i Hz, %i-bit float, ring buffer %i frames)",
        (int)LibretroCore.sampleRate, sampleSize, (int)LibretroCore.audioRingBufferSize);
}

static bool LibretroInitAudioVideo() {
    LibretroGetAudioVideo();
    LibretroInitVideo();
    LibretroInitAudio();

    SetTargetFPS(LibretroCore.fps);

    return true;
}

static bool LoadLibretroGame(const char* gameFile) {
    // Load empty game.
    if (gameFile == NULL) {
        struct retro_game_info info;
        info.data = NULL;
        info.size = 0;
        info.path = "";
        info.meta = "";
        if (LibretroCore.retro_load_game(&info)) {
            TraceLog(LOG_INFO, "LIBRETRO: Loaded without content");
            LibretroCore.loaded = true;
            return LibretroInitAudioVideo();
        }
        TraceLog(LOG_ERROR, "LIBRETRO: Failed to load core without content");
        return false;
    }

    // Ensure the game exists.
    if (!FileExists(gameFile)) {
        TraceLog(LOG_ERROR, "LIBRETRO: Given content does not exist: %s", gameFile);
        LibretroCore.loaded = false;
        return false;
    }

    // See if we just need the full path.
    if (LibretroCore.needFullpath) {
        struct retro_game_info info;
        info.data = NULL;
        info.size = 0;
        info.path = gameFile;
        if (LibretroCore.retro_load_game(&info)) {
            TraceLog(LOG_INFO, "LIBRETRO: Loaded content with full path");
            LibretroCore.loaded = true;
            return LibretroInitAudioVideo();
        }
        else {
            TraceLog(LOG_ERROR, "LIBRETRO: Failed to load full path");
            LibretroCore.loaded = false;
            return false;
        }
    }

    // Load the game data from the given file.
    int size;
    unsigned char * gameData = LoadFileData(gameFile, &size);
    if (size == 0 || gameData == NULL) {
        TraceLog(LOG_ERROR, "LIBRETRO: Failed to load game data with LoadFileData()");
        LibretroCore.loaded = false;
        return false;
    }

    // Load the game.
    struct retro_game_info info;
    info.path = gameFile;
    info.data = gameData;
    info.size = (size_t)size;
    info.meta = "";
    if (!LibretroCore.retro_load_game(&info)) {
        MemFree(gameData);
        TraceLog(LOG_ERROR, "LIBRETRO: Failed to load game data with retro_load_game()");
        LibretroCore.loaded = false;
        return false;
    }

    MemFree(gameData);
    LibretroCore.loaded = true;
    return LibretroInitAudioVideo();
}

static const char* GetLibretroName() {
    return LibretroCore.libraryName;
}

static const char* GetLibretroVersion() {
    return LibretroCore.libraryVersion;
}

static bool DoesLibretroCoreNeedContent() {
    return !LibretroCore.supportNoGame;
}

// Optional WASM core loading – included here so it can reference the callbacks above.
#ifdef RAYLIB_LIBRETRO_WASM
#include "raylib-libretro-wasm.h"
#endif

static bool InitLibretro(const char* core) {
    // Ensure the core exists.
    if (!FileExists(core)) {
        TraceLog(LOG_ERROR, "LIBRETRO: Given core doesn't exist: %s", core);
        return false;
    }

    if (LibretroCore.handle != NULL) {
        CloseLibretro();
    }

#ifdef RAYLIB_LIBRETRO_WASM
    // Route .wasm files through the WebAssembly loader.
    if (IsLibretroWasmCore(core)) {
        return InitLibretroWasm(core);
    }
#endif

    // Open the dynamic library.
    LibretroCore.handle = dylib_load(core);
    if (!LibretroCore.handle) {
        TraceLog(LOG_ERROR, "LIBRETRO: Failed to load provided library");
        return false;
    }

    // Find the libretro API version.
    LoadLibretroMethod(retro_api_version);
    LibretroCore.apiVersion = LibretroCore.retro_api_version();
    TraceLog(LOG_INFO, "LIBRETRO: API version: %i", LibretroCore.apiVersion);
    if (LibretroCore.apiVersion != 1) {
        CloseLibretro();
        TraceLog(LOG_ERROR, "LIBRETRO: Incompatible API version");
        return false;
    }

    // Retrieve the libretro core system information.
    LoadLibretroMethod(retro_get_system_info);
    struct retro_system_info systemInfo;
    LibretroCore.retro_get_system_info(&systemInfo);
    TextCopy(LibretroCore.libraryName, systemInfo.library_name);
    TextCopy(LibretroCore.libraryVersion, systemInfo.library_version);
    TextCopy(LibretroCore.validExtensions, systemInfo.valid_extensions);
    LibretroCore.needFullpath = systemInfo.need_fullpath;
    LibretroCore.blockExtract = systemInfo.block_extract;
    TraceLog(LOG_INFO, "LIBRETRO: Core loaded successfully");
    TraceLog(LOG_INFO, "    > Name:         %s", LibretroCore.libraryName);
    TraceLog(LOG_INFO, "    > Version:      %s", LibretroCore.libraryVersion);
    if (TextLength(LibretroCore.validExtensions) > 0) {
        TraceLog(LOG_INFO, "    > Extensions:   %s", LibretroCore.validExtensions);
    }

    // TODO: Add ability to peek inside the core to grab data.
    /*
    if (peek) {
        CloseLibretro();
        return true;
    }
    */

    // Load all other libretro methods.
    LoadLibretroMethod(retro_init);
    LoadLibretroMethod(retro_deinit);
    LoadLibretroMethod(retro_set_environment);
    LoadLibretroMethod(retro_set_video_refresh);
    LoadLibretroMethod(retro_set_audio_sample);
    LoadLibretroMethod(retro_set_audio_sample_batch);
    LoadLibretroMethod(retro_set_input_poll);
    LoadLibretroMethod(retro_set_input_state);
    LoadLibretroMethod(retro_get_system_av_info);
    LoadLibretroMethod(retro_set_controller_port_device);
    LoadLibretroMethod(retro_reset);
    LoadLibretroMethod(retro_run);
    LoadLibretroMethod(retro_serialize_size);
    LoadLibretroMethod(retro_serialize);
    LoadLibretroMethod(retro_unserialize);
    LoadLibretroMethod(retro_cheat_reset);
    LoadLibretroMethod(retro_cheat_set);
    LoadLibretroMethod(retro_load_game);
    LoadLibretroMethod(retro_load_game_special);
    LoadLibretroMethod(retro_unload_game);
    LoadLibretroMethod(retro_get_region);
    LoadLibretroMethod(retro_get_memory_data);
    LoadLibretroMethod(retro_get_memory_size);

    // Initialize the core.
    LibretroCore.shutdown = false;
    LibretroCore.volume = 1.0f;

    // Set up the callbacks.
    LibretroCore.retro_set_video_refresh(LibretroVideoRefresh);
    LibretroCore.retro_set_input_poll(LibretroInputPoll);
    LibretroCore.retro_set_input_state(LibretroInputState);
    LibretroCore.retro_set_audio_sample(LibretroAudioSample);
    LibretroCore.retro_set_audio_sample_batch(LibretroAudioSampleBatch);
    LibretroCore.retro_set_environment(LibretroSetEnvironment);

    // Initialize the core.
    LibretroCore.retro_init();
    return true;
}

static void DrawLibretroTexture(int posX, int posY, Color tint) {
    if (LibretroCore.loaded == false) {
        return;
    }
    DrawTexture(LibretroCore.texture, posX, posY, tint);
}

static void DrawLibretroV(Vector2 position, Color tint) {
    if (LibretroCore.loaded == false) {
        return;
    }
    DrawTextureV(LibretroCore.texture, position, tint);
}

static void DrawLibretroEx(Vector2 position, float rotation, float scale, Color tint) {
    if (LibretroCore.loaded == false) {
        return;
    }
    DrawTextureEx(LibretroCore.texture, position, rotation, scale, tint);
}

static void DrawLibretroPro(Rectangle destRec, Color tint) {
    if (LibretroCore.loaded == false) {
        return;
    }
    Rectangle source = {0, 0, LibretroCore.width, LibretroCore.height};
    Vector2 origin = {0, 0};
    DrawTexturePro(LibretroCore.texture, source, destRec, origin, 0, tint);
}

static bool DrawLibretroMessage() {
    if (GetTime() > LibretroCore.osdEndTime) {
        return false;
    }

    int fontSize = 20;
    int padding = 8;
    int textWidth = MeasureText(LibretroCore.osdMessage, fontSize);
    int x = (GetScreenWidth() - textWidth) / 2;
    int y = GetScreenHeight() - fontSize - padding * 2;
    DrawRectangle(x - padding, y - padding, textWidth + padding * 2, fontSize + padding * 2, (Color){0, 0, 0, 180});
    DrawText(LibretroCore.osdMessage, x, y, fontSize, WHITE);
    return true;
}

static void DrawLibretroTint(Color tint) {
    if (LibretroCore.loaded == false) {
        return;
    }

    float rotationDeg = (float)LibretroCore.rotation * 90.0f;
    bool swapDims = (LibretroCore.rotation == 1 || LibretroCore.rotation == 3);

    // Find the aspect ratio.
    float aspect = LibretroCore.aspectRatio;
    if (aspect <= 0) {
        aspect = (float)LibretroCore.width / (float)LibretroCore.height;
    }

    // When rotated 90/270, the visual bounding box has the inverted aspect ratio.
    float visAspect = swapDims ? (1.0f / aspect) : aspect;

    // Calculate the optimal visual width/height to fit the screen.
    int visH = GetScreenHeight();
    int visW = (int)(visH * visAspect);
    if (visW > GetScreenWidth()) {
        visW = GetScreenWidth();
        visH = (int)(visW / visAspect);
    }

    // For 90/270 rotations the dest rect dimensions are swapped relative to visual size
    // because DrawTexturePro applies rotation after sizing.
    int destW = swapDims ? visH : visW;
    int destH = swapDims ? visW : visH;

    // Draw centered with rotation around the dest rect's center.
    float cx = GetScreenWidth() / 2.0f;
    float cy = GetScreenHeight() / 2.0f;
    Rectangle source = {0, 0, LibretroCore.width, LibretroCore.height};
    Rectangle dest = {cx, cy, (float)destW, (float)destH};
    Vector2 origin = {destW / 2.0f, destH / 2.0f};
    DrawTexturePro(LibretroCore.texture, source, dest, origin, rotationDeg, tint);
}

static void DrawLibretro() {
    DrawLibretroTint(WHITE);
}

static void ShowLibretroMessage(const char* msg, float duration) {
    TextCopy(LibretroCore.osdMessage, msg);
    LibretroCore.osdEndTime = GetTime() + (double)duration;
}

static void SetLibretroVolume(float volume) {
    LibretroCore.volume = volume;
    SetAudioStreamVolume(LibretroCore.audioStream, volume);
}

static float GetLibretroVolume() {
    return LibretroCore.volume;
}

static unsigned GetLibretroRotation() {
    return LibretroCore.rotation;
}

static unsigned GetLibretroWidth() {
    return LibretroCore.width;
}
static unsigned GetLibretroHeight() {
    return LibretroCore.height;
}

static Texture2D GetLibretroTexture() {
    return LibretroCore.texture;
}

/**
 * Retrieve whether or not the game has been loaded.
 */
static bool IsLibretroGameReady() {
    return LibretroCore.loaded && LibretroCore.retro_run != NULL;
}

/**
 * Reset the libretro core.
 */
static void ResetLibretro() {
    if (IsLibretroReady() && LibretroCore.retro_reset) {
        LibretroCore.retro_reset();
    }
}

/**
 * Unload the active libretro game.
 */
static void UnloadLibretroGame() {
    if (LibretroCore.retro_unload_game != NULL) {
        LibretroCore.retro_unload_game();
    }
    LibretroCore.loaded = false;
}

/**
 * Close the libretro core.
 */
static void CloseLibretro() {
    // Let the core know that the audio device has been deinitialized.
    if (LibretroCore.audio_callback.set_state != NULL) {
        LibretroCore.audio_callback.set_state(false);
    }

    // Call retro_deinit() to deinitialize the core.
    if (LibretroCore.retro_deinit != NULL) {
        LibretroCore.retro_deinit();
    }

    // Stop, close and unload all raylib objects.
    StopAudioStream(LibretroCore.audioStream);
    UnloadAudioStream(LibretroCore.audioStream);
    if (LibretroCore.audioRingBuffer != NULL) {
        MemFree(LibretroCore.audioRingBuffer);
        LibretroCore.audioRingBuffer = NULL;
    }
    UnloadTexture(LibretroCore.texture);

    // Close the dynamically loaded handle (or WASM instance).
    if (LibretroCore.handle != NULL) {
#ifdef RAYLIB_LIBRETRO_WASM
        if (LibretroWasm.instance != NULL) {
            CloseLibretroWasm();
        } else {
            dylib_close(LibretroCore.handle);
        }
#else
        dylib_close(LibretroCore.handle);
#endif
        LibretroCore.handle = NULL;
    }

    // Free the frame conversion buffer.
    MemFree(LibretroCore.frameBuffer);

    LibretroCore = (rLibretro){0};
}

/**
 * Map a libretro retro_log_level to a raylib TraceLogType.
 */
static int LibretroMapRetroLogLevelToTraceLogType(int level) {
    switch (level) {
        case RETRO_LOG_DEBUG:
            return LOG_DEBUG;
        case RETRO_LOG_INFO:
            return LOG_INFO;
        case RETRO_LOG_WARN:
            return LOG_WARNING;
        case RETRO_LOG_ERROR:
            return LOG_ERROR;
    }

    return LOG_INFO;
}

/**
 * Map a libretro retro_key to a raylib KeyboardKey.
 */
static int LibretroMapRetroKeyToKeyboardKey(int key) {
    switch (key){
        case RETROK_BACKSPACE:
            return KEY_BACKSPACE;
        case RETROK_TAB:
            return KEY_TAB;
        case RETROK_CLEAR:
            return 0;
        case RETROK_RETURN:
            return KEY_ENTER;
        case RETROK_PAUSE:
            return KEY_PAUSE;
        case RETROK_ESCAPE:
            return KEY_ESCAPE;
        case RETROK_SPACE:
            return KEY_SPACE;
        case RETROK_EXCLAIM:
            return KEY_ONE;
        case RETROK_QUOTEDBL:
            return 0;
        case RETROK_HASH:
            return 0;
        case RETROK_DOLLAR:
            return 0;
        case RETROK_AMPERSAND:
            return 0;
        case RETROK_QUOTE:
            return KEY_APOSTROPHE;
        case RETROK_LEFTPAREN:
            return 0;
        case RETROK_RIGHTPAREN:
            return 0;
        case RETROK_ASTERISK:
            return 0;
        case RETROK_PLUS:
            return 0;
        case RETROK_COMMA:
            return KEY_COMMA;
        case RETROK_MINUS:
            return KEY_MINUS;
        case RETROK_PERIOD:
            return KEY_PERIOD;
        case RETROK_SLASH:
            return KEY_SLASH;
        case RETROK_0:
            return KEY_ZERO;
        case RETROK_1:
            return KEY_ONE;
        case RETROK_2:
            return KEY_TWO;
        case RETROK_3:
            return KEY_THREE;
        case RETROK_4:
            return KEY_FOUR;
        case RETROK_5:
            return KEY_FIVE;
        case RETROK_6:
            return KEY_SIX;
        case RETROK_7:
            return KEY_SEVEN;
        case RETROK_8:
            return KEY_EIGHT;
        case RETROK_9:
            return KEY_NINE;
        case RETROK_COLON:
            return 0;
        case RETROK_SEMICOLON:
            return KEY_SEMICOLON;
        case RETROK_LESS:
            return 0;
        case RETROK_EQUALS:
            return KEY_EQUAL;
        case RETROK_GREATER:
            return 0;
        case RETROK_QUESTION:
            return 0;
        case RETROK_AT:
            return 0;
        case RETROK_LEFTBRACKET:
            return KEY_LEFT_BRACKET;
        case RETROK_BACKSLASH:
            return KEY_BACKSLASH;
        case RETROK_RIGHTBRACKET:
            return KEY_RIGHT_BRACKET;
        case RETROK_CARET:
            return 0;
        case RETROK_UNDERSCORE:
            return 0;
        case RETROK_BACKQUOTE:
            return KEY_GRAVE;
        case RETROK_a:
            return KEY_A;
        case RETROK_b:
            return KEY_B;
        case RETROK_c:
            return KEY_C;
        case RETROK_d:
            return KEY_D;
        case RETROK_e:
            return KEY_E;
        case RETROK_f:
            return KEY_F;
        case RETROK_g:
            return KEY_G;
        case RETROK_h:
            return KEY_H;
        case RETROK_i:
            return KEY_I;
        case RETROK_j:
            return KEY_J;
        case RETROK_k:
            return KEY_K;
        case RETROK_l:
            return KEY_L;
        case RETROK_m:
            return KEY_M;
        case RETROK_n:
            return KEY_N;
        case RETROK_o:
            return KEY_O;
        case RETROK_p:
            return KEY_P;
        case RETROK_q:
            return KEY_Q;
        case RETROK_r:
            return KEY_R;
        case RETROK_s:
            return KEY_S;
        case RETROK_t:
            return KEY_T;
        case RETROK_u:
            return KEY_U;
        case RETROK_v:
            return KEY_V;
        case RETROK_w:
            return KEY_W;
        case RETROK_x:
            return KEY_X;
        case RETROK_y:
            return KEY_Y;
        case RETROK_z:
            return KEY_Z;
        case RETROK_LEFTBRACE:
            return KEY_LEFT_BRACKET;
        case RETROK_BAR:
            return 0;
        case RETROK_RIGHTBRACE:
            return KEY_RIGHT_BRACKET;
        case RETROK_TILDE:
            return KEY_GRAVE;
        case RETROK_DELETE:
            return KEY_DELETE;
        case RETROK_KP0:
            return KEY_KP_0;
        case RETROK_KP1:
            return KEY_KP_1;
        case RETROK_KP2:
            return KEY_KP_2;
        case RETROK_KP3:
            return KEY_KP_3;
        case RETROK_KP4:
            return KEY_KP_4;
        case RETROK_KP5:
            return KEY_KP_5;
        case RETROK_KP6:
            return KEY_KP_6;
        case RETROK_KP7:
            return KEY_KP_7;
        case RETROK_KP8:
            return KEY_KP_8;
        case RETROK_KP9:
            return KEY_KP_9;
        case RETROK_KP_PERIOD:
            return KEY_KP_DECIMAL;
        case RETROK_KP_DIVIDE:
            return KEY_KP_DIVIDE;
        case RETROK_KP_MULTIPLY:
            return KEY_KP_MULTIPLY;
        case RETROK_KP_MINUS:
            return KEY_KP_SUBTRACT;
        case RETROK_KP_PLUS:
            return KEY_KP_ADD;
        case RETROK_KP_ENTER:
            return KEY_KP_ENTER;
        case RETROK_KP_EQUALS:
            return KEY_KP_EQUAL;
        case RETROK_UP:
            return KEY_UP;
        case RETROK_DOWN:
            return KEY_DOWN;
        case RETROK_RIGHT:
            return KEY_RIGHT;
        case RETROK_LEFT:
            return KEY_LEFT;
        case RETROK_INSERT:
            return KEY_INSERT;
        case RETROK_HOME:
            return KEY_HOME;
        case RETROK_END:
            return KEY_END;
        case RETROK_PAGEUP:
            return KEY_PAGE_UP;
        case RETROK_PAGEDOWN:
            return KEY_PAGE_DOWN;
        case RETROK_F1:
            return KEY_F1;
        case RETROK_F2:
            return KEY_F2;
        case RETROK_F3:
            return KEY_F3;
        case RETROK_F4:
            return KEY_F4;
        case RETROK_F5:
            return KEY_F5;
        case RETROK_F6:
            return KEY_F6;
        case RETROK_F7:
            return KEY_F7;
        case RETROK_F8:
            return KEY_F8;
        case RETROK_F9:
            return KEY_F9;
        case RETROK_F10:
            return KEY_F10;
        case RETROK_F11:
            return KEY_F11;
        case RETROK_F12:
            return KEY_F12;
        case RETROK_F13:
            return 0;
        case RETROK_F14:
            return 0;
        case RETROK_F15:
            return 0;
        case RETROK_NUMLOCK:
            return KEY_NUM_LOCK;
        case RETROK_CAPSLOCK:
            return KEY_CAPS_LOCK;
        case RETROK_SCROLLOCK:
            return KEY_SCROLL_LOCK;
        case RETROK_RSHIFT:
            return KEY_RIGHT_SHIFT;
        case RETROK_LSHIFT:
            return KEY_LEFT_SHIFT;
        case RETROK_RCTRL:
            return KEY_RIGHT_CONTROL;
        case RETROK_LCTRL:
            return KEY_LEFT_CONTROL;
        case RETROK_RALT:
            return KEY_RIGHT_ALT;
        case RETROK_LALT:
            return KEY_LEFT_ALT;
        case RETROK_RMETA:
            return KEY_RIGHT_SUPER;
        case RETROK_LMETA:
            return KEY_LEFT_SUPER;
        case RETROK_LSUPER:
            return KEY_LEFT_SUPER;
        case RETROK_RSUPER:
            return KEY_RIGHT_SUPER;
        case RETROK_MODE:
            return 0;
        case RETROK_COMPOSE:
            return 0;
        case RETROK_HELP:
            return 0;
        case RETROK_PRINT:
            return KEY_PRINT_SCREEN;
        case RETROK_SYSREQ:
            return 0;
        case RETROK_BREAK:
            return KEY_PAUSE;
        case RETROK_MENU:
            return KEY_MENU;
        case RETROK_POWER:
            return 0;
        case RETROK_EURO:
            return 0;
        case RETROK_UNDO:
            return 0;
        case RETROK_OEM_102:
            return 0;
    }

    return 0;
}

/**
 * Map a libretro joypad button to a libretro retro_key.
 */
static int LibretroMapRetroJoypadButtonToRetroKey(int button) {
    switch (button){
        case RETRO_DEVICE_ID_JOYPAD_B:
            return RETROK_z;
        case RETRO_DEVICE_ID_JOYPAD_Y:
            return RETROK_a;
        case RETRO_DEVICE_ID_JOYPAD_SELECT:
            return RETROK_RSHIFT;
        case RETRO_DEVICE_ID_JOYPAD_START:
            return RETROK_RETURN;
        case RETRO_DEVICE_ID_JOYPAD_UP:
            return RETROK_UP;
        case RETRO_DEVICE_ID_JOYPAD_DOWN:
            return RETROK_DOWN;
        case RETRO_DEVICE_ID_JOYPAD_LEFT:
            return RETROK_LEFT;
        case RETRO_DEVICE_ID_JOYPAD_RIGHT:
            return RETROK_RIGHT;
        case RETRO_DEVICE_ID_JOYPAD_A:
            return RETROK_x;
        case RETRO_DEVICE_ID_JOYPAD_X:
            return RETROK_s;
        case RETRO_DEVICE_ID_JOYPAD_L:
            return RETROK_q;
        case RETRO_DEVICE_ID_JOYPAD_R:
            return RETROK_w;
        case RETRO_DEVICE_ID_JOYPAD_L2:
            return RETROK_e;
        case RETRO_DEVICE_ID_JOYPAD_R2:
            return RETROK_r;
        case RETRO_DEVICE_ID_JOYPAD_L3:
            return RETROK_d;
        case RETRO_DEVICE_ID_JOYPAD_R3:
            return RETROK_f;
    }

    return RETROK_UNKNOWN;
}

/**
 * Make a libretro joypad button to a raylib GamepadButton.
 */
static int LibretroMapRetroJoypadButtonToGamepadButton(int button) {
    switch (button){
        case RETRO_DEVICE_ID_JOYPAD_B:
            return GAMEPAD_BUTTON_RIGHT_FACE_DOWN;
        case RETRO_DEVICE_ID_JOYPAD_Y:
            return GAMEPAD_BUTTON_RIGHT_FACE_LEFT;
        case RETRO_DEVICE_ID_JOYPAD_SELECT:
            return GAMEPAD_BUTTON_MIDDLE_LEFT;
        case RETRO_DEVICE_ID_JOYPAD_START:
            return GAMEPAD_BUTTON_MIDDLE_RIGHT;
        case RETRO_DEVICE_ID_JOYPAD_UP:
            return GAMEPAD_BUTTON_LEFT_FACE_UP;
        case RETRO_DEVICE_ID_JOYPAD_DOWN:
            return GAMEPAD_BUTTON_LEFT_FACE_DOWN;
        case RETRO_DEVICE_ID_JOYPAD_LEFT:
            return GAMEPAD_BUTTON_LEFT_FACE_LEFT;
        case RETRO_DEVICE_ID_JOYPAD_RIGHT:
            return GAMEPAD_BUTTON_LEFT_FACE_RIGHT;
        case RETRO_DEVICE_ID_JOYPAD_A:
            return GAMEPAD_BUTTON_RIGHT_FACE_RIGHT;
        case RETRO_DEVICE_ID_JOYPAD_X:
            return GAMEPAD_BUTTON_RIGHT_FACE_UP;
        case RETRO_DEVICE_ID_JOYPAD_L:
            return GAMEPAD_BUTTON_LEFT_TRIGGER_1;
        case RETRO_DEVICE_ID_JOYPAD_R:
            return GAMEPAD_BUTTON_RIGHT_TRIGGER_1;
        case RETRO_DEVICE_ID_JOYPAD_L2:
            return GAMEPAD_BUTTON_LEFT_TRIGGER_2;
        case RETRO_DEVICE_ID_JOYPAD_R2:
            return GAMEPAD_BUTTON_RIGHT_TRIGGER_2;
        case RETRO_DEVICE_ID_JOYPAD_L3:
            return GAMEPAD_BUTTON_LEFT_THUMB;
        case RETRO_DEVICE_ID_JOYPAD_R3:
            return GAMEPAD_BUTTON_RIGHT_THUMB;
    }

    return GAMEPAD_BUTTON_UNKNOWN;
}

/**
 * Maps a libretro pixel format to a raylib PixelFormat.
 */
static int LibretroMapRetroPixelFormatToPixelFormat(int pixelFormat) {
    switch (pixelFormat) {
        case RETRO_PIXEL_FORMAT_XRGB8888:
            return PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
        case RETRO_PIXEL_FORMAT_RGB565:
            return PIXELFORMAT_UNCOMPRESSED_R5G6B5;
        case RETRO_PIXEL_FORMAT_0RGB1555:
        default:
            return PIXELFORMAT_UNCOMPRESSED_R5G6B5;
    }
}

/**
 * Convert a pixel format from 1555 to 565.
 *
 * Convert a pixel format from 1555 to 565. Used for RETRO_PIXEL_FORMAT_0RGB1555 cores.
 */
static void LibretroMapPixelFormatARGB1555ToRGB565(void *output_, const void *input_,
        int width, int height,
        int out_stride, int in_stride) {
    int h;
    const uint16_t *input = (const uint16_t*)input_;
    uint16_t *output = (uint16_t*)output_;

    for (h = 0; h < height;
            h++,
            output += out_stride >> 1,
            input += in_stride >> 1) {
        int w = 0;

        for (; w < width; w++) {
            uint16_t col  = input[w];
            uint16_t rg   = (col << 1) & ((0x1f << 11) | (0x1f << 6));
            uint16_t b    = col & 0x1f;
            uint16_t glow = (col >> 4) & (1 << 5);
            output[w]     = rg | b | glow;
        }
    }
}

static void* GetLibretroSerializedData(unsigned int* size) {
    if (!IsLibretroGameReady()) {
        return NULL;
    }

    if (LibretroCore.retro_serialize_size == NULL || LibretroCore.retro_serialize == NULL) {
        return NULL;
    }

    size_t finalSize = LibretroCore.retro_serialize_size();
    if (finalSize == 0) {
        return NULL;
    }
    if (size != NULL) {
        *size = (unsigned int)finalSize;
    }

    void* saveData = MemAlloc((unsigned int)finalSize);
    if (saveData == NULL) {
        return NULL;
    }

    if (LibretroCore.retro_serialize(saveData, finalSize)) {
        return saveData;
    }
    TraceLog(LOG_ERROR, "LIBRETRO: Failed to get retro_serialize");
    MemFree(saveData);
    return NULL;
}

static bool SetLibretroSerializedData(void* data, unsigned int size) {
    if (!IsLibretroGameReady()) {
        return false;
    }

    if (LibretroCore.retro_unserialize == NULL) {
        return false;
    }

    return LibretroCore.retro_unserialize(data, (size_t)size);
}

#endif

#if defined(__cplusplus)
}
#endif

#endif // RAYLIB_LIBRETRO_IMPLEMENTATION_ONCE
#endif // RAYLIB_LIBRETRO_IMPLEMENTATION
