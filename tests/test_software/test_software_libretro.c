/*
 * test_software_libretro.c — minimal software-rendered test core for
 * raylib-libretro.
 *
 * Draws a vertical RGB gradient into a CPU framebuffer and cycles through
 * all three libretro pixel formats (RGB565, 0RGB1555, XRGB8888) every
 * ~2 seconds. Requires no content (supports_no_game = true).
 */

#include "libretro.h"

#include <stdint.h>
#include <string.h>

#define CORE_WIDTH  320
#define CORE_HEIGHT 240
#define CORE_FPS    60.0
#define FRAMES_PER_FORMAT 120

static retro_video_refresh_t       s_video_refresh = NULL;
static retro_audio_sample_t        s_audio_sample  = NULL;
static retro_audio_sample_batch_t  s_audio_batch   = NULL;
static retro_input_poll_t          s_input_poll     = NULL;
static retro_input_state_t         s_input_state    = NULL;
static retro_environment_t         s_environ        = NULL;

static uint32_t s_fb32[CORE_WIDTH * CORE_HEIGHT];
static uint16_t s_fb16[CORE_WIDTH * CORE_HEIGHT];

static unsigned s_frame_count = 0;
static unsigned s_format_index = 0;

static const enum retro_pixel_format s_formats[] = {
    RETRO_PIXEL_FORMAT_RGB565,
    RETRO_PIXEL_FORMAT_0RGB1555,
    RETRO_PIXEL_FORMAT_XRGB8888,
};
#define NUM_FORMATS (sizeof(s_formats) / sizeof(s_formats[0]))

static void set_pixel_format(unsigned index) {
    enum retro_pixel_format fmt = s_formats[index];
    s_environ(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt);
}

static void fill_gradient_rgb565(void) {
    for (unsigned y = 0; y < CORE_HEIGHT; y++) {
        unsigned third = CORE_HEIGHT / 3;
        uint8_t r = 0, g = 0, b = 0;

        if (y < third) {
            r = (uint8_t)((y * 31) / third);
        } else if (y < third * 2) {
            g = (uint8_t)(((y - third) * 63) / third);
        } else {
            b = (uint8_t)(((y - third * 2) * 31) / (CORE_HEIGHT - third * 2));
        }

        uint16_t pixel = (uint16_t)(((r & 0x1F) << 11) | ((g & 0x3F) << 5) | (b & 0x1F));
        for (unsigned x = 0; x < CORE_WIDTH; x++)
            s_fb16[y * CORE_WIDTH + x] = pixel;
    }
}

static void fill_gradient_0rgb1555(void) {
    for (unsigned y = 0; y < CORE_HEIGHT; y++) {
        unsigned third = CORE_HEIGHT / 3;
        uint8_t r = 0, g = 0, b = 0;

        if (y < third) {
            r = (uint8_t)((y * 31) / third);
        } else if (y < third * 2) {
            g = (uint8_t)(((y - third) * 31) / third);
        } else {
            b = (uint8_t)(((y - third * 2) * 31) / (CORE_HEIGHT - third * 2));
        }

        uint16_t pixel = (uint16_t)(((r & 0x1F) << 10) | ((g & 0x1F) << 5) | (b & 0x1F));
        for (unsigned x = 0; x < CORE_WIDTH; x++)
            s_fb16[y * CORE_WIDTH + x] = pixel;
    }
}

static void fill_gradient_xrgb8888(void) {
    for (unsigned y = 0; y < CORE_HEIGHT; y++) {
        unsigned third = CORE_HEIGHT / 3;
        uint8_t r = 0, g = 0, b = 0;

        if (y < third) {
            r = (uint8_t)((y * 255) / third);
        } else if (y < third * 2) {
            g = (uint8_t)(((y - third) * 255) / third);
        } else {
            b = (uint8_t)(((y - third * 2) * 255) / (CORE_HEIGHT - third * 2));
        }

        uint32_t pixel = ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
        for (unsigned x = 0; x < CORE_WIDTH; x++)
            s_fb32[y * CORE_WIDTH + x] = pixel;
    }
}

void retro_set_video_refresh(retro_video_refresh_t cb)           { s_video_refresh = cb; }
void retro_set_audio_sample(retro_audio_sample_t cb)             { s_audio_sample  = cb; }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { s_audio_batch   = cb; }
void retro_set_input_poll(retro_input_poll_t cb)                 { s_input_poll    = cb; }
void retro_set_input_state(retro_input_state_t cb)               { s_input_state   = cb; }

void retro_set_environment(retro_environment_t cb) {
    s_environ = cb;

    bool no_game = true;
    cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &no_game);

    set_pixel_format(0);
}

void retro_init(void)   {}
void retro_deinit(void) {}

unsigned retro_api_version(void) { return RETRO_API_VERSION; }

void retro_get_system_info(struct retro_system_info *info) {
    memset(info, 0, sizeof(*info));
    info->library_name     = "test_software";
    info->library_version  = "1.0";
    info->valid_extensions = "";
    info->need_fullpath    = false;
    info->block_extract    = false;
}

void retro_get_system_av_info(struct retro_system_av_info *info) {
    memset(info, 0, sizeof(*info));
    info->geometry.base_width   = CORE_WIDTH;
    info->geometry.base_height  = CORE_HEIGHT;
    info->geometry.max_width    = CORE_WIDTH;
    info->geometry.max_height   = CORE_HEIGHT;
    info->geometry.aspect_ratio = (float)CORE_WIDTH / (float)CORE_HEIGHT;
    info->timing.fps            = CORE_FPS;
    info->timing.sample_rate    = 44100.0;
}

void retro_set_controller_port_device(unsigned port, unsigned device) { (void)port; (void)device; }

void retro_reset(void) {
    s_frame_count = 0;
    s_format_index = 0;
    set_pixel_format(0);
}

void retro_run(void) {
    s_input_poll();

    if (s_frame_count > 0 && (s_frame_count % FRAMES_PER_FORMAT) == 0) {
        s_format_index = (s_format_index + 1) % NUM_FORMATS;
        set_pixel_format(s_format_index);
    }

    switch (s_formats[s_format_index]) {
    case RETRO_PIXEL_FORMAT_RGB565:
        fill_gradient_rgb565();
        s_video_refresh(s_fb16, CORE_WIDTH, CORE_HEIGHT,
                        CORE_WIDTH * sizeof(uint16_t));
        break;
    case RETRO_PIXEL_FORMAT_0RGB1555:
        fill_gradient_0rgb1555();
        s_video_refresh(s_fb16, CORE_WIDTH, CORE_HEIGHT,
                        CORE_WIDTH * sizeof(uint16_t));
        break;
    case RETRO_PIXEL_FORMAT_XRGB8888:
        fill_gradient_xrgb8888();
        s_video_refresh(s_fb32, CORE_WIDTH, CORE_HEIGHT,
                        CORE_WIDTH * sizeof(uint32_t));
        break;
    default:
        break;
    }

    s_frame_count++;
}

bool retro_load_game(const struct retro_game_info *game) {
    (void)game;
    return true;
}

bool retro_load_game_special(unsigned type, const struct retro_game_info *info, size_t num) {
    (void)type; (void)info; (void)num;
    return false;
}

void retro_unload_game(void) {}

size_t retro_serialize_size(void)                            { return 0; }
bool   retro_serialize(void *data, size_t size)              { (void)data; (void)size; return false; }
bool   retro_unserialize(const void *data, size_t size)      { (void)data; (void)size; return false; }
void   retro_cheat_reset(void)                               {}
void   retro_cheat_set(unsigned i, bool e, const char *c)    { (void)i; (void)e; (void)c; }
unsigned retro_get_region(void)                              { return RETRO_REGION_NTSC; }
void  *retro_get_memory_data(unsigned id)                    { (void)id; return NULL; }
size_t retro_get_memory_size(unsigned id)                    { (void)id; return 0; }
