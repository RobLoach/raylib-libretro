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

#ifndef RAYLIB_LIBRETRO_RLIBRETRO_H
#define RAYLIB_LIBRETRO_RLIBRETRO_H

#include "libretro.h"
#include "raylib.h"

#if defined(__cplusplus)
extern "C" {            // Prevents name mangling of functions
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
static unsigned GetLibretroWidth();                      // Get the desired width of the libretro core.
static unsigned GetLibretroHeight();                     // Get the desired height of the libretro core.
static Texture2D GetLibretroTexture();                   // Retrieve the texture used to render the libretro state.
static bool DoesLibretroCoreNeedContent();               // Determine whether or not the loaded core require content.
static void UnloadLibretroGame();                        // Unload the game that's currently loaded.
static void CloseLibretro();                             // Close the initialized libretro core.
void* GetLibretroSerializedData(unsigned int* size);
bool SetLibretroSerializedData(void* data, unsigned int size);

void LibretroMapPixelFormatARGB1555ToRGB565(void *output_, const void *input_,
        int width, int height,
        int out_stride, int in_stride);
void LibretroMapPixelFormatARGB8888ToRGBA8888(void *output_, const void *input_,
    int width, int height,
    int out_stride, int in_stride);

static int LibretroMapRetroPixelFormatToPixelFormat(int pixelFormat);
static int LibretroMapRetroJoypadButtonToGamepadButton(int button);
static int LibretroMapRetroJoypadButtonToRetroKey(int button);
static int LibretroMapRetroKeyToKeyboardKey(int key);
static int LibretroMapRetroLogLevelToTraceLogType(int level);

#include "raylib-libretro-vfs.h"

#if defined(__cplusplus)
}
#endif

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

#define RAYLIB_LIBRETRO_VFS_IMPLEMENTATION
#include "raylib-libretro-vfs.h"

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

    // Audio
    AudioStream audioStream;
    int16_t *audioBuffer;
    size_t audioFrames;
    float raylibAudioData[64];

    // Callbacks
    retro_keyboard_event_t keyboard_event;
    struct retro_vfs_interface vfs_interface;
    // struct retro_game_info_ext game_info_ext;
} rLibretro;

#if defined(__cplusplus)
extern "C" {            // Prevents name mangling of functions
#endif

/**
 * The global libretro core object.
 */
static rLibretro LibretroCore = {0};

static void LibretroLogger(enum retro_log_level level, const char *fmt, ...) {
    int type = LibretroMapRetroLogLevelToTraceLogType(level);

    va_list va;
    va_start(va, fmt);
    const char* message = TextReplace(TextFormat(fmt, va), "\n", "");
    va_end(va);

    if (TextLength(message) > 0) {
        TraceLog(type, "LIBRETRO: %s", message);
    }
}

static void LibretroInitVideo() {
    // Unload the existing texture if it exists already.
    UnloadTexture(LibretroCore.texture);

    // Build the rendering image.
    Image image = GenImageColor(LibretroCore.width, LibretroCore.height, BLACK);
    ImageFormat(&image, LibretroMapRetroPixelFormatToPixelFormat(LibretroCore.pixelFormat));
    ImageMipmaps(&image);

    // Create the texture.
    LibretroCore.texture = LoadTextureFromImage(image);
    SetTextureFilter(LibretroCore.texture, TEXTURE_FILTER_POINT);

    // We don't need the image anymore.
    UnloadImage(image);
}

/**
 * Retrieve the current time in milliseconds.
 *
 * @todo Retrieve the unix timestamp in milliseconds?
 * @return retro_time_t The current time in milliseconds.
 */
static retro_time_t LibretroGetTimeUSEC() {
    return (retro_time_t)(GetTime() * 1000.0);
}

/**
 * Get the CPU Features.
 *
 * @see retro_get_cpu_features_t
 * @return uint64_t Returns a bit-mask of detected CPU features (RETRO_SIMD_*).
 */
static uint64_t LibretroGetCPUFeatures() {
    // TODO: Implement CPU Features
    return 0;
}

/**
 * A simple counter. Usually nanoseconds, but can also be CPU cycles.
 *
 * @see retro_perf_get_counter_t
 * @return retro_perf_tick_t The current value of the high resolution counter.
 */
static retro_perf_tick_t LibretroGetPerfCounter() {
    return (retro_perf_tick_t)(GetTime() * 1000000000.0);
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
            TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_SET_ROTATION not implemented");
            return false;
        }

        case RETRO_ENVIRONMENT_GET_OVERSCAN: {
            TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_GET_OVERSCAN not implemented");
            return false;
        }

        case RETRO_ENVIRONMENT_GET_CAN_DUPE: {
            bool* var = (bool*)data;
            *var = false;
            return true;
        }

        case RETRO_ENVIRONMENT_SET_MESSAGE: {
            // TODO: RETRO_ENVIRONMENT_SET_MESSAGE Display a message on the screen.
            if (data == NULL) {
                TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_SET_MESSAGE no data provided");
                return false;
            }
            const struct retro_message message = *(const struct retro_message *)data;
            if (message.msg == NULL) {
                TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_SET_MESSAGE: No message");
                return false;
            }
            TraceLog(LOG_INFO, "LIBRETRO: RETRO_ENVIRONMENT_SET_MESSAGE: %s (%i frames)", message.msg, message.frames);
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
            *dir = GetWorkingDirectory();
            return true;
        }

        case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: {
            if (data == NULL) {
                TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_SET_PIXEL_FORMAT no data set");
                return false;
            }
            const enum retro_pixel_format* pixelFormat = (const enum retro_pixel_format *)data;
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
                TraceLog(LOG_ERROR, "LIBRETRO: RETRO_ENVIRONMENT_SET_PIXEL_FORMAT UNKNOWN");
                return false;
            }
            return true;
        }

        case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS: {
            // TODO: RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS
            TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS not implemented");
            return false;
        }

        case RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK: {
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
            struct retro_variable * var = (struct retro_variable *)data;
            if (data == NULL) {
                TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_GET_VARIABLE no data provided");
                return false;
            }

            // Override some default variables.
            if (TextIsEqual(var->key, "fceumm_sndvolume")) {
                var->value = "10";
                return true;
            }

            TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_GET_VARIABLE requested: %s", var->key);
            return false;
        }

        case RETRO_ENVIRONMENT_SET_VARIABLES: {
            // Adds a variable definition.
            if (data == NULL) {
                TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_SET_VARIABLES no data provided");
                return false;
            }

            const struct retro_variable * var = (const struct retro_variable *)data;
            // TODO: RETRO_ENVIRONMENT_SET_VARIABLES
            TraceLog(LOG_INFO, "LIBRETRO: RETRO_ENVIRONMENT_SET_VARIABLES");
            TraceLog(LOG_INFO, "    > Key:   %s", var->key);
            TraceLog(LOG_INFO, "    > Value: %s", var->value);
            return true;
        }

        case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE: {
            bool* var = (bool*)data;
            *var = false;

            // TODO: RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE
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
            const char** dir = (const char**)data;
            *dir = GetWorkingDirectory();
            return true;
        }

        case RETRO_ENVIRONMENT_SET_FRAME_TIME_CALLBACK: {
            const struct retro_frame_time_callback *frame_time = (const struct retro_frame_time_callback*)data;
            LibretroCore.runloop_frame_time = *frame_time;
            return true;
        }

        case RETRO_ENVIRONMENT_SET_AUDIO_CALLBACK: {
            struct retro_audio_callback *audio_cb = (struct retro_audio_callback*)data;
            LibretroCore.audio_callback = *audio_cb;
            return true;
        }

        case RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE: {
            TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE not implemented");
            return false;
        }

        case RETRO_ENVIRONMENT_GET_INPUT_DEVICE_CAPABILITIES: {
            TraceLog(LOG_INFO, "LIBRETRO: RETRO_ENVIRONMENT_GET_INPUT_DEVICE_CAPABILITIES");
            uint64_t *capabilities = (uint64_t*)data;
            *capabilities = (1 << RETRO_DEVICE_JOYPAD) |
                (1 << RETRO_DEVICE_MOUSE) |
                (1 << RETRO_DEVICE_KEYBOARD) |
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
            TraceLog(LOG_INFO, "LIBRETRO: RETRO_ENVIRONMENT_GET_LOG_INTERFACE");
            struct retro_log_callback *callback = (struct retro_log_callback*)data;
            if (callback != NULL) {
                callback->log = LibretroLogger;
            }
            return true;
        }

        case RETRO_ENVIRONMENT_GET_PERF_INTERFACE: {
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
            const char** dir = (const char**)data;
            *dir = GetWorkingDirectory();
            return true;
        }

        case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY: {
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
            TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_SET_CONTROLLER_INFO not implemented");
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
            const char** name = (const char**)data;
            *name = "raylib";
            return true;
        }

        case RETRO_ENVIRONMENT_GET_LANGUAGE: {
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
            *output = RETRO_AV_ENABLE_VIDEO | RETRO_AV_ENABLE_AUDIO;
            return true;
        }

        case RETRO_ENVIRONMENT_GET_MIDI_INTERFACE: {
            TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_GET_MIDI_INTERFACE not implemented");
            return false;
        }

        case RETRO_ENVIRONMENT_GET_FASTFORWARDING: {
            // Fast forward is not supported currently.
            bool* output = (bool *)data;
            *output = false;
            return true;
        }

        case RETRO_ENVIRONMENT_GET_TARGET_REFRESH_RATE: {
            float *refreshRate = (float *)data;
            int currentRate = GetMonitorRefreshRate(GetCurrentMonitor());
            *refreshRate = (float)currentRate;
            TraceLog(LOG_INFO, "LIBRETRO: Monitor Refresh Rate: %i", currentRate);
            return true;
        }

        case RETRO_ENVIRONMENT_GET_INPUT_BITMASKS: {
            TraceLog(LOG_INFO, "LIBRETRO: RETRO_ENVIRONMENT_GET_INPUT_BITMASKS not supported");
            // TODO: Add RETRO_ENVIRONMENT_GET_INPUT_BITMASKS support
            return false;
        }

        case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION: {
            TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION not implemented");
            return false;
        }

        case RETRO_ENVIRONMENT_SET_CORE_OPTIONS: {
            TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_SET_CORE_OPTIONS not implemented");
            return false;
        }

        case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL: {
            TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL not implemented");
            return false;
        }

        case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY: {
            TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY not implemented");
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
            // TODO: Add support for RETRO_ENVIRONMENT_SET_MESSAGE_EXT.
            unsigned * interfaceVersion = (unsigned *)data;
            *interfaceVersion = 0;
            return true;
        }

        case RETRO_ENVIRONMENT_SET_MESSAGE_EXT: {
            TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_SET_MESSAGE_EXT not implemented");
            return false;
        }

        case RETRO_ENVIRONMENT_GET_INPUT_MAX_USERS: {
            // TODO: RETRO_ENVIRONMENT_GET_INPUT_MAX_USERS: Add support for more users.
            unsigned * maxUsers = (unsigned *)data;
            *maxUsers = 1;
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
            TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2 not implemented");
            return false;
        }

        case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2_INTL: {
            TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2_INTL not implemented");
            return false;
        }

        case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_UPDATE_DISPLAY_CALLBACK: {
            TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_SET_CORE_OPTIONS_UPDATE_DISPLAY_CALLBACK not implemented");
            return false;
        }

        case RETRO_ENVIRONMENT_SET_VARIABLE: {
            TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_SET_VARIABLE not implemented");
            return false;
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
            TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_GET_PLAYLIST_DIRECTORY not implemented");
            return false;
        }

        case RETRO_ENVIRONMENT_GET_FILE_BROWSER_START_DIRECTORY: {
            TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_GET_FILE_BROWSER_START_DIRECTORY not implemented");
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
    LibretroCore.fps = av.timing.fps;
    LibretroCore.sampleRate = av.timing.sample_rate;
    LibretroCore.aspectRatio = av.geometry.aspect_ratio;
    TraceLog(LOG_INFO, "LIBRETRO: Video and Audio information");
    TraceLog(LOG_INFO, "    > Display size: %i x %i", LibretroCore.width, LibretroCore.height);
    TraceLog(LOG_INFO, "    > Target FPS:   %i", LibretroCore.fps);
    TraceLog(LOG_INFO, "    > Sample rate:  %.2f Hz", LibretroCore.sampleRate);

    return true;
}

/**
 * Runs an interation of the libretro core.
 */
static void UpdateLibretro() {
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

        // Check each keyboard key
        for (int key = RETROK_FIRST; key < RETROK_LAST; key++) {
            int raylibKey = LibretroMapRetroKeyToKeyboardKey(key);
            if (raylibKey > 0) {
                // TODO: Figure out character parameter.
                if (IsKeyPressed(raylibKey)) {
                    LibretroCore.keyboard_event(true, key, 0, key_modifiers);
                }
                else if (IsKeyReleased(raylibKey)) {
                    LibretroCore.keyboard_event(false, key, 0, key_modifiers);
                }
            }
        }
    }

    if (IsLibretroGameReady()) {
        LibretroCore.retro_run();

        // Save State
        if (IsKeyPressed(KEY_F5)) {
            unsigned int size;
            void* data = GetLibretroSerializedData(&size);
            if (data != NULL) {
                SaveFileData(TextFormat("save_%s.sav", GetLibretroName()), data, (int)size);
                MemFree(data);
            }
        } else if (IsKeyPressed(KEY_F9)) {
            int dataSize;
            void* data = LoadFileData(TextFormat("save_%s.sav", GetLibretroName()), &dataSize);
            if (data != NULL) {
                SetLibretroSerializedData(data, (unsigned int)dataSize);
                MemFree(data);
            }
        }
    }
}

/**
 * Retrieve whether or no the core has been loaded.
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

void LibretroMapPixelFormatARGB8888ToRGBA8888(void *output_, const void *input_,
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
            uint8_t b   = (input[w] >> 16) & 0xff;
            uint8_t g   = (input[w] >> 8) & 0xff;
            uint8_t r   = (input[w]) & 0xff;
            uint8_t a   = 0xff;

            // Force the alpha channel
            output[w]    = (a << 24) | (r << 16) | (g << 8) | b;
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

    //void* outData = (void*)data;

    // Resize the video if needed.
    if (width != LibretroCore.width || height != LibretroCore.height) {
        LibretroCore.width = width;
        LibretroCore.height = height;
        LibretroInitVideo();
    }

    // TODO: Move the MemAlloc() into a already built screen buffer, or something.
    switch (LibretroCore.pixelFormat) {
        case RETRO_PIXEL_FORMAT_RGB565: {
            // FCEUMM: Working
            // SNES9X: Broken
            UpdateTexture(LibretroCore.texture, data);
        }
        break;
        case RETRO_PIXEL_FORMAT_0RGB1555: {
            Image image;
            image.format = PIXELFORMAT_UNCOMPRESSED_R5G6B5;
            image.mipmaps = 1;
            image.width = width;
            image.height = height;
            image.data = MemAlloc(GetPixelDataSize(width, height, PIXELFORMAT_UNCOMPRESSED_R5G6B5));

            LibretroMapPixelFormatARGB1555ToRGB565(image.data, data, width, height,
                GetPixelDataSize(width, 1, PIXELFORMAT_UNCOMPRESSED_R5G6B5),
                GetPixelDataSize(width, 1, PIXELFORMAT_UNCOMPRESSED_R5G5B5A1));
            UpdateTexture(LibretroCore.texture, image.data);
            UnloadImage(image);
        }
        break;
        case RETRO_PIXEL_FORMAT_XRGB8888: {
            // Blastem: Working
            // BSNES: Working
            Image image;
            image.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
            image.mipmaps = 1;
            image.width = width;
            image.height = height;
            image.data = MemAlloc(GetPixelDataSize(width, height, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8));

            LibretroMapPixelFormatARGB8888ToRGBA8888(image.data, data,
                width, height,
                GetPixelDataSize(width, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8), pitch);

            UpdateTexture(LibretroCore.texture, image.data);
            UnloadImage(image);
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
    // Force player 1 to use the keyboard.
    if (device == RETRO_DEVICE_JOYPAD && port == 0) {
        device = RETRO_DEVICE_KEYBOARD;
        id = LibretroMapRetroJoypadButtonToRetroKey(id);
        if (id == RETROK_UNKNOWN) {
            return 0;
        }
    }

    switch (device) {
        case RETRO_DEVICE_KEYBOARD: {
            id = LibretroMapRetroKeyToKeyboardKey(id);
            if (id > 0) {
                return (int)IsKeyDown(id);
            }
            return 0;
        }
        case RETRO_DEVICE_JOYPAD: {
            id = LibretroMapRetroJoypadButtonToGamepadButton(id);
            return (int)IsGamepadButtonDown(port - 1, id);
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
            }
            return 0;
        }

        case RETRO_DEVICE_POINTER: {
            // TODO: Map the pointer coordinates correctly.
            float max = 0x7fff;
            switch (id) {
                case RETRO_DEVICE_ID_POINTER_X:
                    return (float)(GetMouseX() + GetScreenWidth()) / (GetScreenWidth() * 2.0f) * (float)GetScreenWidth();
                case RETRO_DEVICE_ID_POINTER_Y:
                    return (float)(GetMouseY() + GetScreenHeight()) / (GetScreenHeight() * 2.0f) * (float)GetScreenHeight();
                case RETRO_DEVICE_ID_POINTER_PRESSED:
                    return IsMouseButtonDown(MOUSE_LEFT_BUTTON);
            }
            return 0;
        }

        case RETRO_DEVICE_ID_JOYPAD_MASK: {

        }
    }

    return 0;
}

static size_t LibretroAudioWrite(const int16_t *data, size_t frames) {
    if (data == NULL) {
        return 0;
    }

    // UpdateAudioStream(LibretroCore.audioStream, data, frames);

    // LibretroCore.audioBuffer = data;
    // LibretroCore.audioFrames = frames;

    //     // TODO: Fix Audio being choppy since it doesn't append to the buffer.
        //if (IsAudioStreamProcessed(LibretroCore.audioStream)) {
            // for (size_t i = 0; i < frames; i++) {
            //     LibretroCore.raylibAudioData[i] = (float)data[i];
            // }
        if (IsAudioStreamProcessed(LibretroCore.audioStream)) {
            UpdateAudioStream(LibretroCore.audioStream, data, frames);
        }
    // }

    return frames;
}

static void LibretroAudioSample(int16_t left, int16_t right) {
    int16_t buf[2] = {left, right};
    LibretroAudioWrite(buf, 1);
}

static size_t LibretroAudioSampleBatch(const int16_t *data, size_t frames) {
    return LibretroAudioWrite(data, frames);
}

static void LibretroInitAudio() {
    // Set the audio stream buffer size.
    int sampleSize = 16;
    int channels = 2;

    // Create the audio stream.
    SetAudioStreamBufferSizeDefault(sizeof(uint16_t));
    LibretroCore.audioStream = LoadAudioStream((unsigned int)LibretroCore.sampleRate, sampleSize, channels);

    //SetAudioStreamCallback(LibretroCore.audioStream, LibretroAudioCallback);
    PlayAudioStream(LibretroCore.audioStream);

    // Let the core know that the audio device has been initialized.
    if (LibretroCore.audio_callback.set_state) {
        LibretroCore.audio_callback.set_state(true);
    }

    TraceLog(LOG_INFO, "LIBRETRO: Audio stream initialized (%i Hz, %i bit)", sampleSize, (int)LibretroCore.sampleRate);
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

static bool InitLibretro(const char* core) {
    // Ensure the core exists.
    if (!FileExists(core)) {
        TraceLog(LOG_ERROR, "LIBRETRO: Given core doesn't exist: %s", core);
        return false;
    }

    if (LibretroCore.handle != NULL) {
        CloseLibretro();
    }

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

static void DrawLibretroTint(Color tint) {
    if (LibretroCore.loaded == false) {
        return;
    }
    // Find the aspect ratio.
    float aspect = LibretroCore.aspectRatio;
    if (aspect <= 0) {
        aspect = (float)LibretroCore.width / (float)LibretroCore.height;
    }

    // Calculate the optimal width/height to display in the screen size.
    int height = GetScreenHeight();
    int width = height * aspect;
    if (width > GetScreenWidth()) {
        height = (float)GetScreenWidth() / aspect;
        width = GetScreenWidth();
    }

    // Draw the texture in the middle of the screen.
    int x = (GetScreenWidth() - width) / 2;
    int y = (GetScreenHeight() - height) / 2;
    Rectangle destRect = {x, y, width, height};
    DrawLibretroPro(destRect, tint);
}

static void DrawLibretro() {
    DrawLibretroTint(WHITE);
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
 * Retrieve whether or not the game has been loaded.
 */
static void ResetLibretro() {
    if (IsLibretroReady() && LibretroCore.retro_reset) {
        LibretroCore.retro_reset();
    }
}

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
    // Let the core know that the audio device has been initialized.
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
    UnloadTexture(LibretroCore.texture);

    // Close the dynamically loaded handle.
    if (LibretroCore.handle != NULL) {
        dylib_close(LibretroCore.handle);
        LibretroCore.handle = NULL;
    }
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
    // TODO: Fix key mappings usings KEY_APOSTROPHE.
    switch (key){
        case RETROK_BACKSPACE:
            return KEY_BACKSPACE;
        case RETROK_TAB:
            return KEY_TAB;
        case RETROK_CLEAR:
            return KEY_PAUSE;
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
            return KEY_APOSTROPHE;
        case RETROK_HASH:
            return KEY_APOSTROPHE;
        case RETROK_DOLLAR:
            return KEY_APOSTROPHE;
        case RETROK_AMPERSAND:
            return KEY_APOSTROPHE;
        case RETROK_QUOTE:
            return KEY_APOSTROPHE;
        case RETROK_LEFTPAREN:
            return KEY_APOSTROPHE;
        case RETROK_RIGHTPAREN:
            return KEY_APOSTROPHE;
        case RETROK_ASTERISK:
            return KEY_APOSTROPHE;
        case RETROK_PLUS:
            return KEY_APOSTROPHE;
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
            return KEY_APOSTROPHE;
        case RETROK_SEMICOLON:
            return KEY_SEMICOLON;
        case RETROK_LESS:
            return KEY_APOSTROPHE;
        case RETROK_EQUALS:
            return KEY_APOSTROPHE;
        case RETROK_GREATER:
            return KEY_APOSTROPHE;
        case RETROK_QUESTION:
            return KEY_SLASH;
        case RETROK_AT:
            return KEY_APOSTROPHE;
        case RETROK_LEFTBRACKET:
            return KEY_APOSTROPHE;
        case RETROK_BACKSLASH:
            return KEY_APOSTROPHE;
        case RETROK_RIGHTBRACKET:
            return KEY_APOSTROPHE;
        case RETROK_CARET:
            return KEY_APOSTROPHE;
        case RETROK_UNDERSCORE:
            return KEY_MINUS;
        case RETROK_BACKQUOTE:
            return KEY_APOSTROPHE;
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
            return KEY_APOSTROPHE;
        case RETROK_BAR:
            return KEY_APOSTROPHE;
        case RETROK_RIGHTBRACE:
            return KEY_APOSTROPHE;
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
            return KEY_APOSTROPHE;
        case RETROK_F14:
            return KEY_APOSTROPHE;
        case RETROK_F15:
            return KEY_APOSTROPHE;
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
            return KEY_APOSTROPHE;
        case RETROK_COMPOSE:
            return KEY_APOSTROPHE;
        case RETROK_HELP:
            return KEY_APOSTROPHE;
        case RETROK_PRINT:
            return KEY_PRINT_SCREEN;
        case RETROK_SYSREQ:
            return KEY_APOSTROPHE;
        case RETROK_BREAK:
            return KEY_APOSTROPHE;
        case RETROK_MENU:
            return KEY_APOSTROPHE;
        case RETROK_POWER:
            return KEY_APOSTROPHE;
        case RETROK_EURO:
            return KEY_APOSTROPHE;
        case RETROK_UNDO:
            return KEY_APOSTROPHE;
        case RETROK_OEM_102:
            return KEY_APOSTROPHE;
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
 * TODO: Verify that LibretroMapPixelFormatARGB1555ToRGB565 is working. SNES9x uses it.
 */
void LibretroMapPixelFormatARGB1555ToRGB565(void *output_, const void *input_,
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

void* GetLibretroSerializedData(unsigned int* size) {
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

bool SetLibretroSerializedData(void* data, unsigned int size) {
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
