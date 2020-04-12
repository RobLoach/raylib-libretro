/*******************************************************************************************
*
*   rLibretro v0.0.1 - A simple and easy-to-use immediate-mode libretro library
*
*   DESCRIPTION:
*
*   rLibretro provides a libretro frontend for use with raylib.
*
*   CONTRIBUTORS:
*       Rob Loach:          Initial implementation
*
*   LICENSE: GPL-3.0
*
**********************************************************************************************/

#ifndef RAYLIB_LIBRETRO_RLIBRETRO_HPP
#define RAYLIB_LIBRETRO_RLIBRETRO_HPP

// System dependencies
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

// libretro-common
#include <compat/strl.h>
#include <dynamic/dylib.h>
#include "libretro.h"

// raylib
#include "raylib.h"

// raylib-libretro includes
#include "rLibretroMap.h"

/**
 * Load a symbol using the given names in LibretroCore.
 */
#define load_sym(V, S) do {\
   function_t func = dylib_proc(LibretroCore.handle, #S); \
   memcpy(&V, &func, sizeof(func)); \
   if (!func) \
      TraceLog(LOG_ERROR, "Failed to load symbol '" #S "'"); \
   } while (0)

/**
 * Load the given symbol into LibretroCore.
 */
#define load_retro_sym(S) load_sym(LibretroCore.S, S)

typedef struct rLibretro {
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
	float windowScale;
	int width, height, fps, sampleRate;
	char libraryName[200];
	char libraryVersion[200];
	char validExtensions[200];
	bool needFullpath;
	bool blockExtract;
	bool supportNoGame;
	unsigned apiVersion;
	enum retro_pixel_format pixelFormat;

	// The texture used to render on the screen.
	Texture texture;
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

	TraceLog(type, "[rLibretro] %s", buffer);
}

static void LibretroInitVideo() {
	// Make sure the window size is appropriate.
	SetWindowSize(LibretroCore.width * LibretroCore.windowScale, LibretroCore.height * LibretroCore.windowScale);

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
		case RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME:
		{
			bool supportNoGame = *(bool*)data;
			TraceLog(LOG_INFO, "RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME: %s", supportNoGame ? "true" : "false");
			LibretroCore.supportNoGame = *(bool*)data;
		}
		break;
		case RETRO_ENVIRONMENT_SET_VARIABLES:
		{
			// Sets the given variable.
			// TODO: RETRO_ENVIRONMENT_SET_VARIABLES
			TraceLog(LOG_WARNING, "RETRO_ENVIRONMENT_SET_VARIABLES not implemented.");
		}
		break;
		case RETRO_ENVIRONMENT_GET_VARIABLE:
		{
			// Retrieves the given variable.
			// TODO: RETRO_ENVIRONMENT_GET_VARIABLE
			TraceLog(LOG_WARNING, "RETRO_ENVIRONMENT_GET_VARIABLE not implemented.");
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
			const struct retro_message * message = (const struct retro_message *)data;
			if (message != NULL && message->msg != NULL) {
				TraceLog(LOG_INFO, "RETRO_ENVIRONMENT_SET_MESSAGE: %s (%i)", message->msg, message->frames);
			}
			else {
				TraceLog(LOG_WARNING, "RETRO_ENVIRONMENT_SET_MESSAGE: No message");
			}
		}
		break;
		case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
		{
			const enum retro_pixel_format* pixelFormat = (const enum retro_pixel_format *)data;
			TraceLog(LOG_INFO, "RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: %i", *pixelFormat);
			LibretroCore.pixelFormat = *pixelFormat;
		}
		break;
		case RETRO_ENVIRONMENT_SHUTDOWN:
		{
			TraceLog(LOG_INFO, "RETRO_ENVIRONMENT_SHUTDOWN");
			LibretroCore.shutdown = true;
		}
		break;
		case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
		{
			TraceLog(LOG_INFO, "RETRO_ENVIRONMENT_GET_LOG_INTERFACE");
			struct retro_log_callback *callback = (struct retro_log_callback*)data;
			if (callback != NULL) {
				callback->log = LibretroLogger;
			}
		}
		break;
		case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
		{
			// TODO: RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS
			TraceLog(LOG_WARNING, "TODO: RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS");
		}
		break;
		default:
		{
			// TODO: Add more environment sets.
			TraceLog(LOG_WARNING, "Undefined environment call: %i", cmd);
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
	LibretroCore.width = av.geometry.max_width;
	LibretroCore.height = av.geometry.max_height;
	LibretroCore.fps = av.timing.fps;
	LibretroCore.sampleRate = av.timing.sample_rate;
	return true;
}

/**
 * Runs an interation of the libretro core.
 */
static void LibretroUpdate() {
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
		TraceLog(LOG_WARNING, "VideoRefresh provided no pixel data.");
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
			// Port to UNCOMPRESSED_R8G8B8A8;
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
			// TODO: Port RETRO_PIXEL_FORMAT_0RGB1555 to UNCOMPRESSED_R5G5B5A1;
			TraceLog(LOG_WARNING, "Pixelformat: RETRO_PIXEL_FORMAT_0RGB1555 to UNCOMPRESSED_R5G5B5A1 not implemented");
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
	// TODO: Implement AudioWrite.
	// UpdateAudioStream(LibretroCore.audioStream, data, frames);
	return frames;
}

static void LibretroAudioSample(int16_t left, int16_t right) {
	int16_t buf[2] = {left, right};
	LibretroAudioWrite(buf, 1);
}

static size_t LibretroAudioSampleBatch(const int16_t *data, size_t frames) {
	return LibretroAudioWrite(data, frames);
}

static bool LibretroLoadGame(const char* gameFile) {
	// Load empty game.
	if (gameFile == NULL) {
		if (LibretroCore.retro_load_game(NULL)) {
			TraceLog(LOG_INFO, "Loaded without content");
			return LibretroGetAudioVideo();
		}
		else {
			TraceLog(LOG_ERROR, "failed to load without content");
			return false;
		}
	}

	// Ensure the game exists.
	if (!FileExists(gameFile)) {
		TraceLog(LOG_ERROR, "Given content does not exist: %s", gameFile);
		return false;
	}

	// See if we just need the full path.
	if (LibretroCore.needFullpath) {
		struct retro_game_info info;
		info.data = NULL;
		info.size = 0;
		info.path = gameFile;
		if (LibretroCore.retro_load_game(&info)) {
			TraceLog(LOG_INFO, "Loaded content with full path");
			return LibretroGetAudioVideo();
		}
		else {
			TraceLog(LOG_INFO, "Failed to load full path");
			return false;
		}
	}

	// Load the game data.
	unsigned int size;
	unsigned char * gameData = LoadFileData(gameFile, &size);
	if (size == 0) {
		TraceLog(LOG_ERROR, "Failed to load game data.");
		return false;
	}
	struct retro_game_info info;
	info.path = gameFile;
	info.data = gameData;
	info.size = size;
	info.meta = "";
	if (!LibretroCore.retro_load_game(&info)) {
		TraceLog(LOG_ERROR, "Failed to load game with retro_load_game()");
		free(gameData);
		return false;
	}
	free(gameData);
	return LibretroGetAudioVideo();
}

static void LibretroInitAudio() {
	// TODO: Implement LibretroInitAudio().
	TraceLog(LOG_WARNING, "LibretroInitAudio() not implemented");
	return;
}

static bool LibretroLoadCore(const char* core, bool peek) {
	// Ensure the core exists.
	if (!FileExists(core)) {
		TraceLog(LOG_ERROR, "Given core doesn't exist: %s", core);
		return false;
	}
	LibretroCore.initialized = false;

	LibretroCore.handle = dylib_load(core);
	if (!LibretroCore.handle) {
		TraceLog(LOG_ERROR, "Failed to load library.");
		return false;
	}

	load_retro_sym(retro_init);
	load_retro_sym(retro_deinit);
	load_retro_sym(retro_api_version);
	load_retro_sym(retro_set_environment);
	load_retro_sym(retro_set_video_refresh);
	load_retro_sym(retro_set_audio_sample);
	load_retro_sym(retro_set_audio_sample_batch);
	load_retro_sym(retro_set_input_poll);
	load_retro_sym(retro_set_input_state);
	load_retro_sym(retro_get_system_info);
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

	// Find the libretro API version.
	LibretroCore.apiVersion = LibretroCore.retro_api_version();
	TraceLog(LOG_INFO, "Libretro API: %i", LibretroCore.apiVersion);

	// Retrieve the libretro core system information.
	struct retro_system_info systemInfo;
	LibretroCore.retro_get_system_info(&systemInfo);
	strlcpy(LibretroCore.libraryName, systemInfo.library_name, sizeof(LibretroCore.libraryName));
	strlcpy(LibretroCore.libraryVersion, systemInfo.library_version, sizeof(LibretroCore.libraryVersion));
	strlcpy(LibretroCore.validExtensions, systemInfo.valid_extensions, sizeof(LibretroCore.validExtensions));
	LibretroCore.needFullpath = systemInfo.need_fullpath;
	LibretroCore.blockExtract = systemInfo.block_extract;

	// If we are to just peek insde, now is the time to exit.
	if (peek) {
		return true;
	}

	// Initialize the core.
	LibretroCore.shutdown = false;
	LibretroCore.windowScale = 3;

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

static void LibretroInit() {
	LibretroInitVideo();
	LibretroInitAudio();
	
	// TODO: Support frametime perforance counter instead of static FPS.
	if (LibretroCore.fps > 0) {
		SetTargetFPS(LibretroCore.fps);
	}
}

static void LibretroDraw() {
	Vector2 position = {0, 0};
	DrawTextureEx(LibretroCore.texture, position, 0, LibretroCore.windowScale, RAYWHITE);
}

static void LibretroUnloadGame() {
	LibretroCore.retro_unload_game();
}

static void LibretroClose() {
	LibretroCore.retro_deinit();
	UnloadTexture(LibretroCore.texture);
	LibretroCore.texture = (Texture){0};
	LibretroCore.initialized = false;
	// TODO: Close AudioStream
}

#endif