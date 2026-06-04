/**********************************************************************************************
*
*   raylib-libretro - raylib extension to interact with libretro cores.
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

#ifndef RAYLIB_LIBRETRO_RLIBRETRO_H
#define RAYLIB_LIBRETRO_RLIBRETRO_H

#include <string.h>
#include "raylib.h"

#include "libretro.h"

#if defined(__cplusplus)
extern "C" {
#endif

//------------------------------------------------------------------------------------
// Libretro Raylib Integration (Module: raylib-libretro)
//------------------------------------------------------------------------------------

static bool InitLibretro(const char* core);
static bool PeekLibretroCoreInfo(const char* core);
static bool LoadLibretroGame(const char* gameFile);
static bool IsLibretroReady(void);
static bool IsLibretroGameReady(void);
static void UpdateLibretro(void);
static void UpdateLibretroEx(bool onlyTick);
static bool LibretroShouldClose(void);
static void DrawLibretro(void);
static void DrawLibretroTint(Color tint);
static void DrawLibretroEx(Vector2 position, float rotation, float scale, Color tint);
static void DrawLibretroV(Vector2 position, Color tint);
static void DrawLibretroTexture(int posX, int posY, Color tint);
static void DrawLibretroPro(Rectangle destRec, Color tint);
static double GetLibretroFPS(void);
static float GetLibretroAspectRatio(void);
static const char* GetLibretroName(void);
static const char* GetLibretroContentName(void);
static const char* GetLibretroVersion(void);
static unsigned GetLibretroWidth(void);
static unsigned GetLibretroHeight(void);
static int GetLibretroRotation(void);
static Texture2D GetLibretroTexture(void);
static Image LoadImageFromLibretro();
static bool IsLibretroGameRequired(void);
static bool ResetLibretro(void);
static bool SetLibretroCheat(unsigned index, bool enabled, const char* code);
static bool ResetLibretroCheats(void);
static void UnloadLibretroGame(void);
static void CloseLibretro(void);
static void SetLibretroVolume(float volume);
static float GetLibretroVolume(void);
static void SetLibretroSpeed(float speed);
static float GetLibretroSpeed(void);
static bool SetLibretroCoreOption(const char* key, const char* value);
static const char* GetLibretroCoreOption(const char* key);
static bool ResetLibretroCoreOption(const char* key);
static void ResetAllLibretroCoreOptions(void);
static void* GetLibretroSerializedData(unsigned int* size);
static unsigned int GetLibretroSerializedSize(void);
static bool SetLibretroSerializedData(void* data, unsigned int size);
static void* GetLibretroSRAMData(size_t* size);
static bool SetLibretroSRAMData(const void* data, size_t size);
static void SetLibretroMessage(const char* msg, double duration);
static void SetLibretroMessageEx(const struct retro_message_ext *message);
static bool DrawLibretroMessage(void);
static const char* GetLibretroDirectory(int directory);
static const struct retro_input_descriptor* GetLibretroInputDescriptors(unsigned *count);
static const struct retro_controller_info* GetLibretroControllerInfo(unsigned *count);
static const struct retro_subsystem_info* GetLibretroSubsystemInfo(unsigned *count);
static bool SetLibretroMemoryMaps(const struct retro_memory_map *map);
static void UnloadLibretroMemoryMaps(void);
static const struct retro_memory_descriptor* GetLibretroMemoryMaps(unsigned *count);
static const char* GetLibretroValidExtensions(void);
static bool IsLibretroBlockExtract(void);
static bool GetLibretroNeedFullpath(const char* path, bool* persistent);
static void GetLibretroFileExtensionPattern(const char* exts, char* pattern, size_t patternSize);
static bool LoadLibretroGameFromMemoryEx(unsigned char* fileData, int dataSize,
    const char* contentPath, bool persistent);
static void SetLibretroDynamicRateControl(bool enabled);
static bool IsLibretroDynamicRateControlEnabled(void);
static bool SetLibretroPortDevice(unsigned port, unsigned device);
static unsigned GetLibretroPortDevice(unsigned port);

static void LibretroPixelFormatARGB1555ToRGB565(void *output_, const void *input_,
        int width, int height,
        int out_stride, int in_stride);
static void LibretroMapPixelFormatARGB8888ToRGBA8888(void *output_, const void *input_,
    int width, int height,
    int out_stride, int in_stride);

static int LibretroRetroPixelFormatToPixelFormat(int pixelFormat);
static int LibretroRetroJoypadButtonToGamepadButton(int button);
static int LibretroRetroJoypadButtontoRetroKey(int button);
static bool LibretroAnalogToDpadPressed(int port, int btn);
static int LibretroRetroKeyToKeyboardKey(int key);
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
// Per-extension content info overrides (RETRO_ENVIRONMENT_SET_CONTENT_INFO_OVERRIDE).
#define LIBRETRO_MAX_CONTENT_INFO_OVERRIDES 16
#define LIBRETRO_CONTENT_INFO_OVERRIDE_EXTS_LEN 256

// Core options/variables storage limits
#define LIBRETRO_MAX_CORE_VARIABLES      512
#define LIBRETRO_CORE_VARIABLE_KEY_LEN   512
#define LIBRETRO_CORE_VARIABLE_VALUE_LEN 512
#define LIBRETRO_CORE_VARIABLE_LABEL_LEN 512
#define LIBRETRO_CORE_VARIABLE_VALUES_LEN 512
#define LIBRETRO_CORE_VARIABLE_TOOLTIP_LEN 256
#define LIBRETRO_MAX_CORE_CATEGORIES     64
#define LIBRETRO_CORE_CATEGORY_KEY_LEN   64

/**
 * The amount of controller ports with rumble support.
 */
#define RAYLIB_LIBRETRO_RUMBLE_PORTS 4

// Four joypads multiplexed through one port; index (0-3) selects the sub-controller.
#ifndef RETRO_DEVICE_JOYPAD_MULTITAP
#define RETRO_DEVICE_JOYPAD_MULTITAP RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 1)
#endif

/**
 * Load a symbol from the libretro handle.
 */
#define LoadLibretroMethodHandle(V, S) do {\
    function_t func = dylib_proc(LIBRETRO.core.symbols.handle, #S); \
    memcpy(&V, &func, sizeof(func)); \
    if (!func) { \
        TraceLog(LOG_ERROR, "LIBRETRO: Failed to load symbol '" #S "' ", dylib_error()); \
        return false; \
    }\
} while (0)

/**
 * Loads a method from the libretro core handle.
 */
#define LoadLibretroMethod(S) LoadLibretroMethodHandle(LIBRETRO.core.symbols.S, S)

/**
 * Dynamic library handle plus the function pointers resolved from it.
 */
typedef struct LibretroCoreSymbols {
    void *handle; /** The dynamic linked library handle. */
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
} LibretroCoreSymbols;

/**
 * Dynamic library symbols for a libretro core.
 */
typedef struct LibretroCoreData {
    LibretroCoreSymbols symbols;

    // System Information.
    bool shutdown; /** Indicates whether or not the core requested to shutdown. */
    unsigned width, height;
    double fps; /** Core's target frame rate from retro_system_timing (e.g. 59.94), kept fractional for accurate pacing. */
    double sampleRate;
    float aspectRatio;
    char corePath[RAYLIB_LIBRETRO_VFS_MAX_PATH];
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
    uint64_t serializationQuirks; /** Bitmask from RETRO_ENVIRONMENT_SET_SERIALIZATION_QUIRKS. */

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
    bool textureRebuild;

    // Pre-allocated frame conversion buffer (avoids per-frame MemAlloc).
    void *frameBuffer;
    size_t frameBufferSize;

    // Audio
    AudioStream audioStream;
    float *audioRingBuffer;
    size_t audioRingBufferSize;
    size_t audioRingWritePos;
    size_t audioRingAvailable;
    unsigned minimumAudioLatencyMs; // RETRO_ENVIRONMENT_SET_MINIMUM_AUDIO_LATENCY
    struct retro_audio_buffer_status_callback audio_buffer_status_callback; // RETRO_ENVIRONMENT_SET_AUDIO_BUFFER_STATUS_CALLBACK
    float drcAdjustment;  // DRC pitch multiplier, clamped to [0.995, 1.005]
    bool drcEnabled;      // Whether Dynamic Rate Control is active

    // Callbacks
    retro_keyboard_event_t keyboard_event;
    retro_core_options_update_display_callback_t options_update_display_callback;
    struct retro_vfs_interface vfs_interface;

    // Input descriptors (RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS)
    struct retro_input_descriptor *inputDescriptors;
    unsigned inputDescriptorCount;

    // Controller info (RETRO_ENVIRONMENT_SET_CONTROLLER_INFO)
    struct retro_controller_info *controllerInfo;
    unsigned controllerPortCount;

    // Subsystem info (RETRO_ENVIRONMENT_SET_SUBSYSTEM_INFO)
    struct retro_subsystem_info *subsystemInfo;
    unsigned subsystemCount;

    // Core variables/options
    // TODO: Switch these to MemAlloc'ed strings.
    char variableKeys[LIBRETRO_MAX_CORE_VARIABLES][LIBRETRO_CORE_VARIABLE_KEY_LEN];
    char variableValues[LIBRETRO_MAX_CORE_VARIABLES][LIBRETRO_CORE_VARIABLE_VALUE_LEN];
    char variableDefaults[LIBRETRO_MAX_CORE_VARIABLES][LIBRETRO_CORE_VARIABLE_VALUE_LEN];
    char variableLabels[LIBRETRO_MAX_CORE_VARIABLES][LIBRETRO_CORE_VARIABLE_LABEL_LEN];
    char variableValuesList[LIBRETRO_MAX_CORE_VARIABLES][LIBRETRO_CORE_VARIABLE_VALUES_LEN];
    char variableDisplayList[LIBRETRO_MAX_CORE_VARIABLES][LIBRETRO_CORE_VARIABLE_VALUES_LEN];
    char variableTooltips[LIBRETRO_MAX_CORE_VARIABLES][LIBRETRO_CORE_VARIABLE_TOOLTIP_LEN];
    char variableCategoryKeys[LIBRETRO_MAX_CORE_VARIABLES][LIBRETRO_CORE_CATEGORY_KEY_LEN];
    bool variableVisible[LIBRETRO_MAX_CORE_VARIABLES];
    unsigned variableCount;
    char categoryKeys[LIBRETRO_MAX_CORE_CATEGORIES][LIBRETRO_CORE_CATEGORY_KEY_LEN];
    char categoryLabels[LIBRETRO_MAX_CORE_CATEGORIES][LIBRETRO_CORE_VARIABLE_LABEL_LEN];
    unsigned categoryCount;
    unsigned variableOptionsVersion; // 0=legacy SET_VARIABLES, 1=SET_CORE_OPTIONS, 2=SET_CORE_OPTIONS_V2
    bool variablesDirty; // Whether or not the variables have been changed since last RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE call.
    bool variablesVisibilityDirty; // Whether option visibility changed and the menu should rebuild.

    // Screen rotation: 0=0°, 1=90°, 2=180°, 3=270°
    int rotation;

    // Single-sample audio accumulation buffer (avoids static locals).
    int16_t singleSampleBuffer[LIBRETRO_AUDIO_SINGLE_SAMPLE_BUFFER_SIZE * 2];
    size_t singleSampleCount;
    int audioDropWarnCount;

    // Accumulated in-game time in nanoseconds (does not advance while menu is open).
    retro_perf_tick_t gameTimeNSEC;

    // Loaded content path (empty if no content loaded).
    char contentPath[RAYLIB_LIBRETRO_VFS_MAX_PATH];

    // Backing storage and struct for RETRO_ENVIRONMENT_GET_GAME_INFO_EXT.
    // Pointers in gameInfoExt reference these buffers (or are NULL).
    char contentDir[RAYLIB_LIBRETRO_VFS_MAX_PATH];
    char contentName[RAYLIB_LIBRETRO_VFS_MAX_PATH];
    char contentExt[16];
    struct retro_game_info_ext gameInfoExt;
    bool gameInfoExtValid;

    float rumbleStrong[RAYLIB_LIBRETRO_RUMBLE_PORTS];
    float rumbleWeak[RAYLIB_LIBRETRO_RUMBLE_PORTS];

    // Per-extension content info overrides from
    // RETRO_ENVIRONMENT_SET_CONTENT_INFO_OVERRIDE.
    char contentInfoOverrideExts[LIBRETRO_MAX_CONTENT_INFO_OVERRIDES][LIBRETRO_CONTENT_INFO_OVERRIDE_EXTS_LEN];
    bool contentInfoOverrideNeedFullpath[LIBRETRO_MAX_CONTENT_INFO_OVERRIDES];
    bool contentInfoOverridePersistent[LIBRETRO_MAX_CONTENT_INFO_OVERRIDES];
    unsigned contentInfoOverrideCount;

    // Persistent ROM buffer kept alive when an override sets persistent_data=true.
    unsigned char* persistentGameData;
    int persistentGameDataSize;

    // Virtual joypad state injected by touch controls (port 0 only).
    bool virtualJoypadState[16];

    // Per-port device type; default RETRO_DEVICE_JOYPAD. Set via SetLibretroPortDevice().
    unsigned portDeviceMap[16];

    // Memory map from RETRO_ENVIRONMENT_SET_MEMORY_MAPS (owned copy).
    struct retro_memory_descriptor *memoryMapDescriptors;
    unsigned memoryMapDescriptorCount;
} LibretroCoreData;

typedef struct LibretroData {
    // Persistent settings that survive core loads.
    float volume;
    float speed;
    double speedAccumulator;
    int textureFilter; // TextureFilter
    int analogToDpadIndex; // 0=None, 1=Left Analog, 2=Right Analog
    int keyboardPlayer1[RETRO_DEVICE_ID_JOYPAD_R3 + 1];
    char coreDirectory[RAYLIB_LIBRETRO_VFS_MAX_PATH];
    char saveDirectory[RAYLIB_LIBRETRO_VFS_MAX_PATH];
    char coreAssetsDirectory[RAYLIB_LIBRETRO_VFS_MAX_PATH];
    char systemDirectory[RAYLIB_LIBRETRO_VFS_MAX_PATH];
    char playlistsDirectory[RAYLIB_LIBRETRO_VFS_MAX_PATH];
    char fileBrowserStartDirectory[RAYLIB_LIBRETRO_VFS_MAX_PATH];

    /**
     * The username for the libretro cores.
     *
     * @see RETRO_ENVIRONMENT_GET_USERNAME
     */
    char username[128];

    // OSD message (RETRO_ENVIRONMENT_SET_MESSAGE / SET_MESSAGE_EXT).
    // Lives here rather than in `core` so a core-exit error message survives the
    // core/content unload, as the libretro spec requires. osd holds the active
    // message's metadata (type/priority/progress/level/target); osd.msg always
    // points at our owned osdMessage buffer rather than the core's transient
    // string. osdEndTime is the absolute GetTime() expiry.
    char osdMessage[256];
    double osdEndTime;
    struct retro_message_ext osd;

    // Per-core state (reset with memset(0) on core unload).
    LibretroCoreData core;

} LibretroData;

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * The global libretro core object.
 *
 * TODO: Figure out a way to have LIBRETRO not be a static global?
 */
static LibretroData LIBRETRO = {
    .volume = 1.0f,
    .speed = 1.0f,
    .username = "raylib",
    .keyboardPlayer1 = {
        [RETRO_DEVICE_ID_JOYPAD_B]      = KEY_Z,
        [RETRO_DEVICE_ID_JOYPAD_Y]      = KEY_A,
        [RETRO_DEVICE_ID_JOYPAD_SELECT] = KEY_RIGHT_SHIFT,
        [RETRO_DEVICE_ID_JOYPAD_START]  = KEY_ENTER,
        [RETRO_DEVICE_ID_JOYPAD_UP]     = KEY_UP,
        [RETRO_DEVICE_ID_JOYPAD_DOWN]   = KEY_DOWN,
        [RETRO_DEVICE_ID_JOYPAD_LEFT]   = KEY_LEFT,
        [RETRO_DEVICE_ID_JOYPAD_RIGHT]  = KEY_RIGHT,
        [RETRO_DEVICE_ID_JOYPAD_A]      = KEY_X,
        [RETRO_DEVICE_ID_JOYPAD_X]      = KEY_S,
        [RETRO_DEVICE_ID_JOYPAD_L]      = KEY_Q,
        [RETRO_DEVICE_ID_JOYPAD_R]      = KEY_W,
        [RETRO_DEVICE_ID_JOYPAD_L2]     = KEY_E,
        [RETRO_DEVICE_ID_JOYPAD_R2]     = KEY_R,
        [RETRO_DEVICE_ID_JOYPAD_L3]     = KEY_D,
        [RETRO_DEVICE_ID_JOYPAD_R3]     = KEY_F,
    }
};

static void LibretroInitCoreVariable(const char *key, const char *defaultValue,
    const char *label, const char *valuesList, const char *displayList,
    const char *tooltip, const char *categoryKey) {
    for (unsigned i = 0; i < LIBRETRO.core.variableCount; i++) {
        if (TextIsEqual(LIBRETRO.core.variableKeys[i], key)) {
            return; // Already registered; don't overwrite user-set value.
        }
    }
    if (LIBRETRO.core.variableCount >= LIBRETRO_MAX_CORE_VARIABLES) {
        TraceLog(LOG_WARNING, "LIBRETRO: Core variable limit reached, ignoring: %s", key);
        return;
    }
    unsigned n = LIBRETRO.core.variableCount;
    TextCopy(LIBRETRO.core.variableKeys[n],         key);
    TextCopy(LIBRETRO.core.variableValues[n],       defaultValue  ? defaultValue  : "");
    TextCopy(LIBRETRO.core.variableDefaults[n],     defaultValue  ? defaultValue  : "");
    TextCopy(LIBRETRO.core.variableLabels[n],       label         ? label         : "");
    TextCopy(LIBRETRO.core.variableValuesList[n],   valuesList    ? valuesList    : "");
    TextCopy(LIBRETRO.core.variableDisplayList[n],  displayList   ? displayList   : "");
    TextCopy(LIBRETRO.core.variableTooltips[n],     tooltip       ? tooltip       : "");
    TextCopy(LIBRETRO.core.variableCategoryKeys[n], categoryKey   ? categoryKey   : "");
    LIBRETRO.core.variableVisible[n] = true;
    LIBRETRO.core.variableCount++;
    // A new option appeared: cores that register variables after the menu has
    // been built (e.g. mgba registering GB model options during its deferred
    // setup on first retro_run) need the Core Options section to refresh.
    LIBRETRO.core.variablesVisibilityDirty = true;
}

static const char *LibretroGetCoreVariable(const char *key) {
    for (unsigned i = 0; i < LIBRETRO.core.variableCount; i++) {
        if (TextIsEqual(LIBRETRO.core.variableKeys[i], key)) {
            return LIBRETRO.core.variableValues[i];
        }
    }
    return NULL;
}

/**
 * Set a core option by key.
 * @param key   Option key as reported by the core via RETRO_ENVIRONMENT_SET_VARIABLES.
 * @param value New value for the option.
 * @return true if the key was found and the value applied; false if the key is unknown. */
static bool SetLibretroCoreOption(const char *key, const char *value) {
    for (unsigned i = 0; i < LIBRETRO.core.variableCount; i++) {
        if (TextIsEqual(LIBRETRO.core.variableKeys[i], key)) {
            snprintf(LIBRETRO.core.variableValues[i], LIBRETRO_CORE_VARIABLE_VALUE_LEN, "%s", value);
            LIBRETRO.core.variablesDirty = true;
            return true;
        }
    }
    return false;
}

/**
 * Get a core option value by key.
 * @param key Option key to look up.
 * @return Current value string, or NULL if the key is not found. */
static const char *GetLibretroCoreOption(const char *key) {
    return LibretroGetCoreVariable(key);
}

/**
 * Reset a single core option to its default value.
 * @param key Option key to reset.
 * @return true if the key was found and reset; false if unknown. */
static bool ResetLibretroCoreOption(const char *key) {
    for (unsigned i = 0; i < LIBRETRO.core.variableCount; i++) {
        if (TextIsEqual(LIBRETRO.core.variableKeys[i], key)) {
            snprintf(LIBRETRO.core.variableValues[i], LIBRETRO_CORE_VARIABLE_VALUE_LEN,
                "%s", LIBRETRO.core.variableDefaults[i]);
            LIBRETRO.core.variablesDirty = true;
            return true;
        }
    }
    return false;
}

/**
 * Reset all core options to their default values. */
static void ResetAllLibretroCoreOptions(void) {
    for (unsigned i = 0; i < LIBRETRO.core.variableCount; i++) {
        snprintf(LIBRETRO.core.variableValues[i], LIBRETRO_CORE_VARIABLE_VALUE_LEN,
            "%s", LIBRETRO.core.variableDefaults[i]);
    }
    LIBRETRO.core.variablesDirty = true;
}

/**
 * Retrieves the directory that is configured as a libretro directory.
 *
 * @param directory The key of which directory to retrieve. Can be...
 * - RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY
 * - RETRO_ENVIRONMENT_GET_CORE_ASSETS_DIRECTORY
 * - RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY
 * - RETRO_ENVIRONMENT_GET_PLAYLIST_DIRECTORY
 * - RETRO_ENVIRONMENT_GET_FILE_BROWSER_START_DIRECTORY
 * - RETRO_ENVIRONMENT_GET_CONTENT_DIRECTORY
 *
 * @return The directory that's currently configured for the libretro directory. The application directory by default, or NULL if it's an incorrect directory.
 */
static const char* GetLibretroDirectory(int directory) {
    // Rotating buffer pool so multiple calls in one expression don't clobber each other.
    #define RAYLIB_LIBRETRO_DIR_BUFFERS 4
    static char buffers[RAYLIB_LIBRETRO_DIR_BUFFERS][RAYLIB_LIBRETRO_VFS_MAX_PATH];
    static int bufferIndex = 0;

    char* output = NULL;
    switch (directory) {
        case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY: output = LIBRETRO.saveDirectory; break;
        case RETRO_ENVIRONMENT_GET_CORE_ASSETS_DIRECTORY: output = LIBRETRO.coreAssetsDirectory; break; // RETRO_ENVIRONMENT_GET_CONTENT_DIRECTORY
        case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY: output = LIBRETRO.systemDirectory; break;
        case RETRO_ENVIRONMENT_GET_PLAYLIST_DIRECTORY: output = LIBRETRO.playlistsDirectory; break;
        case RETRO_ENVIRONMENT_GET_FILE_BROWSER_START_DIRECTORY: output = LIBRETRO.fileBrowserStartDirectory; break;
        default: return NULL;
    }

    const char* result = (output == NULL || output[0] == '\0') ? GetApplicationDirectory() : output;

    char* cleaned = buffers[bufferIndex];
    bufferIndex = (bufferIndex + 1) % RAYLIB_LIBRETRO_DIR_BUFFERS;

    // Strip trailing slash to prevent double slashes when building file paths.
    TextCopy(cleaned, result);
    int len = (int)TextLength(cleaned);
    while (len > 1 && (cleaned[len - 1] == '/' || cleaned[len - 1] == '\\')) {
        cleaned[--len] = '\0';
    }
    return cleaned;
}

/**
 * Get the input descriptors reported by the loaded core.
 * @param count Output parameter filled with the number of descriptors.
 * @return Pointer to the descriptor array, or NULL if none were set. */
static const struct retro_input_descriptor* GetLibretroInputDescriptors(unsigned *count) {
    if (count != NULL) *count = LIBRETRO.core.inputDescriptorCount;
    return LIBRETRO.core.inputDescriptors;
}

/**
 * Get the controller port info reported by the loaded core.
 * @param count Output parameter filled with the number of ports.
 * @return Pointer to the controller info array, or NULL if none were set. */
static const struct retro_controller_info* GetLibretroControllerInfo(unsigned *count) {
    if (count != NULL) *count = LIBRETRO.core.controllerPortCount;
    return LIBRETRO.core.controllerInfo;
}

static const struct retro_subsystem_info* GetLibretroSubsystemInfo(unsigned *count) {
    if (count != NULL) *count = LIBRETRO.core.subsystemCount;
    return LIBRETRO.core.subsystemInfo;
}

/**
 * Free the stored memory map descriptors. */
static void UnloadLibretroMemoryMaps(void) {
    if (LIBRETRO.core.memoryMapDescriptors) {
        for (unsigned i = 0; i < LIBRETRO.core.memoryMapDescriptorCount; i++) {
            if (LIBRETRO.core.memoryMapDescriptors[i].addrspace) {
                MemFree((void*)LIBRETRO.core.memoryMapDescriptors[i].addrspace);
            }
        }
        MemFree(LIBRETRO.core.memoryMapDescriptors);
        LIBRETRO.core.memoryMapDescriptors = NULL;
        LIBRETRO.core.memoryMapDescriptorCount = 0;
    }
}

/**
 * Store an owned deep-copy of the core's memory map descriptors.
 * @param map Pointer to the retro_memory_map provided by the core.
 * @return true on success. */
static bool SetLibretroMemoryMaps(const struct retro_memory_map *map) {
    UnloadLibretroMemoryMaps();
    if (!map || !map->descriptors || map->num_descriptors == 0) {
        return map != NULL;
    }
    size_t sz = sizeof(struct retro_memory_descriptor) * map->num_descriptors;
    LIBRETRO.core.memoryMapDescriptors = (struct retro_memory_descriptor*)MemAlloc((unsigned int)sz);
    if (!LIBRETRO.core.memoryMapDescriptors) {
        return false;
    }
    memcpy(LIBRETRO.core.memoryMapDescriptors, map->descriptors, sz);
    LIBRETRO.core.memoryMapDescriptorCount = map->num_descriptors;
    for (unsigned i = 0; i < map->num_descriptors; i++) {
        if (map->descriptors[i].addrspace) {
            size_t len = strlen(map->descriptors[i].addrspace) + 1;
            char *copy = (char*)MemAlloc((unsigned int)len);
            if (copy) memcpy(copy, map->descriptors[i].addrspace, len);
            LIBRETRO.core.memoryMapDescriptors[i].addrspace = copy;
        }
    }
    TraceLog(LOG_INFO, "LIBRETRO: Memory map stored (%u descriptor(s))", map->num_descriptors);
    return true;
}

/**
 * Get the stored memory map descriptors.
 * @param count Output parameter filled with the number of descriptors.
 * @return Pointer to the descriptor array, or NULL if none are stored. */
static const struct retro_memory_descriptor* GetLibretroMemoryMaps(unsigned *count) {
    if (count != NULL) *count = LIBRETRO.core.memoryMapDescriptorCount;
    return LIBRETRO.core.memoryMapDescriptors;
}

// Returns an absolute path for `path`. The result is either the input pointer
// (already absolute) or a pointer into a non-reentrant static buffer; copy it
// before calling again if you need to keep both values.
static const char* LibretroResolveAbsoluteDirectory(const char* path) {
    if (!path || path[0] == '\0') return path;
    if (path[0] == '/' || path[0] == '\\') return path;
#if defined(_WIN32)
    if (path[1] == ':') return path;
#endif
    static char absPath[RAYLIB_LIBRETRO_VFS_MAX_PATH];
#if defined(_WIN32)
    const char sep = '\\';
#else
    const char sep = '/';
#endif
    int n = snprintf(absPath, sizeof(absPath), "%s%c%s", GetWorkingDirectory(), sep, path);
    if (n < 0 || (size_t)n >= sizeof(absPath)) return path;
    return absPath;
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

static void InitLibretroAudio(void);  // Forward declaration.
static size_t UpdateLibretroAudioSampleBatch(const int16_t *data, size_t frames);  // Forward declaration.

static void CloseLibretroVideo(void) {
    // Unload the existing texture if it exists already.
    if (IsTextureValid(LIBRETRO.core.texture)) {
        UnloadTexture(LIBRETRO.core.texture);
        memset(&LIBRETRO.core.texture, 0, sizeof(LIBRETRO.core.texture));
    }

    if (LIBRETRO.core.frameBuffer != NULL) {
        MemFree(LIBRETRO.core.frameBuffer);
    }
    LIBRETRO.core.frameBuffer = NULL;
    LIBRETRO.core.frameBufferSize = 0;
    LIBRETRO.core.textureRebuild = false;
}

static bool InitLibretroVideo(void) {
    CloseLibretroVideo();

    // Build the rendering image.
    if (LIBRETRO.core.width == 0 || LIBRETRO.core.height == 0) {
        return false;
    }

    Image image = GenImageColor(LIBRETRO.core.width, LIBRETRO.core.height, BLACK);
    if (!IsImageValid(image)) {
        return false;
    }
    ImageFormat(&image, LibretroRetroPixelFormatToPixelFormat(LIBRETRO.core.pixelFormat));

    // Create the texture.
    LIBRETRO.core.texture = LoadTextureFromImage(image);
    UnloadImage(image);
    if (!IsTextureValid(LIBRETRO.core.texture)) {
        return false;
    }

    SetTextureFilter(LIBRETRO.core.texture, LIBRETRO.textureFilter);

    // (Re-)allocate the frame conversion buffer sized for XRGB8888→RGBA8888 (worst case).
    size_t needed = (size_t)GetPixelDataSize(LIBRETRO.core.width, LIBRETRO.core.height, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    if (needed == 0) {
        CloseLibretroVideo();
        return false;
    }
    LIBRETRO.core.frameBuffer = MemAlloc(needed);
    LIBRETRO.core.frameBufferSize = needed;

    LIBRETRO.core.textureRebuild = false;
    return true;
}

/**
 * Retrieve the current in-game time in microseconds.
 *
 * @return retro_time_t The current in-game time in microseconds.
 */
static retro_time_t GetLibretroTimeUSEC(void) {
    return (retro_time_t)(LIBRETRO.core.gameTimeNSEC / 1000);
}

/**
 * Get the CPU Features.
 *
 * @see retro_get_cpu_features_t
 * @return uint64_t Returns a bit-mask of detected CPU features (RETRO_SIMD_*).
 */
static uint64_t GetLibretroCPUFeatures(void) {
    return cpu_features_get();
}

/**
 * A simple counter. Usually nanoseconds, but can also be CPU cycles.
 *
 * @see retro_perf_get_counter_t
 * @return retro_perf_tick_t The current value of the high resolution counter.
 */
static retro_perf_tick_t GetLibretroPerfCounter(void) {
    return LIBRETRO.core.gameTimeNSEC;
}

/**
 * Register a performance counter.
 *
 * @see retro_perf_register_t
 */
static void SetLibretroPerformanceCounter(struct retro_perf_counter* counter) {
    LIBRETRO.core.perf_counter_last = counter;
    counter->registered = true;
}

/**
 * Starts a registered counter.
 *
 * @see retro_perf_start_t
 */
static void StartLibretroPerformanceCounter(struct retro_perf_counter* counter) {
    if (counter->registered) {
        counter->start = GetLibretroPerfCounter();
    }
}

/**
 * Stops a registered counter.
 *
 * @see retro_perf_stop_t
 */
static void StopLibretroPerformanceCounter(struct retro_perf_counter* counter) {
    counter->total = GetLibretroPerfCounter() - counter->start;
}

/**
 * Log and display the state of performance counters.
 *
 * @see retro_perf_log_t
 */
static void LogLibretroPerformanceCounter(void) {
    // TODO: Use a linked list of counters, and loop through them all.
    if (LIBRETRO.core.perf_counter_last == NULL) {
        return;
    }
    TraceLog(LOG_INFO, "LIBRETRO: Timer %s: %i - %i", LIBRETRO.core.perf_counter_last->ident, LIBRETRO.core.perf_counter_last->start, LIBRETRO.core.perf_counter_last->total);
}

static bool SetLibretroRumbleState(unsigned port, enum retro_rumble_effect effect, uint16_t strength) {
    if (port >= RAYLIB_LIBRETRO_RUMBLE_PORTS) {
        return false;
    }
    float normalized = (float)strength / 65535.0f;
    float* slot = (effect == RETRO_RUMBLE_STRONG)
        ? &LIBRETRO.core.rumbleStrong[port]
        : &LIBRETRO.core.rumbleWeak[port];

    // Cores call this every frame, almost always with the value unchanged
    // (commonly 0). Skip the hardware update when nothing changed so we don't
    // spam SetGamepadVibration(). The compare is exact: normalized is derived
    // deterministically from the same integer strength each frame.
    if (*slot == normalized) {
        return true;
    }
    *slot = normalized;
    SetGamepadVibration((int)port, LIBRETRO.core.rumbleStrong[port], LIBRETRO.core.rumbleWeak[port], 3600.0f);
    return true;
}

static bool CallLibretroEnvironment(unsigned cmd, void * data) {
    switch (cmd) {
        case RETRO_ENVIRONMENT_SET_ROTATION: {
            if (data == NULL) {
                return false;
            }
            LIBRETRO.core.rotation = (int)*(const unsigned *)data;
            TraceLog(LOG_INFO, "LIBRETRO: RETRO_ENVIRONMENT_SET_ROTATION: %d (%d°)", LIBRETRO.core.rotation, LIBRETRO.core.rotation * 90);
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
            if (message->msg == NULL || message->msg[0] == '\0') {
                return true;
            }

            TraceLog(LOG_INFO, "LIBRETRO: RETRO_ENVIRONMENT_SET_MESSAGE: %s (%i frames)", message->msg, message->frames);

            // Route through SetLibretroMessage so osd metadata (msg pointer, type,
            // progress, priority) gets its defaults; this legacy call carries none.
            double seconds = message->frames / (LIBRETRO.core.fps > 0 ? LIBRETRO.core.fps : 60.0);
            SetLibretroMessage(message->msg, seconds);
            return true;
        }

        case RETRO_ENVIRONMENT_SHUTDOWN: {
            TraceLog(LOG_INFO, "LIBRETRO: RETRO_ENVIRONMENT_SHUTDOWN");
            LIBRETRO.core.shutdown = true;
            return true;
        }

        case RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL: {
            if (data == NULL) {
                TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL no data provided");
                return false;
            }
            LIBRETRO.core.performanceLevel = *(const unsigned *)data;
            TraceLog(LOG_INFO, "LIBRETRO: RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL(%i)", LIBRETRO.core.performanceLevel);
            return true;
        }

        case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY: {
            const char** dir = (const char**)data;
            if (dir == NULL) {
                TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY no data set");
                return false;
            }
            *dir = GetLibretroDirectory(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY);
            return true;
        }

        case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: {
            const enum retro_pixel_format* pixelFormat = (const enum retro_pixel_format *)data;
            if (pixelFormat == NULL) {
                TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_SET_PIXEL_FORMAT no data set");
                return false;
            }
            LIBRETRO.core.pixelFormat = *pixelFormat;

            switch (LIBRETRO.core.pixelFormat) {
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
                LIBRETRO.core.pixelFormat = RETRO_PIXEL_FORMAT_RGB565;
                return false;
            }
            return true;
        }

        case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS: {
            const struct retro_input_descriptor *desc = (const struct retro_input_descriptor *)data;
            if (desc == NULL) {
                TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS no data set");
                return false;
            }
            unsigned count = 0;
            for (const struct retro_input_descriptor *d = desc; d->description != NULL; d++) {
                count++;
            }
            if (LIBRETRO.core.inputDescriptors != NULL) {
                MemFree(LIBRETRO.core.inputDescriptors);
                LIBRETRO.core.inputDescriptors = NULL;
                LIBRETRO.core.inputDescriptorCount = 0;
            }
            if (count > 0) {
                LIBRETRO.core.inputDescriptors = (struct retro_input_descriptor *)MemAlloc(count * sizeof(struct retro_input_descriptor));
                if (LIBRETRO.core.inputDescriptors != NULL) {
                    for (unsigned i = 0; i < count; i++) {
                        LIBRETRO.core.inputDescriptors[i] = desc[i];
                        TraceLog(LOG_INFO, "LIBRETRO: Input port %u device %u index %u id %u: %s",
                            desc[i].port, desc[i].device, desc[i].index, desc[i].id, desc[i].description);
                    }
                    LIBRETRO.core.inputDescriptorCount = count;
                }
            }
            return true;
        }

        case RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK: {
            if (data == NULL) {
                TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK no data set");
                return false;
            }
            const struct retro_keyboard_callback * callback = (const struct retro_keyboard_callback *)data;
            LIBRETRO.core.keyboard_event = callback->callback;
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
            LIBRETRO.core.variableOptionsVersion = 0;
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
                LibretroInitCoreVariable(var->key, defaultVal, label, valuesList, valuesList, "", "");
            }
            return true;
        }

        case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE: {
            if (data == NULL) {
                TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE no data set");
                return false;
            }
            bool *updated = (bool *)data;
            *updated = LIBRETRO.core.variablesDirty;
            LIBRETRO.core.variablesDirty = false;
            return true;
        }

        case RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME: {
            // When set to true, the core will run without content.
            if (data == NULL) {
                TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME no data provided");
                return false;
            }
            LIBRETRO.core.supportNoGame = *(bool*)data;
            TraceLog(LOG_INFO, "LIBRETRO: RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME: %s", LIBRETRO.core.supportNoGame ? "true" : "false");
            return true;
        }

        case RETRO_ENVIRONMENT_GET_LIBRETRO_PATH: {
            if (data == NULL) {
                TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_GET_LIBRETRO_PATH no data provided");
                return false;
            }
            const char** path = (const char**)data;
            *path = LIBRETRO.core.corePath[0] != '\0' ? LIBRETRO.core.corePath : NULL;
            return true;
        }

        case RETRO_ENVIRONMENT_SET_FRAME_TIME_CALLBACK: {
            if (data == NULL) {
                TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_SET_FRAME_TIME_CALLBACK no data provided");
                return false;
            }
            const struct retro_frame_time_callback *frame_time = (const struct retro_frame_time_callback*)data;
            LIBRETRO.core.runloop_frame_time = *frame_time;
            return true;
        }

        case RETRO_ENVIRONMENT_SET_AUDIO_CALLBACK: {
            if (data == NULL) {
                TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_SET_AUDIO_CALLBACK no data provided");
                return false;
            }
            struct retro_audio_callback *audio_cb = (struct retro_audio_callback*)data;
            LIBRETRO.core.audio_callback = *audio_cb;
            return true;
        }

        case RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE: {
            struct retro_rumble_interface *rumble = (struct retro_rumble_interface*)data;
            if (rumble == NULL) {
                return false;
            }
            rumble->set_rumble_state = SetLibretroRumbleState;
            return true;
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
                (1 << RETRO_DEVICE_LIGHTGUN) |
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
            callback->log = &LibretroLogger;
            return true;
        }

        case RETRO_ENVIRONMENT_GET_PERF_INTERFACE: {
            if (data == NULL) {
                TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_GET_PERF_INTERFACE no data provided");
                return false;
            }
            struct retro_perf_callback *perf = (struct retro_perf_callback *)data;
            perf->get_time_usec = &GetLibretroTimeUSEC;
            perf->get_cpu_features = &GetLibretroCPUFeatures;
            perf->get_perf_counter = &GetLibretroPerfCounter;
            perf->perf_register = &SetLibretroPerformanceCounter;
            perf->perf_start = &StartLibretroPerformanceCounter;
            perf->perf_stop = &StopLibretroPerformanceCounter;
            perf->perf_log = &LogLibretroPerformanceCounter;
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
            *dir = GetLibretroDirectory(RETRO_ENVIRONMENT_GET_CORE_ASSETS_DIRECTORY);
            return true;
        }

        case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY: {
            if (data == NULL) {
                return false;
            }
            const char** dir = (const char**)data;
            *dir = GetLibretroDirectory(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY);
            return true;
        }

        case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO: {
            if (data == NULL) {
                TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO no data provided");
                return false;
            }
            const struct retro_system_av_info av = *(const struct retro_system_av_info *)data;
            bool sampleRateChanged = (av.timing.sample_rate != LIBRETRO.core.sampleRate);
            bool dimsChanged = (av.geometry.base_width != LIBRETRO.core.width || av.geometry.base_height != LIBRETRO.core.height);
            LIBRETRO.core.width = av.geometry.base_width;
            LIBRETRO.core.height = av.geometry.base_height;
            LIBRETRO.core.fps = av.timing.fps;
            LIBRETRO.core.sampleRate = av.timing.sample_rate;
            LIBRETRO.core.aspectRatio = av.geometry.aspect_ratio;
            TraceLog(LOG_INFO, "LIBRETRO: RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO %i x %i @ %.3f FPS",
                LIBRETRO.core.width,
                LIBRETRO.core.height,
                LIBRETRO.core.fps
            );
            if (dimsChanged) {
                InitLibretroVideo();
            }
            if (sampleRateChanged) {
                InitLibretroAudio();
            }
            return true;
        }

        case RETRO_ENVIRONMENT_SET_PROC_ADDRESS_CALLBACK: {
            TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_SET_PROC_ADDRESS_CALLBACK not implemented");
            return false;
        }

        case RETRO_ENVIRONMENT_SET_SUBSYSTEM_INFO: {
            if (data == NULL) {
                TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_SET_SUBSYSTEM_INFO no data provided");
                return false;
            }
            const struct retro_subsystem_info *info = (const struct retro_subsystem_info *)data;
            if (LIBRETRO.core.subsystemInfo != NULL) {
                MemFree(LIBRETRO.core.subsystemInfo);
                LIBRETRO.core.subsystemInfo = NULL;
                LIBRETRO.core.subsystemCount = 0;
            }
            unsigned count = 0;
            for (unsigned i = 0; info[i].ident != NULL; i++) {
                count++;
            }
            if (count > 0) {
                LIBRETRO.core.subsystemInfo = (struct retro_subsystem_info *)MemAlloc(count * sizeof(struct retro_subsystem_info));
                if (LIBRETRO.core.subsystemInfo != NULL) {
                    for (unsigned i = 0; i < count; i++) {
                        LIBRETRO.core.subsystemInfo[i] = info[i];
                        TraceLog(LOG_INFO, "LIBRETRO: RETRO_ENVIRONMENT_SET_SUBSYSTEM_INFO [%u] %s (%s)", i, info[i].desc, info[i].ident);
                    }
                    LIBRETRO.core.subsystemCount = count;
                }
            }
            return true;
        }

        case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO: {
            if (data == NULL) {
                TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_SET_CONTROLLER_INFO no data provided");
                return false;
            }
            const struct retro_controller_info *info = (const struct retro_controller_info *)data;
            if (LIBRETRO.core.controllerInfo != NULL) {
                MemFree(LIBRETRO.core.controllerInfo);
                LIBRETRO.core.controllerInfo = NULL;
                LIBRETRO.core.controllerPortCount = 0;
            }
            unsigned portCount = 0;
            for (unsigned port = 0; info[port].types != NULL; port++) {
                portCount++;
            }
            if (portCount > 0) {
                LIBRETRO.core.controllerInfo = (struct retro_controller_info *)MemAlloc(portCount * sizeof(struct retro_controller_info));
                if (LIBRETRO.core.controllerInfo != NULL) {
                    for (unsigned port = 0; port < portCount; port++) {
                        LIBRETRO.core.controllerInfo[port] = info[port];
                        TraceLog(LOG_INFO, "LIBRETRO: RETRO_ENVIRONMENT_SET_CONTROLLER_INFO port %u (%u types)", port, info[port].num_types);
                        for (unsigned i = 0; i < info[port].num_types; i++) {
                            TraceLog(LOG_INFO, "    > [%u] %s (id: %u)", i, info[port].types[i].desc, info[port].types[i].id);
                        }
                    }
                    LIBRETRO.core.controllerPortCount = portCount;
                }
            }
            return true;
        }

        case RETRO_ENVIRONMENT_SET_MEMORY_MAPS: {
            if (!data) {
                TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_SET_MEMORY_MAPS no data");
                return false;
            }
            return SetLibretroMemoryMaps((const struct retro_memory_map *)data);
        }

        case RETRO_ENVIRONMENT_SET_GEOMETRY: {
            if (!data) {
                TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_SET_GEOMETRY no data provided");
                return false;
            }
            const struct retro_game_geometry *geom = (const struct retro_game_geometry *)data;
            LIBRETRO.core.width = geom->base_width;
            LIBRETRO.core.height = geom->base_height;
            LIBRETRO.core.aspectRatio = geom->aspect_ratio;
            TraceLog(LOG_INFO, "LIBRETRO: RETRO_ENVIRONMENT_SET_GEOMETRY %i x %i @ %f",
                LIBRETRO.core.width,
                LIBRETRO.core.height,
                LIBRETRO.core.aspectRatio
            );
            LIBRETRO.core.textureRebuild = true;
            return true;
        }

        case RETRO_ENVIRONMENT_GET_USERNAME: {
            if (!data) {
                return false;
            }
            const char** name = (const char**)data;
            *name = LIBRETRO.username;
            return true;
        }

        case RETRO_ENVIRONMENT_GET_LANGUAGE: {
            if (data == NULL) {
                TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_GET_LANGUAGE no data provided");
                return false;
            }
            unsigned* language = (unsigned *)data;
            *language = RETRO_LANGUAGE_ENGLISH;
            return true;
        }

        case RETRO_ENVIRONMENT_GET_CURRENT_SOFTWARE_FRAMEBUFFER: {
            static bool get_current_software_framebuffer_warned = false;
            if (!get_current_software_framebuffer_warned) {
                // This is not supported because the framebuffer format may not match the expected PixelFormat.
                TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_GET_CURRENT_SOFTWARE_FRAMEBUFFER not supported");
                get_current_software_framebuffer_warned = true;
            }
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
            const uint64_t* quirks = (const uint64_t*)data;
            if (quirks == NULL) {
                return false;
            }
            LIBRETRO.core.serializationQuirks = *quirks;
            TraceLog(LOG_INFO, "LIBRETRO: RETRO_ENVIRONMENT_SET_SERIALIZATION_QUIRKS: 0x%llx", (unsigned long long)*quirks);
            return true;
        }

        case RETRO_ENVIRONMENT_SET_HW_SHARED_CONTEXT: {
            TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_SET_HW_SHARED_CONTEXT not implemented");
            return false;
        }

        case RETRO_ENVIRONMENT_GET_VFS_INTERFACE: {
            if (data == NULL) {
                TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_GET_VFS_INTERFACE no data provided");
                return false;
            }

            struct retro_vfs_interface_info * vfs_interface = (struct retro_vfs_interface_info *)data;
            vfs_interface->iface = &LIBRETRO.core.vfs_interface;

            // VFS 1
            if (vfs_interface->required_interface_version >= 1) {
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
            }

            // VFS 2
            if (vfs_interface->required_interface_version >= 2) {
                vfs_interface->iface->truncate = &raylib_libretro_vfs_truncate;
            }

            // VFS 3
            if (vfs_interface->required_interface_version >= 3) {
                vfs_interface->iface->stat = &raylib_libretro_vfs_stat;
                vfs_interface->iface->mkdir = &raylib_libretro_vfs_mkdir;
                vfs_interface->iface->opendir = &raylib_libretro_vfs_opendir;
                vfs_interface->iface->readdir = &raylib_libretro_vfs_readdir;
                vfs_interface->iface->dirent_get_name = &raylib_libretro_vfs_dirent_get_name;
                vfs_interface->iface->dirent_is_dir = &raylib_libretro_vfs_dirent_is_dir;
                vfs_interface->iface->closedir = &raylib_libretro_vfs_closedir;
            }

            // VFS 4
            if (vfs_interface->required_interface_version >= 4) {
                vfs_interface->iface->stat_64 = &raylib_libretro_vfs_stat_64;
            }

            // VFS 5+
            if (vfs_interface->required_interface_version >= 5) {
                TraceLog(LOG_ERROR, "LIBRETRO: RETRO_ENVIRONMENT_GET_VFS_INTERFACE version 5+ not supported");
                return false;
            }

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
            *output = (LIBRETRO.speed > 1.0f);
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
            LIBRETRO.core.variableOptionsVersion = 1;
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
                    opts->info ? opts->info : "", "");
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
            return CallLibretroEnvironment(RETRO_ENVIRONMENT_SET_CORE_OPTIONS, (void *)intl->us);
        }

        case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY: {
            const struct retro_core_option_display *disp = (const struct retro_core_option_display *)data;
            if (!disp || !disp->key) return false;
            for (unsigned i = 0; i < LIBRETRO.core.variableCount; i++) {
                if (TextIsEqual(LIBRETRO.core.variableKeys[i], disp->key)) {
                    LIBRETRO.core.variableVisible[i] = disp->visible;
                    LIBRETRO.core.variablesVisibilityDirty = true;
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
                SetLibretroMessageEx(message);
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
            if (data == NULL) {
                LIBRETRO.core.audio_buffer_status_callback.callback = NULL;
                TraceLog(LOG_INFO, "LIBRETRO: RETRO_ENVIRONMENT_SET_AUDIO_BUFFER_STATUS_CALLBACK disabled");
                return true;
            }
            const struct retro_audio_buffer_status_callback *cb =
                (const struct retro_audio_buffer_status_callback *)data;
            LIBRETRO.core.audio_buffer_status_callback = *cb;
            TraceLog(LOG_INFO, "LIBRETRO: RETRO_ENVIRONMENT_SET_AUDIO_BUFFER_STATUS_CALLBACK registered");
            return true;
        }

        case RETRO_ENVIRONMENT_SET_MINIMUM_AUDIO_LATENCY: {
            if (data == NULL) {
                TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_SET_MINIMUM_AUDIO_LATENCY no data provided");
                return false;
            }
            unsigned latencyMs = *(const unsigned *)data;
            if (latencyMs == LIBRETRO.core.minimumAudioLatencyMs) {
                return true;
            }
            LIBRETRO.core.minimumAudioLatencyMs = latencyMs;
            TraceLog(LOG_INFO, "LIBRETRO: RETRO_ENVIRONMENT_SET_MINIMUM_AUDIO_LATENCY: %u ms", latencyMs);
            // Reinit audio so the new minimum is reflected in the ring buffer.
            // Safe before audio init (no-op via the NULL check) and at runtime
            // (matches the SET_SYSTEM_AV_INFO sample-rate-change path above).
            if (LIBRETRO.core.audioRingBuffer != NULL && LIBRETRO.core.sampleRate > 0.0) {
                InitLibretroAudio();
            }
            return true;
        }

        case RETRO_ENVIRONMENT_SET_FASTFORWARDING_OVERRIDE: {
            if (data == NULL) {
                return true;
            }
            const struct retro_fastforwarding_override* override = (const struct retro_fastforwarding_override*)data;
            if (override->fastforward) {
                SetLibretroSpeed(override->ratio >= 1.0f ? override->ratio : 2.0f);
            } else {
                SetLibretroSpeed(1.0f);
            }
            return true;
        }

        case RETRO_ENVIRONMENT_SET_CONTENT_INFO_OVERRIDE: {
            // NULL data is a feature-probe: the core wants to know if we honor
            // overrides at all. Return true so it knows to call us again with
            // its real array.
            if (data == NULL) return true;

            LIBRETRO.core.contentInfoOverrideCount = 0;
            const struct retro_system_content_info_override *o =
                (const struct retro_system_content_info_override *)data;
            for (; o->extensions != NULL; o++) {
                if (LIBRETRO.core.contentInfoOverrideCount >= LIBRETRO_MAX_CONTENT_INFO_OVERRIDES) {
                    TraceLog(LOG_WARNING, "LIBRETRO: Too many content info overrides; dropping rest");
                    break;
                }
                unsigned i = LIBRETRO.core.contentInfoOverrideCount++;
                TextCopy(LIBRETRO.core.contentInfoOverrideExts[i], o->extensions);
                LIBRETRO.core.contentInfoOverrideNeedFullpath[i] = o->need_fullpath;
                LIBRETRO.core.contentInfoOverridePersistent[i]   = o->persistent_data;
                TraceLog(LOG_INFO, "LIBRETRO: content_info_override: ext=\"%s\" need_fullpath=%s persistent=%s",
                    o->extensions,
                    o->need_fullpath ? "true" : "false",
                    o->persistent_data ? "true" : "false");
            }
            return true;
        }

        case RETRO_ENVIRONMENT_GET_GAME_INFO_EXT: {
            if (data == NULL || !LIBRETRO.core.gameInfoExtValid) return false;
            *(const struct retro_game_info_ext **)data = &LIBRETRO.core.gameInfoExt;
            return true;
        }

        case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2: {
            if (data == NULL) {
                return false;
            }
            const struct retro_core_options_v2 *opts = (const struct retro_core_options_v2 *)data;
            if (opts->definitions == NULL) {
                return false;
            }
            LIBRETRO.core.variableOptionsVersion = 2;

            // Store category metadata for BuildLibretroMenuOptions to use
            LIBRETRO.core.categoryCount = 0;
            if (opts->categories) {
                const struct retro_core_option_v2_category *cat = opts->categories;
                for (; cat->key != NULL && LIBRETRO.core.categoryCount < LIBRETRO_MAX_CORE_CATEGORIES; cat++) {
                    TextCopy(LIBRETRO.core.categoryKeys[LIBRETRO.core.categoryCount],   cat->key);
                    TextCopy(LIBRETRO.core.categoryLabels[LIBRETRO.core.categoryCount], cat->desc ? cat->desc : cat->key);
                    LIBRETRO.core.categoryCount++;
                }
            }

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
                    label, valuesList, displayList, tooltip,
                    (def->category_key && def->category_key[0]) ? def->category_key : "");
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
            return CallLibretroEnvironment(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2, (void *)intl->us);
        }

        case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_UPDATE_DISPLAY_CALLBACK: {
            const struct retro_core_options_update_display_callback *cb =
                (const struct retro_core_options_update_display_callback *)data;
            LIBRETRO.core.options_update_display_callback = (cb && cb->callback) ? cb->callback : NULL;
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
            struct retro_throttle_state* state = (struct retro_throttle_state*)data;
            if (state == NULL) {
                return false;
            }
            if (LIBRETRO.speed > 1.0f) {
                state->mode = RETRO_THROTTLE_FAST_FORWARD;
                state->rate = LIBRETRO.core.fps * LIBRETRO.speed;
            } else if (LIBRETRO.speed < 1.0f) {
                state->mode = RETRO_THROTTLE_SLOW_MOTION;
                state->rate = LIBRETRO.core.fps * LIBRETRO.speed;
            } else {
                state->mode = RETRO_THROTTLE_NONE;
                state->rate = (float)LIBRETRO.core.fps;
            }
            return true;
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
            *dir = GetLibretroDirectory(RETRO_ENVIRONMENT_GET_PLAYLIST_DIRECTORY);
            return true;
        }

        case RETRO_ENVIRONMENT_GET_FILE_BROWSER_START_DIRECTORY: {
            if (data == NULL) {
                TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_GET_FILE_BROWSER_START_DIRECTORY no data provided");
                return false;
            }
            const char** dir = (const char**)data;
            *dir = GetLibretroDirectory(RETRO_ENVIRONMENT_GET_FILE_BROWSER_START_DIRECTORY);
            return true;
        }

        case RETRO_ENVIRONMENT_GET_TARGET_SAMPLE_RATE: {
            if (data == NULL) {
                TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_GET_TARGET_SAMPLE_RATE no data provided");
                return false;
            }
            // Report the frontend's preferred output rate so the core can
            // pick a matching resampler target. This is *not* the core's own
            // sample rate (that flows from retro_get_system_av_info).
            unsigned *sampleRate = (unsigned *)data;
            *sampleRate = 48000;
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
static bool LibretroGetAudioVideo(void) {
    if (LIBRETRO.core.symbols.retro_get_system_av_info == NULL) {
        return false;
    }

    struct retro_system_av_info av = {0};
    LIBRETRO.core.symbols.retro_get_system_av_info(&av);
    LIBRETRO.core.width = av.geometry.base_width;
    LIBRETRO.core.height = av.geometry.base_height;
    LIBRETRO.core.fps = av.timing.fps;
    LIBRETRO.core.sampleRate = av.timing.sample_rate;
    LIBRETRO.core.aspectRatio = av.geometry.aspect_ratio;
    TraceLog(LOG_INFO, "LIBRETRO: Video and Audio information");
    TraceLog(LOG_INFO, "    > Display size: %i x %i", LIBRETRO.core.width, LIBRETRO.core.height);
    TraceLog(LOG_INFO, "    > Target FPS:   %.3f", LIBRETRO.core.fps);
    TraceLog(LOG_INFO, "    > Sample rate:  %.2f Hz", LIBRETRO.core.sampleRate);
    return true;
}

/**
 * Discard any pending time-accumulator backlog so the next UpdateLibretro()
 * starts timing from a clean slate. Call this after any wall-clock discontinuity
 * that is not real emulation time — game load, save-state load, rewind — so the
 * accumulator doesn't spend the gap as a burst of catch-up frames.
 */
static void ResetLibretroTiming(void) {
    LIBRETRO.speedAccumulator = 0.0;
    LIBRETRO.core.runloop_frame_time_last = 0;
}

/**
 * Run a single emulation tick: report audio buffer occupancy to the core, call
 * retro_run(), then flush any single-sample audio left over from the frame.
 */
static void LibretroTick(void) {
    // Report audio buffer occupancy to the core just before retro_run, so
    // it can decide whether to frameskip to avoid under-runs.
    if (LIBRETRO.core.audio_buffer_status_callback.callback && LIBRETRO.core.audioRingBufferSize > 0) {
        unsigned occupancy = (unsigned)((LIBRETRO.core.audioRingAvailable * 100) / LIBRETRO.core.audioRingBufferSize);
        if (occupancy > 100) occupancy = 100;
        bool underrun_likely = occupancy < 25;
        LIBRETRO.core.audio_buffer_status_callback.callback(true, occupancy, underrun_likely);
    }

    LIBRETRO.core.symbols.retro_run();

    // Flush any single-sample accumulator left over from retro_run so
    // samples arrive in the ring buffer within the same frame.
    if (LIBRETRO.core.singleSampleCount > 0) {
        UpdateLibretroAudioSampleBatch(LIBRETRO.core.singleSampleBuffer, LIBRETRO.core.singleSampleCount);
        LIBRETRO.core.singleSampleCount = 0;
    }
}

/**
 * Extended function for UpdateLibretro().
 *
 * @param onlyTick When enabled, will only step the audio and game, without updating any of the inputs.
 *
 * @see UpdateLibretro()
 */
static void UpdateLibretroEx(bool onlyTick) {
    if (!IsLibretroGameReady()) {
        return;
    }

    if (!onlyTick) {
        UpdateLibretro();
        return;
    }

    // Ensure the texture is updated.
    if (LIBRETRO.core.textureRebuild) {
        InitLibretroVideo();
    }

    LibretroTick();
}

/**
 * Run one emulation frame of the loaded core.
 */
static void UpdateLibretro(void) {
    if (LIBRETRO.core.textureRebuild) {
        InitLibretroVideo();
    }

    if (IsLibretroGameReady()) {
        // Real wall-clock time for RETRO_ENVIRONMENT_GET_PERF_INTERFACE. This is
        // intentionally NOT scaled by LIBRETRO.speed: the perf counter reports
        // actual elapsed time, independent of fast-forward / slow-motion.
        LIBRETRO.core.gameTimeNSEC += (retro_perf_tick_t)((double)GetFrameTime() * 1000000000.0);
    }

    // Update the game loop timer.
    if (LIBRETRO.core.runloop_frame_time.callback) {
        retro_time_t current = GetLibretroTimeUSEC();
        retro_time_t delta = current - LIBRETRO.core.runloop_frame_time_last;

        if (!LIBRETRO.core.runloop_frame_time_last) {
            delta = LIBRETRO.core.runloop_frame_time.reference;
        }
        LIBRETRO.core.runloop_frame_time_last = current;
        LIBRETRO.core.runloop_frame_time.callback(delta);
    }

    // Ask the core to emit the audio.
    if (LIBRETRO.core.audio_callback.callback) {
        LIBRETRO.core.audio_callback.callback();
    }

    // Check keyboard event callback.
    if (LIBRETRO.core.keyboard_event != NULL) {
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
        // KEY_KB_MENU (348) is the highest raylib key code.
        int keyCharMap[350] = {0};
        int pressedKey;
        while ((pressedKey = GetKeyPressed()) != 0) {
            int pressedChar = GetCharPressed();
            if (pressedKey < 350) {
                keyCharMap[pressedKey] = pressedChar;
            }
        }

        // Check each keyboard key
        for (int key = RETROK_FIRST; key < RETROK_LAST; key++) {
            int raylibKey = LibretroRetroKeyToKeyboardKey(key);
            if (raylibKey > 0) {
                if (IsKeyPressed(raylibKey)) {
                    LIBRETRO.core.keyboard_event(true, key, (uint32_t)keyCharMap[raylibKey], key_modifiers);
                }
                else if (IsKeyReleased(raylibKey)) {
                    LIBRETRO.core.keyboard_event(false, key, 0, key_modifiers);
                }
            }
        }
    }

    if (IsLibretroGameReady()) {
        double framePeriod = (LIBRETRO.core.fps > 0.0) ? (1.0 / LIBRETRO.core.fps) : (1.0 / 60.0);
        double frameTime = (double)GetFrameTime();

        // When at normal speed and the loop is already being presented at very
        // close to the core's frame rate (e.g. a vsync'd 60 Hz display with a
        // ~60 fps core), lock to exactly one tick per displayed frame. This
        // avoids the beat-frequency judder you get when the accumulator
        // occasionally emits 0 or 2 ticks in a frame; Dynamic Rate Control below
        // resamples the small audio-rate difference. Gating on the *measured*
        // frame time (not the monitor refresh) keeps this from running the core
        // unbounded when vsync is off / FPS is uncapped.
        double cadence = (framePeriod > 0.0) ? (frameTime / framePeriod) : 0.0;
        bool lockToFrame = (LIBRETRO.speed == 1.0f && cadence > 0.9 && cadence < 1.1);

        if (lockToFrame) {
            LIBRETRO.speedAccumulator = 0.0;
            LibretroTick();
        } else {
            LIBRETRO.speedAccumulator += frameTime * (double)LIBRETRO.speed;

            // Cap iterations to avoid spiral of death on slow hardware.
            int maxTicks = (int)(LIBRETRO.speed + 1.0f);
            if (maxTicks < 1) maxTicks = 1;

            // Clamp the accumulator so a GetFrameTime() spike (game load, window
            // drag, menu pause) can't leave a backlog that runs the core fast for
            // the following frames.
            double maxAccumulator = framePeriod * (double)maxTicks;
            if (LIBRETRO.speedAccumulator > maxAccumulator) {
                LIBRETRO.speedAccumulator = maxAccumulator;
            }

            while (LIBRETRO.speedAccumulator >= framePeriod && maxTicks-- > 0) {
                LIBRETRO.speedAccumulator -= framePeriod;
                LibretroTick();
            }
        }

        // Audio playback rate. The stream pitch tracks LIBRETRO.speed so the
        // device consumes the ring buffer at the same rate the core fills it:
        // at slow-motion the audio stretches out (lower pitch) instead of
        // underrunning into choppy gaps, and at fast-forward it consumes faster
        // instead of overrunning. Dynamic Rate Control rides on top as a small
        // multiplier that nudges ring-buffer occupancy toward 50% to stay in sync.
        if (LIBRETRO.core.audioRingBufferSize > 0 && IsAudioStreamValid(LIBRETRO.core.audioStream)) {
            if (LIBRETRO.core.drcEnabled) {
                // Positive drift when buffer is full (consume faster).
                // Negative drift when buffer is empty (consume slower).
                float drift = ((float)LIBRETRO.core.audioRingAvailable / (float)LIBRETRO.core.audioRingBufferSize) - 0.5f;
                LIBRETRO.core.drcAdjustment += drift * 0.001f;
                if (LIBRETRO.core.drcAdjustment < 0.995f) LIBRETRO.core.drcAdjustment = 0.995f;
                if (LIBRETRO.core.drcAdjustment > 1.005f) LIBRETRO.core.drcAdjustment = 1.005f;
            }
            SetAudioStreamPitch(LIBRETRO.core.audioStream, LIBRETRO.speed * LIBRETRO.core.drcAdjustment);
        }

        // Some cores (notably mgba GB/GBC) defer the actual core reset until
        // the first retro_run, so retro_get_system_av_info called pre-run
        // returns sample_rate=0. Once we have a frame, re-query AV info and
        // reinit audio if a valid sample rate has appeared.
        if (LIBRETRO.core.sampleRate <= 0.0 && LIBRETRO.core.symbols.retro_get_system_av_info) {
            struct retro_system_av_info av = {0};
            LIBRETRO.core.symbols.retro_get_system_av_info(&av);
            if (av.timing.sample_rate > 0.0) {
                LIBRETRO.core.sampleRate = av.timing.sample_rate;
                TraceLog(LOG_INFO, "LIBRETRO: Sample rate now %.2f Hz (post-retro_run); reinitializing audio", LIBRETRO.core.sampleRate);
                InitLibretroAudio();
            }
        }

        if (LIBRETRO.core.options_update_display_callback) {
            if (LIBRETRO.core.options_update_display_callback()) {
                LIBRETRO.core.variablesVisibilityDirty = true;
            }
        }
    }
}

/**
 * Check whether a core has been successfully initialized.
 * @return true if a core is loaded and ready. */
static bool IsLibretroReady(void) {
    return LIBRETRO.core.symbols.handle != NULL;
}

/**
 * Check whether the core has requested the frontend to shut down.
 * @return true if the core wants to close.
 */
static bool LibretroShouldClose(void) {
    return LIBRETRO.core.shutdown;
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
            uint32_t xrgb = input[w];
            output[w] = ((xrgb >> 16) & 0xFF)   // R = byte 0
                      | (xrgb & 0xFF00)         // G = byte 1
                      | ((xrgb & 0xFF) << 16)   // B = byte 2
                      | 0xFF000000;             // A = 0xFF
       }
    }
}

/**
 * Called when the core is updating the video.
 */
static void LibretroVideoRefresh(const void *data, unsigned width, unsigned height, size_t pitch) {
    // Only act when there is usable pixel data.
    if (!data) {
        return;
    }

    // Resize the video if needed.
    if (width != LIBRETRO.core.width || height != LIBRETRO.core.height) {
        LIBRETRO.core.width = width;
        LIBRETRO.core.height = height;
        if (!InitLibretroVideo()) {
            return;
        }
    }

    if (!IsTextureValid(LIBRETRO.core.texture)) {
        return;
    }

    switch (LIBRETRO.core.pixelFormat) {
        case RETRO_PIXEL_FORMAT_UNKNOWN:
        case RETRO_PIXEL_FORMAT_RGB565: {
            // For small pitches, we can use the data 1:1, otherwise we need to adjust it.
            if (pitch == width * 2) {
                // Core: FCEUM
                UpdateTexture(LIBRETRO.core.texture, data);
            }
            else {
                // Core: SNES9x
                const uint8_t *src = (const uint8_t *)data;
                uint8_t *dst = (uint8_t *)LIBRETRO.core.frameBuffer;
                size_t row_bytes = width * 2;
                for (unsigned h = 0; h < height; h++) {
                    memcpy(dst, src, row_bytes);
                    src += pitch;
                    dst += row_bytes;
                }
                UpdateTexture(LIBRETRO.core.texture, LIBRETRO.core.frameBuffer);
            }
        }
        break;
        case RETRO_PIXEL_FORMAT_0RGB1555: {
            LibretroPixelFormatARGB1555ToRGB565(LIBRETRO.core.frameBuffer, data, width, height,
                (int)(width * 2),
                pitch);
            UpdateTexture(LIBRETRO.core.texture, LIBRETRO.core.frameBuffer);
        }
        break;
        case RETRO_PIXEL_FORMAT_XRGB8888: {
            // Core: Blastem
            // Core: BSNES
            LibretroMapPixelFormatARGB8888ToRGBA8888(LIBRETRO.core.frameBuffer, data,
                width, height,
                (int)(width * 4), pitch);
            UpdateTexture(LIBRETRO.core.texture, LIBRETRO.core.frameBuffer);
        }
        break;
    }
}

static void LibretroInputPoll(void) {
    // Mouse
    LIBRETRO.core.inputLastMousePosition = LIBRETRO.core.inputMousePosition;
    LIBRETRO.core.inputMousePosition = GetMousePosition();
}

static int16_t LibretroInputState(unsigned port, unsigned device, unsigned index, unsigned id) {
    switch (device) {
        case RETRO_DEVICE_KEYBOARD: {
            int raylibKey = LibretroRetroKeyToKeyboardKey(id);
            if (raylibKey > 0) {
                return (int)IsKeyDown(raylibKey);
            }
            return 0;
        }
        case RETRO_DEVICE_JOYPAD_MULTITAP:
        case RETRO_DEVICE_JOYPAD: {
            // Multitap maps index (0-3) to a sub-controller: physical pad = port*4 + index.
            bool isMultitap = (device == RETRO_DEVICE_JOYPAD_MULTITAP)
                              || (LIBRETRO.core.portDeviceMap[port] == RETRO_DEVICE_JOYPAD_MULTITAP);
            unsigned physicalPad = isMultitap ? (port * 4 + index) : port;

            // Return a bitmask of all buttons when requested.
            if (id == RETRO_DEVICE_ID_JOYPAD_MASK) {
                int16_t mask = 0;
                bool gpAvail = IsGamepadAvailable((int)physicalPad);
                for (int btn = 0; btn < 16; btn++) {
                    bool pressed = false;
                    if (physicalPad == 0) {
                        if (btn < 16 && LIBRETRO.core.virtualJoypadState[btn]) {
                            pressed = true;
                        } else if (gpAvail) {
                            int gamepadButton = LibretroRetroJoypadButtonToGamepadButton(btn);
                            pressed = (gamepadButton != GAMEPAD_BUTTON_UNKNOWN && IsGamepadButtonDown(0, gamepadButton));
                        }
                        if (!pressed) {
                            int kbKey = (btn < 16) ? LIBRETRO.keyboardPlayer1[btn] : KEY_NULL;
                            if (kbKey != KEY_NULL) {
                                pressed = IsKeyDown(kbKey);
                            } else {
                                int retroKey = LibretroRetroJoypadButtontoRetroKey(btn);
                                if (retroKey != RETROK_UNKNOWN) {
                                    int raylibKey = LibretroRetroKeyToKeyboardKey(retroKey);
                                    pressed = (raylibKey > 0 && IsKeyDown(raylibKey));
                                }
                            }
                        }
                    } else if (gpAvail) {
                        pressed = IsGamepadButtonDown((int)physicalPad, LibretroRetroJoypadButtonToGamepadButton(btn));
                    }
                    if (!pressed) pressed = LibretroAnalogToDpadPressed((int)physicalPad, btn);
                    if (pressed) mask |= (1 << btn);
                }
                return mask;
            }

            // Physical pad 0: virtual joypad (touch) then gamepad then keyboard.
            if (physicalPad == 0) {
                if (id < 16 && LIBRETRO.core.virtualJoypadState[id]) {
                    return 1;
                }
                if (IsGamepadAvailable(0)) {
                    int gamepadButton = LibretroRetroJoypadButtonToGamepadButton(id);
                    if (gamepadButton != GAMEPAD_BUTTON_UNKNOWN && IsGamepadButtonDown(0, gamepadButton)) {
                        return 1;
                    }
                }
                int kbKey = (id < 16) ? LIBRETRO.keyboardPlayer1[id] : KEY_NULL;
                if (kbKey != KEY_NULL) {
                    return (int)IsKeyDown(kbKey);
                }
                int retroKey = LibretroRetroJoypadButtontoRetroKey(id);
                if (retroKey == RETROK_UNKNOWN) {
                    return LibretroAnalogToDpadPressed(0, id) ? 1 : 0;
                }
                int raylibKey = LibretroRetroKeyToKeyboardKey(retroKey);
                if (raylibKey > 0 && IsKeyDown(raylibKey)) return 1;
                return LibretroAnalogToDpadPressed(0, id) ? 1 : 0;
            }

            // Physical pad 1+: map directly to that gamepad index.
            if (!IsGamepadAvailable((int)physicalPad)) {
                return LibretroAnalogToDpadPressed((int)physicalPad, id) ? 1 : 0;
            }
            int gamepadButton = LibretroRetroJoypadButtonToGamepadButton(id);
            if (IsGamepadButtonDown((int)physicalPad, gamepadButton)) return 1;
            return LibretroAnalogToDpadPressed((int)physicalPad, id) ? 1 : 0;
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
                    return LIBRETRO.core.inputMousePosition.x - LIBRETRO.core.inputLastMousePosition.x;
                case RETRO_DEVICE_ID_MOUSE_Y:
                    return LIBRETRO.core.inputMousePosition.y - LIBRETRO.core.inputLastMousePosition.y;
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

        case RETRO_DEVICE_LIGHTGUN: {
            switch (id) {
                case RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X:
                    return (int16_t)(((float)GetMouseX() / (float)GetScreenWidth()) * 2.0f - 1.0f) * 0x7fff;
                case RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y:
                    return (int16_t)(((float)GetMouseY() / (float)GetScreenHeight()) * 2.0f - 1.0f) * 0x7fff;
                case RETRO_DEVICE_ID_LIGHTGUN_IS_OFFSCREEN: {
                    int mx = GetMouseX(), my = GetMouseY();
                    return (mx < 0 || my < 0 || mx >= GetScreenWidth() || my >= GetScreenHeight()) ? 1 : 0;
                }
                case RETRO_DEVICE_ID_LIGHTGUN_TRIGGER:
                    return IsMouseButtonDown(MOUSE_LEFT_BUTTON);
                case RETRO_DEVICE_ID_LIGHTGUN_RELOAD:
                    return IsMouseButtonDown(MOUSE_RIGHT_BUTTON);
                case RETRO_DEVICE_ID_LIGHTGUN_AUX_A:
                    return IsMouseButtonDown(MOUSE_MIDDLE_BUTTON);
                case RETRO_DEVICE_ID_LIGHTGUN_AUX_B:
                    return IsMouseButtonDown(MOUSE_BUTTON_SIDE);
                case RETRO_DEVICE_ID_LIGHTGUN_AUX_C:
                    return IsMouseButtonDown(MOUSE_BUTTON_EXTRA);
                case RETRO_DEVICE_ID_LIGHTGUN_START:
                    return IsKeyDown(KEY_ENTER);
                case RETRO_DEVICE_ID_LIGHTGUN_SELECT:
                    return IsKeyDown(KEY_RIGHT_SHIFT);
                case RETRO_DEVICE_ID_LIGHTGUN_DPAD_UP:
                    return IsKeyDown(KEY_UP);
                case RETRO_DEVICE_ID_LIGHTGUN_DPAD_DOWN:
                    return IsKeyDown(KEY_DOWN);
                case RETRO_DEVICE_ID_LIGHTGUN_DPAD_LEFT:
                    return IsKeyDown(KEY_LEFT);
                case RETRO_DEVICE_ID_LIGHTGUN_DPAD_RIGHT:
                    return IsKeyDown(KEY_RIGHT);
                case RETRO_DEVICE_ID_LIGHTGUN_X:
                    return LIBRETRO.core.inputMousePosition.x - LIBRETRO.core.inputLastMousePosition.x;
                case RETRO_DEVICE_ID_LIGHTGUN_Y:
                    return LIBRETRO.core.inputMousePosition.y - LIBRETRO.core.inputLastMousePosition.y;
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
                    int gamepadButton = LibretroRetroJoypadButtonToGamepadButton(id);
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

    if (!LIBRETRO.core.audioRingBuffer || LIBRETRO.core.audioRingAvailable == 0) {
        memset(output, 0, frameCount * 2 * sizeof(float));
        return;
    }

    size_t frames_to_read = frameCount;
    if (frames_to_read > LIBRETRO.core.audioRingAvailable) {
        size_t silence_frames = frames_to_read - LIBRETRO.core.audioRingAvailable;
        memset(output + LIBRETRO.core.audioRingAvailable * 2, 0, silence_frames * 2 * sizeof(float));
        frames_to_read = LIBRETRO.core.audioRingAvailable;
    }

    size_t read_pos = (LIBRETRO.core.audioRingWritePos + LIBRETRO.core.audioRingBufferSize - LIBRETRO.core.audioRingAvailable) % LIBRETRO.core.audioRingBufferSize;
    size_t first_chunk = LIBRETRO.core.audioRingBufferSize - read_pos;
    if (first_chunk > frames_to_read) first_chunk = frames_to_read;
    size_t second_chunk = frames_to_read - first_chunk;
    memcpy(output, LIBRETRO.core.audioRingBuffer + read_pos * 2, first_chunk * 2 * sizeof(float));
    if (second_chunk > 0) {
        memcpy(output + first_chunk * 2, LIBRETRO.core.audioRingBuffer, second_chunk * 2 * sizeof(float));
    }

    LIBRETRO.core.audioRingAvailable -= frames_to_read;
}

/**
 * Write a batch of int16_t stereo frames into the ring buffer.
 */
static size_t UpdateLibretroAudioSampleBatch(const int16_t *data, size_t frames) {
    if (!data || frames == 0 || !LIBRETRO.core.audioRingBuffer) return 0;

    size_t available_space = LIBRETRO.core.audioRingBufferSize - LIBRETRO.core.audioRingAvailable;
    size_t frames_to_write = (frames < available_space) ? frames : available_space;

    if (frames_to_write == 0) {
        if (LIBRETRO.core.audioDropWarnCount++ < 3) {
            TraceLog(LOG_WARNING, "LIBRETRO: Audio ring buffer full, dropping %zu frames", frames);
        }
        return 0;
    }

    static const float scale = 1.0f / 32768.0f;
    size_t wfirst = LIBRETRO.core.audioRingBufferSize - LIBRETRO.core.audioRingWritePos;
    if (wfirst > frames_to_write) wfirst = frames_to_write;
    size_t wsecond = frames_to_write - wfirst;
    for (size_t i = 0; i < wfirst; i++) {
        LIBRETRO.core.audioRingBuffer[(LIBRETRO.core.audioRingWritePos + i) * 2]     = (float)data[i * 2]     * scale;
        LIBRETRO.core.audioRingBuffer[(LIBRETRO.core.audioRingWritePos + i) * 2 + 1] = (float)data[i * 2 + 1] * scale;
    }
    for (size_t i = 0; i < wsecond; i++) {
        LIBRETRO.core.audioRingBuffer[i * 2]     = (float)data[(wfirst + i) * 2]     * scale;
        LIBRETRO.core.audioRingBuffer[i * 2 + 1] = (float)data[(wfirst + i) * 2 + 1] * scale;
    }

    LIBRETRO.core.audioRingWritePos = (LIBRETRO.core.audioRingWritePos + frames_to_write) % LIBRETRO.core.audioRingBufferSize;
    LIBRETRO.core.audioRingAvailable += frames_to_write;

    return frames_to_write;
}

/**
 * Accumulate single-sample callbacks into a buffer before flushing to the ring buffer.
 */
static void UpdateLibretroAudioSample(int16_t left, int16_t right) {
    if (LIBRETRO.core.singleSampleCount >= LIBRETRO_AUDIO_SINGLE_SAMPLE_BUFFER_SIZE) {
        UpdateLibretroAudioSampleBatch(LIBRETRO.core.singleSampleBuffer, LIBRETRO.core.singleSampleCount);
        LIBRETRO.core.singleSampleCount = 0;
    }

    LIBRETRO.core.singleSampleBuffer[LIBRETRO.core.singleSampleCount * 2]     = left;
    LIBRETRO.core.singleSampleBuffer[LIBRETRO.core.singleSampleCount * 2 + 1] = right;
    LIBRETRO.core.singleSampleCount++;
}

static void CloseLibretroAudio(void) {
    // Unload the audiostream first.
    if (IsAudioStreamValid(LIBRETRO.core.audioStream)) {
        StopAudioStream(LIBRETRO.core.audioStream);
        UnloadAudioStream(LIBRETRO.core.audioStream);
        memset(&LIBRETRO.core.audioStream, 0, sizeof(LIBRETRO.core.audioStream));
    }
    if (LIBRETRO.core.audioRingBuffer != NULL) {
        MemFree(LIBRETRO.core.audioRingBuffer);
        LIBRETRO.core.audioRingBuffer = NULL;
    }
    LIBRETRO.core.audioRingAvailable = 0;
    LIBRETRO.core.audioRingWritePos = 0;
}

static void InitLibretroAudio(void) {
    CloseLibretroAudio();

    // Defer audio setup until we have a usable sample rate. Some cores
    // (mgba GB/GBC) defer their internal reset until the first retro_run,
    // so retro_get_system_av_info returns 0 here. UpdateLibretro will
    // re-call InitLibretroAudio after the first run once the rate appears.
    if (LIBRETRO.core.sampleRate <= 0.0) {
        TraceLog(LOG_INFO, "LIBRETRO: Deferring audio init (sample rate not yet known)");
        return;
    }

    // Allocate the ring buffer (stereo float samples). At minimum
    // LIBRETRO_AUDIO_RING_BUFFER_SIZE frames; grown to satisfy the core's
    // RETRO_ENVIRONMENT_SET_MINIMUM_AUDIO_LATENCY request if any.
    size_t frames = LIBRETRO_AUDIO_RING_BUFFER_SIZE;
    if (LIBRETRO.core.minimumAudioLatencyMs > 0) {
        size_t requested = (size_t)((LIBRETRO.core.minimumAudioLatencyMs / 1000.0) * LIBRETRO.core.sampleRate);
        if (requested > frames) frames = requested;
    }
    LIBRETRO.core.audioRingBufferSize = frames;
    LIBRETRO.core.audioRingBuffer = (float *)MemAlloc(LIBRETRO.core.audioRingBufferSize * 2 * sizeof(float));
    LIBRETRO.core.audioRingWritePos = 0;
    LIBRETRO.core.audioRingAvailable = 0;

    // Create the audio stream: 32-bit float, stereo, pulled via callback.
    int sampleSize = 32;
    int channels = 2;
    LIBRETRO.core.audioStream = LoadAudioStream((unsigned int)LIBRETRO.core.sampleRate, sampleSize, channels);
    SetAudioStreamCallback(LIBRETRO.core.audioStream, LibretroAudioStreamCallback);
    SetAudioStreamVolume(LIBRETRO.core.audioStream, LIBRETRO.volume);
    LIBRETRO.core.drcAdjustment = 1.0f;
    LIBRETRO.core.drcEnabled = true;
    LIBRETRO.core.audioDropWarnCount = 0;
    PlayAudioStream(LIBRETRO.core.audioStream);

    // Let the core know that the audio device has been initialized.
    if (LIBRETRO.core.audio_callback.set_state) {
        LIBRETRO.core.audio_callback.set_state(true);
    }

    TraceLog(LOG_INFO, "LIBRETRO: Audio stream initialized (%i Hz, %i-bit float, ring buffer %i frames)",
        (int)LIBRETRO.core.sampleRate, sampleSize, (int)LIBRETRO.core.audioRingBufferSize);
}

static bool InitLibretroAudioVideo(void) {
    LibretroGetAudioVideo();
    InitLibretroVideo();
    InitLibretroAudio();

    // A fresh game load is a wall-clock discontinuity (disk I/O, core init);
    // start the time accumulator clean so the first frame doesn't burst.
    ResetLibretroTiming();

    return true;
}

// Populate LIBRETRO.core.gameInfoExt from LIBRETRO.core.contentPath for
// RETRO_ENVIRONMENT_GET_GAME_INFO_EXT. Caller must set contentPath first.
// `data`/`size` are only valid while retro_load_game() is executing.
static void SetLibretroGameInfoExt(const void *data, size_t size) {
    memset(&LIBRETRO.core.gameInfoExt, 0, sizeof(LIBRETRO.core.gameInfoExt));
    LIBRETRO.core.contentDir[0]  = '\0';
    LIBRETRO.core.contentName[0] = '\0';
    LIBRETRO.core.contentExt[0]  = '\0';
    LIBRETRO.core.gameInfoExtValid = false;

    if (LIBRETRO.core.contentPath[0] == '\0') return;

    TextCopy(LIBRETRO.core.contentDir,  GetDirectoryPath(LIBRETRO.core.contentPath));
    TextCopy(LIBRETRO.core.contentName, GetFileNameWithoutExt(LIBRETRO.core.contentPath));

    // Extension without leading dot, lower-cased.
    const char *ext = GetFileExtension(LIBRETRO.core.contentPath);
    if (ext != NULL && ext[0] == '.') ext++;
    if (ext != NULL) {
        size_t i = 0;
        for (; ext[i] != '\0' && i + 1 < sizeof(LIBRETRO.core.contentExt); i++) {
            char c = ext[i];
            if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
            LIBRETRO.core.contentExt[i] = c;
        }
        LIBRETRO.core.contentExt[i] = '\0';
    }

    LIBRETRO.core.gameInfoExt.full_path       = LIBRETRO.core.contentPath;
    LIBRETRO.core.gameInfoExt.dir             = LIBRETRO.core.contentDir;
    LIBRETRO.core.gameInfoExt.name            = LIBRETRO.core.contentName;
    LIBRETRO.core.gameInfoExt.ext             = LIBRETRO.core.contentExt;
    LIBRETRO.core.gameInfoExt.meta            = "";
    LIBRETRO.core.gameInfoExt.data            = data;
    LIBRETRO.core.gameInfoExt.size            = size;
    LIBRETRO.core.gameInfoExt.file_in_archive = false;
    LIBRETRO.core.gameInfoExt.persistent_data = false;
    LIBRETRO.core.gameInfoExtValid            = true;
}

static bool LoadLibretroGameFromMemory(const unsigned char *fileData, int dataSize) {
    if (!IsLibretroReady()) {
        TraceLog(LOG_ERROR, "LIBRETRO: Core is required before loading a game");
        return false;
    }

    if (fileData == NULL) {
        return LoadLibretroGame(NULL);
    }

    // Check if it needs the full path
    if (LIBRETRO.core.needFullpath) {
        TraceLog(LOG_ERROR, "LIBRETRO: The core requires the full path, can't load from memory");
        // TODO: Allow saving to a temporary location and then use LoadLibretroGame()?
        return false;
    }

    // Load the game.
    struct retro_game_info info;
    info.path = NULL;
    info.data = fileData;
    info.size = (size_t)dataSize;
    info.meta = "";
    SetLibretroGameInfoExt(fileData, (size_t)dataSize);
    if (!LIBRETRO.core.symbols.retro_load_game(&info)) {
        TraceLog(LOG_ERROR, "LIBRETRO: Failed to load game data with retro_load_game()");
        LIBRETRO.core.loaded = false;
        return false;
    }

    LIBRETRO.core.loaded = InitLibretroAudioVideo();
    if (!LIBRETRO.core.loaded) {
        return false;
    }
    TraceLog(LOG_INFO, "LIBRETRO: Loaded content from memory");
    return true;
}

/**
 * Convert a pipe-separated extension list to a semicolon-separated pattern for IsFileExtension.
 * @param exts        Source string in "ext1|ext2" format.
 * @param pattern     Output buffer for the ".ext1;.ext2" result.
 * @param patternSize Size of the output buffer in bytes. */
static void GetLibretroFileExtensionPattern(const char *exts, char *pattern, size_t patternSize) {
    if (patternSize == 0) return;
    size_t p = 0;
    if (p + 1 < patternSize) pattern[p++] = '.';
    for (const char *v = exts; *v && p + 1 < patternSize; v++) {
        if (*v == '|') {
            pattern[p++] = ';';
            if (p + 1 < patternSize) pattern[p++] = '.';
        } else {
            pattern[p++] = *v;
        }
    }
    pattern[p] = '\0';
}

/**
 * Retrieve whether or not the libretro core requires a full path for a given content file.
 *
 * This honors RETRO_ENVIRONMENT_SET_CONTENT_INFO_OVERRIDE calls.
 *
 * @param path       Content path to inspect. May be NULL, in which case the
 *                   global retro_system_info::need_fullpath value is
 *                   returned.
 * @param persistent Optional out-param set to the matching override's
 *                   persistent_data flag, or \c false when no override
 *                   matched. May be NULL.
 * @return The effective need_fullpath for the given path.
 */
static bool GetLibretroNeedFullpath(const char *path, bool *persistent) {
    if (persistent) *persistent = false;
    if (path == NULL) return LIBRETRO.core.needFullpath;

    for (unsigned i = 0; i < LIBRETRO.core.contentInfoOverrideCount; i++) {
        char pattern[LIBRETRO_CONTENT_INFO_OVERRIDE_EXTS_LEN + 16];
        GetLibretroFileExtensionPattern(LIBRETRO.core.contentInfoOverrideExts[i], pattern, sizeof(pattern));
        if (IsFileExtension(path, pattern)) {
            if (persistent) *persistent = LIBRETRO.core.contentInfoOverridePersistent[i];
            return LIBRETRO.core.contentInfoOverrideNeedFullpath[i];
        }
    }
    return LIBRETRO.core.needFullpath;
}

/**
 * Load game data into the core, with full control over the path stored on
 * LIBRETRO.core.contentPath and over persistent_data ownership.
 *
 * When @p persistent is true (typically from a CONTENT_INFO_OVERRIDE
 * declaring persistent_data), ownership of @p fileData transfers to
 * LIBRETRO.core.persistentGameData and the buffer is released by
 * UnloadLibretroGame(). When false, the caller retains ownership and may
 * free @p fileData immediately after this returns.
 *
 * @param fileData    ROM data. Must not be NULL.
 * @param dataSize    Size of @p fileData in bytes.
 * @param contentPath Optional path stored on LIBRETRO.core.contentPath and
 *                    surfaced via RETRO_ENVIRONMENT_GET_GAME_INFO_EXT. May
 *                    be NULL.
 * @param persistent  See RETRO_ENVIRONMENT_SET_CONTENT_INFO_OVERRIDE; when
 *                    \c true, the buffer is kept alive until retro_deinit().
 * @return \c true on success, \c false on failure.
 */
static bool LoadLibretroGameFromMemoryEx(unsigned char* fileData, int dataSize, const char* contentPath, bool persistent) {
    if (!IsLibretroReady()) {
        TraceLog(LOG_ERROR, "LIBRETRO: Core is required before loading a game");
        return false;
    }
    if (fileData == NULL) {
        return LoadLibretroGame(NULL);
    }

    if (contentPath != NULL && contentPath[0] != '\0') {
        TextCopy(LIBRETRO.core.contentPath, contentPath);
    } else {
        LIBRETRO.core.contentPath[0] = '\0';
    }

    struct retro_game_info info;
    info.path = (contentPath != NULL && contentPath[0] != '\0') ? contentPath : NULL;
    info.data = fileData;
    info.size = (size_t)dataSize;
    info.meta = "";
    SetLibretroGameInfoExt(fileData, (size_t)dataSize);
    if (!LIBRETRO.core.symbols.retro_load_game(&info)) {
        TraceLog(LOG_ERROR, "LIBRETRO: Failed to load game data with retro_load_game()");
        LIBRETRO.core.loaded = false;
        return false;
    }

    if (persistent) {
        if (LIBRETRO.core.persistentGameData != NULL && LIBRETRO.core.persistentGameData != fileData) {
            UnloadFileData(LIBRETRO.core.persistentGameData);
        }
        LIBRETRO.core.persistentGameData = fileData;
        LIBRETRO.core.persistentGameDataSize = dataSize;
    }

    LIBRETRO.core.loaded = InitLibretroAudioVideo();
    if (!LIBRETRO.core.loaded) return false;
    TraceLog(LOG_INFO, "LIBRETRO: Loaded content from memory");
    return true;
}

/**
 * Get the pipe-separated list of file extensions the core accepts.
 * @return Extension list string (e.g. "nes|fds"), or empty string if not reported.
 */
static const char* GetLibretroValidExtensions(void) {
    return LIBRETRO.core.validExtensions;
}

/**
 * Check whether the core forbids the frontend from extracting archives.
 * @return true if the core wants archives passed as-is.
 */
static bool IsLibretroBlockExtract(void) {
    return LIBRETRO.core.blockExtract;
}

/**
 * Load content (ROM/game) into the initialized core.
 * @param gameFile Path to the content file, or NULL for content-less cores.
 * @return true on success, false if loading failed.
 * @note Call InitLibretro() before this function. Pass NULL for cores that do not require content.
 */
static bool LoadLibretroGame(const char* gameFile) {
    if (!IsLibretroReady()) {
        TraceLog(LOG_ERROR, "LIBRETRO: Core is required before loading a game");
        return false;
    }

    if (IsLibretroGameReady()) {
        UnloadLibretroGame();
    }

    // Load empty game.
    if (gameFile == NULL) {
        struct retro_game_info info;
        info.data = NULL;
        info.size = 0;
        info.path = NULL;
        info.meta = "";
        LIBRETRO.core.contentPath[0] = '\0';
        SetLibretroGameInfoExt(NULL, 0);
        if (LIBRETRO.core.symbols.retro_load_game(&info)) {
            TraceLog(LOG_INFO, "LIBRETRO: Loaded without content");
            LIBRETRO.core.loaded = true;
            return InitLibretroAudioVideo();
        }
        TraceLog(LOG_ERROR, "LIBRETRO: Failed to load core without content");
        return false;
    }

    if (!FileExists(gameFile)) {
        TraceLog(LOG_ERROR, "LIBRETRO: Given content does not exist: %s", gameFile);
        LIBRETRO.core.loaded = false;
        return false;
    }

    // Resolve need_fullpath via CONTENT_INFO_OVERRIDE if the core set one.
    bool persistent = false;
    if (GetLibretroNeedFullpath(gameFile, &persistent)) {
        struct retro_game_info info;
        info.data = NULL;
        info.size = 0;
        info.path = gameFile;
        info.meta = "";
        TextCopy(LIBRETRO.core.contentPath, gameFile);
        SetLibretroGameInfoExt(NULL, 0);
        if (LIBRETRO.core.symbols.retro_load_game(&info)) {
            TraceLog(LOG_INFO, "LIBRETRO: Loaded content with full path: %s", gameFile);
            LIBRETRO.core.loaded = true;
            return InitLibretroAudioVideo();
        }
        TraceLog(LOG_ERROR, "LIBRETRO: Failed to load full path: %s", gameFile);
        LIBRETRO.core.loaded = false;
        LIBRETRO.core.contentPath[0] = '\0';
        LIBRETRO.core.gameInfoExtValid = false;
        return false;
    }

    int size = 0;
    unsigned char* gameData = LoadFileData(gameFile, &size);
    if (gameData == NULL || size == 0) {
        TraceLog(LOG_ERROR, "LIBRETRO: Failed to load game data with LoadFileData()");
        LIBRETRO.core.loaded = false;
        UnloadFileData(gameData);
        return false;
    }

    bool ok = LoadLibretroGameFromMemoryEx(gameData, size, gameFile, persistent);
    if (!ok || !persistent) {
        UnloadFileData(gameData);
    }
    return ok;
}

/**
 * Get the display name of the loaded core.
 * @return Library name string, or an empty string if no core is loaded. */
static const char* GetLibretroName(void) {
    return LIBRETRO.core.libraryName;
}

/**
 * Get the base filename (no extension) of the loaded content, or the core name if no content is loaded.
 * @return Content or core name string.
 */
static const char* GetLibretroContentName(void) {
    if (LIBRETRO.core.contentPath[0] != '\0') {
        return GetFileNameWithoutExt(LIBRETRO.core.contentPath);
    }
    return LIBRETRO.core.libraryName;
}

/**
 * Get the version string of the loaded core.
 * @return Version string reported by the core.
 */
static const char* GetLibretroVersion(void) {
    return LIBRETRO.core.libraryVersion;
}

/**
 * Check whether the loaded core requires content to run.
 * @return true if content must be provided; false if the core can run standalone.
 */
static bool IsLibretroGameRequired(void) {
    return !LIBRETRO.core.supportNoGame;
}

/**
 * Load a libretro core's dylib and populate its identity fields
 * (libraryName, libraryVersion, validExtensions, needFullpath, blockExtract)
 * via retro_get_system_info, without performing a full init.
 *
 * The dylib is left open so the caller can read identity fields off
 * LIBRETRO.core. The caller is responsible for calling CloseLibretro().
 *
 * @param core Path to the core shared library.
 * @return true on success.
 */
static bool PeekLibretroCoreInfo(const char* core) {
    // Avoid initializing twice.
    if (IsLibretroReady()) {
        TraceLog(LOG_INFO, "LIBRETRO: Core already loaded, use CloseLibretro()");
        return false;
    }

    // Ensure the core exists.
    if (!FileExists(core)) {
        TraceLog(LOG_ERROR, "LIBRETRO: Core file doesn't exist: %s", core);
        return false;
    }

    // Open the dynamic library.
    LIBRETRO.core.symbols.handle = dylib_load(core);
    if (!LIBRETRO.core.symbols.handle) {
        TraceLog(LOG_ERROR, "LIBRETRO: Failed to load provided library");
        return false;
    }

    // Find the libretro API version.
    LoadLibretroMethod(retro_api_version);
    LIBRETRO.core.apiVersion = LIBRETRO.core.symbols.retro_api_version();
    TraceLog(LOG_INFO, "LIBRETRO: API version: %i", LIBRETRO.core.apiVersion);
    if (LIBRETRO.core.apiVersion != 1) {
        CloseLibretro();
        TraceLog(LOG_ERROR, "LIBRETRO: Incompatible API version");
        return false;
    }

    // Retrieve the libretro core system information.
    LoadLibretroMethod(retro_get_system_info);
    struct retro_system_info systemInfo;
    LIBRETRO.core.symbols.retro_get_system_info(&systemInfo);
    TextCopy(LIBRETRO.core.libraryName, systemInfo.library_name);
    TextCopy(LIBRETRO.core.libraryVersion, systemInfo.library_version);
    TextCopy(LIBRETRO.core.validExtensions, systemInfo.valid_extensions);
    LIBRETRO.core.needFullpath = systemInfo.need_fullpath;
    LIBRETRO.core.blockExtract = systemInfo.block_extract;
    TraceLog(LOG_INFO, "LIBRETRO: Core loaded successfully");
    TraceLog(LOG_INFO, "    > Name:         %s", LIBRETRO.core.libraryName);
    TraceLog(LOG_INFO, "    > Version:      %s", LIBRETRO.core.libraryVersion);
    if (TextLength(LIBRETRO.core.validExtensions) > 0) {
        TraceLog(LOG_INFO, "    > Extensions:   %s", LIBRETRO.core.validExtensions);
    }
    return true;
}

/**
 * Load a libretro core shared library and initialize it.
 * @param core Path to the core .so/.dll/.dylib file.
 * @return true on success, false if the library could not be loaded or initialized.
 * @note Call InitAudioDevice() and InitWindow() before this function.
 */
static bool InitLibretro(const char* core) {
    if (!PeekLibretroCoreInfo(core)) {
        return false;
    }

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

    TextCopy(LIBRETRO.core.corePath, core);

    // Set up the callbacks.
    LIBRETRO.core.symbols.retro_set_video_refresh(LibretroVideoRefresh);
    LIBRETRO.core.symbols.retro_set_input_poll(LibretroInputPoll);
    LIBRETRO.core.symbols.retro_set_input_state(LibretroInputState);
    LIBRETRO.core.symbols.retro_set_audio_sample(UpdateLibretroAudioSample);
    LIBRETRO.core.symbols.retro_set_audio_sample_batch(UpdateLibretroAudioSampleBatch);
    LIBRETRO.core.symbols.retro_set_environment(CallLibretroEnvironment);

    // Initialize the core.
    LIBRETRO.core.symbols.retro_init();
    return true;
}

/**
 * Draw the core framebuffer at pixel coordinates with a color tint.
 * @param posX Horizontal screen position.
 * @param posY Vertical screen position.
 * @param tint Color tint applied to the framebuffer texture.
 *
 * @see DrawTexture()
 */
static void DrawLibretroTexture(int posX, int posY, Color tint) {
    Rectangle destRec = {(float)posX, (float)posY, (float)GetLibretroWidth(), (float)GetLibretroHeight()};
    DrawLibretroPro(destRec, tint);
}

/**
 * Draw the core framebuffer at the given position with a color tint.
 * @param position Top-left position in screen space.
 * @param tint Color tint applied to the framebuffer texture.
 */
static void DrawLibretroV(Vector2 position, Color tint) {
    DrawLibretroTexture((int)position.x, (int)position.y, tint);
}

/**
 * Draw the core framebuffer with extended transform parameters.
 * @param position Top-left origin in screen space.
 * @param rotation Clockwise rotation in degrees.
 * @param scale Uniform scale factor.
 * @param tint Color tint applied to the framebuffer texture.
 */
static void DrawLibretroEx(Vector2 position, float rotation, float scale, Color tint) {
    if (LIBRETRO.core.loaded == false) {
        return;
    }
    Rectangle source = {0, 0, (float)LIBRETRO.core.width, (float)LIBRETRO.core.height};
    Rectangle dest = {position.x, position.y, (float)LIBRETRO.core.width * scale, (float)LIBRETRO.core.height * scale};
    DrawTexturePro(LIBRETRO.core.texture, source, dest, (Vector2){0, 0}, rotation + (float)(LIBRETRO.core.rotation * 90), tint);
}

/**
 * Draw the core framebuffer scaled and fitted into a destination rectangle.
 * @param destRec Destination rectangle in screen space.
 * @param tint Color tint applied to the framebuffer texture.
 */
static void DrawLibretroPro(Rectangle destRec, Color tint) {
    if (LIBRETRO.core.loaded == false) {
        return;
    }
    float rotDeg = (float)(LIBRETRO.core.rotation * 90);
    bool swap = (LIBRETRO.core.rotation == 1 || LIBRETRO.core.rotation == 3);
    float destW = swap ? destRec.height : destRec.width;
    float destH = swap ? destRec.width : destRec.height;
    Rectangle source = {0, 0, (float)LIBRETRO.core.width, (float)LIBRETRO.core.height};
    Rectangle dest = {destRec.x + destRec.width / 2.0f, destRec.y + destRec.height / 2.0f, destW, destH};
    Vector2 origin = {destW / 2.0f, destH / 2.0f};
    DrawTexturePro(LIBRETRO.core.texture, source, dest, origin, rotDeg, tint);
}

/**
 * Draw the current OSD message if one is active.
 * @return true if a message was drawn; false if there was nothing to display.
 */
static bool DrawLibretroMessage(void) {
    if (GetTime() > LIBRETRO.osdEndTime) {
        return false;
    }

    const struct retro_message_ext *osd = &LIBRETRO.osd;
    // Nothing to render for an empty string; skip drawing an empty box.
    if (osd->msg[0] == '\0') {
        return false;
    }
    float fontSize = 20.0f;
    int padding = 8;
    // MeasureTextEx honors embedded newlines (multi-line OSD messages), unlike
    // the single-line MeasureText, so the background box wraps the whole block.
    Font font = GetFontDefault();
    float spacing = fontSize / 10.0f;
    Vector2 textSize = MeasureTextEx(font, osd->msg, fontSize, spacing);

    bool hasProgress = (osd->type == RETRO_MESSAGE_TYPE_PROGRESS);
    int barHeight = hasProgress ? 6 : 0;
    int barGap = hasProgress ? padding / 2 : 0;

    int textH = (int)textSize.y;
    int boxW = (int)textSize.x + padding * 2;
    int boxH = textH + padding * 2 + barHeight + barGap;
    int boxX = (GetScreenWidth() - boxW) / 2;
    // Status messages are persistent readouts, so pin them to the top, out of
    // the way of transient notifications anchored to the bottom.
    int boxY = (osd->type == RETRO_MESSAGE_TYPE_STATUS)
        ? padding
        : GetScreenHeight() - boxH - padding;

    // NOTIFICATION_ALT should read as visually distinct from a plain notice.
    Color bg = (osd->type == RETRO_MESSAGE_TYPE_NOTIFICATION_ALT)
        ? (Color){0, 40, 80, 200}
        : (Color){0, 0, 0, 180};
    DrawRectangle(boxX, boxY, boxW, boxH, bg);

    // Tint the text by severity so errors and warnings stand out.
    Color textColor = WHITE;
    if (osd->level == RETRO_LOG_ERROR) {
        textColor = (Color){255, 80, 80, 255};
    } else if (osd->level == RETRO_LOG_WARN) {
        textColor = (Color){255, 220, 90, 255};
    }
    Vector2 textPos = {(float)(boxX + padding), (float)(boxY + padding)};
    DrawTextEx(font, osd->msg, textPos, fontSize, spacing, textColor);

    if (hasProgress) {
        int barX = boxX + padding;
        int barY = boxY + padding + textH + barGap;
        int barW = boxW - padding * 2;
        DrawRectangle(barX, barY, barW, barHeight, (Color){80, 80, 80, 200});
        if (osd->progress >= 0) {
            // Determinate task: fill proportionally to 0-100. Clamp in case a
            // core reports past 100 (progress is an int8_t, range -128..127).
            int pct = osd->progress > 100 ? 100 : osd->progress;
            int fill = barW * pct / 100;
            DrawRectangle(barX, barY, fill, barHeight, WHITE);
        } else {
            // Indefinite task: a sweep that bounces left-right via a triangle
            // wave, so it eases at the edges instead of snapping back.
            int sweepW = barW / 4;
            int travel = barW - sweepW;
            float t = (float)(GetTime() - (double)(long)GetTime());  // 0..1
            float ping = t < 0.5f ? t * 2.0f : (1.0f - t) * 2.0f;    // 0->1->0
            int offset = travel > 0 ? (int)(ping * travel) : 0;
            DrawRectangle(barX + offset, barY, sweepW, barHeight, WHITE);
        }
    }
    return true;
}

/**
 * Draw the core framebuffer centered on the screen with a color tint.
 * @param tint Color tint applied to the framebuffer texture.
 */
static void DrawLibretroTint(Color tint) {
    if (LIBRETRO.core.loaded == false) {
        return;
    }

    bool swapDims = (LIBRETRO.core.rotation == 1 || LIBRETRO.core.rotation == 3);

    // Find the aspect ratio.
    float aspect = LIBRETRO.core.aspectRatio;
    if (aspect <= 0) {
        aspect = (float)LIBRETRO.core.width / (float)LIBRETRO.core.height;
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
    Rectangle source = {0, 0, LIBRETRO.core.width, LIBRETRO.core.height};
    Rectangle dest = {cx, cy, (float)destW, (float)destH};
    Vector2 origin = {destW / 2.0f, destH / 2.0f};
    DrawTexturePro(LIBRETRO.core.texture, source, dest, origin, (float)(LIBRETRO.core.rotation * 90), tint);
}

/**
 * Draw the core framebuffer centered on the screen.
 */
static void DrawLibretro(void) {
    DrawLibretroTint(WHITE);
}

/**
 * Displays an on-screen display (OSD) message with full control over its
 * appearance, mirroring the fields of RETRO_ENVIRONMENT_SET_MESSAGE_EXT.
 *
 * Honors type (notification/status/progress placement), priority (a lower-
 * priority message won't replace a higher-priority one that's still visible),
 * level (severity tint), progress (progress bar), and duration (milliseconds;
 * 0 falls back to a readable default). The target field is ignored — this
 * function only drives the OSD; callers wanting the log should TraceLog too.
 *
 * @param message The message descriptor.
 */
static void SetLibretroMessageEx(const struct retro_message_ext *message) {
    if (message == NULL || message->msg == NULL || message->msg[0] == '\0') {
        return;
    }

    // Don't let a lower-priority message stomp one that's still on-screen; an
    // expired message imposes no such constraint.
    bool active = GetTime() <= LIBRETRO.osdEndTime;
    if (active && message->priority < LIBRETRO.osd.priority) {
        return;
    }

    // A non-positive duration means "use the frontend default" rather than
    // "show for zero milliseconds", so clamp it to a readable default.
    double duration = (double)message->duration / 1000.0;
    if (duration <= 0.0) {
        duration = 3.0;
    }

    // Capture the full descriptor, then fix up the fields we can't take
    // verbatim: msg must point at our owned buffer.
    LIBRETRO.osd = *message;
    TextCopy(LIBRETRO.osdMessage, message->msg);
    LIBRETRO.osd.msg = LIBRETRO.osdMessage;
    LIBRETRO.osdEndTime = GetTime() + duration;

    // progress is only meaningful for progress-type messages; 0-100 is a real
    // percentage and -1 means indefinite.
    if (message->type != RETRO_MESSAGE_TYPE_PROGRESS || message->progress < 0) {
        LIBRETRO.osd.progress = -1;
    }
}

/**
 * Displays an on-screen display (OSD) message.
 * @param msg      Message text to display, or NULL to clear the current message.
 * @param duration How long to show the message, in seconds. */
static void SetLibretroMessage(const char* msg, double duration) {
    if (msg == NULL) {
        // Clear: reset to the lowest-priority notification defaults so the next
        // message of any priority can take over.
        LIBRETRO.osd = (struct retro_message_ext){0};
        LIBRETRO.osd.msg = LIBRETRO.osdMessage;
        LIBRETRO.osd.type = RETRO_MESSAGE_TYPE_NOTIFICATION;
        LIBRETRO.osd.progress = -1;
        LIBRETRO.osdMessage[0] = '\0';
        LIBRETRO.osdEndTime = 0;
        return;
    }
    // A transient notification at the lowest priority. SetLibretroMessageEx
    // clamps a non-positive duration to a readable default.
    struct retro_message_ext message = {0};
    message.msg = msg;
    message.duration = (unsigned)(duration * 1000.0 + 0.5);
    message.level = RETRO_LOG_INFO;
    message.target = RETRO_MESSAGE_TARGET_OSD;
    message.type = RETRO_MESSAGE_TYPE_NOTIFICATION;
    message.progress = -1;
    SetLibretroMessageEx(&message);
}

/**
 * Set the audio output volume.
 * @param volume Volume level in the range [0.0, 1.0].
 */
static void SetLibretroVolume(float volume) {
    if (volume < 0) volume = 0.0f;
    else if (volume > 1.0f) volume = 1.0f;
    LIBRETRO.volume = volume;
    if (IsAudioStreamValid(LIBRETRO.core.audioStream)) {
        SetAudioStreamVolume(LIBRETRO.core.audioStream, volume);
    }
}

/**
 * Get the current audio output volume.
 * @return Volume level in the range [0.0, 1.0]. */
static float GetLibretroVolume(void) {
    return LIBRETRO.volume;
}

/**
 * Set the emulation playback speed.
 * @param speed Multiplier relative to normal speed (1.0 = normal, >1.0 = fast-forward, <1.0 = slow-motion). */
static void SetLibretroSpeed(float speed) {
    if (speed <= 0.0f) speed = 0.1f;
    LIBRETRO.speed = speed;
}

/**
 * Get the current emulation playback speed multiplier.
 * @return Current speed multiplier. */
static float GetLibretroSpeed(void) {
    return LIBRETRO.speed;
}

/**
 * Enable or disable Dynamic Rate Control (DRC). When enabled, the audio
 * stream pitch is nudged up or down (within ±0.5 %) each frame to steer
 * ring-buffer occupancy toward 50 %, keeping audio and video in sync.
 *
 * DRC is always enabled by default on audio initialisation. It works
 * alongside RETRO_ENVIRONMENT_SET_AUDIO_BUFFER_STATUS_CALLBACK: that callback
 * lets the frontend inform the core of buffer occupancy so the core can
 * frameskip; DRC simultaneously adjusts pitch for long-term clock alignment.
 * The two mechanisms are complementary and do not conflict.
 *
 * @param enabled true to enable DRC, false to disable and restore unity pitch.
 */
static void SetLibretroDynamicRateControl(bool enabled) {
    LIBRETRO.core.drcEnabled = enabled;
    if (!enabled && IsAudioStreamValid(LIBRETRO.core.audioStream)) {
        LIBRETRO.core.drcAdjustment = 1.0f;
        SetAudioStreamPitch(LIBRETRO.core.audioStream, 1.0f);
    }
}

/**
 * Query whether Dynamic Rate Control is currently active.
 *
 * @return true if DRC is enabled, false otherwise.
 */
static bool IsLibretroDynamicRateControlEnabled(void) {
    return LIBRETRO.core.drcEnabled;
}

/**
 * Set the input device type for a controller port and notify the core.
 * Use RETRO_DEVICE_JOYPAD_MULTITAP to enable multitap on a port; the input_state
 * index then selects the sub-controller (0-3), mapping to physical gamepad port*4+index.
 * @param port Controller port (0-15).
 * @param device Device type (e.g. RETRO_DEVICE_JOYPAD, RETRO_DEVICE_JOYPAD_MULTITAP).
 * @return true on success, false if port is out of range.
 */
static bool SetLibretroPortDevice(unsigned port, unsigned device) {
    if (port >= 16) {
        return false;
    }
    LIBRETRO.core.portDeviceMap[port] = device;
    if (IsLibretroReady() && LIBRETRO.core.symbols.retro_set_controller_port_device != NULL) {
        LIBRETRO.core.symbols.retro_set_controller_port_device(port, device);
    }
    return true;
}

/**
 * Get the input device type currently assigned to a controller port.
 * @param port Controller port (0-15).
 * @return Device type, or RETRO_DEVICE_NONE if port is out of range.
 */
static unsigned GetLibretroPortDevice(unsigned port) {
    if (port >= 16) {
        return RETRO_DEVICE_NONE;
    }
    return LIBRETRO.core.portDeviceMap[port];
}

/**
 * Get the current display rotation index (0=0°, 1=90°, 2=180°, 3=270°).
 * @return Rotation index.
 */
static int GetLibretroRotation(void) {
    return LIBRETRO.core.rotation;
}

/**
 * Get the native framebuffer width reported by the core.
 * @return Width in pixels
 */
static unsigned GetLibretroWidth(void) {
    if (LIBRETRO.core.rotation == 1 || LIBRETRO.core.rotation == 3) {
        return LIBRETRO.core.height;
    }

    return LIBRETRO.core.width;
}

/**
 * Get the native framebuffer height reported by the core.
 * @return Height in pixels.
 */
static unsigned GetLibretroHeight(void) {
    if (LIBRETRO.core.rotation == 1 || LIBRETRO.core.rotation == 3) {
        return LIBRETRO.core.width;
    }
    return LIBRETRO.core.height;
}

static double GetLibretroFPS(void) {
    return LIBRETRO.core.fps > 0 ? LIBRETRO.core.fps : 60.0;
}

static float GetLibretroAspectRatio(void) {
    return LIBRETRO.core.aspectRatio;
}

/**
 * Get the internal texture used to render the core framebuffer.
 * @return Texture2D containing the last rendered frame.
 */
static Texture2D GetLibretroTexture(void) {
    return LIBRETRO.core.texture;
}

/**
 * Load an image of the current screen of the core framebuffer.
 *
 * The image must be called with UnloadImage() to clear the memory.
 *
 * @return Essentially a screenshot of the libretro game. Empty image otherwise.
 */
static Image LoadImageFromLibretro() {
    if (!IsLibretroGameReady() || LIBRETRO.core.frameBuffer == NULL || LIBRETRO.core.frameBufferSize == 0) {
        Image empty = {0};
        return empty;
    }

    Image image = {0};
    image.width = (int)LIBRETRO.core.width;
    image.height = (int)LIBRETRO.core.height;
    image.mipmaps = 1;
    image.format = LibretroRetroPixelFormatToPixelFormat(LIBRETRO.core.pixelFormat);

    // Copy the pre-converted frame buffer into a newly allocated image data buffer.
    image.data = MemAlloc((unsigned int)LIBRETRO.core.frameBufferSize);
    if (image.data == NULL) {
        Image empty = {0};
        return empty;
    }
    memcpy(image.data, LIBRETRO.core.frameBuffer, LIBRETRO.core.frameBufferSize);

    return image;
}

/**
 * Check whether content has been successfully loaded.
 * @return true if a game/content is currently loaded.
 */
static bool IsLibretroGameReady(void) {
    return LIBRETRO.core.loaded && LIBRETRO.core.symbols.retro_run != NULL;
}

/**
 * Reset the currently loaded core (equivalent to a hardware reset).
 * @note A game must be loaded before calling this.
 * @return true if the core reset callback was called.
 */
static bool ResetLibretro(void) {
    if (IsLibretroReady() && LIBRETRO.core.symbols.retro_reset) {
        LIBRETRO.core.symbols.retro_reset();
        return true;
    }
    return false;
}

/**
 * Enable or disable a cheat code on the loaded core.
 *
 * @param index The index of the cheat to act upon.
 * @param enabled Whether to enable or disable the cheat.
 * @param code The cheat code string (format is core-specific).
 * @return true if the cheat was passed to the core.
 */
static bool SetLibretroCheat(unsigned index, bool enabled, const char* code) {
    if (IsLibretroGameReady() && LIBRETRO.core.symbols.retro_cheat_set && code != NULL && code[0] != '\0') {
        LIBRETRO.core.symbols.retro_cheat_set(index, enabled, code);
        return true;
    }
    return false;
}

/**
 * Reset all active cheats on the loaded core to their default disabled state.
 * @return true if the core's cheat reset callback was called.
 */
static bool ResetLibretroCheats(void) {
    if (IsLibretroGameReady() && LIBRETRO.core.symbols.retro_cheat_reset) {
        LIBRETRO.core.symbols.retro_cheat_reset();
        return true;
    }
    return false;
}

/**
 * Get a direct pointer to the core's battery-backed SRAM region.
 * @param size Output parameter filled with the SRAM region size in bytes.
 * @return Pointer into the core's SRAM memory, or NULL if the core has no SRAM.
 */
static void* GetLibretroSRAMData(size_t* size) {
    if (!IsLibretroGameReady()) return NULL;
    if (LIBRETRO.core.symbols.retro_get_memory_data == NULL || LIBRETRO.core.symbols.retro_get_memory_size == NULL) return NULL;

    size_t sramSize = LIBRETRO.core.symbols.retro_get_memory_size(RETRO_MEMORY_SAVE_RAM);
    if (sramSize == 0) return NULL;

    void* data = LIBRETRO.core.symbols.retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);
    if (data == NULL) return NULL;

    if (size != NULL) *size = sramSize;
    return data;
}

/**
 * Copy data into the core's SRAM region.
 * @param data Source buffer to copy from.
 * @param size Number of bytes to copy; clipped to the region size if larger.
 * @return true on success, false if the core has no SRAM region.
 */
static bool SetLibretroSRAMData(const void* data, size_t size) {
    if (data == NULL || size == 0) return false;

    size_t sramSize = 0;
    void* sramDest = GetLibretroSRAMData(&sramSize);
    if (sramDest == NULL) return false;

    size_t copySize = size < sramSize ? size : sramSize;
    memcpy(sramDest, data, copySize);
    return true;
}

/**
 * Unload the currently loaded content without closing the core.
 */
static void UnloadLibretroGame(void) {
    if (LIBRETRO.core.symbols.retro_unload_game != NULL) {
        LIBRETRO.core.symbols.retro_unload_game();
        LIBRETRO.core.symbols.retro_unload_game = NULL;
    }
    LIBRETRO.core.symbols.retro_run = NULL;
    LIBRETRO.core.loaded = false;
    LIBRETRO.core.shutdown = false;
    LIBRETRO.core.contentPath[0] = '\0';
    LIBRETRO.core.contentDir[0]  = '\0';
    LIBRETRO.core.contentName[0] = '\0';
    LIBRETRO.core.contentExt[0]  = '\0';
    LIBRETRO.core.gameInfoExtValid = false;
    memset(&LIBRETRO.core.gameInfoExt, 0, sizeof(LIBRETRO.core.gameInfoExt));

    // Per-game runtime state that must not leak into the next game.
    LIBRETRO.core.rotation         = 0;
    LIBRETRO.core.gameTimeNSEC     = 0;
    // Note: the OSD message intentionally survives unload (it lives in LIBRETRO,
    // not LIBRETRO.core) so a core's exit/error message stays on-screen.
    LIBRETRO.core.singleSampleCount = 0;
    LIBRETRO.core.audioDropWarnCount = 0;

    // Stop any in-flight controller rumble before clearing the stored state —
    // SetLibretroRumbleState pushed long-duration values to the hardware that
    // would otherwise keep buzzing after the game is gone.
    for (unsigned p = 0; p < RAYLIB_LIBRETRO_RUMBLE_PORTS; p++) {
        if (LIBRETRO.core.rumbleStrong[p] > 0.0f || LIBRETRO.core.rumbleWeak[p] > 0.0f) {
            SetGamepadVibration((int)p, 0.0f, 0.0f, 0.0f);
        }
    }
    memset(LIBRETRO.core.rumbleStrong, 0, sizeof(LIBRETRO.core.rumbleStrong));
    memset(LIBRETRO.core.rumbleWeak,   0, sizeof(LIBRETRO.core.rumbleWeak));

    // Release the persistent ROM buffer kept alive for persistent_data=true.
    if (LIBRETRO.core.persistentGameData != NULL) {
        UnloadFileData(LIBRETRO.core.persistentGameData);
        LIBRETRO.core.persistentGameData = NULL;
        LIBRETRO.core.persistentGameDataSize = 0;
    }

    // Free memory map descriptors — these are game-specific and must not
    // outlive the game that provided them.
    UnloadLibretroMemoryMaps();
}

/**
 * Deinitialize and unload the libretro core.
 */
static void CloseLibretro(void) {
    // Free game-scoped resources before tearing down the core. Called
    // unconditionally because a partial load (retro_load_game succeeded but
    // InitLibretroAudioVideo failed) leaves persistentGameData allocated
    // with loaded=false, and IsLibretroGameReady() would skip the cleanup.
    UnloadLibretroGame();

    // Let the core know that the audio device has been deinitialized. The
    // function pointer lives in the dylib we're about to close.
    if (LIBRETRO.core.audio_callback.set_state != NULL) {
        LIBRETRO.core.audio_callback.set_state(false);
    }

    // Call retro_deinit() to deinitialize the core.
    if (LIBRETRO.core.symbols.retro_deinit != NULL) {
        LIBRETRO.core.symbols.retro_deinit();
    }

    CloseLibretroAudio();
    CloseLibretroVideo();

    if (LIBRETRO.core.inputDescriptors != NULL) {
        MemFree(LIBRETRO.core.inputDescriptors);
    }

    if (LIBRETRO.core.controllerInfo != NULL) {
        MemFree(LIBRETRO.core.controllerInfo);
    }

    if (LIBRETRO.core.subsystemInfo != NULL) {
        MemFree(LIBRETRO.core.subsystemInfo);
        LIBRETRO.core.subsystemInfo = NULL;
        LIBRETRO.core.subsystemCount = 0;
    }

    // Close the dynamically loaded handle.
    if (LIBRETRO.core.symbols.handle != NULL) {
        dylib_close(LIBRETRO.core.symbols.handle);
    }

    // Release owned pointers before the memset wipes them.
    UnloadLibretroMemoryMaps();

    // Clear the data.
    memset(&LIBRETRO.core, 0, sizeof(LIBRETRO.core));

    // Non-zero defaults.
    LIBRETRO.core.drcAdjustment = 1.0f;
    LIBRETRO.core.drcEnabled = true;
    for (int i = 0; i < 16; i++) {
        LIBRETRO.core.portDeviceMap[i] = RETRO_DEVICE_JOYPAD;
    }
}

/**
 * Map a libretro retro_log_level to a raylib TraceLogType.
 */
static int LibretroMapRetroLogLevelToTraceLogType(int level) {
    switch (level) {
        case RETRO_LOG_DEBUG:
            return LOG_DEBUG;
        case RETRO_LOG_WARN:
            return LOG_WARNING;
        case RETRO_LOG_ERROR:
            return LOG_ERROR;
        case RETRO_LOG_INFO:
        default:
            return LOG_INFO;
    }

    return LOG_INFO;
}

/**
 * Map a libretro retro_key to a raylib KeyboardKey.
 */
static int LibretroRetroKeyToKeyboardKey(int key) {
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
static int LibretroRetroJoypadButtontoRetroKey(int button) {
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
static int LibretroRetroJoypadButtonToGamepadButton(int button) {
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

static bool LibretroAnalogToDpadPressed(int port, int btn) {
    if (LIBRETRO.analogToDpadIndex == 0 || !IsGamepadAvailable(port)) return false;
    float axisX = GetGamepadAxisMovement(port,
        LIBRETRO.analogToDpadIndex == 2 ? GAMEPAD_AXIS_RIGHT_X : GAMEPAD_AXIS_LEFT_X);
    float axisY = GetGamepadAxisMovement(port,
        LIBRETRO.analogToDpadIndex == 2 ? GAMEPAD_AXIS_RIGHT_Y : GAMEPAD_AXIS_LEFT_Y);
    switch (btn) {
        case RETRO_DEVICE_ID_JOYPAD_UP:    return axisY < -0.5f;
        case RETRO_DEVICE_ID_JOYPAD_DOWN:  return axisY >  0.5f;
        case RETRO_DEVICE_ID_JOYPAD_LEFT:  return axisX < -0.5f;
        case RETRO_DEVICE_ID_JOYPAD_RIGHT: return axisX >  0.5f;
        default: return false;
    }
}

/**
 * Maps a libretro pixel format to a raylib PixelFormat.
 */
static int LibretroRetroPixelFormatToPixelFormat(int pixelFormat) {
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
static void LibretroPixelFormatARGB1555ToRGB565(void *output_, const void *input_,
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

/**
 * Serialize the current emulator state into a new buffer.
 * @param size Output parameter filled with the size of the returned buffer in bytes.
 * @return Newly allocated buffer containing the serialized state, or NULL on failure.
 * @note The caller is responsible for freeing the returned buffer with MemFree(). */
/**
 * Size in bytes of the loaded core's serialized state, or 0 if no game is
 * ready. Useful for validating that a buffered state (e.g. a rewind snapshot)
 * still matches the currently loaded content before restoring it.
 */
static unsigned int GetLibretroSerializedSize(void) {
    if (!IsLibretroGameReady() || LIBRETRO.core.symbols.retro_serialize_size == NULL) {
        return 0;
    }
    return (unsigned int)LIBRETRO.core.symbols.retro_serialize_size();
}

static void* GetLibretroSerializedData(unsigned int* size) {
    if (!IsLibretroGameReady()) {
        return NULL;
    }

    if (LIBRETRO.core.symbols.retro_serialize_size == NULL || LIBRETRO.core.symbols.retro_serialize == NULL) {
        return NULL;
    }

    size_t finalSize = LIBRETRO.core.symbols.retro_serialize_size();
    if (finalSize == 0) {
        return NULL;
    }

    // For variable-size cores, re-query the size each call.
    if (LIBRETRO.core.serializationQuirks & RETRO_SERIALIZATION_QUIRK_CORE_VARIABLE_SIZE) {
        finalSize = LIBRETRO.core.symbols.retro_serialize_size();
    }

    if (size != NULL) {
        *size = (unsigned int)finalSize;
    }

    void* saveData = MemAlloc((unsigned int)finalSize);
    if (saveData == NULL) {
        return NULL;
    }

    // MUST_INITIALIZE: zero the buffer before serializing.
    if (LIBRETRO.core.serializationQuirks & RETRO_SERIALIZATION_QUIRK_MUST_INITIALIZE) {
        memset(saveData, 0, finalSize);
    }

    if (LIBRETRO.core.symbols.retro_serialize(saveData, finalSize)) {
        return saveData;
    }
    TraceLog(LOG_ERROR, "LIBRETRO: Failed to get retro_serialize");
    MemFree(saveData);
    return NULL;
}

/**
 * Restore a previously serialized emulator state.
 * @param data Pointer to the serialized state buffer.
 * @param size Size of the buffer in bytes.
 * @return true on success, false if the core rejected the data. */
static bool SetLibretroSerializedData(void* data, unsigned int size) {
    if (!IsLibretroGameReady()) {
        return false;
    }

    if (LIBRETRO.core.symbols.retro_unserialize == NULL) {
        return false;
    }

    // NOTE: callers that restore state every frame (rewind) must NOT reset the
    // time accumulator here — doing so would zero it each frame and starve
    // retro_run() of ticks. One-shot loaders (save-state) call
    // ResetLibretroTiming() themselves.
    return LIBRETRO.core.symbols.retro_unserialize(data, (size_t)size);
}

#endif

#if defined(__cplusplus)
}
#endif

#endif // RAYLIB_LIBRETRO_IMPLEMENTATION_ONCE
#endif // RAYLIB_LIBRETRO_IMPLEMENTATION
