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
static bool LoadLibretroGame(const char* gameFile);      // Load the provided content.
static void UpdateLibretro();                            // Run an iteration of the core.
static bool LibretroShouldClose();                       // Check whether or not the core has requested to shutdown.
static void DrawLibretro();                              // Draw the libretro state on the screen.
static void DrawLibretroTint(Color tint);                // Draw the libretro state on the screen with a tint.
static void DrawLibretroEx(Vector2 position, float rotation, float scale, Color tint);
static void DrawLibretroV(Vector2 position, Color tint);
static void DrawLibretroTexture(int posX, int posY, Color tint);
static void DrawLibretroPro(Rectangle destRec, Color tint);
static unsigned GetLibretroWidth();                      // Get the desired width of the libretro core.
static unsigned GetLibretroHeight();                     // Get the desired height of the libretro core.
static Texture2D GetLibretroTexture();                   // Retrieve the texture used to render the libretro state.
static void UnloadLibretroGame();                        // Unload the game that's currently loaded.
static void CloseLibretro();                             // Close the initialized libretro core.

#define load_sym(V, S) do {\
   function_t func = dylib_proc(LibretroCore.handle, #S); \
   memcpy(&V, &func, sizeof(func)); \
   if (!func) \
      TraceLog(LOG_ERROR, "LIBRETRO: Failed to load symbol '" #S "' - ", dylib_error()); \
   } while (0)

#define load_retro_sym(S) load_sym(LibretroCore.S, S)

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
    bool initialized;
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
                TraceLog(LOG_ERROR, "LIBRETRO: RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO no data provided");
                return false;
            }
            const struct retro_system_av_info av = *(const struct retro_system_av_info *)data;
            LibretroCore.width = av.geometry.base_width;
            LibretroCore.height = av.geometry.base_height;
            LibretroCore.fps = av.timing.fps;
            LibretroCore.sampleRate = av.timing.sample_rate;
            LibretroCore.aspectRatio = av.geometry.aspect_ratio;
            TraceLog(LOG_INFO, "LIBRETRO: RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO %ix%i @ %i FPS %i",
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
                TraceLog(LOG_ERROR, "LIBRETRO: RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL no data provided");
                return false;
            }
            LibretroCore.performanceLevel = *(const unsigned *)data;
            TraceLog(LOG_INFO, "LIBRETRO: RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL(%i)", LibretroCore.performanceLevel);
        }
        break;
        case RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME:
        {
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
            // Sets the given variable.
            // TODO: RETRO_ENVIRONMENT_SET_VARIABLES
            TraceLog(LOG_WARNING, "LIBRETRO: RETRO_ENVIRONMENT_SET_VARIABLES not implemented");
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
        case RETRO_ENVIRONMENT_SET_MESSAGE:
        {
            // TODO: RETRO_ENVIRONMENT_SET_MESSAGE Display a message on the screen.
            if (data == NULL) {
                TraceLog(LOG_ERROR, "LIBRETRO: RETRO_ENVIRONMENT_SET_MESSAGE no data provided");
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
                TraceLog(LOG_ERROR, "LIBRETRO: RETRO_ENVIRONMENT_SET_PIXEL_FORMAT no data set");
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
                TraceLog(LOG_ERROR, "LIBRETRO: RETRO_ENVIRONMENT_GET_LOG_INTERFACE no data provided");
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
            TraceLog(LOG_WARNING, "LIBRETRO: TODO: RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS");
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
    if (LibretroCore.initialized) {
        LibretroCore.retro_run();
    }
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
            LibretroMapPixelFormatARGB8888ToABGR8888((void*)data, data, width, height, width << 2, width << 2);
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
            LibretroMapPixelFormatARGB1555ToRGB565((void*)data, data, width, height, width << 2, width << 2);
        }
    }

    // Update the texture data.
    UpdateTexture(LibretroCore.texture, data);
}

static void LibretroInputPoll() {
    // TODO: LibretroInputPoll().
}

static int16_t LibretroInputState(unsigned port, unsigned device, unsigned index, unsigned id) {
    // TODO: Add multiplayer support.
    if (port > 0) {
        return 0;
    }
    if (device == RETRO_DEVICE_JOYPAD) {
        device = RETRO_DEVICE_KEYBOARD;
        id = LibretroMapRetroJoypadButtonToRetroKey(id);
    }

    if (device == RETRO_DEVICE_KEYBOARD) {
        id = LibretroMapRetroKeyToKeyboardKey(id);
        if (id > 0) {
            return (int)IsKeyDown(id);
        }
    }

    // TODO: Add Gamepad support.
    if (device == RETRO_DEVICE_JOYPAD) {
        id = LibretroMapRetroJoypadButtonToGamepadButton(id);
        if (id >= 0) {
            return (int)IsGamepadButtonDown(port, id);
        }
    }

    // TODO: Add Mouse support.

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

    return true;
}

static bool LoadLibretroGame(const char* gameFile) {
    // Load empty game.
    if (gameFile == NULL) {
        if (LibretroCore.retro_load_game(NULL)) {
            TraceLog(LOG_INFO, "LIBRETRO: Loaded without content");
            return LibretroInitAudioVideo();
        }
        else {
            TraceLog(LOG_ERROR, "LIBRETRO: Failed to load core without content");
            return false;
        }
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
            TraceLog(LOG_INFO, "LIBRETRO: Failed to load full path");
            return false;
        }
    }

    // Load the game data.
    unsigned int size;
    unsigned char * gameData = LoadFileData(gameFile, &size);
    if (size == 0 || gameData == NULL) {
        TraceLog(LOG_ERROR, "LIBRETRO: Failed to load game data with LoadFileData()");
        return false;
    }
    struct retro_game_info info;
    info.path = gameFile;
    info.data = gameData;
    info.size = size;
    info.meta = "";
    if (!LibretroCore.retro_load_game(&info)) {
        TraceLog(LOG_ERROR, "LIBRETRO: Failed to load game data with retro_load_game()");
        free(gameData);
        return false;
    }
    free(gameData);
    return LibretroInitAudioVideo();
}

static const char* GetLibretroName() {
    return LibretroCore.libraryName;
}

static bool InitLibretro(const char* core) {
    // Ensure the core exists.
    if (!FileExists(core)) {
        TraceLog(LOG_ERROR, "LIBRETRO: Given core doesn't exist: %s", core);
        return false;
    }
    LibretroCore.initialized = false;

    if (LibretroCore.handle != NULL) {
        dylib_close(LibretroCore.handle);
    }
    LibretroCore.handle = dylib_load(core);
    if (!LibretroCore.handle) {
        TraceLog(LOG_ERROR, "LIBRETRO: Failed to provided library");
        return false;
    }

    // Find the libretro API version.
    load_retro_sym(retro_api_version);
    LibretroCore.apiVersion = LibretroCore.retro_api_version();
    TraceLog(LOG_INFO, "LIBRETRO: API version: %i", LibretroCore.apiVersion);
    if (LibretroCore.apiVersion != 1) {
        TraceLog(LOG_ERROR, "LIBRETRO: Incompatible API version");
        return false;
    }

    // Retrieve the libretro core system information.
    load_retro_sym(retro_get_system_info);
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
    TraceLog(LOG_INFO, "    > Extensions:   %s", LibretroCore.validExtensions);

    // Load all other libretro methods.
    load_retro_sym(retro_init);
    load_retro_sym(retro_deinit);
    load_retro_sym(retro_set_environment);
    load_retro_sym(retro_set_video_refresh);
    load_retro_sym(retro_set_audio_sample);
    load_retro_sym(retro_set_audio_sample_batch);
    load_retro_sym(retro_set_input_poll);
    load_retro_sym(retro_set_input_state);
    load_retro_sym(retro_get_system_av_info);
    load_retro_sym(retro_set_controller_port_device);
    load_retro_sym(retro_reset);
    load_retro_sym(retro_run);
    load_retro_sym(retro_serialize_size);
    load_retro_sym(retro_serialize);
    load_retro_sym(retro_unserialize);
    load_retro_sym(retro_cheat_reset);
    load_retro_sym(retro_cheat_set);
    load_retro_sym(retro_load_game);
    load_retro_sym(retro_load_game_special);
    load_retro_sym(retro_unload_game);
    load_retro_sym(retro_get_region);
    load_retro_sym(retro_get_memory_data);
    load_retro_sym(retro_get_memory_size);
    // TODO: Add ability to peek inside the core to grab data.
    /*
    if (peek) {
        CloseLibretro();
        return true;
    }
    */

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
    LibretroCore.initialized = true;
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
    Rectangle source = {0, 0, LibretroCore.width, LibretroCore.height};
    Vector2 origin = {0, 0};
    DrawTexturePro(LibretroCore.texture, source, destRec, origin, 0, tint);
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

    // Draw the texture in the middle of the window.
    int x = (GetScreenWidth() - width) / 2;
    int y = (GetScreenHeight() - height) / 2;
    Rectangle destRect = {x,y, width, height};
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

static void UnloadLibretroGame() {
    if (LibretroCore.initialized) {
        LibretroCore.retro_unload_game();
    }
}

static void CloseLibretro() {
    StopAudioStream(LibretroCore.audioStream);
    UnloadLibretroGame();
    if (LibretroCore.initialized) {
        LibretroCore.retro_deinit();
    }
    UnloadTexture(LibretroCore.texture);
    CloseAudioStream(LibretroCore.audioStream);
    if (LibretroCore.handle != NULL) {
        dylib_close(LibretroCore.handle);
    }
    LibretroCore.initialized = false;
    LibretroCore.audioStream = (AudioStream){0};
    LibretroCore.texture = (Texture){0};
}

#if defined(__cplusplus)
}
#endif

#endif // RAYLIB_LIBRETRO_RLIBRETRO_H
