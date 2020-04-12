#ifndef RAYLIB_LIBRETRO_RLIBRETRO_MAP_H
#define RAYLIB_LIBRETRO_RLIBRETRO_MAP_H

#include "raylib.h"
#include "libretro.h"

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
			return KEY_APOSTROPHE;
		case RETROK_MINUS:
			return KEY_APOSTROPHE;
		case RETROK_PERIOD:
			return KEY_APOSTROPHE;
		case RETROK_SLASH:
			return KEY_APOSTROPHE;
		case RETROK_0:
			return KEY_APOSTROPHE;
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
			return KEY_APOSTROPHE;
		case RETROK_LESS:
			return KEY_APOSTROPHE;
		case RETROK_EQUALS:
			return KEY_APOSTROPHE;
		case RETROK_GREATER:
			return KEY_APOSTROPHE;
		case RETROK_QUESTION:
			return KEY_APOSTROPHE;
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
			return KEY_APOSTROPHE;
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
			return KEY_X;
		case RETROK_z:
			return KEY_Z;
		case RETROK_LEFTBRACE:
			return KEY_APOSTROPHE;
		case RETROK_BAR:
			return KEY_APOSTROPHE;
		case RETROK_RIGHTBRACE:
			return KEY_APOSTROPHE;
		case RETROK_TILDE:
			return KEY_APOSTROPHE;
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
			return KEY_PERIOD;
		case RETROK_KP_DIVIDE:
			return KEY_APOSTROPHE;
		case RETROK_KP_MULTIPLY:
			return KEY_APOSTROPHE;
		case RETROK_KP_MINUS:
			return KEY_APOSTROPHE;
		case RETROK_KP_PLUS:
			return KEY_APOSTROPHE;
		case RETROK_KP_ENTER:
			return KEY_APOSTROPHE;
		case RETROK_KP_EQUALS:
			return KEY_APOSTROPHE;
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
			return KEY_APOSTROPHE;
		case RETROK_PAGEDOWN:
			return KEY_APOSTROPHE;
		case RETROK_F1:
			return KEY_F1;
		case RETROK_F2:
			return KEY_F2;
		case RETROK_F3:
			return KEY_F3;
		case RETROK_F4:
			return KEY_F4;
		case RETROK_F5:
			return KEY_APOSTROPHE;
		case RETROK_F6:
			return KEY_APOSTROPHE;
		case RETROK_F7:
			return KEY_APOSTROPHE;
		case RETROK_F8:
			return KEY_APOSTROPHE;
		case RETROK_F9:
			return KEY_APOSTROPHE;
		case RETROK_F10:
			return KEY_APOSTROPHE;
		case RETROK_F11:
			return KEY_APOSTROPHE;
		case RETROK_F12:
			return KEY_APOSTROPHE;
		case RETROK_F13:
			return KEY_APOSTROPHE;
		case RETROK_F14:
			return KEY_APOSTROPHE;
		case RETROK_F15:
			return KEY_APOSTROPHE;
		case RETROK_NUMLOCK:
			return KEY_APOSTROPHE;
		case RETROK_CAPSLOCK:
			return KEY_APOSTROPHE;
		case RETROK_SCROLLOCK:
			return KEY_APOSTROPHE;
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
			return KEY_APOSTROPHE;
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

	return KEY_KB_MENU;
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
		case RETRO_PIXEL_FORMAT_0RGB1555:
			return UNCOMPRESSED_R5G5B5A1;
		case RETRO_PIXEL_FORMAT_XRGB8888:
			return UNCOMPRESSED_R8G8B8A8;
		case RETRO_PIXEL_FORMAT_RGB565:
			return UNCOMPRESSED_R5G6B5;
	}

	// By default, assume 5551.
	return UNCOMPRESSED_R5G5B5A1;
}

/**
 * Ports pixel data of ARGB8888 pixel format to ABGR8888.
 *
 * TODO: Compile as part of pixconv.c in libretro-common
 */
static void LibretroMapPixelFormatARGB8888ToABGR8888(void *output_, const void *input_,
		int width, int height,
		int out_stride, int in_stride) {
	int h, w;
	const uint32_t *input = (const uint32_t*)input_;
	uint32_t *output = (uint32_t*)output_;

	for (h = 0; h < height;
			h++, output += out_stride >> 2, input += in_stride >> 2) {
		for (w = 0; w < width; w++) {
			uint32_t col = input[w];
			output[w] = ((col << 16) & 0xff0000) |
				((col >> 16) & 0xff) |
				(col & 0xff00ff00);
		}
	}
}

#endif