/**********************************************************************************************
*
*   raylib-libretro-wasm - Load libretro cores compiled as WebAssembly modules.
*
*   Implements core loading via the WebAssembly Micro Runtime (WAMR).
*
*   The WASM module must:
*     - Export all retro_* functions
*     - Import callbacks from the "env" namespace:
*         env::video_refresh      (i32 data, i32 width, i32 height, i32 pitch)
*         env::audio_sample       (i32 left, i32 right)
*         env::audio_sample_batch (i32 data, i32 frames) -> i32
*         env::input_poll         ()
*         env::input_state        (i32 port, i32 device, i32 index, i32 id) -> i32
*         env::environment        (i32 cmd, i32 data) -> i32
*
*   WASM32 struct layouts (4-byte pointers, little-endian) are handled explicitly.
*
*   LICENSE: zlib/libpng (same as raylib-libretro)
*
*   Copyright (c) 2024 Rob Loach (@RobLoach)
*
**********************************************************************************************/

#ifndef RAYLIB_LIBRETRO_WASM_H
#define RAYLIB_LIBRETRO_WASM_H

static bool InitLibretroWasm(const char* path);
static void CloseLibretroWasm(void);

#ifdef RAYLIB_LIBRETRO_IMPLEMENTATION
#ifndef RAYLIB_LIBRETRO_WASM_IMPLEMENTATION_ONCE
#define RAYLIB_LIBRETRO_WASM_IMPLEMENTATION_ONCE

#include "wasm_export.h"
#include <stdint.h>
#include <string.h>

#define RAYLIB_LIBRETRO_WASM_SCRATCH 512

typedef struct rLibretroWasmState {
    wasm_module_t          module;
    wasm_module_inst_t     instance;
    wasm_exec_env_t        exec_env;
    uint8_t*               mem_base;   // base of WASM linear memory (host pointer)

    wasm_function_inst_t   fn_api_version;
    wasm_function_inst_t   fn_get_system_info;
    wasm_function_inst_t   fn_get_av_info;
    wasm_function_inst_t   fn_init;
    wasm_function_inst_t   fn_deinit;
    wasm_function_inst_t   fn_set_environment;
    wasm_function_inst_t   fn_set_video_refresh;
    wasm_function_inst_t   fn_set_audio_sample;
    wasm_function_inst_t   fn_set_audio_sample_batch;
    wasm_function_inst_t   fn_set_input_poll;
    wasm_function_inst_t   fn_set_input_state;
    wasm_function_inst_t   fn_set_controller_port_device;
    wasm_function_inst_t   fn_reset;
    wasm_function_inst_t   fn_run;
    wasm_function_inst_t   fn_serialize_size;
    wasm_function_inst_t   fn_serialize;
    wasm_function_inst_t   fn_unserialize;
    wasm_function_inst_t   fn_cheat_reset;
    wasm_function_inst_t   fn_cheat_set;
    wasm_function_inst_t   fn_load_game;
    wasm_function_inst_t   fn_load_game_special;
    wasm_function_inst_t   fn_unload_game;
    wasm_function_inst_t   fn_get_region;
    wasm_function_inst_t   fn_get_memory_data;
    wasm_function_inst_t   fn_get_memory_size;

    uint32_t               scratch_ptr;
} rLibretroWasmState;

static rLibretroWasmState LibretroWasm = {0};

// ============================================================
// Memory helpers
// ============================================================

static inline uint8_t* WasmMem(void) { return LibretroWasm.mem_base; }

static inline uint32_t WasmReadU32(const void* p) {
    uint32_t v; memcpy(&v, p, sizeof(v)); return v;
}
static inline float WasmReadF32(const void* p) {
    float v; memcpy(&v, p, sizeof(v)); return v;
}
static inline double WasmReadF64(const void* p) {
    double v; memcpy(&v, p, sizeof(v)); return v;
}
static inline void WasmWriteU32(void* p, uint32_t v) { memcpy(p, &v, sizeof(v)); }

static uint32_t WasmMalloc(uint32_t size) {
    void* native_ptr = NULL;
    return wasm_runtime_module_malloc(LibretroWasm.instance, size, &native_ptr);
}

static void WasmFree(uint32_t ptr) {
    if (ptr) wasm_runtime_module_free(LibretroWasm.instance, ptr);
}

static uint32_t WasmCopyString(const char* s) {
    if (!s) return 0;
    uint32_t len = (uint32_t)(strlen(s) + 1);
    void* native_ptr = NULL;
    uint32_t wasm_ptr = wasm_runtime_module_malloc(LibretroWasm.instance, len, &native_ptr);
    if (wasm_ptr && native_ptr) memcpy(native_ptr, s, len);
    return wasm_ptr;
}

// ============================================================
// WASM-aware environment callback
// ============================================================

static bool WasmLibretroEnvironment(unsigned cmd, void* data_host) {
    uint8_t* mem = WasmMem();
    uint8_t* d   = (uint8_t*)data_host;

    switch (cmd) {
        case RETRO_ENVIRONMENT_SET_ROTATION:
            if (!d) return false;
            LibretroCore.rotation = WasmReadU32(d);
            return true;

        case RETRO_ENVIRONMENT_GET_CAN_DUPE:
            if (!d) return false;
            *d = 1;
            return true;

        case RETRO_ENVIRONMENT_SET_MESSAGE: {
            if (!d) return false;
            uint32_t msg_ptr = WasmReadU32(d);
            uint32_t frames  = WasmReadU32(d + 4);
            if (!msg_ptr) return false;
            const char* msg = (const char*)(mem + msg_ptr);
            TextCopy(LibretroCore.osdMessage, msg);
            LibretroCore.osdEndTime = GetTime() + frames / (LibretroCore.fps > 0 ? (double)LibretroCore.fps : 60.0);
            TraceLog(LOG_INFO, "LIBRETRO: WASM: SET_MESSAGE: %s", msg);
            return true;
        }

        case RETRO_ENVIRONMENT_SHUTDOWN:
            LibretroCore.shutdown = true;
            return true;

        case RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL:
            if (!d) return false;
            LibretroCore.performanceLevel = WasmReadU32(d);
            return true;

        case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
        case RETRO_ENVIRONMENT_GET_CORE_ASSETS_DIRECTORY:
        case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
        case RETRO_ENVIRONMENT_GET_LIBRETRO_PATH:
            if (!d) return false;
            WasmWriteU32(d, LibretroWasm.scratch_ptr);
            return true;

        case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
            if (!d) return false;
            LibretroCore.pixelFormat = (enum retro_pixel_format)WasmReadU32(d);
            switch (LibretroCore.pixelFormat) {
                case RETRO_PIXEL_FORMAT_0RGB1555: TraceLog(LOG_INFO, "LIBRETRO: WASM: Pixel format 0RGB1555"); break;
                case RETRO_PIXEL_FORMAT_XRGB8888: TraceLog(LOG_INFO, "LIBRETRO: WASM: Pixel format XRGB8888"); break;
                case RETRO_PIXEL_FORMAT_RGB565:   TraceLog(LOG_INFO, "LIBRETRO: WASM: Pixel format RGB565");   break;
                default:
                    TraceLog(LOG_ERROR, "LIBRETRO: WASM: Unknown pixel format, falling back to RGB565");
                    LibretroCore.pixelFormat = RETRO_PIXEL_FORMAT_RGB565;
                    return false;
            }
            return true;

        case RETRO_ENVIRONMENT_GET_VARIABLE: {
            if (!d) return false;
            uint32_t key_ptr = WasmReadU32(d);
            if (!key_ptr) return false;
            const char* key = (const char*)(mem + key_ptr);
            const char* val = LibretroGetCoreVariable(key);
            if (!val) {
                TraceLog(LOG_WARNING, "LIBRETRO: WASM: GET_VARIABLE unknown key: %s", key);
                return false;
            }
            size_t vlen = strlen(val) + 1;
            if (vlen > RAYLIB_LIBRETRO_WASM_SCRATCH) vlen = RAYLIB_LIBRETRO_WASM_SCRATCH;
            memcpy(mem + LibretroWasm.scratch_ptr, val, vlen);
            mem[LibretroWasm.scratch_ptr + vlen - 1] = '\0';
            WasmWriteU32(d + 4, LibretroWasm.scratch_ptr);
            return true;
        }

        case RETRO_ENVIRONMENT_SET_VARIABLES: {
            if (!d) return false;
            LibretroCore.variableOptionsVersion = 0;
            uint8_t* entry = d;
            while (1) {
                uint32_t key_ptr = WasmReadU32(entry);
                uint32_t val_ptr = WasmReadU32(entry + 4);
                if (!key_ptr) break;
                const char* key  = (const char*)(mem + key_ptr);
                const char* desc = val_ptr ? (const char*)(mem + val_ptr) : "";
                char defaultVal[LIBRETRO_CORE_VARIABLE_VALUE_LEN] = {0};
                char label[LIBRETRO_CORE_VARIABLE_LABEL_LEN]      = {0};
                char valuesList[LIBRETRO_CORE_VARIABLE_VALUES_LEN] = {0};
                const char* semi = strchr(desc, ';');
                if (semi) {
                    size_t llen = (size_t)(semi - desc);
                    while (llen > 0 && desc[llen - 1] == ' ') llen--;
                    if (llen >= LIBRETRO_CORE_VARIABLE_LABEL_LEN) llen = LIBRETRO_CORE_VARIABLE_LABEL_LEN - 1;
                    memcpy(label, desc, llen);
                    const char* opts = semi + 1;
                    while (*opts == ' ') opts++;
                    TextCopy(valuesList, opts);
                    const char* pipe = strchr(opts, '|');
                    size_t dlen = pipe ? (size_t)(pipe - opts) : strlen(opts);
                    if (dlen >= LIBRETRO_CORE_VARIABLE_VALUE_LEN) dlen = LIBRETRO_CORE_VARIABLE_VALUE_LEN - 1;
                    memcpy(defaultVal, opts, dlen);
                }
                LibretroInitCoreVariable(key, defaultVal, label, valuesList, valuesList, "");
                entry += 8;
            }
            return true;
        }

        case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
            if (!d) return false;
            *d = LibretroCore.variablesDirty ? 1 : 0;
            LibretroCore.variablesDirty = false;
            return true;

        case RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME:
            if (!d) return false;
            LibretroCore.supportNoGame = (*d != 0);
            return true;

        case RETRO_ENVIRONMENT_GET_INPUT_DEVICE_CAPABILITIES:
            if (!d) return false;
            {
                uint64_t caps = ((uint64_t)1 << RETRO_DEVICE_JOYPAD)   |
                                ((uint64_t)1 << RETRO_DEVICE_MOUSE)    |
                                ((uint64_t)1 << RETRO_DEVICE_KEYBOARD) |
                                ((uint64_t)1 << RETRO_DEVICE_ANALOG)   |
                                ((uint64_t)1 << RETRO_DEVICE_POINTER);
                memcpy(d, &caps, 8);
            }
            return true;

        case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO: {
            if (!d) return false;
            uint32_t bw  = WasmReadU32(d + 0);
            uint32_t bh  = WasmReadU32(d + 4);
            float    ar  = WasmReadF32(d + 16);
            double   fps = WasmReadF64(d + 24);
            double   sr  = WasmReadF64(d + 32);
            bool dimsChanged = (bw != LibretroCore.width || bh != LibretroCore.height);
            bool srChanged   = (sr != LibretroCore.sampleRate);
            LibretroCore.width = bw;
            LibretroCore.height = bh;
            LibretroCore.aspectRatio = ar;
            LibretroCore.fps = (unsigned)fps;
            LibretroCore.sampleRate = sr;
            if (dimsChanged) LibretroInitVideo();
            if (srChanged) {
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

        case RETRO_ENVIRONMENT_SET_GEOMETRY: {
            if (!d) return false;
            LibretroCore.width       = WasmReadU32(d + 0);
            LibretroCore.height      = WasmReadU32(d + 4);
            LibretroCore.aspectRatio = WasmReadF32(d + 16);
            TraceLog(LOG_INFO, "LIBRETRO: WASM: SET_GEOMETRY %ux%u @ %.3f",
                LibretroCore.width, LibretroCore.height, LibretroCore.aspectRatio);
            return true;
        }

        case RETRO_ENVIRONMENT_GET_USERNAME:
            if (!d) return false;
            {
                const char* name = "raylib";
                size_t nlen = strlen(name) + 1;
                if (nlen <= RAYLIB_LIBRETRO_WASM_SCRATCH)
                    memcpy(mem + LibretroWasm.scratch_ptr, name, nlen);
                WasmWriteU32(d, LibretroWasm.scratch_ptr);
            }
            return true;

        case RETRO_ENVIRONMENT_GET_LANGUAGE:
            if (!d) return false;
            WasmWriteU32(d, RETRO_LANGUAGE_ENGLISH);
            return true;

        case RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE:
            if (!d) return false;
            WasmWriteU32(d, RETRO_AV_ENABLE_VIDEO | RETRO_AV_ENABLE_AUDIO);
            return true;

        case RETRO_ENVIRONMENT_GET_FASTFORWARDING:
            if (!d) return false;
            *d = 0;
            return true;

        case RETRO_ENVIRONMENT_GET_TARGET_REFRESH_RATE:
            if (!d) return false;
            {
                float rate = (float)GetMonitorRefreshRate(GetCurrentMonitor());
                memcpy(d, &rate, 4);
            }
            return true;

        case RETRO_ENVIRONMENT_GET_INPUT_BITMASKS:
            return true;

        case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION:
            if (!d) return false;
            WasmWriteU32(d, 2);
            return true;

        default:
            return false;
    }
}

// ============================================================
// Host callback functions (registered as WAMR native imports)
// ============================================================

static void WasmCbVideoRefresh(wasm_exec_env_t exec_env, int32_t data_off,
                               int32_t width, int32_t height, int32_t pitch) {
    const void* data = data_off ? WasmMem() + data_off : NULL;
    LibretroVideoRefresh(data, (unsigned)width, (unsigned)height, (size_t)pitch);
    (void)exec_env;
}

static void WasmCbAudioSample(wasm_exec_env_t exec_env, int32_t left, int32_t right) {
    LibretroAudioSample((int16_t)left, (int16_t)right);
    (void)exec_env;
}

static int32_t WasmCbAudioSampleBatch(wasm_exec_env_t exec_env, int32_t data_off, int32_t frames) {
    const int16_t* data = data_off ? (const int16_t*)(WasmMem() + data_off) : NULL;
    size_t written = data ? LibretroAudioSampleBatch(data, (size_t)frames) : 0;
    (void)exec_env;
    return (int32_t)written;
}

static void WasmCbInputPoll(wasm_exec_env_t exec_env) {
    LibretroInputPoll();
    (void)exec_env;
}

static int32_t WasmCbInputState(wasm_exec_env_t exec_env,
                                int32_t port, int32_t device, int32_t index, int32_t id) {
    (void)exec_env;
    return (int32_t)LibretroInputState((unsigned)port, (unsigned)device, (unsigned)index, (unsigned)id);
}

static int32_t WasmCbEnvironment(wasm_exec_env_t exec_env, int32_t cmd, int32_t data_off) {
    void* data_host = data_off ? WasmMem() + data_off : NULL;
    (void)exec_env;
    return WasmLibretroEnvironment((unsigned)cmd, data_host) ? 1 : 0;
}

// ============================================================
// Call helpers
// ============================================================

static inline void WasmCall0(wasm_function_inst_t fn) {
    if (!fn) return;
    wasm_runtime_call_wasm(LibretroWasm.exec_env, fn, 0, NULL);
}

static inline void WasmCall1i(wasm_function_inst_t fn, uint32_t a0) {
    if (!fn) return;
    uint32_t argv[1] = {a0};
    wasm_runtime_call_wasm(LibretroWasm.exec_env, fn, 1, argv);
}

static inline void WasmCall2i(wasm_function_inst_t fn, uint32_t a0, uint32_t a1) {
    if (!fn) return;
    uint32_t argv[2] = {a0, a1};
    wasm_runtime_call_wasm(LibretroWasm.exec_env, fn, 2, argv);
}

static inline void WasmCall3i(wasm_function_inst_t fn, uint32_t a0, uint32_t a1, uint32_t a2) {
    if (!fn) return;
    uint32_t argv[3] = {a0, a1, a2};
    wasm_runtime_call_wasm(LibretroWasm.exec_env, fn, 3, argv);
}

static inline uint32_t WasmCall0_ri(wasm_function_inst_t fn) {
    if (!fn) return 0;
    uint32_t argv[1] = {0};
    wasm_runtime_call_wasm(LibretroWasm.exec_env, fn, 0, argv);
    return argv[0];
}

static inline uint32_t WasmCall1i_ri(wasm_function_inst_t fn, uint32_t a0) {
    if (!fn) return 0;
    uint32_t argv[1] = {a0};
    wasm_runtime_call_wasm(LibretroWasm.exec_env, fn, 1, argv);
    return argv[0];
}

static inline uint32_t WasmCall2i_ri(wasm_function_inst_t fn, uint32_t a0, uint32_t a1) {
    if (!fn) return 0;
    uint32_t argv[2] = {a0, a1};
    wasm_runtime_call_wasm(LibretroWasm.exec_env, fn, 2, argv);
    return argv[0];
}

// ============================================================
// Retro wrappers
// ============================================================

static void WasmRetroInit(void)       { WasmCall0(LibretroWasm.fn_init); }
static void WasmRetroDeinit(void)     { WasmCall0(LibretroWasm.fn_deinit); }
static void WasmRetroReset(void)      { WasmCall0(LibretroWasm.fn_reset); }
static void WasmRetroRun(void)        { WasmCall0(LibretroWasm.fn_run); }
static void WasmRetroCheatReset(void) { WasmCall0(LibretroWasm.fn_cheat_reset); }
static void WasmRetroUnloadGame(void) { WasmCall0(LibretroWasm.fn_unload_game); }

static unsigned WasmRetroApiVersion(void) {
    return (unsigned)WasmCall0_ri(LibretroWasm.fn_api_version);
}
static unsigned WasmRetroGetRegion(void) {
    return (unsigned)WasmCall0_ri(LibretroWasm.fn_get_region);
}
static size_t WasmRetroSerializeSize(void) {
    return (size_t)WasmCall0_ri(LibretroWasm.fn_serialize_size);
}

static void WasmRetroSetEnvironment(retro_environment_t cb) {
    WasmCall1i(LibretroWasm.fn_set_environment, 0); (void)cb;
}
static void WasmRetroSetVideoRefresh(retro_video_refresh_t cb) {
    WasmCall1i(LibretroWasm.fn_set_video_refresh, 0); (void)cb;
}
static void WasmRetroSetAudioSample(retro_audio_sample_t cb) {
    WasmCall1i(LibretroWasm.fn_set_audio_sample, 0); (void)cb;
}
static void WasmRetroSetAudioSampleBatch(retro_audio_sample_batch_t cb) {
    WasmCall1i(LibretroWasm.fn_set_audio_sample_batch, 0); (void)cb;
}
static void WasmRetroSetInputPoll(retro_input_poll_t cb) {
    WasmCall1i(LibretroWasm.fn_set_input_poll, 0); (void)cb;
}
static void WasmRetroSetInputState(retro_input_state_t cb) {
    WasmCall1i(LibretroWasm.fn_set_input_state, 0); (void)cb;
}
static void WasmRetroSetControllerPortDevice(unsigned port, unsigned device) {
    WasmCall2i(LibretroWasm.fn_set_controller_port_device, (uint32_t)port, (uint32_t)device);
}

static void WasmRetroGetSystemInfo(struct retro_system_info* info) {
    if (!info || !LibretroWasm.fn_get_system_info) return;
    // WASM32 retro_system_info: { ptr(+0), ptr(+4), ptr(+8), bool(+12), bool(+13) } = 16 bytes
    void* native_ptr = NULL;
    uint32_t wptr = wasm_runtime_module_malloc(LibretroWasm.instance, 16, &native_ptr);
    if (!wptr) return;
    memset(native_ptr, 0, 16);
    WasmCall1i(LibretroWasm.fn_get_system_info, wptr);
    uint8_t* p = (uint8_t*)native_ptr;
    uint8_t* mem = WasmMem();
    uint32_t name_ptr = WasmReadU32(p + 0);
    uint32_t ver_ptr  = WasmReadU32(p + 4);
    uint32_t ext_ptr  = WasmReadU32(p + 8);
    info->library_name     = name_ptr ? (const char*)(mem + name_ptr) : "";
    info->library_version  = ver_ptr  ? (const char*)(mem + ver_ptr)  : "";
    info->valid_extensions = ext_ptr  ? (const char*)(mem + ext_ptr)  : "";
    info->need_fullpath    = p[12] != 0;
    info->block_extract    = p[13] != 0;
    wasm_runtime_module_free(LibretroWasm.instance, wptr);
}

static void WasmRetroGetSystemAvInfo(struct retro_system_av_info* info) {
    if (!info || !LibretroWasm.fn_get_av_info) return;
    // WASM32: geometry(+0..+19) + pad(4) + timing{f64,f64}(+24..+39) = 40 bytes
    void* native_ptr = NULL;
    uint32_t wptr = wasm_runtime_module_malloc(LibretroWasm.instance, 40, &native_ptr);
    if (!wptr) return;
    memset(native_ptr, 0, 40);
    WasmCall1i(LibretroWasm.fn_get_av_info, wptr);
    uint8_t* p = (uint8_t*)native_ptr;
    info->geometry.base_width   = WasmReadU32(p + 0);
    info->geometry.base_height  = WasmReadU32(p + 4);
    info->geometry.max_width    = WasmReadU32(p + 8);
    info->geometry.max_height   = WasmReadU32(p + 12);
    info->geometry.aspect_ratio = WasmReadF32(p + 16);
    info->timing.fps            = WasmReadF64(p + 24);
    info->timing.sample_rate    = WasmReadF64(p + 32);
    wasm_runtime_module_free(LibretroWasm.instance, wptr);
}

static bool WasmRetroLoadGame(const struct retro_game_info* game) {
    if (!LibretroWasm.fn_load_game) return false;
    if (!game) return WasmCall1i_ri(LibretroWasm.fn_load_game, 0) != 0;

    // WASM32 retro_game_info: { ptr(+0), ptr(+4), u32(+8), ptr(+12) } = 16 bytes
    void* game_native = NULL;
    uint32_t game_wptr = wasm_runtime_module_malloc(LibretroWasm.instance, 16, &game_native);
    if (!game_wptr) return false;
    memset(game_native, 0, 16);

    uint32_t path_wptr = 0, data_wptr = 0;
    if (game->path) path_wptr = WasmCopyString(game->path);
    if (game->data && game->size > 0) {
        void* data_native = NULL;
        data_wptr = wasm_runtime_module_malloc(LibretroWasm.instance, (uint32_t)game->size, &data_native);
        if (data_wptr && data_native) memcpy(data_native, game->data, game->size);
    }
    uint8_t* p = (uint8_t*)game_native;
    WasmWriteU32(p + 0,  path_wptr);
    WasmWriteU32(p + 4,  data_wptr);
    WasmWriteU32(p + 8,  (uint32_t)game->size);
    WasmWriteU32(p + 12, 0); // meta = NULL

    uint32_t ok = WasmCall1i_ri(LibretroWasm.fn_load_game, game_wptr);

    if (path_wptr) wasm_runtime_module_free(LibretroWasm.instance, path_wptr);
    if (data_wptr) wasm_runtime_module_free(LibretroWasm.instance, data_wptr);
    wasm_runtime_module_free(LibretroWasm.instance, game_wptr);
    return ok != 0;
}

static bool WasmRetroLoadGameSpecial(unsigned type, const struct retro_game_info* infos, size_t num) {
    (void)type; (void)infos; (void)num;
    return false;
}

static bool WasmRetroSerialize(void* data, size_t size) {
    if (!LibretroWasm.fn_serialize || !data || !size) return false;
    void* native_ptr = NULL;
    uint32_t wptr = wasm_runtime_module_malloc(LibretroWasm.instance, (uint32_t)size, &native_ptr);
    if (!wptr) return false;
    memset(native_ptr, 0, size);
    uint32_t ok = WasmCall2i_ri(LibretroWasm.fn_serialize, wptr, (uint32_t)size);
    if (ok) memcpy(data, native_ptr, size);
    wasm_runtime_module_free(LibretroWasm.instance, wptr);
    return ok != 0;
}

static bool WasmRetroUnserialize(const void* data, size_t size) {
    if (!LibretroWasm.fn_unserialize || !data || !size) return false;
    void* native_ptr = NULL;
    uint32_t wptr = wasm_runtime_module_malloc(LibretroWasm.instance, (uint32_t)size, &native_ptr);
    if (!wptr) return false;
    memcpy(native_ptr, data, size);
    uint32_t ok = WasmCall2i_ri(LibretroWasm.fn_unserialize, wptr, (uint32_t)size);
    wasm_runtime_module_free(LibretroWasm.instance, wptr);
    return ok != 0;
}

static void WasmRetroCheatSet(unsigned index, bool enabled, const char* code) {
    if (!LibretroWasm.fn_cheat_set) return;
    uint32_t code_wptr = WasmCopyString(code ? code : "");
    WasmCall3i(LibretroWasm.fn_cheat_set, (uint32_t)index, enabled ? 1 : 0, code_wptr);
    WasmFree(code_wptr);
}

static void* WasmRetroGetMemoryData(unsigned id) {
    uint32_t ptr = WasmCall1i_ri(LibretroWasm.fn_get_memory_data, (uint32_t)id);
    return ptr ? WasmMem() + ptr : NULL;
}

static size_t WasmRetroGetMemorySize(unsigned id) {
    return (size_t)WasmCall1i_ri(LibretroWasm.fn_get_memory_size, (uint32_t)id);
}

// ============================================================
// Export lookup
// ============================================================

static void WasmFindExports(void) {
#define LOOKUP(sym) wasm_runtime_lookup_function(LibretroWasm.instance, #sym, NULL)
    LibretroWasm.fn_api_version               = LOOKUP(retro_api_version);
    LibretroWasm.fn_get_system_info           = LOOKUP(retro_get_system_info);
    LibretroWasm.fn_get_av_info               = LOOKUP(retro_get_system_av_info);
    LibretroWasm.fn_init                      = LOOKUP(retro_init);
    LibretroWasm.fn_deinit                    = LOOKUP(retro_deinit);
    LibretroWasm.fn_set_environment           = LOOKUP(retro_set_environment);
    LibretroWasm.fn_set_video_refresh         = LOOKUP(retro_set_video_refresh);
    LibretroWasm.fn_set_audio_sample          = LOOKUP(retro_set_audio_sample);
    LibretroWasm.fn_set_audio_sample_batch    = LOOKUP(retro_set_audio_sample_batch);
    LibretroWasm.fn_set_input_poll            = LOOKUP(retro_set_input_poll);
    LibretroWasm.fn_set_input_state           = LOOKUP(retro_set_input_state);
    LibretroWasm.fn_set_controller_port_device = LOOKUP(retro_set_controller_port_device);
    LibretroWasm.fn_reset                     = LOOKUP(retro_reset);
    LibretroWasm.fn_run                       = LOOKUP(retro_run);
    LibretroWasm.fn_serialize_size            = LOOKUP(retro_serialize_size);
    LibretroWasm.fn_serialize                 = LOOKUP(retro_serialize);
    LibretroWasm.fn_unserialize               = LOOKUP(retro_unserialize);
    LibretroWasm.fn_cheat_reset               = LOOKUP(retro_cheat_reset);
    LibretroWasm.fn_cheat_set                 = LOOKUP(retro_cheat_set);
    LibretroWasm.fn_load_game                 = LOOKUP(retro_load_game);
    LibretroWasm.fn_load_game_special         = LOOKUP(retro_load_game_special);
    LibretroWasm.fn_unload_game               = LOOKUP(retro_unload_game);
    LibretroWasm.fn_get_region                = LOOKUP(retro_get_region);
    LibretroWasm.fn_get_memory_data           = LOOKUP(retro_get_memory_data);
    LibretroWasm.fn_get_memory_size           = LOOKUP(retro_get_memory_size);
#undef LOOKUP
}

// ============================================================
// Wire LibretroCore function pointers
// ============================================================

static void WasmWireCoreFuncs(void) {
    LibretroCore.retro_init                       = WasmRetroInit;
    LibretroCore.retro_deinit                     = WasmRetroDeinit;
    LibretroCore.retro_api_version                = WasmRetroApiVersion;
    LibretroCore.retro_set_environment            = WasmRetroSetEnvironment;
    LibretroCore.retro_set_video_refresh          = WasmRetroSetVideoRefresh;
    LibretroCore.retro_set_audio_sample           = WasmRetroSetAudioSample;
    LibretroCore.retro_set_audio_sample_batch     = WasmRetroSetAudioSampleBatch;
    LibretroCore.retro_set_input_poll             = WasmRetroSetInputPoll;
    LibretroCore.retro_set_input_state            = WasmRetroSetInputState;
    LibretroCore.retro_get_system_info            = WasmRetroGetSystemInfo;
    LibretroCore.retro_get_system_av_info         = WasmRetroGetSystemAvInfo;
    LibretroCore.retro_set_controller_port_device = WasmRetroSetControllerPortDevice;
    LibretroCore.retro_reset                      = WasmRetroReset;
    LibretroCore.retro_run                        = WasmRetroRun;
    LibretroCore.retro_serialize_size             = WasmRetroSerializeSize;
    LibretroCore.retro_serialize                  = WasmRetroSerialize;
    LibretroCore.retro_unserialize                = WasmRetroUnserialize;
    LibretroCore.retro_cheat_reset                = WasmRetroCheatReset;
    LibretroCore.retro_cheat_set                  = WasmRetroCheatSet;
    LibretroCore.retro_load_game                  = WasmRetroLoadGame;
    LibretroCore.retro_load_game_special          = WasmRetroLoadGameSpecial;
    LibretroCore.retro_unload_game                = WasmRetroUnloadGame;
    LibretroCore.retro_get_region                 = WasmRetroGetRegion;
    LibretroCore.retro_get_memory_data            = WasmRetroGetMemoryData;
    LibretroCore.retro_get_memory_size            = WasmRetroGetMemorySize;
}

// ============================================================
// Public API
// ============================================================

static bool InitLibretroWasm(const char* path) {
    int file_size = 0;
    unsigned char* file_data = LoadFileData(path, &file_size);
    if (!file_data || file_size <= 0) {
        TraceLog(LOG_ERROR, "LIBRETRO: WASM: Failed to read %s", path);
        return false;
    }

    RuntimeInitArgs init_args;
    memset(&init_args, 0, sizeof(init_args));
    init_args.mem_alloc_type = Alloc_With_System_Allocator;
    if (!wasm_runtime_full_init(&init_args)) {
        TraceLog(LOG_ERROR, "LIBRETRO: WASM: Failed to init runtime");
        UnloadFileData(file_data);
        return false;
    }

    static NativeSymbol native_symbols[] = {
        {"video_refresh",      (void*)WasmCbVideoRefresh,      "(iiii)",  NULL},
        {"audio_sample",       (void*)WasmCbAudioSample,       "(ii)",    NULL},
        {"audio_sample_batch", (void*)WasmCbAudioSampleBatch,  "(ii)i",   NULL},
        {"input_poll",         (void*)WasmCbInputPoll,          "()",      NULL},
        {"input_state",        (void*)WasmCbInputState,         "(iiii)i", NULL},
        {"environment",        (void*)WasmCbEnvironment,        "(ii)i",   NULL},
    };
    if (!wasm_runtime_register_natives("env", native_symbols,
                                       sizeof(native_symbols) / sizeof(NativeSymbol))) {
        TraceLog(LOG_ERROR, "LIBRETRO: WASM: Failed to register native symbols");
        UnloadFileData(file_data);
        wasm_runtime_destroy();
        return false;
    }

    char error_buf[128];
    LibretroWasm.module = wasm_runtime_load(file_data, (uint32_t)file_size,
                                             error_buf, sizeof(error_buf));
    UnloadFileData(file_data);
    if (!LibretroWasm.module) {
        TraceLog(LOG_ERROR, "LIBRETRO: WASM: Failed to load %s: %s", path, error_buf);
        wasm_runtime_destroy();
        return false;
    }

    LibretroWasm.instance = wasm_runtime_instantiate(LibretroWasm.module,
                                                      64 * 1024, 512 * 1024,
                                                      error_buf, sizeof(error_buf));
    if (!LibretroWasm.instance) {
        TraceLog(LOG_ERROR, "LIBRETRO: WASM: Failed to instantiate: %s", error_buf);
        CloseLibretroWasm();
        return false;
    }

    LibretroWasm.exec_env = wasm_runtime_create_exec_env(LibretroWasm.instance, 64 * 1024);
    if (!LibretroWasm.exec_env) {
        TraceLog(LOG_ERROR, "LIBRETRO: WASM: Failed to create exec env");
        CloseLibretroWasm();
        return false;
    }

    // Probe memory base by allocating a temporary byte and computing base = native - wasm_offset
    void* temp_native = NULL;
    uint32_t temp_wasm = wasm_runtime_module_malloc(LibretroWasm.instance, 1, &temp_native);
    if (!temp_wasm || !temp_native) {
        TraceLog(LOG_ERROR, "LIBRETRO: WASM: Failed to probe memory base");
        CloseLibretroWasm();
        return false;
    }
    LibretroWasm.mem_base = (uint8_t*)temp_native - temp_wasm;
    wasm_runtime_module_free(LibretroWasm.instance, temp_wasm);

    WasmFindExports();

    if (!LibretroWasm.fn_init || !LibretroWasm.fn_run) {
        TraceLog(LOG_ERROR, "LIBRETRO: WASM: Module missing required retro_* exports");
        CloseLibretroWasm();
        return false;
    }

    LibretroWasm.scratch_ptr = WasmMalloc(RAYLIB_LIBRETRO_WASM_SCRATCH);
    if (!LibretroWasm.scratch_ptr) {
        TraceLog(LOG_ERROR, "LIBRETRO: WASM: Failed to allocate scratch buffer");
        CloseLibretroWasm();
        return false;
    }
    const char* cwd = GetWorkingDirectory();
    size_t cwdlen = strlen(cwd) + 1;
    if (cwdlen > RAYLIB_LIBRETRO_WASM_SCRATCH) cwdlen = RAYLIB_LIBRETRO_WASM_SCRATCH;
    memcpy(WasmMem() + LibretroWasm.scratch_ptr, cwd, cwdlen);

    WasmWireCoreFuncs();

    TraceLog(LOG_INFO, "LIBRETRO: WASM: Loaded %s", path);
    return true;
}

static void CloseLibretroWasm(void) {
    if (LibretroWasm.scratch_ptr && LibretroWasm.instance) {
        wasm_runtime_module_free(LibretroWasm.instance, LibretroWasm.scratch_ptr);
        LibretroWasm.scratch_ptr = 0;
    }
    if (LibretroWasm.exec_env) {
        wasm_runtime_destroy_exec_env(LibretroWasm.exec_env);
        LibretroWasm.exec_env = NULL;
    }
    if (LibretroWasm.instance) {
        wasm_runtime_deinstantiate(LibretroWasm.instance);
        LibretroWasm.instance = NULL;
    }
    if (LibretroWasm.module) {
        wasm_runtime_unload(LibretroWasm.module);
        LibretroWasm.module = NULL;
    }
    wasm_runtime_destroy();
    LibretroWasm.mem_base = NULL;
}

#endif // RAYLIB_LIBRETRO_WASM_IMPLEMENTATION_ONCE
#endif // RAYLIB_LIBRETRO_IMPLEMENTATION
#endif // RAYLIB_LIBRETRO_WASM_H
