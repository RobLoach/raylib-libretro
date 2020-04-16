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

// System dependencies
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

// libretro-common
#include <dynamic/dylib.h>
#include "libretro.h"

// raylib
#include "raylib.h"

// raylib-libretro includes
#include "rlibretro-map.h"

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
static void UnloadLibretroGame();                        // Unload the game that's currently loaded.
static void CloseLibretro();                             // Close the initialized libretro core.

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
    unsigned width, height, fps, sampleRate;
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

    // Game data.
    Vector2 inputLastMousePosition;
    Vector2 inputMousePosition;

    // Raylib objects used to play the libretro core.
    Texture texture;
    AudioStream audioStream;
} rLibretro;

/**
 * The global libretro core object.
 */
rLibretro LibretroCore = {0};

static void LibretroLogger(enum retro_log_level level, const char *fmt, ...) {
    int type = LibretroMapRetroLogLevelToTraceLogType(level);

    va_list va;
    char buffer[4096] = {0};

    va_start(va, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, va);
    va_end(va);

    TraceLog(type, "LIBRETRO: %s", buffer);
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
    SetTextureFilter(LibretroCore.texture, FILTER_POINT);

    // We don't need the image anymore.
    UnloadImage(image);
}

static bool LibretroSetEnvironment(unsigned cmd, void * data) {
    switch (cmd) {
        case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO:
        {
            if (data == NULL) {
                TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO no data provided");
                return false;
            }
            const struct retro_system_av_info av = *(const struct retro_system_av_info *)data;
            LibretroCore.width = av.geometry.base_width;
            LibretroCore.height = av.geometry.base_height;
            LibretroCore.fps = av.timing.fps;
            LibretroCore.sampleRate = av.timing.sample_rate;
            LibretroCore.aspectRatio = av.geometry.aspect_ratio;
            TraceLog(LOG_INFO, "LIBRETRO: RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO %i x %i @ %i FPS %i Hz",
                LibretroCore.width,
                LibretroCore.height,
                LibretroCore.fps,
                LibretroCore.sampleRate
            );
        }
        break;
        case RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL:
        {
            if (data == NULL) {
                TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL no data provided");
                return false;
            }
            LibretroCore.performanceLevel = *(const unsigned *)data;
            TraceLog(LOG_INFO, "LIBRETRO: RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL(%i)", LibretroCore.performanceLevel);
        }
        break;
        case RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME:
        {
            // When set to true, the core will run without content.
            if (data == NULL) {
                TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME no data provided");
                return false;
            }
            LibretroCore.supportNoGame = *(bool*)data;
            TraceLog(LOG_INFO, "LIBRETRO: RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME: %s", LibretroCore.supportNoGame ? "true" : "false");
        }
        break;
        case RETRO_ENVIRONMENT_SET_VARIABLES:
        {
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
        }
        break;
        case RETRO_ENVIRONMENT_GET_VARIABLE:
        {
            if (data == NULL) {
                TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_GET_VARIABLE no data provided");
                return false;
            }
            // TODO: Retrieve varables with RETRO_ENVIRONMENT_GET_VARIABLE.
            struct retro_variable* variableData = (struct retro_variable *)data;
            TraceLog(LOG_INFO, "LIBRETRO: RETRO_ENVIRONMENT_GET_VARIABLE: %s", variableData->key);
            variableData->value = "";
        }
        break;
        case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
        {
            // Whether or not the frontend variables have been changed.
            // TODO: RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE
            return false;
        }
        break;
        case RETRO_ENVIRONMENT_SET_GEOMETRY:
        {
            if (!data) {
                TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_SET_GEOMETRY no data provided");
                return false;
            }
            const struct retro_game_geometry *geom = (const struct retro_game_geometry *)data;
            LibretroCore.width = geom->base_width;
            LibretroCore.height = geom->base_height;
            LibretroCore.aspectRatio = geom->aspect_ratio;
            TraceLog(LOG_INFO, "LIBRETRO: RETRO_ENVIRONMENT_SET_GEOMETRY %i x %i @ %i FPS %i Hz",
                LibretroCore.width,
                LibretroCore.height,
                LibretroCore.aspectRatio
            );
        }
        break;
        case RETRO_ENVIRONMENT_GET_INPUT_DEVICE_CAPABILITIES:
        {
            uint64_t *capabilities = (uint64_t*)data;
            *capabilities = (1 << RETRO_DEVICE_JOYPAD) |
                (1 << RETRO_DEVICE_MOUSE) |
                (1 << RETRO_DEVICE_KEYBOARD) |
                (1 << RETRO_DEVICE_POINTER);
            TraceLog(LOG_INFO, "RETRO_ENVIRONMENT_GET_INPUT_DEVICE_CAPABILITIES");
        }
        break;
        case RETRO_ENVIRONMENT_SET_MESSAGE:
        {
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
        }
        break;
        case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
        {
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
                TraceLog(LOG_INFO, "LIBRETRO: RETRO_ENVIRONMENT_SET_PIXEL_FORMAT UNKNOWN");
                break;
            }
        }
        break;
        case RETRO_ENVIRONMENT_SHUTDOWN:
        {
            TraceLog(LOG_INFO, "LIBRETRO: RETRO_ENVIRONMENT_SHUTDOWN");
            LibretroCore.shutdown = true;
        }
        break;
        case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
        {
            if (data == NULL) {
                TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_GET_LOG_INTERFACE no data provided");
                return false;
            }
            TraceLog(LOG_INFO, "LIBRETRO: RETRO_ENVIRONMENT_GET_LOG_INTERFACE");
            struct retro_log_callback *callback = (struct retro_log_callback*)data;
            if (callback != NULL) {
                callback->log = LibretroLogger;
            }
        }
        break;
        case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
        {
            // TODO: RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS
            TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS not implemented");
        }
        break;
        default:
        {
            // TODO: Add more environment sets.
            TraceLog(LOG_WARNING, "LIBRETRO: Undefined environment call: %i", cmd);
            return false;
        }
        break;
    }

    return true;
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
    TraceLog(LOG_INFO, "    > Sample rate:  %i Hz", LibretroCore.sampleRate);

    return true;
}

/**
 * Runs an interation of the libretro core.
 */
static void UpdateLibretro() {
    if (LibretroCore.loaded && LibretroCore.retro_run != NULL) {
        LibretroCore.retro_run();
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

    // Translate for the pixel format.
    switch (LibretroCore.pixelFormat) {
        case RETRO_PIXEL_FORMAT_XRGB8888:
        {
            // Port RETRO_PIXEL_FORMAT_XRGB8888 to UNCOMPRESSED_R8G8B8A8.
            LibretroMapPixelFormatARGB8888ToABGR8888((void*)data, data, width, height, pitch, pitch);
        }
        break;
        case RETRO_PIXEL_FORMAT_RGB565:
        {
            // Nothing needed to port from RETRO_PIXEL_FORMAT_RGB565 to UNCOMPRESSED_R5G6B5.
        }
        break;
        case RETRO_PIXEL_FORMAT_0RGB1555:
        default:
        {
            // Port RETRO_PIXEL_FORMAT_0RGB1555 to UNCOMPRESSED_R5G6B5
            LibretroMapPixelFormatARGB1555ToRGB565((void*)data, data, width, height, pitch, pitch);
        }
    }

    // Update the texture data.
    UpdateTexture(LibretroCore.texture, data);
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

    // Keyboard
    if (device == RETRO_DEVICE_KEYBOARD) {
        id = LibretroMapRetroKeyToKeyboardKey(id);
        if (id > 0) {
            return (int)IsKeyDown(id);
        }
    }

    // Gamepad
    // TODO: Have gamepads not offset by player 1 on the keyboard.
    if (device == RETRO_DEVICE_JOYPAD) {
        id = LibretroMapRetroJoypadButtonToGamepadButton(id);
        return (int)IsGamepadButtonDown(port - 1, id);
    }

    // Mouse
    if (device == RETRO_DEVICE_MOUSE) {
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
            break;
        }
    }

    // Pointer
    if (device == RETRO_DEVICE_POINTER) {
        float max = 0x7fff;
        switch (id) {
            case RETRO_DEVICE_ID_POINTER_X:
                return (float)(GetMouseX() + GetScreenWidth()) / (GetScreenWidth() * 2.0f) * (float)GetScreenWidth();
            case RETRO_DEVICE_ID_POINTER_Y:
                return (float)(GetMouseY() + GetScreenHeight()) / (GetScreenHeight() * 2.0f) * (float)GetScreenHeight();
            case RETRO_DEVICE_ID_POINTER_PRESSED:
                return IsMouseButtonDown(MOUSE_LEFT_BUTTON);
        }
    }

    return 0;
}

static size_t LibretroAudioWrite(const int16_t *data, size_t frames) {
    // TODO: Fix Audio being choppy since it doesn't append to the buffer.
    if (IsAudioStreamProcessed(LibretroCore.audioStream)) {
        UpdateAudioStream(LibretroCore.audioStream, data, sizeof(*data) * frames);
    }

    return frames;
}

static void LibretroAudioSample(int16_t left, int16_t right) {
    int16_t buf[2] = {left, right};
    LibretroAudioWrite(buf, 1);
}

static size_t LibretroAudioSampleBatch(const int16_t *data, size_t frames) {
    return LibretroAudioWrite(data, frames);
}

static void LibretroInitAudio()
{
    // Ensure the audio stream is closed.
    StopAudioStream(LibretroCore.audioStream);
    CloseAudioStream(LibretroCore.audioStream);

    // Create a new audio stream.
    LibretroCore.audioStream = InitAudioStream(LibretroCore.sampleRate, 16, 2);
    PlayAudioStream(LibretroCore.audioStream);
    return;
}

static bool LibretroInitAudioVideo() {
    LibretroGetAudioVideo();
    LibretroInitVideo();
    LibretroInitAudio();

    // TODO: Support frametime perforance counter instead of static FPS.
    if (LibretroCore.fps > 0) {
        SetTargetFPS(LibretroCore.fps);
    }

    LibretroCore.loaded = true;

    return true;
}

static bool LoadLibretroGame(const char* gameFile) {
    // Load empty game.
    if (gameFile == NULL) {
        if (LibretroCore.retro_load_game(NULL)) {
            TraceLog(LOG_INFO, "LIBRETRO: Loaded without content");
            return LibretroInitAudioVideo();
        }
        TraceLog(LOG_ERROR, "LIBRETRO: Failed to load core without content");
        return false;
    }

    // Ensure the game exists.
    if (!FileExists(gameFile)) {
        TraceLog(LOG_ERROR, "LIBRETRO: Given content does not exist: %s", gameFile);
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
            return LibretroInitAudioVideo();
        }
        else {
            TraceLog(LOG_ERROR, "LIBRETRO: Failed to load full path");
            return false;
        }
    }

    // Load the game data from the given file.
    unsigned int size;
    unsigned char * gameData = LoadFileData(gameFile, &size);
    if (size == 0 || gameData == NULL) {
        TraceLog(LOG_ERROR, "LIBRETRO: Failed to load game data with LoadFileData()");
        return false;
    }

    // Load the game.
    struct retro_game_info info;
    info.path = gameFile;
    info.data = gameData;
    info.size = size;
    info.meta = "";
    if (!LibretroCore.retro_load_game(&info)) {
        free(gameData);
        TraceLog(LOG_ERROR, "LIBRETRO: Failed to load game data with retro_load_game()");
        return false;
    }
    free(gameData);
    return LibretroInitAudioVideo();
}

static const char* GetLibretroName() {
    return LibretroCore.libraryName;
}

static const char* GetLibretroVersion() {
    return LibretroCore.libraryVersion;
}

static bool InitLibretro(const char* core) {
    // Ensure the core exists.
    if (!FileExists(core)) {
        TraceLog(LOG_ERROR, "LIBRETRO: Given core doesn't exist: %s", core);
        return false;
    }

    // If there's an existing libretro core, close it.
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
    DrawTexture(LibretroCore.texture, posX, posY, tint);
}

static void DrawLibretroV(Vector2 position, Color tint) {
    DrawTextureV(LibretroCore.texture, position, tint);
}

static void DrawLibretroEx(Vector2 position, float rotation, float scale, Color tint) {
    DrawTextureEx(LibretroCore.texture, position, rotation, scale, tint);
}

static void DrawLibretroPro(Rectangle destRec, Color tint) {
    if (LibretroCore.texture.width > 0){
        Rectangle source = {0, 0, LibretroCore.width, LibretroCore.height};
        Vector2 origin = {0, 0};
        DrawTexturePro(LibretroCore.texture, source, destRec, origin, 0, tint);
    }
}

static void DrawLibretroTint(Color tint) {
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
    return LibretroCore.loaded;
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
    // Call retro_deinit() to deinitialize the core.
    if (LibretroCore.retro_deinit) {
        LibretroCore.retro_deinit();
    }

    // Stop, close and unload all raylib objects.
    StopAudioStream(LibretroCore.audioStream);
    CloseAudioStream(LibretroCore.audioStream);
    UnloadTexture(LibretroCore.texture);

    // Close the dynamically loaded handle.
    if (LibretroCore.handle != NULL) {
        dylib_close(LibretroCore.handle);
        LibretroCore.handle = NULL;
    }
    LibretroCore = (rLibretro){0};
}

#if defined(__cplusplus)
}
#endif

#endif // RAYLIB_LIBRETRO_RLIBRETRO_H
